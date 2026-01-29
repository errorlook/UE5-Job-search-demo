// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/EnemyCharacter.h"
#include "Components/PrimitiveComponent.h"

//高亮接口实现
void AEnemyCharacter::ToggleHighlight_Implementation(bool bActive)
{
	//获取所有的原始组件
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	//遍历所有组件，设置高亮状态
	for (UPrimitiveComponent* Component : PrimitiveComponents)
	{
        if (Component)
        {
            // 2. 核心开关：开启/关闭自定义深度
            Component->SetRenderCustomDepth(bActive);

            // 3. (可选) 设置模具值 (Stencil Value)
            // 这也是二游常用的技巧。比如：
            // 250 = 红色轮廓 (敌人)
            // 251 = 绿色轮廓 (队友)
            // 252 = 金色轮廓 (道具)
            // 你可以在这里根据 bActive 动态设置这个值
            if (bActive)
            {
                Component->SetCustomDepthStencilValue(250); // 假设 250 代表红光
            }
        }
	}
}