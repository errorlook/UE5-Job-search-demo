#include "UI/Party/PartyPreviewCharacter.h"

#include "AbilitySystem/Data/HeroUIInfo.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/PlayerCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

APartyPreviewCharacter::APartyPreviewCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetReplicateMovement(false);
	SetActorEnableCollision(false);
	SetCanBeDamaged(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PreviewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(
		TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(SceneRoot);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetGenerateOverlapEvents(false);
	PreviewMesh->SetCanEverAffectNavigation(false);
	PreviewMesh->SetReceivesDecals(false);
	PreviewMesh->SetVisibleInSceneCaptureOnly(true);
	PreviewMesh->VisibilityBasedAnimTickOption =
		EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	PreviewMesh->bEnableUpdateRateOptimizations = false;
}

bool APartyPreviewCharacter::ConfigureFromHeroInfo(
	const FHeroSlotInfo& HeroInfo)
{
	const TSubclassOf<APlayerCharacter> LoadedClass =
		HeroInfo.CharacterClass.LoadSynchronous();
	const APlayerCharacter* CharacterDefaults = LoadedClass
		? LoadedClass->GetDefaultObject<APlayerCharacter>()
		: nullptr;
	USkeletalMeshComponent* SourceMesh = CharacterDefaults
		? CharacterDefaults->GetMesh()
		: nullptr;
	if (!SourceMesh || !SourceMesh->GetSkeletalMeshAsset())
	{
		return false;
	}

	PreviewMesh->SetSkeletalMeshAsset(SourceMesh->GetSkeletalMeshAsset());
	PreviewMesh->SetRelativeTransform(SourceMesh->GetRelativeTransform());
	PreviewMesh->SetVisibility(true);
	PreviewMesh->SetHiddenInGame(false);

	for (int32 MaterialIndex = 0;
		MaterialIndex < SourceMesh->GetNumMaterials(); ++MaterialIndex)
	{
		PreviewMesh->SetMaterial(
			MaterialIndex, SourceMesh->GetMaterial(MaterialIndex));
	}

	if (UClass* AnimClass = SourceMesh->GetAnimClass())
	{
		PreviewMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		PreviewMesh->SetAnimInstanceClass(AnimClass);
	}
	else
	{
		PreviewMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	}

	if (!HeroInfo.PreviewIdleMontage.IsNull())
	{
		UAnimMontage* IdleMontage =
			HeroInfo.PreviewIdleMontage.LoadSynchronous();
		const USkeleton* MeshSkeleton =
			PreviewMesh->GetSkeletalMeshAsset()->GetSkeleton();
		if (!IdleMontage || IdleMontage->GetSkeleton() != MeshSkeleton)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Preview idle Montage is missing or has the wrong skeleton for %s."),
				*HeroInfo.HeroTag.ToString());
		}
		else if (UAnimInstance* AnimInstance = PreviewMesh->GetAnimInstance())
		{
			if (AnimInstance->Montage_Play(IdleMontage) > 0.f &&
				IdleMontage->GetNumSections() > 0)
			{
				const FName IdleSection = IdleMontage->GetSectionName(0);
				AnimInstance->Montage_SetNextSection(
					IdleSection, IdleSection, IdleMontage);
			}
		}

	}

	return true;
}
