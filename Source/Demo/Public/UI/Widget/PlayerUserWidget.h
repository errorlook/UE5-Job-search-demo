// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/QuestComponent.h"
#include "PlayerUserWidget.generated.h"

// Forward declarations keep UI headers lightweight.
class UButton;
class UProgressBar;
class APlayerCharacter;
class AEnemyCharacter;
class UOverlayWidgetController;
class UQuestDataAsset;
class UPartyComponent;

/**
 * Base player widget with controller injection and shared close behavior.
 */
UCLASS()
class DEMO_API UPlayerUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Injects the widget controller and notifies the Blueprint.
	UFUNCTION(BlueprintCallable, Category = "UI|Controller")
	void SetWidgetController(UObject* InWidgetController);

	// Controller currently driving this widget.
	UPROPERTY(BlueprintReadOnly, Category = "UI|Controller")
	TObjectPtr<UObject> WidgetController;

	// Overlay Blueprints only decide how the completion presentation looks.
	// Native code owns event binding and supplies the completed quest.
	UFUNCTION(BlueprintImplementableEvent, Category = "Quest|Presentation")
	void ShowQuestCompletion(UQuestDataAsset* Quest);

protected:
	// Native subclasses can own controller binding while Blueprint subclasses
	// continue using WidgetControllerSet for presentation-only setup.
	virtual void NativeWidgetControllerSet();
	virtual bool ShouldRunBlueprintWidgetControllerSet() const;

	// Implemented by Blueprints that bind controller delegates.
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Controller")
	void WidgetControllerSet();

	// UI lifecycle.

	// Binds native widget events.
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	// Removes any child panel owned by this widget.
	virtual void NativeDestruct() override;

	// Optionally binds a Designer button named "close".
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> close;

	// Handles the optional close button.
	UFUNCTION()
	void OnCloseButtonClicked();

	UFUNCTION()
	void HandleHealthChanged(float NewHealth);

	UFUNCTION()
	void HandleMaxHealthChanged(float NewMaxHealth);

	UFUNCTION()
	void HandleTrackedQuestChanged(
		bool bHasTrackedQuest, FQuestRuntimeEntry QuestEntry);

	UFUNCTION()
	void HandleQuestCompleted(UQuestDataAsset* Quest);

	void UpdateHealthPercent();
	void UnbindFromAttributes();
	void UnbindFromOverlayController();
	void UnbindFromPartyComponent();
	void RebuildPartyHud();
	void RefreshSkillHud();

	UFUNCTION()
	void HandlePartyHudChanged();
	void UpdateQuestTrackerTree(
		UUserWidget* RootWidget, bool bHasTrackedQuest,
		const FQuestRuntimeEntry& QuestEntry);
	bool UpdateQuestCompletionTree(
		UUserWidget* RootWidget, UQuestDataAsset* Quest);
	void HideQuestCompletion();
	void SetQuestCompletionVisibility(
		UUserWidget* RootWidget, ESlateVisibility InVisibility);

	// Resolved by name at runtime to avoid colliding with Blueprint variables
	// already named "Health" in existing widget assets.
	TWeakObjectPtr<UProgressBar> BoundHealthProgressBar;

	TWeakObjectPtr<AEnemyCharacter> BoundEnemy;
	TWeakObjectPtr<UOverlayWidgetController> BoundOverlayController;
	TWeakObjectPtr<UPartyComponent> BoundPartyComponent;

	// Preserves the Designer template class after Slots removes its children.
	UPROPERTY(Transient)
	TSubclassOf<UUserWidget> CachedPartySlotClass;

	float CurrentHealth = 0.f;
	float CurrentMaxHealth = 1.f;
	FTimerHandle QuestCompletionTimer;

	// Child panel removed when this widget is destroyed.
	UPROPERTY(BlueprintReadWrite, Category = "UI|SubPanel")
	TObjectPtr<UPlayerUserWidget> SubPanelInstance; 
};
