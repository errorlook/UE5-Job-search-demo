#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "UI/Menu/PauseMenuTypes.h"
#include "PauseMenuWidget.generated.h"

class AOnePlayerController;
class UButton;
class UComboBoxString;
class UVerticalBox;
class ULuaHotReloadSubsystem;

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

	UFUNCTION(BlueprintPure, Category = "Pause Menu|Settings")
	int32 GetSettingValue(FName SettingId) const;

	UFUNCTION(BlueprintCallable, Category = "Pause Menu|Settings")
	bool SetSettingValue(FName SettingId, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Pause Menu|Settings")
	void ApplySettings();

	UFUNCTION(BlueprintCallable, Category = "Pause Menu|Settings")
	void SaveSettings();

	UFUNCTION(BlueprintCallable, Category = "Pause Menu|Settings")
	void ResetSettings();

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

	UFUNCTION()
	void HandleSettingsDefinitionsChanged(int32 NewRevision);

	UFUNCTION()
	void HandleSettingSelectionChanged(
		FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleApplySettingsClicked();

	UFUNCTION()
	void HandleResetSettingsClicked();

	void RebuildLuaSettings();
	void UpdateSettingsText();
	UVerticalBox* ResolveSettingsContainer(FName Category);

	TWeakObjectPtr<AOnePlayerController> MenuController;
	TWeakObjectPtr<ULuaHotReloadSubsystem> HotReloadSubsystem;
	TWeakObjectPtr<UButton> ApplySettingsButton;
	TWeakObjectPtr<UButton> ResetSettingsButton;

	UPROPERTY(Transient)
	TMap<TObjectPtr<UComboBoxString>, FName> SettingComboBoxes;

	UPROPERTY(Transient)
	TMap<TObjectPtr<UComboBoxString>, FString> SelectedSettingOptions;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UVerticalBox>> DynamicSettingsContainers;

	bool bRebuildingSettings = false;
};
