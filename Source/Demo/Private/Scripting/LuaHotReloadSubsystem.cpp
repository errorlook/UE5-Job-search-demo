#include "Scripting/LuaHotReloadSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Components/QuestComponent.h"
#include "Quest/QuestDataAsset.h"
#include "UnLuaModule.h"
#include "LuaEnv.h"
#include "lua.hpp"

DEFINE_LOG_CATEGORY_STATIC(LogLuaHotReload, Log, All);

namespace
{
constexpr TCHAR QuestModuleName[] = TEXT("Quest.QuestConfig");
constexpr TCHAR SettingsModuleName[] = TEXT("Settings.SettingsConfig");
constexpr TCHAR ExternalQuestConfigRelativePath[] =
	TEXT("HotUpdate/Lua/Quest/QuestConfig.lua");
constexpr int64 MaxExternalQuestConfigBytes = 4 * 1024 * 1024;

#if !UE_BUILD_SHIPPING
TMap<FString, FString> GValidationModuleChunks;
#endif

struct FLuaStackGuard
{
	FLuaStackGuard(lua_State* InState, int32 InInitialTop)
		: State(InState), InitialTop(InInitialTop)
	{
	}

	~FLuaStackGuard()
	{
		lua_settop(State, InitialTop);
	}

	lua_State* State;
	int32 InitialTop;
};

FString LuaTypeName(lua_State* State, int32 Index)
{
	return UTF8_TO_TCHAR(lua_typename(State, lua_type(State, Index)));
}

FString GetExternalQuestConfigPathInternal()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectSavedDir(), ExternalQuestConfigRelativePath));
}

const TCHAR* QuestConfigSourceName(ELuaQuestConfigSource Source)
{
	return Source == ELuaQuestConfigSource::ExternalOverride
		? TEXT("ExternalOverride") : TEXT("PackagedDefault");
}

bool ValidateUtf8(
	const uint8* Data, int64 Length, int64& OutInvalidOffset)
{
	OutInvalidOffset = INDEX_NONE;
	int64 Index = 0;
	while (Index < Length)
	{
		const uint8 First = Data[Index];
		if (First == 0)
		{
			OutInvalidOffset = Index;
			return false;
		}
		if (First <= 0x7f)
		{
			++Index;
			continue;
		}

		int32 SequenceLength = 0;
		uint8 MinSecond = 0x80;
		uint8 MaxSecond = 0xbf;
		if (First >= 0xc2 && First <= 0xdf)
		{
			SequenceLength = 2;
		}
		else if (First >= 0xe0 && First <= 0xef)
		{
			SequenceLength = 3;
			if (First == 0xe0) MinSecond = 0xa0;
			if (First == 0xed) MaxSecond = 0x9f;
		}
		else if (First >= 0xf0 && First <= 0xf4)
		{
			SequenceLength = 4;
			if (First == 0xf0) MinSecond = 0x90;
			if (First == 0xf4) MaxSecond = 0x8f;
		}
		else
		{
			OutInvalidOffset = Index;
			return false;
		}

		if (Index + SequenceLength > Length)
		{
			OutInvalidOffset = Index;
			return false;
		}
		const uint8 Second = Data[Index + 1];
		if (Second < MinSecond || Second > MaxSecond)
		{
			OutInvalidOffset = Index + 1;
			return false;
		}
		for (int32 Continuation = 2;
			Continuation < SequenceLength; ++Continuation)
		{
			const uint8 Value = Data[Index + Continuation];
			if (Value < 0x80 || Value > 0xbf)
			{
				OutInvalidOffset = Index + Continuation;
				return false;
			}
		}
		Index += SequenceLength;
	}
	return true;
}

bool LoadExternalQuestTable(
	const UObject* Context, const FString& Path, lua_State*& OutState,
	FString& OutError)
{
	UnLua::FLuaEnv* Env = IUnLuaModule::Get().GetEnv(
		Context ? Context->GetWorld() ? Context->GetWorld()->GetGameInstance()
			: Context->GetTypedOuter<UGameInstance>() : nullptr);
	if (!Env || !Env->GetMainState())
	{
		OutError = TEXT("UnLua environment is unavailable.");
		return false;
	}

	TArray<uint8> FileBytes;
	if (!FFileHelper::LoadFileToArray(FileBytes, *Path))
	{
		OutError = FString::Printf(
			TEXT("Could not read external UTF-8 file: %s"), *Path);
		return false;
	}
	if (FileBytes.Num() > MaxExternalQuestConfigBytes)
	{
		OutError = FString::Printf(
			TEXT("External file is too large (%d bytes, maximum %lld): %s"),
			FileBytes.Num(), MaxExternalQuestConfigBytes, *Path);
		return false;
	}

	int32 Offset = 0;
	if (FileBytes.Num() >= 3 && FileBytes[0] == 0xef &&
		FileBytes[1] == 0xbb && FileBytes[2] == 0xbf)
	{
		Offset = 3;
	}
	const uint8* Utf8Data = FileBytes.Num() > Offset
		? FileBytes.GetData() + Offset
		: reinterpret_cast<const uint8*>("");
	int64 InvalidOffset = INDEX_NONE;
	if (!ValidateUtf8(
		Utf8Data, FileBytes.Num() - Offset, InvalidOffset))
	{
		OutError = FString::Printf(
			TEXT("External file is not valid UTF-8 (byte offset %lld): %s"),
			InvalidOffset + Offset, *Path);
		return false;
	}

	OutState = Env->GetMainState();
	const int32 InitialTop = lua_gettop(OutState);
	const FTCHARToUTF8 ChunkNameUtf8(*Path);
	const char* Buffer = reinterpret_cast<const char*>(Utf8Data);
	const size_t BufferLength = static_cast<size_t>(
		FileBytes.Num() - Offset);
	if (luaL_loadbufferx(
		OutState, Buffer, BufferLength, ChunkNameUtf8.Get(), "t") != LUA_OK)
	{
		const char* Error = lua_tostring(OutState, -1);
		OutError = FString::Printf(TEXT("Compile error in %s: %s"), *Path,
			Error ? UTF8_TO_TCHAR(Error) : TEXT("unknown Lua compile error"));
		lua_settop(OutState, InitialTop);
		return false;
	}

	// Give the external chunk its own empty _ENV table. Configuration literals,
	// locals and expressions work, but the file cannot mutate UnLua globals.
	const int32 ChunkIndex = lua_absindex(OutState, -1);
	lua_newtable(OutState);
	const int32 SandboxIndex = lua_absindex(OutState, -1);
	lua_pushvalue(OutState, SandboxIndex);
	lua_setfield(OutState, SandboxIndex, "_G");
	lua_pushvalue(OutState, SandboxIndex);
	if (!lua_setupvalue(OutState, ChunkIndex, 1))
	{
		// Chunks that never access a global may not have an _ENV upvalue.
		lua_pop(OutState, 1);
	}
	lua_remove(OutState, SandboxIndex);

	if (lua_pcall(OutState, 0, 1, 0) != LUA_OK)
	{
		const char* Error = lua_tostring(OutState, -1);
		OutError = FString::Printf(TEXT("Runtime error in %s: %s"), *Path,
			Error ? UTF8_TO_TCHAR(Error) : TEXT("unknown Lua runtime error"));
		lua_settop(OutState, InitialTop);
		return false;
	}
	if (!lua_istable(OutState, -1))
	{
		OutError = FString::Printf(
			TEXT("%s must return a table, got %s."), *Path,
			*LuaTypeName(OutState, -1));
		lua_settop(OutState, InitialTop);
		return false;
	}
	return true;
}

bool LoadModuleTable(
	const UObject* Context, const TCHAR* ModuleName, lua_State*& OutState,
	FString& OutError, bool bAllowValidationChunk = true)
{
	UnLua::FLuaEnv* Env = IUnLuaModule::Get().GetEnv(
		Context ? Context->GetWorld() ? Context->GetWorld()->GetGameInstance()
			: Context->GetTypedOuter<UGameInstance>() : nullptr);
	if (!Env || !Env->GetMainState())
	{
		OutError = TEXT("UnLua environment is unavailable.");
		return false;
	}

	OutState = Env->GetMainState();
	const int32 InitialTop = lua_gettop(OutState);

#if !UE_BUILD_SHIPPING
	if (bAllowValidationChunk)
	{
		if (const FString* ValidationChunk =
			GValidationModuleChunks.Find(ModuleName))
		{
			const FTCHARToUTF8 ChunkUtf8(**ValidationChunk);
			const FTCHARToUTF8 NameUtf8(ModuleName);
			if (luaL_loadbufferx(OutState, ChunkUtf8.Get(), ChunkUtf8.Length(),
				NameUtf8.Get(), nullptr) != LUA_OK ||
				lua_pcall(OutState, 0, 1, 0) != LUA_OK)
			{
				const char* Error = lua_tostring(OutState, -1);
				OutError = Error ? UTF8_TO_TCHAR(Error)
					: TEXT("Unknown validation Lua error.");
				lua_settop(OutState, InitialTop);
				return false;
			}
			if (!lua_istable(OutState, -1))
			{
				OutError = FString::Printf(
					TEXT("%s validation chunk must return a table, got %s."),
					ModuleName, *LuaTypeName(OutState, -1));
				lua_settop(OutState, InitialTop);
				return false;
			}
			return true;
		}
	}
#endif

	const FString Chunk = FString::Printf(
		TEXT("local name = %s; package.loaded[name] = nil; "
			"return require(name)"),
		*FString::Printf(TEXT("%c%s%c"), TCHAR('\"'), ModuleName, TCHAR('\"')));
	const FTCHARToUTF8 ChunkUtf8(*Chunk);
	const FTCHARToUTF8 NameUtf8(ModuleName);

	if (luaL_loadbufferx(
		OutState, ChunkUtf8.Get(), ChunkUtf8.Length(), NameUtf8.Get(), nullptr)
		!= LUA_OK)
	{
		const char* Error = lua_tostring(OutState, -1);
		OutError = Error ? UTF8_TO_TCHAR(Error) : TEXT("Unknown Lua compile error.");
		lua_settop(OutState, InitialTop);
		return false;
	}
	if (lua_pcall(OutState, 0, 1, 0) != LUA_OK)
	{
		const char* Error = lua_tostring(OutState, -1);
		OutError = Error ? UTF8_TO_TCHAR(Error) : TEXT("Unknown Lua runtime error.");
		lua_settop(OutState, InitialTop);
		return false;
	}
	if (!lua_istable(OutState, -1))
	{
		OutError = FString::Printf(
			TEXT("%s must return a table, got %s."), ModuleName,
			*LuaTypeName(OutState, -1));
		lua_settop(OutState, InitialTop);
		return false;
	}
	return true;
}

bool LoadQuestTable(
	const UObject* Context, bool bForcePackagedDefault, lua_State*& OutState,
	ELuaQuestConfigSource& OutSource, FString& OutError)
{
#if !UE_BUILD_SHIPPING
	if (!bForcePackagedDefault &&
		GValidationModuleChunks.Contains(QuestModuleName))
	{
		OutSource = ELuaQuestConfigSource::PackagedDefault;
		return LoadModuleTable(
			Context, QuestModuleName, OutState, OutError, true);
	}
#endif

	const FString ExternalPath = GetExternalQuestConfigPathInternal();
	if (!bForcePackagedDefault &&
		IFileManager::Get().FileExists(*ExternalPath))
	{
		OutSource = ELuaQuestConfigSource::ExternalOverride;
		return LoadExternalQuestTable(
			Context, ExternalPath, OutState, OutError);
	}

	OutSource = ELuaQuestConfigSource::PackagedDefault;
	return LoadModuleTable(
		Context, QuestModuleName, OutState, OutError, false);
}

bool ReadRequiredString(
	lua_State* State, int32 TableIndex, const char* FieldName,
	const FString& FieldPath, FString& OutValue, FString& OutError)
{
	TableIndex = lua_absindex(State, TableIndex);
	lua_getfield(State, TableIndex, FieldName);
	if (lua_type(State, -1) != LUA_TSTRING)
	{
		OutError = FString::Printf(
			TEXT("%s must be string, got %s."), *FieldPath,
			*LuaTypeName(State, -1));
		lua_pop(State, 1);
		return false;
	}
	size_t Length = 0;
	const char* Value = lua_tolstring(State, -1, &Length);
	OutValue = FString(UTF8_TO_TCHAR(Value));
	lua_pop(State, 1);
	if (OutValue.TrimStartAndEnd().IsEmpty())
	{
		OutError = FString::Printf(TEXT("%s cannot be empty."), *FieldPath);
		return false;
	}
	return true;
}

bool ReadOptionalString(
	lua_State* State, int32 TableIndex, const char* FieldName,
	const FString& FieldPath, FString& OutValue, const FString& DefaultValue,
	FString& OutError)
{
	TableIndex = lua_absindex(State, TableIndex);
	lua_getfield(State, TableIndex, FieldName);
	if (lua_isnil(State, -1))
	{
		OutValue = DefaultValue;
		lua_pop(State, 1);
		return true;
	}
	if (lua_type(State, -1) != LUA_TSTRING)
	{
		OutError = FString::Printf(
			TEXT("%s must be string, got %s."), *FieldPath,
			*LuaTypeName(State, -1));
		lua_pop(State, 1);
		return false;
	}
	OutValue = UTF8_TO_TCHAR(lua_tostring(State, -1));
	lua_pop(State, 1);
	return true;
}

bool ReadRequiredInteger(
	lua_State* State, int32 TableIndex, const char* FieldName,
	const FString& FieldPath, int32& OutValue, FString& OutError)
{
	TableIndex = lua_absindex(State, TableIndex);
	lua_getfield(State, TableIndex, FieldName);
	if (!lua_isinteger(State, -1))
	{
		OutError = FString::Printf(
			TEXT("%s must be integer, got %s."), *FieldPath,
			*LuaTypeName(State, -1));
		lua_pop(State, 1);
		return false;
	}
	const lua_Integer Value = lua_tointeger(State, -1);
	lua_pop(State, 1);
	if (Value < MIN_int32 || Value > MAX_int32)
	{
		OutError = FString::Printf(TEXT("%s is outside int32 range."), *FieldPath);
		return false;
	}
	OutValue = static_cast<int32>(Value);
	return true;
}

bool ReadOptionalInteger(
	lua_State* State, int32 TableIndex, const char* FieldName,
	const FString& FieldPath, int32& OutValue, int32 DefaultValue,
	FString& OutError)
{
	TableIndex = lua_absindex(State, TableIndex);
	lua_getfield(State, TableIndex, FieldName);
	if (lua_isnil(State, -1))
	{
		OutValue = DefaultValue;
		lua_pop(State, 1);
		return true;
	}
	if (!lua_isinteger(State, -1))
	{
		OutError = FString::Printf(
			TEXT("%s must be integer, got %s."), *FieldPath,
			*LuaTypeName(State, -1));
		lua_pop(State, 1);
		return false;
	}
	const lua_Integer Value = lua_tointeger(State, -1);
	lua_pop(State, 1);
	if (Value < MIN_int32 || Value > MAX_int32)
	{
		OutError = FString::Printf(TEXT("%s is outside int32 range."), *FieldPath);
		return false;
	}
	OutValue = static_cast<int32>(Value);
	return true;
}

bool ReadOptionalBool(
	lua_State* State, int32 TableIndex, const char* FieldName,
	const FString& FieldPath, bool& OutValue, bool DefaultValue,
	FString& OutError)
{
	TableIndex = lua_absindex(State, TableIndex);
	lua_getfield(State, TableIndex, FieldName);
	if (lua_isnil(State, -1))
	{
		OutValue = DefaultValue;
		lua_pop(State, 1);
		return true;
	}
	if (!lua_isboolean(State, -1))
	{
		OutError = FString::Printf(
			TEXT("%s must be boolean, got %s."), *FieldPath,
			*LuaTypeName(State, -1));
		lua_pop(State, 1);
		return false;
	}
	OutValue = lua_toboolean(State, -1) != 0;
	lua_pop(State, 1);
	return true;
}

bool QuestDefinitionsEqual(
	const FLuaQuestDefinition& Left, const FLuaQuestDefinition& Right)
{
	return Left.QuestId == Right.QuestId &&
		Left.Title.EqualTo(Right.Title) &&
		Left.Description.EqualTo(Right.Description) &&
		Left.ObjectiveText.EqualTo(Right.ObjectiveText) &&
		Left.TargetCount == Right.TargetCount &&
		Left.RewardText.EqualTo(Right.RewardText) &&
		Left.SortOrder == Right.SortOrder &&
		Left.bVisible == Right.bVisible && Left.bCanAccept == Right.bCanAccept;
}

FString EscapeLuaString(const FString& Value)
{
	FString Escaped = Value;
	Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
	Escaped.ReplaceInline(TEXT("\r"), TEXT("\\r"));
	Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"));
	Escaped.ReplaceInline(TEXT("\t"), TEXT("\\t"));
	return Escaped;
}

FString SerializeQuestDefinitions(
	const TArray<FLuaQuestDefinition>& Definitions)
{
	FString Output =
		TEXT("-- External quest presentation override.\n")
		TEXT("-- Edit this UTF-8 file, then run Lua.ReloadQuests in-game.\n")
		TEXT("-- Runtime progress, completion and reward state remain in C++.\n")
		TEXT("return {\n");
	for (const FLuaQuestDefinition& Definition : Definitions)
	{
		Output += FString::Printf(
			TEXT("    {\n")
			TEXT("        QuestId = \"%s\",\n")
			TEXT("        Title = \"%s\",\n")
			TEXT("        Description = \"%s\",\n")
			TEXT("        ObjectiveText = \"%s\",\n")
			TEXT("        TargetCount = %d,\n")
			TEXT("        RewardText = \"%s\",\n")
			TEXT("        SortOrder = %d,\n")
			TEXT("        Visible = %s,\n")
			TEXT("        CanAccept = %s,\n")
			TEXT("    },\n"),
			*EscapeLuaString(Definition.QuestId.ToString()),
			*EscapeLuaString(Definition.Title.ToString()),
			*EscapeLuaString(Definition.Description.ToString()),
			*EscapeLuaString(Definition.ObjectiveText.ToString()),
			Definition.TargetCount,
			*EscapeLuaString(Definition.RewardText.ToString()),
			Definition.SortOrder,
			Definition.bVisible ? TEXT("true") : TEXT("false"),
			Definition.bCanAccept ? TEXT("true") : TEXT("false"));
	}
	Output += TEXT("}\n");
	return Output;
}

ULuaHotReloadSubsystem* ResolveSubsystem(UWorld* World)
{
	return World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<ULuaHotReloadSubsystem>()
		: nullptr;
}

#if !UE_BUILD_SHIPPING
void ReloadQuestsCommand(UWorld* World)
{
	if (ULuaHotReloadSubsystem* Subsystem = ResolveSubsystem(World))
	{
		Subsystem->ReloadQuests();
	}
	else
	{
		UE_LOG(LogLuaHotReload, Warning,
			TEXT("[LuaHotReload] Lua.ReloadQuests requires an active game world."));
	}
}

void ReloadSettingsCommand(UWorld* World)
{
	if (ULuaHotReloadSubsystem* Subsystem = ResolveSubsystem(World))
	{
		Subsystem->ReloadSettings();
	}
	else
	{
		UE_LOG(LogLuaHotReload, Warning,
			TEXT("[LuaHotReload] Lua.ReloadSettings requires an active game world."));
	}
}

void ReloadAllCommand(UWorld* World)
{
	if (ULuaHotReloadSubsystem* Subsystem = ResolveSubsystem(World))
	{
		Subsystem->ReloadAll();
	}
	else
	{
		UE_LOG(LogLuaHotReload, Warning,
			TEXT("[LuaHotReload] Lua.ReloadAll requires an active game world."));
	}
}

void ValidateHotReloadCommand(UWorld* World)
{
	if (const ULuaHotReloadSubsystem* Subsystem = ResolveSubsystem(World))
	{
		Subsystem->LogQuestHotReloadStatus();
	}
	else
	{
		UE_LOG(LogLuaHotReload, Error,
			TEXT("[LuaHotReload] Lua.ValidateHotReload requires an active "
				"game world."));
	}
}

void ExportQuestOverrideCommand(
	const TArray<FString>& Args, UWorld* World)
{
	ULuaHotReloadSubsystem* Subsystem = ResolveSubsystem(World);
	if (!Subsystem)
	{
		UE_LOG(LogLuaHotReload, Error,
			TEXT("[LuaHotReload] Lua.ExportQuestOverride requires an active "
				"game world."));
		return;
	}

	bool bForce = false;
	for (const FString& Arg : Args)
	{
		if (Arg.Equals(TEXT("Force"), ESearchCase::IgnoreCase))
		{
			bForce = true;
		}
		else
		{
			UE_LOG(LogLuaHotReload, Error,
				TEXT("[LuaHotReload] Unknown argument '%s'. Usage: "
					"Lua.ExportQuestOverride [Force]"), *Arg);
			return;
		}
	}
	Subsystem->ExportQuestOverride(bForce);
}

void RunHotReloadTestsCommand(UWorld* World)
{
	ULuaHotReloadSubsystem* Subsystem = ResolveSubsystem(World);
	if (!Subsystem)
	{
		UE_LOG(LogLuaHotReload, Error,
			TEXT("[LuaHotReload] Lua.RunHotReloadTests requires an active "
				"game world."));
		return;
	}

	AActor* ValidationOwner = nullptr;
	bool bSyntheticProgressReady = World->GetNetMode() == NM_Client;
	if (!bSyntheticProgressReady)
	{
		const FLuaQuestDefinition* TestDefinition =
			Subsystem->GetQuestDefinitions().FindByPredicate(
				[](const FLuaQuestDefinition& Definition)
				{
					return Definition.bVisible && Definition.bCanAccept;
				});
		if (TestDefinition)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.ObjectFlags |= RF_Transient;
			ValidationOwner = World->SpawnActor<AActor>(
				AActor::StaticClass(), FTransform::Identity, SpawnParameters);
			if (ValidationOwner)
			{
				UQuestComponent* TestComponent = NewObject<UQuestComponent>(
					ValidationOwner, TEXT("LuaHotReloadValidationQuestComponent"),
					RF_Transient);
				UQuestDataAsset* TestAsset = NewObject<UQuestDataAsset>(
					ValidationOwner, TEXT("LuaHotReloadValidationQuestAsset"),
					RF_Transient);
				ValidationOwner->AddInstanceComponent(TestComponent);
				TestComponent->RegisterComponent();
				TestAsset->QuestId = TestDefinition->QuestId;
				TestAsset->ObjectiveTargetId = TEXT("Validation.Objective");
				TestAsset->RequiredCount = TestDefinition->TargetCount;
				const bool bAccepted = TestComponent->AcceptQuest(TestAsset);
				const bool bProgressed = bAccepted &&
					TestComponent->NotifyObjectiveProgress(
						TestAsset->ObjectiveTargetId, 1);
				FQuestRuntimeEntry TestEntry;
				bSyntheticProgressReady = bProgressed &&
					TestComponent->GetQuestEntry(TestAsset->QuestId, TestEntry) &&
					TestEntry.CurrentProgress > 0;
			}
		}
	}
	if (!bSyntheticProgressReady)
	{
		UE_LOG(LogLuaHotReload, Error,
			TEXT("[LuaHotReload] Validation could not create non-zero quest state."));
	}

	auto CaptureAcceptedState = [World]()
	{
		TMap<FString, FString> Snapshot;
		for (TObjectIterator<UQuestComponent> It; It; ++It)
		{
			UQuestComponent* Component = *It;
			if (!IsValid(Component) || Component->GetWorld() != World) continue;
			for (const FQuestRuntimeEntry& Entry : Component->GetQuestEntries())
			{
				if (!Entry.bAccepted || !IsValid(Entry.Quest)) continue;
				const FString Key = Component->GetPathName() + TEXT("|") +
					Entry.Quest->QuestId.ToString();
				Snapshot.Add(Key, FString::Printf(TEXT("%d|%d|%d"),
					Entry.CurrentProgress, static_cast<int32>(Entry.State),
					Entry.bRewardClaimed ? 1 : 0));
			}
		}
		return Snapshot;
	};

	const TMap<FString, FString> Before = CaptureAcceptedState();
	bool bPassed = bSyntheticProgressReady;
	for (int32 Iteration = 1; Iteration <= 5; ++Iteration)
	{
		if (!Subsystem->ReloadAll())
		{
			UE_LOG(LogLuaHotReload, Error,
				TEXT("[LuaHotReload] Validation reload %d failed."), Iteration);
			bPassed = false;
			break;
		}
		for (TObjectIterator<UQuestComponent> It; It; ++It)
		{
			UQuestComponent* Component = *It;
			if (!IsValid(Component) || Component->GetWorld() != World) continue;
			TSet<FName> SeenIds;
			for (const FQuestRuntimeEntry& Entry : Component->GetQuestEntries())
			{
				if (!IsValid(Entry.Quest) ||
					SeenIds.Contains(Entry.Quest->QuestId))
				{
					bPassed = false;
					UE_LOG(LogLuaHotReload, Error,
						TEXT("[LuaHotReload] Duplicate or invalid quest row after "
							"reload %d."), Iteration);
					break;
				}
				SeenIds.Add(Entry.Quest->QuestId);
			}
		}
	}

	auto CaptureQuestCache = [Subsystem]()
	{
		FString Snapshot;
		for (const FLuaQuestDefinition& Definition :
			Subsystem->GetQuestDefinitions())
		{
			Snapshot += FString::Printf(TEXT("%s|%s|%d|%d|%d;"),
				*Definition.QuestId.ToString(), *Definition.Title.ToString(),
				Definition.TargetCount, Definition.bVisible ? 1 : 0,
				Definition.bCanAccept ? 1 : 0);
		}
		return Snapshot;
	};
	auto CaptureSettingsCache = [Subsystem]()
	{
		FString Snapshot;
		for (const FLuaSettingDefinition& Definition :
			Subsystem->GetSettingDefinitions())
		{
			Snapshot += FString::Printf(TEXT("%s|%s|%d;"),
				*Definition.SettingId.ToString(),
				*Definition.DisplayName.ToString(), Definition.Options.Num());
		}
		return Snapshot;
	};

	auto ExpectQuestRollback = [Subsystem, &bPassed,
		&CaptureQuestCache](const TCHAR* TestName, const FString& Chunk)
	{
		GValidationModuleChunks.Add(QuestModuleName, Chunk);
		const int32 Revision = Subsystem->GetQuestConfigRevision();
		const FString Cache = CaptureQuestCache();
		const bool bReloadRejected = !Subsystem->ReloadQuests();
		const bool bRolledBack = bReloadRejected &&
			Subsystem->GetQuestConfigRevision() == Revision &&
			CaptureQuestCache() == Cache;
		bPassed = bPassed && bRolledBack;
		if (bRolledBack)
		{
			UE_LOG(LogLuaHotReload, Display,
				TEXT("[LuaHotReload] Validation %s: rollback preserved"),
				TestName);
		}
		else
		{
			UE_LOG(LogLuaHotReload, Error,
				TEXT("[LuaHotReload] Validation %s: FAILED"), TestName);
		}
	};

	ExpectQuestRollback(TEXT("duplicate QuestId"), TEXT(R"(
		local q = { Title="T", Description="D", ObjectiveText="O", TargetCount=1 }
		return {
			{ QuestId="Validation.Duplicate", Title=q.Title, Description=q.Description, ObjectiveText=q.ObjectiveText, TargetCount=q.TargetCount },
			{ QuestId="Validation.Duplicate", Title=q.Title, Description=q.Description, ObjectiveText=q.ObjectiveText, TargetCount=q.TargetCount },
		}
	)"));
	ExpectQuestRollback(TEXT("missing Title"), TEXT(R"(
		return { { QuestId="Validation.Missing", Description="D", ObjectiveText="O", TargetCount=1 } }
	)"));
	ExpectQuestRollback(TEXT("wrong TargetCount type"), TEXT(R"(
		return { { QuestId="Validation.Type", Title="T", Description="D", ObjectiveText="O", TargetCount="1" } }
	)"));

	GValidationModuleChunks.Add(QuestModuleName, TEXT("return {}"));
	const bool bEmptyHandled = Subsystem->ReloadQuests() &&
		Subsystem->GetQuestDefinitions().IsEmpty();
	bPassed = bPassed && bEmptyHandled;
	if (bEmptyHandled)
	{
		UE_LOG(LogLuaHotReload, Display,
			TEXT("[LuaHotReload] Validation empty quest list: handled"));
	}
	else
	{
		UE_LOG(LogLuaHotReload, Error,
			TEXT("[LuaHotReload] Validation empty quest list: FAILED"));
	}

	GValidationModuleChunks.Add(SettingsModuleName, TEXT(R"(
		return { Settings = { { SettingId="Graphics.Quality", DisplayName="Q", Description="D", Options={ { Label="Low", Value="0" } } } } }
	)"));
	const int32 SettingsRevision = Subsystem->GetSettingsConfigRevision();
	const FString SettingsCache = CaptureSettingsCache();
	const bool bSettingsReloadRejected = !Subsystem->ReloadSettings();
	const bool bSettingsRolledBack = bSettingsReloadRejected &&
		Subsystem->GetSettingsConfigRevision() == SettingsRevision &&
		CaptureSettingsCache() == SettingsCache;
	bPassed = bPassed && bSettingsRolledBack;
	if (bSettingsRolledBack)
	{
		UE_LOG(LogLuaHotReload, Display,
			TEXT("[LuaHotReload] Validation wrong setting option type: "
				"rollback preserved"));
	}
	else
	{
		UE_LOG(LogLuaHotReload, Error,
			TEXT("[LuaHotReload] Validation wrong setting option type: FAILED"));
	}

	GValidationModuleChunks.Empty();
	const bool bRestored = Subsystem->ReloadAll();
	bPassed = bPassed && bRestored;
	if (!bRestored)
	{
		UE_LOG(LogLuaHotReload, Error,
			TEXT("[LuaHotReload] Validation failed to restore disk configuration."));
	}

	const TMap<FString, FString> After = CaptureAcceptedState();
	bool bStateMatches = Before.Num() == After.Num();
	for (const TPair<FString, FString>& Pair : Before)
	{
		const FString* AfterValue = After.Find(Pair.Key);
		bStateMatches = bStateMatches && AfterValue && *AfterValue == Pair.Value;
	}
	if (ValidationOwner)
	{
		ValidationOwner->Destroy();
	}
	if (bStateMatches && bPassed)
	{
		UE_LOG(LogLuaHotReload, Display,
			TEXT("[LuaHotReload] Validation PASSED: 5 reloads, unique rows, "
				"accepted quest state preserved."));
	}
	else
	{
		UE_LOG(LogLuaHotReload, Error,
			TEXT("[LuaHotReload] Validation FAILED: accepted quest state changed "
				"or duplicate rows were produced."));
	}
}

FAutoConsoleCommandWithWorld GReloadQuestsCommand(
	TEXT("Lua.ReloadQuests"), TEXT("Atomically reload Lua quest definitions."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&ReloadQuestsCommand));
FAutoConsoleCommandWithWorld GReloadSettingsCommand(
	TEXT("Lua.ReloadSettings"), TEXT("Atomically reload Lua setting definitions."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&ReloadSettingsCommand));
FAutoConsoleCommandWithWorld GReloadAllCommand(
	TEXT("Lua.ReloadAll"), TEXT("Reload all Demo Lua presentation configuration."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&ReloadAllCommand));
FAutoConsoleCommandWithWorld GValidateHotReloadCommand(
	TEXT("Lua.ValidateHotReload"),
	TEXT("Print non-destructive quest hot-reload path and source diagnostics."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&ValidateHotReloadCommand));
FAutoConsoleCommandWithWorldAndArgs GExportQuestOverrideCommand(
	TEXT("Lua.ExportQuestOverride"),
	TEXT("Export packaged quest defaults to the external override path. "
		"Pass Force to replace an existing file."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		&ExportQuestOverrideCommand));
FAutoConsoleCommandWithWorld GRunHotReloadTestsCommand(
	TEXT("Lua.RunHotReloadTests"),
	TEXT("Run destructive quest/settings hot-reload rollback tests."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&RunHotReloadTestsCommand));
#endif
}

void ULuaHotReloadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Establish an always-valid packaged baseline first. If an external file was
	// left broken between launches, its failure can then retain this baseline.
	TArray<FLuaQuestDefinition> PackagedDefinitions;
	ELuaQuestConfigSource PackagedSource =
		ELuaQuestConfigSource::PackagedDefault;
	FString PackagedError;
	if (ParseQuestModule(
		PackagedDefinitions, PackagedSource, PackagedError, true))
	{
		CommitQuestDefinitions(
			MoveTemp(PackagedDefinitions), PackagedSource, false);
	}
	else
	{
		UE_LOG(LogLuaHotReload, Error,
			TEXT("[LuaHotReload] Packaged Quest.QuestConfig failed during "
				"initialization: %s"), *PackagedError);
	}

	ReloadSettings();
	if (IFileManager::Get().FileExists(*GetExternalQuestConfigPath()))
	{
		ReloadQuests();
	}
}

void ULuaHotReloadSubsystem::Deinitialize()
{
	OnQuestDefinitionsChanged.Clear();
	OnSettingsDefinitionsChanged.Clear();
	QuestViewAssets.Empty();
	QuestDefinitions.Empty();
	SettingDefinitions.Empty();
	LastExternalQuestLoadError.Empty();
	Super::Deinitialize();
}

bool ULuaHotReloadSubsystem::ReloadQuests()
{
	TArray<FLuaQuestDefinition> ParsedDefinitions;
	ELuaQuestConfigSource ParsedSource =
		ELuaQuestConfigSource::PackagedDefault;
	FString Error;
	if (!ParseQuestModule(ParsedDefinitions, ParsedSource, Error))
	{
		bool bExternalAttempt = IFileManager::Get().FileExists(
			*GetExternalQuestConfigPath());
#if !UE_BUILD_SHIPPING
		bExternalAttempt = bExternalAttempt &&
			!GValidationModuleChunks.Contains(QuestModuleName);
#endif
		if (bExternalAttempt)
		{
			LastExternalQuestLoadError = Error;
		}
		UE_LOG(LogLuaHotReload, Error,
			TEXT("[LuaHotReload] Quest reload failed. Source=%s Path=%s "
				"Error=%s Previous configuration retained at revision %d."),
			bExternalAttempt ? TEXT("ExternalOverride") :
				TEXT("PackagedDefault"),
			bExternalAttempt ? *GetExternalQuestConfigPath() :
				QuestModuleName,
			*Error, QuestConfigRevision);
		return false;
	}
	if (ParsedSource == ELuaQuestConfigSource::ExternalOverride)
	{
		LastExternalQuestLoadError.Empty();
	}
	CommitQuestDefinitions(MoveTemp(ParsedDefinitions), ParsedSource);
	return true;
}

void ULuaHotReloadSubsystem::CommitQuestDefinitions(
	TArray<FLuaQuestDefinition>&& NewDefinitions,
	ELuaQuestConfigSource NewSource, bool bBroadcast)
{
	TMap<FName, const FLuaQuestDefinition*> PreviousById;
	for (const FLuaQuestDefinition& Definition : QuestDefinitions)
	{
		PreviousById.Add(Definition.QuestId, &Definition);
	}
	TSet<FName> NewIds;
	for (const FLuaQuestDefinition& Definition : NewDefinitions)
	{
		NewIds.Add(Definition.QuestId);
		const FLuaQuestDefinition* const* Previous =
			PreviousById.Find(Definition.QuestId);
		if (!Previous)
		{
			UE_LOG(LogLuaHotReload, Log,
				TEXT("[LuaHotReload] Added: %s"),
				*Definition.QuestId.ToString());
		}
		else if (!QuestDefinitionsEqual(**Previous, Definition))
		{
			UE_LOG(LogLuaHotReload, Log,
				TEXT("[LuaHotReload] Updated: %s"),
				*Definition.QuestId.ToString());
		}
	}
	for (const FLuaQuestDefinition& Previous : QuestDefinitions)
	{
		if (!NewIds.Contains(Previous.QuestId))
		{
			UE_LOG(LogLuaHotReload, Warning,
				TEXT("[LuaHotReload] Removed definition: %s. Any accepted "
					"runtime state is preserved by UQuestComponent."),
				*Previous.QuestId.ToString());
		}
	}

	QuestDefinitions = MoveTemp(NewDefinitions);
	QuestConfigSource = NewSource;
	RebuildQuestViewAssets();
	++QuestConfigRevision;
	UE_LOG(LogLuaHotReload, Display,
		TEXT("[LuaHotReload] Quest configuration committed. Source=%s "
			"Revision=%d Definitions=%d Path=%s"),
		QuestConfigSourceName(QuestConfigSource), QuestConfigRevision,
		QuestDefinitions.Num(),
		QuestConfigSource == ELuaQuestConfigSource::ExternalOverride
			? *GetExternalQuestConfigPath() : QuestModuleName);
	if (bBroadcast)
	{
		OnQuestDefinitionsChanged.Broadcast(QuestConfigRevision);
	}
}

FString ULuaHotReloadSubsystem::GetExternalQuestConfigPath() const
{
	return GetExternalQuestConfigPathInternal();
}

bool ULuaHotReloadSubsystem::ExportQuestOverride(bool bForce)
{
	const FString OutputPath = GetExternalQuestConfigPath();
	if (IFileManager::Get().FileExists(*OutputPath) && !bForce)
	{
		UE_LOG(LogLuaHotReload, Warning,
			TEXT("[LuaHotReload] Export skipped: external quest override "
				"already exists and was not overwritten. Path=%s "
				"Use 'Lua.ExportQuestOverride Force' to replace it."),
			*OutputPath);
		return false;
	}

	TArray<FLuaQuestDefinition> PackagedDefinitions;
	ELuaQuestConfigSource Source = ELuaQuestConfigSource::PackagedDefault;
	FString Error;
	if (!ParseQuestModule(PackagedDefinitions, Source, Error, true))
	{
		UE_LOG(LogLuaHotReload, Error,
			TEXT("[LuaHotReload] Export failed: packaged default could not "
				"be loaded. Error=%s Path=%s"), *Error, *OutputPath);
		return false;
	}

	const FString Directory = FPaths::GetPath(OutputPath);
	if (!IFileManager::Get().DirectoryExists(*Directory) &&
		!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		UE_LOG(LogLuaHotReload, Error,
			TEXT("[LuaHotReload] Export failed: could not create directory. "
				"Path=%s"), *Directory);
		return false;
	}

	const FString FileContents =
		SerializeQuestDefinitions(PackagedDefinitions);
	if (!FFileHelper::SaveStringToFile(
		FileContents, *OutputPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogLuaHotReload, Error,
			TEXT("[LuaHotReload] Export failed: could not write UTF-8 file. "
				"Path=%s"), *OutputPath);
		return false;
	}

	UE_LOG(LogLuaHotReload, Display,
		TEXT("[LuaHotReload] Exported packaged quest defaults. "
			"Definitions=%d Encoding=UTF-8 Path=%s"),
		PackagedDefinitions.Num(), *OutputPath);
	return true;
}

void ULuaHotReloadSubsystem::LogQuestHotReloadStatus() const
{
	const FString SavedDir = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir());
	const FString ExternalPath = GetExternalQuestConfigPath();
	const bool bExternalExists =
		IFileManager::Get().FileExists(*ExternalPath);
	UE_LOG(LogLuaHotReload, Display,
		TEXT("[LuaHotReload] Status ProjectSavedDir=%s"), *SavedDir);
	UE_LOG(LogLuaHotReload, Display,
		TEXT("[LuaHotReload] Status ExternalQuestConfig=%s Exists=%s"),
		*ExternalPath, bExternalExists ? TEXT("true") : TEXT("false"));
	UE_LOG(LogLuaHotReload, Display,
		TEXT("[LuaHotReload] Status Source=%s Revision=%d Definitions=%d"),
		QuestConfigSourceName(QuestConfigSource), QuestConfigRevision,
		QuestDefinitions.Num());
	UE_LOG(LogLuaHotReload, Display,
		TEXT("[LuaHotReload] Status LastExternalLoadError=%s"),
		LastExternalQuestLoadError.IsEmpty()
			? TEXT("<none>") : *LastExternalQuestLoadError);
}

bool ULuaHotReloadSubsystem::ReloadSettings()
{
	TArray<FLuaSettingDefinition> ParsedDefinitions;
	FLuaSettingsUiText ParsedUiText;
	FString Error;
	if (!ParseSettingsModule(ParsedDefinitions, ParsedUiText, Error))
	{
		UE_LOG(LogLuaHotReload, Error,
			TEXT("[LuaHotReload] Settings.SettingsConfig reload failed: %s "
				"Previous configuration retained."), *Error);
		return false;
	}
	CommitSettingDefinitions(MoveTemp(ParsedDefinitions), MoveTemp(ParsedUiText));
	return true;
}

void ULuaHotReloadSubsystem::CommitSettingDefinitions(
	TArray<FLuaSettingDefinition>&& NewDefinitions,
	FLuaSettingsUiText&& NewUiText, bool bBroadcast)
{
	SettingDefinitions = MoveTemp(NewDefinitions);
	SettingsUiText = MoveTemp(NewUiText);
	++SettingsConfigRevision;
	UE_LOG(LogLuaHotReload, Display,
		TEXT("[LuaHotReload] Settings configuration reloaded."));
	UE_LOG(LogLuaHotReload, Display,
		TEXT("[LuaHotReload] Loaded %d setting definitions."),
		SettingDefinitions.Num());
	if (bBroadcast)
	{
		OnSettingsDefinitionsChanged.Broadcast(SettingsConfigRevision);
	}
}

bool ULuaHotReloadSubsystem::ReloadAll()
{
	TArray<FLuaQuestDefinition> ParsedQuests;
	TArray<FLuaSettingDefinition> ParsedSettings;
	FLuaSettingsUiText ParsedUiText;
	ELuaQuestConfigSource ParsedQuestSource =
		ELuaQuestConfigSource::PackagedDefault;
	FString QuestError;
	FString SettingsError;
	const bool bQuestsValid = ParseQuestModule(
		ParsedQuests, ParsedQuestSource, QuestError);
	const bool bSettingsValid = ParseSettingsModule(
		ParsedSettings, ParsedUiText, SettingsError);
	if (!bQuestsValid || !bSettingsValid)
	{
		bool bExternalAttempt = IFileManager::Get().FileExists(
			*GetExternalQuestConfigPath());
#if !UE_BUILD_SHIPPING
		bExternalAttempt = bExternalAttempt &&
			!GValidationModuleChunks.Contains(QuestModuleName);
#endif
		if (!bQuestsValid && bExternalAttempt)
		{
			LastExternalQuestLoadError = QuestError;
		}
		UE_LOG(LogLuaHotReload, Error,
			TEXT("[LuaHotReload] Lua.ReloadAll rejected. Quest: %s; Settings: %s. "
				"Both previous configurations retained."),
			bQuestsValid ? TEXT("valid") : *QuestError,
			bSettingsValid ? TEXT("valid") : *SettingsError);
		return false;
	}
	if (ParsedQuestSource == ELuaQuestConfigSource::ExternalOverride)
	{
		LastExternalQuestLoadError.Empty();
	}
	// Replace both validated caches before either observer is notified.
	CommitQuestDefinitions(
		MoveTemp(ParsedQuests), ParsedQuestSource, false);
	CommitSettingDefinitions(
		MoveTemp(ParsedSettings), MoveTemp(ParsedUiText), false);
	OnQuestDefinitionsChanged.Broadcast(QuestConfigRevision);
	OnSettingsDefinitionsChanged.Broadcast(SettingsConfigRevision);
	UE_LOG(LogLuaHotReload, Display,
		TEXT("[LuaHotReload] All configuration reloaded atomically."));
	return true;
}

const FLuaQuestDefinition* ULuaHotReloadSubsystem::FindQuestDefinition(
	FName QuestId) const
{
	return QuestDefinitions.FindByPredicate(
		[QuestId](const FLuaQuestDefinition& Definition)
		{
			return Definition.QuestId == QuestId;
		});
}

UQuestDataAsset* ULuaHotReloadSubsystem::ResolveQuestViewAsset(
	UQuestDataAsset* NativeQuest) const
{
	if (!IsValid(NativeQuest)) return NativeQuest;
	if (UQuestDataAsset* ViewAsset = FindQuestViewAsset(NativeQuest->QuestId))
	{
		return ViewAsset;
	}
	return NativeQuest;
}

UQuestDataAsset* ULuaHotReloadSubsystem::FindQuestViewAsset(FName QuestId) const
{
	const TObjectPtr<UQuestDataAsset>* Found = QuestViewAssets.Find(QuestId);
	return Found ? Found->Get() : nullptr;
}

int32 ULuaHotReloadSubsystem::GetEffectiveTargetCount(
	const UQuestDataAsset* NativeQuest) const
{
	if (IsValid(NativeQuest))
	{
		if (const FLuaQuestDefinition* Definition =
			FindQuestDefinition(NativeQuest->QuestId))
		{
			return FMath::Max(1, Definition->TargetCount);
		}
		return FMath::Max(1, NativeQuest->RequiredCount);
	}
	return 1;
}

bool ULuaHotReloadSubsystem::CanAcceptQuestDefinition(FName QuestId) const
{
	const FLuaQuestDefinition* Definition = FindQuestDefinition(QuestId);
	return Definition && Definition->bVisible && Definition->bCanAccept;
}

int32 ULuaHotReloadSubsystem::GetSettingValue(FName SettingId) const
{
	const UGameUserSettings* UserSettings =
		GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!UserSettings) return INDEX_NONE;
	if (SettingId == TEXT("Graphics.Quality"))
	{
		return UserSettings->GetOverallScalabilityLevel();
	}
	return INDEX_NONE;
}

bool ULuaHotReloadSubsystem::SetSettingValue(FName SettingId, int32 Value)
{
	UGameUserSettings* UserSettings =
		GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!UserSettings) return false;
	if (SettingId == TEXT("Graphics.Quality") && Value >= 0 && Value <= 4)
	{
		UserSettings->SetOverallScalabilityLevel(Value);
		return true;
	}
	UE_LOG(LogLuaHotReload, Warning,
		TEXT("[LuaHotReload] Rejected setting %s value %d."),
		*SettingId.ToString(), Value);
	return false;
}

void ULuaHotReloadSubsystem::ApplySettings()
{
	if (UGameUserSettings* UserSettings =
		GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		UserSettings->ApplySettings(false);
	}
}

void ULuaHotReloadSubsystem::SaveSettings()
{
	if (UGameUserSettings* UserSettings =
		GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		UserSettings->SaveSettings();
	}
}

void ULuaHotReloadSubsystem::ResetSettings()
{
	if (UGameUserSettings* UserSettings =
		GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		UserSettings->SetToDefaults();
		OnSettingsDefinitionsChanged.Broadcast(SettingsConfigRevision);
	}
}

bool ULuaHotReloadSubsystem::ParseQuestModule(
	TArray<FLuaQuestDefinition>& OutDefinitions,
	ELuaQuestConfigSource& OutSource, FString& OutError,
	bool bForcePackagedDefault) const
{
	lua_State* State = nullptr;
	if (!LoadQuestTable(
		this, bForcePackagedDefault, State, OutSource, OutError))
	{
		return false;
	}
	FLuaStackGuard StackGuard(State, lua_gettop(State) - 1);
	const int32 RootIndex = lua_absindex(State, -1);
	const int32 Count = static_cast<int32>(lua_rawlen(State, RootIndex));
	TSet<FName> QuestIds;

	for (int32 Index = 1; Index <= Count; ++Index)
	{
		lua_rawgeti(State, RootIndex, Index);
		if (!lua_istable(State, -1))
		{
			OutError = FString::Printf(
				TEXT("Quest.QuestConfig[%d] must be table, got %s."),
				Index, *LuaTypeName(State, -1));
			return false;
		}
		const int32 EntryIndex = lua_absindex(State, -1);
		const FString EntryPath = FString::Printf(
			TEXT("Quest.QuestConfig[%d]"), Index);
		FString QuestIdString;
		FString Title;
		FString Description;
		FString ObjectiveText;
		FString RewardText;
		FLuaQuestDefinition Definition;
		if (!ReadRequiredString(State, EntryIndex, "QuestId",
			EntryPath + TEXT(".QuestId"), QuestIdString, OutError) ||
			!ReadRequiredString(State, EntryIndex, "Title",
			EntryPath + TEXT(".Title"), Title, OutError) ||
			!ReadRequiredString(State, EntryIndex, "Description",
			EntryPath + TEXT(".Description"), Description, OutError) ||
			!ReadRequiredString(State, EntryIndex, "ObjectiveText",
			EntryPath + TEXT(".ObjectiveText"), ObjectiveText, OutError) ||
			!ReadRequiredInteger(State, EntryIndex, "TargetCount",
			EntryPath + TEXT(".TargetCount"), Definition.TargetCount, OutError) ||
			!ReadOptionalString(State, EntryIndex, "RewardText",
			EntryPath + TEXT(".RewardText"), RewardText, TEXT(""), OutError) ||
			!ReadOptionalInteger(State, EntryIndex, "SortOrder",
			EntryPath + TEXT(".SortOrder"), Definition.SortOrder, 0, OutError) ||
			!ReadOptionalBool(State, EntryIndex, "Visible",
			EntryPath + TEXT(".Visible"), Definition.bVisible, true, OutError) ||
			!ReadOptionalBool(State, EntryIndex, "CanAccept",
			EntryPath + TEXT(".CanAccept"), Definition.bCanAccept, true, OutError))
		{
			return false;
		}
		if (Definition.TargetCount < 1 || Definition.TargetCount > 1000000)
		{
			OutError = FString::Printf(
				TEXT("%s.TargetCount must be in [1, 1000000]."), *EntryPath);
			return false;
		}
		Definition.QuestId = FName(*QuestIdString);
		if (Definition.QuestId.IsNone() || QuestIds.Contains(Definition.QuestId))
		{
			OutError = FString::Printf(TEXT("%s.QuestId '%s' is duplicate or invalid."),
				*EntryPath, *QuestIdString);
			return false;
		}
		QuestIds.Add(Definition.QuestId);
		Definition.Title = FText::FromString(Title);
		Definition.Description = FText::FromString(Description);
		Definition.ObjectiveText = FText::FromString(ObjectiveText);
		Definition.RewardText = FText::FromString(RewardText);
		OutDefinitions.Add(MoveTemp(Definition));
		lua_pop(State, 1);
	}

	OutDefinitions.Sort([](
		const FLuaQuestDefinition& Left, const FLuaQuestDefinition& Right)
	{
		return Left.SortOrder == Right.SortOrder
			? Left.QuestId.LexicalLess(Right.QuestId)
			: Left.SortOrder < Right.SortOrder;
	});
	return true;
}

bool ULuaHotReloadSubsystem::ParseSettingsModule(
	TArray<FLuaSettingDefinition>& OutDefinitions,
	FLuaSettingsUiText& OutUiText, FString& OutError) const
{
	lua_State* State = nullptr;
	if (!LoadModuleTable(this, SettingsModuleName, State, OutError)) return false;
	FLuaStackGuard StackGuard(State, lua_gettop(State) - 1);
	const int32 RootIndex = lua_absindex(State, -1);

	lua_getfield(State, RootIndex, "Settings");
	const int32 DefinitionsIndex = lua_istable(State, -1)
		? lua_absindex(State, -1) : RootIndex;
	if (DefinitionsIndex == RootIndex) lua_pop(State, 1);

	const int32 Count = static_cast<int32>(lua_rawlen(State, DefinitionsIndex));
	TSet<FName> SettingIds;
	for (int32 Index = 1; Index <= Count; ++Index)
	{
		lua_rawgeti(State, DefinitionsIndex, Index);
		if (!lua_istable(State, -1))
		{
			OutError = FString::Printf(
				TEXT("Settings.SettingsConfig.Settings[%d] must be table, got %s."),
				Index, *LuaTypeName(State, -1));
			return false;
		}
		const int32 EntryIndex = lua_absindex(State, -1);
		const FString EntryPath = FString::Printf(
			TEXT("Settings.SettingsConfig.Settings[%d]"), Index);
		FString SettingIdString;
		FString DisplayName;
		FString Description;
		FString Category;
		FLuaSettingDefinition Definition;
		if (!ReadRequiredString(State, EntryIndex, "SettingId",
			EntryPath + TEXT(".SettingId"), SettingIdString, OutError) ||
			!ReadRequiredString(State, EntryIndex, "DisplayName",
			EntryPath + TEXT(".DisplayName"), DisplayName, OutError) ||
			!ReadRequiredString(State, EntryIndex, "Description",
			EntryPath + TEXT(".Description"), Description, OutError) ||
			!ReadOptionalString(State, EntryIndex, "Category",
			EntryPath + TEXT(".Category"), Category, TEXT("Video"), OutError) ||
			!ReadOptionalInteger(State, EntryIndex, "SortOrder",
			EntryPath + TEXT(".SortOrder"), Definition.SortOrder, 0, OutError) ||
			!ReadOptionalBool(State, EntryIndex, "Visible",
			EntryPath + TEXT(".Visible"), Definition.bVisible, true, OutError))
		{
			return false;
		}
		Definition.SettingId = FName(*SettingIdString);
		if (Definition.SettingId != TEXT("Graphics.Quality"))
		{
			OutError = FString::Printf(
				TEXT("%s.SettingId '%s' is not in the C++ setting whitelist."),
				*EntryPath, *SettingIdString);
			return false;
		}
		if (SettingIds.Contains(Definition.SettingId))
		{
			OutError = FString::Printf(TEXT("%s.SettingId '%s' is duplicate."),
				*EntryPath, *SettingIdString);
			return false;
		}
		SettingIds.Add(Definition.SettingId);
		Definition.DisplayName = FText::FromString(DisplayName);
		Definition.Description = FText::FromString(Description);
		Definition.Category = FName(*Category);

		lua_getfield(State, EntryIndex, "Options");
		if (!lua_istable(State, -1))
		{
			OutError = FString::Printf(TEXT("%s.Options must be table, got %s."),
				*EntryPath, *LuaTypeName(State, -1));
			return false;
		}
		const int32 OptionsIndex = lua_absindex(State, -1);
		const int32 OptionCount = static_cast<int32>(
			lua_rawlen(State, OptionsIndex));
		if (OptionCount == 0)
		{
			OutError = EntryPath + TEXT(".Options cannot be empty.");
			return false;
		}
		TSet<int32> OptionValues;
		for (int32 OptionIndex = 1; OptionIndex <= OptionCount; ++OptionIndex)
		{
			lua_rawgeti(State, OptionsIndex, OptionIndex);
			if (!lua_istable(State, -1))
			{
				OutError = FString::Printf(TEXT("%s.Options[%d] must be table."),
					*EntryPath, OptionIndex);
				return false;
			}
			const int32 OptionTableIndex = lua_absindex(State, -1);
			const FString OptionPath = FString::Printf(
				TEXT("%s.Options[%d]"), *EntryPath, OptionIndex);
			FString Label;
			FLuaSettingOption Option;
			if (!ReadRequiredString(State, OptionTableIndex, "Label",
				OptionPath + TEXT(".Label"), Label, OutError) ||
				!ReadRequiredInteger(State, OptionTableIndex, "Value",
				OptionPath + TEXT(".Value"), Option.Value, OutError))
			{
				return false;
			}
			if (Option.Value < 0 || Option.Value > 4 ||
				OptionValues.Contains(Option.Value))
			{
				OutError = FString::Printf(
					TEXT("%s.Value must be unique and in [0, 4]."), *OptionPath);
				return false;
			}
			OptionValues.Add(Option.Value);
			Option.Label = FText::FromString(Label);
			Definition.Options.Add(MoveTemp(Option));
			lua_pop(State, 1);
		}
		lua_pop(State, 1);
		OutDefinitions.Add(MoveTemp(Definition));
		lua_pop(State, 1);
	}
	if (DefinitionsIndex != RootIndex) lua_pop(State, 1);

	lua_getfield(State, RootIndex, "Text");
	if (lua_istable(State, -1))
	{
		const int32 TextIndex = lua_absindex(State, -1);
		FString Value;
		auto ReadText = [&](const char* Field, const TCHAR* Default, FText& Output)
		{
			if (!ReadOptionalString(State, TextIndex, Field,
				FString::Printf(TEXT("Settings.SettingsConfig.Text.%s"),
					UTF8_TO_TCHAR(Field)), Value, Default, OutError))
			{
				return false;
			}
			Output = FText::FromString(Value);
			return true;
		};
		if (!ReadText("PageTitle", TEXT("Settings"), OutUiText.PageTitle) ||
			!ReadText("ApplyButton", TEXT("Apply"), OutUiText.ApplyButton) ||
			!ReadText("ResetButton", TEXT("Reset"), OutUiText.ResetButton) ||
			!ReadText("VideoTab", TEXT("Video"), OutUiText.VideoTab) ||
			!ReadText("AudioTab", TEXT("Audio"), OutUiText.AudioTab) ||
			!ReadText("KeysTab", TEXT("Keys"), OutUiText.KeysTab))
		{
			return false;
		}
	}
	else if (!lua_isnil(State, -1))
	{
		OutError = FString::Printf(
			TEXT("Settings.SettingsConfig.Text must be table, got %s."),
			*LuaTypeName(State, -1));
		return false;
	}
	else
	{
		OutUiText.PageTitle = FText::FromString(TEXT("Settings"));
		OutUiText.ApplyButton = FText::FromString(TEXT("Apply"));
		OutUiText.ResetButton = FText::FromString(TEXT("Reset"));
		OutUiText.VideoTab = FText::FromString(TEXT("Video"));
		OutUiText.AudioTab = FText::FromString(TEXT("Audio"));
		OutUiText.KeysTab = FText::FromString(TEXT("Keys"));
	}
	lua_pop(State, 1);

	OutDefinitions.Sort([](
		const FLuaSettingDefinition& Left, const FLuaSettingDefinition& Right)
	{
		return Left.SortOrder == Right.SortOrder
			? Left.SettingId.LexicalLess(Right.SettingId)
			: Left.SortOrder < Right.SortOrder;
	});
	return true;
}

void ULuaHotReloadSubsystem::RebuildQuestViewAssets()
{
	TMap<FName, TObjectPtr<UQuestDataAsset>> NewAssets;
	for (const FLuaQuestDefinition& Definition : QuestDefinitions)
	{
		UQuestDataAsset* ViewAsset = NewObject<UQuestDataAsset>(
			this, NAME_None, RF_Transient);
		ViewAsset->QuestId = Definition.QuestId;
		ViewAsset->Title = Definition.Title;
		ViewAsset->Description = Definition.Description;
		ViewAsset->ObjectiveText = Definition.ObjectiveText;
		ViewAsset->RequiredCount = Definition.TargetCount;
		ViewAsset->RewardText = Definition.RewardText;
		NewAssets.Add(Definition.QuestId, ViewAsset);
	}
	QuestViewAssets = MoveTemp(NewAssets);
}
