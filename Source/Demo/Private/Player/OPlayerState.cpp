// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/OPlayerState.h"
#include <AbilitySystem/PlayerAbilitySystemComponent.h>
#include <AbilitySystem/PlayerAttributeSet.h>
#include "Components/ExpComponent.h"
#include "Components/PartyComponent.h"
#include "Components/QuestComponent.h"
#include "Net/UnrealNetwork.h"

AOPlayerState::AOPlayerState()
{
	
	AbilitySystemComponent = CreateDefaultSubobject<UPlayerAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	
	AttributeSet = CreateDefaultSubobject<UPlayerAttributeSet>(TEXT("AttributeSet"));

	ExpComponent = CreateDefaultSubobject<UExpComponent>(TEXT("ExpComponent"));
	QuestComponent = CreateDefaultSubobject<UQuestComponent>(TEXT("QuestComponent"));
	PartyComponent = CreateDefaultSubobject<UPartyComponent>(TEXT("PartyComponent"));
	

	NetUpdateFrequency = 100.f;
}

void AOPlayerState::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AOPlayerState, Level);
}

void AOPlayerState::OnRep_Level(int32 OldLevel)
{
	(void)OldLevel;
	BroadcastLevelChanged();
}

bool AOPlayerState::SetPlayerLevel(int32 NewLevel)
{
	if (!HasAuthority() || NewLevel < 1 || NewLevel == Level)
	{
		return false;
	}

	Level = NewLevel;
	if (UPlayerAbilitySystemComponent* PlayerASC =
		Cast<UPlayerAbilitySystemComponent>(AbilitySystemComponent))
	{
		PlayerASC->SetAuthoritativeAbilityLevel(Level);
	}
	ForceNetUpdate();
	BroadcastLevelChanged();
	return true;
}

void AOPlayerState::BroadcastLevelChanged()
{
	OnLevelChanged.Broadcast(Level);
	OnLevelChangedNative.Broadcast(Level);
}

UAbilitySystemComponent* AOPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
