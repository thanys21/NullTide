// Fill out your copyright notice in the Description page of Project Settings.

#include "InventoryComponentTestTypes.h"

#include "Inventory/InventoryComponent.h"
#include "Compatibility/LegacyInventoryCompatibilityLibrary.h"
#include "GameFramework/Actor.h"
#include "Items/ItemInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

void UInventoryComponentTestListener::HandleInventoryChanged(int32 NewRevision)
{
	++EventCount;
	LastRevision = NewRevision;
	if (Inventory)
	{
		ObservedItemCount = Inventory->GetItemCount();
		EInventoryOperationResult QueryResult;
		ObservedDefinitions = ULegacyInventoryCompatibilityLibrary::GetLegacyItemsSnapshot(Inventory, QueryResult);
		if (bAttemptReentrantInitialization)
		{
			ReentrantInitializationResult = Inventory->InitializeFromLegacyInventory();
		}
	}

	if (bAttemptReentrantMutation && Inventory)
	{
		ReentrantResult = Inventory->TryAddDefinition(ReentrantDefinitionClass);
	}
}

bool UInventoryWorldDropTestComponent::ResolveWorldDropTransform(
	const APawn* OwningPawn,
	const TSubclassOf<UItemDefinition> DefinitionClass,
	FTransform& OutTransform) const
{
	OutTransform = FTransform::Identity;
	return bResolveTransformSucceeds;
}

AActor* UInventoryWorldDropTestComponent::SpawnAndConfigureWorldDrop(
	APawn* OwningPawn,
	const TSubclassOf<UItemDefinition> DefinitionClass,
	const FTransform& DropTransform)
{
	++SpawnAttempts;
	if (!bSpawnSucceeds || !DefinitionClass)
	{
		return nullptr;
	}
	LastSpawnedPickup = NewObject<AActor>(GetTransientPackage());
	return LastSpawnedPickup;
}

void UInventoryWorldDropTestComponent::RollbackWorldDrop(AActor* SpawnedPickup)
{
	++RollbackCount;
	LastRolledBackPickup = SpawnedPickup;
}

FInventoryOperationResult UInventoryWorldDropTestComponent::CommitWorldDropRemoval(const FGuid ItemId)
{
	++CommitAttempts;
	if (bCommitSucceeds)
	{
		return Super::CommitWorldDropRemoval(ItemId);
	}

	FInventoryOperationResult Failure;
	Failure.Result = EInventoryOperationResult::Busy;
	Failure.ItemId = ItemId;
	return Failure;
}

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
TStrongObjectPtr<UInventoryComponent> MakeInventory()
{
	return TStrongObjectPtr<UInventoryComponent>(
		NewObject<UInventoryComponent>(GetTransientPackage()));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryValidAddTest,
	"NullTide.Inventory.M1.ValidAdd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryValidAddTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory = MakeInventory();
	const FInventoryOperationResult Result = Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());

	TestTrue(TEXT("Valid definition succeeds"), Result.IsSuccess());
	TestNotNull(TEXT("Successful add returns an item"), Result.Item.Get());
	TestTrue(TEXT("Successful add returns a valid ID"), Result.ItemId.IsValid());
	TestEqual(TEXT("Inventory contains one item"), Inventory->GetItemCount(), 1);
	TestEqual(TEXT("Item outer is the inventory component"), Result.Item->GetOuter(), static_cast<UObject*>(Inventory.Get()));
	TestEqual(TEXT("Definition class is retained"), Result.Item->GetDefinitionClass().Get(), UInventoryTestItemDefinition::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryRepeatedDefinitionTest,
	"NullTide.Inventory.M1.RepeatedDefinitionHasUniqueIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryRepeatedDefinitionTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory = MakeInventory();
	const FInventoryOperationResult First = Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
	const FInventoryOperationResult Second = Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
	const TArray<UItemInstance*> Snapshot = Inventory->GetItemsSnapshot();

	TestTrue(TEXT("First add succeeds"), First.IsSuccess());
	TestTrue(TEXT("Second add succeeds"), Second.IsSuccess());
	TestTrue(TEXT("Repeated definitions get unique IDs"), First.ItemId != Second.ItemId);
	TestEqual(TEXT("Both units are retained"), Snapshot.Num(), 2);
	TestEqual(TEXT("First unit keeps insertion order"), Snapshot[0], First.Item.Get());
	TestEqual(TEXT("Second unit keeps insertion order"), Snapshot[1], Second.Item.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryInvalidDefinitionTest,
	"NullTide.Inventory.M1.InvalidDefinition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryInvalidDefinitionTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory = MakeInventory();
	const FInventoryOperationResult Result = Inventory->TryAddDefinition(nullptr);

	TestTrue(TEXT("Null definition is rejected"), Result.Result == EInventoryOperationResult::InvalidDefinition);
	TestEqual(TEXT("Failed add does not change membership"), Inventory->GetItemCount(), 0);
	TestEqual(TEXT("Failed add does not change revision"), Inventory->GetRevision(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryRemoveValidItemTest,
	"NullTide.Inventory.M1.RemoveValidItem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryRemoveValidItemTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory = MakeInventory();
	const FInventoryOperationResult Added = Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
	const FInventoryOperationResult Removed = Inventory->TryRemoveItem(Added.ItemId);

	TestTrue(TEXT("Valid member removal succeeds"), Removed.IsSuccess());
	TestEqual(TEXT("Remove returns the removed item"), Removed.Item.Get(), Added.Item.Get());
	TestEqual(TEXT("Inventory is empty after removal"), Inventory->GetItemCount(), 0);
	TestFalse(TEXT("Removed ID is no longer a member"), Inventory->ContainsItem(Added.ItemId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryRemoveInvalidIdTest,
	"NullTide.Inventory.M1.RemoveInvalidId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryRemoveInvalidIdTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory = MakeInventory();
	const FInventoryOperationResult Added = Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
	const FInventoryOperationResult Removed = Inventory->TryRemoveItem(FGuid::NewGuid());

	TestTrue(TEXT("Unknown ID is rejected"), Removed.Result == EInventoryOperationResult::InvalidItemId);
	TestEqual(TEXT("Failed remove preserves membership"), Inventory->GetItemCount(), 1);
	TestTrue(TEXT("Original item remains"), Inventory->ContainsItem(Added.ItemId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventorySnapshotIsolationTest,
	"NullTide.Inventory.M1.SnapshotMembershipIsIsolated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventorySnapshotIsolationTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory = MakeInventory();
	const FInventoryOperationResult Added = Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
	TArray<UItemInstance*> Snapshot = Inventory->GetItemsSnapshot();
	Snapshot.Reset();
	Snapshot.Add(nullptr);

	TestEqual(TEXT("Editing snapshot does not change authoritative count"), Inventory->GetItemCount(), 1);
	TestTrue(TEXT("Authoritative member remains findable"), Inventory->ContainsItem(Added.ItemId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryRevisionTest,
	"NullTide.Inventory.M1.RevisionOnlyChangesOnSuccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryRevisionTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory = MakeInventory();
	Inventory->TryAddDefinition(nullptr);
	TestEqual(TEXT("Failed add leaves revision unchanged"), Inventory->GetRevision(), 0);

	const FInventoryOperationResult Added = Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
	TestEqual(TEXT("Successful add increments revision"), Inventory->GetRevision(), 1);

	Inventory->TryRemoveItem(FGuid::NewGuid());
	TestEqual(TEXT("Failed remove leaves revision unchanged"), Inventory->GetRevision(), 1);

	Inventory->TryRemoveItem(Added.ItemId);
	TestEqual(TEXT("Successful remove increments revision"), Inventory->GetRevision(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryEventTest,
	"NullTide.Inventory.M1.EventFiresOnceAfterSuccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryEventTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory = MakeInventory();
	const TStrongObjectPtr<UInventoryComponentTestListener> Listener(
		NewObject<UInventoryComponentTestListener>(GetTransientPackage()));
	Inventory->OnInventoryChanged.AddDynamic(Listener.Get(), &UInventoryComponentTestListener::HandleInventoryChanged);

	Inventory->TryAddDefinition(nullptr);
	TestEqual(TEXT("Failed operation emits no event"), Listener->EventCount, 0);

	const FInventoryOperationResult Added = Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
	TestEqual(TEXT("Successful add emits exactly one event"), Listener->EventCount, 1);
	TestEqual(TEXT("Add event observes committed revision"), Listener->LastRevision, 1);

	Inventory->TryRemoveItem(Added.ItemId);
	TestEqual(TEXT("Successful remove emits exactly one more event"), Listener->EventCount, 2);
	TestEqual(TEXT("Remove event observes committed revision"), Listener->LastRevision, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryReentrantMutationTest,
	"NullTide.Inventory.M1.ReentrantMutationReturnsBusy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryReentrantMutationTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory = MakeInventory();
	const TStrongObjectPtr<UInventoryComponentTestListener> Listener(
		NewObject<UInventoryComponentTestListener>(GetTransientPackage()));
	Listener->Inventory = Inventory.Get();
	Listener->ReentrantDefinitionClass = UInventoryTestItemDefinition::StaticClass();
	Listener->bAttemptReentrantMutation = true;
	Inventory->OnInventoryChanged.AddDynamic(Listener.Get(), &UInventoryComponentTestListener::HandleInventoryChanged);

	const FInventoryOperationResult Added = Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());

	TestTrue(TEXT("Outer add succeeds"), Added.IsSuccess());
	TestTrue(TEXT("Reentrant add is rejected as busy"), Listener->ReentrantResult.Result == EInventoryOperationResult::Busy);
	TestEqual(TEXT("Reentrant attempt does not add membership"), Inventory->GetItemCount(), 1);
	TestEqual(TEXT("Reentrant attempt does not increment revision"), Inventory->GetRevision(), 1);
	TestEqual(TEXT("Reentrant attempt does not emit another event"), Listener->EventCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryLegacyModeTest,
	"NullTide.Inventory.M3.LegacyModeRejectsNativeMutations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryLegacyModeTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory = MakeInventory();
	FBoolProperty* LegacyMode = FindFProperty<FBoolProperty>(
		UInventoryComponent::StaticClass(), TEXT("bLegacyInventoryMode"));
	if (!TestNotNull(TEXT("Legacy-mode default is reflected"), LegacyMode))
	{
		return false;
	}
	LegacyMode->SetPropertyValue_InContainer(Inventory.Get(), true);
	const TStrongObjectPtr<UInventoryComponentTestListener> Listener(
		NewObject<UInventoryComponentTestListener>(GetTransientPackage()));
	Inventory->OnInventoryChanged.AddDynamic(Listener.Get(), &UInventoryComponentTestListener::HandleInventoryChanged);

	TestTrue(TEXT("Legacy mode is enabled"), Inventory->IsLegacyInventoryMode());
	TestTrue(TEXT("Native add is disabled in legacy mode"),
		Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass()).Result == EInventoryOperationResult::NotInitialized);
	TestTrue(TEXT("Native remove is disabled in legacy mode"),
		Inventory->TryRemoveItem(FGuid::NewGuid()).Result == EInventoryOperationResult::NotInitialized);
	TestEqual(TEXT("Native membership stays empty"), Inventory->GetItemsSnapshot().Num(), 0);
	TestEqual(TEXT("Native revision stays zero"), Inventory->GetRevision(), 0);
	TestEqual(TEXT("Rejected native mutations emit no event"), Listener->EventCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInventoryReparentedBlueprintTest,
	"NullTide.Inventory.M3.ReparentedBlueprintKeepsLegacyAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryReparentedBlueprintTest::RunTest(const FString& Parameters)
{
	UClass* ManagerClass = LoadClass<UInventoryComponent>(nullptr,
		TEXT("/Game/LevelPrototyping/InventorySystem/InventoryManagerComponent.InventoryManagerComponent_C"));
	if (!TestNotNull(TEXT("Existing manager loads as a native inventory subclass"), ManagerClass))
	{
		return false;
	}
	TestEqual(TEXT("Manager has the intended direct parent"), ManagerClass->GetSuperClass(), UInventoryComponent::StaticClass());
	const TStrongObjectPtr<UInventoryComponent> Inventory(
		NewObject<UInventoryComponent>(GetTransientPackage(), ManagerClass));
	// Keep the M3 rollback contract under test while production opts into M4 at BeginPlay.
	FBoolProperty* AutoImport = FindFProperty<FBoolProperty>(ManagerClass, TEXT("bImportLegacyInventoryOnBeginPlay"));
	if (!TestNotNull(TEXT("Controlled auto-import setting is reflected"), AutoImport))
	{
		return false;
	}
	AutoImport->SetPropertyValue_InContainer(Inventory.Get(), false);
	TestTrue(TEXT("Existing manager defaults disable native authority"), Inventory->IsLegacyInventoryMode());
	FArrayProperty* LegacyItems = FindFProperty<FArrayProperty>(ManagerClass, TEXT("InventoryItems"));
	UFunction* AddItem = ManagerClass->FindFunctionByName(TEXT("AddItem"));
	if (!TestNotNull(TEXT("Legacy InventoryItems remains reflected"), LegacyItems)
		|| !TestNotNull(TEXT("Legacy AddItem remains callable"), AddItem))
	{
		return false;
	}
	FClassProperty* DefinitionParameter = FindFProperty<FClassProperty>(AddItem, TEXT("ItemDefinitions"));
	if (!TestNotNull(TEXT("AddItem retains its ItemDefinitions class parameter"), DefinitionParameter))
	{
		return false;
	}
	FScriptArrayHelper LegacyArray(LegacyItems, LegacyItems->ContainerPtrToValuePtr<void>(Inventory.Get()));
	TestEqual(TEXT("Legacy default inventory is still empty"), LegacyArray.Num(), 0);
	for (const TCHAR* DefinitionPath : {
		TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Wood.Item_Wood_C"),
		TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_ShortSword.Item_ShortSword_C") })
	{
		UClass* DefinitionClass = LoadClass<UItemDefinition>(nullptr, DefinitionPath);
		if (!TestNotNull(TEXT("Existing item definition loads"), DefinitionClass))
		{
			return false;
		}
		TestTrue(TEXT("Native add cannot create a second store"),
			Inventory->TryAddDefinition(DefinitionClass).Result == EInventoryOperationResult::NotInitialized);
		FStructOnScope Arguments(AddItem);
		DefinitionParameter->SetObjectPropertyValue_InContainer(Arguments.GetStructMemory(), DefinitionClass);
		const int32 PreviousCount = LegacyArray.Num();
		Inventory->ProcessEvent(AddItem, Arguments.GetStructMemory());
		TestEqual(TEXT("Legacy AddItem appends exactly one entry"),
			LegacyArray.Num(), PreviousCount + 1);
	}
	TestEqual(TEXT("Wood and Sword are retained by the legacy store"), LegacyArray.Num(), 2);
	TestEqual(TEXT("Native Items stays empty"), Inventory->GetItemCount(), 0);
	TestEqual(TEXT("Native revision stays zero"), Inventory->GetRevision(), 0);
	TestTrue(TEXT("Native removal remains disabled"),
		Inventory->TryRemoveItem(FGuid::NewGuid()).Result == EInventoryOperationResult::NotInitialized);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
