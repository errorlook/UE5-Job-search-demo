#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QuestComponent.generated.h"

class UQuestDataAsset;

UENUM(BlueprintType)
enum class EQuestRuntimeState : uint8
{
	Active,
	Completed
};

USTRUCT(BlueprintType)
struct FQuestRuntimeEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UQuestDataAsset> Quest = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 CurrentProgress = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	EQuestRuntimeState State = EQuestRuntimeState::Active;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	bool bRewardClaimed = false;

	// Definition-list rows may exist without being accepted by this player.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	bool bAccepted = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FQuestAcceptedSignature, UQuestDataAsset*, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FQuestProgressChangedSignature, UQuestDataAsset*, Quest,
	int32, CurrentProgress, int32, RequiredProgress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FQuestCompletedSignature, UQuestDataAsset*, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FQuestRewardClaimedSignature, UQuestDataAsset*, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FQuestListChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTrackedQuestChangedSignature);

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEMO_API UQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQuestComponent();

	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool AcceptQuest(UQuestDataAsset* Quest);

	// Only succeeds when Lua allows acceptance and a native quest asset exists.
	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool AcceptQuestById(FName QuestId);

	UFUNCTION(BlueprintCallable, Category = "Quest|Progress")
	bool NotifyObjectiveProgress(
		FName ObjectiveTargetId, int32 ProgressAmount = 1);

	UFUNCTION(BlueprintCallable, Category = "Quest|Reward")
	bool ClaimQuestReward(FName QuestId);

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool HasQuest(FName QuestId) const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool IsQuestCompleted(FName QuestId) const;

	// Used by gameplay objectives to award progress only to an eligible player.
	UFUNCTION(BlueprintPure, Category = "Quest|Progress")
	bool HasActiveObjective(FName ObjectiveTargetId) const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool GetQuestEntry(FName QuestId, FQuestRuntimeEntry& OutEntry) const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	TArray<FQuestRuntimeEntry> GetQuestEntries() const;

	UFUNCTION(BlueprintCallable, Category = "Quest|Tracking")
	bool SetTrackedQuest(FName QuestId);

	UFUNCTION(BlueprintPure, Category = "Quest|Tracking")
	bool GetTrackedQuest(FQuestRuntimeEntry& OutEntry) const;

	UFUNCTION(BlueprintPure, Category = "Quest|Tracking")
	FName GetTrackedQuestId() const { return TrackedQuestId; }

	UPROPERTY(BlueprintAssignable, Category = "Quest|Events")
	FQuestAcceptedSignature OnQuestAccepted;

	UPROPERTY(BlueprintAssignable, Category = "Quest|Events")
	FQuestProgressChangedSignature OnQuestProgressChanged;

	UPROPERTY(BlueprintAssignable, Category = "Quest|Events")
	FQuestCompletedSignature OnQuestCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Quest|Events")
	FQuestRewardClaimedSignature OnQuestRewardClaimed;

	UPROPERTY(BlueprintAssignable, Category = "Quest|Events")
	FQuestListChangedSignature OnQuestListChanged;

	UPROPERTY(BlueprintAssignable, Category = "Quest|Events")
	FTrackedQuestChangedSignature OnTrackedQuestChanged;

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool AcceptQuestInternal(UQuestDataAsset* Quest);
	bool NotifyObjectiveProgressInternal(
		FName ObjectiveTargetId, int32 ProgressAmount);
	bool ClaimQuestRewardInternal(FName QuestId);
	bool SetTrackedQuestInternal(FName QuestId);
	bool GrantReward(FQuestRuntimeEntry& QuestEntry);
	void SelectNextTrackedQuest();
	int32 FindQuestIndex(FName QuestId) const;
	UQuestDataAsset* ResolveQuestForPresentation(UQuestDataAsset* Quest) const;
	int32 ResolveRequiredCount(const UQuestDataAsset* Quest) const;

	UFUNCTION()
	void HandleQuestDefinitionsChanged(int32 NewRevision);

	UFUNCTION(Server, Reliable)
	void ServerAcceptQuest(UQuestDataAsset* Quest);

	UFUNCTION(Server, Reliable)
	void ServerNotifyObjectiveProgress(
		FName ObjectiveTargetId, int32 ProgressAmount);

	UFUNCTION(Server, Reliable)
	void ServerClaimQuestReward(FName QuestId);

	UFUNCTION(Server, Reliable)
	void ServerSetTrackedQuest(FName QuestId);

	UFUNCTION()
	void OnRep_QuestEntries(
		const TArray<FQuestRuntimeEntry>& PreviousEntries);

	UFUNCTION()
	void OnRep_TrackedQuestId();

	UPROPERTY(ReplicatedUsing = OnRep_QuestEntries, VisibleAnywhere,
		BlueprintReadOnly, Category = "Quest|State",
		meta = (AllowPrivateAccess = "true"))
	TArray<FQuestRuntimeEntry> QuestEntries;

	UPROPERTY(ReplicatedUsing = OnRep_TrackedQuestId, VisibleAnywhere,
		BlueprintReadOnly, Category = "Quest|Tracking",
		meta = (AllowPrivateAccess = "true"))
	FName TrackedQuestId = NAME_None;
};
