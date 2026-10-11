#include "InventoryComponentTestTypes.h"

#include "Compatibility/LegacyInventoryCompatibilityLibrary.h"
#include "Inventory/InventoryComponent.h"
#include "Items/Fragments/ItemFragment_Stackable.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Storage/StorageComponent.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

UInventoryStackableTestItemDefinition::UInventoryStackableTestItemDefinition()
{
	UItemFragment_Stackable* Stackable = CreateDefaultSubobject<UItemFragment_Stackable>(TEXT("Stackable"));
	Stackable->MaxStackSize = 50;
	ItemFragments.Add(Stackable);
}

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
TStrongObjectPtr<UInventoryComponent> MakeStackingInventory()
{
	return TStrongObjectPtr<UInventoryComponent>(NewObject<UInventoryComponent>(GetTransientPackage()));
}

TStrongObjectPtr<UInventoryComponent> MakeNativeReadyStackingInventory()
{
	UInventoryCutoverTestManager* Inventory = NewObject<UInventoryCutoverTestManager>(GetTransientPackage());
	FBoolProperty* LegacyModeProperty = FindFProperty<FBoolProperty>(UInventoryComponent::StaticClass(), TEXT("bLegacyInventoryMode"));
	if (!LegacyModeProperty)
	{
		return nullptr;
	}
	LegacyModeProperty->SetPropertyValue_InContainer(Inventory, true);
	if (!Inventory->InitializeFromLegacyInventory().IsSuccess())
	{
		return nullptr;
	}
	return TStrongObjectPtr<UInventoryComponent>(Inventory);
}

TStrongObjectPtr<UStorageComponent> MakeStackingStorage()
{
	return TStrongObjectPtr<UStorageComponent>(NewObject<UStorageComponent>(GetTransientPackage()));
}

bool HasStackQuantities(const TArray<UItemInstance*>& Items, const TArray<int32>& Expected)
{
	if (Items.Num() != Expected.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		if (!IsValid(Items[Index]) || Items[Index]->GetQuantity() != Expected[Index])
		{
			return false;
		}
	}
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryStackingFragmentContractTest,
	"NullTide.Inventory.Stacking.FragmentContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryStackingFragmentContractTest::RunTest(const FString& Parameters)
{
	const UItemDefinition* Stackable = UInventoryStackableTestItemDefinition::StaticClass()->GetDefaultObject<UItemDefinition>();
	const UItemDefinition* NonStackable = UInventoryTestItemDefinition::StaticClass()->GetDefaultObject<UItemDefinition>();
	const UItemFragment_Stackable* Fragment = Stackable
		? Cast<UItemFragment_Stackable>(Stackable->FindFragmentByClass(UItemFragment_Stackable::StaticClass()))
		: nullptr;
	TestNotNull(TEXT("Stackable definition resolves the canonical fragment"), Fragment);
	TestEqual(TEXT("Stackable max size is static configuration"), Fragment ? Fragment->MaxStackSize : 0, 50);
	TestFalse(TEXT("Definition without the fragment is non-stackable"),
		NonStackable && NonStackable->HasFragmentByClass(UItemFragment_Stackable::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryStackingAddOrderTest,
	"NullTide.Inventory.Stacking.AddFillAndOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryStackingAddOrderTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory = MakeStackingInventory();
	const FInventoryOperationResult First = Inventory->TryAddDefinitionQuantity(UInventoryStackableTestItemDefinition::StaticClass(), 48);
	const FInventoryOperationResult Second = Inventory->TryAddDefinitionQuantity(UInventoryStackableTestItemDefinition::StaticClass(), 5);
	const TArray<UItemInstance*> AfterSecond = Inventory->GetItemsSnapshot();
	TestTrue(TEXT("Initial stack add succeeds"), First.IsSuccess());
	TestTrue(TEXT("Partial-stack add succeeds"), Second.IsSuccess());
	TestTrue(TEXT("Existing stack is filled before a new stack is appended"),
		HasStackQuantities(AfterSecond, { 50, 3 }));
	TestEqual(TEXT("Partial add returns the first affected stack"), Second.ItemId, First.ItemId);

	const FInventoryOperationResult Third = Inventory->TryAddDefinitionQuantity(UInventoryStackableTestItemDefinition::StaticClass(), 97);
	const TArray<UItemInstance*> AfterThird = Inventory->GetItemsSnapshot();
	TestTrue(TEXT("Multi-stack overflow add succeeds"), Third.IsSuccess());
	TestTrue(TEXT("Compatible stacks fill in inventory order"), HasStackQuantities(AfterThird, { 50, 50, 50 }));
	for (const UItemInstance* Item : AfterThird)
	{
		TestTrue(TEXT("No stack exceeds its static maximum"), Item && Item->GetQuantity() <= 50);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryStackingCapacityAtomicityTest,
	"NullTide.Inventory.Stacking.CapacityAndAtomicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryStackingCapacityAtomicityTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> PartialFullInventory = MakeStackingInventory();
	const FInventoryOperationResult Partial = PartialFullInventory->TryAddDefinitionQuantity(UInventoryStackableTestItemDefinition::StaticClass(), 49);
	if (!TestTrue(TEXT("Partial compatible stack setup succeeds"), Partial.IsSuccess()))
	{
		return false;
	}
	for (int32 Index = 0; Index < 23; ++Index)
	{
		if (!TestTrue(TEXT("Non-stackable capacity setup succeeds"),
			PartialFullInventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass()).IsSuccess()))
		{
			return false;
		}
	}
	TestEqual(TEXT("Capacity counts occupied stacks rather than units"), PartialFullInventory->GetItemCount(), 24);
	TestTrue(TEXT("A compatible partial stack accepts quantity at 24 occupied slots"),
		PartialFullInventory->TryAddDefinitionQuantity(UInventoryStackableTestItemDefinition::StaticClass(), 1).IsSuccess());
	TestEqual(TEXT("Compatible fill keeps the same occupied slot count"), PartialFullInventory->GetItemCount(), 24);
	TestEqual(TEXT("Compatible fill reaches the configured maximum"), PartialFullInventory->FindItemById(Partial.ItemId)->GetQuantity(), 50);

	const TStrongObjectPtr<UInventoryComponent> AtomicInventory = MakeStackingInventory();
	const FInventoryOperationResult AtomicSeed = AtomicInventory->TryAddDefinitionQuantity(UInventoryStackableTestItemDefinition::StaticClass(), 48);
	if (!TestTrue(TEXT("Atomic failure setup stack succeeds"), AtomicSeed.IsSuccess()))
	{
		return false;
	}
	for (int32 Index = 0; Index < 23; ++Index)
	{
		if (!TestTrue(TEXT("Atomic failure setup fills remaining slots"),
			AtomicInventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass()).IsSuccess()))
		{
			return false;
		}
	}
	const TStrongObjectPtr<UInventoryComponentTestListener> Listener(
		NewObject<UInventoryComponentTestListener>(GetTransientPackage()));
	AtomicInventory->OnInventoryChanged.AddDynamic(Listener.Get(), &UInventoryComponentTestListener::HandleInventoryChanged);
	const int32 RevisionBefore = AtomicInventory->GetRevision();
	const FInventoryOperationResult Rejected = AtomicInventory->TryAddDefinitionQuantity(UInventoryStackableTestItemDefinition::StaticClass(), 3);
	TestTrue(TEXT("Insufficient space rejects the entire requested quantity"), Rejected.Result == EInventoryOperationResult::CapacityFull);
	TestEqual(TEXT("Atomic rejection preserves the partial stack quantity"), AtomicInventory->FindItemById(AtomicSeed.ItemId)->GetQuantity(), 48);
	TestEqual(TEXT("Atomic rejection preserves occupied stack count"), AtomicInventory->GetItemCount(), 24);
	TestEqual(TEXT("Atomic rejection preserves revision"), AtomicInventory->GetRevision(), RevisionBefore);
	TestEqual(TEXT("Atomic rejection emits no change event"), Listener->EventCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryStackingExactRemovalTest,
	"NullTide.Inventory.Stacking.ExactQuantityRemoval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryStackingExactRemovalTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory = MakeStackingInventory();
	const FInventoryOperationResult Added = Inventory->TryAddDefinitionQuantity(UInventoryStackableTestItemDefinition::StaticClass(), 55);
	const TArray<UItemInstance*> InitialItems = Inventory->GetItemsSnapshot();
	if (!TestTrue(TEXT("Two-stack removal setup succeeds"), Added.IsSuccess())
		|| !TestTrue(TEXT("Overflow produced deterministic stacks"), HasStackQuantities(InitialItems, { 50, 5 })))
	{
		return false;
	}
	const FGuid FirstStackId = InitialItems[0]->GetInstanceId();
	const FGuid SecondStackId = InitialItems[1]->GetInstanceId();
	TestTrue(TEXT("Exact partial removal succeeds"), Inventory->TryRemoveItemQuantity(FirstStackId, 2).IsSuccess());
	TestEqual(TEXT("Exact partial removal decrements only its stack"), Inventory->FindItemById(FirstStackId)->GetQuantity(), 48);
	TestEqual(TEXT("Other stack remains unchanged"), Inventory->FindItemById(SecondStackId)->GetQuantity(), 5);
	TestTrue(TEXT("Exact final removal succeeds"), Inventory->TryRemoveItemQuantity(SecondStackId, 5).IsSuccess());
	TestFalse(TEXT("Zero quantity removes only that exact stack identity"), Inventory->ContainsItem(SecondStackId));
	TestTrue(TEXT("Remaining order is preserved"), Inventory->GetItemsSnapshot()[0]->GetInstanceId() == FirstStackId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryStackingStorageTransferTest,
	"NullTide.Inventory.Stacking.StorageMergeAndFailureAtomicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryStackingStorageTransferTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory = MakeStackingInventory();
	const TStrongObjectPtr<UStorageComponent> Storage = MakeStackingStorage();
	const FInventoryOperationResult DestinationSeed = Inventory->TryAddDefinitionQuantity(UInventoryStackableTestItemDefinition::StaticClass(), 48);
	if (!TestTrue(TEXT("Storage merge destination seed adds"), DestinationSeed.IsSuccess())
		|| !TestTrue(TEXT("Destination seed transfers to storage"),
			Inventory->TryTransferItemToStorage(Storage.Get(), DestinationSeed.ItemId).IsSuccess()))
	{
		return false;
	}
	const FInventoryOperationResult SourceSeed = Inventory->TryAddDefinitionQuantity(UInventoryStackableTestItemDefinition::StaticClass(), 5);
	const int32 InventoryRevisionBefore = Inventory->GetRevision();
	const int32 StorageRevisionBefore = Storage->GetRevision();
	const FInventoryStorageTransferResult Merged = Inventory->TryTransferItemToStorage(Storage.Get(), SourceSeed.ItemId);
	TestTrue(TEXT("Whole source stack transfers into compatible storage stack"), Merged.IsSuccess());
	TestFalse(TEXT("Fully merged source stack identity no longer occupies a source slot"), Inventory->ContainsItem(SourceSeed.ItemId));
	TestEqual(TEXT("Destination stack owns the merged quantity"), Storage->FindItemById(DestinationSeed.ItemId)->GetQuantity(), 50);
	TestEqual(TEXT("Storage merge increments source revision once"), Inventory->GetRevision(), InventoryRevisionBefore + 1);
	TestEqual(TEXT("Storage merge increments destination revision once"), Storage->GetRevision(), StorageRevisionBefore + 1);

	const FInventoryStorageTransferResult Reverse = Storage->TryTransferItemToInventory(Inventory.Get(), DestinationSeed.ItemId);
	TestTrue(TEXT("Reverse transfer succeeds with the destination stack identity"), Reverse.IsSuccess());
	TestTrue(TEXT("Reverse transfer preserves the unmerged stack identity"), Inventory->ContainsItem(DestinationSeed.ItemId));
	TestEqual(TEXT("Reverse transfer preserves the full stack quantity"), Inventory->FindItemById(DestinationSeed.ItemId)->GetQuantity(), 50);

	const TStrongObjectPtr<UInventoryComponent> FullInventory = MakeStackingInventory();
	const TStrongObjectPtr<UStorageComponent> FullStorage = MakeStackingStorage();
	for (int32 Index = 0; Index < 24; ++Index)
	{
		const FInventoryOperationResult Added = FullInventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
		if (!TestTrue(TEXT("Full storage setup add succeeds"), Added.IsSuccess())
			|| !TestTrue(TEXT("Full storage setup transfer succeeds"), FullInventory->TryTransferItemToStorage(FullStorage.Get(), Added.ItemId).IsSuccess()))
		{
			return false;
		}
	}
	const FInventoryOperationResult Candidate = FullInventory->TryAddDefinitionQuantity(UInventoryStackableTestItemDefinition::StaticClass(), 5);
	const int32 FullInventoryRevision = FullInventory->GetRevision();
	const int32 FullStorageRevision = FullStorage->GetRevision();
	const FInventoryStorageTransferResult Rejected = FullInventory->TryTransferItemToStorage(FullStorage.Get(), Candidate.ItemId);
	TestTrue(TEXT("No-fit whole-stack transfer is rejected atomically"), Rejected.Result == EInventoryStorageTransferResult::DestinationFull);
	TestTrue(TEXT("Rejected transfer keeps source stack and quantity"),
		FullInventory->FindItemById(Candidate.ItemId) && FullInventory->FindItemById(Candidate.ItemId)->GetQuantity() == 5);
	TestEqual(TEXT("Rejected transfer preserves source revision"), FullInventory->GetRevision(), FullInventoryRevision);
	TestEqual(TEXT("Rejected transfer preserves destination revision"), FullStorage->GetRevision(), FullStorageRevision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryStackingPickupCompatibilityTest,
	"NullTide.Inventory.Stacking.PickupCompatibilityAddsAtomically",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryStackingPickupCompatibilityTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory = MakeNativeReadyStackingInventory();
	if (!TestNotNull(TEXT("Native-ready compatibility inventory initializes"), Inventory.Get()))
	{
		return false;
	}
	const FInventoryOperationResult First = ULegacyInventoryCompatibilityLibrary::TryAddDefinitionQuantityToLegacyInventory(
		Inventory.Get(), UInventoryStackableTestItemDefinition::StaticClass(), 48);
	const FInventoryOperationResult Second = ULegacyInventoryCompatibilityLibrary::TryAddDefinitionQuantityToLegacyInventory(
		Inventory.Get(), UInventoryStackableTestItemDefinition::StaticClass(), 2);
	TestTrue(TEXT("Pickup adapter accepts a native stack quantity"), First.IsSuccess());
	TestTrue(TEXT("Pickup adapter fills the compatible native stack"), Second.IsSuccess());
	TestEqual(TEXT("Pickup adapter preserves one occupied stack"), Inventory->GetItemCount(), 1);
	UItemInstance* Stack = Inventory->FindItemById(First.ItemId);
	if (!TestNotNull(TEXT("Pickup adapter returns a resolvable affected stack"), Stack))
	{
		return false;
	}
	TestEqual(TEXT("Pickup adapter commits the requested quantity atomically"), Stack->GetQuantity(), 50);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryStackingWorldDropTest,
	"NullTide.Inventory.Stacking.WholeStackWorldDrop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryStackingWorldDropTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryWorldDropTestComponent> Inventory(
		NewObject<UInventoryWorldDropTestComponent>(GetTransientPackage()));
	const TStrongObjectPtr<APawn> Pawn(NewObject<APawn>(GetTransientPackage()));
	const FInventoryOperationResult Added = Inventory->TryAddDefinitionQuantity(UInventoryStackableTestItemDefinition::StaticClass(), 5);
	if (!TestTrue(TEXT("World-drop stack setup succeeds"), Added.IsSuccess()))
	{
		return false;
	}
	const FInventoryWorldDropResult Dropped = Inventory->TryDropItemToWorld(Pawn.Get(), Added.ItemId);
	TestTrue(TEXT("Whole stack drop succeeds"), Dropped.IsSuccess());
	TestEqual(TEXT("Drop result reports whole-stack quantity"), Dropped.Quantity, 5);
	TestEqual(TEXT("Pickup spawn receives whole-stack quantity"), Inventory->LastSpawnedQuantity, 5);
	TestFalse(TEXT("Whole dropped stack is removed by its exact identity"), Inventory->ContainsItem(Added.ItemId));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
