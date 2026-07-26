#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/Data/HeroUIInfo.h"
#include "PartyComponent.generated.h"

class UHeroUIInfo;

UENUM(BlueprintType)
enum class EPartySetupApplyResult : uint8
{
	Success,
	InvalidParty,
	InvalidActiveSlot,
	UnknownHero,
	MissingCharacterClass,
	SpawnFailed,
	PossessFailed,
	AlreadyProcessing
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPartyStateChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FHeroUnlockedSignature, FGameplayTag, HeroTag);

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEMO_API UPartyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPartyComponent();

	UFUNCTION(BlueprintCallable, Category = "Party")
	bool UnlockHero(FGameplayTag HeroTag);

	UFUNCTION(BlueprintCallable, Category = "Party")
	bool AddHeroToParty(FGameplayTag HeroTag);

	UFUNCTION(BlueprintCallable, Category = "Party")
	bool RemoveHeroFromParty(FGameplayTag HeroTag);

	UFUNCTION(BlueprintPure, Category = "Party")
	bool IsHeroUnlocked(FGameplayTag HeroTag) const;

	UFUNCTION(BlueprintPure, Category = "Party")
	bool IsHeroInParty(FGameplayTag HeroTag) const;

	UFUNCTION(BlueprintPure, Category = "Party")
	TArray<FGameplayTag> GetUnlockedHeroTags() const { return UnlockedHeroTags; }

	UFUNCTION(BlueprintPure, Category = "Party")
	TArray<FGameplayTag> GetActivePartyTags() const;

	UFUNCTION(BlueprintPure, Category = "Party")
	TArray<FHeroSlotInfo> GetUnlockedHeroInfo() const;

	UFUNCTION(BlueprintPure, Category = "Party")
	TArray<FHeroSlotInfo> GetActivePartyInfo() const;

	// Always returns MaxPartySize entries, including empty slots.
	UFUNCTION(BlueprintPure, Category = "Party|Setup")
	TArray<FPartySlotViewData> GetPartySlots() const;

	UFUNCTION(BlueprintPure, Category = "Party|Setup")
	int32 GetActiveSlotIndex() const { return ActiveSlotIndex; }

	UFUNCTION(BlueprintPure, Category = "Party|Setup")
	FGameplayTag GetActiveHeroTag() const;

	EPartySetupApplyResult ValidatePartySetup(
		const TArray<FGameplayTag>& NewParty,
		int32 NewActiveSlotIndex) const;

	bool ApplyPartySetup(
		const TArray<FGameplayTag>& NewParty,
		int32 NewActiveSlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Party|Setup")
	bool SetPartySlot(int32 SlotIndex, FGameplayTag HeroTag);

	UFUNCTION(BlueprintCallable, Category = "Party|Setup")
	bool ClearPartySlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Party|Setup")
	bool SwapPartySlots(int32 FirstSlotIndex, int32 SecondSlotIndex);

	UFUNCTION(BlueprintPure, Category = "Party")
	bool FindHeroInfo(FGameplayTag HeroTag, FHeroSlotInfo& OutHeroInfo) const;

	UPROPERTY(BlueprintAssignable, Category = "Party|Events")
	FPartyStateChangedSignature OnRosterChanged;

	UPROPERTY(BlueprintAssignable, Category = "Party|Events")
	FPartyStateChangedSignature OnPartyChanged;

	UPROPERTY(BlueprintAssignable, Category = "Party|Events")
	FHeroUnlockedSignature OnHeroUnlocked;

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party|Data")
	TObjectPtr<UHeroUIInfo> HeroCatalog;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party|Rules",
		meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxPartySize = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party|Rules")
	FGameplayTag InitialHeroTag;

private:
	bool UnlockHeroInternal(FGameplayTag HeroTag);
	bool AddHeroToPartyInternal(FGameplayTag HeroTag);
	bool RemoveHeroFromPartyInternal(FGameplayTag HeroTag);
	bool SetPartySlotInternal(int32 SlotIndex, FGameplayTag HeroTag);
	bool ClearPartySlotInternal(int32 SlotIndex);
	bool SwapPartySlotsInternal(int32 FirstSlotIndex, int32 SecondSlotIndex);
	void NormalizePartySlots();
	void RestoreActiveSlotForHero(
		FGameplayTag PreviousActiveHero, int32 PreferredFallbackSlot);
	void NotifyPartyChanged();
	int32 GetOccupiedPartyCount() const;
	int32 FindFirstEmptyPartySlot() const;
	void InitializeStartingParty();
	TArray<FHeroSlotInfo> ResolveHeroInfo(
		const TArray<FGameplayTag>& HeroTags) const;

	UFUNCTION(Server, Reliable)
	void ServerUnlockHero(FGameplayTag HeroTag);

	UFUNCTION(Server, Reliable)
	void ServerAddHeroToParty(FGameplayTag HeroTag);

	UFUNCTION(Server, Reliable)
	void ServerRemoveHeroFromParty(FGameplayTag HeroTag);

	UFUNCTION(Server, Reliable)
	void ServerSetPartySlot(int32 SlotIndex, FGameplayTag HeroTag);

	UFUNCTION(Server, Reliable)
	void ServerClearPartySlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerSwapPartySlots(int32 FirstSlotIndex, int32 SecondSlotIndex);

	UFUNCTION()
	void OnRep_UnlockedHeroTags(
		const TArray<FGameplayTag>& PreviousHeroTags);

	UFUNCTION()
	void OnRep_PartyStateRevision();

	UPROPERTY(ReplicatedUsing = OnRep_UnlockedHeroTags, VisibleAnywhere,
		BlueprintReadOnly, Category = "Party|State",
		meta = (AllowPrivateAccess = "true"))
	TArray<FGameplayTag> UnlockedHeroTags;

	UPROPERTY(Replicated, VisibleAnywhere,
		BlueprintReadOnly, Category = "Party|State",
		meta = (AllowPrivateAccess = "true"))
	TArray<FGameplayTag> ActivePartyTags;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly,
		Category = "Party|State", meta = (AllowPrivateAccess = "true"))
	int32 ActiveSlotIndex = 0;

	// The slots and active index replicate as one logical update. Only this
	// revision invokes the client-side party notification.
	UPROPERTY(ReplicatedUsing = OnRep_PartyStateRevision)
	int32 PartyStateRevision = 0;
};
