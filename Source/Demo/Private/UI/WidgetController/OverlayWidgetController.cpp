#include "UI/WidgetController/OverlayWidgetController.h"
#include "AbilitySystem/PlayerAbilitySystemComponent.h"
#include "AbilitySystem/PlayerAttributeSet.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "Chaos/Deformable/MuscleActivationConstraints.h" 
#include "Player/OPlayerState.h" 
#include "Components/ExpComponent.h" 
#include "Components/PartyComponent.h"
#include "Components/QuestComponent.h"
#include "Quest/QuestDataAsset.h"

void UOverlayWidgetController::BroadcastInitialValues()
{
    // Broadcast values owned by the base controller first.
    Super::BroadcastInitialValues();

    // Broadcast initial health values.
    const UPlayerAttributeSet* PlayerAttributeSet = CastChecked<UPlayerAttributeSet>(AttributeSet);
    OnHealthChanged.Broadcast(PlayerAttributeSet->GetHealth());
    OnMaxHealthChanged.Broadcast(PlayerAttributeSet->GetMaxHealth());

    // The experience component can also provide initial XP values.
    if (AOPlayerState* OPlayerState = Cast<AOPlayerState>(PlayerState))
    {
        if (OPlayerState->ExpComponent)
        {
            // Enable this once the component exposes the required getters.
            // OnXPChangedDelegate.Broadcast(OPlayerState->ExpComponent->GetCurrentXP(), OPlayerState->ExpComponent->GetXPToNext());
        }

        if (OPlayerState->GetPartyComponent())
        {
            OnActivePartyChanged.Broadcast(
                OPlayerState->GetPartyComponent()->GetActivePartyInfo());
        }

        BroadcastTrackedQuest();
    }
}

void UOverlayWidgetController::BindCallbacksToDependencies()
{
    // Bind dependencies owned by the base controller first.
    Super::BindCallbacksToDependencies();

    const UPlayerAttributeSet* PlayerAttributeSet = CastChecked<UPlayerAttributeSet>(AttributeSet);
    
    // Forward health changes to the overlay.
    AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(PlayerAttributeSet->GetHealthAttribute()).AddLambda(
    [this](const FOnAttributeChangeData& Data)
    {
       OnHealthChanged.Broadcast(Data.NewValue);  
    });
    
    // Forward maximum-health changes to the overlay.
    AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(PlayerAttributeSet->GetMaxHealthAttribute()).AddLambda(
    [this](const FOnAttributeChangeData& Data)
    {
        OnMaxHealthChanged.Broadcast(Data.NewValue);  
    });
    
    // Convert gameplay-effect asset tags into UI messages.
    Cast<UPlayerAbilitySystemComponent>(AbilitySystemComponent)->EffectAssetTags.AddLambda(
    [this](const FGameplayTagContainer& AssetTag)
    {
       for (const FGameplayTag& Tag : AssetTag)
       {     
          FGameplayTag MessageTag = FGameplayTag::RequestGameplayTag(FName("Message"));
          if (Tag.MatchesTag(MessageTag))
          {
             const FUIWidgetRow* Row = GetDataTableRowByTag<FUIWidgetRow>(MessageWidgetDataTable, Tag);
             MessageWidgetRowDelegate.Broadcast(*Row);
          }
       }
    });

    // Forward experience-component events.
    if (AOPlayerState* OPlayerState = Cast<AOPlayerState>(PlayerState))
    {
        if (OPlayerState->ExpComponent)
        {
            // Experience remains owned by the component.
            OPlayerState->ExpComponent->OnExperienceChanged.AddDynamic(this, &UOverlayWidgetController::XPChangedCallback);
        }

        // Level is replicated and broadcast by PlayerState on every network role.
        OPlayerState->OnLevelChanged.AddUniqueDynamic(
            this, &UOverlayWidgetController::LevelUpCallback);

        if (UPartyComponent* PartyComponent = OPlayerState->GetPartyComponent())
        {
            PartyComponent->OnPartyChanged.AddUniqueDynamic(
                this, &UOverlayWidgetController::PartyChangedCallback);
        }

        if (UQuestComponent* QuestComponent = OPlayerState->GetQuestComponent())
        {
			QuestComponent->OnQuestListChanged.AddUniqueDynamic(
				this, &UOverlayWidgetController::QuestListChangedCallback);
            QuestComponent->OnTrackedQuestChanged.AddUniqueDynamic(
                this, &UOverlayWidgetController::TrackedQuestChangedCallback);
            QuestComponent->OnQuestCompleted.AddUniqueDynamic(
                this, &UOverlayWidgetController::QuestCompletedCallback);
            QuestComponent->OnQuestRewardClaimed.AddUniqueDynamic(
                this, &UOverlayWidgetController::QuestRewardClaimedCallback);
        }
    }
}


void UOverlayWidgetController::XPChangedCallback(int32 CurrentXP, int32 XPToNext)
{
    OnXPChangedDelegate.Broadcast(CurrentXP, XPToNext);
}

void UOverlayWidgetController::LevelUpCallback(int32 NewLevel)
{
    OnLevelUpDelegate.Broadcast(NewLevel);
}

void UOverlayWidgetController::PartyChangedCallback()
{
    if (const AOPlayerState* OPlayerState = Cast<AOPlayerState>(PlayerState))
    {
        if (UPartyComponent* PartyComponent = OPlayerState->GetPartyComponent())
        {
            OnActivePartyChanged.Broadcast(PartyComponent->GetActivePartyInfo());
        }
    }
}

void UOverlayWidgetController::QuestCompletedCallback(UQuestDataAsset* Quest)
{
    OnQuestCompleted.Broadcast(Quest);

    FQuestNotificationData Notification;
    Notification.Header = FText::FromString(
        TEXT("\u4efb\u52a1\u5b8c\u6210"));
    if (IsValid(Quest))
    {
        Notification.QuestTitle = Quest->Title;
        Notification.RewardText = Quest->RewardText;
    }
    OnQuestNotification.Broadcast(Notification);
}

void UOverlayWidgetController::QuestRewardClaimedCallback(UQuestDataAsset* Quest)
{
    OnQuestRewardClaimed.Broadcast(Quest);
}

void UOverlayWidgetController::QuestListChangedCallback()
{
    BroadcastTrackedQuest();
}

void UOverlayWidgetController::TrackedQuestChangedCallback()
{
    BroadcastTrackedQuest();
}

void UOverlayWidgetController::RefreshQuestUI()
{
    BroadcastTrackedQuest();
}

FPlayerAbilityInfo UOverlayWidgetController::FindAbilityInfoForClass(
	TSubclassOf<UGameplayAbility> AbilityClass) const
{
	return AbilityInfoDataAsset
		? AbilityInfoDataAsset->FindAbilityInfoForClass(AbilityClass)
		: FPlayerAbilityInfo();
}

void UOverlayWidgetController::BroadcastTrackedQuest()
{
    FQuestRuntimeEntry QuestEntry;
    bool bHasTrackedQuest = false;
    if (const AOPlayerState* OPlayerState = Cast<AOPlayerState>(PlayerState))
    {
        if (const UQuestComponent* QuestComponent =
            OPlayerState->GetQuestComponent())
        {
            bHasTrackedQuest = QuestComponent->GetTrackedQuest(QuestEntry);
        }
    }

    OnTrackedQuestChanged.Broadcast(bHasTrackedQuest, QuestEntry);
}
