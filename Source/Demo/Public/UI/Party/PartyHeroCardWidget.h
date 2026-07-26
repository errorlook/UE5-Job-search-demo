#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/HeroUIInfo.h"
#include "UI/Widget/PlayerUserWidget.h"
#include "PartyHeroCardWidget.generated.h"

class UButton;
class UBorder;
class UImage;
class URichTextBlock;
class UPartyComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FPartyHeroSelectedSignature, FGameplayTag, HeroTag);

/** Selectable roster card. It reports intent and never mutates the party itself. */
UCLASS(Abstract, Blueprintable)
class DEMO_API UPartyHeroCardWidget : public UPlayerUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Party|Setup")
	void ApplyHeroData(
		const FHeroSlotInfo& InHeroInfo, UPartyComponent* InPartyComponent);

	void ApplyPreviewHeroData(
		const FHeroSlotInfo& InHeroInfo, bool bInPendingParty);

	void ApplySelectionData(
		const FHeroSlotInfo& InHeroInfo,
		int32 InPendingPartySlotIndex,
		bool bInCandidateSelected);

	UPROPERTY(BlueprintAssignable, Category = "Party|Setup")
	FPartyHeroSelectedSignature OnHeroSelected;

	// Kept expose-on-spawn for compatibility with the existing WBP_CharacterSelect
	// Create Widget nodes. Native setup still calls ApplyHeroData after creation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party|Setup", meta = (ExposeOnSpawn = "true"))
	FHeroSlotInfo HeroInfo;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> HeroButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> HeroAvatarImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URichTextBlock> HeroNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URichTextBlock> PartyStateText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PartyStateBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> HoverFrame;

private:
	UFUNCTION()
	void HandleHeroClicked();

	void RefreshPartyState(int32 PendingPartySlotIndex);
	void SetCandidateSelected(bool bInCandidateSelected);

	UPROPERTY(Transient)
	TObjectPtr<UPartyComponent> PartyComponent;
};
