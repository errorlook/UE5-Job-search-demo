#include "Scripting/DemoLuaLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Player/OnePlayerController.h"
#include "Player/OPlayerState.h"
#include "Character/PlayerCharacter.h"
#include "Components/PartyComponent.h"
#include "Components/QuestComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/ExpComponent.h"

AOnePlayerController* UDemoLuaLibrary::GetDemoPlayerController(
	const UObject* WorldContextObject, int32 PlayerIndex)
{
	return WorldContextObject
		? Cast<AOnePlayerController>(UGameplayStatics::GetPlayerController(
			WorldContextObject, PlayerIndex))
		: nullptr;
}

AOPlayerState* UDemoLuaLibrary::GetDemoPlayerState(
	const UObject* WorldContextObject, int32 PlayerIndex)
{
	const AOnePlayerController* PlayerController =
		GetDemoPlayerController(WorldContextObject, PlayerIndex);
	return PlayerController
		? PlayerController->GetPlayerState<AOPlayerState>()
		: nullptr;
}

APlayerCharacter* UDemoLuaLibrary::GetDemoPlayerCharacter(
	const UObject* WorldContextObject, int32 PlayerIndex)
{
	const AOnePlayerController* PlayerController =
		GetDemoPlayerController(WorldContextObject, PlayerIndex);
	return PlayerController
		? Cast<APlayerCharacter>(PlayerController->GetPawn())
		: nullptr;
}

UPartyComponent* UDemoLuaLibrary::GetPartyComponent(
	const UObject* WorldContextObject, int32 PlayerIndex)
{
	const AOPlayerState* PlayerState =
		GetDemoPlayerState(WorldContextObject, PlayerIndex);
	return PlayerState ? PlayerState->GetPartyComponent() : nullptr;
}

UQuestComponent* UDemoLuaLibrary::GetQuestComponent(
	const UObject* WorldContextObject, int32 PlayerIndex)
{
	const AOPlayerState* PlayerState =
		GetDemoPlayerState(WorldContextObject, PlayerIndex);
	return PlayerState ? PlayerState->GetQuestComponent() : nullptr;
}

UInventoryComponent* UDemoLuaLibrary::GetInventoryComponent(
	const UObject* WorldContextObject, int32 PlayerIndex)
{
	const APlayerCharacter* PlayerCharacter =
		GetDemoPlayerCharacter(WorldContextObject, PlayerIndex);
	return PlayerCharacter
		? PlayerCharacter->GetInventoryComponent()
		: nullptr;
}

UExpComponent* UDemoLuaLibrary::GetExperienceComponent(
	const UObject* WorldContextObject, int32 PlayerIndex)
{
	const AOPlayerState* PlayerState =
		GetDemoPlayerState(WorldContextObject, PlayerIndex);
	return PlayerState ? PlayerState->ExpComponent : nullptr;
}

void UDemoLuaLibrary::SwitchPartySlot(
	const UObject* WorldContextObject, int32 SlotIndex, int32 PlayerIndex)
{
	if (AOnePlayerController* PlayerController =
		GetDemoPlayerController(WorldContextObject, PlayerIndex))
	{
		PlayerController->SwitchToPartySlot(SlotIndex);
	}
}

void UDemoLuaLibrary::TogglePartyPage(
	const UObject* WorldContextObject, int32 PlayerIndex)
{
	if (AOnePlayerController* PlayerController =
		GetDemoPlayerController(WorldContextObject, PlayerIndex))
	{
		PlayerController->TogglePartyPage();
	}
}

void UDemoLuaLibrary::ToggleQuestList(
	const UObject* WorldContextObject, int32 PlayerIndex)
{
	if (AOnePlayerController* PlayerController =
		GetDemoPlayerController(WorldContextObject, PlayerIndex))
	{
		PlayerController->ToggleQuestList();
	}
}

void UDemoLuaLibrary::TogglePauseMenu(
	const UObject* WorldContextObject, int32 PlayerIndex)
{
	if (AOnePlayerController* PlayerController =
		GetDemoPlayerController(WorldContextObject, PlayerIndex))
	{
		PlayerController->TogglePauseMenu();
	}
}

void UDemoLuaLibrary::OpenPauseMenu(
	const UObject* WorldContextObject, int32 PlayerIndex)
{
	if (AOnePlayerController* PlayerController =
		GetDemoPlayerController(WorldContextObject, PlayerIndex))
	{
		PlayerController->OpenPauseMenu();
	}
}

void UDemoLuaLibrary::ClosePauseMenu(
	const UObject* WorldContextObject, int32 PlayerIndex)
{
	if (AOnePlayerController* PlayerController =
		GetDemoPlayerController(WorldContextObject, PlayerIndex))
	{
		PlayerController->ClosePauseMenu();
	}
}

bool UDemoLuaLibrary::IsPauseMenuOpen(
	const UObject* WorldContextObject, int32 PlayerIndex)
{
	const AOnePlayerController* PlayerController =
		GetDemoPlayerController(WorldContextObject, PlayerIndex);
	return PlayerController && PlayerController->IsPauseMenuOpen();
}

void UDemoLuaLibrary::RequestPauseMenuAction(
	const UObject* WorldContextObject, EPauseMenuAction Action,
	int32 PlayerIndex)
{
	if (AOnePlayerController* PlayerController =
		GetDemoPlayerController(WorldContextObject, PlayerIndex))
	{
		PlayerController->RequestPauseMenuAction(Action);
	}
}

void UDemoLuaLibrary::ConfirmPauseMenuAction(
	const UObject* WorldContextObject, bool bConfirmed, int32 PlayerIndex)
{
	if (AOnePlayerController* PlayerController =
		GetDemoPlayerController(WorldContextObject, PlayerIndex))
	{
		PlayerController->ConfirmPauseMenuAction(bConfirmed);
	}
}
