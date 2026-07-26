#include "UI/Party/PartyPreviewStage.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "UI/Party/PartyPreviewCharacter.h"

namespace
{
	constexpr int32 PreviewSlotCount = 4;
	constexpr int32 PreviewWidth = 1280;
	constexpr int32 PreviewHeight = 720;
}

APartyPreviewStage::APartyPreviewStage()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetReplicateMovement(false);
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(
		TEXT("SceneCapture"));
	SceneCapture->SetupAttachment(SceneRoot);
	SceneCapture->SetRelativeLocation(FVector(-950.f, 0.f, 105.f));
	SceneCapture->SetRelativeRotation(FRotator::ZeroRotator);
	SceneCapture->FOVAngle = 38.f;
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneCapture->PrimitiveRenderMode =
		ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;
	SceneCapture->bAlwaysPersistRenderingState = true;
	SceneCapture->SetComponentTickEnabled(false);

	const float PreviewPointY[PreviewSlotCount] =
	{
		-255.f, -85.f, 85.f, 255.f
	};
	for (int32 SlotIndex = 0; SlotIndex < PreviewSlotCount; ++SlotIndex)
	{
		const FName PointName(*FString::Printf(
			TEXT("PreviewPoint%d"), SlotIndex + 1));
		USceneComponent* PreviewPoint =
			CreateDefaultSubobject<USceneComponent>(PointName);
		PreviewPoint->SetupAttachment(SceneRoot);
		PreviewPoint->SetRelativeLocation(
			FVector(0.f, PreviewPointY[SlotIndex], 90.f));
		PreviewPoint->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
		PreviewPoints.Add(PreviewPoint);
	}

	KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(SceneRoot);
	KeyLight->SetRelativeLocation(FVector(-250.f, -350.f, 350.f));
	KeyLight->SetIntensity(6500.f);
	KeyLight->SetAttenuationRadius(1500.f);
	KeyLight->SetLightColor(FLinearColor(1.f, 0.9f, 0.78f));

	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(SceneRoot);
	FillLight->SetRelativeLocation(FVector(-100.f, 420.f, 180.f));
	FillLight->SetIntensity(3500.f);
	FillLight->SetAttenuationRadius(1300.f);
	FillLight->SetLightColor(FLinearColor(0.55f, 0.72f, 1.f));

	PreviewCharacters.SetNum(PreviewSlotCount);
	PreviewHeroTags.SetNum(PreviewSlotCount);
}

bool APartyPreviewStage::InitializeStage()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	if (!RenderTarget)
	{
		RenderTarget = NewObject<UTextureRenderTarget2D>(this, NAME_None,
			RF_Transient);
		RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
		RenderTarget->ClearColor = FLinearColor::Transparent;
		RenderTarget->bAutoGenerateMips = false;
		RenderTarget->InitAutoFormat(PreviewWidth, PreviewHeight);
		RenderTarget->UpdateResourceImmediate(true);
	}

	SceneCapture->TextureTarget = RenderTarget;
	SetCaptureEnabled(true);
	UE_LOG(LogTemp, Log,
		TEXT("Party preview stage initialized with %dx%d RenderTarget."),
		PreviewWidth, PreviewHeight);
	return true;
}

void APartyPreviewStage::RefreshParty(
	const TArray<FPartySlotViewData>& PartySlots)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	bool bChanged = false;
	for (int32 SlotIndex = 0; SlotIndex < PreviewSlotCount; ++SlotIndex)
	{
		const FPartySlotViewData* SlotData = PartySlots.IsValidIndex(SlotIndex)
			? &PartySlots[SlotIndex]
			: nullptr;
		const FGameplayTag NewHeroTag = SlotData && SlotData->bOccupied
			? SlotData->HeroInfo.HeroTag
			: FGameplayTag();
		if (PreviewHeroTags[SlotIndex] == NewHeroTag)
		{
			continue;
		}

		bChanged = true;
		PreviewHeroTags[SlotIndex] = NewHeroTag;
		if (APartyPreviewCharacter* ExistingCharacter =
			PreviewCharacters[SlotIndex])
		{
			ExistingCharacter->Destroy();
			PreviewCharacters[SlotIndex] = nullptr;
		}

		if (!SlotData || !SlotData->bOccupied ||
			!PreviewPoints.IsValidIndex(SlotIndex))
		{
			continue;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APartyPreviewCharacter* PreviewCharacter =
			World->SpawnActor<APartyPreviewCharacter>(
				APartyPreviewCharacter::StaticClass(),
				PreviewPoints[SlotIndex]->GetComponentTransform(),
				SpawnParameters);
		if (!PreviewCharacter ||
			!PreviewCharacter->ConfigureFromHeroInfo(SlotData->HeroInfo))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Party preview could not load a mesh for hero %s."),
				*SlotData->HeroInfo.HeroTag.ToString());
			if (PreviewCharacter)
			{
				PreviewCharacter->Destroy();
			}
			PreviewHeroTags[SlotIndex] = FGameplayTag();
			continue;
		}

		PreviewCharacters[SlotIndex] = PreviewCharacter;
		UE_LOG(LogTemp, Log,
			TEXT("Party preview spawned slot %d for hero %s."),
			SlotIndex, *NewHeroTag.ToString());
	}

	if (bChanged)
	{
		RebuildShowOnlyList();
		if (bCaptureEnabled)
		{
			SceneCapture->CaptureSceneDeferred();
		}
	}
}

void APartyPreviewStage::SetCaptureEnabled(bool bEnabled)
{
	bCaptureEnabled = bEnabled && RenderTarget != nullptr;
	SceneCapture->bCaptureEveryFrame = bCaptureEnabled;
	SceneCapture->SetComponentTickEnabled(bCaptureEnabled);
	if (bCaptureEnabled)
	{
		SceneCapture->CaptureSceneDeferred();
	}
}

void APartyPreviewStage::Shutdown()
{
	bCaptureEnabled = false;
	if (SceneCapture)
	{
		SceneCapture->bCaptureEveryFrame = false;
		SceneCapture->SetComponentTickEnabled(false);
		SceneCapture->TextureTarget = nullptr;
		SceneCapture->ClearShowOnlyComponents();
	}

	for (APartyPreviewCharacter* PreviewCharacter : PreviewCharacters)
	{
		if (PreviewCharacter)
		{
			PreviewCharacter->Destroy();
		}
	}
	PreviewCharacters.SetNumZeroed(PreviewSlotCount);
	PreviewHeroTags.SetNum(PreviewSlotCount);
	for (FGameplayTag& HeroTag : PreviewHeroTags)
	{
		HeroTag = FGameplayTag();
	}
	RenderTarget = nullptr;
}

void APartyPreviewStage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Shutdown();
	Super::EndPlay(EndPlayReason);
}

void APartyPreviewStage::RebuildShowOnlyList()
{
	SceneCapture->ClearShowOnlyComponents();
	for (APartyPreviewCharacter* PreviewCharacter : PreviewCharacters)
	{
		if (PreviewCharacter)
		{
			SceneCapture->ShowOnlyActorComponents(PreviewCharacter);
		}
	}
}
