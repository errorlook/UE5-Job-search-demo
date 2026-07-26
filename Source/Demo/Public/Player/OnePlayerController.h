// OnePlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Components/PartyComponent.h"
#include "input/PlayerInputConfig.h"
#include "InputActionValue.h"
#include "UI/Menu/PauseMenuTypes.h"
#include "OnePlayerController.generated.h"

class UDamageTextComponent;
class UInputMappingContext;
class UInputAction;
class UPlayerAbilitySystemComponent;
class UDialogueDataAsset;
class UDialogueWidget;
class UPlayerUserWidget;
class UPauseMenuWidget;
class UUserWidget;
class ABossCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FPauseMenuStateChangedSignature, bool, bIsOpen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FPauseMenuPageChangedSignature, EPauseMenuPage, NewPage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FPauseMenuConfirmationRequestedSignature, EPauseMenuAction, Action);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FPartySetupApplyResultSignature, EPartySetupApplyResult, Result);

UCLASS(BlueprintType, Blueprintable)
class DEMO_API AOnePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AOnePlayerController();

	// Toggles cursor visibility from the Alt input action.
	UFUNCTION(BlueprintCallable, Category = "Input")
	void ToggleMouseCursor();

	// Returns focus to the game when the player clicks the viewport.
	UFUNCTION(BlueprintCallable, Category = "Input")
	void OnClickScreen();

	virtual void SetupInputComponent() override;

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void OpenDialogue(UDialogueDataAsset* DialogueData, AActor* InteractionSource);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void CloseDialogue();

	UFUNCTION(BlueprintCallable, Category = "Quest|UI")
	void ToggleQuestList();

	UFUNCTION(BlueprintCallable, Category = "Party|UI")
	void TogglePartyPage();

	UFUNCTION(BlueprintCallable, Category = "Party|Switch")
	void SwitchToPartySlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Party|Setup")
	void RequestApplyPartySetup(
		const TArray<FGameplayTag>& NewParty, int32 NewActiveSlotIndex);

	UPROPERTY(BlueprintAssignable, Category = "Party|Setup")
	FPartySetupApplyResultSignature OnPartySetupApplyResult;

	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void TogglePauseMenu();

	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void OpenPauseMenu();

	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void ClosePauseMenu();

	UFUNCTION(BlueprintPure, Category = "Pause Menu")
	bool IsPauseMenuOpen() const;

	UFUNCTION(BlueprintPure, Category = "Pause Menu")
	EPauseMenuPage GetPauseMenuPage() const { return PauseMenuPage; }

	UFUNCTION(BlueprintPure, Category = "Pause Menu")
	EPauseMenuAction GetPendingPauseMenuAction() const
	{
		return PendingPauseMenuAction;
	}

	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void RequestPauseMenuAction(EPauseMenuAction Action);

	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void ConfirmPauseMenuAction(bool bConfirmed);

	UPROPERTY(BlueprintAssignable, Category = "Pause Menu")
	FPauseMenuStateChangedSignature OnPauseMenuStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Pause Menu")
	FPauseMenuPageChangedSignature OnPauseMenuPageChanged;

	UPROPERTY(BlueprintAssignable, Category = "Pause Menu")
	FPauseMenuConfirmationRequestedSignature
		OnPauseMenuConfirmationRequested;

	// Called by UPauseMenuWidget when it is removed outside the facade.
	void NotifyPauseMenuWidgetRemoved(UPauseMenuWidget* Widget);

	// Opens or closes the single controller-managed gameplay menu.
	void OpenManagedMenu(UUserWidget* Widget, int32 ZOrder = 0);
	bool CloseManagedMenu(UUserWidget* Widget);

	UFUNCTION(BlueprintCallable, Category = "Boss|UI")
	void ShowBossHealthBar(ABossCharacter* Boss);

	UFUNCTION(BlueprintCallable, Category = "Boss|UI")
	void LeaveBossEncounter(ABossCharacter* Boss);

	void HideBossHealthBar();

	UFUNCTION(Client, Reliable)
	void ClientShowDeathScreen();

	UFUNCTION(BlueprintCallable, Category = "Death")
	void RequestRespawn();
	
	
	UFUNCTION(Client,Reliable)
	void ShowDamageNumber(float DamageAmount,ACharacter* TargetCharacter,bool bCriticalHit);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(EditDefaultsOnly, Category = "Boss|UI")
	TSubclassOf<UPlayerUserWidget> BossHealthBarClass;

	UPROPERTY()
	TObjectPtr<UPlayerUserWidget> BossHealthBarInstance;

	// Retained while the death screen is open so an active encounter can
	// restore its HUD after the player respawns.
	UPROPERTY()
	TWeakObjectPtr<ABossCharacter> ActiveBoss;

	UPROPERTY(EditDefaultsOnly, Category = "Death|UI")
	TSubclassOf<UUserWidget> DeathScreenClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> DeathScreenInstance;

	UPROPERTY(EditDefaultsOnly, Category = "Quest|UI")
	TSubclassOf<UPlayerUserWidget> QuestListWidgetClass;

	UPROPERTY()
	TObjectPtr<UPlayerUserWidget> QuestListWidgetInstance;

	UPROPERTY(EditDefaultsOnly, Category = "Party|UI")
	TSubclassOf<UPlayerUserWidget> PartyWidgetClass;

	UPROPERTY()
	TObjectPtr<UPlayerUserWidget> PartyWidgetInstance;

	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveManagedMenu;

	UPROPERTY(EditDefaultsOnly, Category = "Pause Menu|UI")
	TSubclassOf<UPauseMenuWidget> PauseMenuWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPauseMenuWidget> PauseMenuWidgetInstance;

	UPROPERTY(EditDefaultsOnly, Category = "Pause Menu|Travel")
	TSoftObjectPtr<UWorld> MainMenuLevel;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestRespawn();

	UFUNCTION(Server, Reliable)
	void ServerSwitchToPartySlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerRequestApplyPartySetup(
		const TArray<FGameplayTag>& NewParty, int32 NewActiveSlotIndex);

	UFUNCTION(Client, Reliable)
	void ClientPartySetupApplyResult(EPartySetupApplyResult Result);

	UFUNCTION(Client, Reliable)
	void ClientHideDeathScreen();

	void HandleRespawn();
	void PerformPartySwitch(int32 SlotIndex);
	EPartySetupApplyResult PerformPartySwitchToHero(FGameplayTag TargetHeroTag);
	EPartySetupApplyResult ProcessPartySetupRequest(
		const TArray<FGameplayTag>& NewParty, int32 NewActiveSlotIndex);
	FGameplayTag ResolveCurrentPawnHeroTag(
		const UPartyComponent* PartyComponent) const;
	void SwitchToPartySlot1();
	void SwitchToPartySlot2();
	void SwitchToPartySlot3();
	void SwitchToPartySlot4();
	void OpenManagedMenu(UPlayerUserWidget* Widget, UObject* DataSource);
	void RestoreGameplayInput();
	void ApplyPauseMenuInputState(UUserWidget* WidgetToFocus);
	void RestorePauseMenuInputState();
	bool IsGameplayInputBlockedByPauseMenu() const;
	void SetGameplayPausedForMenu(bool bPaused);
	void SetPauseMenuPage(EPauseMenuPage NewPage);
	void ResetPauseMenuState();
	void ExecuteConfirmedPauseMenuAction(EPauseMenuAction Action);

	UFUNCTION()
	void HandleDialogueFinished();

	UPROPERTY(EditDefaultsOnly, Category = "Dialogue")
	TSubclassOf<UDialogueWidget> DialogueWidgetClass;

	UPROPERTY()
	TObjectPtr<UDialogueWidget> ActiveDialogueWidget;

	UPROPERTY()
	TObjectPtr<AActor> ActiveInteractionSource;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> PlayerContext;

	// Alt-key input action.
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> AltAction;

	// Left-click input action.
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ClickAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> QuestLogAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> PartyAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> PauseMenuAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> PartySlot1Action;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> PartySlot2Action;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> PartySlot3Action;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> PartySlot4Action;
	
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UDamageTextComponent>DamageTextComponentClass;

	UPROPERTY(VisibleInstanceOnly, Category = "Pause Menu")
	EPauseMenuPage PauseMenuPage = EPauseMenuPage::Main;

	UPROPERTY(VisibleInstanceOnly, Category = "Pause Menu")
	EPauseMenuAction PendingPauseMenuAction = EPauseMenuAction::None;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> PauseMenuInputDisabledPawn;

	bool bPauseMenuStateBroadcastOpen = false;
	bool bPauseMenuInputStateCaptured = false;
	bool bPauseAppliedByPauseMenu = false;
	bool bGameplayInputDisabledByPauseMenu = false;
	bool bMoveInputIgnoredByPauseMenu = false;
	bool bLookInputIgnoredByPauseMenu = false;
	bool bMouseCursorVisibleBeforePauseMenu = false;

	UPROPERTY(Transient)
	bool bPartySetupRequestInFlight = false;
	
	
	
};
