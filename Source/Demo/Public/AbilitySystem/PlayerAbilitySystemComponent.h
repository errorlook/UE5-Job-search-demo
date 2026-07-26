// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "PlayerAbilitySystemComponent.generated.h"


DECLARE_MULTICAST_DELEGATE_OneParam(FEffectAssetTags,const FGameplayTagContainer& );
/**
 * 
 */
UCLASS(BlueprintType, Blueprintable)
class DEMO_API UPlayerAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	void AbilityActorInfoSet();
	
	FEffectAssetTags EffectAssetTags;
	
	UFUNCTION(BlueprintCallable, Category = "Ability|Loadout")
	void AddCharacterAbilities(
		const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities);

	UFUNCTION(BlueprintCallable, Category = "Ability|Loadout")
	void SetCharacterAbilities(
		const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities);

	/** Keeps granted ability specs aligned with the authoritative PlayerState level. */
	void SetAuthoritativeAbilityLevel(int32 NewLevel);

	UFUNCTION(BlueprintCallable, Category = "Ability|Input")
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	UFUNCTION(BlueprintCallable, Category = "Ability|Input")
	void AbilityInputTagHeld(const FGameplayTag& InputTag);

	UFUNCTION(BlueprintCallable, Category = "Ability|Input")
	void AbilityInputTagReleased(const FGameplayTag& InputTag);
protected:
	
	void EffectApplied(UAbilitySystemComponent *AbilitySystemComponent , const FGameplayEffectSpec &EffectSpec ,FActiveGameplayEffectHandle ActiveEffectHandle );

	UPROPERTY(Transient)
	TArray<FGameplayAbilitySpecHandle> CharacterAbilityHandles;

	int32 GetAuthoritativeAbilityLevel() const;
};
