#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "Items/Fragments/ItemFragment_Consumable.h"
#include "Items/Fragments/ItemFragment_Weapon.h"
#include "Inventory/InventoryComponent.h"
#include "InventoryComponentTestTypes.h"
#include "InventoryUITestTypes.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "UI/InventoryDragDropOperation.h"
#include "UI/InventoryUICategory.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FInventoryUICatalogCase
{
	const TCHAR* Id;
	EInventoryCategory Category;
	const TCHAR* SummaryNeedle;
};

const FInventoryUICatalogCase Cases[] = {
	{ TEXT("HealthPotion"), EInventoryCategory::Consumable, TEXT("Healing: 50") },
	{ TEXT("ShortSword"), EInventoryCategory::Weapon, TEXT("Damage: 10") },
	{ TEXT("Wood"), EInventoryCategory::Resource, TEXT("Type: Wood") },
	{ TEXT("Sandwich"), EInventoryCategory::Consumable, TEXT("Hunger: 30") },
	{ TEXT("Stone"), EInventoryCategory::Resource, TEXT("Type: Stone") },
	{ TEXT("Axe"), EInventoryCategory::Misc, TEXT("Tool: Axe") },
	{ TEXT("Pickaxe"), EInventoryCategory::Misc, TEXT("Tool: Pickaxe") },
	{ TEXT("Meat"), EInventoryCategory::Consumable, TEXT("Hunger: 20") },
	{ TEXT("LongSword"), EInventoryCategory::Weapon, TEXT("Durability: 150") },
	{ TEXT("WoodBow"), EInventoryCategory::Weapon, TEXT("Range: 800") },
	{ TEXT("WoodArrow"), EInventoryCategory::Ammo, TEXT("Type: Arrow") },
	{ TEXT("WaterBottle"), EInventoryCategory::Consumable, TEXT("Hydration: 30") }
};

UClass* LoadProductionDefinition(const TCHAR* Id)
{
	const FString ClassPath = FString::Printf(
		TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_%s.Item_%s_C"), Id, Id);
	return LoadClass<UItemDefinition>(nullptr, *ClassPath);
}

bool InventoryStateIsUnchanged(FAutomationTestBase& Test, const UInventoryComponent* Inventory,
	const TArray<UItemInstance*>& ExpectedItems, const int32 ExpectedRevision)
{
	return Test.TestEqual(TEXT("Inventory revision is unchanged"), Inventory->GetRevision(), ExpectedRevision)
		&& Test.TestTrue(TEXT("Inventory membership and order are unchanged"), Inventory->GetItemsSnapshot() == ExpectedItems);
}
}

UInventoryUIMixedDefinition::UInventoryUIMixedDefinition()
{
	ItemFragments.Add(CreateDefaultSubobject<UItemFragment_Consumable>(TEXT("Consumable")));
	ItemFragments.Add(CreateDefaultSubobject<UItemFragment_Weapon>(TEXT("Weapon")));
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FInventoryUICatalogProjectionTest, "NullTide.Gameplay.G4.InventoryUI.CatalogProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FInventoryUICatalogProjectionTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (const FInventoryUICatalogCase& TestCase : Cases)
	{
		Names.Add(TestCase.Id);
		Commands.Add(TestCase.Id);
	}
}

bool FInventoryUICatalogProjectionTest::RunTest(const FString& Parameters)
{
	const FInventoryUICatalogCase* TestCase = nullptr;
	for (const FInventoryUICatalogCase& Candidate : Cases)
	{
		if (Parameters == Candidate.Id)
		{
			TestCase = &Candidate;
			break;
		}
	}
	if (!TestNotNull(TEXT("Known catalog case"), TestCase))
	{
		return false;
	}

	UClass* DefinitionClass = LoadProductionDefinition(TestCase->Id);
	if (!TestNotNull(TEXT("Production definition loads"), DefinitionClass))
	{
		return false;
	}
	TestEqual(TEXT("Canonical category projection"), UInventoryUIFunctionLibrary::ResolveCategory(DefinitionClass), TestCase->Category);
	const FString Summary = UInventoryUIFunctionLibrary::BuildCapabilitySummary(DefinitionClass).ToString();
	TestTrue(TEXT("Canonical capability summary: ") + Summary, Summary.Contains(TestCase->SummaryNeedle));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryUIIdentityProjectionTest, "NullTide.Gameplay.G4.InventoryUI.IdentityProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryUIIdentityProjectionTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Weapon wins category precedence"),
		UInventoryUIFunctionLibrary::ResolveCategory(UInventoryUIMixedDefinition::StaticClass()), EInventoryCategory::Weapon);

	UClass* WoodClass = LoadProductionDefinition(TEXT("Wood"));
	if (!TestNotNull(TEXT("Wood definition loads"), WoodClass))
	{
		return false;
	}

	const TStrongObjectPtr<UInventoryComponent> Inventory(NewObject<UInventoryComponent>());
	const FInventoryOperationResult First = Inventory->TryAddDefinition(WoodClass);
	const FInventoryOperationResult Second = Inventory->TryAddDefinition(WoodClass);
	if (!TestTrue(TEXT("Two identities acquired"), First.IsSuccess() && Second.IsSuccess() && First.Item && Second.Item))
	{
		return false;
	}

	TestTrue(TEXT("Selected ItemId is valid in All"),
		UInventoryUIFunctionLibrary::IsSelectedItemValid(Inventory.Get(), First.ItemId, EInventoryCategory::All));
	TestTrue(TEXT("Resource filter includes Wood"),
		UInventoryUIFunctionLibrary::IsItemVisible(Inventory.Get(), First.Item, EInventoryCategory::Resource));
	TestFalse(TEXT("Weapon filter hides Wood"),
		UInventoryUIFunctionLibrary::IsItemVisible(Inventory.Get(), First.Item, EInventoryCategory::Weapon));
	TestTrue(TEXT("Exact drag identity is valid"),
		UInventoryUIFunctionLibrary::IsDragPayloadValid(Inventory.Get(), First.ItemId, First.Item));
	TestFalse(TEXT("Mismatched drag object is rejected"),
		UInventoryUIFunctionLibrary::IsDragPayloadValid(Inventory.Get(), First.ItemId, Second.Item));

	const FInventoryOperationResult Removed = Inventory->TryRemoveItem(First.ItemId);
	TestTrue(TEXT("Selected item removed"), Removed.IsSuccess());
	TestFalse(TEXT("Removed selected ItemId is invalid"),
		UInventoryUIFunctionLibrary::IsSelectedItemValid(Inventory.Get(), First.ItemId, EInventoryCategory::All));
	TestFalse(TEXT("Removed drag payload is stale"),
		UInventoryUIFunctionLibrary::IsDragPayloadValid(Inventory.Get(), First.ItemId, First.Item));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryUIDragLifecycleTest, "NullTide.Gameplay.G5.DragDrop.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryUIDragLifecycleTest::RunTest(const FString& Parameters)
{
	UClass* WoodClass = LoadProductionDefinition(TEXT("Wood"));
	if (!TestNotNull(TEXT("Wood definition loads"), WoodClass))
	{
		return false;
	}

	const TStrongObjectPtr<UInventoryComponent> Inventory(NewObject<UInventoryComponent>());
	const FInventoryOperationResult Added = Inventory->TryAddDefinition(WoodClass);
	if (!TestTrue(TEXT("Item acquisition succeeds"), Added.IsSuccess() && Added.Item))
	{
		return false;
	}

	const int32 RevisionBeforeDrag = Inventory->GetRevision();
	const TArray<UItemInstance*> ItemsBeforeDrag = Inventory->GetItemsSnapshot();
	UInventoryDragDropOperation* Operation = NewObject<UInventoryDragDropOperation>();
	Operation->InitializePayload(Inventory.Get(), Added.Item);
	TestTrue(TEXT("Exact authoritative payload is valid"), Operation->IsPayloadValid(Inventory.Get()));
	Operation->Complete(EInventoryUIDropResult::Cancelled);
	TestTrue(TEXT("Cancel records a result"), Operation->IsComplete() && Operation->GetResult() == EInventoryUIDropResult::Cancelled);
	TestFalse(TEXT("Completed payload cannot be reused"), Operation->IsPayloadValid(Inventory.Get()));
	return InventoryStateIsUnchanged(*this, Inventory.Get(), ItemsBeforeDrag, RevisionBeforeDrag);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryUISafeDropTest, "NullTide.Gameplay.G5.DragDrop.SafeDropResults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryUISafeDropTest::RunTest(const FString& Parameters)
{
	UClass* WoodClass = LoadProductionDefinition(TEXT("Wood"));
	if (!TestNotNull(TEXT("Wood definition loads"), WoodClass))
	{
		return false;
	}

	const TStrongObjectPtr<UInventoryComponent> Inventory(NewObject<UInventoryComponent>());
	const FInventoryOperationResult First = Inventory->TryAddDefinition(WoodClass);
	const FInventoryOperationResult Second = Inventory->TryAddDefinition(WoodClass);
	if (!TestTrue(TEXT("Two source and target identities acquired"), First.IsSuccess() && Second.IsSuccess()))
	{
		return false;
	}

	const int32 RevisionBeforeDrop = Inventory->GetRevision();
	const TArray<UItemInstance*> ItemsBeforeDrop = Inventory->GetItemsSnapshot();
	for (const EInventoryUIDropResult Result : {
		EInventoryUIDropResult::AcceptedNoMutation,
		EInventoryUIDropResult::OutsideDropRequested })
	{
		UInventoryDragDropOperation* Operation = NewObject<UInventoryDragDropOperation>();
		Operation->InitializePayload(Inventory.Get(), First.Item);
		TestTrue(TEXT("Payload is valid before non-mutating drop"), Operation->IsPayloadValid(Inventory.Get()));
		Operation->Complete(Result);
		TestEqual(TEXT("Drop result is preserved"), Operation->GetResult(), Result);
		if (!InventoryStateIsUnchanged(*this, Inventory.Get(), ItemsBeforeDrop, RevisionBeforeDrop))
		{
			return false;
		}
	}

	TestTrue(TEXT("Target identity remains independently valid"), Inventory->ContainsItem(Second.ItemId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryUIDragInvalidationTest, "NullTide.Gameplay.G5.DragDrop.FilterAndRefreshInvalidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryUIDragInvalidationTest::RunTest(const FString& Parameters)
{
	UClass* WoodClass = LoadProductionDefinition(TEXT("Wood"));
	UClass* PotionClass = LoadProductionDefinition(TEXT("HealthPotion"));
	if (!TestTrue(TEXT("Definitions load"), WoodClass && PotionClass))
	{
		return false;
	}

	const TStrongObjectPtr<UInventoryComponent> Inventory(NewObject<UInventoryComponent>());
	const FInventoryOperationResult Wood = Inventory->TryAddDefinition(WoodClass);
	const FInventoryOperationResult Potion = Inventory->TryAddDefinition(PotionClass);
	if (!TestTrue(TEXT("Items acquired"), Wood.IsSuccess() && Potion.IsSuccess()))
	{
		return false;
	}

	const int32 RevisionBeforeFilter = Inventory->GetRevision();
	const TArray<UItemInstance*> ItemsBeforeFilter = Inventory->GetItemsSnapshot();
	UInventoryDragDropOperation* FilterOperation = NewObject<UInventoryDragDropOperation>();
	FilterOperation->InitializePayload(Inventory.Get(), Wood.Item);
	TestFalse(TEXT("Changing to Consumable hides the dragged Wood"),
		UInventoryUIFunctionLibrary::IsItemVisible(Inventory.Get(), Wood.Item, EInventoryCategory::Consumable));
	FilterOperation->Complete(EInventoryUIDropResult::Cancelled);
	if (!InventoryStateIsUnchanged(*this, Inventory.Get(), ItemsBeforeFilter, RevisionBeforeFilter))
	{
		return false;
	}

	UInventoryDragDropOperation* StaleOperation = NewObject<UInventoryDragDropOperation>();
	StaleOperation->InitializePayload(Inventory.Get(), Wood.Item);
	TestTrue(TEXT("Payload is valid before refresh removes its item"), StaleOperation->IsPayloadValid(Inventory.Get()));
	TestTrue(TEXT("Authoritative removal succeeds"), Inventory->TryRemoveItem(Wood.ItemId).IsSuccess());
	TestFalse(TEXT("Refresh rejects the removed payload"), StaleOperation->IsPayloadValid(Inventory.Get()));
	StaleOperation->Complete(EInventoryUIDropResult::Invalid);
	TestEqual(TEXT("Stale payload records invalid result"), StaleOperation->GetResult(), EInventoryUIDropResult::Invalid);
	TestEqual(TEXT("Only authoritative removal advances revision"), Inventory->GetRevision(), RevisionBeforeFilter + 1);
	TestTrue(TEXT("Unrelated item remains in inventory"), Inventory->ContainsItem(Potion.ItemId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryWorldDropSuccessTest, "NullTide.Gameplay.G6.WorldDrop.SuccessPreservesDuplicateIdentityAndProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryWorldDropSuccessTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryWorldDropTestComponent> Inventory(
		NewObject<UInventoryWorldDropTestComponent>(GetTransientPackage()));
	const TStrongObjectPtr<APawn> Pawn(NewObject<APawn>(GetTransientPackage()));
	const TStrongObjectPtr<UInventoryComponentTestListener> Listener(NewObject<UInventoryComponentTestListener>(GetTransientPackage()));
	Listener->Inventory = Inventory.Get();
	Inventory->OnInventoryChanged.AddDynamic(Listener.Get(), &UInventoryComponentTestListener::HandleInventoryChanged);

	const FInventoryOperationResult First = Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
	const FInventoryOperationResult Second = Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
	if (!TestTrue(TEXT("Two same-definition instances are acquired"), First.IsSuccess() && Second.IsSuccess()))
	{
		return false;
	}
	const int32 RevisionBeforeDrop = Inventory->GetRevision();
	const int32 EventsBeforeDrop = Listener->EventCount;

	const FInventoryWorldDropResult Dropped = Inventory->TryDropItemToWorld(Pawn.Get(), First.ItemId);
	TestTrue(TEXT("World drop transaction succeeds"), Dropped.IsSuccess());
	TestEqual(TEXT("World drop reports the requested identity"), Dropped.ItemId, First.ItemId);
	TestNotNull(TEXT("Generic pickup spawn is returned"), Dropped.SpawnedPickup.Get());
	TestEqual(TEXT("Only the exact duplicate identity is removed"), Inventory->GetItemsSnapshot(), TArray<UItemInstance*>({ Second.Item.Get() }));
	TestTrue(TEXT("Remaining duplicate keeps its distinct ID"), Inventory->ContainsItem(Second.ItemId) && !Inventory->ContainsItem(First.ItemId));
	TestEqual(TEXT("Successful drop commits one revision"), Inventory->GetRevision(), RevisionBeforeDrop + 1);
	TestEqual(TEXT("Successful drop emits one event"), Listener->EventCount, EventsBeforeDrop + 1);
	TestEqual(TEXT("No rollback follows a successful commit"), Inventory->RollbackCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryWorldDropFailureTest, "NullTide.Gameplay.G6.WorldDrop.FailuresAreAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryWorldDropFailureTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryWorldDropTestComponent> Inventory(
		NewObject<UInventoryWorldDropTestComponent>(GetTransientPackage()));
	const TStrongObjectPtr<APawn> Pawn(NewObject<APawn>(GetTransientPackage()));
	const TStrongObjectPtr<UInventoryComponentTestListener> Listener(NewObject<UInventoryComponentTestListener>(GetTransientPackage()));
	Listener->Inventory = Inventory.Get();
	Inventory->OnInventoryChanged.AddDynamic(Listener.Get(), &UInventoryComponentTestListener::HandleInventoryChanged);
	const FInventoryOperationResult Added = Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
	if (!TestTrue(TEXT("Item acquisition succeeds"), Added.IsSuccess()))
	{
		return false;
	}
	const TArray<UItemInstance*> ItemsBefore = Inventory->GetItemsSnapshot();
	const int32 RevisionBefore = Inventory->GetRevision();
	const int32 EventsBefore = Listener->EventCount;

	TestEqual(TEXT("Stale identity is rejected before spawn"),
		Inventory->TryDropItemToWorld(Pawn.Get(), FGuid::NewGuid()).Result, EInventoryWorldDropResult::InvalidItemId);
	TestEqual(TEXT("Missing pawn is rejected before spawn"),
		Inventory->TryDropItemToWorld(nullptr, Added.ItemId).Result, EInventoryWorldDropResult::InvalidPawn);
	Inventory->bResolveTransformSucceeds = false;
	TestEqual(TEXT("Unsafe placement is rejected before spawn"),
		Inventory->TryDropItemToWorld(Pawn.Get(), Added.ItemId).Result, EInventoryWorldDropResult::NoSafeTransform);
	Inventory->bResolveTransformSucceeds = true;
	Inventory->bSpawnSucceeds = false;
	TestEqual(TEXT("Spawn failure is rejected before removal"),
		Inventory->TryDropItemToWorld(Pawn.Get(), Added.ItemId).Result, EInventoryWorldDropResult::SpawnFailed);
	TestEqual(TEXT("Only the spawn-failure path attempted a spawn"), Inventory->SpawnAttempts, 1);
	TestEqual(TEXT("Failures emit no event"), Listener->EventCount, EventsBefore);
	return InventoryStateIsUnchanged(*this, Inventory.Get(), ItemsBefore, RevisionBefore);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryWorldDropRollbackTest, "NullTide.Gameplay.G6.WorldDrop.RemoveFailureRollsBackAndProjectsOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryWorldDropRollbackTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryWorldDropTestComponent> Inventory(
		NewObject<UInventoryWorldDropTestComponent>(GetTransientPackage()));
	const TStrongObjectPtr<APawn> Pawn(NewObject<APawn>(GetTransientPackage()));
	const TStrongObjectPtr<UInventoryComponentTestListener> Listener(NewObject<UInventoryComponentTestListener>(GetTransientPackage()));
	Listener->Inventory = Inventory.Get();
	Inventory->OnInventoryChanged.AddDynamic(Listener.Get(), &UInventoryComponentTestListener::HandleInventoryChanged);
	Inventory->InventoryItems = { UInventoryTestItemDefinition::StaticClass(), UInventoryOtherTestItemDefinition::StaticClass(), UInventoryTestItemDefinition::StaticClass() };
	FindFProperty<FBoolProperty>(UInventoryComponent::StaticClass(), TEXT("bLegacyInventoryMode"))
		->SetPropertyValue_InContainer(Inventory.Get(), true);
	FindFProperty<FBoolProperty>(UInventoryComponent::StaticClass(), TEXT("bImportLegacyInventoryOnBeginPlay"))
		->SetPropertyValue_InContainer(Inventory.Get(), true);
	if (!TestTrue(TEXT("Compatibility seed initializes"), Inventory->InitializeFromLegacyInventory().IsSuccess()))
	{
		return false;
	}
	const TArray<UItemInstance*> ItemsBefore = Inventory->GetItemsSnapshot();
	const TArray<TSubclassOf<UItemDefinition>> ProjectionBefore = Inventory->InventoryItems;
	const int32 RevisionBefore = Inventory->GetRevision();
	const int32 EventsBefore = Listener->EventCount;
	Inventory->bCommitSucceeds = false;

	const FInventoryWorldDropResult Dropped = Inventory->TryDropItemToWorld(Pawn.Get(), ItemsBefore[1]->GetInstanceId());
	TestEqual(TEXT("Removal failure is reported after rollback"), Dropped.Result, EInventoryWorldDropResult::RemovalFailedRolledBack);
	TestEqual(TEXT("Spawned pickup is rolled back exactly once"), Inventory->RollbackCount, 1);
	TestEqual(TEXT("Rollback receives the spawned pickup"), Inventory->LastRolledBackPickup, Inventory->LastSpawnedPickup);
	TestTrue(TEXT("Compatibility projection order remains unchanged"), Inventory->InventoryItems == ProjectionBefore);
	TestEqual(TEXT("Rollback emits no event"), Listener->EventCount, EventsBefore);
	if (!InventoryStateIsUnchanged(*this, Inventory.Get(), ItemsBefore, RevisionBefore))
	{
		return false;
	}

	Inventory->bCommitSucceeds = true;
	const FInventoryWorldDropResult Committed = Inventory->TryDropItemToWorld(Pawn.Get(), ItemsBefore[1]->GetInstanceId());
	TestTrue(TEXT("Exact legacy instance commits after a prior rollback"), Committed.IsSuccess());
	TestEqual(TEXT("Remaining authoritative order skips only the dropped identity"),
		Inventory->GetItemsSnapshot(), TArray<UItemInstance*>({ ItemsBefore[0], ItemsBefore[2] }));
	TestTrue(TEXT("Compatibility projection rebuilds in remaining order"), Inventory->InventoryItems == TArray<TSubclassOf<UItemDefinition>>({
		UInventoryTestItemDefinition::StaticClass(), UInventoryTestItemDefinition::StaticClass() }));
	TestEqual(TEXT("Successful compatibility drop advances revision once"), Inventory->GetRevision(), RevisionBefore + 1);
	TestEqual(TEXT("Successful compatibility drop emits one event"), Listener->EventCount, EventsBefore + 1);
	return true;
}
#endif
