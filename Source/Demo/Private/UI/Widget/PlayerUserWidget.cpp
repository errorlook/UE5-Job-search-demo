
#include "UI/Widget/PlayerUserWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/PlayerAttributeSet.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "Character/EnemyCharacter.h"
#include "Character/PlayerCharacter.h" 
#include "Components/PartyComponent.h"
#include "Player/OnePlayerController.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "Quest/QuestDataAsset.h"
#include "Player/OPlayerState.h"
#include "TimerManager.h"

void UPlayerUserWidget::SetWidgetController(UObject* InWidgetController)
{
	UnbindFromAttributes();
	UnbindFromOverlayController();
	UnbindFromPartyComponent();
	BoundEnemy.Reset();

	WidgetController = InWidgetController;

	if (ShouldRunBlueprintWidgetControllerSet())
	{
		WidgetControllerSet();
	}
	NativeWidgetControllerSet();

	// Quest delegates are bound inside WidgetControllerSet. Replay the current
	// state immediately so a quest accepted before HUD construction is not lost.
	if (UOverlayWidgetController* OverlayController =
		Cast<UOverlayWidgetController>(InWidgetController))
	{
		BoundOverlayController = OverlayController;
		OverlayController->OnTrackedQuestChanged.AddUniqueDynamic(
			this, &UPlayerUserWidget::HandleTrackedQuestChanged);
		OverlayController->OnQuestCompleted.AddUniqueDynamic(
			this, &UPlayerUserWidget::HandleQuestCompleted);
		OverlayController->RefreshQuestUI();

		if (AOPlayerState* OPlayerState = GetOwningPlayerState<AOPlayerState>())
		{
			BoundPartyComponent = OPlayerState->GetPartyComponent();
			if (UPartyComponent* PartyComponent = BoundPartyComponent.Get())
			{
				PartyComponent->OnPartyChanged.AddUniqueDynamic(
					this, &UPlayerUserWidget::HandlePartyHudChanged);
				RebuildPartyHud();
				RefreshSkillHud();
			}
		}
	}

	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(InWidgetController))
	{
		BoundEnemy = Enemy;
		Enemy->OnHealthChanged.AddUniqueDynamic(
			this, &UPlayerUserWidget::HandleHealthChanged);
		Enemy->OnMaxHealthChanged.AddUniqueDynamic(
			this, &UPlayerUserWidget::HandleMaxHealthChanged);

		if (const UPlayerAttributeSet* Attributes =
			Cast<UPlayerAttributeSet>(Enemy->GetAttributeSet()))
		{
			CurrentHealth = Attributes->GetHealth();
			CurrentMaxHealth = Attributes->GetMaxHealth();
			UpdateHealthPercent();
		}
	}
}

void UPlayerUserWidget::NativeWidgetControllerSet()
{
}

bool UPlayerUserWidget::ShouldRunBlueprintWidgetControllerSet() const
{
	return true;
}

void UPlayerUserWidget::UnbindFromAttributes()
{
	if (AEnemyCharacter* Enemy = BoundEnemy.Get())
	{
		Enemy->OnHealthChanged.RemoveDynamic(
			this, &UPlayerUserWidget::HandleHealthChanged);
		Enemy->OnMaxHealthChanged.RemoveDynamic(
			this, &UPlayerUserWidget::HandleMaxHealthChanged);
	}

	BoundEnemy.Reset();
}

void UPlayerUserWidget::UnbindFromOverlayController()
{
	if (UOverlayWidgetController* OverlayController =
		BoundOverlayController.Get())
	{
		OverlayController->OnTrackedQuestChanged.RemoveDynamic(
			this, &UPlayerUserWidget::HandleTrackedQuestChanged);
		OverlayController->OnQuestCompleted.RemoveDynamic(
			this, &UPlayerUserWidget::HandleQuestCompleted);
	}
	BoundOverlayController.Reset();
}

void UPlayerUserWidget::UnbindFromPartyComponent()
{
	if (UPartyComponent* PartyComponent = BoundPartyComponent.Get())
	{
		PartyComponent->OnPartyChanged.RemoveDynamic(
			this, &UPlayerUserWidget::HandlePartyHudChanged);
	}
	BoundPartyComponent.Reset();
}

void UPlayerUserWidget::HandlePartyHudChanged()
{
	RebuildPartyHud();
}

void UPlayerUserWidget::RebuildPartyHud()
{
	if (!WidgetTree || !BoundPartyComponent.IsValid()) return;

	UPanelWidget* SlotsPanel = Cast<UPanelWidget>(
		WidgetTree->FindWidget(TEXT("Slots")));
	if (!SlotsPanel) return;

	if (!CachedPartySlotClass)
	{
		for (int32 ChildIndex = 0;
			ChildIndex < SlotsPanel->GetChildrenCount(); ++ChildIndex)
		{
			if (UUserWidget* TemplateSlot = Cast<UUserWidget>(
				SlotsPanel->GetChildAt(ChildIndex)))
			{
				CachedPartySlotClass = TemplateSlot->GetClass();
				break;
			}
		}
	}
	if (!CachedPartySlotClass) return;

	SlotsPanel->ClearChildren();
	const TArray<FHeroSlotInfo> ActiveHeroes =
		BoundPartyComponent->GetActivePartyInfo();
	for (int32 Index = 0; Index < ActiveHeroes.Num(); ++Index)
	{
		const FHeroSlotInfo& HeroInfo = ActiveHeroes[Index];
		UUserWidget* PartySlot = CreateWidget<UUserWidget>(
			GetOwningPlayer(), CachedPartySlotClass);
		if (!PartySlot) continue;

		if (PartySlot->WidgetTree)
		{
			if (UImage* AvatarImage = Cast<UImage>(
				PartySlot->WidgetTree->FindWidget(TEXT("Img_Avatar"))))
			{
				AvatarImage->SetBrushFromTexture(
					const_cast<UTexture2D*>(HeroInfo.AvatarIcon.Get()), false);
			}
			if (UTextBlock* HotkeyText = Cast<UTextBlock>(
				PartySlot->WidgetTree->FindWidget(TEXT("Text_Hotkey"))))
			{
				HotkeyText->SetText(HeroInfo.SwapHotkey.IsEmpty()
					? FText::AsNumber(Index + 1)
					: HeroInfo.SwapHotkey);
			}
		}
		SlotsPanel->AddChild(PartySlot);
	}
}

void UPlayerUserWidget::RefreshSkillHud()
{
	if (!WidgetTree) return;

	const AOPlayerState* OPlayerState =
		GetOwningPlayerState<AOPlayerState>();
	const UAbilitySystemComponent* AbilitySystemComponent =
		OPlayerState ? OPlayerState->GetAbilitySystemComponent() : nullptr;
	const APlayerCharacter* PlayerCharacter = AbilitySystemComponent
		? Cast<APlayerCharacter>(AbilitySystemComponent->GetAvatarActor())
		: nullptr;
	const UOverlayWidgetController* OverlayController =
		BoundOverlayController.Get();
	if (!PlayerCharacter || !OverlayController) return;

	for (const TSubclassOf<UGameplayAbility> AbilityClass :
		PlayerCharacter->GetStartupAbilityClasses())
	{
		const FPlayerAbilityInfo Info =
			OverlayController->FindAbilityInfoForClass(AbilityClass);
		const FString InputName = Info.InputTag.ToString();
		const FName SlotName = InputName.EndsWith(TEXT(".E"))
			? FName(TEXT("Slot_Skill1"))
			: InputName.EndsWith(TEXT(".R"))
				? FName(TEXT("Slot_Skill2"))
				: NAME_None;
		if (SlotName.IsNone()) continue;

		UUserWidget* SkillSlot = Cast<UUserWidget>(
			WidgetTree->FindWidget(SlotName));
		if (!SkillSlot || !SkillSlot->WidgetTree) continue;

		if (UImage* SkillIcon = Cast<UImage>(
			SkillSlot->WidgetTree->FindWidget(TEXT("Img_SkillIcon"))))
		{
			SkillIcon->SetBrushFromTexture(Info.Icon, true);
		}
		if (UTextBlock* InputText = Cast<UTextBlock>(
			SkillSlot->WidgetTree->FindWidget(TEXT("Text_InputKey"))))
		{
			InputText->SetText(InputName.EndsWith(TEXT(".E"))
				? FText::FromString(TEXT("E"))
				: FText::FromString(TEXT("R")));
		}
	}
}

void UPlayerUserWidget::HandleTrackedQuestChanged(
	bool bHasTrackedQuest, FQuestRuntimeEntry QuestEntry)
{
	UpdateQuestTrackerTree(this, bHasTrackedQuest, QuestEntry);
}

void UPlayerUserWidget::UpdateQuestTrackerTree(
	UUserWidget* RootWidget, bool bHasTrackedQuest,
	const FQuestRuntimeEntry& QuestEntry)
{
	if (!RootWidget || !RootWidget->WidgetTree) return;

	if (UWidget* TrackerPanel =
		RootWidget->WidgetTree->FindWidget(TEXT("TrackerPanel")))
	{
		TrackerPanel->SetVisibility(bHasTrackedQuest
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);

		const UQuestDataAsset* Quest = bHasTrackedQuest
			? QuestEntry.Quest.Get()
			: nullptr;
		if (Quest)
		{
			auto SetNamedText = [RootWidget](FName WidgetName, const FText& Text)
			{
				if (URichTextBlock* RichText = Cast<URichTextBlock>(
					RootWidget->WidgetTree->FindWidget(WidgetName)))
				{
					RichText->SetText(Text);
				}
				else if (UTextBlock* TextBlock = Cast<UTextBlock>(
					RootWidget->WidgetTree->FindWidget(WidgetName)))
				{
					TextBlock->SetText(Text);
				}
			};

			SetNamedText(TEXT("TrackedTitleText"), Quest->Title);
			SetNamedText(TEXT("TrackedObjectiveText"), Quest->ObjectiveText);
			SetNamedText(TEXT("TrackedProgressText"), FText::Format(
				FText::FromString(TEXT("{0}/{1}")),
				FText::AsNumber(QuestEntry.CurrentProgress),
				FText::AsNumber(FMath::Max(1, Quest->RequiredCount))));
		}
		return;
	}

	TArray<UWidget*> ChildWidgets;
	RootWidget->WidgetTree->GetAllWidgets(ChildWidgets);
	for (UWidget* ChildWidget : ChildWidgets)
	{
		if (UUserWidget* ChildUserWidget = Cast<UUserWidget>(ChildWidget))
		{
			UpdateQuestTrackerTree(
				ChildUserWidget, bHasTrackedQuest, QuestEntry);
		}
	}
}

void UPlayerUserWidget::HandleQuestCompleted(UQuestDataAsset* Quest)
{
	if (UpdateQuestCompletionTree(this, Quest))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				QuestCompletionTimer, this,
				&UPlayerUserWidget::HideQuestCompletion, 4.f, false);
		}
	}
	ShowQuestCompletion(Quest);
}

bool UPlayerUserWidget::UpdateQuestCompletionTree(
	UUserWidget* RootWidget, UQuestDataAsset* Quest)
{
	if (!RootWidget || !RootWidget->WidgetTree || !IsValid(Quest)) return false;

	if (UWidget* Panel =
		RootWidget->WidgetTree->FindWidget(TEXT("QuestCompletePanel")))
	{
		Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		auto SetNamedText = [RootWidget](FName WidgetName, const FText& Text)
		{
			if (URichTextBlock* RichText = Cast<URichTextBlock>(
				RootWidget->WidgetTree->FindWidget(WidgetName)))
			{
				RichText->SetText(Text);
			}
			else if (UTextBlock* TextBlock = Cast<UTextBlock>(
				RootWidget->WidgetTree->FindWidget(WidgetName)))
			{
				TextBlock->SetText(Text);
			}
		};

		SetNamedText(TEXT("QuestCompleteHeaderText"), FText::FromString(
			TEXT("\u4efb\u52a1\u5b8c\u6210")));
		SetNamedText(TEXT("QuestCompleteTitleText"), Quest->Title);
		SetNamedText(TEXT("QuestCompleteRewardText"), Quest->RewardText);
		return true;
	}

	TArray<UWidget*> ChildWidgets;
	RootWidget->WidgetTree->GetAllWidgets(ChildWidgets);
	for (UWidget* ChildWidget : ChildWidgets)
	{
		if (UUserWidget* ChildUserWidget = Cast<UUserWidget>(ChildWidget))
		{
			if (UpdateQuestCompletionTree(ChildUserWidget, Quest)) return true;
		}
	}
	return false;
}

void UPlayerUserWidget::HideQuestCompletion()
{
	SetQuestCompletionVisibility(this, ESlateVisibility::Collapsed);
}

void UPlayerUserWidget::SetQuestCompletionVisibility(
	UUserWidget* RootWidget, ESlateVisibility InVisibility)
{
	if (!RootWidget || !RootWidget->WidgetTree) return;
	if (UWidget* Panel =
		RootWidget->WidgetTree->FindWidget(TEXT("QuestCompletePanel")))
	{
		Panel->SetVisibility(InVisibility);
		return;
	}

	TArray<UWidget*> ChildWidgets;
	RootWidget->WidgetTree->GetAllWidgets(ChildWidgets);
	for (UWidget* ChildWidget : ChildWidgets)
	{
		if (UUserWidget* ChildUserWidget = Cast<UUserWidget>(ChildWidget))
		{
			SetQuestCompletionVisibility(ChildUserWidget, InVisibility);
		}
	}
}

void UPlayerUserWidget::HandleHealthChanged(float NewHealth)
{
	CurrentHealth = NewHealth;
	UpdateHealthPercent();
}

void UPlayerUserWidget::HandleMaxHealthChanged(float NewMaxHealth)
{
	CurrentMaxHealth = NewMaxHealth;
	UpdateHealthPercent();
}

void UPlayerUserWidget::UpdateHealthPercent()
{
	// Player HUD widgets use their Overlay WidgetController Blueprint flow.
	// Native health updates are only for Enemy/Boss-backed widgets.
	if (!BoundEnemy.IsValid())
	{
		return;
	}

	if (!BoundHealthProgressBar.IsValid() && WidgetTree)
	{
		BoundHealthProgressBar =
			Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("HealthBar")));
		if (!BoundHealthProgressBar.IsValid())
		{
			BoundHealthProgressBar =
				Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("Health")));
		}
	}

	if (UProgressBar* HealthProgressBar = BoundHealthProgressBar.Get())
	{
		const float Percent = CurrentMaxHealth > 0.f
			? CurrentHealth / CurrentMaxHealth
			: 0.f;
		// Enemy and Boss bars are driven by the native attribute delegates.
		// A stale Designer binding would otherwise overwrite SetPercent each frame.
		HealthProgressBar->PercentDelegate.Unbind();
		HealthProgressBar->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
	}
	else if (BoundEnemy.IsValid())
	{
		UE_LOG(LogTemp, Error,
			TEXT("%s requires a ProgressBar named HealthBar (or legacy Health)."),
			*GetName());
	}
}

// Bind native events once for the lifetime of the widget.
void UPlayerUserWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree)
	{
		BoundHealthProgressBar =
			Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("HealthBar")));
		if (!BoundHealthProgressBar.IsValid())
		{
			BoundHealthProgressBar =
				Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("Health")));
		}
	}

	// NativeOnInitialized avoids duplicate bindings across repeated opens.
	if (close)
	{
		close->OnClicked.AddDynamic(this, &UPlayerUserWidget::OnCloseButtonClicked);
	}
}

void UPlayerUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateHealthPercent();
}

void UPlayerUserWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(QuestCompletionTimer);
	}
	UnbindFromAttributes();
	UnbindFromOverlayController();
	UnbindFromPartyComponent();

	if (SubPanelInstance && SubPanelInstance->IsInViewport())
	{
		SubPanelInstance->RemoveFromParent();
		SubPanelInstance = nullptr; 
	}
	Super::NativeDestruct();
}

// Main panels restore game input; child panels only remove themselves.
void UPlayerUserWidget::OnCloseButtonClicked()
{
	if (AOnePlayerController* PlayerController =
		Cast<AOnePlayerController>(GetOwningPlayer()))
	{
		if (PlayerController->CloseManagedMenu(this)) return;
	}

	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetOwningPlayerPawn()))
	{
		// Compare against the main panel cached by the character.
		if (PlayerChar->CharacterPanelInstance == this)
		{
			// The character owns the full close and input-restoration flow.
			PlayerChar->ToggleOpenPanelAction();
		}
		else
		{
			// Secondary panels can remove themselves directly.
			this->RemoveFromParent();
		}
	}
}
