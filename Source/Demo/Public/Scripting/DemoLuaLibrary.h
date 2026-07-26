#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UI/Menu/PauseMenuTypes.h"
#include "DemoLuaLibrary.generated.h"

class AOnePlayerController;
class AOPlayerState;
class APlayerCharacter;
class UPartyComponent;
class UQuestComponent;
class UInventoryComponent;
class UExpComponent;

/**
 * Stable reflected entry points for Blueprint and UnLua gameplay scripts.
 * Network authority remains inside the underlying gameplay classes.
 */
UCLASS()
class DEMO_API UDemoLuaLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Demo|Lua",
		meta = (WorldContext = "WorldContextObject"))
	static AOnePlayerController* GetDemoPlayerController(
		const UObject* WorldContextObject, int32 PlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Demo|Lua",
		meta = (WorldContext = "WorldContextObject"))
	static AOPlayerState* GetDemoPlayerState(
		const UObject* WorldContextObject, int32 PlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Demo|Lua",
		meta = (WorldContext = "WorldContextObject"))
	static APlayerCharacter* GetDemoPlayerCharacter(
		const UObject* WorldContextObject, int32 PlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Demo|Lua|Party",
		meta = (WorldContext = "WorldContextObject"))
	static UPartyComponent* GetPartyComponent(
		const UObject* WorldContextObject, int32 PlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Demo|Lua|Quest",
		meta = (WorldContext = "WorldContextObject"))
	static UQuestComponent* GetQuestComponent(
		const UObject* WorldContextObject, int32 PlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Demo|Lua|Inventory",
		meta = (WorldContext = "WorldContextObject"))
	static UInventoryComponent* GetInventoryComponent(
		const UObject* WorldContextObject, int32 PlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Demo|Lua|Experience",
		meta = (WorldContext = "WorldContextObject"))
	static UExpComponent* GetExperienceComponent(
		const UObject* WorldContextObject, int32 PlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Demo|Lua|Party",
		meta = (WorldContext = "WorldContextObject"))
	static void SwitchPartySlot(
		const UObject* WorldContextObject, int32 SlotIndex,
		int32 PlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Demo|Lua|UI",
		meta = (WorldContext = "WorldContextObject"))
	static void TogglePartyPage(
		const UObject* WorldContextObject, int32 PlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Demo|Lua|UI",
		meta = (WorldContext = "WorldContextObject"))
	static void ToggleQuestList(
		const UObject* WorldContextObject, int32 PlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Demo|Lua|Pause Menu",
		meta = (WorldContext = "WorldContextObject"))
	static void TogglePauseMenu(
		const UObject* WorldContextObject, int32 PlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Demo|Lua|Pause Menu",
		meta = (WorldContext = "WorldContextObject"))
	static void OpenPauseMenu(
		const UObject* WorldContextObject, int32 PlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Demo|Lua|Pause Menu",
		meta = (WorldContext = "WorldContextObject"))
	static void ClosePauseMenu(
		const UObject* WorldContextObject, int32 PlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Demo|Lua|Pause Menu",
		meta = (WorldContext = "WorldContextObject"))
	static bool IsPauseMenuOpen(
		const UObject* WorldContextObject, int32 PlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Demo|Lua|Pause Menu",
		meta = (WorldContext = "WorldContextObject"))
	static void RequestPauseMenuAction(
		const UObject* WorldContextObject, EPauseMenuAction Action,
		int32 PlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Demo|Lua|Pause Menu",
		meta = (WorldContext = "WorldContextObject"))
	static void ConfirmPauseMenuAction(
		const UObject* WorldContextObject, bool bConfirmed,
		int32 PlayerIndex = 0);
};
