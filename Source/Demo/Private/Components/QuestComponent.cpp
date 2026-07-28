#include "Components/QuestComponent.h"

#include "Components/PartyComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Quest/QuestDataAsset.h"
#include "Scripting/LuaHotReloadSubsystem.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogQuestComponent, Log, All);

namespace
{
const TCHAR* NetModeName(ENetMode NetMode)
{
	switch (NetMode)
	{
	case NM_Client: return TEXT("Client");
	case NM_DedicatedServer: return TEXT("DedicatedServer");
	case NM_ListenServer: return TEXT("ListenServer");
	default: return TEXT("Standalone");
	}
}

const TCHAR* NetRoleName(ENetRole Role)
{
	switch (Role)
	{
	case ROLE_SimulatedProxy: return TEXT("SimulatedProxy");
	case ROLE_AutonomousProxy: return TEXT("AutonomousProxy");
	case ROLE_Authority: return TEXT("Authority");
	default: return TEXT("None");
	}
}
}

UQuestComponent::UQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UQuestComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (ULuaHotReloadSubsystem* HotReload =
				GameInstance->GetSubsystem<ULuaHotReloadSubsystem>())
			{
				HotReload->OnQuestDefinitionsChanged.AddUniqueDynamic(
					this, &UQuestComponent::HandleQuestDefinitionsChanged);
			}
		}
	}
}

void UQuestComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (ULuaHotReloadSubsystem* HotReload =
				GameInstance->GetSubsystem<ULuaHotReloadSubsystem>())
			{
				HotReload->OnQuestDefinitionsChanged.RemoveDynamic(
					this, &UQuestComponent::HandleQuestDefinitionsChanged);
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

bool UQuestComponent::AcceptQuest(UQuestDataAsset* Quest)
{
	if (!IsValid(Quest) || Quest->QuestId.IsNone() || HasQuest(Quest->QuestId))
	{
		return false;
	}

	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerAcceptQuest(Quest);
		return true;
	}

	return AcceptQuestInternal(Quest);
}

bool UQuestComponent::AcceptQuestById(FName QuestId)
{
	if (QuestId.IsNone() || HasQuest(QuestId)) return false;
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const ULuaHotReloadSubsystem* HotReload = GameInstance
		? GameInstance->GetSubsystem<ULuaHotReloadSubsystem>() : nullptr;
	if (!HotReload || !HotReload->CanAcceptQuestDefinition(QuestId))
	{
		return false;
	}

	for (TObjectIterator<UQuestDataAsset> It; It; ++It)
	{
		UQuestDataAsset* Candidate = *It;
		if (IsValid(Candidate) &&
			!Candidate->HasAnyFlags(RF_Transient | RF_ClassDefaultObject) &&
			Candidate->QuestId == QuestId)
		{
			return AcceptQuest(Candidate);
		}
	}

	UE_LOG(LogQuestComponent, Warning,
		TEXT("Quest definition %s cannot be accepted because it has no native "
			"UQuestDataAsset backing gameplay validation and rewards."),
		*QuestId.ToString());
	return false;
}

bool UQuestComponent::AcceptQuestInternal(UQuestDataAsset* Quest)
{
	if (!IsValid(Quest) || Quest->QuestId.IsNone() || HasQuest(Quest->QuestId))
	{
		return false;
	}
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const ULuaHotReloadSubsystem* HotReload = GameInstance
		? GameInstance->GetSubsystem<ULuaHotReloadSubsystem>() : nullptr;
	if (HotReload && HotReload->GetQuestConfigRevision() > 0 &&
		!HotReload->CanAcceptQuestDefinition(Quest->QuestId))
	{
		UE_LOG(LogQuestComponent, Warning,
			TEXT("Quest %s acceptance rejected by the active Lua definition cache."),
			*Quest->QuestId.ToString());
		return false;
	}

	FQuestRuntimeEntry& NewEntry = QuestEntries.AddDefaulted_GetRef();
	NewEntry.Quest = Quest;
	NewEntry.bAccepted = true;
	if (TrackedQuestId.IsNone())
	{
		TrackedQuestId = Quest->QuestId;
		OnTrackedQuestChanged.Broadcast();
	}
	OnQuestAccepted.Broadcast(ResolveQuestForPresentation(Quest));
	OnQuestListChanged.Broadcast();
	if (AActor* Owner = GetOwner()) Owner->ForceNetUpdate();

	return true;
}

bool UQuestComponent::NotifyObjectiveProgress(
	FName ObjectiveTargetId, int32 ProgressAmount)
{
	if (ObjectiveTargetId.IsNone() || ProgressAmount <= 0) return false;

	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerNotifyObjectiveProgress(ObjectiveTargetId, ProgressAmount);
		return true;
	}

	return NotifyObjectiveProgressInternal(ObjectiveTargetId, ProgressAmount);
}

bool UQuestComponent::NotifyObjectiveProgressInternal(
	FName ObjectiveTargetId, int32 ProgressAmount)
{
	bool bChanged = false;
	for (FQuestRuntimeEntry& Entry : QuestEntries)
	{
		if (!IsValid(Entry.Quest) ||
			Entry.State != EQuestRuntimeState::Active ||
			!Entry.Quest->MatchesObjectiveTarget(ObjectiveTargetId))
		{
			continue;
		}

		const int32 RequiredCount = ResolveRequiredCount(Entry.Quest);
		Entry.CurrentProgress = FMath::Clamp(
			Entry.CurrentProgress + ProgressAmount, 0, RequiredCount);
		OnQuestProgressChanged.Broadcast(
			ResolveQuestForPresentation(Entry.Quest),
			Entry.CurrentProgress, RequiredCount);
		bChanged = true;

		if (Entry.CurrentProgress >= RequiredCount)
		{
			Entry.State = EQuestRuntimeState::Completed;
			OnQuestCompleted.Broadcast(ResolveQuestForPresentation(Entry.Quest));
			GrantReward(Entry);

		}
	}

	if (bChanged)
	{
		SelectNextTrackedQuest();
		OnQuestListChanged.Broadcast();
		if (AActor* Owner = GetOwner()) Owner->ForceNetUpdate();
	}
	return bChanged;
}

bool UQuestComponent::ClaimQuestReward(FName QuestId)
{
	const int32 QuestIndex = FindQuestIndex(QuestId);
	if (!QuestEntries.IsValidIndex(QuestIndex) ||
		QuestEntries[QuestIndex].State != EQuestRuntimeState::Completed ||
		QuestEntries[QuestIndex].bRewardClaimed)
	{
		return false;
	}

	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerClaimQuestReward(QuestId);
		return true;
	}

	return ClaimQuestRewardInternal(QuestId);
}

bool UQuestComponent::ClaimQuestRewardInternal(FName QuestId)
{
	const int32 QuestIndex = FindQuestIndex(QuestId);
	if (!QuestEntries.IsValidIndex(QuestIndex)) return false;

	const bool bClaimed = GrantReward(QuestEntries[QuestIndex]);
	if (bClaimed)
	{
		OnQuestListChanged.Broadcast();
		if (AActor* Owner = GetOwner()) Owner->ForceNetUpdate();
	}
	return bClaimed;
}

bool UQuestComponent::GrantReward(FQuestRuntimeEntry& QuestEntry)
{
	if (!IsValid(QuestEntry.Quest) ||
		QuestEntry.State != EQuestRuntimeState::Completed ||
		QuestEntry.bRewardClaimed)
	{
		return false;
	}

	if (QuestEntry.Quest->RewardHeroTag.IsValid())
	{
		AActor* Owner = GetOwner();
		if (UPartyComponent* PartyComponent = Owner
			? Owner->FindComponentByClass<UPartyComponent>()
			: nullptr)
		{
			PartyComponent->UnlockHero(QuestEntry.Quest->RewardHeroTag);
		}
		else
		{
			return false;
		}
	}

	QuestEntry.bRewardClaimed = true;
	OnQuestRewardClaimed.Broadcast(
		ResolveQuestForPresentation(QuestEntry.Quest));
	return true;
}

bool UQuestComponent::HasQuest(FName QuestId) const
{
	return FindQuestIndex(QuestId) != INDEX_NONE;
}

bool UQuestComponent::IsQuestCompleted(FName QuestId) const
{
	const int32 QuestIndex = FindQuestIndex(QuestId);
	return QuestEntries.IsValidIndex(QuestIndex) &&
		QuestEntries[QuestIndex].State == EQuestRuntimeState::Completed;
}

bool UQuestComponent::HasActiveObjective(FName ObjectiveTargetId) const
{
	if (ObjectiveTargetId.IsNone()) return false;

	return QuestEntries.ContainsByPredicate(
		[ObjectiveTargetId](const FQuestRuntimeEntry& Entry)
		{
			return IsValid(Entry.Quest) &&
				Entry.State == EQuestRuntimeState::Active &&
				Entry.Quest->MatchesObjectiveTarget(ObjectiveTargetId);
		});
}

bool UQuestComponent::GetQuestEntry(
	FName QuestId, FQuestRuntimeEntry& OutEntry) const
{
	const int32 QuestIndex = FindQuestIndex(QuestId);
	if (!QuestEntries.IsValidIndex(QuestIndex)) return false;
	OutEntry = QuestEntries[QuestIndex];
	OutEntry.bAccepted = true;
	OutEntry.Quest = ResolveQuestForPresentation(OutEntry.Quest);
	return true;
}

TArray<FQuestRuntimeEntry> UQuestComponent::GetQuestEntries() const
{
	TArray<FQuestRuntimeEntry> Result;
	TSet<FName> AddedQuestIds;
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const ULuaHotReloadSubsystem* HotReload = GameInstance
		? GameInstance->GetSubsystem<ULuaHotReloadSubsystem>() : nullptr;

	if (HotReload)
	{
		for (const FLuaQuestDefinition& Definition :
			HotReload->GetQuestDefinitions())
		{
			if (!Definition.bVisible) continue;
			FQuestRuntimeEntry ViewEntry;
			const int32 RuntimeIndex = FindQuestIndex(Definition.QuestId);
			if (QuestEntries.IsValidIndex(RuntimeIndex))
			{
				ViewEntry = QuestEntries[RuntimeIndex];
				ViewEntry.bAccepted = true;
			}
			else
			{
				ViewEntry.bAccepted = false;
			}
			ViewEntry.Quest = HotReload->FindQuestViewAsset(Definition.QuestId);
			if (IsValid(ViewEntry.Quest))
			{
				Result.Add(MoveTemp(ViewEntry));
				AddedQuestIds.Add(Definition.QuestId);
			}
		}
	}

	// Accepted state outlives deleted Lua definitions and remains inspectable.
	for (const FQuestRuntimeEntry& RuntimeEntry : QuestEntries)
	{
		if (!IsValid(RuntimeEntry.Quest) ||
			AddedQuestIds.Contains(RuntimeEntry.Quest->QuestId))
		{
			continue;
		}
		FQuestRuntimeEntry PreservedEntry = RuntimeEntry;
		PreservedEntry.bAccepted = true;
		Result.Add(MoveTemp(PreservedEntry));
	}
	return Result;
}

bool UQuestComponent::SetTrackedQuest(FName QuestId)
{
	if (!QuestId.IsNone())
	{
		const int32 QuestIndex = FindQuestIndex(QuestId);
		if (!QuestEntries.IsValidIndex(QuestIndex) ||
			QuestEntries[QuestIndex].State != EQuestRuntimeState::Active)
		{
			return false;
		}
	}

	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerSetTrackedQuest(QuestId);
		return true;
	}

	return SetTrackedQuestInternal(QuestId);
}

bool UQuestComponent::SetTrackedQuestInternal(FName QuestId)
{
	if (TrackedQuestId == QuestId) return false;
	if (!QuestId.IsNone())
	{
		const int32 QuestIndex = FindQuestIndex(QuestId);
		if (!QuestEntries.IsValidIndex(QuestIndex) ||
			QuestEntries[QuestIndex].State != EQuestRuntimeState::Active)
		{
			return false;
		}
	}

	TrackedQuestId = QuestId;
	OnTrackedQuestChanged.Broadcast();
	OnQuestListChanged.Broadcast();
	if (AActor* Owner = GetOwner()) Owner->ForceNetUpdate();
	return true;
}

bool UQuestComponent::GetTrackedQuest(FQuestRuntimeEntry& OutEntry) const
{
	const int32 QuestIndex = FindQuestIndex(TrackedQuestId);
	if (!QuestEntries.IsValidIndex(QuestIndex) ||
		QuestEntries[QuestIndex].State != EQuestRuntimeState::Active)
	{
		return false;
	}

	OutEntry = QuestEntries[QuestIndex];
	OutEntry.bAccepted = true;
	OutEntry.Quest = ResolveQuestForPresentation(OutEntry.Quest);
	return true;
}

void UQuestComponent::SelectNextTrackedQuest()
{
	const int32 TrackedIndex = FindQuestIndex(TrackedQuestId);
	if (QuestEntries.IsValidIndex(TrackedIndex) &&
		QuestEntries[TrackedIndex].State == EQuestRuntimeState::Active)
	{
		return;
	}

	FName NextQuestId = NAME_None;
	for (const FQuestRuntimeEntry& Entry : QuestEntries)
	{
		if (IsValid(Entry.Quest) &&
			Entry.State == EQuestRuntimeState::Active)
		{
			NextQuestId = Entry.Quest->QuestId;
			break;
		}
	}

	if (TrackedQuestId != NextQuestId)
	{
		TrackedQuestId = NextQuestId;
		OnTrackedQuestChanged.Broadcast();
	}
}

int32 UQuestComponent::FindQuestIndex(FName QuestId) const
{
	if (QuestId.IsNone()) return INDEX_NONE;
	return QuestEntries.IndexOfByPredicate(
		[QuestId](const FQuestRuntimeEntry& Entry)
		{
			return IsValid(Entry.Quest) && Entry.Quest->QuestId == QuestId;
		});
}

void UQuestComponent::ServerAcceptQuest_Implementation(UQuestDataAsset* Quest)
{
	AcceptQuestInternal(Quest);
}

void UQuestComponent::ServerNotifyObjectiveProgress_Implementation(
	FName ObjectiveTargetId, int32 ProgressAmount)
{
	NotifyObjectiveProgressInternal(ObjectiveTargetId, ProgressAmount);
}

void UQuestComponent::ServerClaimQuestReward_Implementation(FName QuestId)
{
	ClaimQuestRewardInternal(QuestId);
}

void UQuestComponent::ServerSetTrackedQuest_Implementation(FName QuestId)
{
	SetTrackedQuestInternal(QuestId);
}

void UQuestComponent::OnRep_QuestEntries(
	const TArray<FQuestRuntimeEntry>& PreviousEntries)
{
	for (const FQuestRuntimeEntry& Entry : QuestEntries)
	{
		if (!IsValid(Entry.Quest)) continue;

		const FQuestRuntimeEntry* PreviousEntry = PreviousEntries.FindByPredicate(
			[&Entry](const FQuestRuntimeEntry& Candidate)
			{
				return IsValid(Candidate.Quest) &&
					Candidate.Quest->QuestId == Entry.Quest->QuestId;
			});

		if (!PreviousEntry)
		{
			OnQuestAccepted.Broadcast(ResolveQuestForPresentation(Entry.Quest));
		}
		else
		{
			if (PreviousEntry->CurrentProgress != Entry.CurrentProgress)
			{
				OnQuestProgressChanged.Broadcast(
					ResolveQuestForPresentation(Entry.Quest),
					Entry.CurrentProgress, ResolveRequiredCount(Entry.Quest));
			}
			if (PreviousEntry->State != EQuestRuntimeState::Completed &&
				Entry.State == EQuestRuntimeState::Completed)
			{
				OnQuestCompleted.Broadcast(
					ResolveQuestForPresentation(Entry.Quest));
			}
			if (!PreviousEntry->bRewardClaimed && Entry.bRewardClaimed)
			{
				OnQuestRewardClaimed.Broadcast(
					ResolveQuestForPresentation(Entry.Quest));
			}
		}
	}
	OnQuestListChanged.Broadcast();
}

UQuestDataAsset* UQuestComponent::ResolveQuestForPresentation(
	UQuestDataAsset* Quest) const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const ULuaHotReloadSubsystem* HotReload = GameInstance
		? GameInstance->GetSubsystem<ULuaHotReloadSubsystem>() : nullptr;
	return HotReload ? HotReload->ResolveQuestViewAsset(Quest) : Quest;
}

int32 UQuestComponent::ResolveRequiredCount(const UQuestDataAsset* Quest) const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const ULuaHotReloadSubsystem* HotReload = GameInstance
		? GameInstance->GetSubsystem<ULuaHotReloadSubsystem>() : nullptr;
	return HotReload ? HotReload->GetEffectiveTargetCount(Quest)
		: FMath::Max(1, IsValid(Quest) ? Quest->RequiredCount : 1);
}

void UQuestComponent::HandleQuestDefinitionsChanged(int32 NewRevision)
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const ULuaHotReloadSubsystem* HotReload = GameInstance
		? GameInstance->GetSubsystem<ULuaHotReloadSubsystem>() : nullptr;
	if (HotReload)
	{
		for (const FQuestRuntimeEntry& Entry : QuestEntries)
		{
			if (IsValid(Entry.Quest) &&
				!HotReload->FindQuestDefinition(Entry.Quest->QuestId))
			{
				UE_LOG(LogQuestComponent, Warning,
					TEXT("[LuaHotReload] Accepted quest %s has no current Lua "
						"definition; progress and native fallback text are preserved."),
					*Entry.Quest->QuestId.ToString());
			}
		}
	}
	OnQuestListChanged.Broadcast();
	OnTrackedQuestChanged.Broadcast();
	const AActor* Owner = GetOwner();
	const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
	const ENetRole LocalRole = Owner ? Owner->GetLocalRole() : ROLE_None;
	UE_LOG(LogQuestComponent, Log,
		TEXT("[LuaHotReload] Quest UI refreshed. Revision=%d NetMode=%s "
			"Role=%s Component=%s Owner=%s"),
		NewRevision,
		NetModeName(NetMode), NetRoleName(LocalRole),
		*GetPathName(), *GetNameSafe(Owner));
}

void UQuestComponent::OnRep_TrackedQuestId()
{
	OnTrackedQuestChanged.Broadcast();
	OnQuestListChanged.Broadcast();
}

void UQuestComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UQuestComponent, QuestEntries, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UQuestComponent, TrackedQuestId, COND_OwnerOnly);
}
