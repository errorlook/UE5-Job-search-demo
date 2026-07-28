#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/InventoryComponent.h"
#include "Engine/DataTable.h"

namespace
{
    const FName ItemA(TEXT("ItemA"));
    const FName ItemB(TEXT("ItemB"));
    constexpr int32 DefaultMaxStackSize = 10;

    UDataTable* MakeItemDataTable(
        int32 ItemAMaxStackSize = DefaultMaxStackSize,
        int32 ItemBMaxStackSize = DefaultMaxStackSize)
    {
        UDataTable* DataTable = NewObject<UDataTable>();
        DataTable->RowStruct = FItemStaticData::StaticStruct();

        FItemStaticData ItemAData;
        ItemAData.ItemID = ItemA;
        ItemAData.MaxStackSize = ItemAMaxStackSize;
        DataTable->AddRow(ItemA, ItemAData);

        FItemStaticData ItemBData;
        ItemBData.ItemID = ItemB;
        ItemBData.MaxStackSize = ItemBMaxStackSize;
        DataTable->AddRow(ItemB, ItemBData);
        return DataTable;
    }

    struct FInventoryFixture
    {
        explicit FInventoryFixture(
            int32 SlotCount,
            int32 ItemAMaxStackSize = DefaultMaxStackSize,
            bool bCreateItemData = true)
        {
            Inventory = NewObject<UInventoryComponent>();
            Inventory->InventorySlots.SetNum(SlotCount);
            Inventory->ItemDataTable = bCreateItemData
                ? MakeItemDataTable(ItemAMaxStackSize)
                : nullptr;
            BroadcastHandle = Inventory->OnInventoryUpdatedTestProxy.AddLambda(
                [this]()
                {
                    ++BroadcastCount;
                });
        }

        ~FInventoryFixture()
        {
            Inventory->OnInventoryUpdatedTestProxy.Remove(BroadcastHandle);
        }

        UInventoryComponent* Inventory = nullptr;
        int32 BroadcastCount = 0;
        FDelegateHandle BroadcastHandle;
    };

    void SetSlot(
        UInventoryComponent* Inventory,
        int32 SlotIndex,
        FName ItemID,
        int32 Quantity)
    {
        Inventory->InventorySlots[SlotIndex].ItemID = ItemID;
        Inventory->InventorySlots[SlotIndex].Quantity = Quantity;
    }

    int32 GetItemQuantity(
        const UInventoryComponent* Inventory,
        FName ItemID)
    {
        int32 Total = 0;
        for (const FInventorySlot& Slot : Inventory->InventorySlots)
        {
            if (!Slot.IsEmpty() && Slot.ItemID == ItemID)
            {
                Total += Slot.Quantity;
            }
        }
        return Total;
    }

    bool TestSlotsExactlyEqual(
        FAutomationTestBase& Test,
        const FString& Context,
        const TArray<FInventorySlot>& Actual,
        const TArray<FInventorySlot>& Expected)
    {
        bool bEqual = Actual.Num() == Expected.Num();
        Test.TestEqual(
            FString::Printf(TEXT("%s: slot count"), *Context),
            Actual.Num(),
            Expected.Num());

        const int32 ComparableSlotCount = FMath::Min(
            Actual.Num(), Expected.Num());
        for (int32 SlotIndex = 0;
             SlotIndex < ComparableSlotCount;
             ++SlotIndex)
        {
            const bool bItemMatches =
                Actual[SlotIndex].ItemID == Expected[SlotIndex].ItemID;
            const bool bQuantityMatches =
                Actual[SlotIndex].Quantity == Expected[SlotIndex].Quantity;
            bEqual = bEqual && bItemMatches && bQuantityMatches;

            Test.TestEqual(
                FString::Printf(
                    TEXT("%s: slot %d ItemID"),
                    *Context,
                    SlotIndex),
                Actual[SlotIndex].ItemID,
                Expected[SlotIndex].ItemID);
            Test.TestEqual(
                FString::Printf(
                    TEXT("%s: slot %d Quantity"),
                    *Context,
                    SlotIndex),
                Actual[SlotIndex].Quantity,
                Expected[SlotIndex].Quantity);
        }
        return bEqual;
    }

    void TestStackLimit(
        FAutomationTestBase& Test,
        const UInventoryComponent* Inventory,
        int32 MaxStackSize,
        const FString& Context)
    {
        for (int32 SlotIndex = 0;
             SlotIndex < Inventory->InventorySlots.Num();
             ++SlotIndex)
        {
            const FInventorySlot& Slot = Inventory->InventorySlots[SlotIndex];
            if (!Slot.IsEmpty())
            {
                Test.TestTrue(
                    FString::Printf(
                        TEXT("%s: slot %d does not exceed MaxStackSize"),
                        *Context,
                        SlotIndex),
                    Slot.Quantity <= MaxStackSize);
            }
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryAddItemAtomicityTest,
    "Demo.Inventory.AddItemAtomicity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryAddItemAtomicityTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    {
        FInventoryFixture Fixture(2);
        SetSlot(Fixture.Inventory, 0, ItemA, 4);
        const TArray<FInventorySlot> Before = Fixture.Inventory->InventorySlots;
        TestFalse(TEXT("zero amount is rejected"), Fixture.Inventory->AddItem(ItemA, 0));
        TestTrue(
            TEXT("zero amount leaves every slot unchanged"),
            TestSlotsExactlyEqual(
                *this, TEXT("zero amount"),
                Fixture.Inventory->InventorySlots, Before));
        TestEqual(TEXT("zero amount does not broadcast"), Fixture.BroadcastCount, 0);
    }

    {
        FInventoryFixture Fixture(2);
        SetSlot(Fixture.Inventory, 0, ItemA, 4);
        const TArray<FInventorySlot> Before = Fixture.Inventory->InventorySlots;
        TestFalse(TEXT("negative amount is rejected"), Fixture.Inventory->AddItem(ItemA, -3));
        TestTrue(
            TEXT("negative amount leaves every slot unchanged"),
            TestSlotsExactlyEqual(
                *this, TEXT("negative amount"),
                Fixture.Inventory->InventorySlots, Before));
        TestEqual(TEXT("negative amount does not broadcast"), Fixture.BroadcastCount, 0);
    }

    {
        FInventoryFixture Fixture(2, DefaultMaxStackSize, false);
        const TArray<FInventorySlot> Before = Fixture.Inventory->InventorySlots;
        TestFalse(TEXT("missing item data is rejected"), Fixture.Inventory->AddItem(ItemA, 1));
        TestTrue(
            TEXT("missing item data leaves every slot unchanged"),
            TestSlotsExactlyEqual(
                *this, TEXT("missing item data"),
                Fixture.Inventory->InventorySlots, Before));
    }

    {
        FInventoryFixture Fixture(2, 0);
        const TArray<FInventorySlot> Before = Fixture.Inventory->InventorySlots;
        TestFalse(TEXT("zero MaxStackSize is rejected"), Fixture.Inventory->AddItem(ItemA, 1));
        TestTrue(
            TEXT("zero MaxStackSize leaves every slot unchanged"),
            TestSlotsExactlyEqual(
                *this, TEXT("zero MaxStackSize"),
                Fixture.Inventory->InventorySlots, Before));
    }

    {
        FInventoryFixture Fixture(2, -1);
        const TArray<FInventorySlot> Before = Fixture.Inventory->InventorySlots;
        TestFalse(TEXT("negative MaxStackSize is rejected"), Fixture.Inventory->AddItem(ItemA, 1));
        TestTrue(
            TEXT("negative MaxStackSize leaves every slot unchanged"),
            TestSlotsExactlyEqual(
                *this, TEXT("negative MaxStackSize"),
                Fixture.Inventory->InventorySlots, Before));
    }

    {
        FInventoryFixture Fixture(2);
        TestTrue(TEXT("empty inventory accepts one item"), Fixture.Inventory->AddItem(ItemA, 1));
        TestEqual(TEXT("single item uses first slot"), Fixture.Inventory->InventorySlots[0].ItemID, ItemA);
        TestEqual(TEXT("single item quantity is one"), Fixture.Inventory->InventorySlots[0].Quantity, 1);
    }

    {
        FInventoryFixture Fixture(2);
        TestTrue(TEXT("empty inventory accepts exactly one stack"), Fixture.Inventory->AddItem(ItemA, 10));
        TestEqual(TEXT("exact stack has MaxStackSize"), Fixture.Inventory->InventorySlots[0].Quantity, 10);
        TestTrue(TEXT("second slot remains empty"), Fixture.Inventory->InventorySlots[1].IsEmpty());
    }

    {
        FInventoryFixture Fixture(2);
        TestTrue(TEXT("amount above one stack is accepted"), Fixture.Inventory->AddItem(ItemA, 11));
        TestEqual(TEXT("first split stack is full"), Fixture.Inventory->InventorySlots[0].Quantity, 10);
        TestEqual(TEXT("second split stack has remainder"), Fixture.Inventory->InventorySlots[1].Quantity, 1);
    }

    {
        FInventoryFixture Fixture(3);
        TestTrue(TEXT("amount exactly fills multiple empty slots"), Fixture.Inventory->AddItem(ItemA, 30));
        for (int32 SlotIndex = 0; SlotIndex < 3; ++SlotIndex)
        {
            TestEqual(TEXT("each exact-fill stack is full"), Fixture.Inventory->InventorySlots[SlotIndex].Quantity, 10);
        }
    }

    {
        FInventoryFixture Fixture(2);
        SetSlot(Fixture.Inventory, 0, ItemA, 4);
        TestTrue(TEXT("partial stack can be filled exactly"), Fixture.Inventory->AddItem(ItemA, 6));
        TestEqual(TEXT("partial stack becomes full"), Fixture.Inventory->InventorySlots[0].Quantity, 10);
        TestTrue(TEXT("exact partial-stack fill uses no empty slot"), Fixture.Inventory->InventorySlots[1].IsEmpty());
    }

    {
        FInventoryFixture Fixture(3);
        SetSlot(Fixture.Inventory, 0, ItemA, 8);
        SetSlot(Fixture.Inventory, 1, ItemA, 6);
        TestTrue(TEXT("multiple partial stacks accept the amount"), Fixture.Inventory->AddItem(ItemA, 6));
        TestEqual(TEXT("first partial stack is filled first"), Fixture.Inventory->InventorySlots[0].Quantity, 10);
        TestEqual(TEXT("second partial stack receives the remainder"), Fixture.Inventory->InventorySlots[1].Quantity, 10);
        TestTrue(TEXT("deterministic partial fill uses no new slot"), Fixture.Inventory->InventorySlots[2].IsEmpty());
    }

    {
        FInventoryFixture Fixture(3);
        SetSlot(Fixture.Inventory, 0, ItemA, 4);
        SetSlot(Fixture.Inventory, 1, ItemA, 7);
        TestTrue(TEXT("existing partial capacity is sufficient"), Fixture.Inventory->AddItem(ItemA, 5));
        TestEqual(TEXT("first compatible stack receives the amount"), Fixture.Inventory->InventorySlots[0].Quantity, 9);
        TestEqual(TEXT("later compatible stack is unchanged"), Fixture.Inventory->InventorySlots[1].Quantity, 7);
        TestTrue(TEXT("sufficient partial capacity avoids a new slot"), Fixture.Inventory->InventorySlots[2].IsEmpty());
    }

    {
        FInventoryFixture Fixture(3);
        SetSlot(Fixture.Inventory, 0, ItemA, 8);
        TestTrue(TEXT("remainder is split across empty slots"), Fixture.Inventory->AddItem(ItemA, 15));
        TestEqual(TEXT("existing stack is filled"), Fixture.Inventory->InventorySlots[0].Quantity, 10);
        TestEqual(TEXT("first new stack is full"), Fixture.Inventory->InventorySlots[1].Quantity, 10);
        TestEqual(TEXT("second new stack has final remainder"), Fixture.Inventory->InventorySlots[2].Quantity, 3);
    }

    {
        FInventoryFixture Fixture(2);
        SetSlot(Fixture.Inventory, 0, ItemA, 8);
        SetSlot(Fixture.Inventory, 1, ItemB, 10);
        const TArray<FInventorySlot> Before = Fixture.Inventory->InventorySlots;
        TestFalse(TEXT("insufficient total capacity rejects the whole request"), Fixture.Inventory->AddItem(ItemA, 3));
        TestTrue(
            TEXT("insufficient capacity preserves the complete array snapshot"),
            TestSlotsExactlyEqual(
                *this, TEXT("insufficient total capacity"),
                Fixture.Inventory->InventorySlots, Before));
        TestEqual(TEXT("insufficient capacity does not broadcast"), Fixture.BroadcastCount, 0);
    }

    {
        FInventoryFixture Fixture(2);
        SetSlot(Fixture.Inventory, 0, ItemA, 10);
        SetSlot(Fixture.Inventory, 1, ItemB, 10);
        const TArray<FInventorySlot> Before = Fixture.Inventory->InventorySlots;
        TestFalse(TEXT("full inventory rejects an item"), Fixture.Inventory->AddItem(ItemA, 1));
        TestTrue(
            TEXT("full inventory remains exactly unchanged"),
            TestSlotsExactlyEqual(
                *this, TEXT("full inventory"),
                Fixture.Inventory->InventorySlots, Before));
    }

    {
        FInventoryFixture Fixture(2);
        SetSlot(Fixture.Inventory, 0, ItemB, 3);
        TestTrue(TEXT("different item can use another slot"), Fixture.Inventory->AddItem(ItemA, 4));
        TestEqual(TEXT("different item ID is preserved"), Fixture.Inventory->InventorySlots[0].ItemID, ItemB);
        TestEqual(TEXT("different item quantity is preserved"), Fixture.Inventory->InventorySlots[0].Quantity, 3);
        TestEqual(TEXT("new item uses an empty slot"), Fixture.Inventory->InventorySlots[1].ItemID, ItemA);
        TestEqual(TEXT("new item has requested quantity"), Fixture.Inventory->InventorySlots[1].Quantity, 4);
    }

    {
        FInventoryFixture Fixture(3);
        TestTrue(TEXT("successful multi-stack request succeeds"), Fixture.Inventory->AddItem(ItemA, 21));
        TestEqual(TEXT("successful AddItem broadcasts exactly once"), Fixture.BroadcastCount, 1);
    }

    {
        FInventoryFixture Fixture(1);
        SetSlot(Fixture.Inventory, 0, ItemA, 10);
        TestFalse(TEXT("failed request remains failed"), Fixture.Inventory->AddItem(ItemA, 1));
        TestEqual(TEXT("failed AddItem never broadcasts"), Fixture.BroadcastCount, 0);
    }

    {
        FInventoryFixture Fixture(3);
        TestTrue(TEXT("first sequential add succeeds"), Fixture.Inventory->AddItem(ItemA, 7));
        TestTrue(TEXT("second sequential add succeeds"), Fixture.Inventory->AddItem(ItemA, 12));
        TestTrue(TEXT("third sequential add succeeds"), Fixture.Inventory->AddItem(ItemA, 6));
        TestStackLimit(*this, Fixture.Inventory, 10, TEXT("sequential adds"));
        TestEqual(TEXT("sequential adds preserve total quantity"), GetItemQuantity(Fixture.Inventory, ItemA), 25);
    }

    {
        FInventoryFixture Fixture(3);
        SetSlot(Fixture.Inventory, 0, ItemA, 2);
        SetSlot(Fixture.Inventory, 1, ItemA, 5);
        const int32 BeforeTotal = GetItemQuantity(Fixture.Inventory, ItemA);
        TestTrue(TEXT("successful total-count request succeeds"), Fixture.Inventory->AddItem(ItemA, 12));
        TestEqual(
            TEXT("successful request increases total by exactly Amount"),
            GetItemQuantity(Fixture.Inventory, ItemA),
            BeforeTotal + 12);
    }

    {
        FInventoryFixture Fixture(2);
        SetSlot(Fixture.Inventory, 0, ItemA, 9);
        SetSlot(Fixture.Inventory, 1, ItemB, 10);
        const int32 BeforeTotal = GetItemQuantity(Fixture.Inventory, ItemA);
        const TArray<FInventorySlot> Before = Fixture.Inventory->InventorySlots;
        TestFalse(TEXT("failed total-count request fails"), Fixture.Inventory->AddItem(ItemA, 2));
        TestEqual(
            TEXT("failed request leaves total quantity unchanged"),
            GetItemQuantity(Fixture.Inventory, ItemA),
            BeforeTotal);
        TestTrue(
            TEXT("failed total-count request also preserves all slot fields"),
            TestSlotsExactlyEqual(
                *this, TEXT("failed total count"),
                Fixture.Inventory->InventorySlots, Before));
    }

    return true;
}

#endif
