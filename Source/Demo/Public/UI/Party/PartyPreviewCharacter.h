#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PartyPreviewCharacter.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
struct FHeroSlotInfo;

/** Lightweight visual-only copy of a playable character's mesh setup. */
UCLASS(NotBlueprintable, Transient)
class DEMO_API APartyPreviewCharacter : public AActor
{
	GENERATED_BODY()

public:
	APartyPreviewCharacter();

	bool ConfigureFromHeroInfo(const FHeroSlotInfo& HeroInfo);

	USkeletalMeshComponent* GetPreviewMesh() const { return PreviewMesh; }

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> PreviewMesh;
};
