#include "Components/PartyComponent.h"

#include "Character/PlayerCharacter.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UPartyComponent::UPartyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UPartyComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeStartingParty();
	NormalizePartySlots();
	RestoreActiveSlotForHero(GetActiveHeroTag(), 0);
}

void UPartyComponent::InitializeStartingParty()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !UnlockedHeroTags.IsEmpty())
	{
		return;
	}

	if (!InitialHeroTag.IsValid())
	{
		InitialHeroTag = FGameplayTag::RequestGameplayTag(
			FName(TEXT("Hero.Shinbi")), false);
	}

	if (InitialHeroTag.IsValid())
	{
		UnlockedHeroTags.Add(InitialHeroTag);
		ActivePartyTags.SetNum(MaxPartySize);
		ActivePartyTags[0] = InitialHeroTag;
		ActiveSlotIndex = 0;
		OnRosterChanged.Broadcast();
		NotifyPartyChanged();
	}
}

bool UPartyComponent::UnlockHero(FGameplayTag HeroTag)
{
	if (!HeroTag.IsValid() || IsHeroUnlocked(HeroTag)) return false;

	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerUnlockHero(HeroTag);
		return true;
	}

	return UnlockHeroInternal(HeroTag);
}

bool UPartyComponent::UnlockHeroInternal(FGameplayTag HeroTag)
{
	if (!HeroTag.IsValid() || IsHeroUnlocked(HeroTag)) return false;

	UnlockedHeroTags.Add(HeroTag);
	OnHeroUnlocked.Broadcast(HeroTag);
	OnRosterChanged.Broadcast();
	if (AActor* Owner = GetOwner()) Owner->ForceNetUpdate();
	return true;
}

bool UPartyComponent::AddHeroToParty(FGameplayTag HeroTag)
{
	if (!IsHeroUnlocked(HeroTag) || IsHeroInParty(HeroTag) ||
		GetOccupiedPartyCount() >= MaxPartySize)
	{
		return false;
	}

	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerAddHeroToParty(HeroTag);
		return true;
	}

	return AddHeroToPartyInternal(HeroTag);
}

bool UPartyComponent::AddHeroToPartyInternal(FGameplayTag HeroTag)
{
	if (!IsHeroUnlocked(HeroTag) || IsHeroInParty(HeroTag) ||
		GetOccupiedPartyCount() >= MaxPartySize)
	{
		return false;
	}

	NormalizePartySlots();
	const int32 EmptySlotIndex = FindFirstEmptyPartySlot();
	if (!ActivePartyTags.IsValidIndex(EmptySlotIndex)) return false;
	ActivePartyTags[EmptySlotIndex] = HeroTag;
	NotifyPartyChanged();
	return true;
}

bool UPartyComponent::RemoveHeroFromParty(FGameplayTag HeroTag)
{
	if (!IsHeroInParty(HeroTag) || GetOccupiedPartyCount() <= 1) return false;

	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerRemoveHeroFromParty(HeroTag);
		return true;
	}

	return RemoveHeroFromPartyInternal(HeroTag);
}

bool UPartyComponent::RemoveHeroFromPartyInternal(FGameplayTag HeroTag)
{
	if (!IsHeroInParty(HeroTag) || GetOccupiedPartyCount() <= 1) return false;

	NormalizePartySlots();
	const FGameplayTag PreviousActiveHero = GetActiveHeroTag();
	const int32 SlotIndex = ActivePartyTags.IndexOfByKey(HeroTag);
	if (!ActivePartyTags.IsValidIndex(SlotIndex)) return false;
	ActivePartyTags[SlotIndex] = FGameplayTag();
	RestoreActiveSlotForHero(PreviousActiveHero, 0);
	NotifyPartyChanged();
	return true;
}

bool UPartyComponent::IsHeroUnlocked(FGameplayTag HeroTag) const
{
	return HeroTag.IsValid() && UnlockedHeroTags.Contains(HeroTag);
}

bool UPartyComponent::IsHeroInParty(FGameplayTag HeroTag) const
{
	return HeroTag.IsValid() && ActivePartyTags.Contains(HeroTag);
}

TArray<FGameplayTag> UPartyComponent::GetActivePartyTags() const
{
	TArray<FGameplayTag> OccupiedTags;
	OccupiedTags.Reserve(GetOccupiedPartyCount());
	for (const FGameplayTag HeroTag : ActivePartyTags)
	{
		if (HeroTag.IsValid()) OccupiedTags.Add(HeroTag);
	}
	return OccupiedTags;
}

TArray<FHeroSlotInfo> UPartyComponent::GetUnlockedHeroInfo() const
{
	return ResolveHeroInfo(UnlockedHeroTags);
}

TArray<FHeroSlotInfo> UPartyComponent::GetActivePartyInfo() const
{
	return ResolveHeroInfo(ActivePartyTags);
}

TArray<FPartySlotViewData> UPartyComponent::GetPartySlots() const
{
	TArray<FPartySlotViewData> Slots;
	Slots.Reserve(MaxPartySize);
	for (int32 SlotIndex = 0; SlotIndex < MaxPartySize; ++SlotIndex)
	{
		FPartySlotViewData& Slot = Slots.AddDefaulted_GetRef();
		Slot.SlotIndex = SlotIndex;
		if (ActivePartyTags.IsValidIndex(SlotIndex))
		{
			Slot.bOccupied = FindHeroInfo(
				ActivePartyTags[SlotIndex], Slot.HeroInfo);
		}
	}
	return Slots;
}

FGameplayTag UPartyComponent::GetActiveHeroTag() const
{
	return ActivePartyTags.IsValidIndex(ActiveSlotIndex)
		? ActivePartyTags[ActiveSlotIndex]
		: FGameplayTag();
}

EPartySetupApplyResult UPartyComponent::ValidatePartySetup(
	const TArray<FGameplayTag>& NewParty, int32 NewActiveSlotIndex) const
{
	if (NewParty.Num() != MaxPartySize)
	{
		return EPartySetupApplyResult::InvalidParty;
	}
	if (!NewParty.IsValidIndex(NewActiveSlotIndex) ||
		!NewParty[NewActiveSlotIndex].IsValid())
	{
		return EPartySetupApplyResult::InvalidActiveSlot;
	}

	TSet<FGameplayTag> UniqueHeroes;
	for (const FGameplayTag HeroTag : NewParty)
	{
		if (!HeroTag.IsValid()) continue;
		if (UniqueHeroes.Contains(HeroTag))
		{
			return EPartySetupApplyResult::InvalidParty;
		}

		FHeroSlotInfo HeroInfo;
		if (!FindHeroInfo(HeroTag, HeroInfo))
		{
			return EPartySetupApplyResult::UnknownHero;
		}
		if (!IsHeroUnlocked(HeroTag))
		{
			return EPartySetupApplyResult::InvalidParty;
		}
		if (HeroInfo.CharacterClass.IsNull() ||
			!HeroInfo.CharacterClass.LoadSynchronous())
		{
			return EPartySetupApplyResult::MissingCharacterClass;
		}
		UniqueHeroes.Add(HeroTag);
	}

	return EPartySetupApplyResult::Success;
}

bool UPartyComponent::ApplyPartySetup(
	const TArray<FGameplayTag>& NewParty, int32 NewActiveSlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() ||
		ValidatePartySetup(NewParty, NewActiveSlotIndex) !=
			EPartySetupApplyResult::Success)
	{
		return false;
	}

	if (ActivePartyTags == NewParty && ActiveSlotIndex == NewActiveSlotIndex)
	{
		return true;
	}

	ActivePartyTags = NewParty;
	ActiveSlotIndex = NewActiveSlotIndex;
	NotifyPartyChanged();
	return true;
}

bool UPartyComponent::SetPartySlot(int32 SlotIndex, FGameplayTag HeroTag)
{
	if (SlotIndex < 0 || SlotIndex >= MaxPartySize ||
		!IsHeroUnlocked(HeroTag))
	{
		return false;
	}

	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerSetPartySlot(SlotIndex, HeroTag);
		return true;
	}
	return SetPartySlotInternal(SlotIndex, HeroTag);
}

bool UPartyComponent::SetPartySlotInternal(
	int32 SlotIndex, FGameplayTag HeroTag)
{
	if (SlotIndex < 0 || SlotIndex >= MaxPartySize ||
		!IsHeroUnlocked(HeroTag))
	{
		return false;
	}

	NormalizePartySlots();
	const FGameplayTag PreviousActiveHero = GetActiveHeroTag();
	const int32 ExistingIndex = ActivePartyTags.IndexOfByKey(HeroTag);
	if (ExistingIndex == SlotIndex) return false;

	if (ExistingIndex != INDEX_NONE)
	{
		ActivePartyTags.Swap(ExistingIndex, SlotIndex);
	}
	else
	{
		ActivePartyTags[SlotIndex] = HeroTag;
	}

	RestoreActiveSlotForHero(PreviousActiveHero, SlotIndex);
	NotifyPartyChanged();
	return true;
}

bool UPartyComponent::ClearPartySlot(int32 SlotIndex)
{
	if (!ActivePartyTags.IsValidIndex(SlotIndex) ||
		!ActivePartyTags[SlotIndex].IsValid() ||
		GetOccupiedPartyCount() <= 1)
	{
		return false;
	}
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerClearPartySlot(SlotIndex);
		return true;
	}
	return ClearPartySlotInternal(SlotIndex);
}

bool UPartyComponent::ClearPartySlotInternal(int32 SlotIndex)
{
	NormalizePartySlots();
	const FGameplayTag PreviousActiveHero = GetActiveHeroTag();
	if (!ActivePartyTags.IsValidIndex(SlotIndex) ||
		!ActivePartyTags[SlotIndex].IsValid() ||
		GetOccupiedPartyCount() <= 1)
	{
		return false;
	}
	ActivePartyTags[SlotIndex] = FGameplayTag();
	RestoreActiveSlotForHero(PreviousActiveHero, 0);
	NotifyPartyChanged();
	return true;
}

bool UPartyComponent::SwapPartySlots(
	int32 FirstSlotIndex, int32 SecondSlotIndex)
{
	if (FirstSlotIndex < 0 || FirstSlotIndex >= MaxPartySize ||
		SecondSlotIndex < 0 || SecondSlotIndex >= MaxPartySize ||
		FirstSlotIndex == SecondSlotIndex)
	{
		return false;
	}
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerSwapPartySlots(FirstSlotIndex, SecondSlotIndex);
		return true;
	}
	return SwapPartySlotsInternal(FirstSlotIndex, SecondSlotIndex);
}

bool UPartyComponent::SwapPartySlotsInternal(
	int32 FirstSlotIndex, int32 SecondSlotIndex)
{
	NormalizePartySlots();
	const FGameplayTag PreviousActiveHero = GetActiveHeroTag();
	if (!ActivePartyTags.IsValidIndex(FirstSlotIndex) ||
		!ActivePartyTags.IsValidIndex(SecondSlotIndex) ||
		FirstSlotIndex == SecondSlotIndex)
	{
		return false;
	}
	ActivePartyTags.Swap(FirstSlotIndex, SecondSlotIndex);
	RestoreActiveSlotForHero(PreviousActiveHero, FirstSlotIndex);
	NotifyPartyChanged();
	return true;
}

void UPartyComponent::NormalizePartySlots()
{
	if (MaxPartySize < 1) MaxPartySize = 1;
	if (ActivePartyTags.Num() > MaxPartySize)
	{
		ActivePartyTags.SetNum(MaxPartySize);
	}
	else if (ActivePartyTags.Num() < MaxPartySize)
	{
		ActivePartyTags.SetNum(MaxPartySize);
	}
}

void UPartyComponent::RestoreActiveSlotForHero(
	FGameplayTag PreviousActiveHero, int32 PreferredFallbackSlot)
{
	if (PreviousActiveHero.IsValid())
	{
		const int32 MovedActiveSlot =
			ActivePartyTags.IndexOfByKey(PreviousActiveHero);
		if (MovedActiveSlot != INDEX_NONE)
		{
			ActiveSlotIndex = MovedActiveSlot;
			return;
		}
	}

	if (ActivePartyTags.IsValidIndex(PreferredFallbackSlot) &&
		ActivePartyTags[PreferredFallbackSlot].IsValid())
	{
		ActiveSlotIndex = PreferredFallbackSlot;
		return;
	}

	ActiveSlotIndex = ActivePartyTags.IndexOfByPredicate(
		[](const FGameplayTag HeroTag) { return HeroTag.IsValid(); });
}

void UPartyComponent::NotifyPartyChanged()
{
	++PartyStateRevision;
	OnPartyChanged.Broadcast();
	if (AActor* Owner = GetOwner()) Owner->ForceNetUpdate();
}

int32 UPartyComponent::GetOccupiedPartyCount() const
{
	int32 OccupiedCount = 0;
	for (const FGameplayTag HeroTag : ActivePartyTags)
	{
		if (HeroTag.IsValid()) ++OccupiedCount;
	}
	return OccupiedCount;
}

int32 UPartyComponent::FindFirstEmptyPartySlot() const
{
	return ActivePartyTags.IndexOfByPredicate(
		[](const FGameplayTag HeroTag) { return !HeroTag.IsValid(); });
}

bool UPartyComponent::FindHeroInfo(
	FGameplayTag HeroTag, FHeroSlotInfo& OutHeroInfo) const
{
	if (!HeroCatalog || !HeroTag.IsValid()) return false;

	const FHeroSlotInfo* FoundInfo = HeroCatalog->PartyMembers.FindByPredicate(
		[HeroTag](const FHeroSlotInfo& HeroInfo)
		{
			return HeroInfo.HeroTag == HeroTag;
		});
	if (!FoundInfo) return false;

	OutHeroInfo = *FoundInfo;
	return true;
}

TArray<FHeroSlotInfo> UPartyComponent::ResolveHeroInfo(
	const TArray<FGameplayTag>& HeroTags) const
{
	TArray<FHeroSlotInfo> Result;
	Result.Reserve(HeroTags.Num());
	for (const FGameplayTag HeroTag : HeroTags)
	{
		FHeroSlotInfo HeroInfo;
		if (FindHeroInfo(HeroTag, HeroInfo))
		{
			Result.Add(HeroInfo);
		}
	}
	return Result;
}

void UPartyComponent::ServerUnlockHero_Implementation(FGameplayTag HeroTag)
{
	UnlockHeroInternal(HeroTag);
}

void UPartyComponent::ServerAddHeroToParty_Implementation(FGameplayTag HeroTag)
{
	AddHeroToPartyInternal(HeroTag);
}

void UPartyComponent::ServerRemoveHeroFromParty_Implementation(FGameplayTag HeroTag)
{
	RemoveHeroFromPartyInternal(HeroTag);
}

void UPartyComponent::ServerSetPartySlot_Implementation(
	int32 SlotIndex, FGameplayTag HeroTag)
{
	SetPartySlotInternal(SlotIndex, HeroTag);
}

void UPartyComponent::ServerClearPartySlot_Implementation(int32 SlotIndex)
{
	ClearPartySlotInternal(SlotIndex);
}

void UPartyComponent::ServerSwapPartySlots_Implementation(
	int32 FirstSlotIndex, int32 SecondSlotIndex)
{
	SwapPartySlotsInternal(FirstSlotIndex, SecondSlotIndex);
}

void UPartyComponent::OnRep_UnlockedHeroTags(
	const TArray<FGameplayTag>& PreviousHeroTags)
{
	for (const FGameplayTag HeroTag : UnlockedHeroTags)
	{
		if (!PreviousHeroTags.Contains(HeroTag))
		{
			OnHeroUnlocked.Broadcast(HeroTag);
		}
	}
	OnRosterChanged.Broadcast();
}

void UPartyComponent::OnRep_PartyStateRevision()
{
	NormalizePartySlots();
	OnPartyChanged.Broadcast();
}

void UPartyComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UPartyComponent, UnlockedHeroTags, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UPartyComponent, ActivePartyTags, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UPartyComponent, ActiveSlotIndex, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UPartyComponent, PartyStateRevision, COND_OwnerOnly);
}

