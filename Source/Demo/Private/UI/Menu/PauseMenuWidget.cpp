#include "UI/Menu/PauseMenuWidget.h"

#include "Player/OnePlayerController.h"

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	MenuController = Cast<AOnePlayerController>(GetOwningPlayer());
	if (AOnePlayerController* PlayerController = MenuController.Get())
	{
		PlayerController->OnPauseMenuStateChanged.AddUniqueDynamic(
			this, &UPauseMenuWidget::HandlePauseMenuStateChanged);
		PlayerController->OnPauseMenuPageChanged.AddUniqueDynamic(
			this, &UPauseMenuWidget::HandlePauseMenuPageChanged);
		PlayerController->OnPauseMenuConfirmationRequested.AddUniqueDynamic(
			this, &UPauseMenuWidget::HandlePauseMenuConfirmationRequested);
	}
}

void UPauseMenuWidget::NativeDestruct()
{
	if (AOnePlayerController* PlayerController = MenuController.Get())
	{
		// Notify first so the widget can receive the final closed-state
		// broadcast before its controller delegates are removed.
		PlayerController->NotifyPauseMenuWidgetRemoved(this);
		PlayerController->OnPauseMenuStateChanged.RemoveDynamic(
			this, &UPauseMenuWidget::HandlePauseMenuStateChanged);
		PlayerController->OnPauseMenuPageChanged.RemoveDynamic(
			this, &UPauseMenuWidget::HandlePauseMenuPageChanged);
		PlayerController->OnPauseMenuConfirmationRequested.RemoveDynamic(
			this, &UPauseMenuWidget::HandlePauseMenuConfirmationRequested);
	}

	MenuController.Reset();
	Super::NativeDestruct();
}

void UPauseMenuWidget::RequestAction(EPauseMenuAction Action)
{
	if (AOnePlayerController* PlayerController = MenuController.Get())
	{
		PlayerController->RequestPauseMenuAction(Action);
	}
}

void UPauseMenuWidget::ConfirmPendingAction(bool bConfirmed)
{
	if (AOnePlayerController* PlayerController = MenuController.Get())
	{
		PlayerController->ConfirmPauseMenuAction(bConfirmed);
	}
}

void UPauseMenuWidget::HandlePauseMenuStateChanged(bool bIsOpen)
{
	OnPauseMenuStateChanged(bIsOpen);
}

void UPauseMenuWidget::HandlePauseMenuPageChanged(EPauseMenuPage NewPage)
{
	const EPauseMenuAction PendingAction = MenuController.IsValid()
		? MenuController->GetPendingPauseMenuAction()
		: EPauseMenuAction::None;
	OnPauseMenuPageChanged(NewPage, PendingAction);
}

void UPauseMenuWidget::HandlePauseMenuConfirmationRequested(
	EPauseMenuAction Action)
{
	OnPauseMenuConfirmationRequested(Action);
}
