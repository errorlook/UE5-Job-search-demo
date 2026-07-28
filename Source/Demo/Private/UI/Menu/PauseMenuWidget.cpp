#include "UI/Menu/PauseMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/PanelWidget.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Player/OnePlayerController.h"
#include "Scripting/LuaHotReloadSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogLuaSettingsWidget, Log, All);

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	MenuController = Cast<AOnePlayerController>(GetOwningPlayer());
	if (AOnePlayerController* PlayerController = MenuController.Get())
	{
		PlayerController->OnPauseMenuStateChanged.AddUniqueDynamic(
			this, &UPauseMenuWidget::HandlePauseMenuStateChanged);
		PlayerController->OnPauseMenuPageChanged.AddUniqueDynamic(
			this, &UPauseMenuWidget::HandlePauseMenuPageChanged);
		PlayerController->OnPauseMenuConfirmationRequested.AddUniqueDynamic(
			this, &UPauseMenuWidget::HandlePauseMenuConfirmationRequested);
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		HotReloadSubsystem =
			GameInstance->GetSubsystem<ULuaHotReloadSubsystem>();
		if (ULuaHotReloadSubsystem* HotReload = HotReloadSubsystem.Get())
		{
			HotReload->OnSettingsDefinitionsChanged.AddUniqueDynamic(
				this, &UPauseMenuWidget::HandleSettingsDefinitionsChanged);
		}
	}

	ApplySettingsButton = WidgetTree
		? Cast<UButton>(WidgetTree->FindWidget(TEXT("Btn_ApplySettings")))
		: nullptr;
	ResetSettingsButton = WidgetTree
		? Cast<UButton>(WidgetTree->FindWidget(TEXT("Btn_ResetSettings")))
		: nullptr;
	if (UButton* Button = ApplySettingsButton.Get())
	{
		Button->OnClicked.AddUniqueDynamic(
			this, &UPauseMenuWidget::HandleApplySettingsClicked);
	}
	if (UButton* Button = ResetSettingsButton.Get())
	{
		Button->OnClicked.AddUniqueDynamic(
			this, &UPauseMenuWidget::HandleResetSettingsClicked);
	}
	RebuildLuaSettings();
}

void UPauseMenuWidget::NativeDestruct()
{
	if (ULuaHotReloadSubsystem* HotReload = HotReloadSubsystem.Get())
	{
		HotReload->OnSettingsDefinitionsChanged.RemoveDynamic(
			this, &UPauseMenuWidget::HandleSettingsDefinitionsChanged);
	}
	if (UButton* Button = ApplySettingsButton.Get())
	{
		Button->OnClicked.RemoveDynamic(
			this, &UPauseMenuWidget::HandleApplySettingsClicked);
	}
	if (UButton* Button = ResetSettingsButton.Get())
	{
		Button->OnClicked.RemoveDynamic(
			this, &UPauseMenuWidget::HandleResetSettingsClicked);
	}
	SettingComboBoxes.Empty();
	SelectedSettingOptions.Empty();
	for (const TPair<FName, TObjectPtr<UVerticalBox>>& Pair :
		DynamicSettingsContainers)
	{
		if (UVerticalBox* Container = Pair.Value.Get())
		{
			Container->ClearChildren();
			if (Container->GetFName() != TEXT("LuaSettingsContainer"))
			{
				Container->RemoveFromParent();
			}
		}
	}
	DynamicSettingsContainers.Empty();
	ApplySettingsButton.Reset();
	ResetSettingsButton.Reset();
	HotReloadSubsystem.Reset();

	if (AOnePlayerController* PlayerController = MenuController.Get())
	{
		// Notify first so the widget can receive the final closed-state
		// broadcast before its controller delegates are removed.
		PlayerController->NotifyPauseMenuWidgetRemoved(this);
		PlayerController->OnPauseMenuStateChanged.RemoveDynamic(
			this, &UPauseMenuWidget::HandlePauseMenuStateChanged);
		PlayerController->OnPauseMenuPageChanged.RemoveDynamic(
			this, &UPauseMenuWidget::HandlePauseMenuPageChanged);
		PlayerController->OnPauseMenuConfirmationRequested.RemoveDynamic(
			this, &UPauseMenuWidget::HandlePauseMenuConfirmationRequested);
	}

	MenuController.Reset();
	Super::NativeDestruct();
}

int32 UPauseMenuWidget::GetSettingValue(FName SettingId) const
{
	const ULuaHotReloadSubsystem* HotReload = HotReloadSubsystem.Get();
	return HotReload ? HotReload->GetSettingValue(SettingId) : INDEX_NONE;
}

bool UPauseMenuWidget::SetSettingValue(FName SettingId, int32 Value)
{
	ULuaHotReloadSubsystem* HotReload = HotReloadSubsystem.Get();
	return HotReload && HotReload->SetSettingValue(SettingId, Value);
}

void UPauseMenuWidget::ApplySettings()
{
	if (ULuaHotReloadSubsystem* HotReload = HotReloadSubsystem.Get())
	{
		HotReload->ApplySettings();
	}
}

void UPauseMenuWidget::SaveSettings()
{
	if (ULuaHotReloadSubsystem* HotReload = HotReloadSubsystem.Get())
	{
		HotReload->SaveSettings();
	}
}

void UPauseMenuWidget::ResetSettings()
{
	if (ULuaHotReloadSubsystem* HotReload = HotReloadSubsystem.Get())
	{
		HotReload->ResetSettings();
	}
}

void UPauseMenuWidget::RequestAction(EPauseMenuAction Action)
{
	if (AOnePlayerController* PlayerController = MenuController.Get())
	{
		PlayerController->RequestPauseMenuAction(Action);
	}
}

void UPauseMenuWidget::ConfirmPendingAction(bool bConfirmed)
{
	if (AOnePlayerController* PlayerController = MenuController.Get())
	{
		PlayerController->ConfirmPauseMenuAction(bConfirmed);
	}
}

void UPauseMenuWidget::HandlePauseMenuStateChanged(bool bIsOpen)
{
	OnPauseMenuStateChanged(bIsOpen);
}

void UPauseMenuWidget::HandlePauseMenuPageChanged(EPauseMenuPage NewPage)
{
	const EPauseMenuAction PendingAction = MenuController.IsValid()
		? MenuController->GetPendingPauseMenuAction()
		: EPauseMenuAction::None;
	OnPauseMenuPageChanged(NewPage, PendingAction);
}

void UPauseMenuWidget::HandlePauseMenuConfirmationRequested(
	EPauseMenuAction Action)
{
	OnPauseMenuConfirmationRequested(Action);
}

void UPauseMenuWidget::HandleSettingsDefinitionsChanged(int32 NewRevision)
{
	RebuildLuaSettings();
	UE_LOG(LogLuaSettingsWidget, Display,
		TEXT("[LuaHotReload] Settings UI refreshed (revision %d)."),
		NewRevision);
}

void UPauseMenuWidget::HandleSettingSelectionChanged(
	FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bRebuildingSettings || SelectionType == ESelectInfo::Direct) return;
	ULuaHotReloadSubsystem* HotReload = HotReloadSubsystem.Get();
	if (!HotReload) return;

	for (const TPair<TObjectPtr<UComboBoxString>, FName>& Pair :
		SettingComboBoxes)
	{
		UComboBoxString* ComboBox = Pair.Key.Get();
		FString* PreviousSelection = SelectedSettingOptions.Find(ComboBox);
		if (!ComboBox || ComboBox->GetSelectedOption() != SelectedItem ||
			(PreviousSelection && *PreviousSelection == SelectedItem))
		{
			continue;
		}
		const FLuaSettingDefinition* Definition =
			HotReload->GetSettingDefinitions().FindByPredicate(
				[&Pair](const FLuaSettingDefinition& Candidate)
				{
					return Candidate.SettingId == Pair.Value;
				});
		if (!Definition) continue;
		const int32 OptionIndex = ComboBox->FindOptionIndex(SelectedItem);
		if (Definition->Options.IsValidIndex(OptionIndex))
		{
			HotReload->SetSettingValue(
				Pair.Value, Definition->Options[OptionIndex].Value);
			SelectedSettingOptions.FindOrAdd(ComboBox) = SelectedItem;
		}
		return;
	}
}

void UPauseMenuWidget::HandleApplySettingsClicked()
{
	ApplySettings();
	SaveSettings();
}

void UPauseMenuWidget::HandleResetSettingsClicked()
{
	ResetSettings();
}

void UPauseMenuWidget::RebuildLuaSettings()
{
	if (!WidgetTree || !HotReloadSubsystem.IsValid()) return;
	TGuardValue<bool> RebuildGuard(bRebuildingSettings, true);
	SettingComboBoxes.Empty();
	SelectedSettingOptions.Empty();
	for (const TPair<FName, TObjectPtr<UVerticalBox>>& Pair :
		DynamicSettingsContainers)
	{
		if (UVerticalBox* Container = Pair.Value.Get())
		{
			Container->ClearChildren();
			if (Container->GetFName() != TEXT("LuaSettingsContainer"))
			{
				Container->RemoveFromParent();
			}
		}
	}
	DynamicSettingsContainers.Empty();
	UpdateSettingsText();

	for (const FLuaSettingDefinition& Definition :
		HotReloadSubsystem->GetSettingDefinitions())
	{
		if (!Definition.bVisible) continue;
		TObjectPtr<UVerticalBox>& Container =
			DynamicSettingsContainers.FindOrAdd(Definition.Category);
		if (!Container.Get())
		{
			Container = ResolveSettingsContainer(Definition.Category);
			if (Container.Get()) Container->ClearChildren();
		}
		if (!Container.Get()) continue;

		UVerticalBox* SettingBlock = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(Definition.DisplayName);
		Label->SetToolTipText(Definition.Description);
		Row->AddChildToHorizontalBox(Label);

		UComboBoxString* ComboBox =
			WidgetTree->ConstructWidget<UComboBoxString>();
		int32 SelectedIndex = INDEX_NONE;
		const int32 CurrentValue = GetSettingValue(Definition.SettingId);
		for (int32 Index = 0; Index < Definition.Options.Num(); ++Index)
		{
			ComboBox->AddOption(Definition.Options[Index].Label.ToString());
			if (Definition.Options[Index].Value == CurrentValue)
			{
				SelectedIndex = Index;
			}
		}
		if (Definition.Options.IsValidIndex(SelectedIndex))
		{
			ComboBox->SetSelectedOption(
				Definition.Options[SelectedIndex].Label.ToString());
		}
		ComboBox->SetToolTipText(Definition.Description);
		ComboBox->OnSelectionChanged.AddUniqueDynamic(
			this, &UPauseMenuWidget::HandleSettingSelectionChanged);
		SettingComboBoxes.Add(ComboBox, Definition.SettingId);
		SelectedSettingOptions.Add(ComboBox, ComboBox->GetSelectedOption());
		Row->AddChildToHorizontalBox(ComboBox);
		SettingBlock->AddChildToVerticalBox(Row);
		UTextBlock* Description = WidgetTree->ConstructWidget<UTextBlock>();
		Description->SetText(Definition.Description);
		Description->SetAutoWrapText(true);
		SettingBlock->AddChildToVerticalBox(Description);
		Container->AddChildToVerticalBox(SettingBlock);
	}
}

void UPauseMenuWidget::UpdateSettingsText()
{
	if (!WidgetTree || !HotReloadSubsystem.IsValid()) return;
	const FLuaSettingsUiText Text = HotReloadSubsystem->GetSettingsUiText();
	auto SetNamedText = [this](FName WidgetName, const FText& Value)
	{
		if (UTextBlock* TextBlock = Cast<UTextBlock>(
			WidgetTree->FindWidget(WidgetName)))
		{
			TextBlock->SetText(Value);
		}
		else if (URichTextBlock* RichText = Cast<URichTextBlock>(
			WidgetTree->FindWidget(WidgetName)))
		{
			RichText->SetText(Value);
		}
	};
	SetNamedText(TEXT("Txt_SettingsTitle"), Text.PageTitle);
	SetNamedText(TEXT("Txt_ApplySettings"), Text.ApplyButton);
	SetNamedText(TEXT("Txt_ResetSettings"), Text.ResetButton);
	SetNamedText(TEXT("Txt_VideoTab"), Text.VideoTab);
	SetNamedText(TEXT("Txt_AudioTab"), Text.AudioTab);
	SetNamedText(TEXT("Txt_KeysTab"), Text.KeysTab);
}

UVerticalBox* UPauseMenuWidget::ResolveSettingsContainer(FName Category)
{
	if (!WidgetTree) return nullptr;
	if (Category == TEXT("Video"))
	{
		if (UVerticalBox* Existing = Cast<UVerticalBox>(
			WidgetTree->FindWidget(TEXT("LuaSettingsContainer"))))
		{
			return Existing;
		}
	}
	const FName PageName = Category == TEXT("Audio")
		? TEXT("Page_AudioSettings")
		: Category == TEXT("Keys") || Category == TEXT("Input")
			? TEXT("Page_KeySettings") : TEXT("Page_VideoSettings");
	UPanelWidget* Page = Cast<UPanelWidget>(WidgetTree->FindWidget(PageName));
	if (!Page) return nullptr;

	UVerticalBox* Container = WidgetTree->ConstructWidget<UVerticalBox>();
	UPanelSlot* AddedSlot = Page->AddChild(Container);
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(AddedSlot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(24.f));
	}
	return Container;
}
