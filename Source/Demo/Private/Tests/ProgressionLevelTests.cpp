#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystem/Abilities/PlayerDamageGameplayAbility.h"
#include "AbilitySystem/Abilities/PlayerGameplayAbility.h"
#include "AbilitySystem/PlayerAbilitySystemComponent.h"
#include "Components/ExpComponent.h"
#include "Curves/RichCurve.h"
#include "Engine/CurveTable.h"
#include "Engine/World.h"
#include "Player/OPlayerState.h"
#include "UI/WidgetController/PlayerWidgetController.h"
#include "UObject/UnrealType.h"

namespace
{
	class FScopedProgressionTestWorld
	{
	public:
		FScopedProgressionTestWorld()
		{
			FWorldInitializationValues InitializationValues;
			InitializationValues
				.AllowAudioPlayback(false)
				.RequiresHitProxies(false)
				.CreatePhysicsScene(false)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.ShouldSimulatePhysics(false)
				.SetTransactional(false)
				.CreateFXSystem(false);

			World = UWorld::CreateWorld(
				EWorldType::Game,
				false,
				MakeUniqueObjectName(
					GetTransientPackage(), UWorld::StaticClass(),
					TEXT("ProgressionLevelTestWorld")),
				GetTransientPackage(),
				true,
				ERHIFeatureLevel::Num,
				&InitializationValues);
		}

		~FScopedProgressionTestWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
		}

		UWorld* Get() const { return World; }

	private:
		TObjectPtr<UWorld> World = nullptr;
	};

	UCurveTable* MakeExperienceCurve()
	{
		UCurveTable* CurveTable = NewObject<UCurveTable>();
		FRichCurve& Curve = CurveTable->AddRichCurve(TEXT("XP"));
		Curve.AddKey(1.f, 100.f);
		Curve.AddKey(2.f, 200.f);
		Curve.AddKey(3.f, 300.f);
		Curve.AddKey(4.f, 400.f);
		return CurveTable;
	}

	AOPlayerState* SpawnProgressionPlayer(UWorld* World, UCurveTable* CurveTable)
	{
		AOPlayerState* PlayerState = World
			? World->SpawnActor<AOPlayerState>()
			: nullptr;
		if (PlayerState && PlayerState->ExpComponent)
		{
			PlayerState->ExpComponent->ExperienceCurveTable = CurveTable;
		}
		return PlayerState;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProgressionLevelAuthorityTest,
	"Demo.Progression.LevelAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProgressionLevelAuthorityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FScopedProgressionTestWorld TestWorld;
	UWorld* World = TestWorld.Get();
	TestNotNull(TEXT("progression test world exists"), World);
	if (!World)
	{
		return false;
	}

	UCurveTable* ExperienceCurve = MakeExperienceCurve();
	TestNotNull(TEXT("experience curve exists"), ExperienceCurve);

	const FProperty* LevelProperty = FindFProperty<FProperty>(
		AOPlayerState::StaticClass(), TEXT("Level"));
	TestNotNull(TEXT("PlayerState Level property exists"), LevelProperty);
	if (LevelProperty)
	{
		TestTrue(
			TEXT("PlayerState Level is replicated"),
			LevelProperty->HasAnyPropertyFlags(CPF_Net));
		TestEqual(
			TEXT("PlayerState Level uses OnRep_Level"),
			LevelProperty->RepNotifyFunc,
			FName(TEXT("OnRep_Level")));
	}

	const UFunction* TryLevelUpFunction =
		UExpComponent::StaticClass()->FindFunctionByName(TEXT("TryLevelUp"));
	TestNotNull(TEXT("compatibility level-up function exists"), TryLevelUpFunction);
	if (TryLevelUpFunction)
	{
		TestTrue(
			TEXT("compatibility level-up function is authority-only"),
			TryLevelUpFunction->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));
		TestFalse(
			TEXT("level-up function is not a network RPC"),
			TryLevelUpFunction->HasAnyFunctionFlags(FUNC_Net));
	}
	TestNull(
		TEXT("authoritative PlayerState setter is not exposed to Blueprint"),
		AOPlayerState::StaticClass()->FindFunctionByName(TEXT("SetPlayerLevel")));

	{
		AOPlayerState* PlayerState = SpawnProgressionPlayer(World, ExperienceCurve);
		TestNotNull(TEXT("under-threshold PlayerState exists"), PlayerState);
		if (PlayerState)
		{
			UExpComponent* ExpComponent = PlayerState->ExpComponent;
			ExpComponent->AddExperience(99);
			TestEqual(TEXT("under threshold keeps level 1"), PlayerState->GetPlayerLevel(), 1);
			TestEqual(TEXT("under threshold keeps XP"), ExpComponent->CurrentXP, 99);
		}
	}

	{
		AOPlayerState* PlayerState = SpawnProgressionPlayer(World, ExperienceCurve);
		TestNotNull(TEXT("exact-threshold PlayerState exists"), PlayerState);
		if (PlayerState)
		{
			UExpComponent* ExpComponent = PlayerState->ExpComponent;
			UPlayerAbilitySystemComponent* PlayerASC =
				Cast<UPlayerAbilitySystemComponent>(
					PlayerState->GetAbilitySystemComponent());
			TestNotNull(TEXT("player ASC exists"), PlayerASC);
			FGameplayAbilitySpec* AbilitySpec = nullptr;
			FGameplayAbilitySpec* CommonAbilitySpec = nullptr;
			if (PlayerASC)
			{
				PlayerASC->InitAbilityActorInfo(PlayerState, PlayerState);
				PlayerASC->GiveAbility(FGameplayAbilitySpec(
					UPlayerGameplayAbility::StaticClass(), 1));
				CommonAbilitySpec = PlayerASC->FindAbilitySpecFromClass(
					UPlayerGameplayAbility::StaticClass());
				TestNotNull(TEXT("common ability is granted"), CommonAbilitySpec);
				PlayerASC->SetCharacterAbilities(
					{UPlayerDamageGameplayAbility::StaticClass()});
				AbilitySpec = PlayerASC->FindAbilitySpecFromClass(
					UPlayerDamageGameplayAbility::StaticClass());
				TestNotNull(TEXT("test ability is granted"), AbilitySpec);
				if (AbilitySpec)
				{
					TestEqual(TEXT("granted ability starts at PlayerState level"), AbilitySpec->Level, 1);
				}
			}

			ExpComponent->AddExperience(100);
			TestEqual(TEXT("exact threshold reaches level 2"), PlayerState->GetPlayerLevel(), 2);
			TestEqual(TEXT("exact threshold consumes all XP"), ExpComponent->CurrentXP, 0);
			if (AbilitySpec)
			{
				TestEqual(TEXT("ability spec follows PlayerState level"), AbilitySpec->Level, 2);
			}
			if (CommonAbilitySpec)
			{
				TestEqual(TEXT("common ability level is not changed"), CommonAbilitySpec->Level, 1);
			}
		}
	}

	{
		AOPlayerState* PlayerState = SpawnProgressionPlayer(World, ExperienceCurve);
		TestNotNull(TEXT("multi-level PlayerState exists"), PlayerState);
		if (PlayerState)
		{
			UExpComponent* ExpComponent = PlayerState->ExpComponent;
			ExpComponent->AddExperience(350);
			TestEqual(TEXT("one request reaches level 3"), PlayerState->GetPlayerLevel(), 3);
			TestEqual(TEXT("multi-level request retains remainder"), ExpComponent->CurrentXP, 50);
		}
	}

	{
		AOPlayerState* PlayerState = SpawnProgressionPlayer(World, ExperienceCurve);
		TestNotNull(TEXT("maximum-level PlayerState exists"), PlayerState);
		if (PlayerState)
		{
			UExpComponent* ExpComponent = PlayerState->ExpComponent;
			ExpComponent->AddExperience(1600);
			TestEqual(TEXT("large XP stops at curve maximum"), PlayerState->GetPlayerLevel(), 4);
			TestEqual(TEXT("maximum level retains unspent XP"), ExpComponent->CurrentXP, 1000);
			TestFalse(TEXT("maximum level cannot level again"), ExpComponent->CanLevelUp());
			TestEqual(TEXT("maximum level has no next threshold"), ExpComponent->GetXPToNextLevel(), 0);

			ExpComponent->TryLevelUp();
			TestEqual(TEXT("repeat request stays at maximum"), PlayerState->GetPlayerLevel(), 4);
			TestEqual(TEXT("repeat request does not consume XP"), ExpComponent->CurrentXP, 1000);
		}
	}

	{
		AOPlayerState* PlayerState = SpawnProgressionPlayer(World, ExperienceCurve);
		TestNotNull(TEXT("UI-broadcast PlayerState exists"), PlayerState);
		if (PlayerState)
		{
			UPlayerWidgetController* WidgetController =
				NewObject<UPlayerWidgetController>();
			FWidgetControllerParams Params(
				nullptr, PlayerState, nullptr, nullptr);
			WidgetController->SetWidgetControllerParams(Params);
			WidgetController->BindCallbacksToDependencies();

			int32 BroadcastLevel = INDEX_NONE;
			WidgetController->OnPlayerLevelChangedNative.AddLambda(
				[&BroadcastLevel](int32 NewLevel)
				{
					BroadcastLevel = NewLevel;
				});

			PlayerState->ExpComponent->AddExperience(100);
			TestEqual(TEXT("UI receives the authoritative level"), BroadcastLevel, 2);
			TestEqual(
				TEXT("UI level equals PlayerState level"),
				BroadcastLevel,
				PlayerState->GetPlayerLevel());

			PlayerState->ExpComponent->CurrentLevel = 99;
			TestEqual(
				TEXT("legacy component getter still reads PlayerState"),
				PlayerState->ExpComponent->GetCurrentLevel(),
				PlayerState->GetPlayerLevel());
		}
	}

	{
		AOPlayerState* ClientPlayerState =
			SpawnProgressionPlayer(World, ExperienceCurve);
		TestNotNull(TEXT("client-authority PlayerState exists"), ClientPlayerState);
		if (ClientPlayerState)
		{
			ClientPlayerState->SetRole(ROLE_AutonomousProxy);
			ClientPlayerState->ExpComponent->CurrentXP = 100;
			ClientPlayerState->ExpComponent->TryLevelUp();
			TestFalse(
				TEXT("client role cannot set PlayerState level"),
				ClientPlayerState->SetPlayerLevel(2));
			TestEqual(
				TEXT("client write attempt leaves level unchanged"),
				ClientPlayerState->GetPlayerLevel(),
				1);
			TestEqual(
				TEXT("client level-up call cannot consume authoritative XP"),
				ClientPlayerState->ExpComponent->CurrentXP,
				100);
		}
	}

	return true;
}

#endif
