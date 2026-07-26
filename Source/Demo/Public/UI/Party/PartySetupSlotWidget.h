#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/HeroUIInfo.h"
#include "UI/Widget/PlayerUserWidget.h"
#include "PartySetupSlotWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class URichTextBlock;
class UTextBlock;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FPartySetupSlotSelectedSignature, int32, SlotIndex);

/** Native behavior for one fixed party-setup slot. Blueprint owns visuals. */
UCLASS(Abstract, Blueprintable)
class DEMO_API UPartySetupSlotWidget : public UPlayerUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Party|Setup")
	void ApplySlotData(const FPartySlotViewData& InSlotData);

	UFUNCTION(BlueprintCallable, Category = "Party|Setup")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintCallable, Category = "Party|Setup")
	void SetActive(bool bInActive);

	UFUNCTION(BlueprintPure, Category = "Party|Setup")
	int32 GetSlotIndex() const { return SlotData.SlotIndex; }

	UPROPERTY(BlueprintAssignable, Category = "Party|Setup")
	FPartySetupSlotSelectedSignature OnPartySlotSelected;

	UPROPERTY(BlueprintReadOnly, Category = "Party|Setup")
	FPartySlotViewData SlotData;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SlotButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SlotBackground;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> FullBodyImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ElementIconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URichTextBlock> EmptyPlusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URichTextBlock> HeroNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URichTextBlock> HeroLevelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HeroInfoPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SelectedFrame;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SlotNumberText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ActiveBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ActiveBadgeText;

private:
	UFUNCTION()
	void HandleSlotClicked();

	void RefreshPresentation();
	bool bIsActive = false;
};
