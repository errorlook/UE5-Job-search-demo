#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Menu/PauseMenuTypes.h"
#include "PauseMenuWidget.generated.h"

class AOnePlayerController;

/**
 * Presentation-facing pause widget base. Blueprint owns layout, animation,
 * focus, and button events; all state and actions are forwarded to C++.
 */
UCLASS(Abstract, Blueprintable)
class DEMO_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void RequestAction(EPauseMenuAction Action);

	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void ConfirmPendingAction(bool bConfirmed);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Pause Menu|Presentation")
	void OnPauseMenuStateChanged(bool bIsOpen);

	UFUNCTION(BlueprintImplementableEvent, Category = "Pause Menu|Presentation")
	void OnPauseMenuPageChanged(
		EPauseMenuPage NewPage, EPauseMenuAction PendingAction);

	UFUNCTION(BlueprintImplementableEvent, Category = "Pause Menu|Presentation")
	void OnPauseMenuConfirmationRequested(EPauseMenuAction Action);

private:
	UFUNCTION()
	void HandlePauseMenuStateChanged(bool bIsOpen);

	UFUNCTION()
	void HandlePauseMenuPageChanged(EPauseMenuPage NewPage);

	UFUNCTION()
	void HandlePauseMenuConfirmationRequested(EPauseMenuAction Action);

	TWeakObjectPtr<AOnePlayerController> MenuController;
};
