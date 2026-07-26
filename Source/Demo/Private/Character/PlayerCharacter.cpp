#include "Character/PlayerCharacter.h"

// Core character dependencies.
#include "GameFramework/SpringArmComponent.h" 
#include "Camera/CameraComponent.h"           
#include "AbilitySystem/PlayerAbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h" 
#include "GameFramework/Controller.h"         
#include "EnhancedInputComponent.h"           
#include "AbilitySystemComponent.h"
#include "Player/OnePlayerController.h"
#include "AbilitySystem/PlayerAbilitySystemLibrary.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "InputActionValue.h"                 
#include "Player/OPlayerState.h"              
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/HUD/PlayerHUD.h"
#include "Blueprint/UserWidget.h"
#include "Components/ItemPickup.h"
#include "Components/InventoryComponent.h" 
#include "Components/QuestComponent.h"
#include "UI/Inventory/InventoryPanelWidget.h"
#include "input/PlayerInputComponent.h" 
#include "interaction/EnemyInterface.h" 
#include "interaction/InteractableInterface.h"

void APlayerCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    InitAbilityActorInfo();
}

APlayerCharacter::APlayerCharacter()
{
    // Face movement direction independently of camera rotation.
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    GetCharacterMovement()->bOrientRotationToMovement = true; 
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); 
    GetCharacterMovement()->JumpZVelocity = 700.f; 
    GetCharacterMovement()->AirControl = 0.35f;    

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 400.0f; 
    CameraBoom->bUsePawnControlRotation = true; 

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); 
    FollowCamera->bUsePawnControlRotation = false; 

}

void APlayerCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitAbilityActorInfo();
}

void APlayerCharacter::OnRep_Controller()
{
    Super::OnRep_Controller();
    InitAbilityActorInfo();
}

UAbilitySystemComponent* APlayerCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

UQuestComponent* APlayerCharacter::GetQuestComponent() const
{
    if (const AOPlayerState* OPlayerState = GetPlayerState<AOPlayerState>())
    {
        return OPlayerState->GetQuestComponent();
    }
    return nullptr;
}

UInventoryComponent* APlayerCharacter::GetInventoryComponent() const
{
	return FindComponentByClass<UInventoryComponent>();
}

int32 APlayerCharacter::GetPlayerLevel()
{
    const AOPlayerState* OPlayerState = GetPlayerState<AOPlayerState>();
    check(OPlayerState)
    return OPlayerState->GetPlayerLevel();
}

void APlayerCharacter::InitAbilityActorInfo()
{
    AOPlayerState* OPlayerState = GetPlayerState<AOPlayerState>();
    if (!OPlayerState)
    {
        return;
    }
    
    OPlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(OPlayerState, this);
    Cast<UPlayerAbilitySystemComponent>(OPlayerState->GetAbilitySystemComponent())->AbilityActorInfoSet();
    
    AbilitySystemComponent = OPlayerState->GetAbilitySystemComponent();
    AttributeSet = OPlayerState->GetAttributeSet();
    
    // ==========================================
    // Initialize attributes before the overlay reads them.
    // ==========================================
    InitialzeDefaultAttributes();
    
    // Also apply VitalAttributes from CharacterClassInfo if available
    if (UCharacterClassInfo* ClassInfo = UPlayerAbilitySystemLibrary::GetCharacterClassInfo(this))
    {
        if (ClassInfo->VitalAttributes)
        {
            FGameplayEffectContextHandle VitalCtx = AbilitySystemComponent->MakeEffectContext();
            VitalCtx.AddSourceObject(this);
            FGameplayEffectSpecHandle VitalSpec = AbilitySystemComponent->MakeOutgoingSpec(
                ClassInfo->VitalAttributes, 
                static_cast<float>(GetPlayerLevel()), 
                VitalCtx);
            AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*VitalSpec.Data.Get());
		}
    } 
    
    // ==========================================
    // Grant startup abilities, including the primary attack.
    // ==========================================
    if (UPlayerAbilitySystemComponent* PlayerASC = Cast<UPlayerAbilitySystemComponent>(AbilitySystemComponent))
    {
		PlayerASC->SetCharacterAbilities(StartupAbilities);
    }

    if (AOnePlayerController* OnePlayerController = Cast<AOnePlayerController>(GetController()))
    {
        if (APlayerHUD* PlayerHUD = Cast<APlayerHUD>(OnePlayerController->GetHUD()))
        {
            PlayerHUD->InitOverlay(OnePlayerController, OPlayerState, AbilitySystemComponent, AttributeSet);
        }
    }
}

void APlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
    {
        MovementComponent->MaxWalkSpeed = WalkSpeed;
    }
}

void APlayerCharacter::Die()
{
    if (IsDeathSequenceStarted()) return;

    if (bIsHardLocked && IsValid(LockedTarget) &&
        LockedTarget->Implements<UEnemyInterface>())
    {
        IEnemyInterface::Execute_ToggleHighlight(LockedTarget, false);
    }
    bIsHardLocked = false;
    LockedTarget = nullptr;

    Super::Die();

    if (AOnePlayerController* PlayerController =
        Cast<AOnePlayerController>(GetController()))
    {
        PlayerController->ClientShowDeathScreen();
    }
}

// ==========================================
// Input bindings.
// ==========================================
void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    
    UPlayerInputComponent* AuraInputComponent = CastChecked<UPlayerInputComponent>(PlayerInputComponent);

    // Bind tag-driven GAS actions.
    if (InputConfig)
    {
        AuraInputComponent->BindAbilityAction(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
    }

    // Bind native character actions.
    if (MoveAction)
        AuraInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);

    if (LookAction)
        AuraInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);

    if (ZoomAction)
        AuraInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Zoom);

    if (SprintAction)
    {
        AuraInputComponent->BindAction(SprintAction, ETriggerEvent::Triggered, this, &APlayerCharacter::SprintStart);
        AuraInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &APlayerCharacter::SprintStop);
        AuraInputComponent->BindAction(SprintAction, ETriggerEvent::Canceled, this, &APlayerCharacter::SprintStop);
    }

    if (DashAction)
        AuraInputComponent->BindAction(DashAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Dash);

    if (JumpAction)
    {
        AuraInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
        AuraInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
    }
    
    // Bind interaction and pickup input.
    if (InteractAction)
    {
        AuraInputComponent->BindAction(
     InteractAction,
     ETriggerEvent::Started,
     this,
     &APlayerCharacter::Interact);
    }

    // Bind lock-on input.
    if (LockAction)
        AuraInputComponent->BindAction(LockAction, ETriggerEvent::Started, this, &APlayerCharacter::ToggleLockOn);

    // Bind the character-panel toggle.
    if (OpenPanelAction)
        AuraInputComponent->BindAction(OpenPanelAction, ETriggerEvent::Started, this, &APlayerCharacter::ToggleOpenPanelAction);
          
    // Bind the inventory-panel toggle.
    if (OpenInventoryAction)
        AuraInputComponent->BindAction(OpenInventoryAction, ETriggerEvent::Started, this, &APlayerCharacter::ToggleInventoryPanelAction);
}

void APlayerCharacter::Move(const FInputActionValue& Value)
{
    if (GetAbilitySystemComponent() && GetAbilitySystemComponent()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"))))
    {
        return;
    }
    FVector2D MovementVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
       const FRotator Rotation = Controller->GetControlRotation();
       const FRotator YawRotation(0, Rotation.Yaw, 0); 

       const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
       const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

       AddMovementInput(ForwardDirection, MovementVector.Y);
       AddMovementInput(RightDirection, MovementVector.X);
    }
}

void APlayerCharacter::Look(const FInputActionValue& Value)
{
    if (bIsHardLocked) return;
    
    FVector2D LookAxisVector = Value.Get<FVector2D>();
    if (Controller != nullptr)
    {
       AddControllerYawInput(LookAxisVector.X);
       AddControllerPitchInput(LookAxisVector.Y);
    }
}

void APlayerCharacter::Zoom(const FInputActionValue& Value)
{
    float ZoomValue = Value.Get<float>();
    if (CameraBoom)
    {
       const float MinDistance = 150.0f; 
       const float MaxDistance = 1000.0f; 
       const float ZoomSpeed = 20.0f;     

       float NewDistance = CameraBoom->TargetArmLength - (ZoomValue * ZoomSpeed);
       CameraBoom->TargetArmLength = FMath::Clamp(NewDistance, MinDistance, MaxDistance);
    }
}

void APlayerCharacter::SprintStart()
{
    if (GetCharacterMovement()) GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void APlayerCharacter::SprintStop()
{
    if (GetCharacterMovement()) GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void APlayerCharacter::Dash()
{
    UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
    if (DashMontage)
    {
        PlayAnimMontage(DashMontage);
    }
    if (!MovementComponent)
    {
        return;
    }

    if (GetAbilitySystemComponent() &&
        GetAbilitySystemComponent()->HasMatchingGameplayTag(
            FGameplayTag::RequestGameplayTag(FName("State.Attacking"))))
    {
        return;
    }

    FVector DashDirection = GetLastMovementInputVector();
    DashDirection.Z = 0.f;

    if (DashDirection.IsNearlyZero())
    {
        DashDirection = GetActorForwardVector();
        DashDirection.Z = 0.f;
    }

    DashDirection.Normalize();
    MovementComponent->Velocity = FVector(
        DashDirection.X * DashSpeed,
        DashDirection.Y * DashSpeed,
        MovementComponent->Velocity.Z);
}

void APlayerCharacter::Interact()
{
    if (AActor* InteractableActor = NearbyInteractable.Get())
    {
        if (InteractableActor->Implements<UInteractableInterface>() &&
            IInteractableInterface::Execute_CanInteract(
                InteractableActor, this))
        {
            IInteractableInterface::Execute_Interact(InteractableActor, this);
            return;
        }

        NearbyInteractable.Reset();
    }

    const FVector TraceStart = FollowCamera ? FollowCamera->GetComponentLocation() : GetActorLocation();
    const FVector TraceEnd = TraceStart + (FollowCamera ? FollowCamera->GetForwardVector() : GetActorForwardVector()) * InteractionDistance;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlayerInteraction), false, this);
    FHitResult Hit;

    if (GetWorld()->SweepSingleByChannel(
        Hit, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility,
        FCollisionShape::MakeSphere(35.f), QueryParams))
    {
        AActor* HitActor = Hit.GetActor();
        if (IsValid(HitActor) && HitActor->Implements<UInteractableInterface>() &&
            IInteractableInterface::Execute_CanInteract(HitActor, this))
        {
            IInteractableInterface::Execute_Interact(HitActor, this);
            return;
        }
    }

    UInventoryComponent* Inventory = FindComponentByClass<UInventoryComponent>();
    if (!Inventory) return;
    
    TArray<AActor*> OverlappingActors;
    GetOverlappingActors(OverlappingActors, AItemPickup::StaticClass());
    
    for (AActor* OverlappedActor : OverlappingActors)
    {
        if (AItemPickup* Pickup = Cast<AItemPickup>(OverlappedActor))
        {
            if (Pickup->ItemID == NAME_None)
            {
                continue;
            }

            // Add the pickup to inventory before destroying it.
            if (Inventory->AddItem(Pickup->ItemID, 1))
            {
                Pickup->Destroy();
            }
        }
    }
}

void APlayerCharacter::SetNearbyInteractable(AActor* InteractableActor)
{
    if (IsValid(InteractableActor) &&
        InteractableActor->Implements<UInteractableInterface>())
    {
        NearbyInteractable = InteractableActor;
    }
}

void APlayerCharacter::ClearNearbyInteractable(AActor* InteractableActor)
{
    if (NearbyInteractable.Get() == InteractableActor)
    {
        NearbyInteractable.Reset();
    }
}

void APlayerCharacter::AbilityInputTagPressed(FGameplayTag InputTag)
{
    if (UPlayerAbilitySystemComponent* PlayerASC = Cast<UPlayerAbilitySystemComponent>(GetAbilitySystemComponent()))
    {
        PlayerASC->AbilityInputTagPressed(InputTag);
    }
}

void APlayerCharacter::AbilityInputTagReleased(FGameplayTag InputTag)
{
    if (UPlayerAbilitySystemComponent* PlayerASC = Cast<UPlayerAbilitySystemComponent>(GetAbilitySystemComponent()))
    {
        PlayerASC->AbilityInputTagReleased(InputTag);
    }
}

void APlayerCharacter::AbilityInputTagHeld(FGameplayTag InputTag)
{
    if (UPlayerAbilitySystemComponent* PlayerASC = Cast<UPlayerAbilitySystemComponent>(GetAbilitySystemComponent()))
    {
        PlayerASC->AbilityInputTagHeld(InputTag); 
    }
}

// ========================================================
// Character panel.
// ========================================================
void APlayerCharacter::ToggleOpenPanelAction()
{
    AOnePlayerController* PC = Cast<AOnePlayerController>(GetController());
    if (!PC) return;

    if (CharacterPanelInstance && CharacterPanelInstance->IsInViewport())
    {
       PC->CloseManagedMenu(CharacterPanelInstance);
    }
    else
    {
       if (CharacterPanelInstance == nullptr && CharacterPanelClass != nullptr)
       {
          CharacterPanelInstance = CreateWidget<UUserWidget>(PC, CharacterPanelClass);
       }
       if (CharacterPanelInstance)
       {
          PC->OpenManagedMenu(CharacterPanelInstance);
       }
    }
}

// ========================================================
// Inventory panel.
// ========================================================
void APlayerCharacter::ToggleInventoryPanelAction()
{
    AOnePlayerController* PC = Cast<AOnePlayerController>(GetController());
    if (!PC) return;

    if (InventoryPanelInstance && InventoryPanelInstance->IsInViewport())
    {
        PC->CloseManagedMenu(InventoryPanelInstance);
    }
    else
    {
        if (InventoryPanelInstance == nullptr && InventoryPanelClass != nullptr)
        {
            InventoryPanelInstance = CreateWidget<UInventoryPanelWidget>(PC, InventoryPanelClass);
        }
   
        if (InventoryPanelInstance)
        {
            // Inject the character's inventory component into the panel.
            UInventoryComponent* InventoryComp = FindComponentByClass<UInventoryComponent>();
            if (InventoryComp)
            {
                InventoryPanelInstance->InitializePanel(InventoryComp);
            }

            PC->OpenManagedMenu(InventoryPanelInstance);
        }
    }
}

// Lock-on targeting.

AActor* APlayerCharacter::FindBestTarget()
{
    TArray<AActor*> OverlappingActors;
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(this);

    UKismetSystemLibrary::SphereOverlapActors(
        this, GetActorLocation(), LockTargetRadius, 
        TArray<TEnumAsByte<EObjectTypeQuery>>{ UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn) },
        AActor::StaticClass(), ActorsToIgnore, OverlappingActors
    );

    AActor* BestTarget = nullptr;
    float HighestScore = -1.f; 
    FVector CameraLocation = FollowCamera->GetComponentLocation();
    FVector CameraForward = FollowCamera->GetForwardVector();

    for (AActor* Actor : OverlappingActors)
    {
        if (!Actor->Implements<UEnemyInterface>()) continue;
        FVector DirToTarget = (Actor->GetActorLocation() - CameraLocation).GetSafeNormal();
        float DotProduct = FVector::DotProduct(CameraForward, DirToTarget);

        if (DotProduct > 0.6f && DotProduct > HighestScore)
        {
            HighestScore = DotProduct;
            BestTarget = Actor;
        }
    }
    return BestTarget;
}

void APlayerCharacter::ToggleLockOn()
{
    if (bIsHardLocked && LockedTarget)
    {
        bIsHardLocked = false;
        if (LockedTarget->Implements<UEnemyInterface>()) IEnemyInterface::Execute_ToggleHighlight(LockedTarget, false);
        LockedTarget = nullptr;
        GetCharacterMovement()->bOrientRotationToMovement = true; 
        bUseControllerRotationYaw = false; 
    }
    else
    {
        AActor* TargetToLock = FindBestTarget();
        if (TargetToLock)
        {
            LockedTarget = TargetToLock;
            bIsHardLocked = true;
            if (LockedTarget->Implements<UEnemyInterface>()) IEnemyInterface::Execute_ToggleHighlight(LockedTarget, true);
            GetCharacterMovement()->bOrientRotationToMovement = false; 
            bUseControllerRotationYaw = true; 
        }
    }
}

void APlayerCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsHardLocked && IsValid(LockedTarget))
    {
        FVector TargetLocation = LockedTarget->GetActorLocation();
        TargetLocation.Z -= 50.f; 
        FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), TargetLocation);
        FRotator CurrentRotation = Controller->GetControlRotation();
        FRotator SmoothedRotation = FMath::RInterpTo(CurrentRotation, LookAtRotation, DeltaTime, 8.0f);
        SmoothedRotation.Roll = 0.f;
        Controller->SetControlRotation(SmoothedRotation);
    }
    else if (bIsHardLocked)
    {
        ToggleLockOn();
    }
}
