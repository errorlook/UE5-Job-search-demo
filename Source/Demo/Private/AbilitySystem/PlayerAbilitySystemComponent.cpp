// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/PlayerAbilitySystemComponent.h"

#include "PlayerGameplayTags.h"
#include "AbilitySystem/Abilities/PlayerGameplayAbility.h"
#include "Player/OPlayerState.h"

void UPlayerAbilitySystemComponent::AbilityActorInfoSet()
{
	OnGameplayEffectAppliedDelegateToSelf.RemoveAll(this);
	OnGameplayEffectAppliedDelegateToSelf.AddUObject(this,&UPlayerAbilitySystemComponent::EffectApplied);
	
}

void UPlayerAbilitySystemComponent::AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities)
{
	if (!IsOwnerActorAuthoritative()) return;

	for (const TSubclassOf<UGameplayAbility> AbilityClass : StartupAbilities)
	{
		if (!AbilityClass || FindAbilitySpecFromClass(AbilityClass)) continue;

		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(
			AbilityClass, GetAuthoritativeAbilityLevel());
		if (const UPlayerGameplayAbility* PlayerAbility =
			Cast<UPlayerGameplayAbility>(AbilitySpec.Ability))
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(
				PlayerAbility->StartupInputTag);
		}
		GiveAbility(AbilitySpec);
	}
}

void UPlayerAbilitySystemComponent::SetCharacterAbilities(
	const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities)
{
	if (!IsOwnerActorAuthoritative()) return;

	for (const FGameplayAbilitySpecHandle Handle : CharacterAbilityHandles)
	{
		ClearAbility(Handle);
	}
	CharacterAbilityHandles.Reset();

	for (const TSubclassOf<UGameplayAbility> AbilityClass : StartupAbilities)
	{
		if (!AbilityClass) continue;

		FGameplayAbilitySpec AbilitySpec(
			AbilityClass, GetAuthoritativeAbilityLevel());
		if (const UPlayerGameplayAbility* PlayerAbility =
			Cast<UPlayerGameplayAbility>(AbilitySpec.Ability))
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(
				PlayerAbility->StartupInputTag);
		}

		CharacterAbilityHandles.Add(GiveAbility(AbilitySpec));
	}
}

void UPlayerAbilitySystemComponent::SetAuthoritativeAbilityLevel(int32 NewLevel)
{
	if (!IsOwnerActorAuthoritative() || NewLevel < 1)
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle Handle : CharacterAbilityHandles)
	{
		FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(Handle);
		if (AbilitySpec && AbilitySpec->Level != NewLevel)
		{
			AbilitySpec->Level = NewLevel;
			MarkAbilitySpecDirty(*AbilitySpec);
		}
	}
}

int32 UPlayerAbilitySystemComponent::GetAuthoritativeAbilityLevel() const
{
	if (const AOPlayerState* PlayerState = Cast<AOPlayerState>(GetOwnerActor()))
	{
		return PlayerState->GetPlayerLevel();
	}

	return 1;
}

void UPlayerAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;

	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			AbilitySpecInputPressed(AbilitySpec);
            
			
			if (!AbilitySpec.IsActive())
			{
				TryActivateAbility(AbilitySpec.Handle);
			}
		}
	}
}

void UPlayerAbilitySystemComponent::AbilityInputTagHeld(const FGameplayTag& InputTag)
{
	if(!InputTag.IsValid()) return;
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			AbilitySpecInputPressed(AbilitySpec);
			if (!AbilitySpec.IsActive())
			{
				TryActivateAbility(AbilitySpec.Handle);
			}
		}
	}
}

void UPlayerAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if(!InputTag.IsValid()) return;
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			AbilitySpecInputReleased(AbilitySpec);
		}
	}
}

void UPlayerAbilitySystemComponent::EffectApplied(UAbilitySystemComponent* AbilitySystemComponent,
                                                  const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle)
{
	FGameplayTagContainer TagContainer;
	EffectSpec.GetAllAssetTags(TagContainer);
	
	EffectAssetTags.Broadcast(TagContainer);
	
	
}
