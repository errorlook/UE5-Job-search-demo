#include "Components/ExpComponent.h"

#include "Engine/CurveTable.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Player/OPlayerState.h"

namespace
{
	const FName ExperienceCurveRowName(TEXT("XP"));
	constexpr int32 MissingCurveXPRequirement = 999999;
}

UExpComponent::UExpComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UExpComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UExpComponent, CurrentXP);
}

void UExpComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AOPlayerState* PlayerState = Cast<AOPlayerState>(GetOwner()))
	{
		CurrentLevel = PlayerState->GetPlayerLevel();
		PlayerState->OnLevelChangedNative.RemoveAll(this);
		PlayerState->OnLevelChangedNative.AddUObject(
			this, &UExpComponent::HandleAuthoritativeLevelChanged);
	}
}

void UExpComponent::AddExperience(int32 Amount)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || Amount <= 0)
	{
		return;
	}

	const int64 NewXP = static_cast<int64>(CurrentXP) + Amount;
	CurrentXP = static_cast<int32>(FMath::Min<int64>(NewXP, MAX_int32));

	TryLevelUpOnAuthority();
	BroadcastExperienceChanged();
	Owner->ForceNetUpdate();
}

bool UExpComponent::CanLevelUp() const
{
	const int32 RequiredXP = GetXPToNextLevel();
	return RequiredXP > 0 && CurrentXP >= RequiredXP;
}

void UExpComponent::TryLevelUp()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	if (TryLevelUpOnAuthority())
	{
		BroadcastExperienceChanged();
		Owner->ForceNetUpdate();
	}
}

bool UExpComponent::TryLevelUpOnAuthority()
{
	AOPlayerState* PlayerState = Cast<AOPlayerState>(GetOwner());
	if (!IsValid(PlayerState) || !PlayerState->HasAuthority())
	{
		return false;
	}

	const int32 InitialLevel = PlayerState->GetPlayerLevel();
	const int32 MaximumLevel = GetMaximumLevel();
	int32 NewLevel = InitialLevel;
	int32 RemainingXP = CurrentXP;

	while (NewLevel < MaximumLevel)
	{
		const int32 RequiredXP = GetXPRequirementForLevel(NewLevel);
		if (RequiredXP <= 0 || RemainingXP < RequiredXP)
		{
			break;
		}

		RemainingXP -= RequiredXP;
		++NewLevel;
	}

	if (NewLevel == InitialLevel)
	{
		return false;
	}

	const int32 PreviousXP = CurrentXP;
	CurrentXP = RemainingXP;
	if (!PlayerState->SetPlayerLevel(NewLevel))
	{
		CurrentXP = PreviousXP;
		return false;
	}

	return true;
}

int32 UExpComponent::GetXPToNextLevel() const
{
	const int32 PlayerLevel = GetCurrentLevel();
	if (PlayerLevel >= GetMaximumLevel())
	{
		return 0;
	}

	return GetXPRequirementForLevel(PlayerLevel);
}

int32 UExpComponent::GetMaximumLevel() const
{
	if (!ExperienceCurveTable)
	{
		return MAX_int32;
	}

	const FRealCurve* Curve = ExperienceCurveTable->FindCurve(
		ExperienceCurveRowName, FString(), false);
	if (!Curve || Curve->GetNumKeys() == 0)
	{
		return MAX_int32;
	}

	float MinimumTime = 0.f;
	float MaximumTime = 0.f;
	Curve->GetTimeRange(MinimumTime, MaximumTime);
	return FMath::Max(1, FMath::FloorToInt(MaximumTime + KINDA_SMALL_NUMBER));
}

int32 UExpComponent::GetCurrentLevel() const
{
	if (const AOPlayerState* PlayerState = Cast<AOPlayerState>(GetOwner()))
	{
		return PlayerState->GetPlayerLevel();
	}

	return 1;
}

void UExpComponent::SetCurrentLevel(int32 NewLevel)
{
	if (AOPlayerState* PlayerState = Cast<AOPlayerState>(GetOwner()))
	{
		PlayerState->SetPlayerLevel(NewLevel);
	}
}

int32 UExpComponent::GetXPRequirementForLevel(int32 PlayerLevel) const
{
	if (!ExperienceCurveTable)
	{
		return MissingCurveXPRequirement;
	}

	const FRealCurve* Curve = ExperienceCurveTable->FindCurve(
		ExperienceCurveRowName, FString(), false);
	if (Curve)
	{
		return FMath::RoundToInt(Curve->Eval(static_cast<float>(PlayerLevel)));
	}

	const int64 FallbackRequirement =
		static_cast<int64>(PlayerLevel) * 100
		+ static_cast<int64>(PlayerLevel) * PlayerLevel * 25;
	return static_cast<int32>(FMath::Min<int64>(FallbackRequirement, MAX_int32));
}

float UExpComponent::GetLevelProgress() const
{
	const int32 RequiredXP = GetXPToNextLevel();
	return RequiredXP > 0
		? static_cast<float>(CurrentXP) / static_cast<float>(RequiredXP)
		: 1.0f;
}

void UExpComponent::OnRep_CurrentXP()
{
	BroadcastExperienceChanged();
}

void UExpComponent::HandleAuthoritativeLevelChanged(int32 NewLevel)
{
	CurrentLevel = NewLevel;
	OnLeveledUp.Broadcast(NewLevel);
	if (const AActor* Owner = GetOwner(); Owner && !Owner->HasAuthority())
	{
		// Level and XP replicate independently. Re-broadcasting on clients makes
		// the final threshold correct regardless of RepNotify arrival order.
		BroadcastExperienceChanged();
	}
}

void UExpComponent::BroadcastExperienceChanged()
{
	OnExperienceChanged.Broadcast(CurrentXP, GetXPToNextLevel());
}
