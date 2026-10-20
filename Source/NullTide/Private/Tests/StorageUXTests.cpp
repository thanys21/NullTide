#include "InventoryComponentTestTypes.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "Misc/AutomationTest.h"
#include "Storage/StorageComponent.h"
#include "Storage/StorageTransferLibrary.h"
#include "UI/InventoryDragVisualWidget.h"
#include "UI/StorageDragDropOperation.h"
#include "UI/StorageScreenWidget.h"
#include "UI/StorageSlotWidget.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
const TCHAR* G9ScreenPath = TEXT("/Game/LevelPrototyping/InventorySystem/UI/Widgets/W_StorageScreen.W_StorageScreen_C");
const TCHAR* G9SlotPath = TEXT("/Game/LevelPrototyping/InventorySystem/UI/Widgets/W_StorageSlot.W_StorageSlot_C");
const TCHAR* G9DragVisualPath = TEXT("/Game/LevelPrototyping/InventorySystem/UI/Widgets/W_InventoryDragVisual.W_InventoryDragVisual_C");
const TCHAR* G9WoodPath = TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Wood.Item_Wood_C");
const TCHAR* G9StonePath = TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Stone.Item_Stone_C");
const TCHAR* G9MeatPath = TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Meat.Item_Meat_C");
const TCHAR* G9SwordPath = TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_ShortSword.Item_ShortSword_C");

TSubclassOf<UItemDefinition> G9Stack() { return UInventoryStackableTestItemDefinition::StaticClass(); }   // max 50
TSubclassOf<UItemDefinition> G9Single() { return UInventoryOtherTestItemDefinition::StaticClass(); }       // max 1

/** An inventory and a storage (24 slots each) with event counters. */
struct FG9Containers
{
	TStrongObjectPtr<UInventoryComponent> Inventory{ NewObject<UInventoryComponent>(GetTransientPackage()) };
	TStrongObjectPtr<UStorageComponent> Storage{ NewObject<UStorageComponent>(GetTransientPackage()) };
	TStrongObjectPtr<UInventoryComponentTestListener> InventoryListener{ NewObject<UInventoryComponentTestListener>() };
	TStrongObjectPtr<UStorageComponentTestListener> StorageListener{ NewObject<UStorageComponentTestListener>() };

	FG9Containers()
	{
		Inventory->OnInventoryChanged.AddDynamic(InventoryListener.Get(), &UInventoryComponentTestListener::HandleInventoryChanged);
		Storage->OnStorageChanged.AddDynamic(StorageListener.Get(), &UStorageComponentTestListener::HandleStorageChanged);
	}

	/** Puts Quantity of a definition the inventory does not hold yet into the storage (exact G7 transfers). */
	void Seed(const TSubclassOf<UItemDefinition> Definition, const int32 Quantity) const
	{
		Inventory->TryAddDefinitionQuantity(Definition, Quantity);
		for (const UItemInstance* Item : Inventory->GetItemsSnapshot())
		{
			if (Item && Item->GetDefinitionClass() == Definition)
			{
				UStorageTransferLibrary::TransferInventoryToStorage(Inventory.Get(), Storage.Get(), Item->GetInstanceId());
			}
		}
	}

	/** Event counts and revisions, for exact "one event per side" checks. */
	FIntVector4 Counters() const
	{
		return FIntVector4(InventoryListener->EventCount, StorageListener->EventCount, Inventory->GetRevision(), Storage->GetRevision());
	}

	static int32 Quantity(const TArray<UItemInstance*>& Items, const TSubclassOf<UItemDefinition> Definition)
	{
		int32 Total = 0;
		for (const UItemInstance* Item : Items)
		{
			Total += Item && Item->GetDefinitionClass() == Definition ? Item->GetQuantity() : 0;
		}
		return Total;
	}
};

/** A stack's quantity, or -1 when it is missing (an assertion fails instead of crashing the run). */
int32 G9Qty(const UItemInstance* Item)
{
	return Item ? Item->GetQuantity() : -1;
}

FGuid G9FirstId(const TArray<UItemInstance*>& Items, const TSubclassOf<UItemDefinition> Definition)
{
	for (const UItemInstance* Item : Items)
	{
		if (Item && Item->GetDefinitionClass() == Definition)
		{
			return Item->GetInstanceId();
		}
	}
	return FGuid();
}

/** A transient game world with a world context, for creating real widgets (the G8 fixture). */
struct FG9World
{
	UWorld* World = nullptr;

	FG9World()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (World)
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
	}

	~FG9World()
	{
		if (World)
		{
			GEngine->ShutdownWorldNetDriver(World);
			World->DestroyWorld(true);
			GEngine->DestroyWorldContext(World);
		}
	}
};

UObject* G9ReadObject(const UObject* Object, const TCHAR* PropertyName)
{
	const FObjectPropertyBase* Property = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName) : nullptr;
	return Property ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
}

UClass* G9ReadClass(const UObject* Object, const TCHAR* PropertyName)
{
	const FClassProperty* Property = Object ? FindFProperty<FClassProperty>(Object->GetClass(), PropertyName) : nullptr;
	return Property ? Cast<UClass>(Property->GetPropertyValue_InContainer(Object)) : nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStorageUXMoveStackTest,
	"NullTide.Inventory.G9.MoveStackAsMuchAsFits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStorageUXMoveStackTest::RunTest(const FString& Parameters)
{
	FG9Containers C;
	C.Inventory->TryAddDefinitionQuantity(G9Stack(), 30);
	const FGuid Whole = G9FirstId(C.Inventory->GetItemsSnapshot(), G9Stack());
	FIntVector4 Before = C.Counters();
	FInventoryStorageTransferResult Result = UStorageTransferLibrary::MoveInventoryStackToStorage(C.Inventory.Get(), C.Storage.Get(), Whole);
	TestTrue(TEXT("A whole fit moves the identity (30)"), Result.IsSuccess() && Result.MovedQuantity == 30
		&& C.Storage->ContainsItem(Whole) && !C.Inventory->ContainsItem(Whole) && G9Qty(C.Storage->FindItemById(Whole)) == 30);
	TestTrue(TEXT("One event and one revision per side"), C.Counters() - Before == FIntVector4(1, 1, 1, 1));

	// The storage is full of stacks (23 singles + the 30 stack); only the 30 stack can take 20 more.
	for (int32 Index = 0; Index < 23; ++Index)
	{
		C.Seed(G9Single(), 1);
	}
	C.Inventory->TryAddDefinitionQuantity(G9Stack(), 25);
	const FGuid Partial = G9FirstId(C.Inventory->GetItemsSnapshot(), G9Stack());
	Before = C.Counters();
	Result = UStorageTransferLibrary::MoveInventoryStackToStorage(C.Inventory.Get(), C.Storage.Get(), Partial);
	TestTrue(FString::Printf(TEXT("A partial fit moves 20 and keeps the rest under the same id (moved %d)"), Result.MovedQuantity),
		Result.IsSuccess() && Result.MovedQuantity == 20 && C.Inventory->FindItemById(Partial) && G9Qty(C.Inventory->FindItemById(Partial)) == 5
		&& G9Qty(C.Storage->FindItemById(Whole)) == 50 && C.Storage->GetItemCount() == 24);
	TestTrue(TEXT("Partial: one event and one revision per side"), C.Counters() - Before == FIntVector4(1, 1, 1, 1));

	Before = C.Counters();
	Result = UStorageTransferLibrary::MoveInventoryStackToStorage(C.Inventory.Get(), C.Storage.Get(), Partial);
	TestTrue(TEXT("Nothing fits: DestinationFull, nothing changes"), Result.Result == EInventoryStorageTransferResult::DestinationFull
		&& C.Counters() == Before && G9Qty(C.Inventory->FindItemById(Partial)) == 5);

	// The reverse direction: storage to inventory, with room for part of a stack only.
	C.Inventory->TryAddDefinitionQuantity(G9Single(), 1);
	for (int32 Index = C.Inventory->GetItemCount(); Index < C.Inventory->GetCapacity(); ++Index)
	{
		C.Inventory->TryAddDefinitionQuantity(G9Single(), 1);
	}
	Before = C.Counters();
	Result = UStorageTransferLibrary::MoveStorageStackToInventory(C.Storage.Get(), C.Inventory.Get(), Whole);
	TestTrue(FString::Printf(TEXT("Back to a full inventory: only the 45 the 5 stack can take (moved %d)"), Result.MovedQuantity),
		Result.IsSuccess() && Result.MovedQuantity == 45 && G9Qty(C.Inventory->FindItemById(Partial)) == 50
		&& G9Qty(C.Storage->FindItemById(Whole)) == 5 && C.Counters() - Before == FIntVector4(1, 1, 1, 1));
	TestEqual(TEXT("A stale id is refused"), UStorageTransferLibrary::MoveInventoryStackToStorage(C.Inventory.Get(), C.Storage.Get(),
		FGuid::NewGuid()).Result, EInventoryStorageTransferResult::InvalidItemId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStorageUXMoveAllOfItemTest,
	"NullTide.Inventory.G9.MoveAllOfItem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStorageUXMoveAllOfItemTest::RunTest(const FString& Parameters)
{
	FG9Containers C;
	C.Inventory->TryAddDefinitionQuantity(G9Stack(), 70);   // 50 + 20
	C.Inventory->TryAddDefinitionQuantity(G9Single(), 1);
	C.Inventory->TryAddDefinitionQuantity(G9Single(), 1);
	const FIntVector4 Before = C.Counters();
	const FStorageTransferBatchResult Batch = UStorageTransferLibrary::MoveAllInventoryToStorage(C.Inventory.Get(), C.Storage.Get(), G9Stack());
	TestTrue(FString::Printf(TEXT("Every stack of the item moves (%d units, %d stacks, %d left)"), Batch.MovedQuantity, Batch.MovedStacks, Batch.StacksLeft),
		Batch.IsSuccess() && Batch.MovedQuantity == 70 && Batch.MovedStacks == 2 && Batch.StacksLeft == 0);
	TestTrue(TEXT("Other items stay"), FG9Containers::Quantity(C.Inventory->GetItemsSnapshot(), G9Single()) == 2
		&& FG9Containers::Quantity(C.Inventory->GetItemsSnapshot(), G9Stack()) == 0
		&& FG9Containers::Quantity(C.Storage->GetItemsSnapshot(), G9Stack()) == 70);
	TestTrue(TEXT("One event and one revision per side for the whole batch"), C.Counters() - Before == FIntVector4(1, 1, 1, 1));

	const FIntVector4 Again = C.Counters();
	const FStorageTransferBatchResult None = UStorageTransferLibrary::MoveAllInventoryToStorage(C.Inventory.Get(), C.Storage.Get(), G9Stack());
	TestTrue(TEXT("No stack of that item left: NothingToMove, no event"), None.Result == EInventoryStorageTransferResult::NothingToMove
		&& C.Counters() == Again);

	const FStorageTransferBatchResult Back = UStorageTransferLibrary::MoveAllStorageToInventory(C.Storage.Get(), C.Inventory.Get(), G9Stack());
	TestTrue(TEXT("And back the other way"), Back.IsSuccess() && Back.MovedQuantity == 70
		&& FG9Containers::Quantity(C.Inventory->GetItemsSnapshot(), G9Stack()) == 70 && C.Storage->GetItemCount() == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStorageUXMoveAllTest,
	"NullTide.Inventory.G9.MoveAll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStorageUXMoveAllTest::RunTest(const FString& Parameters)
{
	FG9Containers C;
	for (int32 Index = 0; Index < 22; ++Index)
	{
		C.Seed(G9Single(), 1);   // the storage has 2 free slots
	}
	C.Inventory->TryAddDefinitionQuantity(G9Single(), 1);
	C.Inventory->TryAddDefinitionQuantity(G9Single(), 1);
	C.Inventory->TryAddDefinitionQuantity(G9Single(), 1);
	C.Inventory->TryAddDefinitionQuantity(G9Stack(), 30);
	const TArray<UItemInstance*> InventoryBefore = C.Inventory->GetItemsSnapshot();
	FIntVector4 Before = C.Counters();
	const FStorageTransferBatchResult Batch = UStorageTransferLibrary::MoveAllInventoryToStorage(C.Inventory.Get(), C.Storage.Get());
	TestTrue(FString::Printf(TEXT("What fits moves, in order (%d units, %d stacks, %d left)"), Batch.MovedQuantity, Batch.MovedStacks, Batch.StacksLeft),
		Batch.IsSuccess() && Batch.MovedQuantity == 2 && Batch.MovedStacks == 2 && Batch.StacksLeft == 2 && C.Storage->GetItemCount() == 24);
	TestTrue(TEXT("The first two inventory stacks moved with their identity; the rest stayed in order"),
		C.Storage->ContainsItem(InventoryBefore[0]->GetInstanceId()) && C.Storage->ContainsItem(InventoryBefore[1]->GetInstanceId())
		&& C.Inventory->GetItemsSnapshot().Num() == 2 && C.Inventory->GetItemsSnapshot()[0] == InventoryBefore[2]
		&& C.Inventory->GetItemsSnapshot()[1] == InventoryBefore[3]);
	TestTrue(TEXT("One event and one revision per side"), C.Counters() - Before == FIntVector4(1, 1, 1, 1));

	Before = C.Counters();
	const FStorageTransferBatchResult Full = UStorageTransferLibrary::MoveAllInventoryToStorage(C.Inventory.Get(), C.Storage.Get());
	TestTrue(TEXT("Nothing fits: DestinationFull, no event"), Full.Result == EInventoryStorageTransferResult::DestinationFull
		&& Full.StacksLeft == 2 && C.Counters() == Before);
	FG9Containers Empty;
	TestEqual(TEXT("An empty source: NothingToMove"), UStorageTransferLibrary::MoveAllInventoryToStorage(Empty.Inventory.Get(), Empty.Storage.Get()).Result,
		EInventoryStorageTransferResult::NothingToMove);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStorageUXSplitStackTest,
	"NullTide.Inventory.G9.SplitStack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStorageUXSplitStackTest::RunTest(const FString& Parameters)
{
	FG9Containers C;
	C.Inventory->TryAddDefinitionQuantity(G9Stack(), 37);
	C.Inventory->TryAddDefinitionQuantity(G9Single(), 1);
	const FGuid Source = G9FirstId(C.Inventory->GetItemsSnapshot(), G9Stack());
	FIntVector4 Before = C.Counters();
	const FInventoryOperationResult Split = C.Inventory->TrySplitStack(Source);
	const TArray<UItemInstance*> Items = C.Inventory->GetItemsSnapshot();
	TestTrue(FString::Printf(TEXT("37 splits into 19 (source, same id) + 18 (new id, right after it): %d stacks"), Items.Num()),
		Split.IsSuccess() && Items.Num() == 3 && Items[0]->GetInstanceId() == Source && Items[0]->GetQuantity() == 19
		&& Items[1] == Split.Item && Items[1]->GetQuantity() == 18 && Split.ItemId != Source && Split.ItemId.IsValid()
		&& Items[2]->GetDefinitionClass() == G9Single());
	TestTrue(TEXT("One inventory event and revision"), C.Counters() - Before == FIntVector4(1, 0, 1, 0));

	Before = C.Counters();
	TestEqual(TEXT("A single item cannot split"), C.Inventory->TrySplitStack(Items[2]->GetInstanceId()).Result, EInventoryOperationResult::CannotSplit);
	for (int32 Index = C.Inventory->GetItemCount(); Index < C.Inventory->GetCapacity(); ++Index)
	{
		C.Inventory->TryAddDefinitionQuantity(G9Single(), 1);
	}
	const FIntVector4 Full = C.Counters();
	TestEqual(TEXT("No free slot: no split"), C.Inventory->TrySplitStack(Source).Result, EInventoryOperationResult::CapacityFull);
	TestTrue(TEXT("Refused splits change nothing"), C.Counters() == Full && G9Qty(C.Inventory->FindItemById(Source)) == 19);

	FG9Containers S;
	S.Seed(G9Stack(), 9);
	const FGuid Stored = G9FirstId(S.Storage->GetItemsSnapshot(), G9Stack());
	Before = S.Counters();
	const FInventoryOperationResult StorageSplit = S.Storage->TrySplitStack(Stored);
	TestTrue(TEXT("Storage: 9 splits into 5 + 4, one storage event"), StorageSplit.IsSuccess()
		&& G9Qty(S.Storage->FindItemById(Stored)) == 5 && StorageSplit.Quantity == 4 && S.Storage->GetItemCount() == 2
		&& S.Counters() - Before == FIntVector4(0, 1, 0, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStorageUXSlotInputMappingTest,
	"NullTide.Inventory.G9.SlotInputMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStorageUXSlotInputMappingTest::RunTest(const FString& Parameters)
{
	struct FCase { FKey Button; bool bShift; bool bCtrl; EStorageSlotAction Expected; const TCHAR* Name; };
	const FCase Cases[] = {
		{ EKeys::LeftMouseButton, false, false, EStorageSlotAction::Select, TEXT("LMB selects") },
		{ EKeys::LeftMouseButton, true, false, EStorageSlotAction::MoveStack, TEXT("Shift + LMB moves the stack") },
		{ EKeys::LeftMouseButton, false, true, EStorageSlotAction::MoveAllOfItem, TEXT("Ctrl + LMB moves every stack of the item") },
		{ EKeys::LeftMouseButton, true, true, EStorageSlotAction::MoveAllOfItem, TEXT("Shift + Ctrl + LMB: Ctrl wins") },
		{ EKeys::RightMouseButton, true, false, EStorageSlotAction::Split, TEXT("Shift + RMB splits") },
		{ EKeys::RightMouseButton, false, false, EStorageSlotAction::None, TEXT("RMB alone does nothing") },
		{ EKeys::RightMouseButton, false, true, EStorageSlotAction::None, TEXT("Ctrl + RMB does nothing") },
		{ EKeys::RightMouseButton, true, true, EStorageSlotAction::None, TEXT("Shift + Ctrl + RMB does nothing") },
		{ EKeys::MiddleMouseButton, true, false, EStorageSlotAction::None, TEXT("Other buttons do nothing") },
	};
	for (const FCase& Case : Cases)
	{
		TestEqual(Case.Name, UStorageSlotWidget::ResolveSlotAction(Case.Button, Case.bShift, Case.bCtrl), Case.Expected);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStorageUXScreenActionsTest,
	"NullTide.Inventory.G9.ScreenActions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStorageUXScreenActionsTest::RunTest(const FString& Parameters)
{
	UClass* ScreenClass = LoadClass<UStorageScreenWidget>(nullptr, G9ScreenPath);
	if (!TestNotNull(TEXT("W_StorageScreen loads"), ScreenClass)) { return false; }
	FG9World Fixture;
	if (!TestNotNull(TEXT("Transient world"), Fixture.World)) { return false; }
	FG9Containers C;
	C.Inventory->TryAddDefinitionQuantity(G9Stack(), 30);
	C.Inventory->TryAddDefinitionQuantity(G9Single(), 1);
	UStorageScreenWidget* Screen = CreateWidget<UStorageScreenWidget>(Fixture.World, ScreenClass);
	if (!TestNotNull(TEXT("The real screen is created"), Screen)) { return false; }
	Screen->SetPlayerInventoryOverride(C.Inventory.Get());
	Screen->OpenForStorage(C.Storage.Get(), FText::FromString(TEXT("Test Chest")));
	TestTrue(TEXT("Both sides are built (2 inventory slots, 0 storage slots)"), Screen->GetSlots(true).Num() == 2 && Screen->GetSlots(false).Num() == 0);

	const FGuid Stack = G9FirstId(C.Inventory->GetItemsSnapshot(), G9Stack());
	// Through the real slot → screen wiring (the delegates bound in RebuildPanel); the 30 stack sorts first.
	UStorageSlotWidget* StackSlot = Screen->GetSlots(true).IsEmpty() ? nullptr : Screen->GetSlots(true)[0];
	if (!TestTrue(TEXT("The 30 stack's slot is first (sorted by quantity)"), StackSlot && StackSlot->GetItemId() == Stack)) { return false; }
	StackSlot->HandleSlotInput(EKeys::LeftMouseButton, false, false);
	TestTrue(FString::Printf(TEXT("LMB on the slot selects and shows the quantity in the middle (%s)"), *Screen->GetDetailsQuantityText().ToString()),
		Screen->GetSelectedPlayerItemId() == Stack && StackSlot->IsSelected() && Screen->GetDetailsQuantityText().ToString().Contains(TEXT("30")));

	FIntVector4 Before = C.Counters();
	TestTrue(TEXT("Shift + LMB on the slot moves the stack once"), StackSlot->HandleSlotInput(EKeys::LeftMouseButton, true, false)
		&& C.Storage->ContainsItem(Stack) && C.Counters() - Before == FIntVector4(1, 1, 1, 1));
	TestTrue(FString::Printf(TEXT("Feedback after the move (%s); the panels follow"), *Screen->GetFeedbackText().ToString()),
		Screen->GetFeedbackText().ToString().Contains(TEXT("Moved 30")) && Screen->GetSlots(false).Num() == 1 && Screen->GetSlots(true).Num() == 1
		&& !Screen->GetSelectedPlayerItemId().IsValid());

	Before = C.Counters();
	TestTrue(TEXT("Shift + RMB splits in the storage"), Screen->HandleSlotAction(Stack, false, EStorageSlotAction::Split)
		&& C.Storage->GetItemCount() == 2 && C.Counters() - Before == FIntVector4(0, 1, 0, 1)
		&& Screen->GetFeedbackText().ToString().Contains(TEXT("15 + 15")));
	TestFalse(TEXT("A single item does not split"), Screen->HandleSlotAction(G9FirstId(C.Inventory->GetItemsSnapshot(), G9Single()), true,
		EStorageSlotAction::Split));
	TestTrue(TEXT("... with a message"), Screen->GetFeedbackText().ToString().Contains(TEXT("single")));

	// Drag and drop: the same panel does nothing, the other panel moves, a stale payload does nothing.
	UStorageDragDropOperation* Drag = NewObject<UStorageDragDropOperation>();
	Drag->ItemId = Stack;
	Drag->bFromPlayerSide = false;
	Before = C.Counters();
	TestFalse(TEXT("Dropping on the same panel does nothing"), Screen->HandleDropOnSide(Drag, false));
	TestTrue(TEXT("... and changes nothing"), C.Counters() == Before);
	TestTrue(TEXT("Dropping on the other panel moves the stack"), Screen->HandleDropOnSide(Drag, true) && C.Inventory->ContainsItem(Stack));
	Before = C.Counters();
	TestFalse(TEXT("A stale payload does nothing"), Screen->HandleDropOnSide(Drag, true));
	TestTrue(TEXT("... and changes nothing"), C.Counters() == Before);

	Screen->MoveAllToStorage();
	TestTrue(FString::Printf(TEXT("ALL → moves everything (%s)"), *Screen->GetFeedbackText().ToString()),
		C.Inventory->GetItemCount() == 0 && Screen->GetFeedbackText().ToString().Contains(TEXT("Moved")));
	Screen->MoveAllToStorage();
	TestTrue(TEXT("Again: nothing to move"), Screen->GetFeedbackText().ToString().Contains(TEXT("Nothing")));
	Before = C.Counters();
	Screen->MoveAllToInventory();
	TestTrue(TEXT("← ALL takes everything back in one event per side"), C.Storage->GetItemCount() == 0
		&& C.Counters() - Before == FIntVector4(1, 1, 1, 1) && Screen->GetFeedbackText().ToString().Contains(TEXT("Took")));

	// The 15 that moved back merged in the chest earlier, so the stack has a different id now.
	const FGuid CurrentStack = G9FirstId(C.Inventory->GetItemsSnapshot(), G9Stack());
	const FGuid Single = G9FirstId(C.Inventory->GetItemsSnapshot(), G9Single());
	Before = C.Counters();
	TestTrue(TEXT("Ctrl + LMB moves every stack of the item"), Screen->HandleSlotAction(CurrentStack, true, EStorageSlotAction::MoveAllOfItem)
		&& FG9Containers::Quantity(C.Storage->GetItemsSnapshot(), G9Stack()) == 30 && C.Inventory->ContainsItem(Single)
		&& C.Counters() - Before == FIntVector4(1, 1, 1, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStorageUXDefaultSortTest,
	"NullTide.Inventory.G9.DefaultSort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStorageUXDefaultSortTest::RunTest(const FString& Parameters)
{
	UClass* Wood = LoadClass<UItemDefinition>(nullptr, G9WoodPath);
	UClass* Stone = LoadClass<UItemDefinition>(nullptr, G9StonePath);
	UClass* Meat = LoadClass<UItemDefinition>(nullptr, G9MeatPath);
	UClass* Sword = LoadClass<UItemDefinition>(nullptr, G9SwordPath);
	if (!TestTrue(TEXT("Item_Wood, Item_Stone, Item_Meat, Item_ShortSword load"), Wood && Stone && Meat && Sword)) { return false; }
	// Two inventories, so the two Wood stacks have different quantities without depending on the stack limit.
	FG9Containers A;
	FG9Containers B;
	A.Inventory->TryAddDefinitionQuantity(Sword, 1);
	A.Inventory->TryAddDefinitionQuantity(G9Single(), 1);
	A.Inventory->TryAddDefinitionQuantity(Wood, 3);
	A.Inventory->TryAddDefinitionQuantity(Meat, 1);
	A.Inventory->TryAddDefinitionQuantity(Stone, 2);
	B.Inventory->TryAddDefinitionQuantity(Wood, 9);
	const TArray<UItemInstance*> ContainerOrder = A.Inventory->GetItemsSnapshot();
	TArray<UItemInstance*> Items = ContainerOrder;
	Items.Add(B.Inventory->GetItemsSnapshot()[0]);
	UStorageScreenWidget::SortForDisplay(Items);

	TArray<FString> Order;
	for (const UItemInstance* Item : Items)
	{
		Order.Add(FString::Printf(TEXT("%s x%d"), *Item->GetDefinitionClass()->GetName(), Item->GetQuantity()));
	}
	const FString Got = FString::Join(Order, TEXT(", "));
	TestTrue(FString::Printf(TEXT("Category, then name, then quantity: %s"), *Got), Items.Num() == 6
		&& Items[0]->GetDefinitionClass() == Stone
		&& Items[1]->GetDefinitionClass() == Wood && Items[1]->GetQuantity() == 9
		&& Items[2]->GetDefinitionClass() == Wood && Items[2]->GetQuantity() == 3
		&& Items[3]->GetDefinitionClass() == Meat
		&& Items[4]->GetDefinitionClass() == Sword
		&& Items[5]->GetDefinitionClass() == G9Single());
	TestTrue(TEXT("The container order is unchanged (display only)"), A.Inventory->GetItemsSnapshot() == ContainerOrder);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStorageUXProductionScreenTest,
	"NullTide.Inventory.G9.ProductionScreen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStorageUXProductionScreenTest::RunTest(const FString& Parameters)
{
	UClass* ScreenClass = LoadClass<UStorageScreenWidget>(nullptr, G9ScreenPath);
	UClass* SlotClass = LoadClass<UStorageSlotWidget>(nullptr, G9SlotPath);
	UClass* DragVisualClass = LoadClass<UInventoryDragVisualWidget>(nullptr, G9DragVisualPath);
	if (!TestTrue(TEXT("W_StorageScreen, W_StorageSlot and W_InventoryDragVisual load"), ScreenClass && SlotClass && DragVisualClass)) { return false; }
	FG9World Fixture;
	if (!TestNotNull(TEXT("Transient world"), Fixture.World)) { return false; }
	UStorageScreenWidget* Screen = CreateWidget<UStorageScreenWidget>(Fixture.World, ScreenClass);
	if (!TestNotNull(TEXT("Screen"), Screen)) { return false; }
	const TCHAR* Bound[] = {
		TEXT("PlayerItemsWrapBox"), TEXT("StorageItemsWrapBox"), TEXT("PlayerPanel"), TEXT("StoragePanel"), TEXT("PlayerCountText"),
		TEXT("StorageCountText"), TEXT("StorageTitleText"), TEXT("TransferAllToStorageButton"), TEXT("TransferAllToInventoryButton"),
		TEXT("CloseButton"), TEXT("DetailsIcon"), TEXT("DetailsItemName"), TEXT("DetailsCategory"), TEXT("DetailsQuantity"),
		TEXT("DetailsDescription"), TEXT("DetailsCapabilities"), TEXT("HintText"), TEXT("FeedbackText") };
	for (const TCHAR* Name : Bound)
	{
		TestNotNull(FString::Printf(TEXT("W_StorageScreen binds %s"), Name), G9ReadObject(Screen, Name));
	}
	TestTrue(TEXT("The removed selected-transfer buttons are gone"), Screen->WidgetTree
		&& !Screen->WidgetTree->FindWidget(TEXT("TransferToStorageButton")) && !Screen->WidgetTree->FindWidget(TEXT("TransferToInventoryButton")));
	TestTrue(TEXT("Slot class is W_StorageSlot"), G9ReadClass(Screen, TEXT("SlotWidgetClass")) == SlotClass);

	UStorageSlotWidget* StorageSlot = CreateWidget<UStorageSlotWidget>(Fixture.World, SlotClass);
	if (!TestNotNull(TEXT("Slot"), StorageSlot)) { return false; }
	for (const TCHAR* Name : { TEXT("ItemButton"), TEXT("SelectionBorder"), TEXT("ItemIcon"), TEXT("ItemName"), TEXT("QuantityText") })
	{
		TestNotNull(FString::Printf(TEXT("W_StorageSlot binds %s"), Name), G9ReadObject(StorageSlot, Name));
	}
	TestTrue(TEXT("The slot's drag visual is W_InventoryDragVisual"), G9ReadClass(StorageSlot, TEXT("DragVisualClass")) == DragVisualClass);
	const UButton* ItemButton = Cast<UButton>(G9ReadObject(StorageSlot, TEXT("ItemButton")));
	TestTrue(TEXT("The slot button takes no keyboard focus (Esc and I keep reaching the screen)"), ItemButton && !ItemButton->GetIsFocusable());

	// PIE findings: Tab closes like I and Esc; a real item icon keeps the 72 x 72 details size.
	TestTrue(TEXT("I, Tab and Esc close the screen; other keys do not"), UStorageScreenWidget::IsCloseKey(EKeys::I)
		&& UStorageScreenWidget::IsCloseKey(EKeys::Tab) && UStorageScreenWidget::IsCloseKey(EKeys::Escape) && !UStorageScreenWidget::IsCloseKey(EKeys::E));
	UClass* Stone = LoadClass<UItemDefinition>(nullptr, G9StonePath);
	FG9Containers C;
	if (TestNotNull(TEXT("Item_Stone loads"), Stone))
	{
		C.Inventory->TryAddDefinitionQuantity(Stone, 5);
		Screen->SetPlayerInventoryOverride(C.Inventory.Get());
		Screen->OpenForStorage(C.Storage.Get(), FText::FromString(TEXT("Test Chest")));
		Screen->HandleSlotAction(G9FirstId(C.Inventory->GetItemsSnapshot(), Stone), true, EStorageSlotAction::Select);
		const UImage* DetailsIcon = Cast<UImage>(G9ReadObject(Screen, TEXT("DetailsIcon")));
		TestTrue(FString::Printf(TEXT("The details icon shows Stone at 72 x 72 (%s)"),
				DetailsIcon ? *DetailsIcon->GetBrush().ImageSize.ToString() : TEXT("none")),
			DetailsIcon && DetailsIcon->GetBrush().GetResourceObject() && DetailsIcon->GetBrush().ImageSize.Equals(FVector2D(72.0, 72.0)));
	}
	return true;
}

#endif
