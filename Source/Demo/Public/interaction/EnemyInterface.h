// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnemyInterface.generated.h"

//UEnemyInterface 类
// 这个类主要是为了让虚幻的反射系统（Reflection System）能识别它。
UINTERFACE(MinimalAPI)
class UEnemyInterface : public UInterface
{
	GENERATED_BODY()
};

// IEnemyInterface 类
class DEMO_API IEnemyInterface
{
	GENERATED_BODY()

	
public:

	/** * 切换高亮状态
	 * @param bActive - true 开启高亮，false 取消高亮
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void ToggleHighlight(bool bActive);
};
