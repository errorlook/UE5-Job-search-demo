#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LuaHotReloadSubsystem.generated.h"

class UQuestDataAsset;

UENUM(BlueprintType)
enum class ELuaQuestConfigSource : uint8
{
	PackagedDefault,
	ExternalOverride
};

USTRUCT(BlueprintType)
struct FLuaQuestDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Quest")
	FName QuestId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Quest")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Quest")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Quest")
	FText ObjectiveText;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Quest")
	int32 TargetCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Quest")
	FText RewardText;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Quest")
	int32 SortOrder = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Quest")
	bool bVisible = true;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Quest")
	bool bCanAccept = true;
};

USTRUCT(BlueprintType)
struct FLuaSettingOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Settings")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Settings")
	int32 Value = 0;
};

USTRUCT(BlueprintType)
struct FLuaSettingDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Settings")
	FName SettingId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Settings")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Settings")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Settings")
	FName Category = TEXT("Video");

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Settings")
	TArray<FLuaSettingOption> Options;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Settings")
	int32 SortOrder = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Settings")
	bool bVisible = true;
};

USTRUCT(BlueprintType)
struct FLuaSettingsUiText
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Settings")
	FText PageTitle;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Settings")
	FText ApplyButton;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Settings")
	FText ResetButton;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Settings")
	FText VideoTab;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Settings")
	FText AudioTab;

	UPROPERTY(BlueprintReadOnly, Category = "Lua Hot Reload|Settings")
	FText KeysTab;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FQuestDefinitionsChangedSignature, int32, NewRevision);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSettingsDefinitionsChangedSignature, int32, NewRevision);

/**
 * Per-game-instance, validated presentation configuration loaded from Lua.
 * Gameplay state and engine setting application remain in their native owners.
 */
UCLASS(BlueprintType)
class DEMO_API ULuaHotReloadSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Lua Hot Reload")
	bool ReloadQuests();

	UFUNCTION(BlueprintCallable, Category = "Lua Hot Reload")
	bool ReloadSettings();

	UFUNCTION(BlueprintCallable, Category = "Lua Hot Reload")
	bool ReloadAll();

	UFUNCTION(BlueprintCallable, Category = "Lua Hot Reload|Quest")
	bool ExportQuestOverride(bool bForce = false);

	void LogQuestHotReloadStatus() const;

	UFUNCTION(BlueprintPure, Category = "Lua Hot Reload|Quest")
	const TArray<FLuaQuestDefinition>& GetQuestDefinitions() const
	{
		return QuestDefinitions;
	}

	const FLuaQuestDefinition* FindQuestDefinition(FName QuestId) const;
	UQuestDataAsset* ResolveQuestViewAsset(UQuestDataAsset* NativeQuest) const;
	UQuestDataAsset* FindQuestViewAsset(FName QuestId) const;
	int32 GetEffectiveTargetCount(const UQuestDataAsset* NativeQuest) const;
	bool CanAcceptQuestDefinition(FName QuestId) const;

	UFUNCTION(BlueprintPure, Category = "Lua Hot Reload|Quest")
	int32 GetQuestConfigRevision() const { return QuestConfigRevision; }

	UFUNCTION(BlueprintPure, Category = "Lua Hot Reload|Quest")
	FString GetExternalQuestConfigPath() const;

	UFUNCTION(BlueprintPure, Category = "Lua Hot Reload|Quest")
	ELuaQuestConfigSource GetQuestConfigSource() const
	{
		return QuestConfigSource;
	}

	UFUNCTION(BlueprintPure, Category = "Lua Hot Reload|Quest")
	FString GetLastExternalQuestLoadError() const
	{
		return LastExternalQuestLoadError;
	}

	UFUNCTION(BlueprintPure, Category = "Lua Hot Reload|Settings")
	const TArray<FLuaSettingDefinition>& GetSettingDefinitions() const
	{
		return SettingDefinitions;
	}

	UFUNCTION(BlueprintPure, Category = "Lua Hot Reload|Settings")
	FLuaSettingsUiText GetSettingsUiText() const { return SettingsUiText; }

	UFUNCTION(BlueprintPure, Category = "Lua Hot Reload|Settings")
	int32 GetSettingValue(FName SettingId) const;

	UFUNCTION(BlueprintCallable, Category = "Lua Hot Reload|Settings")
	bool SetSettingValue(FName SettingId, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Lua Hot Reload|Settings")
	void ApplySettings();

	UFUNCTION(BlueprintCallable, Category = "Lua Hot Reload|Settings")
	void SaveSettings();

	UFUNCTION(BlueprintCallable, Category = "Lua Hot Reload|Settings")
	void ResetSettings();

	UFUNCTION(BlueprintPure, Category = "Lua Hot Reload|Settings")
	int32 GetSettingsConfigRevision() const { return SettingsConfigRevision; }

	UPROPERTY(BlueprintAssignable, Category = "Lua Hot Reload|Quest")
	FQuestDefinitionsChangedSignature OnQuestDefinitionsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lua Hot Reload|Settings")
	FSettingsDefinitionsChangedSignature OnSettingsDefinitionsChanged;

private:
	bool ParseQuestModule(
		TArray<FLuaQuestDefinition>& OutDefinitions,
		ELuaQuestConfigSource& OutSource, FString& OutError,
		bool bForcePackagedDefault = false) const;
	bool ParseSettingsModule(
		TArray<FLuaSettingDefinition>& OutDefinitions,
		FLuaSettingsUiText& OutUiText, FString& OutError) const;
	void CommitQuestDefinitions(
		TArray<FLuaQuestDefinition>&& NewDefinitions,
		ELuaQuestConfigSource NewSource, bool bBroadcast = true);
	void CommitSettingDefinitions(
		TArray<FLuaSettingDefinition>&& NewDefinitions,
		FLuaSettingsUiText&& NewUiText, bool bBroadcast = true);
	void RebuildQuestViewAssets();

	UPROPERTY(Transient)
	TArray<FLuaQuestDefinition> QuestDefinitions;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UQuestDataAsset>> QuestViewAssets;

	UPROPERTY(Transient)
	TArray<FLuaSettingDefinition> SettingDefinitions;

	UPROPERTY(Transient)
	FLuaSettingsUiText SettingsUiText;

	int32 QuestConfigRevision = 0;
	int32 SettingsConfigRevision = 0;
	ELuaQuestConfigSource QuestConfigSource =
		ELuaQuestConfigSource::PackagedDefault;
	FString LastExternalQuestLoadError;
};
