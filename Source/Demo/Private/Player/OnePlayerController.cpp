// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/OnePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"

AOnePlayerController::AOnePlayerController()
{
	bReplicates = true;
}

void AOnePlayerController::ToggleMouseCursor()
{
	// 1. 切换状态开关 (取反)
	bIsMouseVisible = !bIsMouseVisible;

	// 2. 将控制器的鼠标显示属性设置为当前状态
	bShowMouseCursor = bIsMouseVisible;

	// 3. 根据状态应用不同的输入模式
	if (bIsMouseVisible)
	{
		// === 情况一：鼠标显示出来了 (Alt 模式) ===
		// 我们需要允许鼠标点击 UI，同时不锁定鼠标在屏幕中心

		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	else
	{
		// === 情况二：鼠标隐藏了 (回到游戏) ===
	
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
	}
}

void AOnePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent(); // 别忘了调用父类方法，这是好习惯

	// 1. 先把输入组件转换成增强版
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// 2. 进行绑定
		// 参数依次是：动作资产，触发时机，执行者，执行函数
		EnhancedInputComponent->BindAction(AltAction, ETriggerEvent::Started, this, &AOnePlayerController::ToggleMouseCursor);
	}
}

void AOnePlayerController::BeginPlay()
{
	Super::BeginPlay();
	//检查输入系统
	check(PlayerContext);

	//获得增强输入子系统
	if (ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			//添加输入映射上下文，优先级为0
			Subsystem->AddMappingContext(PlayerContext, 0);
		}
	}


}
