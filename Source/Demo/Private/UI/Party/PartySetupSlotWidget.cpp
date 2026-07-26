#include "UI/Party/PartySetupSlotWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"

void UPartySetupSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (SlotButton)
	{
		SlotButton->SetBackgroundColor(FLinearColor::Transparent);
		SlotButton->SetVisibility(ESlateVisibility::Visible);
		SlotButton->SetIsEnabled(true);
		if (UOverlaySlot* ButtonSlot = Cast<UOverlaySlot>(SlotButton->Slot))
		{
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
			ButtonSlot->SetVerticalAlignment(VAlign_Fill);
			ButtonSlot->SetPadding(FMargin(0.f));
		}
		// Replace legacy Blueprint click bindings. The page owns party mutation.
		SlotButton->OnClicked.Clear();
		SlotButton->OnClicked.AddUniqueDynamic(
			this, &UPartySetupSlotWidget::HandleSlotClicked);
	}

	if (SlotBackground)
	{
		SlotBackground->SetBrushColor(
			FLinearColor(0.03f, 0.04f, 0.05f, 0.12f));
		if (UOverlaySlot* BackgroundSlot = Cast<UOverlaySlot>(SlotBackground->Slot))
		{
			BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
			BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	if (FullBodyImage)
	{
		FullBodyImage->SetBrushResourceObject(nullptr);
		FullBodyImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (EmptyPlusText)
	{
		EmptyPlusText->SetDefaultColorAndOpacity(FSlateColor(FLinearColor::White));
		if (UOverlaySlot* PlusSlot = Cast<UOverlaySlot>(EmptyPlusText->Slot))
		{
			PlusSlot->SetHorizontalAlignment(HAlign_Center);
			PlusSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	if (HeroInfoPanel)
	{
		if (UOverlaySlot* InfoSlot = Cast<UOverlaySlot>(HeroInfoPanel->Slot))
		{
			InfoSlot->SetHorizontalAlignment(HAlign_Left);
			InfoSlot->SetVerticalAlignment(VAlign_Bottom);
			InfoSlot->SetPadding(FMargin(18.f, 8.f, 8.f, 14.f));
		}
	}
	if (HeroNameText)
	{
		HeroNameText->SetDefaultColorAndOpacity(FSlateColor(FLinearColor::White));
	}
	if (HeroLevelText)
	{
		HeroLevelText->SetDefaultColorAndOpacity(
			FSlateColor(FLinearColor(0.8f, 0.86f, 1.f, 1.f)));
	}
	if (SelectedFrame)
	{
		if (UOverlaySlot* FrameSlot = Cast<UOverlaySlot>(SelectedFrame->Slot))
		{
			FrameSlot->SetHorizontalAlignment(HAlign_Fill);
			FrameSlot->SetVerticalAlignment(VAlign_Fill);
			FrameSlot->SetPadding(FMargin(2.f));
		}
	}
}

void UPartySetupSlotWidget::ApplySlotData(
	const FPartySlotViewData& InSlotData)
{
	SlotData = InSlotData;
	if (SlotNumberText)
	{
		FNumberFormattingOptions NumberOptions;
		NumberOptions.MinimumIntegralDigits = 2;
		NumberOptions.MaximumIntegralDigits = 2;
		SlotNumberText->SetText(FText::AsNumber(
			SlotData.SlotIndex + 1, &NumberOptions));
	}
	RefreshPresentation();
}

void UPartySetupSlotWidget::SetSelected(bool bInSelected)
{
	if (SelectedFrame)
	{
		SelectedFrame->SetVisibility(bInSelected
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

void UPartySetupSlotWidget::SetActive(bool bInActive)
{
	bIsActive = bInActive;
	RefreshPresentation();
}

void UPartySetupSlotWidget::HandleSlotClicked()
{
	UE_LOG(LogTemp, Log, TEXT("Party setup slot %d clicked."),
		SlotData.SlotIndex);
	OnPartySlotSelected.Broadcast(SlotData.SlotIndex);
}

void UPartySetupSlotWidget::RefreshPresentation()
{
	const bool bOccupied = SlotData.bOccupied;
	const ESlateVisibility OccupiedVisibility = bOccupied
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed;
	const ESlateVisibility ActiveVisibility = bOccupied && bIsActive
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed;

	if (HeroInfoPanel) HeroInfoPanel->SetVisibility(OccupiedVisibility);
	if (ActiveBadge) ActiveBadge->SetVisibility(ActiveVisibility);
	if (ActiveBadgeText)
	{
		ActiveBadgeText->SetText(
			NSLOCTEXT("Party", "ActiveBadge", "\u5f53\u524d\u51fa\u6218"));
	}
	if (EmptyPlusText)
	{
		EmptyPlusText->SetVisibility(bOccupied
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}

	if (FullBodyImage)
	{
		FullBodyImage->SetBrushResourceObject(nullptr);
		FullBodyImage->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (ElementIconImage)
	{
		UTexture2D* ElementTexture = bOccupied
			? const_cast<UTexture2D*>(SlotData.HeroInfo.ElementIcon.Get())
			: nullptr;
		ElementIconImage->SetBrushFromTexture(ElementTexture);
		ElementIconImage->SetVisibility(ElementTexture
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	if (HeroNameText)
	{
		HeroNameText->SetText(bOccupied
			? SlotData.HeroInfo.HeroName
			: FText::GetEmpty());
	}
	if (HeroLevelText)
	{
		HeroLevelText->SetText(bOccupied
			? FText::Format(NSLOCTEXT("Party", "HeroLevel", "Lv.{0}"),
				FText::AsNumber(SlotData.HeroInfo.DisplayLevel))
			: FText::GetEmpty());
	}
}
