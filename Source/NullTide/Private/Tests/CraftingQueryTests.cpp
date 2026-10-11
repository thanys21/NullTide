#include "CraftingTestTypes.h"

#include "InventoryComponentTestTypes.h"
#include "Crafting/CraftingQueryLibrary.h"
#include "Crafting/CraftingRecipeDefinition.h"
#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using FC2Stacks = TArray<TPair<FGuid, int32>>;

TSubclassOf<UItemDefinition> C2Stackable() { return UInventoryStackableTestItemDefinition::StaticClass(); }
TSubclassOf<UItemDefinition> C2Single() { return UInventoryTestItemDefinition::StaticClass(); }
TSubclassOf<UItemDefinition> C2Other() { return UInventoryOtherTestItemDefinition::StaticClass(); }

/** Inventory plus a change listener, so every query can prove it changed nothing. */
struct FC2Inventory
{
	TStrongObjectPtr<UInventoryComponent> Inventory;
	TStrongObjectPtr<UInventoryComponentTestListener> Listener;

	FC2Inventory()
		: Inventory(NewObject<UInventoryComponent>(GetTransientPackage()))
		, Listener(NewObject<UInventoryComponentTestListener>(GetTransientPackage()))
	{
	}

	UInventoryComponent* operator->() const { return Inventory.Get(); }
	UInventoryComponent* Get() const { return Inventory.Get(); }

	/** Call after setup so only query-time events are counted. */
	void StartObserving()
	{
		Inventory->OnInventoryChanged.AddDynamic(Listener.Get(), &UInventoryComponentTestListener::HandleInventoryChanged);
	}
};

FC2Stacks C2CaptureStacks(const UInventoryComponent* Inventory)
{
	FC2Stacks Stacks;
	for (const UItemInstance* Item : Inventory->GetItemsSnapshot())
	{
		Stacks.Emplace(Item ? Item->GetInstanceId() : FGuid(), Item ? Item->GetQuantity() : 0);
	}
	return Stacks;
}

/** Records contents and revision before queries, then verifies nothing changed and no event fired. */
struct FC2UnchangedScope
{
	FAutomationTestBase& Test;
	const FC2Inventory& Inventory;
	const TCHAR* Label;
	FC2Stacks Stacks;
	int32 Revision;
	int32 Events;

	FC2UnchangedScope(FAutomationTestBase& InTest, const FC2Inventory& InInventory, const TCHAR* InLabel)
		: Test(InTest), Inventory(InInventory), Label(InLabel), Stacks(C2CaptureStacks(InInventory.Get()))
		, Revision(InInventory->GetRevision()), Events(InInventory.Listener->EventCount)
	{
	}

	~FC2UnchangedScope()
	{
		Test.TestTrue(FString::Printf(TEXT("%s: stacks, ids and quantities unchanged"), Label),
			C2CaptureStacks(Inventory.Get()) == Stacks);
		Test.TestEqual(FString::Printf(TEXT("%s: revision unchanged"), Label), Inventory->GetRevision(), Revision);
		Test.TestEqual(FString::Printf(TEXT("%s: no change event"), Label), Inventory.Listener->EventCount, Events);
	}
};

bool C2Add(FAutomationTestBase& Test, UInventoryComponent* Inventory, TSubclassOf<UItemDefinition> Definition, int32 Quantity)
{
	return Test.TestTrue(FString::Printf(TEXT("Setup adds %d of %s"), Quantity, *GetNameSafe(Definition.Get())),
		Inventory->TryAddDefinitionQuantity(Definition, Quantity).IsSuccess());
}

/** Builds Stackable stacks 20 / 8 / 3 (total 31) by trimming three overflow stacks. */
bool C2MakeDistributedStacks(FAutomationTestBase& Test, UInventoryComponent* Inventory)
{
	if (!C2Add(Test, Inventory, C2Stackable(), 131))
	{
		return false;
	}
	const TArray<UItemInstance*> Items = Inventory->GetItemsSnapshot();
	if (!Test.TestEqual(TEXT("Setup overflow creates 50/50/31"), Items.Num(), 3))
	{
		return false;
	}
	const int32 Trims[] = { 30, 42, 28 };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (!Test.TestTrue(TEXT("Setup trims one exact stack"),
			Inventory->TryRemoveItemQuantity(Items[Index]->GetInstanceId(), Trims[Index]).IsSuccess()))
		{
			return false;
		}
	}
	const FC2Stacks Stacks = C2CaptureStacks(Inventory);
	return Test.TestTrue(TEXT("Setup stacks are 20/8/3"),
		Stacks.Num() == 3 && Stacks[0].Value == 20 && Stacks[1].Value == 8 && Stacks[2].Value == 3);
}

FInventoryDefinitionQuantity C2Request(TSubclassOf<UItemDefinition> Definition, int32 Quantity)
{
	FInventoryDefinitionQuantity Request;
	Request.DefinitionClass = Definition;
	Request.Quantity = Quantity;
	return Request;
}

FCraftingItemQuantity C2Entry(TSubclassOf<UItemDefinition> Definition, int32 Quantity)
{
	FCraftingItemQuantity Entry;
	Entry.ItemDefinition = Definition;
	Entry.Quantity = Quantity;
	return Entry;
}

TStrongObjectPtr<UCraftingTestRecipeDefinition> C2MakeRecipe(const TArray<FCraftingItemQuantity>& Ingredients,
	const TArray<FCraftingItemQuantity>& Outputs)
{
	TStrongObjectPtr<UCraftingTestRecipeDefinition> Recipe(NewObject<UCraftingTestRecipeDefinition>(GetTransientPackage()));
	Recipe->Ingredients = Ingredients;
	Recipe->Outputs = Outputs;
	return Recipe;
}

bool C2IsResult(const FInventoryOperationResult& Result, EInventoryOperationResult Expected)
{
	return Result.Result == Expected;
}

bool C2MakeLegacyModeInventory(FAutomationTestBase& Test, FC2Inventory& Inventory)
{
	FBoolProperty* LegacyModeProperty = FindFProperty<FBoolProperty>(UInventoryComponent::StaticClass(), TEXT("bLegacyInventoryMode"));
	if (!Test.TestNotNull(TEXT("Legacy mode flag exists"), LegacyModeProperty))
	{
		return false;
	}
	LegacyModeProperty->SetPropertyValue_InContainer(Inventory.Get(), true);
	return Test.TestFalse(TEXT("Legacy mode deactivates native authority"), Inventory->IsNativeAuthorityActive());
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC2TotalQuantityTest,
	"NullTide.Crafting.C2.InventoryTotalQuantity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC2TotalQuantityTest::RunTest(const FString& Parameters)
{
	FC2Inventory Empty;
	Empty.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, Empty, TEXT("Empty inventory"));
		TestEqual(TEXT("Empty inventory totals zero"), Empty->GetTotalQuantity(C2Stackable()), 0);
	}

	FC2Inventory Single;
	if (!C2Add(*this, Single.Get(), C2Stackable(), 20)) { return false; }
	Single.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, Single, TEXT("One stack"));
		TestEqual(TEXT("One stack totals its quantity"), Single->GetTotalQuantity(C2Stackable()), 20);
	}

	FC2Inventory Distributed;
	if (!C2MakeDistributedStacks(*this, Distributed.Get())) { return false; }
	if (!C2Add(*this, Distributed.Get(), C2Single(), 3)) { return false; }
	Distributed.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, Distributed, TEXT("Multiple stacks"));
		TestEqual(TEXT("20 + 8 + 3 totals 31"), Distributed->GetTotalQuantity(C2Stackable()), 31);
		TestEqual(TEXT("Three non-stackable instances count 1 each"), Distributed->GetTotalQuantity(C2Single()), 3);
		TestEqual(TEXT("Absent definition totals zero"), Distributed->GetTotalQuantity(C2Other()), 0);
		TestEqual(TEXT("Null definition totals zero"), Distributed->GetTotalQuantity(nullptr), 0);
		TestEqual(TEXT("Abstract root totals zero; matching is by exact class"),
			Distributed->GetTotalQuantity(UItemDefinition::StaticClass()), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC2RemovePreflightTest,
	"NullTide.Crafting.C2.InventoryRemovePreflight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC2RemovePreflightTest::RunTest(const FString& Parameters)
{
	FC2Inventory Inventory;
	if (!C2MakeDistributedStacks(*this, Inventory.Get())) { return false; }
	if (!C2Add(*this, Inventory.Get(), C2Single(), 2)) { return false; }
	Inventory.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, Inventory, TEXT("Remove preflight"));
		const FInventoryOperationResult Exact = Inventory->CanRemoveDefinitionQuantity(C2Stackable(), 31);
		TestTrue(TEXT("Exact total amount succeeds"), C2IsResult(Exact, EInventoryOperationResult::Success));
		TestEqual(TEXT("Success reports the requested quantity"), Exact.Quantity, 31);
		TestFalse(TEXT("Query names no stack identity"), Exact.ItemId.IsValid());
		TestTrue(TEXT("Amount spanning three stacks succeeds"),
			C2IsResult(Inventory->CanRemoveDefinitionQuantity(C2Stackable(), 25), EInventoryOperationResult::Success));
		TestTrue(TEXT("One more than the total is insufficient"),
			C2IsResult(Inventory->CanRemoveDefinitionQuantity(C2Stackable(), 32), EInventoryOperationResult::InsufficientQuantity));
		TestTrue(TEXT("Non-stackable instances sum to their count"),
			C2IsResult(Inventory->CanRemoveDefinitionQuantity(C2Single(), 2), EInventoryOperationResult::Success));
		TestTrue(TEXT("Non-stackable request above count is insufficient"),
			C2IsResult(Inventory->CanRemoveDefinitionQuantity(C2Single(), 3), EInventoryOperationResult::InsufficientQuantity));
		TestTrue(TEXT("Absent definition is insufficient"),
			C2IsResult(Inventory->CanRemoveDefinitionQuantity(C2Other(), 1), EInventoryOperationResult::InsufficientQuantity));
		TestTrue(TEXT("Zero quantity is rejected"),
			C2IsResult(Inventory->CanRemoveDefinitionQuantity(C2Stackable(), 0), EInventoryOperationResult::InvalidQuantity));
		TestTrue(TEXT("Negative quantity is rejected"),
			C2IsResult(Inventory->CanRemoveDefinitionQuantity(C2Stackable(), -4), EInventoryOperationResult::InvalidQuantity));
		TestTrue(TEXT("Null definition is rejected"),
			C2IsResult(Inventory->CanRemoveDefinitionQuantity(nullptr, 1), EInventoryOperationResult::InvalidDefinition));
	}

	FC2Inventory Legacy;
	if (!C2MakeLegacyModeInventory(*this, Legacy)) { return false; }
	Legacy.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, Legacy, TEXT("Inactive authority"));
		TestTrue(TEXT("Inactive native authority reports NotInitialized"),
			C2IsResult(Legacy->CanRemoveDefinitionQuantity(C2Stackable(), 1), EInventoryOperationResult::NotInitialized));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC2AddPreflightTest,
	"NullTide.Crafting.C2.InventoryAddPreflight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC2AddPreflightTest::RunTest(const FString& Parameters)
{
	FC2Inventory Partial;
	if (!C2Add(*this, Partial.Get(), C2Stackable(), 48)) { return false; }
	Partial.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, Partial, TEXT("Partial stack"));
		TestTrue(TEXT("Quantity that fits the partial stack succeeds"),
			C2IsResult(Partial->CanAddDefinitionQuantity(C2Stackable(), 2), EInventoryOperationResult::Success));
		const FInventoryOperationResult Overflow = Partial->CanAddDefinitionQuantity(C2Stackable(), 5);
		TestTrue(TEXT("Overflow into a free slot succeeds"), C2IsResult(Overflow, EInventoryOperationResult::Success));
		TestEqual(TEXT("Success reports the requested quantity"), Overflow.Quantity, 5);
		TestTrue(TEXT("Zero quantity is rejected"),
			C2IsResult(Partial->CanAddDefinitionQuantity(C2Stackable(), 0), EInventoryOperationResult::InvalidQuantity));
		TestTrue(TEXT("Null definition is rejected"),
			C2IsResult(Partial->CanAddDefinitionQuantity(nullptr, 1), EInventoryOperationResult::InvalidDefinition));
		TestTrue(TEXT("Abstract definition is rejected"),
			C2IsResult(Partial->CanAddDefinitionQuantity(UItemDefinition::StaticClass(), 1), EInventoryOperationResult::InvalidDefinition));
		TestTrue(TEXT("Invalid definition data is rejected"),
			C2IsResult(Partial->CanAddDefinitionQuantity(UInventoryInvalidDataTestDefinition::StaticClass(), 1),
				EInventoryOperationResult::InvalidDefinitionData));
	}

	FC2Inventory FullWithPartial;
	if (!C2Add(*this, FullWithPartial.Get(), C2Stackable(), 48)) { return false; }
	if (!C2Add(*this, FullWithPartial.Get(), C2Single(), 23)) { return false; }
	FullWithPartial.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, FullWithPartial, TEXT("Full inventory with partial stack"));
		TestTrue(TEXT("Full inventory accepts what the compatible stack can hold"),
			C2IsResult(FullWithPartial->CanAddDefinitionQuantity(C2Stackable(), 2), EInventoryOperationResult::Success));
		TestTrue(TEXT("Full inventory rejects overflow beyond the compatible stack"),
			C2IsResult(FullWithPartial->CanAddDefinitionQuantity(C2Stackable(), 3), EInventoryOperationResult::CapacityFull));
		TestTrue(TEXT("Full inventory rejects a non-stackable item"),
			C2IsResult(FullWithPartial->CanAddDefinitionQuantity(C2Single(), 1), EInventoryOperationResult::CapacityFull));
	}

	FC2Inventory FullNoCompatible;
	if (!C2Add(*this, FullNoCompatible.Get(), C2Single(), 24)) { return false; }
	FullNoCompatible.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, FullNoCompatible, TEXT("Full inventory without compatible stack"));
		TestTrue(TEXT("No compatible capacity rejects a stackable item"),
			C2IsResult(FullNoCompatible->CanAddDefinitionQuantity(C2Stackable(), 1), EInventoryOperationResult::CapacityFull));
	}

	FC2Inventory NonStackable;
	if (!C2Add(*this, NonStackable.Get(), C2Single(), 22)) { return false; }
	NonStackable.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, NonStackable, TEXT("Non-stackable capacity"));
		TestTrue(TEXT("Each non-stackable unit needs its own slot: 2 fit"),
			C2IsResult(NonStackable->CanAddDefinitionQuantity(C2Other(), 2), EInventoryOperationResult::Success));
		TestTrue(TEXT("Each non-stackable unit needs its own slot: 3 do not"),
			C2IsResult(NonStackable->CanAddDefinitionQuantity(C2Other(), 3), EInventoryOperationResult::CapacityFull));

		TestTrue(TEXT("Batch: two requests needing the last two slots fit"),
			C2IsResult(NonStackable->CanAddDefinitionQuantities({ C2Request(C2Stackable(), 5), C2Request(C2Other(), 1) }),
				EInventoryOperationResult::Success));
		TestTrue(TEXT("Batch: duplicate definitions are summed (60 needs two slots)"),
			C2IsResult(NonStackable->CanAddDefinitionQuantities({ C2Request(C2Stackable(), 30), C2Request(C2Stackable(), 30) }),
				EInventoryOperationResult::Success));
		TestTrue(TEXT("Batch: empty request is rejected"),
			C2IsResult(NonStackable->CanAddDefinitionQuantities({}), EInventoryOperationResult::InvalidQuantity));
		TestTrue(TEXT("Batch: invalid entry quantity is rejected"),
			C2IsResult(NonStackable->CanAddDefinitionQuantities({ C2Request(C2Stackable(), 0) }), EInventoryOperationResult::InvalidQuantity));
		TestTrue(TEXT("Batch: invalid entry definition is rejected"),
			C2IsResult(NonStackable->CanAddDefinitionQuantities({ C2Request(nullptr, 1) }), EInventoryOperationResult::InvalidDefinition));
	}

	FC2Inventory OneSlotLeft;
	if (!C2Add(*this, OneSlotLeft.Get(), C2Single(), 23)) { return false; }
	OneSlotLeft.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, OneSlotLeft, TEXT("Batch competing for one slot"));
		TestTrue(TEXT("Alone, the stackable request fits"),
			C2IsResult(OneSlotLeft->CanAddDefinitionQuantity(C2Stackable(), 5), EInventoryOperationResult::Success));
		TestTrue(TEXT("Alone, the non-stackable request fits"),
			C2IsResult(OneSlotLeft->CanAddDefinitionQuantity(C2Other(), 1), EInventoryOperationResult::Success));
		TestTrue(TEXT("Together they need two slots and are rejected"),
			C2IsResult(OneSlotLeft->CanAddDefinitionQuantities({ C2Request(C2Stackable(), 5), C2Request(C2Other(), 1) }),
				EInventoryOperationResult::CapacityFull));
		TestTrue(TEXT("Summed duplicates needing two slots are rejected"),
			C2IsResult(OneSlotLeft->CanAddDefinitionQuantities({ C2Request(C2Stackable(), 30), C2Request(C2Stackable(), 30) }),
				EInventoryOperationResult::CapacityFull));
	}

	FC2Inventory Legacy;
	if (!C2MakeLegacyModeInventory(*this, Legacy)) { return false; }
	Legacy.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, Legacy, TEXT("Inactive authority"));
		TestTrue(TEXT("Inactive native authority reports NotInitialized"),
			C2IsResult(Legacy->CanAddDefinitionQuantity(C2Stackable(), 1), EInventoryOperationResult::NotInitialized));
		TestTrue(TEXT("Inactive native authority reports NotInitialized for batches"),
			C2IsResult(Legacy->CanAddDefinitionQuantities({ C2Request(C2Stackable(), 1) }), EInventoryOperationResult::NotInitialized));
	}

	// The query and the commit share one preflight, so their results must agree. Commits run last.
	const EInventoryOperationResult PredictedReject = FullWithPartial->CanAddDefinitionQuantity(C2Stackable(), 3).Result;
	TestTrue(TEXT("Commit agrees with a rejected query"),
		FullWithPartial->TryAddDefinitionQuantity(C2Stackable(), 3).Result == PredictedReject);
	const EInventoryOperationResult PredictedAccept = FullWithPartial->CanAddDefinitionQuantity(C2Stackable(), 2).Result;
	TestTrue(TEXT("Commit agrees with an accepted query"),
		FullWithPartial->TryAddDefinitionQuantity(C2Stackable(), 2).Result == PredictedAccept);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC2IngredientQueryTest,
	"NullTide.Crafting.C2.RecipeIngredientQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC2IngredientQueryTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UCraftingTestRecipeDefinition> Recipe = C2MakeRecipe(
		{ C2Entry(C2Stackable(), 25), C2Entry(C2Single(), 2) }, { C2Entry(C2Other(), 1) });
	FString Diagnostic;
	if (!TestTrue(TEXT("Query recipe is valid"), Recipe->ValidateRecipe(Diagnostic))) { return false; }

	FC2Inventory Available;
	if (!C2MakeDistributedStacks(*this, Available.Get())) { return false; }
	if (!C2Add(*this, Available.Get(), C2Single(), 2)) { return false; }
	Available.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, Available, TEXT("Ingredients available"));
		const FCraftingQueryResult Result = UCraftingQueryLibrary::CheckIngredients(Available.Get(), Recipe.Get());
		TestTrue(TEXT("Ingredients spread over 20/8/3 stacks are available"), Result.IsSuccess());
		TestEqual(TEXT("Success names no failing entry"), Result.EntryIndex, static_cast<int32>(INDEX_NONE));
	}

	FC2Inventory ShortSecond;
	if (!C2MakeDistributedStacks(*this, ShortSecond.Get())) { return false; }
	if (!C2Add(*this, ShortSecond.Get(), C2Single(), 1)) { return false; }
	ShortSecond.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, ShortSecond, TEXT("One ingredient insufficient"));
		const FCraftingQueryResult Result = UCraftingQueryLibrary::CheckIngredients(ShortSecond.Get(), Recipe.Get());
		TestTrue(TEXT("A short ingredient fails the recipe"), Result.Result == ECraftingQueryResult::InsufficientIngredients);
		TestEqual(TEXT("The failing ingredient index is reported"), Result.EntryIndex, 1);
		TestTrue(TEXT("The inventory reason is reported"), Result.InventoryResult == EInventoryOperationResult::InsufficientQuantity);
	}

	const TStrongObjectPtr<UCraftingTestRecipeDefinition> NeedsMore = C2MakeRecipe(
		{ C2Entry(C2Stackable(), 32), C2Entry(C2Single(), 2) }, { C2Entry(C2Other(), 1) });
	{
		FC2UnchangedScope Unchanged(*this, Available, TEXT("Distributed total short by one"));
		const FCraftingQueryResult Result = UCraftingQueryLibrary::CheckIngredients(Available.Get(), NeedsMore.Get());
		TestTrue(TEXT("31 across stacks does not cover 32"), Result.Result == ECraftingQueryResult::InsufficientIngredients);
		TestEqual(TEXT("First ingredient is reported"), Result.EntryIndex, 0);
	}

	{
		FC2UnchangedScope Unchanged(*this, Available, TEXT("Invalid query inputs"));
		TestTrue(TEXT("Null inventory is rejected"),
			UCraftingQueryLibrary::CheckIngredients(nullptr, Recipe.Get()).Result == ECraftingQueryResult::InvalidInventory);
		TestTrue(TEXT("Null recipe class is rejected"),
			UCraftingQueryLibrary::CheckRecipeIngredients(Available.Get(), nullptr).Result == ECraftingQueryResult::InvalidRecipe);
		TestTrue(TEXT("Abstract recipe class is rejected"),
			UCraftingQueryLibrary::CheckRecipeIngredients(Available.Get(), UCraftingRecipeDefinition::StaticClass()).Result
				== ECraftingQueryResult::InvalidRecipe);
		const TStrongObjectPtr<UCraftingTestRecipeDefinition> Invalid = C2MakeRecipe({ C2Entry(C2Stackable(), 1) }, {});
		TestTrue(TEXT("A recipe failing validation is rejected"),
			UCraftingQueryLibrary::CheckIngredients(Available.Get(), Invalid.Get()).Result == ECraftingQueryResult::InvalidRecipe);
		// The fixture CDO needs Stackable x3 and Single x2.
		TestTrue(TEXT("The class form reads the recipe default object"),
			UCraftingQueryLibrary::CheckRecipeIngredients(Available.Get(), UCraftingTestRecipeDefinition::StaticClass()).IsSuccess());
	}

	FC2Inventory Legacy;
	if (!C2MakeLegacyModeInventory(*this, Legacy)) { return false; }
	TestTrue(TEXT("Inactive native authority is an invalid inventory"),
		UCraftingQueryLibrary::CheckIngredients(Legacy.Get(), Recipe.Get()).Result == ECraftingQueryResult::InvalidInventory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC2OutputQueryTest,
	"NullTide.Crafting.C2.RecipeOutputQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC2OutputQueryTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UCraftingTestRecipeDefinition> StackableOutput = C2MakeRecipe(
		{ C2Entry(C2Single(), 1) }, { C2Entry(C2Stackable(), 5) });
	const TStrongObjectPtr<UCraftingTestRecipeDefinition> SingleOutput = C2MakeRecipe(
		{ C2Entry(C2Stackable(), 1) }, { C2Entry(C2Other(), 1) });
	const TStrongObjectPtr<UCraftingTestRecipeDefinition> TwoOutputs = C2MakeRecipe(
		{ C2Entry(C2Single(), 1) }, { C2Entry(C2Stackable(), 5), C2Entry(C2Other(), 1) });

	FC2Inventory MergeFits;
	if (!C2Add(*this, MergeFits.Get(), C2Stackable(), 40)) { return false; }
	if (!C2Add(*this, MergeFits.Get(), C2Single(), 23)) { return false; }
	MergeFits.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, MergeFits, TEXT("Output merges into a partial stack"));
		TestTrue(TEXT("Full inventory still fits a stackable output by merge"),
			UCraftingQueryLibrary::CheckOutputCapacity(MergeFits.Get(), StackableOutput.Get()).IsSuccess());
		TestTrue(TEXT("Full inventory has no slot for a non-stackable output"),
			UCraftingQueryLibrary::CheckOutputCapacity(MergeFits.Get(), SingleOutput.Get()).Result
				== ECraftingQueryResult::OutputCapacityFull);
	}

	FC2Inventory NeedsSlot;
	if (!C2Add(*this, NeedsSlot.Get(), C2Stackable(), 48)) { return false; }
	if (!C2Add(*this, NeedsSlot.Get(), C2Single(), 22)) { return false; }
	NeedsSlot.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, NeedsSlot, TEXT("Output needs a new slot"));
		TestTrue(TEXT("Overflow beyond the partial stack uses the free slot"),
			UCraftingQueryLibrary::CheckOutputCapacity(NeedsSlot.Get(), StackableOutput.Get()).IsSuccess());
		TestTrue(TEXT("A non-stackable output uses the free slot"),
			UCraftingQueryLibrary::CheckOutputCapacity(NeedsSlot.Get(), SingleOutput.Get()).IsSuccess());
	}

	FC2Inventory NoSlot;
	if (!C2Add(*this, NoSlot.Get(), C2Stackable(), 48)) { return false; }
	if (!C2Add(*this, NoSlot.Get(), C2Single(), 23)) { return false; }
	NoSlot.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, NoSlot, TEXT("Output overflow without a slot"));
		const FCraftingQueryResult Result = UCraftingQueryLibrary::CheckOutputCapacity(NoSlot.Get(), StackableOutput.Get());
		TestTrue(TEXT("Overflow with no free slot is rejected"), Result.Result == ECraftingQueryResult::OutputCapacityFull);
		TestTrue(TEXT("The inventory reason is reported"), Result.InventoryResult == EInventoryOperationResult::CapacityFull);
		TestEqual(TEXT("Output failures are not tied to one ingredient"), Result.EntryIndex, static_cast<int32>(INDEX_NONE));
	}

	FC2Inventory TwoFree;
	if (!C2Add(*this, TwoFree.Get(), C2Single(), 22)) { return false; }
	FC2Inventory OneFree;
	if (!C2Add(*this, OneFree.Get(), C2Single(), 23)) { return false; }
	TwoFree.StartObserving();
	OneFree.StartObserving();
	{
		FC2UnchangedScope UnchangedTwo(*this, TwoFree, TEXT("Two outputs, two free slots"));
		FC2UnchangedScope UnchangedOne(*this, OneFree, TEXT("Two outputs, one free slot"));
		TestTrue(TEXT("Two outputs fit two free slots"),
			UCraftingQueryLibrary::CheckOutputCapacity(TwoFree.Get(), TwoOutputs.Get()).IsSuccess());
		TestTrue(TEXT("Each output alone would fit the last slot"),
			UCraftingQueryLibrary::CheckOutputCapacity(OneFree.Get(), StackableOutput.Get()).IsSuccess()
			&& UCraftingQueryLibrary::CheckOutputCapacity(OneFree.Get(), SingleOutput.Get()).IsSuccess());
		TestTrue(TEXT("Outputs are checked together, not one at a time"),
			UCraftingQueryLibrary::CheckOutputCapacity(OneFree.Get(), TwoOutputs.Get()).Result
				== ECraftingQueryResult::OutputCapacityFull);
	}

	// C2 overlap contract: outputs are checked against the inventory before consumption. This is conservative.
	const TStrongObjectPtr<UCraftingTestRecipeDefinition> Overlap = C2MakeRecipe(
		{ C2Entry(C2Stackable(), 10) }, { C2Entry(C2Stackable(), 5) });
	FString Diagnostic;
	if (!TestTrue(TEXT("Overlapping ingredient/output recipe is valid"), Overlap->ValidateRecipe(Diagnostic))) { return false; }
	FC2Inventory OverlapFull;
	if (!C2Add(*this, OverlapFull.Get(), C2Stackable(), 50)) { return false; }
	if (!C2Add(*this, OverlapFull.Get(), C2Single(), 23)) { return false; }
	OverlapFull.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, OverlapFull, TEXT("Ingredient/output overlap"));
		TestTrue(TEXT("Overlap: the ingredient is available"),
			UCraftingQueryLibrary::CheckIngredients(OverlapFull.Get(), Overlap.Get()).IsSuccess());
		TestTrue(TEXT("Overlap: outputs are checked before consumption, so a full 50/50 stack rejects +5 "
			"even though consuming 10 first would make room (exact check deferred to C3/C4)"),
			UCraftingQueryLibrary::CheckOutputCapacity(OverlapFull.Get(), Overlap.Get()).Result
				== ECraftingQueryResult::OutputCapacityFull);
	}

	const TStrongObjectPtr<UCraftingTestRecipeDefinition> FreesSlot = C2MakeRecipe(
		{ C2Entry(C2Single(), 1) }, { C2Entry(C2Other(), 1) });
	FC2Inventory SlotFull;
	if (!C2Add(*this, SlotFull.Get(), C2Single(), 24)) { return false; }
	SlotFull.StartObserving();
	{
		FC2UnchangedScope Unchanged(*this, SlotFull, TEXT("Consumption would free a slot"));
		TestTrue(TEXT("Pre-consumption check rejects although consuming one item would free a slot"),
			UCraftingQueryLibrary::CheckOutputCapacity(SlotFull.Get(), FreesSlot.Get()).Result
				== ECraftingQueryResult::OutputCapacityFull);
	}

	TestTrue(TEXT("Null inventory is rejected"),
		UCraftingQueryLibrary::CheckOutputCapacity(nullptr, StackableOutput.Get()).Result == ECraftingQueryResult::InvalidInventory);
	TestTrue(TEXT("Null recipe class is rejected"),
		UCraftingQueryLibrary::CheckRecipeOutputCapacity(TwoFree.Get(), nullptr).Result == ECraftingQueryResult::InvalidRecipe);
	FC2Inventory Legacy;
	if (!C2MakeLegacyModeInventory(*this, Legacy)) { return false; }
	TestTrue(TEXT("Inactive native authority is an invalid inventory"),
		UCraftingQueryLibrary::CheckOutputCapacity(Legacy.Get(), StackableOutput.Get()).Result == ECraftingQueryResult::InvalidInventory);
	return true;
}

#endif
