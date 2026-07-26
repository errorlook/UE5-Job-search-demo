#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/HeroUIInfo.h"
#include "GameFramework/Actor.h"
#include "PartyPreviewStage.generated.h"

class APartyPreviewCharacter;
class UPointLightComponent;
class USceneCaptureComponent2D;
class USceneComponent;
class UTextureRenderTarget2D;

/** One shared capture stage for all four pending party members. */
UCLASS(NotBlueprintable, Transient)
class DEMO_API APartyPreviewStage : public AActor
{
	GENERATED_BODY()

public:
	APartyPreviewStage();

	bool InitializeStage();
	void RefreshParty(const TArray<FPartySlotViewData>& PartySlots);
	void SetCaptureEnabled(bool bEnabled);
	void Shutdown();

	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RebuildShowOnlyList();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<USceneComponent>> PreviewPoints;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY(Transient)
	TArray<TObjectPtr<APartyPreviewCharacter>> PreviewCharacters;

	TArray<FGameplayTag> PreviewHeroTags;
	bool bCaptureEnabled = false;
};
