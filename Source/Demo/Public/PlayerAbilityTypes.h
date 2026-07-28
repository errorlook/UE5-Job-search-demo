#pragma once


#include "GameplayEffectTypes.h"
#include "PlayerAbilityTypes.generated.h"

USTRUCT(BlueprintType)
struct FPlayerGamePlayEffectContext :public FGameplayEffectContext
{
	GENERATED_BODY()
public:
	
	bool IsCriticalHit() const {return bIsCriticalHit;}  

	bool IsBlockedHit() const {return bIsBlockedHit;}  
	
	void SetIsCriticalHit(bool bInIsCriticalHit) {bIsCriticalHit = bInIsCriticalHit;}
	void SetIsBlockedHit(bool bInIsBlockedHit) {bIsBlockedHit = bInIsBlockedHit;}
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return StaticStruct();
	}
	
	virtual FPlayerGamePlayEffectContext* Duplicate() const override
	{
		FPlayerGamePlayEffectContext * NewContext = new FPlayerGamePlayEffectContext();
		*NewContext = *this;
		if (GetHitResult())
		{
			NewContext->AddHitResult(*GetHitResult(),true);
		}
		return NewContext;
	}
	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;
	
protected:
	
	UPROPERTY()
	bool bIsBlockedHit =false ;
	
	UPROPERTY()
	bool bIsCriticalHit = false ;
};

template<>
struct TStructOpsTypeTraits<FPlayerGamePlayEffectContext> : public TStructOpsTypeTraitsBase2<FPlayerGamePlayEffectContext>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true,
	};
};
