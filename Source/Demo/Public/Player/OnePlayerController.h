// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OnePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;

UCLASS()
class DEMO_API AOnePlayerController : public APlayerController
{
	GENERATED_BODY()
	

public:
	AOnePlayerController();
	//绑定到ALT的函数
	void ToggleMouseCursor();
	// 重写绑定输入功能的函数
	virtual void SetupInputComponent() override;

protected:
	//重写开始游戏函数
	virtual void BeginPlay() override;
	// 记录鼠标显示状态的开关
	bool bIsMouseVisible;



private:
	//增强输入组件
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext>PlayerContext;

	//鼠标
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction>AltAction;

	//
};
