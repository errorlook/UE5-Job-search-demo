#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "GameplayEffect.h" // Required for item effects.
#include "Components/ItemTypes.h"
#include "InventoryComponent.generated.h"

// =========================================================================
// Static item definition stored in a data table.
// =========================================================================
USTRUCT(BlueprintType)
struct FItemStaticData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Data")
    FName ItemID = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Data")
    FText ItemName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Data")
    UTexture2D* ItemIcon = nullptr;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Data")
    UStaticMesh* ItemMesh = nullptr;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
    EItemType ItemType = EItemType::Material;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Data")
    int32 MaxStackSize = 99; // Maximum items per stack.

    // Effect applied when a consumable is used.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|GAS")
    TSubclassOf<class UGameplayEffect> UsedGameplayEffect;
};

// =========================================================================
// Runtime state for one inventory slot.
// =========================================================================
USTRUCT(BlueprintType)
struct FInventorySlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Slot")
    FName ItemID = NAME_None; // NAME_None marks an empty slot.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Slot")
    int32 Quantity = 0;
    
    // Returns whether this slot contains no item.
    bool IsEmpty() const { return ItemID == NAME_None || Quantity <= 0; }
};

// Notifies UMG whenever inventory contents change.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);


// =========================================================================
// Inventory component owned by the player.
// =========================================================================
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEMO_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:	
    UInventoryComponent();

protected:
    virtual void BeginPlay() override;

public:
    // Configuration.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Config")
    int32 InventoryCapacity = 20; // Number of available slots.

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Config")
    UDataTable* ItemDataTable; // Configured by the owning Blueprint.

    // Runtime state.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|State")
    TArray<FInventorySlot> InventorySlots;

    // Change notifications.
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnInventoryUpdated OnInventoryUpdated;

    // Blueprint API.
    
    // Looks up static item data.
    UFUNCTION(BlueprintCallable, Category = "Inventory|Logic")
    bool GetItemStaticData(FName ItemID, FItemStaticData& OutItemData);
    
    // Adds an item using existing stacks before empty slots.
    UFUNCTION(BlueprintCallable, Category = "Inventory|Logic")
    bool AddItem(FName ItemID, int32 Amount);

    // Consumes an item and applies its configured GAS effect.
    UFUNCTION(BlueprintCallable, Category = "Inventory|Logic")
    bool UseItemAtIndex(int32 SlotIndex);
};
