// OnePlayerController.h

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

	// 绑定到 ALT 的函数 (切换显示/隐藏)
	void ToggleMouseCursor();

	// 【新增】绑定到 鼠标左键 的函数 (点击屏幕返回游戏)
	void OnClickScreen();

	virtual void SetupInputComponent() override;

protected:
	virtual void BeginPlay() override;

	// 【优化】删除了 bIsMouseVisible，因为父类有 bShowMouseCursor 可以直接用

private:
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> PlayerContext;

	// Alt 键动作
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> AltAction;

	// 【新增】鼠标左键动作
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ClickAction;
};