// Fill out your copyright notice in the Description page of Project Settings.

#include "InventoryComponentTestTypes.h"

#include "Inventory/InventoryComponent.h"
#include "Items/ItemInstance.h"
#include "Misc/AutomationTest.h"
#include "Storage/StorageComponent.h"
#include "UObject/StrongObjectPtr.h"

void UStorageComponentTestListener::HandleStorageChanged(int32 NewRevision)
{
	++EventCount;
	LastRevision = NewRevision;
	if (Storage)
	{
		ObservedItemCount = Storage->GetItemCount();
	}
}

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
TStrongObjectPtr<UInventoryComponent> MakeInventory()
{
	return TStrongObjectPtr<UInventoryComponent>(NewObject<UInventoryComponent>(GetTransientPackage()));
}

TStrongObjectPtr<UStorageComponent> MakeStorage()
{
	return TStrongObjectPtr<UStorageComponent>(NewObject<UStorageComponent>(GetTransientPackage()));
}

int32 CountMatchingId(const TArray<UItemInstance*>& Items, const FGuid ItemId)
{
	int32 Count = 0;
	for (const UItemInstance* Item : Items)
	{
		if (IsValid(Item) && Item->GetInstanceId() == ItemId)
		{
			++Count;
		}
	}
	return Count;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStorageInventoryToStorageTransferTest,
	"NullTide.Inventory.G7.Storage.InventoryToStoragePreservesExactIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStorageInventoryToStorageTransferTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory = MakeInventory();
	const TStrongObjectPtr<UStorageComponent> Storage = MakeStorage();
	const FInventoryOperationResult First = Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
	const FInventoryOperationResult Second = Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
	const FInventoryOperationResult Third = Inventory->TryAddDefinition(UInventoryOtherTestItemDefinition::StaticClass());
	if (!TestTrue(TEXT("Duplicate-definition seed items add"), First.IsSuccess() && Second.IsSuccess() && Third.IsSuccess()))
	{
		return false;
	}

	const TStrongObjectPtr<UInventoryComponentTestListener> InventoryListener(
		NewObject<UInventoryComponentTestListener>(GetTransientPackage()));
	const TStrongObjectPtr<UStorageComponentTestListener> StorageListener(
		NewObject<UStorageComponentTestListener>(GetTransientPackage()));
	InventoryListener->Inventory = Inventory.Get();
	StorageListener->Storage = Storage.Get();
	Inventory->OnInventoryChanged.AddDynamic(InventoryListener.Get(), &UInventoryComponentTestListener::HandleInventoryChanged);
	Storage->OnStorageChanged.AddDynamic(StorageListener.Get(), &UStorageComponentTestListener::HandleStorageChanged);
	const int32 InventoryRevisionBefore = Inventory->GetRevision();
	const int32 StorageRevisionBefore = Storage->GetRevision();

	const FInventoryStorageTransferResult Result = Inventory->TryTransferItemToStorage(Storage.Get(), Second.ItemId);
	const TArray<UItemInstance*> InventoryItems = Inventory->GetItemsSnapshot();
	const TArray<UItemInstance*> StorageItems = Storage->GetItemsSnapshot();

	TestTrue(TEXT("Inventory to storage succeeds"), Result.IsSuccess());
	TestEqual(TEXT("The returned ItemId is exact"), Result.ItemId, Second.ItemId);
	TestEqual(TEXT("The exact runtime item is returned"), Result.Item.Get(), Second.Item.Get());
	TestEqual(TEXT("The runtime item outer moves to storage"), Second.Item->GetOuter(), static_cast<UObject*>(Storage.Get()));
	TestFalse(TEXT("Source no longer contains transferred ID"), Inventory->ContainsItem(Second.ItemId));
	TestTrue(TEXT("Destination contains transferred ID"), Storage->ContainsItem(Second.ItemId));
	TestEqual(TEXT("Destination owns the ID exactly once"), CountMatchingId(StorageItems, Second.ItemId), 1);
	TestEqual(TEXT("Duplicate definitions retain distinct identity"), First.ItemId != Second.ItemId, true);
	TestEqual(TEXT("Source removal preserves remaining order"), InventoryItems[0], First.Item.Get());
	TestEqual(TEXT("Source removal preserves trailing order"), InventoryItems[1], Third.Item.Get());
	TestEqual(TEXT("Destination appends deterministically"), StorageItems[0], Second.Item.Get());
	TestEqual(TEXT("Total membership is conserved"), InventoryItems.Num() + StorageItems.Num(), 3);
	TestEqual(TEXT("Inventory revision increments once"), Inventory->GetRevision(), InventoryRevisionBefore + 1);
	TestEqual(TEXT("Storage revision increments once"), Storage->GetRevision(), StorageRevisionBefore + 1);
	TestEqual(TEXT("Inventory emits one committed event"), InventoryListener->EventCount, 1);
	TestEqual(TEXT("Storage emits one committed event"), StorageListener->EventCount, 1);
	TestEqual(TEXT("Inventory event observes committed source count"), InventoryListener->ObservedItemCount, 2);
	TestEqual(TEXT("Storage event observes committed destination count"), StorageListener->ObservedItemCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStorageStorageToInventoryTransferTest,
	"NullTide.Inventory.G7.Storage.StorageToInventoryAndCapacityBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStorageStorageToInventoryTransferTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory = MakeInventory();
	const TStrongObjectPtr<UStorageComponent> Storage = MakeStorage();
	const FInventoryOperationResult First = Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
	const FInventoryOperationResult Second = Inventory->TryAddDefinition(UInventoryOtherTestItemDefinition::StaticClass());
	if (!TestTrue(TEXT("Reverse-transfer seed items add"), First.IsSuccess() && Second.IsSuccess()))
	{
		return false;
	}
	if (!TestTrue(TEXT("Seed transfer into storage succeeds"),
		Inventory->TryTransferItemToStorage(Storage.Get(), First.ItemId).IsSuccess()))
	{
		return false;
	}

	const TStrongObjectPtr<UInventoryComponentTestListener> InventoryListener(
		NewObject<UInventoryComponentTestListener>(GetTransientPackage()));
	const TStrongObjectPtr<UStorageComponentTestListener> StorageListener(
		NewObject<UStorageComponentTestListener>(GetTransientPackage()));
	InventoryListener->Inventory = Inventory.Get();
	StorageListener->Storage = Storage.Get();
	Inventory->OnInventoryChanged.AddDynamic(InventoryListener.Get(), &UInventoryComponentTestListener::HandleInventoryChanged);
	Storage->OnStorageChanged.AddDynamic(StorageListener.Get(), &UStorageComponentTestListener::HandleStorageChanged);
	const int32 InventoryRevisionBefore = Inventory->GetRevision();
	const int32 StorageRevisionBefore = Storage->GetRevision();
	const FInventoryStorageTransferResult Reverse = Storage->TryTransferItemToInventory(Inventory.Get(), First.ItemId);

	TestTrue(TEXT("Storage to inventory succeeds"), Reverse.IsSuccess());
	TestEqual(TEXT("Reverse transfer preserves exact ItemId"), Reverse.ItemId, First.ItemId);
	TestEqual(TEXT("Reverse transfer preserves the same runtime object"), Reverse.Item.Get(), First.Item.Get());
	TestEqual(TEXT("The runtime item outer moves back to inventory"), First.Item->GetOuter(), static_cast<UObject*>(Inventory.Get()));
	TestEqual(TEXT("Reverse source loses the exact ID"), Storage->ContainsItem(First.ItemId), false);
	TestEqual(TEXT("Reverse destination gains the exact ID"), Inventory->ContainsItem(First.ItemId), true);
	const TArray<UItemInstance*> ReverseInventoryItems = Inventory->GetItemsSnapshot();
	TestEqual(TEXT("Reverse destination appends after existing items"), ReverseInventoryItems[0], Second.Item.Get());
	TestEqual(TEXT("Reverse destination retains transferred item last"), ReverseInventoryItems[1], First.Item.Get());
	TestEqual(TEXT("Reverse source revision increments once"), Storage->GetRevision(), StorageRevisionBefore + 1);
	TestEqual(TEXT("Reverse destination revision increments once"), Inventory->GetRevision(), InventoryRevisionBefore + 1);
	TestEqual(TEXT("Reverse source emits once"), StorageListener->EventCount, 1);
	TestEqual(TEXT("Reverse destination emits once"), InventoryListener->EventCount, 1);

	const TStrongObjectPtr<UInventoryComponent> BoundaryInventory = MakeInventory();
	const TStrongObjectPtr<UStorageComponent> BoundaryStorage = MakeStorage();
	TArray<FGuid> SeedIds;
	for (int32 Index = 0; Index < 23; ++Index)
	{
		const FInventoryOperationResult Added = BoundaryInventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
		if (!TestTrue(TEXT("Capacity seed add succeeds"), Added.IsSuccess())
			|| !TestTrue(TEXT("Capacity seed transfer succeeds"),
				BoundaryInventory->TryTransferItemToStorage(BoundaryStorage.Get(), Added.ItemId).IsSuccess()))
		{
			return false;
		}
	}
	const FInventoryOperationResult TwentyFourth = BoundaryInventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
	TestTrue(TEXT("The 24th destination slot can be seeded"), TwentyFourth.IsSuccess());
	TestTrue(TEXT("Capacity transition 23 to 24 succeeds"),
		BoundaryInventory->TryTransferItemToStorage(BoundaryStorage.Get(), TwentyFourth.ItemId).IsSuccess());
	TestEqual(TEXT("Storage reaches its fixed 24 slot capacity"), BoundaryStorage->GetItemCount(), 24);
	const FInventoryOperationResult TwentyFifth = BoundaryInventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
	const int32 InventoryRevisionAtCapacity = BoundaryInventory->GetRevision();
	const int32 StorageRevisionAtCapacity = BoundaryStorage->GetRevision();
	const FInventoryStorageTransferResult Overflow = BoundaryInventory->TryTransferItemToStorage(BoundaryStorage.Get(), TwentyFifth.ItemId);
	TestTrue(TEXT("Capacity transition 24 to 25 is rejected"), Overflow.Result == EInventoryStorageTransferResult::DestinationFull);
	TestTrue(TEXT("Overflow source keeps its exact item"), BoundaryInventory->ContainsItem(TwentyFifth.ItemId));
	TestEqual(TEXT("Overflow destination remains at 24"), BoundaryStorage->GetItemCount(), 24);
	TestEqual(TEXT("Overflow leaves source revision unchanged"), BoundaryInventory->GetRevision(), InventoryRevisionAtCapacity);
	TestEqual(TEXT("Overflow leaves destination revision unchanged"), BoundaryStorage->GetRevision(), StorageRevisionAtCapacity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStorageTransferFailureAtomicityTest,
	"NullTide.Inventory.G7.Storage.FailuresAreAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStorageTransferFailureAtomicityTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory = MakeInventory();
	const TStrongObjectPtr<UStorageComponent> Storage = MakeStorage();
	const FInventoryOperationResult Candidate = Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
	if (!TestTrue(TEXT("Failure-path candidate adds"), Candidate.IsSuccess()))
	{
		return false;
	}

	const TStrongObjectPtr<UInventoryComponentTestListener> InventoryListener(
		NewObject<UInventoryComponentTestListener>(GetTransientPackage()));
	const TStrongObjectPtr<UStorageComponentTestListener> StorageListener(
		NewObject<UStorageComponentTestListener>(GetTransientPackage()));
	InventoryListener->Inventory = Inventory.Get();
	StorageListener->Storage = Storage.Get();
	Inventory->OnInventoryChanged.AddDynamic(InventoryListener.Get(), &UInventoryComponentTestListener::HandleInventoryChanged);
	Storage->OnStorageChanged.AddDynamic(StorageListener.Get(), &UStorageComponentTestListener::HandleStorageChanged);
	const int32 InitialInventoryRevision = Inventory->GetRevision();
	const int32 InitialStorageRevision = Storage->GetRevision();

	TestTrue(TEXT("Stale source ItemId is rejected"),
		Inventory->TryTransferItemToStorage(Storage.Get(), FGuid::NewGuid()).Result == EInventoryStorageTransferResult::InvalidItemId);
	TestTrue(TEXT("Null destination is rejected"),
		Inventory->TryTransferItemToStorage(nullptr, Candidate.ItemId).Result == EInventoryStorageTransferResult::InvalidDestination);
	TestTrue(TEXT("Stale and null failures preserve source membership"), Inventory->ContainsItem(Candidate.ItemId));
	TestEqual(TEXT("Stale and null failures preserve destination membership"), Storage->GetItemCount(), 0);
	TestEqual(TEXT("Stale and null failures preserve source revision"), Inventory->GetRevision(), InitialInventoryRevision);
	TestEqual(TEXT("Stale and null failures preserve destination revision"), Storage->GetRevision(), InitialStorageRevision);
	TestEqual(TEXT("Stale and null failures emit no source event"), InventoryListener->EventCount, 0);
	TestEqual(TEXT("Stale and null failures emit no destination event"), StorageListener->EventCount, 0);

	for (int32 Index = 0; Index < 24; ++Index)
	{
		const FInventoryOperationResult Added = Inventory->TryAddDefinition(UInventoryOtherTestItemDefinition::StaticClass());
		if (!TestTrue(TEXT("Full-storage setup add succeeds"), Added.IsSuccess())
			|| !TestTrue(TEXT("Full-storage setup transfer succeeds"), Inventory->TryTransferItemToStorage(Storage.Get(), Added.ItemId).IsSuccess()))
		{
			return false;
		}
	}
	InventoryListener->EventCount = 0;
	StorageListener->EventCount = 0;
	const int32 FullStorageInventoryRevision = Inventory->GetRevision();
	const int32 FullStorageRevision = Storage->GetRevision();
	const FInventoryStorageTransferResult FullStorage = Inventory->TryTransferItemToStorage(Storage.Get(), Candidate.ItemId);
	TestTrue(TEXT("Full storage rejects a valid source item"), FullStorage.Result == EInventoryStorageTransferResult::DestinationFull);
	TestTrue(TEXT("Full storage failure retains the exact source item"), Inventory->ContainsItem(Candidate.ItemId));
	TestEqual(TEXT("Full storage failure leaves destination unchanged"), Storage->GetItemCount(), 24);
	TestEqual(TEXT("Full storage failure leaves source revision unchanged"), Inventory->GetRevision(), FullStorageInventoryRevision);
	TestEqual(TEXT("Full storage failure leaves destination revision unchanged"), Storage->GetRevision(), FullStorageRevision);
	TestEqual(TEXT("Full storage failure emits no source event"), InventoryListener->EventCount, 0);
	TestEqual(TEXT("Full storage failure emits no destination event"), StorageListener->EventCount, 0);

	const TStrongObjectPtr<UInventoryComponent> FullInventory = MakeInventory();
	const TStrongObjectPtr<UStorageComponent> ReverseStorage = MakeStorage();
	const FInventoryOperationResult ReverseCandidate = FullInventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
	if (!TestTrue(TEXT("Full-inventory source candidate adds"), ReverseCandidate.IsSuccess())
		|| !TestTrue(TEXT("Full-inventory source candidate transfers to storage"),
			FullInventory->TryTransferItemToStorage(ReverseStorage.Get(), ReverseCandidate.ItemId).IsSuccess()))
	{
		return false;
	}
	for (int32 Index = 0; Index < 24; ++Index)
	{
		if (!TestTrue(TEXT("Full-inventory setup add succeeds"),
			FullInventory->TryAddDefinition(UInventoryOtherTestItemDefinition::StaticClass()).IsSuccess()))
		{
			return false;
		}
	}
	const int32 FullInventoryRevision = FullInventory->GetRevision();
	const int32 ReverseStorageRevision = ReverseStorage->GetRevision();
	const FInventoryStorageTransferResult FullInventoryResult = ReverseStorage->TryTransferItemToInventory(FullInventory.Get(), ReverseCandidate.ItemId);
	TestTrue(TEXT("Full inventory rejects reverse transfer"), FullInventoryResult.Result == EInventoryStorageTransferResult::DestinationFull);
	TestTrue(TEXT("Full inventory failure retains storage ownership"), ReverseStorage->ContainsItem(ReverseCandidate.ItemId));
	TestFalse(TEXT("Full inventory failure does not duplicate item"), FullInventory->ContainsItem(ReverseCandidate.ItemId));
	TestEqual(TEXT("Full inventory failure preserves source revision"), ReverseStorage->GetRevision(), ReverseStorageRevision);
	TestEqual(TEXT("Full inventory failure preserves destination revision"), FullInventory->GetRevision(), FullInventoryRevision);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
