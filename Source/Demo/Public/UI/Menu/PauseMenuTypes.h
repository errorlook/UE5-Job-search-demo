#pragma once

#include "CoreMinimal.h"
#include "PauseMenuTypes.generated.h"

UENUM(BlueprintType)
enum class EPauseMenuPage : uint8
{
	Main,
	Settings,
	Confirmation
};

UENUM(BlueprintType)
enum class EPauseMenuAction : uint8
{
	None UMETA(Hidden),
	Continue,
	OpenSettings,
	ReturnToPauseMenu,
	ReturnToMainMenu,
	ExitGame
};
