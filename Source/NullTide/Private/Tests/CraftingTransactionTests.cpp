#include "CraftingTestTypes.h"

#include "InventoryComponentTestTypes.h"
#include "Crafting/CraftingQueryLibrary.h"
#include "Crafting/CraftingRecipeDefinition.h"
#include "Crafting/CraftingTransaction.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryStacking.h"
#include "Items/Fragments/ItemFragment_Stackable.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

UCraftingSmallStackTestItemDefinition::UCraftingSmallStackTestItemDefinition()
{
	UItemFragment_Stackable* Stackable = CreateDefaultSubobject<UItemFragment_Stackable>(TEXT("Stackable"));
	Stackable->MaxStackSize = 20;
	ItemFragments.Add(Stackable);
}

void UCraftingTransactionTestListener::HandleInventoryChanged(int32 NewRevision)
{
	++EventCount;
	LastRevision = NewRevision;
	if (!Inventory)
	{
		return;
	}
	ObservedItemCount = Inventory->GetItemCount();
	if (bAttemptReentrantTransaction)
	{
		ReentrantTransactionResult = Inventory->TryApplyDefinitionTransaction(ReentrantRemovals, ReentrantAdditions);
	}
	if (ReentrantRecipe)
	{
		ReentrantCraftResult = FCraftingTransaction::TryCraftRecipeAtomic(Inventory, ReentrantRecipe.Get());
	}
}

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
struct FC34Stack
{
	FGuid Id;
	TSubclassOf<UItemDefinition> Definition;
	int32 Quantity = 0;

	bool operator==(const FC34Stack& Other) const
	{
		return Id == Other.Id && Definition == Other.Definition && Quantity == Other.Quantity;
	}
};

TSubclassOf<UItemDefinition> C34Big() { return UInventoryStackableTestItemDefinition::StaticClass(); }      // max 50
TSubclassOf<UItemDefinition> C34Small() { return UCraftingSmallStackTestItemDefinition::StaticClass(); }   // max 20
TSubclassOf<UItemDefinition> C34Single() { return UInventoryTestItemDefinition::StaticClass(); }
TSubclassOf<UItemDefinition> C34Other() { return UInventoryOtherTestItemDefinition::StaticClass(); }

TArray<FC34Stack> C34Capture(const UInventoryComponent* Inventory)
{
	TArray<FC34Stack> Stacks;
	for (const UItemInstance* Item : Inventory->GetItemsSnapshot())
	{
		FC34Stack& Stack = Stacks.AddDefaulted_GetRef();
		if (Item)
		{
			Stack.Id = Item->GetInstanceId();
			Stack.Definition = Item->GetDefinitionClass();
			Stack.Quantity = Item->GetQuantity();
		}
	}
	return Stacks;
}

/** Inventory plus a change listener; StartObserving after setup so only transaction events count. */
struct FC34Inventory
{
	TStrongObjectPtr<UInventoryComponent> Inventory;
	TStrongObjectPtr<UCraftingTransactionTestListener> Listener;

	explicit FC34Inventory(UInventoryComponent* InInventory = nullptr)
		: Inventory(InInventory ? InInventory : NewObject<UInventoryComponent>(GetTransientPackage()))
		, Listener(NewObject<UCraftingTransactionTestListener>(GetTransientPackage()))
	{
		Listener->Inventory = Inventory.Get();
	}

	UInventoryComponent* operator->() const { return Inventory.Get(); }
	UInventoryComponent* Get() const { return Inventory.Get(); }

	void StartObserving()
	{
		Inventory->OnInventoryChanged.AddDynamic(Listener.Get(), &UCraftingTransactionTestListener::HandleInventoryChanged);
	}
};

/** Verifies that a failed operation left contents, ItemIds, revision and events untouched. */
struct FC34UnchangedScope
{
	FAutomationTestBase& Test;
	const FC34Inventory& Inventory;
	const TCHAR* Label;
	TArray<FC34Stack> Stacks;
	int32 Revision;
	int32 Events;

	FC34UnchangedScope(FAutomationTestBase& InTest, const FC34Inventory& InInventory, const TCHAR* InLabel)
		: Test(InTest), Inventory(InInventory), Label(InLabel), Stacks(C34Capture(InInventory.Get()))
		, Revision(InInventory->GetRevision()), Events(InInventory.Listener->EventCount)
	{
	}

	~FC34UnchangedScope()
	{
		Test.TestTrue(FString::Printf(TEXT("%s: contents and ItemIds unchanged"), Label), C34Capture(Inventory.Get()) == Stacks);
		Test.TestEqual(FString::Printf(TEXT("%s: revision unchanged"), Label), Inventory->GetRevision(), Revision);
		Test.TestEqual(FString::Printf(TEXT("%s: no change event"), Label), Inventory.Listener->EventCount, Events);
	}
};

FInventoryDefinitionQuantity C34Request(TSubclassOf<UItemDefinition> Definition, int32 Quantity)
{
	FInventoryDefinitionQuantity Request;
	Request.DefinitionClass = Definition;
	Request.Quantity = Quantity;
	return Request;
}

FCraftingItemQuantity C34Entry(TSubclassOf<UItemDefinition> Definition, int32 Quantity)
{
	FCraftingItemQuantity Entry;
	Entry.ItemDefinition = Definition;
	Entry.Quantity = Quantity;
	return Entry;
}

TStrongObjectPtr<UCraftingTestRecipeDefinition> C34Recipe(const TArray<FCraftingItemQuantity>& Ingredients,
	const TArray<FCraftingItemQuantity>& Outputs)
{
	TStrongObjectPtr<UCraftingTestRecipeDefinition> Recipe(NewObject<UCraftingTestRecipeDefinition>(GetTransientPackage()));
	Recipe->Ingredients = Ingredients;
	Recipe->Outputs = Outputs;
	return Recipe;
}

bool C34Add(FAutomationTestBase& Test, UInventoryComponent* Inventory, TSubclassOf<UItemDefinition> Definition, int32 Quantity)
{
	return Test.TestTrue(FString::Printf(TEXT("Setup adds %d of %s"), Quantity, *GetNameSafe(Definition.Get())),
		Inventory->TryAddDefinitionQuantity(Definition, Quantity).IsSuccess());
}

/** Trims the stack at Index down to Quantity. */
bool C34Trim(FAutomationTestBase& Test, UInventoryComponent* Inventory, int32 Index, int32 Quantity)
{
	const TArray<UItemInstance*> Items = Inventory->GetItemsSnapshot();
	if (!Test.TestTrue(TEXT("Setup trim index exists"), Items.IsValidIndex(Index) && Items[Index]->GetQuantity() > Quantity))
	{
		return false;
	}
	return Test.TestTrue(TEXT("Setup trims a stack"),
		Inventory->TryRemoveItemQuantity(Items[Index]->GetInstanceId(), Items[Index]->GetQuantity() - Quantity).IsSuccess());
}

/** Builds [Big 5, Single, Big 10]: two Big stacks separated by another item, total 15. */
bool C34MakeSplitBig(FAutomationTestBase& Test, UInventoryComponent* Inventory)
{
	return C34Add(Test, Inventory, C34Big(), 50) && C34Add(Test, Inventory, C34Single(), 1)
		&& C34Add(Test, Inventory, C34Big(), 10) && C34Trim(Test, Inventory, 0, 5);
}

bool C34HasLayout(const TArray<FC34Stack>& Stacks, const TArray<TPair<TSubclassOf<UItemDefinition>, int32>>& Expected)
{
	if (Stacks.Num() != Expected.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < Expected.Num(); ++Index)
	{
		if (Stacks[Index].Definition != Expected[Index].Key || Stacks[Index].Quantity != Expected[Index].Value)
		{
			return false;
		}
	}
	return true;
}

bool C34TestResult(FAutomationTestBase& Test, const FString& What, EInventoryOperationResult Actual,
	EInventoryOperationResult Expected)
{
	return Test.TestEqual(What, static_cast<int32>(Actual), static_cast<int32>(Expected));
}

bool C34ContainsId(const TArray<FC34Stack>& Stacks, const FGuid& Id)
{
	return Stacks.ContainsByPredicate([&Id](const FC34Stack& Stack) { return Stack.Id == Id; });
}

bool C34MakeLegacyMode(FAutomationTestBase& Test, FC34Inventory& Inventory)
{
	FBoolProperty* LegacyModeProperty = FindFProperty<FBoolProperty>(UInventoryComponent::StaticClass(), TEXT("bLegacyInventoryMode"));
	if (!Test.TestNotNull(TEXT("Legacy mode flag exists"), LegacyModeProperty))
	{
		return false;
	}
	LegacyModeProperty->SetPropertyValue_InContainer(Inventory.Get(), true);
	return Test.TestFalse(TEXT("Legacy mode deactivates native authority"), Inventory->IsNativeAuthorityActive());
}

UInventoryCutoverTestManager* C34MakeNativeReady(FAutomationTestBase& Test)
{
	UInventoryCutoverTestManager* Inventory = NewObject<UInventoryCutoverTestManager>(GetTransientPackage());
	FBoolProperty* LegacyModeProperty = FindFProperty<FBoolProperty>(UInventoryComponent::StaticClass(), TEXT("bLegacyInventoryMode"));
	if (!Test.TestNotNull(TEXT("Legacy mode flag exists"), LegacyModeProperty))
	{
		return nullptr;
	}
	LegacyModeProperty->SetPropertyValue_InContainer(Inventory, true);
	return Test.TestTrue(TEXT("Native cutover succeeds"), Inventory->InitializeFromLegacyInventory().IsSuccess()) ? Inventory : nullptr;
}

TArray<TSubclassOf<UItemDefinition>> C34DefinitionsOf(const TArray<FC34Stack>& Stacks)
{
	TArray<TSubclassOf<UItemDefinition>> Definitions;
	for (const FC34Stack& Stack : Stacks)
	{
		Definitions.Add(Stack.Definition);
	}
	return Definitions;
}

UClass* C34LoadItem(const TCHAR* Id)
{
	return LoadClass<UItemDefinition>(nullptr,
		*FString::Printf(TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_%s.Item_%s_C"), Id, Id));
}

UClass* C34LoadRecipe(const TCHAR* Id)
{
	return LoadClass<UCraftingRecipeDefinition>(nullptr,
		*FString::Printf(TEXT("/Game/LevelPrototyping/CraftingSystem/Recipes/Recipe_%s.Recipe_%s_C"), Id, Id));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC34RemovalPlanTest,
	"NullTide.Crafting.C3C4.RemovalPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC34RemovalPlanTest::RunTest(const FString& Parameters)
{
	FC34Inventory Inventory;
	if (!C34MakeSplitBig(*this, Inventory.Get())) { return false; }
	const TArray<FC34Stack> Before = C34Capture(Inventory.Get());
	if (!TestTrue(TEXT("Setup layout is Big 5, Single, Big 10"),
		C34HasLayout(Before, { { C34Big(), 5 }, { C34Single(), 1 }, { C34Big(), 10 } }))) { return false; }

	// Planner level: works on a copy only.
	TArray<TObjectPtr<UItemInstance>> LiveItems;
	for (UItemInstance* Item : Inventory->GetItemsSnapshot()) { LiveItems.Add(Item); }
	TArray<FInventoryStackEntry> Entries;
	FInventoryStacking::MakeEntries(LiveItems, Entries);
	TestTrue(TEXT("Planner removes 12 Big"), FInventoryStacking::PlanRemoval(Entries, C34Big(), 12));
	TestEqual(TEXT("Emptied first stack is dropped from the projection"), Entries.Num(), 2);
	if (Entries.Num() == 2)
	{
		TestTrue(TEXT("Other definitions keep their position"), Entries[0].Item == LiveItems[1]);
		TestTrue(TEXT("Second Big stack is the same live stack"), Entries[1].Item == LiveItems[2]);
		TestEqual(TEXT("Second Big stack projects to 3"), Entries[1].Quantity, 3);
	}
	TestTrue(TEXT("Planning never touches live stacks"), C34Capture(Inventory.Get()) == Before);

	TArray<FInventoryStackEntry> Short;
	FInventoryStacking::MakeEntries(LiveItems, Short);
	TestFalse(TEXT("Planner rejects more than available"), FInventoryStacking::PlanRemoval(Short, C34Big(), 16));
	TestEqual(TEXT("Rejected plan leaves entries unchanged"), Short.Num(), 3);
	TestFalse(TEXT("Planner rejects zero"), FInventoryStacking::PlanRemoval(Short, C34Big(), 0));
	TestFalse(TEXT("Planner rejects null"), FInventoryStacking::PlanRemoval(Short, nullptr, 1));

	// Inventory level: Wood 5, 10 - remove 12 -> first removed, second 3.
	Inventory.StartObserving();
	const FInventoryTransactionResult Result = Inventory->TryApplyDefinitionTransaction({ C34Request(C34Big(), 12) }, {});
	TestTrue(TEXT("Removal-only transaction succeeds"), Result.IsSuccess());
	const TArray<FC34Stack> After = C34Capture(Inventory.Get());
	TestTrue(TEXT("Layout becomes Single, Big 3"), C34HasLayout(After, { { C34Single(), 1 }, { C34Big(), 3 } }));
	TestFalse(TEXT("Fully consumed stack ItemId is gone"), C34ContainsId(After, Before[0].Id));
	TestTrue(TEXT("Untouched stack keeps its ItemId"), After.Num() == 2 && After[0].Id == Before[1].Id);
	TestTrue(TEXT("Partially consumed stack keeps its ItemId"), After.Num() == 2 && After[1].Id == Before[2].Id);

	const FInventoryTransactionResult All = Inventory->TryApplyDefinitionTransaction({ C34Request(C34Big(), 3) }, {});
	TestTrue(TEXT("Removing the exact remaining total succeeds"), All.IsSuccess());
	TestEqual(TEXT("Only the other definition remains"), Inventory->GetItemCount(), 1);
	C34TestResult(*this, TEXT("Exact-class matching: the abstract root matches nothing"),
		Inventory->TryApplyDefinitionTransaction({ C34Request(UItemDefinition::StaticClass(), 1) }, {}).Result,
		EInventoryOperationResult::InsufficientQuantity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC34ProjectedCapacityTest,
	"NullTide.Crafting.C3C4.ProjectedCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC34ProjectedCapacityTest::RunTest(const FString& Parameters)
{
	// A removed stack frees the slot an output needs.
	FC34Inventory Freed;
	if (!C34Add(*this, Freed.Get(), C34Big(), 5) || !C34Add(*this, Freed.Get(), C34Single(), 23)) { return false; }
	Freed.StartObserving();
	C34TestResult(*this, TEXT("Advisory batch query on current state says full"),
		Freed->CanAddDefinitionQuantities({ C34Request(C34Other(), 1) }).Result, EInventoryOperationResult::CapacityFull);
	TestTrue(TEXT("Removing a whole stack frees capacity for a new output stack"),
		Freed->TryApplyDefinitionTransaction({ C34Request(C34Big(), 5) }, { C34Request(C34Other(), 1) }).IsSuccess());
	TestEqual(TEXT("Still 24 occupied slots"), Freed->GetItemCount(), 24);
	TestEqual(TEXT("Big is gone"), Freed->GetTotalQuantity(C34Big()), 0);
	TestEqual(TEXT("Output was created"), Freed->GetTotalQuantity(C34Other()), 1);

	// Output merges into a stack that removal left partial.
	FC34Inventory Partial;
	if (!C34Add(*this, Partial.Get(), C34Small(), 20) || !C34Add(*this, Partial.Get(), C34Single(), 23)) { return false; }
	const FGuid SmallId = C34Capture(Partial.Get())[0].Id;
	Partial.StartObserving();
	TestTrue(TEXT("Output merges into the post-removal partial stack"),
		Partial->TryApplyDefinitionTransaction({ C34Request(C34Small(), 8) }, { C34Request(C34Small(), 6) }).IsSuccess());
	const TArray<FC34Stack> PartialAfter = C34Capture(Partial.Get());
	TestTrue(TEXT("20 - 8 + 6 = 18 in the same stack"), PartialAfter[0].Id == SmallId && PartialAfter[0].Quantity == 18);
	TestEqual(TEXT("No new slot used"), Partial->GetItemCount(), 24);

	// Non-stackable output in the slot a non-stackable ingredient frees.
	FC34Inventory SingleFull;
	if (!C34Add(*this, SingleFull.Get(), C34Single(), 24)) { return false; }
	SingleFull.StartObserving();
	TestTrue(TEXT("Non-stackable output uses the freed slot"),
		SingleFull->TryApplyDefinitionTransaction({ C34Request(C34Single(), 1) }, { C34Request(C34Other(), 1) }).IsSuccess());
	{
		FC34UnchangedScope Unchanged(*this, SingleFull, TEXT("Output larger than freed space"));
		const FInventoryTransactionResult Result =
			SingleFull->TryApplyDefinitionTransaction({ C34Request(C34Single(), 1) }, { C34Request(C34Other(), 2) });
		C34TestResult(*this, TEXT("Two outputs cannot fit one freed slot"), Result.Result, EInventoryOperationResult::CapacityFull);
		TestTrue(TEXT("Failure is reported on the addition side"), Result.FailedSide == EInventoryTransactionSide::Addition);
		TestEqual(TEXT("Failure names the addition index"), Result.FailedIndex, 0);
	}

	// Later additions see the slots planned by earlier additions.
	FC34Inventory Sequential;
	if (!C34Add(*this, Sequential.Get(), C34Big(), 48) || !C34Add(*this, Sequential.Get(), C34Single(), 23)) { return false; }
	Sequential.StartObserving();
	{
		FC34UnchangedScope Unchanged(*this, Sequential, TEXT("Additions compete for one freed slot"));
		const FInventoryTransactionResult Result = Sequential->TryApplyDefinitionTransaction(
			{ C34Request(C34Single(), 1) }, { C34Request(C34Big(), 5), C34Request(C34Other(), 1) });
		C34TestResult(*this, TEXT("Overflow stack plus a new output need two slots"), Result.Result, EInventoryOperationResult::CapacityFull);
		TestEqual(TEXT("The second addition is the one that no longer fits"), Result.FailedIndex, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC34AtomicSuccessTest,
	"NullTide.Crafting.C3C4.AtomicSuccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC34AtomicSuccessTest::RunTest(const FString& Parameters)
{
	// [Big 5, Single#1, Big 10, Small 15, Single#2]
	FC34Inventory Inventory;
	if (!C34MakeSplitBig(*this, Inventory.Get())) { return false; }
	if (!C34Add(*this, Inventory.Get(), C34Small(), 15) || !C34Add(*this, Inventory.Get(), C34Single(), 1)) { return false; }
	const TArray<FC34Stack> Before = C34Capture(Inventory.Get());
	Inventory.StartObserving();
	const int32 RevisionBefore = Inventory->GetRevision();

	const FInventoryTransactionResult Result = Inventory->TryApplyDefinitionTransaction(
		{ C34Request(C34Big(), 12), C34Request(C34Single(), 1), C34Request(C34Small(), 5) },
		{ C34Request(C34Other(), 1), C34Request(C34Small(), 8), C34Request(C34Big(), 7) });
	TestTrue(TEXT("Multi-ingredient, multi-stack, multi-output transaction succeeds"), Result.IsSuccess());
	TestTrue(TEXT("Success reports no failed side"), Result.FailedSide == EInventoryTransactionSide::None);

	const TArray<FC34Stack> After = C34Capture(Inventory.Get());
	TestTrue(TEXT("Final layout is Big 10, Small 18, Single, Other"),
		C34HasLayout(After, { { C34Big(), 10 }, { C34Small(), 18 }, { C34Single(), 1 }, { C34Other(), 1 } }));
	if (After.Num() == 4)
	{
		TestEqual(TEXT("Big merges into the partially consumed second Big stack"), After[0].Id, Before[2].Id);
		TestEqual(TEXT("Small merges into its partially consumed stack"), After[1].Id, Before[3].Id);
		TestEqual(TEXT("The later Single survives; the earliest was consumed"), After[2].Id, Before[4].Id);
		TestFalse(TEXT("Output stack has a fresh ItemId"), C34ContainsId(Before, After[3].Id));
	}
	TestEqual(TEXT("Exactly one revision"), Inventory->GetRevision(), RevisionBefore + 1);
	TestEqual(TEXT("Exactly one event"), Inventory.Listener->EventCount, 1);

	// The crafting helper runs the same transaction from a recipe.
	const TStrongObjectPtr<UCraftingTestRecipeDefinition> Recipe = C34Recipe(
		{ C34Entry(C34Big(), 4), C34Entry(C34Small(), 3) }, { C34Entry(C34Other(), 1) });
	const FCraftingTransactionResult Craft = FCraftingTransaction::TryCraftRecipeAtomic(Inventory.Get(), Recipe.Get());
	TestTrue(TEXT("Recipe craft succeeds"), Craft.IsSuccess());
	TestEqual(TEXT("Big 10 - 4"), Inventory->GetTotalQuantity(C34Big()), 6);
	TestEqual(TEXT("Small 18 - 3"), Inventory->GetTotalQuantity(C34Small()), 15);
	TestEqual(TEXT("Second Other created"), Inventory->GetTotalQuantity(C34Other()), 2);
	TestEqual(TEXT("Second craft adds exactly one more event"), Inventory.Listener->EventCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC34AtomicFailureTest,
	"NullTide.Crafting.C3C4.AtomicFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC34AtomicFailureTest::RunTest(const FString& Parameters)
{
	FC34Inventory Inventory;
	if (!C34MakeSplitBig(*this, Inventory.Get())) { return false; }
	if (!C34Add(*this, Inventory.Get(), C34Single(), 21)) { return false; }  // 24 occupied: Big 5, Single, Big 10, 21 Single
	Inventory.StartObserving();

	struct FCase
	{
		const TCHAR* Name;
		TArray<FInventoryDefinitionQuantity> Removals;
		TArray<FInventoryDefinitionQuantity> Additions;
		EInventoryOperationResult Expected;
		EInventoryTransactionSide Side;
		int32 Index;
	};
	const TArray<FCase> Cases = {
		{ TEXT("Insufficient ingredient across stacks"), { C34Request(C34Big(), 16) }, {},
			EInventoryOperationResult::InsufficientQuantity, EInventoryTransactionSide::Removal, 0 },
		{ TEXT("Second ingredient insufficient after the first passes"), { C34Request(C34Big(), 1), C34Request(C34Other(), 1) }, {},
			EInventoryOperationResult::InsufficientQuantity, EInventoryTransactionSide::Removal, 1 },
		{ TEXT("Duplicate removals are summed (8 + 8 > 15)"), { C34Request(C34Big(), 8), C34Request(C34Big(), 8) }, {},
			EInventoryOperationResult::InsufficientQuantity, EInventoryTransactionSide::Removal, 0 },
		{ TEXT("Output cannot fit after removals"), { C34Request(C34Big(), 1) }, { C34Request(C34Other(), 1) },
			EInventoryOperationResult::CapacityFull, EInventoryTransactionSide::Addition, 0 },
		{ TEXT("Duplicate outputs are summed (Big 5 frees one slot; Small 10 + 10 + 10 needs two stacks of max 20)"),
			{ C34Request(C34Big(), 5) }, { C34Request(C34Small(), 10), C34Request(C34Small(), 10), C34Request(C34Small(), 10) },
			EInventoryOperationResult::CapacityFull, EInventoryTransactionSide::Addition, 0 },
		{ TEXT("Zero removal quantity"), { C34Request(C34Big(), 0) }, {},
			EInventoryOperationResult::InvalidQuantity, EInventoryTransactionSide::Removal, 0 },
		{ TEXT("Negative addition quantity"), { C34Request(C34Big(), 1) }, { C34Request(C34Big(), -1) },
			EInventoryOperationResult::InvalidQuantity, EInventoryTransactionSide::Addition, 0 },
		{ TEXT("Null removal definition"), { C34Request(nullptr, 1) }, {},
			EInventoryOperationResult::InvalidDefinition, EInventoryTransactionSide::Removal, 0 },
		{ TEXT("Null addition definition"), { C34Request(C34Big(), 1) }, { C34Request(nullptr, 1) },
			EInventoryOperationResult::InvalidDefinition, EInventoryTransactionSide::Addition, 0 },
		{ TEXT("Abstract addition definition"), { C34Request(C34Big(), 1) }, { C34Request(UItemDefinition::StaticClass(), 1) },
			EInventoryOperationResult::InvalidDefinition, EInventoryTransactionSide::Addition, 0 },
		{ TEXT("Invalid addition definition data"), { C34Request(C34Big(), 1) },
			{ C34Request(C34Big(), 1), C34Request(UInventoryInvalidDataTestDefinition::StaticClass(), 1) },
			EInventoryOperationResult::InvalidDefinitionData, EInventoryTransactionSide::Addition, 1 },
		{ TEXT("Empty transaction"), {}, {},
			EInventoryOperationResult::InvalidQuantity, EInventoryTransactionSide::None, INDEX_NONE }
	};
	for (const FCase& Case : Cases)
	{
		FC34UnchangedScope Unchanged(*this, Inventory, Case.Name);
		const FInventoryTransactionResult Result = Inventory->TryApplyDefinitionTransaction(Case.Removals, Case.Additions);
		C34TestResult(*this, FString::Printf(TEXT("%s: result"), Case.Name), Result.Result, Case.Expected);
		TestTrue(FString::Printf(TEXT("%s: side"), Case.Name), Result.FailedSide == Case.Side);
		TestEqual(FString::Printf(TEXT("%s: index"), Case.Name), Result.FailedIndex, Case.Index);
	}

	// Duplicate aggregation also succeeds when the sum fits.
	FC34Inventory Summed;
	if (!C34MakeSplitBig(*this, Summed.Get())) { return false; }
	Summed.StartObserving();
	TestTrue(TEXT("Duplicate removals summing to 12 succeed"),
		Summed->TryApplyDefinitionTransaction({ C34Request(C34Big(), 6), C34Request(C34Big(), 6) },
			{ C34Request(C34Small(), 15), C34Request(C34Small(), 15) }).IsSuccess());
	TestTrue(TEXT("Summed removal and summed output layout"),
		C34HasLayout(C34Capture(Summed.Get()), { { C34Single(), 1 }, { C34Big(), 3 }, { C34Small(), 20 }, { C34Small(), 10 } }));
	TestEqual(TEXT("Summed transaction emits one event"), Summed.Listener->EventCount, 1);

	// Not initialized.
	FC34Inventory Legacy;
	if (!C34MakeLegacyMode(*this, Legacy)) { return false; }
	Legacy.StartObserving();
	{
		FC34UnchangedScope Unchanged(*this, Legacy, TEXT("Inactive authority"));
		C34TestResult(*this, TEXT("Inactive native authority reports NotInitialized"),
			Legacy->TryApplyDefinitionTransaction({}, { C34Request(C34Big(), 1) }).Result, EInventoryOperationResult::NotInitialized);
	}

	// Busy: a reentrant transaction and a reentrant craft during another mutation's broadcast.
	FC34Inventory Busy;
	if (!C34Add(*this, Busy.Get(), C34Big(), 10)) { return false; }
	const TStrongObjectPtr<UCraftingTestRecipeDefinition> BusyRecipe = C34Recipe({ C34Entry(C34Big(), 1) }, { C34Entry(C34Other(), 1) });
	Busy.Listener->bAttemptReentrantTransaction = true;
	Busy.Listener->ReentrantRemovals = { C34Request(C34Big(), 1) };
	Busy.Listener->ReentrantRecipe = BusyRecipe.Get();
	Busy.StartObserving();
	const int32 BusyRevision = Busy->GetRevision();
	TestTrue(TEXT("Outer mutation succeeds"), Busy->TryAddDefinitionQuantity(C34Big(), 2).IsSuccess());
	C34TestResult(*this, TEXT("Reentrant transaction reports Busy"), Busy.Listener->ReentrantTransactionResult.Result, EInventoryOperationResult::Busy);
	TestTrue(TEXT("Reentrant craft reports InventoryBusy"),
		Busy.Listener->ReentrantCraftResult.Result == ECraftingTransactionResult::InventoryBusy);
	TestEqual(TEXT("Only the outer mutation changed quantity"), Busy->GetTotalQuantity(C34Big()), 12);
	TestEqual(TEXT("Only the outer mutation changed revision"), Busy->GetRevision(), BusyRevision + 1);
	TestEqual(TEXT("Only the outer mutation broadcast"), Busy.Listener->EventCount, 1);

	// Crafting-side result mapping, each failure leaving the inventory unchanged.
	{
		FC34UnchangedScope Unchanged(*this, Inventory, TEXT("Craft failures"));
		const TStrongObjectPtr<UCraftingTestRecipeDefinition> Short = C34Recipe(
			{ C34Entry(C34Big(), 1), C34Entry(C34Small(), 1) }, { C34Entry(C34Big(), 1) });
		const FCraftingTransactionResult ShortCraft = FCraftingTransaction::TryCraftRecipeAtomic(Inventory.Get(), Short.Get());
		TestTrue(TEXT("Missing ingredient maps to InsufficientIngredients"),
			ShortCraft.Result == ECraftingTransactionResult::InsufficientIngredients);
		TestEqual(TEXT("Recipe ingredient index is reported"), ShortCraft.EntryIndex, 1);

		const TStrongObjectPtr<UCraftingTestRecipeDefinition> NoRoom = C34Recipe(
			{ C34Entry(C34Big(), 1) }, { C34Entry(C34Big(), 1), C34Entry(C34Other(), 1) });
		const FCraftingTransactionResult NoRoomCraft = FCraftingTransaction::TryCraftRecipeAtomic(Inventory.Get(), NoRoom.Get());
		TestTrue(TEXT("Outputs without room map to OutputCapacityFull"),
			NoRoomCraft.Result == ECraftingTransactionResult::OutputCapacityFull);
		TestEqual(TEXT("Recipe output index is reported"), NoRoomCraft.EntryIndex, 1);

		const TStrongObjectPtr<UCraftingTestRecipeDefinition> Invalid = C34Recipe({ C34Entry(C34Big(), 1) }, {});
		const FCraftingTransactionResult InvalidCraft = FCraftingTransaction::TryCraftRecipeAtomic(Inventory.Get(), Invalid.Get());
		TestTrue(TEXT("Invalid recipe is rejected before the inventory"), InvalidCraft.Result == ECraftingTransactionResult::InvalidRecipe);
		TestFalse(TEXT("Invalid recipe carries the validation diagnostic"), InvalidCraft.Diagnostic.IsEmpty());
		TestTrue(TEXT("Null recipe class is rejected"),
			FCraftingTransaction::TryCraftRecipeAtomic(Inventory.Get(), TSubclassOf<UCraftingRecipeDefinition>()).Result
				== ECraftingTransactionResult::InvalidRecipe);
		TestTrue(TEXT("Null inventory is rejected"),
			FCraftingTransaction::TryCraftRecipeAtomic(nullptr, Short.Get()).Result == ECraftingTransactionResult::InvalidInventory);
	}
	TestTrue(TEXT("Inactive authority maps to InvalidInventory"),
		FCraftingTransaction::TryCraftRecipeAtomic(Legacy.Get(), BusyRecipe.Get()).Result == ECraftingTransactionResult::InvalidInventory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC34OverlapTest,
	"NullTide.Crafting.C3C4.IngredientOutputOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC34OverlapTest::RunTest(const FString& Parameters)
{
	// Wood 50/50, recipe Wood x10 -> Wood x5, inventory otherwise full.
	FC34Inventory Stackable;
	if (!C34Add(*this, Stackable.Get(), C34Big(), 50) || !C34Add(*this, Stackable.Get(), C34Single(), 23)) { return false; }
	const FGuid BigId = C34Capture(Stackable.Get())[0].Id;
	const TStrongObjectPtr<UCraftingTestRecipeDefinition> Overlap = C34Recipe({ C34Entry(C34Big(), 10) }, { C34Entry(C34Big(), 5) });
	Stackable.StartObserving();
	TestTrue(TEXT("C2 advisory output check is conservative and says full"),
		UCraftingQueryLibrary::CheckOutputCapacity(Stackable.Get(), Overlap.Get()).Result == ECraftingQueryResult::OutputCapacityFull);
	TestTrue(TEXT("The authoritative craft succeeds"), FCraftingTransaction::TryCraftRecipeAtomic(Stackable.Get(), Overlap.Get()).IsSuccess());
	const TArray<FC34Stack> StackableAfter = C34Capture(Stackable.Get());
	TestTrue(TEXT("50 - 10 + 5 = 45 in the same stack"), StackableAfter[0].Id == BigId && StackableAfter[0].Quantity == 45);
	TestEqual(TEXT("Slot count unchanged"), Stackable->GetItemCount(), 24);
	TestEqual(TEXT("One event"), Stackable.Listener->EventCount, 1);

	// Non-stackable: consuming one item frees the only slot the output can use.
	FC34Inventory Single;
	if (!C34Add(*this, Single.Get(), C34Single(), 24)) { return false; }
	const FGuid FirstSingle = C34Capture(Single.Get())[0].Id;
	const TStrongObjectPtr<UCraftingTestRecipeDefinition> Swap = C34Recipe({ C34Entry(C34Single(), 1) }, { C34Entry(C34Other(), 1) });
	Single.StartObserving();
	TestTrue(TEXT("C2 advisory output check says full"),
		UCraftingQueryLibrary::CheckOutputCapacity(Single.Get(), Swap.Get()).Result == ECraftingQueryResult::OutputCapacityFull);
	TestTrue(TEXT("Craft uses the slot freed by its own ingredient"),
		FCraftingTransaction::TryCraftRecipeAtomic(Single.Get(), Swap.Get()).IsSuccess());
	TestEqual(TEXT("23 Single left"), Single->GetTotalQuantity(C34Single()), 23);
	TestEqual(TEXT("1 Other created"), Single->GetTotalQuantity(C34Other()), 1);
	TestFalse(TEXT("The earliest Single was the one consumed"), C34ContainsId(C34Capture(Single.Get()), FirstSingle));

	// Same definition, ingredient consumes the whole stack: the output becomes a new stack with a new ItemId.
	FC34Inventory Replaced;
	if (!C34Add(*this, Replaced.Get(), C34Big(), 10)) { return false; }
	const FGuid ReplacedId = C34Capture(Replaced.Get())[0].Id;
	Replaced.StartObserving();
	TestTrue(TEXT("Consume all 10 and output 5 succeeds"),
		Replaced->TryApplyDefinitionTransaction({ C34Request(C34Big(), 10) }, { C34Request(C34Big(), 5) }).IsSuccess());
	const TArray<FC34Stack> ReplacedAfter = C34Capture(Replaced.Get());
	TestTrue(TEXT("One Big stack of 5"), C34HasLayout(ReplacedAfter, { { C34Big(), 5 } }));
	TestTrue(TEXT("Fully consumed identity is not reused or cloned"), ReplacedAfter.Num() == 1 && ReplacedAfter[0].Id != ReplacedId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC34RevisionEventTest,
	"NullTide.Crafting.C3C4.RevisionAndEventContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC34RevisionEventTest::RunTest(const FString& Parameters)
{
	FC34Inventory Inventory;
	if (!C34MakeSplitBig(*this, Inventory.Get())) { return false; }
	if (!C34Add(*this, Inventory.Get(), C34Small(), 15)) { return false; }
	Inventory.StartObserving();
	const int32 RevisionBefore = Inventory->GetRevision();

	// Consumes two Big stacks and one Small stack partially; creates two output definitions and one merge.
	const FInventoryTransactionResult Result = Inventory->TryApplyDefinitionTransaction(
		{ C34Request(C34Big(), 15), C34Request(C34Small(), 15) },
		{ C34Request(C34Other(), 1), C34Request(C34Single(), 2), C34Request(C34Small(), 25) });
	TestTrue(TEXT("Large transaction succeeds"), Result.IsSuccess());
	TestEqual(TEXT("Revision increments exactly once"), Inventory->GetRevision(), RevisionBefore + 1);
	TestEqual(TEXT("OnInventoryChanged fires exactly once"), Inventory.Listener->EventCount, 1);
	TestEqual(TEXT("The event carries the committed revision"), Inventory.Listener->LastRevision, Inventory->GetRevision());
	TestEqual(TEXT("The handler observes only the final contents"), Inventory.Listener->ObservedItemCount, Inventory->GetItemCount());
	TestTrue(TEXT("Final layout: Single, Other, Single, Single, Small 20, Small 5"),
		C34HasLayout(C34Capture(Inventory.Get()), { { C34Single(), 1 }, { C34Other(), 1 }, { C34Single(), 1 }, { C34Single(), 1 },
			{ C34Small(), 20 }, { C34Small(), 5 } }));

	TestTrue(TEXT("A second transaction succeeds"),
		Inventory->TryApplyDefinitionTransaction({ C34Request(C34Single(), 3) }, {}).IsSuccess());
	TestEqual(TEXT("Each transaction adds exactly one revision"), Inventory->GetRevision(), RevisionBefore + 2);
	TestEqual(TEXT("Each transaction adds exactly one event"), Inventory.Listener->EventCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC34IdentityTest,
	"NullTide.Crafting.C3C4.IdentityPreservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC34IdentityTest::RunTest(const FString& Parameters)
{
	// [Big 5, Single, Big 10, Small 18, Other]
	FC34Inventory Inventory;
	if (!C34MakeSplitBig(*this, Inventory.Get())) { return false; }
	if (!C34Add(*this, Inventory.Get(), C34Small(), 18) || !C34Add(*this, Inventory.Get(), C34Other(), 1)) { return false; }
	const TArray<FC34Stack> Before = C34Capture(Inventory.Get());
	Inventory.StartObserving();
	TestTrue(TEXT("Transaction succeeds"), Inventory->TryApplyDefinitionTransaction(
		{ C34Request(C34Big(), 7) }, { C34Request(C34Small(), 5), C34Request(C34Big(), 1) }).IsSuccess());
	const TArray<FC34Stack> After = C34Capture(Inventory.Get());
	// Big 5 consumed; Big 10 -> 8 -> +1 = 9; Small 18 +2 = 20 and +3 overflow.
	TestTrue(TEXT("Layout: Single, Big 9, Small 20, Other, Small 3"),
		C34HasLayout(After, { { C34Single(), 1 }, { C34Big(), 9 }, { C34Small(), 20 }, { C34Other(), 1 }, { C34Small(), 3 } }));
	if (After.Num() == 5)
	{
		TestEqual(TEXT("Untouched Single keeps its ItemId"), After[0].Id, Before[1].Id);
		TestEqual(TEXT("Partially consumed then merged Big keeps its ItemId"), After[1].Id, Before[2].Id);
		TestEqual(TEXT("Merge destination keeps its ItemId"), After[2].Id, Before[3].Id);
		TestEqual(TEXT("Untouched Other keeps its ItemId"), After[3].Id, Before[4].Id);
		TestFalse(TEXT("Overflow stack has a fresh ItemId"), C34ContainsId(Before, After[4].Id));
		TestTrue(TEXT("Overflow ItemId is valid"), After[4].Id.IsValid());
	}
	TestFalse(TEXT("Fully consumed ItemId disappears"), C34ContainsId(After, Before[0].Id));
	TSet<FGuid> Unique;
	for (const FC34Stack& Stack : After) { Unique.Add(Stack.Id); }
	TestEqual(TEXT("No ItemId is duplicated"), Unique.Num(), After.Num());

	// Legacy projection: rebuilt to match the committed stacks on success, untouched on failure.
	UInventoryCutoverTestManager* NativeReady = C34MakeNativeReady(*this);
	if (!NativeReady) { return false; }
	FC34Inventory Native(NativeReady);
	if (!C34Add(*this, Native.Get(), C34Big(), 10) || !C34Add(*this, Native.Get(), C34Single(), 1)) { return false; }
	Native.StartObserving();
	TestTrue(TEXT("Native-ready transaction succeeds"), Native->TryApplyDefinitionTransaction(
		{ C34Request(C34Big(), 10) }, { C34Request(C34Other(), 1), C34Request(C34Small(), 3) }).IsSuccess());
	TestTrue(TEXT("Legacy projection matches the committed stacks"),
		NativeReady->InventoryItems == C34DefinitionsOf(C34Capture(Native.Get())));
	const TArray<TSubclassOf<UItemDefinition>> ProjectionBefore = NativeReady->InventoryItems;
	{
		FC34UnchangedScope Unchanged(*this, Native, TEXT("Native-ready failure"));
		C34TestResult(*this, TEXT("Native-ready failure is rejected"),
			Native->TryApplyDefinitionTransaction({ C34Request(C34Single(), 2) }, {}).Result, EInventoryOperationResult::InsufficientQuantity);
	}
	TestTrue(TEXT("Failure leaves the legacy projection untouched"), NativeReady->InventoryItems == ProjectionBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC34ProductionRecipesTest,
	"NullTide.Crafting.C3C4.ProductionRecipes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC34ProductionRecipesTest::RunTest(const FString& Parameters)
{
	UClass* Wood = C34LoadItem(TEXT("Wood"));
	UClass* Stone = C34LoadItem(TEXT("Stone"));
	UClass* Axe = C34LoadItem(TEXT("Axe"));
	UClass* WoodArrow = C34LoadItem(TEXT("WoodArrow"));
	UClass* RecipeAxe = C34LoadRecipe(TEXT("Axe"));
	UClass* RecipeWoodArrow = C34LoadRecipe(TEXT("WoodArrow"));
	if (!TestTrue(TEXT("Production items and recipes load"), Wood && Stone && Axe && WoodArrow && RecipeAxe && RecipeWoodArrow))
	{
		return false;
	}

	// Recipe_Axe: Wood x3 + Stone x5 -> Axe x1.
	FC34Inventory AxeInventory;
	if (!C34Add(*this, AxeInventory.Get(), Wood, 10) || !C34Add(*this, AxeInventory.Get(), Stone, 5)) { return false; }
	const FGuid WoodId = C34Capture(AxeInventory.Get())[0].Id;
	AxeInventory.StartObserving();
	const FCraftingTransactionResult AxeCraft = FCraftingTransaction::TryCraftRecipeAtomic(AxeInventory.Get(), RecipeAxe);
	TestTrue(TEXT("Recipe_Axe crafts"), AxeCraft.IsSuccess());
	const TArray<FC34Stack> AxeAfter = C34Capture(AxeInventory.Get());
	TestTrue(TEXT("Wood 7 (same stack), Stone consumed, Axe 1"), C34HasLayout(AxeAfter, { { Wood, 7 }, { Axe, 1 } }));
	TestTrue(TEXT("Partially consumed Wood keeps its ItemId"), AxeAfter.Num() == 2 && AxeAfter[0].Id == WoodId);
	TestEqual(TEXT("Recipe_Axe emits one event"), AxeInventory.Listener->EventCount, 1);
	{
		FC34UnchangedScope Unchanged(*this, AxeInventory, TEXT("Recipe_Axe without Stone"));
		const FCraftingTransactionResult Again = FCraftingTransaction::TryCraftRecipeAtomic(AxeInventory.Get(), RecipeAxe);
		TestTrue(TEXT("Second Recipe_Axe lacks Stone"), Again.Result == ECraftingTransactionResult::InsufficientIngredients);
		TestEqual(TEXT("Stone is ingredient index 1"), Again.EntryIndex, 1);
	}

	// Recipe_WoodArrow: Wood x1 -> WoodArrow x5, merging into a 48 stack and overflowing.
	FC34Inventory ArrowInventory;
	if (!C34Add(*this, ArrowInventory.Get(), Wood, 2) || !C34Add(*this, ArrowInventory.Get(), WoodArrow, 48)) { return false; }
	const TArray<FC34Stack> ArrowBefore = C34Capture(ArrowInventory.Get());
	ArrowInventory.StartObserving();
	TestTrue(TEXT("Recipe_WoodArrow crafts"), FCraftingTransaction::TryCraftRecipeAtomic(ArrowInventory.Get(), RecipeWoodArrow).IsSuccess());
	const TArray<FC34Stack> ArrowAfter = C34Capture(ArrowInventory.Get());
	TestTrue(TEXT("Wood 1, WoodArrow 50, WoodArrow 3"), C34HasLayout(ArrowAfter, { { Wood, 1 }, { WoodArrow, 50 }, { WoodArrow, 3 } }));
	if (ArrowAfter.Num() == 3)
	{
		TestEqual(TEXT("Merge destination WoodArrow keeps its ItemId"), ArrowAfter[1].Id, ArrowBefore[1].Id);
		TestFalse(TEXT("Overflow WoodArrow stack is new"), C34ContainsId(ArrowBefore, ArrowAfter[2].Id));
	}

	// Full inventory: the last Wood frees the slot the overflow WoodArrow stack needs.
	FC34Inventory FullArrow;
	if (!C34Add(*this, FullArrow.Get(), Wood, 1) || !C34Add(*this, FullArrow.Get(), WoodArrow, 50)
		|| !C34Add(*this, FullArrow.Get(), Axe, 22)) { return false; }
	FullArrow.StartObserving();
	TestTrue(TEXT("Recipe_WoodArrow crafts in a full inventory using the freed Wood slot"),
		FCraftingTransaction::TryCraftRecipeAtomic(FullArrow.Get(), RecipeWoodArrow).IsSuccess());
	TestEqual(TEXT("Wood is gone"), FullArrow->GetTotalQuantity(Wood), 0);
	TestEqual(TEXT("WoodArrow 55 across stacks"), FullArrow->GetTotalQuantity(WoodArrow), 55);
	TestEqual(TEXT("Still 24 occupied slots"), FullArrow->GetItemCount(), 24);
	return true;
}

#endif
