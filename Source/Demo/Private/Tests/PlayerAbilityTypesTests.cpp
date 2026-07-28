#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/PlayerDamageGameplayAbility.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "PlayerAbilityTypes.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

namespace
{
	constexpr int64 MaxSerializedContextBits = 64 * 1024;

	bool RoundTripContext(
		FAutomationTestBase& Test,
		const TCHAR* CaseName,
		FPlayerGamePlayEffectContext& Source,
		FPlayerGamePlayEffectContext& Destination)
	{
		FBitWriter Writer(MaxSerializedContextBits);
		FObjectAndNameAsStringProxyArchive SaveArchive(Writer, false);
		bool bSaveSuccess = false;
		const bool bSaveResult = Source.NetSerialize(SaveArchive, nullptr, bSaveSuccess);

		Test.TestTrue(*FString::Printf(TEXT("%s save returns true"), CaseName), bSaveResult);
		Test.TestTrue(*FString::Printf(TEXT("%s save succeeds"), CaseName), bSaveSuccess);
		Test.TestFalse(*FString::Printf(TEXT("%s writer has no error"), CaseName), Writer.IsError());

		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FObjectAndNameAsStringProxyArchive LoadArchive(Reader, true);
		bool bLoadSuccess = false;
		const bool bLoadResult = Destination.NetSerialize(LoadArchive, nullptr, bLoadSuccess);

		Test.TestTrue(*FString::Printf(TEXT("%s load returns true"), CaseName), bLoadResult);
		Test.TestTrue(*FString::Printf(TEXT("%s load succeeds"), CaseName), bLoadSuccess);
		Test.TestFalse(*FString::Printf(TEXT("%s reader has no error"), CaseName), Reader.IsError());
		Test.TestEqual(
			*FString::Printf(TEXT("%s consumes every serialized bit"), CaseName),
			Reader.GetPosBits(),
			Writer.GetNumBits());
		return bSaveResult && bSaveSuccess && !Writer.IsError()
			&& bLoadResult && bLoadSuccess && !Reader.IsError();
	}

	FHitResult MakeHitResult()
	{
		FHitResult HitResult;
		HitResult.bBlockingHit = true;
		HitResult.Time = 0.25f;
		HitResult.Location = FVector(100.0, 200.0, 300.0);
		HitResult.ImpactPoint = FVector(101.0, 202.0, 303.0);
		HitResult.Normal = FVector::UpVector;
		HitResult.ImpactNormal = FVector::ForwardVector;
		HitResult.TraceStart = FVector(10.0, 20.0, 30.0);
		HitResult.TraceEnd = FVector(400.0, 500.0, 600.0);
		HitResult.Item = 7;
		HitResult.FaceIndex = 11;
		HitResult.ElementIndex = 13;
		HitResult.PenetrationDepth = 2.5f;
		return HitResult;
	}

	void TestEmptyContext(FAutomationTestBase& Test, const TCHAR* Prefix, const FPlayerGamePlayEffectContext& Context)
	{
		Test.TestNull(*FString::Printf(TEXT("%s instigator is clear"), Prefix), Context.GetInstigator());
		Test.TestNull(*FString::Printf(TEXT("%s effect causer is clear"), Prefix), Context.GetEffectCauser());
		Test.TestNull(*FString::Printf(TEXT("%s ability CDO is clear"), Prefix), Context.GetAbility());
		Test.TestNull(
			*FString::Printf(TEXT("%s non-replicated ability instance is clear"), Prefix),
			Context.GetAbilityInstance_NotReplicated());
		Test.TestEqual(
			*FString::Printf(TEXT("%s ability level is the safe default"), Prefix),
			Context.GetAbilityLevel(),
			1);
		Test.TestNull(*FString::Printf(TEXT("%s source object is clear"), Prefix), Context.GetSourceObject());
		Test.TestEqual(*FString::Printf(TEXT("%s actors are clear"), Prefix), Context.GetActors().Num(), 0);
		Test.TestNull(*FString::Printf(TEXT("%s hit result is clear"), Prefix), Context.GetHitResult());
		Test.TestFalse(*FString::Printf(TEXT("%s world origin is unset"), Prefix), Context.HasOrigin());
		Test.TestEqual(*FString::Printf(TEXT("%s world origin is zero"), Prefix), Context.GetOrigin(), FVector::ZeroVector);
		Test.TestFalse(*FString::Printf(TEXT("%s critical hit is false"), Prefix), Context.IsCriticalHit());
		Test.TestFalse(*FString::Printf(TEXT("%s blocked hit is false"), Prefix), Context.IsBlockedHit());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlayerGameplayEffectContextNetSerializeTest,
	"Demo.AbilitySystem.PlayerGameplayEffectContext.NetSerialize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerGameplayEffectContextNetSerializeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	AActor* Instigator = GetMutableDefault<AActor>();
	APawn* EffectCauser = GetMutableDefault<APawn>();
	ACharacter* SourceObject = GetMutableDefault<ACharacter>();
	UGameplayAbility* AbilityCDO = GetMutableDefault<UPlayerDamageGameplayAbility>();

	TestNotNull(TEXT("stable instigator exists"), Instigator);
	TestNotNull(TEXT("stable effect causer exists"), EffectCauser);
	TestNotNull(TEXT("stable source object exists"), SourceObject);
	TestNotNull(TEXT("ability CDO exists"), AbilityCDO);

	{
		FPlayerGamePlayEffectContext Source;
		FPlayerGamePlayEffectContext Destination;
		RoundTripContext(*this, TEXT("empty"), Source, Destination);
		TestEmptyContext(*this, TEXT("empty"), Destination);
	}

	{
		FPlayerGamePlayEffectContext Source;
		Source.AddInstigator(Instigator, EffectCauser);
		FPlayerGamePlayEffectContext Destination;
		RoundTripContext(*this, TEXT("instigator and effect causer"), Source, Destination);
		TestEqual(TEXT("instigator round-trips"), Destination.GetInstigator(), Instigator);
		TestEqual(
			TEXT("effect causer round-trips"),
			Destination.GetEffectCauser(),
			static_cast<AActor*>(EffectCauser));
	}

	{
		FPlayerGamePlayEffectContext Source;
		Source.SetAbility(AbilityCDO);
		FPlayerGamePlayEffectContext Destination;
		RoundTripContext(*this, TEXT("ability CDO"), Source, Destination);
		TestEqual(
			TEXT("ability CDO round-trips"),
			Destination.GetAbility(),
			static_cast<const UGameplayAbility*>(AbilityCDO));
	}

	{
		FPlayerGamePlayEffectContext Source;
		Source.AddSourceObject(SourceObject);
		FPlayerGamePlayEffectContext Destination;
		RoundTripContext(*this, TEXT("source object"), Source, Destination);
		TestEqual(
			TEXT("source object round-trips"),
			Destination.GetSourceObject(),
			static_cast<UObject*>(SourceObject));
	}

	{
		FPlayerGamePlayEffectContext Source;
		const TArray<TWeakObjectPtr<AActor>> Actors = {Instigator, EffectCauser};
		Source.AddActors(Actors);
		FPlayerGamePlayEffectContext Destination;
		RoundTripContext(*this, TEXT("actors"), Source, Destination);
		TestEqual(TEXT("actor count round-trips"), Destination.GetActors().Num(), Actors.Num());
		if (Destination.GetActors().Num() == Actors.Num())
		{
			TestEqual(TEXT("first actor round-trips"), Destination.GetActors()[0].Get(), Instigator);
			TestEqual(
				TEXT("second actor round-trips"),
				Destination.GetActors()[1].Get(),
				static_cast<AActor*>(EffectCauser));
		}
	}

	{
		const FHitResult ExpectedHitResult = MakeHitResult();
		FPlayerGamePlayEffectContext Source;
		Source.AddHitResult(ExpectedHitResult);
		FPlayerGamePlayEffectContext Destination;
		RoundTripContext(*this, TEXT("hit result"), Source, Destination);
		const FHitResult* ActualHitResult = Destination.GetHitResult();
		TestNotNull(TEXT("hit result round-trips"), ActualHitResult);
		if (ActualHitResult)
		{
			TestEqual(
				TEXT("hit blocking flag round-trips"),
				static_cast<bool>(ActualHitResult->bBlockingHit),
				static_cast<bool>(ExpectedHitResult.bBlockingHit));
			TestEqual(TEXT("hit time round-trips"), ActualHitResult->Time, ExpectedHitResult.Time);
			TestEqual(TEXT("hit location round-trips"), ActualHitResult->Location, ExpectedHitResult.Location);
			TestEqual(TEXT("hit impact point round-trips"), ActualHitResult->ImpactPoint, ExpectedHitResult.ImpactPoint);
			TestEqual(TEXT("hit normal round-trips"), ActualHitResult->Normal, ExpectedHitResult.Normal);
			TestEqual(TEXT("hit impact normal round-trips"), ActualHitResult->ImpactNormal, ExpectedHitResult.ImpactNormal);
			TestEqual(TEXT("hit trace start round-trips"), ActualHitResult->TraceStart, ExpectedHitResult.TraceStart);
			TestEqual(TEXT("hit trace end round-trips"), ActualHitResult->TraceEnd, ExpectedHitResult.TraceEnd);
			TestEqual(TEXT("hit item round-trips"), ActualHitResult->Item, ExpectedHitResult.Item);
			TestEqual(TEXT("hit face index round-trips"), ActualHitResult->FaceIndex, ExpectedHitResult.FaceIndex);
			TestEqual(TEXT("hit element index round-trips"), ActualHitResult->ElementIndex, ExpectedHitResult.ElementIndex);
			TestEqual(TEXT("hit penetration depth round-trips"), ActualHitResult->PenetrationDepth, ExpectedHitResult.PenetrationDepth);
		}
	}

	{
		const FVector ExpectedOrigin(111.0, 222.0, 333.0);
		FPlayerGamePlayEffectContext Source;
		Source.AddOrigin(ExpectedOrigin);
		FPlayerGamePlayEffectContext Destination;
		RoundTripContext(*this, TEXT("world origin"), Source, Destination);
		TestTrue(TEXT("world origin remains set"), Destination.HasOrigin());
		TestEqual(TEXT("world origin round-trips"), Destination.GetOrigin(), ExpectedOrigin);
	}

	{
		FPlayerGamePlayEffectContext Source;
		Source.SetIsCriticalHit(true);
		FPlayerGamePlayEffectContext Destination;
		RoundTripContext(*this, TEXT("critical hit true"), Source, Destination);
		TestTrue(TEXT("critical hit true round-trips"), Destination.IsCriticalHit());
		TestFalse(TEXT("blocked hit remains false"), Destination.IsBlockedHit());
	}

	{
		FPlayerGamePlayEffectContext Source;
		Source.SetIsBlockedHit(true);
		FPlayerGamePlayEffectContext Destination;
		RoundTripContext(*this, TEXT("blocked hit true"), Source, Destination);
		TestTrue(TEXT("blocked hit true round-trips"), Destination.IsBlockedHit());
		TestFalse(TEXT("critical hit remains false"), Destination.IsCriticalHit());
	}

	{
		FPlayerGamePlayEffectContext Source;
		Source.SetIsCriticalHit(false);
		Source.SetIsBlockedHit(false);
		FPlayerGamePlayEffectContext Destination;
		Destination.SetIsCriticalHit(true);
		Destination.SetIsBlockedHit(true);
		RoundTripContext(*this, TEXT("false hit flags"), Source, Destination);
		TestFalse(TEXT("critical hit false clears destination"), Destination.IsCriticalHit());
		TestFalse(TEXT("blocked hit false clears destination"), Destination.IsBlockedHit());
	}

	{
		FPlayerGamePlayEffectContext Source;
		TArray<TWeakObjectPtr<AActor>> TooManyActors;
		TooManyActors.Init(Instigator, 32);
		Source.AddActors(TooManyActors);

		FBitWriter Writer(MaxSerializedContextBits);
		FObjectAndNameAsStringProxyArchive SaveArchive(Writer, false);
		bool bSaveSuccess = true;
		const bool bSaveResult = Source.NetSerialize(SaveArchive, nullptr, bSaveSuccess);
		TestTrue(TEXT("actor overflow still uses the native serializer"), bSaveResult);
		TestFalse(TEXT("actor overflow reports bOutSuccess false"), bSaveSuccess);
		TestFalse(TEXT("actor overflow does not corrupt the archive"), Writer.IsError());
	}

	FPlayerGamePlayEffectContext MixedSource;
	MixedSource.AddInstigator(Instigator, EffectCauser);
	MixedSource.SetAbility(AbilityCDO);
	MixedSource.AddSourceObject(SourceObject);
	const TArray<TWeakObjectPtr<AActor>> MixedActors = {EffectCauser, Instigator};
	MixedSource.AddActors(MixedActors);
	const FHitResult MixedHitResult = MakeHitResult();
	MixedSource.AddHitResult(MixedHitResult);
	const FVector MixedOrigin(777.0, 888.0, 999.0);
	MixedSource.AddOrigin(MixedOrigin);
	MixedSource.SetIsCriticalHit(true);
	MixedSource.SetIsBlockedHit(true);

	{
		FPlayerGamePlayEffectContext Destination;
		RoundTripContext(*this, TEXT("mixed fields"), MixedSource, Destination);
		TestEqual(TEXT("mixed instigator matches"), Destination.GetInstigator(), MixedSource.GetInstigator());
		TestEqual(TEXT("mixed effect causer matches"), Destination.GetEffectCauser(), MixedSource.GetEffectCauser());
		TestEqual(TEXT("mixed ability matches"), Destination.GetAbility(), MixedSource.GetAbility());
		TestEqual(TEXT("mixed source object matches"), Destination.GetSourceObject(), MixedSource.GetSourceObject());
		TestEqual(TEXT("mixed actor count matches"), Destination.GetActors().Num(), MixedSource.GetActors().Num());
		if (Destination.GetActors().Num() == MixedSource.GetActors().Num())
		{
			for (int32 ActorIndex = 0; ActorIndex < Destination.GetActors().Num(); ++ActorIndex)
			{
				TestEqual(
					*FString::Printf(TEXT("mixed actor %d matches"), ActorIndex),
					Destination.GetActors()[ActorIndex].Get(),
					MixedSource.GetActors()[ActorIndex].Get());
			}
		}
		TestNotNull(TEXT("mixed hit result exists"), Destination.GetHitResult());
		TestTrue(TEXT("mixed world origin remains set"), Destination.HasOrigin());
		TestEqual(TEXT("mixed world origin matches"), Destination.GetOrigin(), MixedSource.GetOrigin());
		TestEqual(TEXT("mixed critical flag matches"), Destination.IsCriticalHit(), MixedSource.IsCriticalHit());
		TestEqual(TEXT("mixed blocked flag matches"), Destination.IsBlockedHit(), MixedSource.IsBlockedHit());
	}

	{
		FPlayerGamePlayEffectContext EmptySource;
		FPlayerGamePlayEffectContext DirtyDestination = MixedSource;
		RoundTripContext(*this, TEXT("dirty destination reset"), EmptySource, DirtyDestination);
		TestEmptyContext(*this, TEXT("dirty destination reset"), DirtyDestination);

		FPlayerGamePlayEffectContext ReserializedDestination;
		RoundTripContext(
			*this,
			TEXT("cleaned destination reserialized"),
			DirtyDestination,
			ReserializedDestination);
		TestEmptyContext(*this, TEXT("cleaned destination reserialized"), ReserializedDestination);
	}

	TestEqual(
		TEXT("custom script struct is reported"),
		MixedSource.GetScriptStruct(),
		FPlayerGamePlayEffectContext::StaticStruct());

	{
		FGameplayEffectContextHandle SourceHandle(new FPlayerGamePlayEffectContext(MixedSource));
		FBitWriter Writer(MaxSerializedContextBits);
		FObjectAndNameAsStringProxyArchive SaveArchive(Writer, false);
		bool bSaveSuccess = false;
		const bool bSaveResult = SourceHandle.NetSerialize(SaveArchive, nullptr, bSaveSuccess);
		TestTrue(TEXT("context handle save returns true"), bSaveResult);
		TestTrue(TEXT("context handle save succeeds"), bSaveSuccess);
		TestFalse(TEXT("context handle writer has no error"), Writer.IsError());

		FGameplayEffectContextHandle DestinationHandle;
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FObjectAndNameAsStringProxyArchive LoadArchive(Reader, true);
		bool bLoadSuccess = false;
		const bool bLoadResult = DestinationHandle.NetSerialize(LoadArchive, nullptr, bLoadSuccess);
		TestTrue(TEXT("context handle load returns true"), bLoadResult);
		TestTrue(TEXT("context handle load succeeds"), bLoadSuccess);
		TestFalse(TEXT("context handle reader has no error"), Reader.IsError());

		const FGameplayEffectContext* LoadedBaseContext = DestinationHandle.Get();
		TestNotNull(TEXT("context handle produces a context"), LoadedBaseContext);
		if (LoadedBaseContext)
		{
			TestEqual(
				TEXT("context handle preserves the custom script struct"),
				LoadedBaseContext->GetScriptStruct(),
				FPlayerGamePlayEffectContext::StaticStruct());
			const FPlayerGamePlayEffectContext* LoadedPlayerContext =
				static_cast<const FPlayerGamePlayEffectContext*>(LoadedBaseContext);
			TestTrue(TEXT("context handle preserves critical hit"), LoadedPlayerContext->IsCriticalHit());
			TestTrue(TEXT("context handle preserves blocked hit"), LoadedPlayerContext->IsBlockedHit());
			TestEqual(TEXT("context handle preserves origin"), LoadedPlayerContext->GetOrigin(), MixedOrigin);
			TestNotNull(TEXT("context handle preserves hit result"), LoadedPlayerContext->GetHitResult());
		}
	}

	TUniquePtr<FGameplayEffectContext> Duplicate(MixedSource.Duplicate());
	const FPlayerGamePlayEffectContext* TypedDuplicate =
		static_cast<const FPlayerGamePlayEffectContext*>(Duplicate.Get());
	TestNotNull(TEXT("duplicate exists"), TypedDuplicate);
	if (TypedDuplicate)
	{
		TestTrue(TEXT("duplicate preserves critical hit"), TypedDuplicate->IsCriticalHit());
		TestTrue(TEXT("duplicate preserves blocked hit"), TypedDuplicate->IsBlockedHit());
		TestNotNull(TEXT("duplicate preserves hit result"), TypedDuplicate->GetHitResult());
		TestTrue(
			TEXT("duplicate owns a deep-copied hit result"),
			TypedDuplicate->GetHitResult() != MixedSource.GetHitResult());
	}

	return true;
}

#endif
