#pragma once

#include "CoreMinimal.h"
#include "Components/PartyComponent.h"
#include "UI/Widget/PlayerUserWidget.h"
#include "PartySetupWidget.generated.h"

class UPanelWidget;
class UButton;
class UImage;
class UPartyHeroCardWidget;
class UPartySetupSlotWidget;
class UTextBlock;
class UTextureRenderTarget2D;
class UWidgetSwitcher;
class APartyPreviewStage;

UENUM(BlueprintType)
enum class EPartySetupView : uint8
{
	PartyOverview,
	HeroSelection
};

/** Native controller for the Genshin-style party setup page. */
UCLASS(Abstract, Blueprintable)
class DEMO_API UPartySetupWidget : public UPlayerUserWidget
{
	GENERATED_BODY()

public:
	// Used by the controller's ESC/toggle path. Selection returns to overview.
	void RequestCancel();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeWidgetControllerSet() override;
	virtual bool ShouldRunBlueprintWidgetControllerSet() const override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(
		const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ActivePartyBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> PartyPreviewImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> RosterBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CancelButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> PartyViewSwitcher;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SelectionActionButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SelectionActionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SelectionSubtitle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party|Setup")
	TSubclassOf<UPartySetupSlotWidget> PartySlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Party|Setup")
	TSubclassOf<UPartyHeroCardWidget> HeroCardWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "Party|Setup")
	int32 SelectedSlotIndex = 0;

private:
	UFUNCTION()
	void HandleNativePartyChanged();

	UFUNCTION()
	void HandleNativeRosterChanged();

	UFUNCTION()
	void HandleSlotSelected(int32 SlotIndex);

	UFUNCTION()
	void HandleHeroSelected(FGameplayTag HeroTag);

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleSelectionActionClicked();

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleApplyResult(EPartySetupApplyResult Result);

	void InitializePendingState();
	void UpdatePendingActiveSlot(int32 PreferredFallbackSlot);
	void UpdateDirtyState();
	void SetCommitControlsEnabled(bool bEnabled);
	void ShowPartyOverview();
	void ShowHeroSelection(int32 SlotIndex);
	void RefreshSelectionActionState();
	void RebuildPartySlots();
	void RebuildRoster();
	void RefreshSlotSelection();
	void ApplyPageStyle();
	void UnbindPartyEvents();
	void UnbindControllerEvents();
	void InitializePartyPreview();
	void RefreshPartyPreview();
	void ShutdownPartyPreview();

	UPROPERTY(Transient)
	TObjectPtr<UPartyComponent> PartyComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPartySetupSlotWidget>> PartySlotWidgets;

	UPROPERTY(Transient)
	TObjectPtr<APartyPreviewStage> PartyPreviewStage;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PartyPreviewRenderTarget;

	TArray<FGameplayTag> OriginalParty;
	TArray<FGameplayTag> PendingParty;
	int32 OriginalActiveSlotIndex = INDEX_NONE;
	int32 PendingActiveSlotIndex = INDEX_NONE;
	int32 EditingSlotIndex = INDEX_NONE;
	FGameplayTag OriginalActiveHeroTag;
	FGameplayTag CandidateHeroTag;
	EPartySetupView CurrentView = EPartySetupView::PartyOverview;
	bool bPartyDirty = false;
	bool bCommitInProgress = false;
};
