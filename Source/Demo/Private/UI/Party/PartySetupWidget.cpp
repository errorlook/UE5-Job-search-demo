#include "UI/Party/PartySetupWidget.h"

#include "Components/Button.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/RichTextBlock.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/PartyComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "UI/Party/PartyHeroCardWidget.h"
#include "UI/Party/PartyPreviewStage.h"
#include "UI/Party/PartySetupSlotWidget.h"
#include "Player/OnePlayerController.h"

void UPartySetupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddUniqueDynamic(
			this, &UPartySetupWidget::HandleConfirmClicked);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s has no optional ConfirmButton; add it in the Widget Blueprint."),
			*GetName());
	}

	if (CancelButton)
	{
		CancelButton->OnClicked.AddUniqueDynamic(
			this, &UPartySetupWidget::HandleCancelClicked);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("%s has no optional CancelButton; the existing close button remains available."),
			*GetName());
	}

	if (close)
	{
		close->OnClicked.Clear();
		close->OnClicked.AddUniqueDynamic(
			this, &UPartySetupWidget::HandleCancelClicked);
	}

	if (BackButton)
	{
		BackButton->OnClicked.AddUniqueDynamic(
			this, &UPartySetupWidget::HandleBackClicked);
	}

	if (SelectionActionButton)
	{
		SelectionActionButton->OnClicked.AddUniqueDynamic(
			this, &UPartySetupWidget::HandleSelectionActionClicked);
	}
}

bool UPartySetupWidget::ShouldRunBlueprintWidgetControllerSet() const
{
	// The legacy Blueprint graph rebuilds only occupied entries and binds old
	// Add/Remove semantics. Native code owns the page data flow now.
	return false;
}

void UPartySetupWidget::NativeWidgetControllerSet()
{
	ApplyPageStyle();
	UnbindPartyEvents();
	UnbindControllerEvents();
	ShutdownPartyPreview();
	PartyComponent = Cast<UPartyComponent>(WidgetController);
	if (!PartyComponent) return;

	PartyComponent->OnPartyChanged.AddUniqueDynamic(
		this, &UPartySetupWidget::HandleNativePartyChanged);
	PartyComponent->OnRosterChanged.AddUniqueDynamic(
		this, &UPartySetupWidget::HandleNativeRosterChanged);
	if (AOnePlayerController* PlayerController =
		Cast<AOnePlayerController>(GetOwningPlayer()))
	{
		PlayerController->OnPartySetupApplyResult.AddUniqueDynamic(
			this, &UPartySetupWidget::HandleApplyResult);
	}

	InitializePendingState();
	InitializePartyPreview();
	RebuildPartySlots();
	RefreshPartyPreview();
	ShowPartyOverview();
}

void UPartySetupWidget::ApplyPageStyle()
{
	if (!WidgetTree) return;

	// The Designer background is intentionally texture-agnostic.  Tinting it
	// here keeps the world visible while giving all party controls contrast.
	if (UImage* Background = Cast<UImage>(WidgetTree->FindWidget(TEXT("Image"))))
	{
		Background->SetColorAndOpacity(FLinearColor(0.025f, 0.04f, 0.075f, 0.82f));
	}

	TArray<UWidget*> PageWidgets;
	WidgetTree->GetAllWidgets(PageWidgets);
	for (UWidget* Widget : PageWidgets)
	{
		if (URichTextBlock* Text = Cast<URichTextBlock>(Widget))
		{
			Text->SetDefaultColorAndOpacity(FSlateColor(FLinearColor::White));
		}
	}
}

void UPartySetupWidget::NativeDestruct()
{
	ShutdownPartyPreview();
	UnbindPartyEvents();
	UnbindControllerEvents();
	OriginalParty.Reset();
	PendingParty.Reset();
	EditingSlotIndex = INDEX_NONE;
	CandidateHeroTag = FGameplayTag();
	CurrentView = EPartySetupView::PartyOverview;
	bPartyDirty = false;
	bCommitInProgress = false;
	Super::NativeDestruct();
}

void UPartySetupWidget::UnbindPartyEvents()
{
	if (PartyComponent)
	{
		PartyComponent->OnPartyChanged.RemoveDynamic(
			this, &UPartySetupWidget::HandleNativePartyChanged);
		PartyComponent->OnRosterChanged.RemoveDynamic(
			this, &UPartySetupWidget::HandleNativeRosterChanged);
	}
	PartyComponent = nullptr;
}

void UPartySetupWidget::UnbindControllerEvents()
{
	if (AOnePlayerController* PlayerController =
		Cast<AOnePlayerController>(GetOwningPlayer()))
	{
		PlayerController->OnPartySetupApplyResult.RemoveDynamic(
			this, &UPartySetupWidget::HandleApplyResult);
	}
}

void UPartySetupWidget::HandleNativePartyChanged()
{
	if (bCommitInProgress || bPartyDirty) return;
	InitializePendingState();
	RebuildPartySlots();
	RefreshPartyPreview();
	ShowPartyOverview();
}

void UPartySetupWidget::HandleNativeRosterChanged()
{
	if (CurrentView == EPartySetupView::HeroSelection)
	{
		RebuildRoster();
	}
}

void UPartySetupWidget::HandleSlotSelected(int32 SlotIndex)
{
	if (bCommitInProgress || !PendingParty.IsValidIndex(SlotIndex)) return;

	SelectedSlotIndex = SlotIndex;
	RefreshSlotSelection();
	ShowHeroSelection(SlotIndex);
}

void UPartySetupWidget::HandleHeroSelected(FGameplayTag HeroTag)
{
	if (bCommitInProgress || CurrentView != EPartySetupView::HeroSelection ||
		!HeroTag.IsValid() || !PendingParty.IsValidIndex(EditingSlotIndex))
	{
		return;
	}

	CandidateHeroTag = HeroTag;
	RebuildRoster();
	RefreshSelectionActionState();
}

void UPartySetupWidget::HandleBackClicked()
{
	if (bCommitInProgress || CurrentView != EPartySetupView::HeroSelection)
	{
		return;
	}

	ShowPartyOverview();
}

void UPartySetupWidget::HandleSelectionActionClicked()
{
	if (bCommitInProgress || CurrentView != EPartySetupView::HeroSelection ||
		!PendingParty.IsValidIndex(EditingSlotIndex) ||
		!CandidateHeroTag.IsValid())
	{
		return;
	}

	const int32 CandidateSlotIndex =
		PendingParty.IndexOfByKey(CandidateHeroTag);
	if (CandidateSlotIndex == EditingSlotIndex) return;

	const int32 CompletedSlotIndex = EditingSlotIndex;
	if (CandidateSlotIndex != INDEX_NONE)
	{
		PendingParty.Swap(EditingSlotIndex, CandidateSlotIndex);
	}
	else
	{
		PendingParty[EditingSlotIndex] = CandidateHeroTag;
	}

	SelectedSlotIndex = CompletedSlotIndex;
	UpdatePendingActiveSlot(CompletedSlotIndex);
	UpdateDirtyState();
	RebuildPartySlots();
	RefreshPartyPreview();
	ShowPartyOverview();
}

void UPartySetupWidget::InitializePendingState()
{
	if (!PartyComponent) return;

	OriginalParty.Reset();
	for (const FPartySlotViewData& PartySlotData :
		PartyComponent->GetPartySlots())
	{
		OriginalParty.Add(PartySlotData.bOccupied
			? PartySlotData.HeroInfo.HeroTag
			: FGameplayTag());
	}
	PendingParty = OriginalParty;
	OriginalActiveSlotIndex = PartyComponent->GetActiveSlotIndex();
	PendingActiveSlotIndex = OriginalActiveSlotIndex;
	OriginalActiveHeroTag = PartyComponent->GetActiveHeroTag();
	bPartyDirty = false;
	bCommitInProgress = false;
	EditingSlotIndex = INDEX_NONE;
	CandidateHeroTag = FGameplayTag();
	CurrentView = EPartySetupView::PartyOverview;
	SetCommitControlsEnabled(true);
}

void UPartySetupWidget::UpdatePendingActiveSlot(int32 PreferredFallbackSlot)
{
	const int32 OriginalHeroSlot =
		PendingParty.IndexOfByKey(OriginalActiveHeroTag);
	if (OriginalHeroSlot != INDEX_NONE)
	{
		PendingActiveSlotIndex = OriginalHeroSlot;
		return;
	}

	if (PendingParty.IsValidIndex(PreferredFallbackSlot) &&
		PendingParty[PreferredFallbackSlot].IsValid())
	{
		PendingActiveSlotIndex = PreferredFallbackSlot;
		return;
	}

	PendingActiveSlotIndex = PendingParty.IndexOfByPredicate(
		[](const FGameplayTag HeroTag) { return HeroTag.IsValid(); });
}

void UPartySetupWidget::UpdateDirtyState()
{
	bPartyDirty = PendingParty != OriginalParty ||
		PendingActiveSlotIndex != OriginalActiveSlotIndex;
}

void UPartySetupWidget::HandleConfirmClicked()
{
	if (bCommitInProgress || CurrentView != EPartySetupView::PartyOverview ||
		!PartyComponent)
	{
		return;
	}

	AOnePlayerController* PlayerController =
		Cast<AOnePlayerController>(GetOwningPlayer());
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Error,
			TEXT("%s cannot confirm without AOnePlayerController."), *GetName());
		return;
	}

	bCommitInProgress = true;
	SetCommitControlsEnabled(false);
	PlayerController->RequestApplyPartySetup(
		PendingParty, PendingActiveSlotIndex);
}

void UPartySetupWidget::HandleApplyResult(EPartySetupApplyResult Result)
{
	if (!bCommitInProgress) return;

	bCommitInProgress = false;
	if (Result == EPartySetupApplyResult::Success)
	{
		OriginalParty = PendingParty;
		OriginalActiveSlotIndex = PendingActiveSlotIndex;
		OriginalActiveHeroTag = PendingParty.IsValidIndex(
			PendingActiveSlotIndex)
			? PendingParty[PendingActiveSlotIndex]
			: FGameplayTag();
		bPartyDirty = false;
		ShutdownPartyPreview();
		if (AOnePlayerController* PlayerController =
			Cast<AOnePlayerController>(GetOwningPlayer()))
		{
			PlayerController->CloseManagedMenu(this);
		}
		return;
	}

	SetCommitControlsEnabled(true);
	const UEnum* ResultEnum = StaticEnum<EPartySetupApplyResult>();
	UE_LOG(LogTemp, Error, TEXT("Party setup commit failed: %s"),
		ResultEnum
			? *ResultEnum->GetNameStringByValue(static_cast<int64>(Result))
			: TEXT("Unknown"));
}

void UPartySetupWidget::RequestCancel()
{
	if (bCommitInProgress) return;

	if (CurrentView == EPartySetupView::HeroSelection)
	{
		HandleBackClicked();
		return;
	}

	HandleCancelClicked();
}

void UPartySetupWidget::HandleCancelClicked()
{
	if (bCommitInProgress)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Party setup cannot close while a commit is in progress."));
		return;
	}

	PendingParty = OriginalParty;
	PendingActiveSlotIndex = OriginalActiveSlotIndex;
	bPartyDirty = false;
	ShutdownPartyPreview();
	if (AOnePlayerController* PlayerController =
		Cast<AOnePlayerController>(GetOwningPlayer()))
	{
		PlayerController->CloseManagedMenu(this);
	}
}

void UPartySetupWidget::SetCommitControlsEnabled(bool bEnabled)
{
	if (ConfirmButton) ConfirmButton->SetIsEnabled(bEnabled);
	if (CancelButton) CancelButton->SetIsEnabled(bEnabled);
	if (close) close->SetIsEnabled(bEnabled);
	if (BackButton) BackButton->SetIsEnabled(bEnabled);
	if (!bEnabled && SelectionActionButton)
	{
		SelectionActionButton->SetIsEnabled(false);
	}
	else if (bEnabled)
	{
		RefreshSelectionActionState();
	}
}

void UPartySetupWidget::ShowPartyOverview()
{
	CurrentView = EPartySetupView::PartyOverview;
	EditingSlotIndex = INDEX_NONE;
	CandidateHeroTag = FGameplayTag();
	if (PartyViewSwitcher)
	{
		PartyViewSwitcher->SetActiveWidgetIndex(0);
	}
	if (PartyPreviewStage)
	{
		PartyPreviewStage->SetCaptureEnabled(true);
	}
}

void UPartySetupWidget::ShowHeroSelection(int32 SlotIndex)
{
	if (!PendingParty.IsValidIndex(SlotIndex)) return;

	EditingSlotIndex = SlotIndex;
	CandidateHeroTag = PendingParty[SlotIndex];
	CurrentView = EPartySetupView::HeroSelection;
	if (PartyViewSwitcher)
	{
		PartyViewSwitcher->SetActiveWidgetIndex(1);
	}
	if (PartyPreviewStage)
	{
		PartyPreviewStage->SetCaptureEnabled(false);
	}

	if (SelectionSubtitle)
	{
		FNumberFormattingOptions NumberOptions;
		NumberOptions.MinimumIntegralDigits = 2;
		NumberOptions.MaximumIntegralDigits = 2;
		SelectionSubtitle->SetText(FText::Format(
			NSLOCTEXT("Party", "SelectionSubtitle",
				"\u4e3a\u4f4d\u7f6e {0} \u9009\u62e9\u89d2\u8272"),
			FText::AsNumber(SlotIndex + 1, &NumberOptions)));
	}

	RebuildRoster();
	RefreshSelectionActionState();
}

void UPartySetupWidget::RefreshSelectionActionState()
{
	if (!SelectionActionButton) return;

	FText ActionText = NSLOCTEXT(
		"Party", "SelectionActionChoose", "\u9009\u62e9\u89d2\u8272");
	bool bEnabled = false;
	if (CurrentView == EPartySetupView::HeroSelection &&
		PendingParty.IsValidIndex(EditingSlotIndex) &&
		CandidateHeroTag.IsValid())
	{
		const FGameplayTag CurrentHeroTag = PendingParty[EditingSlotIndex];
		const int32 CandidateSlotIndex =
			PendingParty.IndexOfByKey(CandidateHeroTag);
		if (CurrentHeroTag == CandidateHeroTag)
		{
			ActionText = NSLOCTEXT(
				"Party", "SelectionActionCurrent", "\u5f53\u524d\u89d2\u8272");
		}
		else if (CandidateSlotIndex != INDEX_NONE)
		{
			ActionText = NSLOCTEXT(
				"Party", "SelectionActionSwap", "\u4ea4\u6362");
			bEnabled = true;
		}
		else if (CurrentHeroTag.IsValid())
		{
			ActionText = NSLOCTEXT(
				"Party", "SelectionActionReplace", "\u66f4\u6362");
			bEnabled = true;
		}
		else
		{
			ActionText = NSLOCTEXT(
				"Party", "SelectionActionJoin", "\u52a0\u5165");
			bEnabled = true;
		}
	}

	if (SelectionActionText) SelectionActionText->SetText(ActionText);
	SelectionActionButton->SetIsEnabled(bEnabled && !bCommitInProgress);
}

void UPartySetupWidget::RebuildPartySlots()
{
	if (!PartyComponent || !ActivePartyBox || !PartySlotWidgetClass) return;

	ActivePartyBox->ClearChildren();
	PartySlotWidgets.Reset();
	if (!PendingParty.IsValidIndex(SelectedSlotIndex)) SelectedSlotIndex = 0;

	for (int32 SlotIndex = 0; SlotIndex < PendingParty.Num(); ++SlotIndex)
	{
		FPartySlotViewData SlotData;
		SlotData.SlotIndex = SlotIndex;
		SlotData.bOccupied = PartyComponent->FindHeroInfo(
			PendingParty[SlotIndex], SlotData.HeroInfo);
		UPartySetupSlotWidget* SlotWidget =
			CreateWidget<UPartySetupSlotWidget>(
				GetOwningPlayer(), PartySlotWidgetClass);
		if (!SlotWidget) continue;

		SlotWidget->ApplySlotData(SlotData);
		SlotWidget->SetActive(SlotIndex == PendingActiveSlotIndex);
		SlotWidget->OnPartySlotSelected.AddUniqueDynamic(
			this, &UPartySetupWidget::HandleSlotSelected);
		UPanelSlot* AddedSlot = ActivePartyBox->AddChild(SlotWidget);
		if (UHorizontalBoxSlot* HorizontalSlot =
			Cast<UHorizontalBoxSlot>(AddedSlot))
		{
			HorizontalSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			HorizontalSlot->SetHorizontalAlignment(HAlign_Fill);
			HorizontalSlot->SetVerticalAlignment(VAlign_Fill);
			HorizontalSlot->SetPadding(FMargin(12.f, 0.f));
		}
		else if (UVerticalBoxSlot* VerticalSlot =
			Cast<UVerticalBoxSlot>(AddedSlot))
		{
			VerticalSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			VerticalSlot->SetHorizontalAlignment(HAlign_Fill);
			VerticalSlot->SetVerticalAlignment(VAlign_Fill);
			VerticalSlot->SetPadding(FMargin(0.f, 4.f));
		}
		PartySlotWidgets.Add(SlotWidget);
	}
	RefreshSlotSelection();
}

void UPartySetupWidget::RebuildRoster()
{
	if (!PartyComponent || !RosterBox || !HeroCardWidgetClass) return;

	RosterBox->ClearChildren();
	for (const FHeroSlotInfo& HeroInfo :
		PartyComponent->GetUnlockedHeroInfo())
	{
		UPartyHeroCardWidget* HeroCard =
			CreateWidget<UPartyHeroCardWidget>(
				GetOwningPlayer(), HeroCardWidgetClass);
		if (!HeroCard) continue;

		HeroCard->ApplySelectionData(
			HeroInfo,
			PendingParty.IndexOfByKey(HeroInfo.HeroTag),
			HeroInfo.HeroTag == CandidateHeroTag);
		HeroCard->OnHeroSelected.AddUniqueDynamic(
			this, &UPartySetupWidget::HandleHeroSelected);
		// Roster cards use the same full-screen Designer preview, so constrain
		// their desired size before adding them to the wrap/grid container.
		USizeBox* CardSizeBox = NewObject<USizeBox>(this);
		CardSizeBox->SetWidthOverride(150.f);
		CardSizeBox->SetHeightOverride(180.f);
		CardSizeBox->SetContent(HeroCard);
		RosterBox->AddChild(CardSizeBox);
	}
}

FReply UPartySetupWidget::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		RequestCancel();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UPartySetupWidget::RefreshSlotSelection()
{
	for (UPartySetupSlotWidget* SlotWidget : PartySlotWidgets)
	{
		if (SlotWidget)
		{
			SlotWidget->SetSelected(
				SlotWidget->GetSlotIndex() == SelectedSlotIndex);
		}
	}
}

void UPartySetupWidget::InitializePartyPreview()
{
	if (!PartyPreviewImage || !PartyComponent)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Party preview skipped: image=%s, party component=%s."),
			PartyPreviewImage ? TEXT("bound") : TEXT("missing"),
			PartyComponent ? TEXT("valid") : TEXT("missing"));
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* OwningPlayer = GetOwningPlayer();
	if (!World || World->GetNetMode() == NM_DedicatedServer ||
		!OwningPlayer || !OwningPlayer->IsLocalController())
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = OwningPlayer;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PartyPreviewStage = World->SpawnActor<APartyPreviewStage>(
		APartyPreviewStage::StaticClass(),
		FTransform(FVector(0.f, 0.f, 100000.f)), SpawnParameters);
	if (!PartyPreviewStage || !PartyPreviewStage->InitializeStage())
	{
		ShutdownPartyPreview();
		return;
	}

	PartyPreviewRenderTarget = PartyPreviewStage->GetRenderTarget();
	PartyPreviewImage->SetBrushResourceObject(PartyPreviewRenderTarget.Get());
	PartyPreviewImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PartyPreviewImage->SynchronizeProperties();
	UE_LOG(LogTemp, Log, TEXT("Party preview RenderTarget bound to image."));
}

void UPartySetupWidget::RefreshPartyPreview()
{
	if (!PartyPreviewStage || !PartyComponent)
	{
		return;
	}

	TArray<FPartySlotViewData> PreviewSlots;
	PreviewSlots.Reserve(PendingParty.Num());
	for (int32 SlotIndex = 0; SlotIndex < PendingParty.Num(); ++SlotIndex)
	{
		FPartySlotViewData& SlotData = PreviewSlots.AddDefaulted_GetRef();
		SlotData.SlotIndex = SlotIndex;
		SlotData.bOccupied = PartyComponent->FindHeroInfo(
			PendingParty[SlotIndex], SlotData.HeroInfo);
	}
	PartyPreviewStage->RefreshParty(PreviewSlots);
}

void UPartySetupWidget::ShutdownPartyPreview()
{
	if (PartyPreviewImage)
	{
		PartyPreviewImage->SetBrushResourceObject(nullptr);
		PartyPreviewImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	PartyPreviewRenderTarget = nullptr;

	if (PartyPreviewStage)
	{
		PartyPreviewStage->Shutdown();
		PartyPreviewStage->Destroy();
		PartyPreviewStage = nullptr;
	}
}
