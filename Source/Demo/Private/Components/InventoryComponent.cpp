#include "Components/InventoryComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

namespace
{
    struct FExistingStackUpdate
    {
        int32 SlotIndex = INDEX_NONE;
        int32 QuantityToAdd = 0;
    };

    struct FNewStackCreate
    {
        int32 SlotIndex = INDEX_NONE;
        int32 Quantity = 0;
    };
}

UInventoryComponent::UInventoryComponent()
{
    // Inventory is event-driven and does not require ticking.
    PrimaryComponentTick.bCanEverTick = false; 
}

void UInventoryComponent::BeginPlay()
{
    Super::BeginPlay();

    // Initialize the fixed-capacity slot array.
    InventorySlots.SetNum(InventoryCapacity);
}

bool UInventoryComponent::GetItemStaticData(FName ItemID, FItemStaticData& OutItemData)
{
    if (ItemDataTable)
    {
        // Resolve static item data by row identifier.
        FItemStaticData* Data = ItemDataTable->FindRow<FItemStaticData>(ItemID, TEXT("Inventory Lookup"));
        if (Data)
        {
            OutItemData = *Data;
            return true;
        }
    }
    return false;
}

bool UInventoryComponent::AddItem(FName ItemID, int32 Amount)
{
    if (Amount <= 0 || ItemID == NAME_None)
    {
        return false;
    }

    FItemStaticData ItemData;
    if (!GetItemStaticData(ItemID, ItemData) || ItemData.MaxStackSize <= 0)
    {
        return false;
    }

    TArray<int32> ExistingStackIndices;
    TArray<int32> EmptySlotIndices;
    int64 AvailableCapacity = 0;

    // Phase one: inspect capacity without changing inventory state.
    for (int32 SlotIndex = 0; SlotIndex < InventorySlots.Num(); ++SlotIndex)
    {
        const FInventorySlot& Slot = InventorySlots[SlotIndex];
        if (Slot.IsEmpty())
        {
            EmptySlotIndices.Add(SlotIndex);
            AvailableCapacity += ItemData.MaxStackSize;
            continue;
        }

        if (Slot.ItemID == ItemID)
        {
            if (Slot.Quantity > ItemData.MaxStackSize)
            {
                return false;
            }

            if (Slot.Quantity < ItemData.MaxStackSize)
            {
                ExistingStackIndices.Add(SlotIndex);
                AvailableCapacity += ItemData.MaxStackSize - Slot.Quantity;
            }
        }
    }

    if (AvailableCapacity < static_cast<int64>(Amount))
    {
        return false;
    }

    TArray<FExistingStackUpdate> ExistingStackUpdates;
    TArray<FNewStackCreate> NewStackCreates;
    int32 RemainingAmount = Amount;

    for (const int32 SlotIndex : ExistingStackIndices)
    {
        if (RemainingAmount == 0)
        {
            break;
        }

        const FInventorySlot& Slot = InventorySlots[SlotIndex];
        const int32 QuantityToAdd = FMath::Min(
            RemainingAmount,
            ItemData.MaxStackSize - Slot.Quantity);
        ExistingStackUpdates.Add({SlotIndex, QuantityToAdd});
        RemainingAmount -= QuantityToAdd;
    }

    for (const int32 SlotIndex : EmptySlotIndices)
    {
        if (RemainingAmount == 0)
        {
            break;
        }

        const int32 StackQuantity = FMath::Min(
            RemainingAmount,
            ItemData.MaxStackSize);
        NewStackCreates.Add({SlotIndex, StackQuantity});
        RemainingAmount -= StackQuantity;
    }

    int64 PlannedAmount = 0;
    bool bPlanIsValid = RemainingAmount == 0;

    for (const FExistingStackUpdate& Update : ExistingStackUpdates)
    {
        bPlanIsValid = bPlanIsValid
            && InventorySlots.IsValidIndex(Update.SlotIndex)
            && Update.QuantityToAdd > 0;
        if (!bPlanIsValid)
        {
            break;
        }

        const FInventorySlot& Slot = InventorySlots[Update.SlotIndex];
        bPlanIsValid = Slot.ItemID == ItemID
            && !Slot.IsEmpty()
            && Slot.Quantity + Update.QuantityToAdd <= ItemData.MaxStackSize;
        PlannedAmount += Update.QuantityToAdd;
    }

    for (const FNewStackCreate& Create : NewStackCreates)
    {
        bPlanIsValid = bPlanIsValid
            && InventorySlots.IsValidIndex(Create.SlotIndex)
            && InventorySlots[Create.SlotIndex].IsEmpty()
            && Create.Quantity > 0
            && Create.Quantity <= ItemData.MaxStackSize;
        PlannedAmount += Create.Quantity;
    }

    if (!bPlanIsValid || PlannedAmount != static_cast<int64>(Amount))
    {
        return false;
    }

    // Phase two: the complete plan is known to fit, so commit it atomically.
    for (const FExistingStackUpdate& Update : ExistingStackUpdates)
    {
        InventorySlots[Update.SlotIndex].Quantity += Update.QuantityToAdd;
    }

    for (const FNewStackCreate& Create : NewStackCreates)
    {
        FInventorySlot& Slot = InventorySlots[Create.SlotIndex];
        Slot.ItemID = ItemID;
        Slot.Quantity = Create.Quantity;
    }

    OnInventoryUpdated.Broadcast();
#if WITH_DEV_AUTOMATION_TESTS
    OnInventoryUpdatedTestProxy.Broadcast();
#endif
    return true;
}

bool UInventoryComponent::UseItemAtIndex(int32 SlotIndex)
{
    // Reject invalid or empty slots.
    if (!InventorySlots.IsValidIndex(SlotIndex) || InventorySlots[SlotIndex].IsEmpty()) return false;

    FName ItemToUse = InventorySlots[SlotIndex].ItemID;
    FItemStaticData ItemData;
    
    if (GetItemStaticData(ItemToUse, ItemData))
    {
        // ==========================================
        // Apply the consumable's configured gameplay effect.
        // ==========================================
        if (ItemData.UsedGameplayEffect)
        {
            AActor* Owner = GetOwner();
            UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner);
            
            if (ASC)
            {
                // Build an effect context and apply the effect to the owner.
                FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
                ContextHandle.AddInstigator(Owner, Owner);
                
                FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(ItemData.UsedGameplayEffect, 1.f, ContextHandle);
                if (SpecHandle.IsValid())
                {
                    ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
                }
            }
        }

        // ==========================================
        // Consume one item.
        // ==========================================
        InventorySlots[SlotIndex].Quantity -= 1;
        
        // Reset the slot after the stack is exhausted.
        if (InventorySlots[SlotIndex].Quantity <= 0)
        {
            InventorySlots[SlotIndex].ItemID = NAME_None;
            InventorySlots[SlotIndex].Quantity = 0;
        }

        // Refresh inventory UI.
        OnInventoryUpdated.Broadcast();
        return true;
    }

    return false;
}
