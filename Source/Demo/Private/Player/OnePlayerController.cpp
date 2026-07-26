// OnePlayerController.cpp

#include "Player/OnePlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/Widget/DamageTextComponent.h"
#include "AbilitySystem/PlayerAttributeSet.h"
#include "AbilitySystem/Data/HeroUIInfo.h"
#include "Character/PlayerCharacter.h"
#include "Components/PartyComponent.h"
#include "Components/QuestComponent.h"
#include "Dialogue/DialogueDataAsset.h"
#include "interaction/InteractableInterface.h"
#include "UI/Widget/PlayerUserWidget.h"
#include "Character/BossCharacter.h"
#include "UI/Dialogue/DialogueWidget.h"
#include "UI/Menu/PauseMenuWidget.h"
#include "UI/Party/PartySetupWidget.h"
#include "Player/OPlayerState.h"
#include "Components/SkeletalMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogPauseMenu, Log, All);

AOnePlayerController::AOnePlayerController()
{
    bReplicates = true;
    DialogueWidgetClass = UDialogueWidget::StaticClass();
}
void AOnePlayerController::ShowBossHealthBar(ABossCharacter* Boss)
{
    if (!IsLocalController() || !Boss || !BossHealthBarClass) return;

    ActiveBoss = Boss;

    if (!BossHealthBarInstance)
    {
        BossHealthBarInstance =
            CreateWidget<UPlayerUserWidget>(this, BossHealthBarClass);
    }

    if (BossHealthBarInstance)
    {
        if (!BossHealthBarInstance->IsInViewport())
        {
            BossHealthBarInstance->AddToViewport(50);
        }
        BossHealthBarInstance->SetWidgetController(Boss);

        if (const UPlayerAttributeSet* BossAttributes =
            Cast<UPlayerAttributeSet>(Boss->GetAttributeSet()))
        {
            Boss->OnHealthChanged.Broadcast(BossAttributes->GetHealth());
            Boss->OnMaxHealthChanged.Broadcast(BossAttributes->GetMaxHealth());
        }
    }
}

void AOnePlayerController::HideBossHealthBar()
{
    if (BossHealthBarInstance)
    {
        BossHealthBarInstance->RemoveFromParent();
    }
}

void AOnePlayerController::LeaveBossEncounter(ABossCharacter* Boss)
{
    if (!IsLocalController() || !Boss || ActiveBoss.Get() != Boss) return;

    ActiveBoss.Reset();
    HideBossHealthBar();
}

void AOnePlayerController::ClientShowDeathScreen_Implementation()
{
    if (!IsLocalController()) return;

    HideBossHealthBar();
    if (!DeathScreenClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("%s requires DeathScreenClass to display the respawn UI."),
            *GetName());
        return;
    }

    if (!DeathScreenInstance)
    {
        DeathScreenInstance = CreateWidget<UUserWidget>(this, DeathScreenClass);
    }
    if (!DeathScreenInstance) return;

    if (ActiveDialogueWidget) CloseDialogue();
    if (ActiveManagedMenu) CloseManagedMenu(ActiveManagedMenu);

    if (!DeathScreenInstance->IsInViewport())
    {
        DeathScreenInstance->AddToViewport(200);
    }

    FInputModeUIOnly InputMode;
    SetInputMode(InputMode);
    SetIgnoreMoveInput(true);
    SetIgnoreLookInput(true);
    bShowMouseCursor = true;
}

void AOnePlayerController::RequestRespawn()
{
    if (HasAuthority())
    {
        HandleRespawn();
    }
    else
    {
        ServerRequestRespawn();
    }
}

void AOnePlayerController::ServerRequestRespawn_Implementation()
{
    HandleRespawn();
}

void AOnePlayerController::HandleRespawn()
{
    APlayerCharacter* DeadCharacter = Cast<APlayerCharacter>(GetPawn());
    if (!DeadCharacter || !DeadCharacter->IsDeathSequenceStarted()) return;

    UAbilitySystemComponent* PlayerASC = DeadCharacter->GetAbilitySystemComponent();
    if (PlayerASC)
    {
        PlayerASC->CancelAllAbilities();
    }

    APawn* PawnToDestroy = GetPawn();
    UnPossess();
    if (IsValid(PawnToDestroy))
    {
        PawnToDestroy->Destroy();
    }

    if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AGameModeBase>())
    {
        GameMode->RestartPlayer(this);
    }

    if (PlayerASC)
    {
        const float MaxHealth = PlayerASC->GetNumericAttribute(
            UPlayerAttributeSet::GetMaxHealthAttribute());
        PlayerASC->SetNumericAttributeBase(
            UPlayerAttributeSet::GetHealthAttribute(), MaxHealth);
    }

    ClientHideDeathScreen();
}

void AOnePlayerController::ClientHideDeathScreen_Implementation()
{
    if (DeathScreenInstance)
    {
        DeathScreenInstance->RemoveFromParent();
    }

    SetIgnoreMoveInput(false);
    SetIgnoreLookInput(false);
    bShowMouseCursor = false;
    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);

    if (ABossCharacter* Boss = ActiveBoss.Get())
    {
        if (Boss->BossPhase != EBossPhase::Dormant &&
            Boss->BossPhase != EBossPhase::Dead)
        {
            ShowBossHealthBar(Boss);
        }
    }
}

void AOnePlayerController::ToggleMouseCursor()
{
    // Modal UI owns cursor visibility and input routing until it closes.
    if ((ActiveManagedMenu && ActiveManagedMenu->IsInViewport()) ||
        ActiveDialogueWidget ||
        (DeathScreenInstance && DeathScreenInstance->IsInViewport()))
    {
        return;
    }

    bShowMouseCursor = !bShowMouseCursor;

    // Switch input routing with cursor visibility.
    if (bShowMouseCursor)
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetHideCursorDuringCapture(false);
        SetInputMode(InputMode);
    }
    else
    {
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
    }
}

void AOnePlayerController::OnClickScreen()
{
    // Clicking the viewport returns focus to the game only when the cursor is visible.
    if (bShowMouseCursor)
    {
        ToggleMouseCursor();
    }
}

void AOnePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
    {
        if (AltAction)
        {
            EnhancedInputComponent->BindAction(AltAction, ETriggerEvent::Started, this, &AOnePlayerController::ToggleMouseCursor);
        }

        if (ClickAction)
        {
            EnhancedInputComponent->BindAction(ClickAction, ETriggerEvent::Started, this, &AOnePlayerController::OnClickScreen);
        }

        if (QuestLogAction)
        {
            EnhancedInputComponent->BindAction(
                QuestLogAction, ETriggerEvent::Started,
                this, &AOnePlayerController::ToggleQuestList);
        }

        if (PartyAction)
        {
            EnhancedInputComponent->BindAction(
                PartyAction, ETriggerEvent::Started,
                this, &AOnePlayerController::TogglePartyPage);
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("%s requires PartyAction to be assigned."),
                *GetName());
        }

        if (PauseMenuAction)
        {
            EnhancedInputComponent->BindAction(
                PauseMenuAction, ETriggerEvent::Started,
                this, &AOnePlayerController::TogglePauseMenu);
        }
        else
        {
            UE_LOG(LogPauseMenu, Error,
                TEXT("%s requires PauseMenuAction to be assigned."),
                *GetName());
        }

        if (PartySlot1Action)
        {
            EnhancedInputComponent->BindAction(
                PartySlot1Action, ETriggerEvent::Started,
                this, &AOnePlayerController::SwitchToPartySlot1);
        }
        if (PartySlot2Action)
        {
            EnhancedInputComponent->BindAction(
                PartySlot2Action, ETriggerEvent::Started,
                this, &AOnePlayerController::SwitchToPartySlot2);
        }
        if (PartySlot3Action)
        {
            EnhancedInputComponent->BindAction(
                PartySlot3Action, ETriggerEvent::Started,
                this, &AOnePlayerController::SwitchToPartySlot3);
        }
        if (PartySlot4Action)
        {
            EnhancedInputComponent->BindAction(
                PartySlot4Action, ETriggerEvent::Started,
                this, &AOnePlayerController::SwitchToPartySlot4);
        }
    }

}

void AOnePlayerController::SwitchToPartySlot1() { SwitchToPartySlot(0); }
void AOnePlayerController::SwitchToPartySlot2() { SwitchToPartySlot(1); }
void AOnePlayerController::SwitchToPartySlot3() { SwitchToPartySlot(2); }
void AOnePlayerController::SwitchToPartySlot4() { SwitchToPartySlot(3); }

void AOnePlayerController::SwitchToPartySlot(int32 SlotIndex)
{
    if (!IsLocalController() || IsGameplayInputBlockedByPauseMenu() ||
        IsPaused() || ActiveDialogueWidget ||
        (ActiveManagedMenu && ActiveManagedMenu->IsInViewport()))
    {
        return;
    }

    if (HasAuthority())
    {
        PerformPartySwitch(SlotIndex);
    }
    else
    {
        ServerSwitchToPartySlot(SlotIndex);
    }
}

void AOnePlayerController::ServerSwitchToPartySlot_Implementation(int32 SlotIndex)
{
    PerformPartySwitch(SlotIndex);
}

void AOnePlayerController::PerformPartySwitch(int32 SlotIndex)
{
    AOPlayerState* OPlayerState = GetPlayerState<AOPlayerState>();
    UPartyComponent* PartyComponent = OPlayerState
        ? OPlayerState->GetPartyComponent()
        : nullptr;
    if (!PartyComponent) return;

    const TArray<FPartySlotViewData> PartySlots = PartyComponent->GetPartySlots();
    if (!PartySlots.IsValidIndex(SlotIndex) ||
        !PartySlots[SlotIndex].bOccupied)
    {
        return;
    }

    TArray<FGameplayTag> CurrentParty;
    CurrentParty.Reserve(PartySlots.Num());
    for (const FPartySlotViewData& Slot : PartySlots)
    {
        CurrentParty.Add(Slot.bOccupied
            ? Slot.HeroInfo.HeroTag
            : FGameplayTag());
    }
    if (PartyComponent->ValidatePartySetup(CurrentParty, SlotIndex) !=
        EPartySetupApplyResult::Success)
    {
        return;
    }

    const FGameplayTag TargetHeroTag = PartySlots[SlotIndex].HeroInfo.HeroTag;
    const FGameplayTag CurrentHeroTag =
        ResolveCurrentPawnHeroTag(PartyComponent);
    if (CurrentHeroTag != TargetHeroTag &&
        PerformPartySwitchToHero(TargetHeroTag) !=
            EPartySetupApplyResult::Success)
    {
        return;
    }
    if (!PartyComponent->ApplyPartySetup(CurrentParty, SlotIndex))
    {
        UE_LOG(LogTemp, Error,
            TEXT("Failed to update the active party slot after switching."));
    }
}

EPartySetupApplyResult AOnePlayerController::PerformPartySwitchToHero(
    FGameplayTag TargetHeroTag)
{
    AOPlayerState* OPlayerState = GetPlayerState<AOPlayerState>();
    UPartyComponent* PartyComponent = OPlayerState
        ? OPlayerState->GetPartyComponent()
        : nullptr;
    FHeroSlotInfo TargetHeroInfo;
    if (!PartyComponent ||
        !PartyComponent->FindHeroInfo(TargetHeroTag, TargetHeroInfo))
    {
        return EPartySetupApplyResult::UnknownHero;
    }

    const TSubclassOf<APlayerCharacter> CharacterClass =
        TargetHeroInfo.CharacterClass.LoadSynchronous();
    if (!CharacterClass)
    {
        return EPartySetupApplyResult::MissingCharacterClass;
    }

    APlayerCharacter* CurrentCharacter = Cast<APlayerCharacter>(GetPawn());
    const FTransform SpawnTransform = CurrentCharacter
        ? CurrentCharacter->GetActorTransform()
        : FTransform::Identity;
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.Instigator = CurrentCharacter;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    APlayerCharacter* NewCharacter = GetWorld()->SpawnActor<APlayerCharacter>(
        CharacterClass, SpawnTransform, SpawnParameters);
    if (!NewCharacter)
    {
        return EPartySetupApplyResult::SpawnFailed;
    }

    APawn* PreviousPawn = GetPawn();
    Possess(NewCharacter);

    if (GetPawn() != NewCharacter || NewCharacter->GetController() != this)
    {
        if (IsValid(PreviousPawn) && GetPawn() != PreviousPawn)
        {
            Possess(PreviousPawn);
        }
        NewCharacter->Destroy();
        return EPartySetupApplyResult::PossessFailed;
    }

    if (USkeletalMeshComponent* CharacterMesh = NewCharacter->GetMesh())
    {
        CharacterMesh->InitAnim(true);
    }
    if (IsValid(PreviousPawn)) PreviousPawn->Destroy();

    return EPartySetupApplyResult::Success;
}

FGameplayTag AOnePlayerController::ResolveCurrentPawnHeroTag(
    const UPartyComponent* PartyComponent) const
{
    const APlayerCharacter* CurrentCharacter =
        Cast<APlayerCharacter>(GetPawn());
    if (!PartyComponent || !CurrentCharacter) return FGameplayTag();

    auto MatchesCurrentPawn = [CurrentCharacter](const FHeroSlotInfo& HeroInfo)
    {
        const UClass* CharacterClass =
            HeroInfo.CharacterClass.LoadSynchronous();
        return CharacterClass && CurrentCharacter->GetClass() == CharacterClass;
    };

    FHeroSlotInfo ActiveHeroInfo;
    const FGameplayTag ActiveHeroTag = PartyComponent->GetActiveHeroTag();
    if (PartyComponent->FindHeroInfo(ActiveHeroTag, ActiveHeroInfo) &&
        MatchesCurrentPawn(ActiveHeroInfo))
    {
        return ActiveHeroTag;
    }

    for (const FHeroSlotInfo& HeroInfo : PartyComponent->GetUnlockedHeroInfo())
    {
        if (MatchesCurrentPawn(HeroInfo)) return HeroInfo.HeroTag;
    }
    return FGameplayTag();
}

void AOnePlayerController::RequestApplyPartySetup(
    const TArray<FGameplayTag>& NewParty, int32 NewActiveSlotIndex)
{
    if (!IsLocalController()) return;
    if (bPartySetupRequestInFlight)
    {
        OnPartySetupApplyResult.Broadcast(
            EPartySetupApplyResult::AlreadyProcessing);
        return;
    }

    bPartySetupRequestInFlight = true;
    if (HasAuthority())
    {
        const EPartySetupApplyResult Result =
            ProcessPartySetupRequest(NewParty, NewActiveSlotIndex);
        bPartySetupRequestInFlight = false;
        OnPartySetupApplyResult.Broadcast(Result);
    }
    else
    {
        ServerRequestApplyPartySetup(NewParty, NewActiveSlotIndex);
    }
}

void AOnePlayerController::ServerRequestApplyPartySetup_Implementation(
    const TArray<FGameplayTag>& NewParty, int32 NewActiveSlotIndex)
{
    if (bPartySetupRequestInFlight)
    {
        ClientPartySetupApplyResult(
            EPartySetupApplyResult::AlreadyProcessing);
        return;
    }

    bPartySetupRequestInFlight = true;
    const EPartySetupApplyResult Result =
        ProcessPartySetupRequest(NewParty, NewActiveSlotIndex);
    bPartySetupRequestInFlight = false;
    ClientPartySetupApplyResult(Result);
}

void AOnePlayerController::ClientPartySetupApplyResult_Implementation(
    EPartySetupApplyResult Result)
{
    bPartySetupRequestInFlight = false;
    OnPartySetupApplyResult.Broadcast(Result);
}

EPartySetupApplyResult AOnePlayerController::ProcessPartySetupRequest(
    const TArray<FGameplayTag>& NewParty, int32 NewActiveSlotIndex)
{
    AOPlayerState* OPlayerState = GetPlayerState<AOPlayerState>();
    UPartyComponent* PartyComponent = OPlayerState
        ? OPlayerState->GetPartyComponent()
        : nullptr;
    if (!PartyComponent)
    {
        return EPartySetupApplyResult::InvalidParty;
    }

    const EPartySetupApplyResult ValidationResult =
        PartyComponent->ValidatePartySetup(NewParty, NewActiveSlotIndex);
    if (ValidationResult != EPartySetupApplyResult::Success)
    {
        return ValidationResult;
    }

    const FGameplayTag TargetHeroTag = NewParty[NewActiveSlotIndex];
    const FGameplayTag CurrentHeroTag =
        ResolveCurrentPawnHeroTag(PartyComponent);
    if (CurrentHeroTag != TargetHeroTag)
    {
        const EPartySetupApplyResult SwitchResult =
            PerformPartySwitchToHero(TargetHeroTag);
        if (SwitchResult != EPartySetupApplyResult::Success)
        {
            return SwitchResult;
        }
    }

    if (!PartyComponent->ApplyPartySetup(NewParty, NewActiveSlotIndex))
    {
        UE_LOG(LogTemp, Error,
            TEXT("Validated party setup failed during authoritative apply."));
        return EPartySetupApplyResult::InvalidParty;
    }
    return EPartySetupApplyResult::Success;
}

void AOnePlayerController::ToggleQuestList()
{
    if (!IsLocalController() || IsGameplayInputBlockedByPauseMenu()) return;

    if (QuestListWidgetInstance && QuestListWidgetInstance->IsInViewport())
    {
        CloseManagedMenu(QuestListWidgetInstance);
        return;
    }

    AOPlayerState* OPlayerState = GetPlayerState<AOPlayerState>();
    if (!OPlayerState || !OPlayerState->GetQuestComponent()) return;
    if (!QuestListWidgetClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("%s requires QuestListWidgetClass."), *GetName());
        return;
    }

    if (!QuestListWidgetInstance)
    {
        QuestListWidgetInstance =
            CreateWidget<UPlayerUserWidget>(this, QuestListWidgetClass);
    }
    OpenManagedMenu(QuestListWidgetInstance, OPlayerState->GetQuestComponent());
}

void AOnePlayerController::TogglePartyPage()
{
    if (!IsLocalController() || IsGameplayInputBlockedByPauseMenu()) return;

    if (PartyWidgetInstance && PartyWidgetInstance->IsInViewport())
    {
        if (UPartySetupWidget* PartySetupWidget =
            Cast<UPartySetupWidget>(PartyWidgetInstance))
        {
            PartySetupWidget->RequestCancel();
        }
        else
        {
            CloseManagedMenu(PartyWidgetInstance);
        }
        return;
    }

    AOPlayerState* OPlayerState = GetPlayerState<AOPlayerState>();
    if (!OPlayerState || !OPlayerState->GetPartyComponent()) return;
    if (!PartyWidgetClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("%s requires PartyWidgetClass."), *GetName());
        return;
    }

    if (!PartyWidgetInstance)
    {
        PartyWidgetInstance =
            CreateWidget<UPlayerUserWidget>(this, PartyWidgetClass);
    }
    OpenManagedMenu(PartyWidgetInstance, OPlayerState->GetPartyComponent());
}

void AOnePlayerController::TogglePauseMenu()
{
    if (!IsLocalController()) return;

    if (!IsPauseMenuOpen())
    {
        OpenPauseMenu();
        return;
    }

    if (PauseMenuPage != EPauseMenuPage::Main)
    {
        PendingPauseMenuAction = EPauseMenuAction::None;
        SetPauseMenuPage(EPauseMenuPage::Main);
        return;
    }

    ClosePauseMenu();
}

void AOnePlayerController::OpenPauseMenu()
{
    if (!IsLocalController() || IsPauseMenuOpen()) return;
    if (DeathScreenInstance && DeathScreenInstance->IsInViewport()) return;
    if (ActiveDialogueWidget)
    {
        UE_LOG(LogPauseMenu, Log,
            TEXT("%s rejected pause menu open because a dialogue is active."),
            *GetName());
        return;
    }

    if (!PauseMenuWidgetClass)
    {
        UE_LOG(LogPauseMenu, Error,
            TEXT("%s requires PauseMenuWidgetClass."), *GetName());
        return;
    }

    if (!PauseMenuWidgetInstance)
    {
        PauseMenuWidgetInstance =
            CreateWidget<UPauseMenuWidget>(this, PauseMenuWidgetClass);
    }
    if (!PauseMenuWidgetInstance) return;

    OpenManagedMenu(PauseMenuWidgetInstance, 150);
    if (ActiveManagedMenu != PauseMenuWidgetInstance) return;

    PauseMenuPage = EPauseMenuPage::Main;
    PendingPauseMenuAction = EPauseMenuAction::None;
    bPauseMenuStateBroadcastOpen = true;
    OnPauseMenuStateChanged.Broadcast(true);
    OnPauseMenuPageChanged.Broadcast(PauseMenuPage);
}

void AOnePlayerController::ClosePauseMenu()
{
    if (!IsPauseMenuOpen()) return;
    CloseManagedMenu(PauseMenuWidgetInstance);
}

bool AOnePlayerController::IsPauseMenuOpen() const
{
    return PauseMenuWidgetInstance &&
        ActiveManagedMenu == PauseMenuWidgetInstance &&
        PauseMenuWidgetInstance->IsInViewport();
}

void AOnePlayerController::RequestPauseMenuAction(EPauseMenuAction Action)
{
    if (!IsLocalController() || !IsPauseMenuOpen()) return;

    switch (Action)
    {
    case EPauseMenuAction::Continue:
        ClosePauseMenu();
        break;
    case EPauseMenuAction::OpenSettings:
        PendingPauseMenuAction = EPauseMenuAction::None;
        SetPauseMenuPage(EPauseMenuPage::Settings);
        break;
    case EPauseMenuAction::ReturnToPauseMenu:
        PendingPauseMenuAction = EPauseMenuAction::None;
        SetPauseMenuPage(EPauseMenuPage::Main);
        break;
    case EPauseMenuAction::ReturnToMainMenu:
    case EPauseMenuAction::ExitGame:
        PendingPauseMenuAction = Action;
        SetPauseMenuPage(EPauseMenuPage::Confirmation);
        OnPauseMenuConfirmationRequested.Broadcast(Action);
        break;
    case EPauseMenuAction::None:
    default:
        break;
    }
}

void AOnePlayerController::ConfirmPauseMenuAction(bool bConfirmed)
{
    if (!IsLocalController() || !IsPauseMenuOpen() ||
        PendingPauseMenuAction == EPauseMenuAction::None)
    {
        return;
    }

    const EPauseMenuAction ConfirmedAction = PendingPauseMenuAction;
    PendingPauseMenuAction = EPauseMenuAction::None;

    if (!bConfirmed)
    {
        SetPauseMenuPage(EPauseMenuPage::Main);
        return;
    }

    ExecuteConfirmedPauseMenuAction(ConfirmedAction);
}

void AOnePlayerController::SetPauseMenuPage(EPauseMenuPage NewPage)
{
    if (!IsPauseMenuOpen() || PauseMenuPage == NewPage) return;

    PauseMenuPage = NewPage;
    OnPauseMenuPageChanged.Broadcast(PauseMenuPage);
}

void AOnePlayerController::ExecuteConfirmedPauseMenuAction(
    EPauseMenuAction Action)
{
    if (Action == EPauseMenuAction::ReturnToMainMenu)
    {
        if (MainMenuLevel.IsNull())
        {
            UE_LOG(LogPauseMenu, Error,
                TEXT("%s requires MainMenuLevel before returning to menu."),
                *GetName());
            SetPauseMenuPage(EPauseMenuPage::Main);
            return;
        }

        const FString MainMenuPackageName =
            MainMenuLevel.ToSoftObjectPath().GetLongPackageName();
        ClosePauseMenu();
        ClientTravel(MainMenuPackageName, TRAVEL_Absolute);
        return;
    }

    if (Action == EPauseMenuAction::ExitGame)
    {
        ClosePauseMenu();
        UKismetSystemLibrary::QuitGame(
            this, this, EQuitPreference::Quit, false);
    }
}

void AOnePlayerController::OpenManagedMenu(
    UPlayerUserWidget* Widget, UObject* DataSource)
{
    if (!Widget || !DataSource) return;

    Widget->SetWidgetController(DataSource);
    OpenManagedMenu(static_cast<UUserWidget*>(Widget), 100);
}

void AOnePlayerController::OpenManagedMenu(UUserWidget* Widget, int32 ZOrder)
{
    if (!Widget) return;

    if (ActiveDialogueWidget)
    {
        UE_LOG(LogPauseMenu, Verbose,
            TEXT("%s rejected managed menu '%s' because a dialogue is active."),
            *GetName(), *GetNameSafe(Widget));
        return;
    }
    if (ActiveManagedMenu)
    {
        if (ActiveManagedMenu == Widget && Widget->IsInViewport()) return;
        CloseManagedMenu(ActiveManagedMenu);
    }

    if (!Widget->IsInViewport()) Widget->AddToViewport(ZOrder);
    ActiveManagedMenu = Widget;

    if (Widget == PauseMenuWidgetInstance)
    {
        ApplyPauseMenuInputState(Widget);
    }
    else
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(Widget->TakeWidget());
        InputMode.SetHideCursorDuringCapture(false);
        SetInputMode(InputMode);
        bShowMouseCursor = true;
        SetIgnoreMoveInput(true);
        SetIgnoreLookInput(true);
        SetGameplayPausedForMenu(true);
    }
}

bool AOnePlayerController::CloseManagedMenu(UUserWidget* Widget)
{
    if (!Widget || Widget != ActiveManagedMenu) return false;

    const bool bClosingPauseMenu = Widget == PauseMenuWidgetInstance;
    if (bClosingPauseMenu)
    {
        // Broadcast while the widget is still attached and subscribed.
        ResetPauseMenuState();
    }

    ActiveManagedMenu = nullptr;
    Widget->RemoveFromParent();
    if (bClosingPauseMenu)
    {
        RestorePauseMenuInputState();
    }
    else
    {
        RestoreGameplayInput();
    }
    return true;
}

void AOnePlayerController::NotifyPauseMenuWidgetRemoved(
    UPauseMenuWidget* Widget)
{
    if (!Widget || Widget != PauseMenuWidgetInstance ||
        ActiveManagedMenu != Widget)
    {
        return;
    }

    ResetPauseMenuState();
    ActiveManagedMenu = nullptr;
    RestorePauseMenuInputState();
}

void AOnePlayerController::ResetPauseMenuState()
{
    PauseMenuPage = EPauseMenuPage::Main;
    PendingPauseMenuAction = EPauseMenuAction::None;

    if (!bPauseMenuStateBroadcastOpen) return;

    bPauseMenuStateBroadcastOpen = false;
    OnPauseMenuStateChanged.Broadcast(false);
}

void AOnePlayerController::ApplyPauseMenuInputState(
    UUserWidget* WidgetToFocus)
{
    if (bPauseMenuInputStateCaptured) return;

    bPauseMenuInputStateCaptured = true;
    bMouseCursorVisibleBeforePauseMenu = bShowMouseCursor;

    FInputModeGameAndUI InputMode;
    if (WidgetToFocus)
    {
        InputMode.SetWidgetToFocus(WidgetToFocus->TakeWidget());
    }
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    SetInputMode(InputMode);
    bShowMouseCursor = true;

    if (!IsMoveInputIgnored())
    {
        SetIgnoreMoveInput(true);
        bMoveInputIgnoredByPauseMenu = true;
    }
    if (!IsLookInputIgnored())
    {
        SetIgnoreLookInput(true);
        bLookInputIgnoredByPauseMenu = true;
    }

    if (APawn* ControlledPawn = GetPawn();
        ControlledPawn && ControlledPawn->InputEnabled())
    {
        ControlledPawn->DisableInput(this);
        if (!ControlledPawn->InputEnabled())
        {
            PauseMenuInputDisabledPawn = ControlledPawn;
            bGameplayInputDisabledByPauseMenu = true;
        }
    }

    if (GetNetMode() == NM_Standalone)
    {
        if (IsPaused())
        {
            UE_LOG(LogPauseMenu, Verbose,
                TEXT("%s opened while the world was already paused; "
                     "the pause menu will not own that pause."),
                *GetName());
        }
        else
        {
            bPauseAppliedByPauseMenu = SetPause(true);
            if (!bPauseAppliedByPauseMenu)
            {
                UE_LOG(LogPauseMenu, Error,
                    TEXT("%s could not pause the standalone world; "
                         "local gameplay input remains blocked."),
                    *GetName());
            }
        }
    }

    UE_LOG(LogPauseMenu, Log,
        TEXT("%s captured pause-menu input state "
             "(PawnInput=%s, StandalonePause=%s)."),
        *GetName(),
        bGameplayInputDisabledByPauseMenu ? TEXT("blocked") : TEXT("unchanged"),
        bPauseAppliedByPauseMenu ? TEXT("owned") : TEXT("not-owned"));
}

void AOnePlayerController::RestorePauseMenuInputState()
{
    if (!bPauseMenuInputStateCaptured) return;

    if (bPauseAppliedByPauseMenu)
    {
        if (!SetPause(false))
        {
            UE_LOG(LogPauseMenu, Warning,
                TEXT("%s failed to release the pause owned by the pause menu."),
                *GetName());
        }
        bPauseAppliedByPauseMenu = false;
    }

    if (bGameplayInputDisabledByPauseMenu)
    {
        if (APawn* DisabledPawn = PauseMenuInputDisabledPawn.Get())
        {
            DisabledPawn->EnableInput(this);
        }
        PauseMenuInputDisabledPawn.Reset();
        bGameplayInputDisabledByPauseMenu = false;
    }

    if (bMoveInputIgnoredByPauseMenu)
    {
        SetIgnoreMoveInput(false);
        bMoveInputIgnoredByPauseMenu = false;
    }
    if (bLookInputIgnoredByPauseMenu)
    {
        SetIgnoreLookInput(false);
        bLookInputIgnoredByPauseMenu = false;
    }

    bShowMouseCursor = bMouseCursorVisibleBeforePauseMenu;
    if (bShowMouseCursor)
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetHideCursorDuringCapture(false);
        SetInputMode(InputMode);
    }
    else
    {
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
    }

    bPauseMenuInputStateCaptured = false;

    UE_LOG(LogPauseMenu, Log,
        TEXT("%s restored the input state owned by the pause menu."),
        *GetName());
}

bool AOnePlayerController::IsGameplayInputBlockedByPauseMenu() const
{
    return bPauseMenuInputStateCaptured;
}

void AOnePlayerController::RestoreGameplayInput()
{
    SetGameplayPausedForMenu(false);
    SetIgnoreMoveInput(false);
    SetIgnoreLookInput(false);
    bShowMouseCursor = false;
    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);
}

void AOnePlayerController::SetGameplayPausedForMenu(bool bPaused)
{
    if (GetNetMode() == NM_Standalone)
    {
        SetPause(bPaused);
    }
}

void AOnePlayerController::OpenDialogue(UDialogueDataAsset* DialogueData, AActor* InteractionSource)
{
    if (!IsLocalController() || !DialogueData || !DialogueWidgetClass || ActiveDialogueWidget) return;

    ActiveDialogueWidget = CreateWidget<UDialogueWidget>(this, DialogueWidgetClass);
    if (!ActiveDialogueWidget) return;

    if (ActiveManagedMenu) CloseManagedMenu(ActiveManagedMenu);

    ActiveInteractionSource = InteractionSource;
    ActiveDialogueWidget->OnDialogueFinished.AddDynamic(this, &AOnePlayerController::HandleDialogueFinished);
    ActiveDialogueWidget->AddToViewport(100);

    FInputModeGameAndUI InputMode;
    InputMode.SetWidgetToFocus(ActiveDialogueWidget->TakeWidget());
    InputMode.SetHideCursorDuringCapture(false);
    SetInputMode(InputMode);
    bShowMouseCursor = true;
    SetIgnoreMoveInput(true);
    SetIgnoreLookInput(true);
    SetGameplayPausedForMenu(true);
    ActiveDialogueWidget->InitializeDialogue(DialogueData);
}

void AOnePlayerController::CloseDialogue()
{
    if (!ActiveDialogueWidget) return;

    ActiveDialogueWidget->RemoveFromParent();
    ActiveDialogueWidget = nullptr;

    ActiveInteractionSource = nullptr;
    SetGameplayPausedForMenu(false);
    SetIgnoreMoveInput(false);
    SetIgnoreLookInput(false);
    bShowMouseCursor = false;
    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);
}

void AOnePlayerController::HandleDialogueFinished()
{
    AActor* FinishedSource = ActiveInteractionSource;
    APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetPawn());
    CloseDialogue();

    if (IsValid(FinishedSource) && FinishedSource->Implements<UInteractableInterface>())
    {
        IInteractableInterface::Execute_OnInteractionFinished(FinishedSource, PlayerCharacter);
    }
}

void AOnePlayerController::ShowDamageNumber_Implementation(float DamageAmount, ACharacter* TargetCharacter, bool bCriticalHit)
{
    if (IsValid(TargetCharacter) && DamageTextComponentClass)
    {
        UDamageTextComponent* DamageText = NewObject<UDamageTextComponent>(TargetCharacter, DamageTextComponentClass);
        DamageText->RegisterComponent();
        DamageText->AttachToComponent(TargetCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        DamageText->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        DamageText->DamageText(DamageAmount, bCriticalHit);
    }
}

void AOnePlayerController::BeginPlay()
{
    Super::BeginPlay();

    bShowMouseCursor = false;

    FInputModeGameOnly InputModeData;
    InputModeData.SetConsumeCaptureMouseDown(true);
    SetInputMode(InputModeData);

    // Install the gameplay mapping context for the local player.
    if (PlayerContext)
    {
        if (ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player))
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
            {
                Subsystem->AddMappingContext(PlayerContext, 0);
            }
        }
    }
}

void AOnePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    const bool bPauseMenuWasActive =
        ActiveManagedMenu == PauseMenuWidgetInstance;
    if (bPauseMenuWasActive || bPauseMenuStateBroadcastOpen)
    {
        ResetPauseMenuState();
    }
    if (bPauseMenuWasActive)
    {
        ActiveManagedMenu = nullptr;
    }
    RestorePauseMenuInputState();

    if (PauseMenuWidgetInstance)
    {
        PauseMenuWidgetInstance->RemoveFromParent();
        PauseMenuWidgetInstance = nullptr;
    }

    if (PlayerContext)
    {
        if (ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player))
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
                    LocalPlayer))
            {
                Subsystem->RemoveMappingContext(PlayerContext);
            }
        }
    }

    PauseMenuPage = EPauseMenuPage::Main;
    PendingPauseMenuAction = EPauseMenuAction::None;
    Super::EndPlay(EndPlayReason);
}
