#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h" 
#include "ItemTypes.generated.h" 

// ==========================================
// Item categories used by inventory data.
// ==========================================
UENUM(BlueprintType)
enum class EItemType : uint8
{
	Material    UMETA(DisplayName = "材料"),
	Weapon      UMETA(DisplayName = "武器"),
	Quest       UMETA(DisplayName = "任务道具"),
	Apparatus   UMETA(DisplayName = "仪器")
};

// ==========================================
// Row format for the item data table.
// ==========================================
USTRUCT(BlueprintType)
struct FItemDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText ItemName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EItemType ItemType = EItemType::Material;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTexture2D> ItemIcon;
};
