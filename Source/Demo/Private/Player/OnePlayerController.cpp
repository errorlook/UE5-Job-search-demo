// OnePlayerController.cpp

#include "Player/OnePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputMappingContext.h" 

AOnePlayerController::AOnePlayerController()
{
	bReplicates = true;
}

void AOnePlayerController::ToggleMouseCursor()
{
	// 1. 直接使用父类的 bShowMouseCursor 取反
	// 如果当前是 true，就变成 false；如果是 false，就变成 true
	bShowMouseCursor = !bShowMouseCursor;

	// 2. 根据状态应用不同的输入模式
	if (bShowMouseCursor)
	{
		// === 鼠标显示模式 (Alt 呼出) ===
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		// 【可选优化】这行代码能让鼠标点击瞬间不隐藏，防止闪烁，但不是必须的
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
	else
	{
		// === 游戏模式 (鼠标消失) ===
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
	}
}

// 【新增】点击屏幕时的逻辑
void AOnePlayerController::OnClickScreen()
{
	// 只有当鼠标当前是【显示】的时候，点击屏幕才需要把它【隐藏】
	// 如果鼠标本来就是隐藏的（你在正常玩游戏开枪），不要执行这个，否则会干扰正常操作
	if (bShowMouseCursor)
	{
		ToggleMouseCursor(); // 调用切换函数，把它关掉
	}
}

void AOnePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// 绑定 Alt 键
		EnhancedInputComponent->BindAction(AltAction, ETriggerEvent::Started, this, &AOnePlayerController::ToggleMouseCursor);

		// 【新增】绑定 鼠标左键 (注意这里检查一下指针是否为空，防止编辑器没设置崩溃)
		if (ClickAction)
		{
			EnhancedInputComponent->BindAction(ClickAction, ETriggerEvent::Started, this, &AOnePlayerController::OnClickScreen);
		}
	}
}

void AOnePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 安全检查
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