

#pragma once

#include "CoreMinimal.h"
#include "Character/CharacterBase.h"
#include "InputActionValue.h" 
#include "AbilitySystemInterface.h"
#include "PlayerCharacter.generated.h"


class USpringArmComponent;
class UCameraComponent;
class UInputAction;

UCLASS()
class DEMO_API APlayerCharacter : public ACharacterBase
{
	GENERATED_BODY()

public:
	APlayerCharacter(); 
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

private:
	void InitAbilityActorInfo();


protected:
	virtual void BeginPlay() override;

	// 重写绑定输入功能的函数
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** 输入回调函数 */
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Zoom(const FInputActionValue& Value);

	//奔跑
	void SprintStart();
	void SprintStop();

protected:
	/** 摄像机组件部分 - 只有玩家才需要这些 */

	// 【新增2】弹簧臂 (脖子/自拍杆)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	// 【新增3】跟随摄像机 (眼睛)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** 输入资产部分 */

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ZoomAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

};