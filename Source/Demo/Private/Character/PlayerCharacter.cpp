
#include "Character/PlayerCharacter.h"

// 【核心头文件】
#include "GameFramework/SpringArmComponent.h" // 弹簧臂
#include "Camera/CameraComponent.h"           // 摄像机
#include "GameFramework/CharacterMovementComponent.h" // 角色移动组件
#include "GameFramework/Controller.h"         // 控制器
#include "EnhancedInputComponent.h"           // 增强输入组件
#include "AbilitySystemComponent.h"
#include "InputActionValue.h"                 // 输入值类型
#include "Player/OPlayerState.h"              // 玩家状态




APlayerCharacter::APlayerCharacter()
{
	// 1. 设置角色的旋转逻辑 (二游经典设置)
	// 这里的逻辑是：鼠标转动时，角色身体不跟着转，只有摄像机转。
	// 只有当你按WASD移动时，角色才自动转向移动方向。
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 配置移动组件
	GetCharacterMovement()->bOrientRotationToMovement = true; // 让角色朝向移动方向
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // 转身速度
	GetCharacterMovement()->JumpZVelocity = 700.f; // 跳跃高度
	GetCharacterMovement()->AirControl = 0.35f;    // 空中可控性

	// 2. 创建弹簧臂 (SpringArm) - 相当于自拍杆
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // 杆子长度 (摄像机距离)
	CameraBoom->bUsePawnControlRotation = true; // 关键：让杆子跟随鼠标旋转

	// 3. 创建摄像机 (Camera) - 相当于眼睛
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // 装在杆子末端
	FollowCamera->bUsePawnControlRotation = false; // 摄像机不需要再转了，因为杆子已经转了
}

void APlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// 服务器端初始化 GAS
	InitAbilityActorInfo();
}

void APlayerCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	// 客户端初始化 GAS
	InitAbilityActorInfo();
}

UAbilitySystemComponent* APlayerCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void APlayerCharacter::InitAbilityActorInfo()
{
	// 获取 PlayerState
	AOPlayerState* OPlayerState = GetPlayerState<AOPlayerState>();

	if (OPlayerState)
	{
		// 初始化 ASC 的 ActorInfo (OwnerActor = PlayerState, AvatarActor = Character)
		OPlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(OPlayerState, this);

		// 设置本地指针
		AbilitySystemComponent = OPlayerState->GetAbilitySystemComponent();
		AttributeSet = OPlayerState->GetAttributeSet();
	}
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 将普通输入组件强转为增强输入组件
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// 绑定 移动
		if (MoveAction)
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);

		// 绑定 视角
		if (LookAction)
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);

		// 绑定 缩放
		if (ZoomAction)
			EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Zoom);

		// 绑定 奔跑
		if (SprintAction)
		{
			// 按下时加速
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Triggered, this, &APlayerCharacter::SprintStart);
			// 松开时减速
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &APlayerCharacter::SprintStop);
		}

		// 绑定 跳跃
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}
	}
}

void APlayerCharacter::Move(const FInputActionValue& Value)
{
	// 获取输入的二维向量 (X=A/D, Y=W/S)
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// 1. 找出我们要往哪个方向跑
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0); // 只取水平方向

		// 2. 计算出前方向量 (X轴)
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// 3. 计算出右方向量 (Y轴)
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// 4. 添加移动输入
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void APlayerCharacter::Look(const FInputActionValue& Value)
{
	// 获取鼠标移动的二维向量
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// 左右转头
		AddControllerYawInput(LookAxisVector.X);
		// 上下抬头
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void APlayerCharacter::Zoom(const FInputActionValue& Value)
{
	// 获取滚轮的值 (通常是 1.0 或 -1.0)
	float ZoomValue = Value.Get<float>();

	if (CameraBoom)
	{
		// 定义缩放的范围
		const float MinDistance = 150.0f; // 最近距离
		const float MaxDistance = 1000.0f; // 最远距离
		const float ZoomSpeed = 20.0f;     // 缩放速度

		// 计算新的臂长
		float NewDistance = CameraBoom->TargetArmLength - (ZoomValue * ZoomSpeed);

		// 限制在这个范围内 (Clamp)
		CameraBoom->TargetArmLength = FMath::Clamp(NewDistance, MinDistance, MaxDistance);
	}
}

void APlayerCharacter::SprintStart()
{
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = 1000.f; // 设置为奔跑速度
	}
}

void APlayerCharacter::SprintStop()
{
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = 400.f; // 恢复默认速度
	}
}