// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/OPlayerState.h"
#include <AbilitySystem/PlayerAbilitySystemComponent.h>
#include <AbilitySystem/PlayerAttributeSet.h>

AOPlayerState::AOPlayerState()
{
	//创建能力系统组件
	AbilitySystemComponent = CreateDefaultSubobject<UPlayerAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	//构建属性集
	AttributeSet = CreateDefaultSubobject<UPlayerAttributeSet>(TEXT("AttributeSet"));

	//网络更新频率
	NetUpdateFrequency = 100.f;
}

UAbilitySystemComponent* AOPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
