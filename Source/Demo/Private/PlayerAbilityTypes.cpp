
#include  "PlayerAbilityTypes.h"


bool FPlayerGamePlayEffectContext::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	uint32 RepBits = 0;
	if (Ar.IsSaving())
	{
		if (bReplicateInstigator && Instigator.IsValid())
		{
			RepBits |= 1 << 0;
		}
		if (bReplicateEffectCauser && EffectCauser.IsValid() )
		{
			RepBits |= 1 << 1;
		}
		if (AbilityCDO.IsValid())
		{
			RepBits |= 1 << 2;
		}
		if (bReplicateSourceObject && SourceObject.IsValid())
		{
			RepBits |= 1 << 3;
		}
		if (Actors.Num() > 0)
		{
			RepBits |= 1 << 4;
		}
		if (HitResult.IsValid())
		{
			RepBits |= 1 << 5;
		}
		if (bHasWorldOrigin)
		{
			RepBits |= 1 << 6;
		}
		if (bIsBlockedHit)
		{
			RepBits |= 1 << 7;
		}
		if (bIsCriticalHit)
		{
			RepBits |= 1 << 8;
		}
	}
	
	bOutSuccess = true;
	Ar.SerializeBits(&RepBits, 9);
	bOutSuccess &= !Ar.IsError();

	if (Ar.IsLoading())
	{
		Instigator.Reset();
		EffectCauser.Reset();
		AbilityCDO.Reset();
		AbilityInstanceNotReplicated.Reset();
		AbilityLevel = 1;
		SourceObject.Reset();
		InstigatorAbilitySystemComponent.Reset();
		Actors.Reset();
		HitResult.Reset();
		WorldOrigin = FVector::ZeroVector;
		bHasWorldOrigin = false;
		bReplicateSourceObject = false;
		bIsBlockedHit = false;
		bIsCriticalHit = false;
	}
	
	if (RepBits & (1 << 0))
	{
		Ar << Instigator;
	}
	if (RepBits & (1 << 1))
	{
		Ar << EffectCauser;
	}
	if (RepBits & (1 << 2))
	{
		Ar << AbilityCDO;
	}
	if (RepBits & (1 << 3))
	{
		Ar << SourceObject;
		if (Ar.IsLoading())
		{
			bReplicateSourceObject = true;
		}
	}
	if (RepBits & (1 << 4))
	{
		bOutSuccess &= SafeNetSerializeTArray_Default<31>(Ar, Actors);
	}
	if (RepBits & (1 << 5))
	{
		if (Ar.IsLoading())
		{
			if (!HitResult.IsValid())
			{
				HitResult = TSharedPtr<FHitResult>(new FHitResult());
			}
		}
		bool bHitResultSuccess = true;
		HitResult->NetSerialize(Ar, Map, bHitResultSuccess);
		bOutSuccess &= bHitResultSuccess;
	}
	if (RepBits & (1 << 6))
	{
		Ar << WorldOrigin;
		bHasWorldOrigin = true;
	}
	else
	{
		bHasWorldOrigin = false;
	}
	if (RepBits & (1 << 7))
	{
		Ar << bIsBlockedHit;
	}
	if (RepBits & (1 << 8))
	{
		Ar << bIsCriticalHit;
	}
	if (Ar.IsLoading())
	{
		AddInstigator(Instigator.Get(),EffectCauser.Get());
	}
	bOutSuccess &= !Ar.IsError();
	return true;
}
