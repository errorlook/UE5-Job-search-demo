// Rylan

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "HeroUIInfo.generated.h"

class APlayerCharacter;
class UAnimMontage;

/**
 * UI data for one party member.
 */
USTRUCT(BlueprintType)
struct FHeroSlotInfo
{
	GENERATED_BODY()

	// Stable identity tag, such as Hero.Shinbi.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag HeroTag = FGameplayTag();

	// Display name.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText HeroName = FText();

	// Portrait texture.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UTexture2D> AvatarIcon = nullptr;

	// Optional transparent full-body art used by the party setup stage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UTexture2D> FullBodyArtwork = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UTexture2D> ElementIcon = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 DisplayLevel = 1;

	// Party-switch hotkey displayed next to the portrait.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText SwapHotkey = FText();

	// Playable pawn used when this party member becomes active.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftClassPtr<APlayerCharacter> CharacterClass;

	// Optional preview-only idle. It must match the playable mesh skeleton.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftObjectPtr<UAnimMontage> PreviewIdleMontage;
};

/**
 * Data asset describing the current party.
 */
UCLASS(BlueprintType)
class DEMO_API UHeroUIInfo : public UDataAsset
{
	GENERATED_BODY()
    
public:
	// Party members in display order.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero Information")
	TArray<FHeroSlotInfo> PartyMembers;
};

/** One of the four stable setup-page slots. */
USTRUCT(BlueprintType)
struct FPartySlotViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	bool bOccupied = false;

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	FHeroSlotInfo HeroInfo;
};
