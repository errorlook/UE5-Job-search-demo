#include "UI/Party/PartyHeroCardWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/PartyComponent.h"
#include "Components/RichTextBlock.h"

void UPartyHeroCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (HeroButton)
	{
		HeroButton->SetBackgroundColor(FLinearColor(0.055f, 0.075f, 0.12f, 0.94f));
		// Discard the legacy Add/Remove binding from WBP_HeroCard.
		HeroButton->OnClicked.Clear();
		HeroButton->OnClicked.AddUniqueDynamic(
			this, &UPartyHeroCardWidget::HandleHeroClicked);
	}

	if (HeroAvatarImage)
	{
		if (UOverlaySlot* AvatarSlot = Cast<UOverlaySlot>(HeroAvatarImage->Slot))
		{
			AvatarSlot->SetHorizontalAlignment(HAlign_Fill);
			AvatarSlot->SetVerticalAlignment(VAlign_Fill);
			AvatarSlot->SetPadding(FMargin(6.f, 6.f, 6.f, 28.f));
		}
	}
	if (HeroNameText)
	{
		HeroNameText->SetDefaultColorAndOpacity(FSlateColor(FLinearColor::White));
		if (UOverlaySlot* NameSlot = Cast<UOverlaySlot>(HeroNameText->Slot))
		{
			NameSlot->SetHorizontalAlignment(HAlign_Center);
			NameSlot->SetVerticalAlignment(VAlign_Bottom);
			NameSlot->SetPadding(FMargin(4.f, 0.f, 4.f, 5.f));
		}
	}
	if (PartyStateText)
	{
		PartyStateText->SetDefaultColorAndOpacity(
			FSlateColor(FLinearColor(0.75f, 0.95f, 1.f, 1.f)));
		if (UOverlaySlot* StateSlot = Cast<UOverlaySlot>(PartyStateText->Slot))
		{
			StateSlot->SetHorizontalAlignment(HAlign_Left);
			StateSlot->SetVerticalAlignment(VAlign_Top);
			StateSlot->SetPadding(FMargin(8.f));
		}
	}
}

void UPartyHeroCardWidget::ApplyHeroData(
	const FHeroSlotInfo& InHeroInfo, UPartyComponent* InPartyComponent)
{
	HeroInfo = InHeroInfo;
	PartyComponent = InPartyComponent;

	if (HeroAvatarImage)
	{
		HeroAvatarImage->SetBrushFromTexture(
			const_cast<UTexture2D*>(HeroInfo.AvatarIcon.Get()));
	}
	if (HeroNameText) HeroNameText->SetText(HeroInfo.HeroName);

	int32 PartySlotIndex = INDEX_NONE;
	if (PartyComponent)
	{
		for (const FPartySlotViewData& PartySlotData :
			PartyComponent->GetPartySlots())
		{
			if (PartySlotData.bOccupied &&
				PartySlotData.HeroInfo.HeroTag == HeroInfo.HeroTag)
			{
				PartySlotIndex = PartySlotData.SlotIndex;
				break;
			}
		}
	}
	RefreshPartyState(PartySlotIndex);
	SetCandidateSelected(false);
}

void UPartyHeroCardWidget::ApplyPreviewHeroData(
	const FHeroSlotInfo& InHeroInfo, bool bInPendingParty)
{
	HeroInfo = InHeroInfo;
	PartyComponent = nullptr;
	if (HeroAvatarImage)
	{
		HeroAvatarImage->SetBrushFromTexture(
			const_cast<UTexture2D*>(HeroInfo.AvatarIcon.Get()));
	}
	if (HeroNameText) HeroNameText->SetText(HeroInfo.HeroName);
	if (PartyStateText)
	{
		PartyStateText->SetText(bInPendingParty
			? NSLOCTEXT("Party", "InParty", "\u5df2\u5165\u961f")
			: FText::GetEmpty());
		PartyStateText->SetVisibility(bInPendingParty
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (PartyStateBadge)
	{
		PartyStateBadge->SetVisibility(bInPendingParty
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	SetCandidateSelected(false);
}

void UPartyHeroCardWidget::ApplySelectionData(
	const FHeroSlotInfo& InHeroInfo,
	int32 InPendingPartySlotIndex,
	bool bInCandidateSelected)
{
	HeroInfo = InHeroInfo;
	PartyComponent = nullptr;
	if (HeroAvatarImage)
	{
		HeroAvatarImage->SetBrushFromTexture(
			const_cast<UTexture2D*>(HeroInfo.AvatarIcon.Get()));
	}
	if (HeroNameText) HeroNameText->SetText(HeroInfo.HeroName);
	RefreshPartyState(InPendingPartySlotIndex);
	SetCandidateSelected(bInCandidateSelected);
}

void UPartyHeroCardWidget::RefreshPartyState(int32 PendingPartySlotIndex)
{
	const bool bInParty = PendingPartySlotIndex != INDEX_NONE;
	if (PartyStateText)
	{
		FNumberFormattingOptions NumberOptions;
		NumberOptions.MinimumIntegralDigits = 2;
		NumberOptions.MaximumIntegralDigits = 2;
		PartyStateText->SetText(bInParty
			? FText::Format(
				NSLOCTEXT("Party", "AssignedSlot", "\u5df2\u7f16\u5165 {0}"),
				FText::AsNumber(PendingPartySlotIndex + 1, &NumberOptions))
			: FText::GetEmpty());
		PartyStateText->SetVisibility(bInParty
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (PartyStateBadge)
	{
		PartyStateBadge->SetVisibility(bInParty
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

void UPartyHeroCardWidget::SetCandidateSelected(bool bInCandidateSelected)
{
	if (HoverFrame)
	{
		HoverFrame->SetVisibility(bInCandidateSelected
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

void UPartyHeroCardWidget::HandleHeroClicked()
{
	if (HeroInfo.HeroTag.IsValid())
	{
		OnHeroSelected.Broadcast(HeroInfo.HeroTag);
	}
}
