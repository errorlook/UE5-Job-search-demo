// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/WidgetController/PlayerWidgetController.h"
#include "Player/OPlayerState.h"
#include "interaction/CombatInterface.h"

void UPlayerWidgetController::SetWidgetControllerParams(const FWidgetControllerParams& WCParams)
{
	PlayerController = WCParams.PlayerController;
	PlayerState = WCParams.PlayerState;
	AbilitySystemComponent = WCParams.AbilitySystemComponent;
	AttributeSet = WCParams.AttributeSet;
}

void UPlayerWidgetController::BroadcastInitialValues()
{
	if (AOPlayerState* OPlayerState = Cast<AOPlayerState>(PlayerState))
	{
		const int32 CurrentLevel = OPlayerState->GetPlayerLevel();
		// Broadcast the current level to all dependent widgets.
		OnPlayerLevelChanged.Broadcast(CurrentLevel);
		OnPlayerLevelChangedNative.Broadcast(CurrentLevel);
	}

}

void UPlayerWidgetController::BindCallbacksToDependencies()
{
	if (AOPlayerState* OPlayerState = Cast<AOPlayerState>(PlayerState))
	{
		OPlayerState->OnLevelChangedNative.RemoveAll(this);
		OPlayerState->OnLevelChangedNative.AddUObject(
			this, &UPlayerWidgetController::PlayerLevelChangedCallback);
	}
}

void UPlayerWidgetController::PlayerLevelChangedCallback(int32 NewLevel)
{
	OnPlayerLevelChanged.Broadcast(NewLevel);
	OnPlayerLevelChangedNative.Broadcast(NewLevel);
}
