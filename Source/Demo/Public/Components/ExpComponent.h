#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ExpComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnExperienceChangedSignature, int32, CurrentXP, int32, XPToNext);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLeveledUpSignature, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnXPThresholdReachedSignature);

class UCurveTable;

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEMO_API UExpComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UExpComponent();
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience")
	TObjectPtr<UCurveTable> ExperienceCurveTable;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_CurrentXP,
		Category = "Experience")
	int32 CurrentXP = 0;

	/**
	 * Compatibility property only. Blueprint reads and writes are redirected to
	 * AOPlayerState; no progression logic uses this storage as a level source.
	 */
	UPROPERTY(
		Transient, BlueprintReadWrite, Category = "Experience",
		BlueprintGetter = GetCurrentLevel,
		BlueprintSetter = SetCurrentLevel,
		meta = (DeprecatedProperty,
			DeprecationMessage = "CurrentLevel is no longer a level source. Read AOPlayerState.GetPlayerLevel and request upgrades with TryLevelUp."))
	int32 CurrentLevel = 1;

	UPROPERTY(BlueprintAssignable)
	FOnExperienceChangedSignature OnExperienceChanged;

	UPROPERTY(BlueprintAssignable)
	FOnLeveledUpSignature OnLeveledUp;

	UPROPERTY(BlueprintAssignable)
	FOnXPThresholdReachedSignature OnXPThresholdReached;

	// Adds trusted reward XP on the server. Does not auto-level.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Experience")
	void AddExperience(int32 Amount);

	// Returns true if player has enough XP to level up
	UFUNCTION(BlueprintPure)
	bool CanLevelUp() const;

	// Compatibility entry point for server-side callers. AddExperience auto-levels.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Experience")
	void TryLevelUp();

	UFUNCTION(BlueprintPure, Category = "Experience")
	int32 GetMaximumLevel() const;

	UFUNCTION(
		BlueprintPure, BlueprintGetter, Category = "Experience",
		meta = (DeprecatedFunction,
			DeprecationMessage = "Read AOPlayerState.GetPlayerLevel instead."))
	int32 GetCurrentLevel() const;

	UFUNCTION(
		BlueprintCallable, BlueprintSetter, Category = "Experience",
		meta = (DeprecatedFunction,
			DeprecationMessage = "Do not set CurrentLevel. Call TryLevelUp; the server updates AOPlayerState.Level."))
	void SetCurrentLevel(int32 NewLevel);

	UFUNCTION(BlueprintPure)
	int32 GetXPToNextLevel() const;

	UFUNCTION(BlueprintPure)
	float GetLevelProgress() const;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_CurrentXP();

	bool TryLevelUpOnAuthority();
	void HandleAuthoritativeLevelChanged(int32 NewLevel);
	void BroadcastExperienceChanged();
	int32 GetXPRequirementForLevel(int32 PlayerLevel) const;
};
