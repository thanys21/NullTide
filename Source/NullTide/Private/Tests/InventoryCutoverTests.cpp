#include "InventoryComponentTestTypes.h"

#include "Compatibility/LegacyInventoryCompatibilityLibrary.h"
#include "Items/ItemInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
TStrongObjectPtr<UInventoryCutoverTestManager> MakeLegacySeedInventory()
{
	TStrongObjectPtr<UInventoryCutoverTestManager> Inventory(
		NewObject<UInventoryCutoverTestManager>(GetTransientPackage()));
	FindFProperty<FBoolProperty>(UInventoryComponent::StaticClass(), TEXT("bLegacyInventoryMode"))
		->SetPropertyValue_InContainer(Inventory.Get(), true);
	FindFProperty<FBoolProperty>(UInventoryComponent::StaticClass(), TEXT("bImportLegacyInventoryOnBeginPlay"))
		->SetPropertyValue_InContainer(Inventory.Get(), true);
	return Inventory;
}

TStrongObjectPtr<UInventoryComponentTestListener> Listen(UInventoryComponent* Inventory)
{
	TStrongObjectPtr<UInventoryComponentTestListener> Listener(
		NewObject<UInventoryComponentTestListener>(GetTransientPackage()));
	Listener->Inventory = Inventory;
	Inventory->OnInventoryChanged.AddDynamic(Listener.Get(), &UInventoryComponentTestListener::HandleInventoryChanged);
	return Listener;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryDuplicateSeedTest,
	"NullTide.Inventory.M4.DuplicateSeedPreservesOrderAndIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryDuplicateSeedTest::RunTest(const FString& Parameters)
{
	const auto Inventory = MakeLegacySeedInventory();
	Inventory->InventoryItems = { UInventoryTestItemDefinition::StaticClass(),
		UInventoryOtherTestItemDefinition::StaticClass(), UInventoryTestItemDefinition::StaticClass() };
	const auto Seed = Inventory->InventoryItems;
	const auto Listener = Listen(Inventory.Get());
	TestTrue(TEXT("Legacy seed starts pending"), Inventory->GetInitializationState() == EInventoryInitializationState::PendingLegacyImport);
	TestTrue(TEXT("Whole seed import succeeds"), Inventory->InitializeFromLegacyInventory().IsSuccess());
	const auto Items = Inventory->GetItemsSnapshot();
	if (!TestEqual(TEXT("One instance per legacy entry"), Items.Num(), 3)) { return false; }
	TSet<FGuid> Ids;
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		TestEqual(TEXT("Definition order and duplicates are preserved"), Items[Index]->GetDefinitionClass().Get(), Seed[Index].Get());
		TestEqual(TEXT("Imported outer is the existing component"), Items[Index]->GetOuter(), static_cast<UObject*>(Inventory.Get()));
		TestTrue(TEXT("Imported ID is valid and unique"), Items[Index]->GetInstanceId().IsValid() && !Ids.Contains(Items[Index]->GetInstanceId()));
		Ids.Add(Items[Index]->GetInstanceId());
	}
	TestTrue(TEXT("Duplicate definitions have distinct instances"), Items[0] != Items[2]);
	TestFalse(TEXT("Successful import disables legacy mode"), Inventory->IsLegacyInventoryMode());
	TestTrue(TEXT("Explicit state is native ready"), Inventory->GetInitializationState() == EInventoryInitializationState::NativeReady);
	TestTrue(TEXT("Native authority is active"), Inventory->IsNativeAuthorityActive());
	TestTrue(TEXT("Projection preserves the seed"), Inventory->InventoryItems == Seed);
	TestEqual(TEXT("Import commits one revision"), Inventory->GetRevision(), 1);
	TestEqual(TEXT("Import emits one event"), Listener->EventCount, 1);
	TestEqual(TEXT("Callback sees all committed items"), Listener->ObservedItemCount, 3);
	TestTrue(TEXT("Callback query sees committed definitions"), Listener->ObservedDefinitions == Seed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryEmptySeedTest,
	"NullTide.Inventory.M4.EmptySeedCutsOver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryEmptySeedTest::RunTest(const FString& Parameters)
{
	const auto Inventory = MakeLegacySeedInventory();
	const auto Listener = Listen(Inventory.Get());
	TestTrue(TEXT("Empty seed succeeds"), Inventory->InitializeFromLegacyInventory().IsSuccess());
	TestEqual(TEXT("Empty import creates no units"), Inventory->GetItemCount(), 0);
	TestTrue(TEXT("Empty import enables native authority"), Inventory->IsNativeAuthorityActive());
	TestEqual(TEXT("Empty cutover commits one revision"), Inventory->GetRevision(), 1);
	TestEqual(TEXT("Empty cutover notifies once"), Listener->EventCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryInvalidSeedTest,
	"NullTide.Inventory.M4.InvalidSeedIsAtomicAndTerminal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryInvalidSeedTest::RunTest(const FString& Parameters)
{
	const auto Inventory = MakeLegacySeedInventory();
	Inventory->InventoryItems = { UInventoryTestItemDefinition::StaticClass(), nullptr,
		UInventoryOtherTestItemDefinition::StaticClass() };
	const auto Seed = Inventory->InventoryItems;
	const auto Listener = Listen(Inventory.Get());
	TestTrue(TEXT("Null seed entry is rejected"), Inventory->InitializeFromLegacyInventory().Result == EInventoryOperationResult::InvalidDefinition);
	TestEqual(TEXT("No partial native import"), Inventory->GetItemCount(), 0);
	TestTrue(TEXT("Failed import preserves the entire seed"), Inventory->InventoryItems == Seed);
	TestEqual(TEXT("Failed import leaves revision zero"), Inventory->GetRevision(), 0);
	TestEqual(TEXT("Failed import emits no event"), Listener->EventCount, 0);
	TestTrue(TEXT("Failed state is explicit"), Inventory->GetInitializationState() == EInventoryInitializationState::Failed);
	TestTrue(TEXT("Failure result is retained"), Inventory->GetLastInitializationResult() == EInventoryOperationResult::InvalidDefinition);
	TestTrue(TEXT("Failed component remains in legacy mode"), Inventory->IsLegacyInventoryMode());
	TestTrue(TEXT("Failed production legacy writes are blocked"),
		ULegacyInventoryCompatibilityLibrary::TryAddDefinitionToLegacyInventory(Inventory.Get(), UInventoryTestItemDefinition::StaticClass()).Result == EInventoryOperationResult::NotInitialized);
	TestTrue(TEXT("Failed native writes are blocked"),
		Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass()).Result == EInventoryOperationResult::NotInitialized);
	Inventory->InventoryItems = { UInventoryTestItemDefinition::StaticClass() };
	TestTrue(TEXT("Failure cannot re-read a changed seed on retry"),
		Inventory->InitializeFromLegacyInventory().Result == EInventoryOperationResult::InvalidDefinition);
	TestEqual(TEXT("Retry still imports nothing"), Inventory->GetItemCount(), 0);
	TestEqual(TEXT("Retry leaves revision zero"), Inventory->GetRevision(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryInvalidSeedDataTest,
	"NullTide.Inventory.M4.InvalidSeedDataIsAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryInvalidSeedDataTest::RunTest(const FString& Parameters)
{
	const auto Inventory = MakeLegacySeedInventory();
	Inventory->InventoryItems = { UInventoryTestItemDefinition::StaticClass(),
		UInventoryInvalidDataTestDefinition::StaticClass() };
	const auto Seed = Inventory->InventoryItems;
	TestTrue(TEXT("Null fragment rejects the entire seed"),
		Inventory->InitializeFromLegacyInventory().Result == EInventoryOperationResult::InvalidDefinitionData);
	TestEqual(TEXT("Valid prefix was not imported"), Inventory->GetItemCount(), 0);
	TestTrue(TEXT("Original invalid seed is untouched"), Inventory->InventoryItems == Seed);
	TestEqual(TEXT("Invalid data does not change revision"), Inventory->GetRevision(), 0);
	const auto AbstractSeed = MakeLegacySeedInventory();
	AbstractSeed->InventoryItems = { UInventoryTestItemDefinition::StaticClass(), UItemDefinition::StaticClass() };
	TestTrue(TEXT("Abstract definition is rejected"), AbstractSeed->InitializeFromLegacyInventory().Result == EInventoryOperationResult::InvalidDefinition);
	TestEqual(TEXT("Abstract seed imports nothing"), AbstractSeed->GetItemCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryIdempotentCutoverTest,
	"NullTide.Inventory.M4.InitializationIsIdempotentAndProjectionIsNotInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryIdempotentCutoverTest::RunTest(const FString& Parameters)
{
	const auto Inventory = MakeLegacySeedInventory();
	Inventory->InventoryItems = { UInventoryTestItemDefinition::StaticClass(), UInventoryTestItemDefinition::StaticClass() };
	const auto Listener = Listen(Inventory.Get());
	if (!TestTrue(TEXT("Initial import succeeds"), Inventory->InitializeFromLegacyInventory().IsSuccess())) { return false; }
	const auto OriginalItems = Inventory->GetItemsSnapshot();
	Inventory->InventoryItems = { nullptr, UInventoryOtherTestItemDefinition::StaticClass() };
	TestTrue(TEXT("Second initialization returns success without importing"), Inventory->InitializeFromLegacyInventory().IsSuccess());
	TestTrue(TEXT("Authoritative instances and IDs are unchanged"), Inventory->GetItemsSnapshot() == OriginalItems);
	TestEqual(TEXT("Initialization cannot commit twice"), Inventory->GetRevision(), 1);
	TestEqual(TEXT("Initialization cannot notify twice"), Listener->EventCount, 1);
	EInventoryOperationResult QueryResult;
	const auto Query = ULegacyInventoryCompatibilityLibrary::GetLegacyItemsSnapshot(Inventory.Get(), QueryResult);
	TestTrue(TEXT("Compatibility query is native-derived despite projection tampering"),
		QueryResult == EInventoryOperationResult::Success && Query.Num() == 2
		&& Query[0] == UInventoryTestItemDefinition::StaticClass() && Query[1] == UInventoryTestItemDefinition::StaticClass());
	TestTrue(TEXT("Native add ignores corrupt projection input"),
		Inventory->TryAddDefinition(UInventoryOtherTestItemDefinition::StaticClass()).IsSuccess());
	TestEqual(TEXT("No additional projected entry was imported"), Inventory->GetItemCount(), 3);
	TestTrue(TEXT("Successful mutation rebuilds projection from native items"),
		Inventory->InventoryItems.Num() == 3 && Inventory->InventoryItems[0] == UInventoryTestItemDefinition::StaticClass()
		&& Inventory->InventoryItems[1] == UInventoryTestItemDefinition::StaticClass()
		&& Inventory->InventoryItems[2] == UInventoryOtherTestItemDefinition::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryNativeProjectionTest,
	"NullTide.Inventory.M4.NativeMutationsProjectBeforeNotification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryNativeProjectionTest::RunTest(const FString& Parameters)
{
	const auto Inventory = MakeLegacySeedInventory();
	Inventory->InventoryItems = { UInventoryTestItemDefinition::StaticClass() };
	const auto Listener = Listen(Inventory.Get());
	if (!TestTrue(TEXT("Seed import succeeds"), Inventory->InitializeFromLegacyInventory().IsSuccess())) { return false; }
	const FGuid SeedId = Inventory->GetItemsSnapshot()[0]->GetInstanceId();
	const auto Added = Inventory->TryAddDefinition(UInventoryOtherTestItemDefinition::StaticClass());
	TestTrue(TEXT("Native add succeeds after cutover"), Added.IsSuccess());
	TestTrue(TEXT("Added item has a fresh valid ID"), Added.ItemId.IsValid() && Added.ItemId != SeedId);
	TestTrue(TEXT("Projection and callback both see two entries"), Inventory->InventoryItems.Num() == 2 && Listener->ObservedDefinitions == Inventory->InventoryItems);
	TestEqual(TEXT("Add revision is two"), Inventory->GetRevision(), 2);
	TestTrue(TEXT("Native remove succeeds"), Inventory->TryRemoveItem(SeedId).IsSuccess());
	TestTrue(TEXT("Removal preserves remaining insertion order"),
		Inventory->GetItemCount() == 1 && Inventory->GetItemsSnapshot()[0] == Added.Item.Get());
	TestTrue(TEXT("Removal projection contains only the remaining definition"),
		Inventory->InventoryItems.Num() == 1 && Inventory->InventoryItems[0] == UInventoryOtherTestItemDefinition::StaticClass()
		&& Listener->ObservedDefinitions == Inventory->InventoryItems);
	TestEqual(TEXT("Remove revision is three"), Inventory->GetRevision(), 3);
	TestEqual(TEXT("Import/add/remove each notified once"), Listener->EventCount, 3);
	const auto Projection = Inventory->InventoryItems;
	TestTrue(TEXT("Invalid native add fails"), Inventory->TryAddDefinition(nullptr).Result == EInventoryOperationResult::InvalidDefinition);
	TestTrue(TEXT("Invalid native remove fails"), Inventory->TryRemoveItem(FGuid::NewGuid()).Result == EInventoryOperationResult::InvalidItemId);
	TestTrue(TEXT("Failures preserve projection"), Inventory->InventoryItems == Projection);
	TestEqual(TEXT("Failures preserve membership"), Inventory->GetItemCount(), 1);
	TestEqual(TEXT("Failures preserve revision"), Inventory->GetRevision(), 3);
	TestEqual(TEXT("Failures emit no event"), Listener->EventCount, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryNativeCompatibilityTest,
	"NullTide.Inventory.M4.CompatibilityDispatchesNativeAndBlocksPendingWrites",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryNativeCompatibilityTest::RunTest(const FString& Parameters)
{
	const auto Inventory = MakeLegacySeedInventory();
	TestTrue(TEXT("Pending production compatibility add is rejected"),
		ULegacyInventoryCompatibilityLibrary::TryAddDefinitionToLegacyInventory(Inventory.Get(), UInventoryTestItemDefinition::StaticClass()).Result == EInventoryOperationResult::NotInitialized);
	TestEqual(TEXT("Pending add cannot alter the seed"), Inventory->InventoryItems.Num(), 0);
	EInventoryOperationResult QueryResult;
	ULegacyInventoryCompatibilityLibrary::GetLegacyItemsSnapshot(Inventory.Get(), QueryResult);
	TestTrue(TEXT("Pending production query reports uninitialized"), QueryResult == EInventoryOperationResult::NotInitialized);
	if (!TestTrue(TEXT("Empty cutover succeeds"), Inventory->InitializeFromLegacyInventory().IsSuccess())) { return false; }
	const auto Added = ULegacyInventoryCompatibilityLibrary::TryAddDefinitionToLegacyInventory(Inventory.Get(), UInventoryTestItemDefinition::StaticClass());
	TestTrue(TEXT("Compatibility add returns native success"), Added.IsSuccess());
	TestNotNull(TEXT("Compatibility add now returns a native instance"), Added.Item.Get());
	TestTrue(TEXT("Compatibility add now returns a valid GUID"), Added.ItemId.IsValid());
	TestEqual(TEXT("Only the native store acquired one unit"), Inventory->GetItemCount(), 1);
	TestEqual(TEXT("Compatibility projection has one entry"), Inventory->InventoryItems.Num(), 1);
	TestEqual(TEXT("Compatibility add commits a native revision"), Inventory->GetRevision(), 2);
	const auto Invalid = ULegacyInventoryCompatibilityLibrary::TryAddDefinitionToLegacyInventory(Inventory.Get(), nullptr);
	TestTrue(TEXT("Native failure has no legacy fallback"), Invalid.Result == EInventoryOperationResult::InvalidDefinition);
	TestEqual(TEXT("Failed compatibility add awards nothing"), Inventory->GetItemCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryCutoverReentrantTest,
	"NullTide.Inventory.M4.ReentrantCutoverAndMutationReturnBusy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryCutoverReentrantTest::RunTest(const FString& Parameters)
{
	const auto Inventory = MakeLegacySeedInventory();
	Inventory->InventoryItems = { UInventoryTestItemDefinition::StaticClass() };
	const auto Listener = Listen(Inventory.Get());
	Listener->bAttemptReentrantMutation = true;
	Listener->ReentrantDefinitionClass = UInventoryOtherTestItemDefinition::StaticClass();
	Listener->bAttemptReentrantInitialization = true;
	TestTrue(TEXT("Outer cutover succeeds"), Inventory->InitializeFromLegacyInventory().IsSuccess());
	TestTrue(TEXT("Nested initialization returns Busy"), Listener->ReentrantInitializationResult.Result == EInventoryOperationResult::Busy);
	TestTrue(TEXT("Nested native add returns Busy"), Listener->ReentrantResult.Result == EInventoryOperationResult::Busy);
	TestEqual(TEXT("Reentrant calls acquire no additional units"), Inventory->GetItemCount(), 1);
	TestEqual(TEXT("Reentrant calls do not increment revision"), Inventory->GetRevision(), 1);
	TestEqual(TEXT("Reentrant calls do not emit another event"), Listener->EventCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryMissingSeedSchemaTest,
	"NullTide.Inventory.M4.MissingSeedSchemaFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryMissingSeedSchemaTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryComponent> Inventory(NewObject<UInventoryComponent>(GetTransientPackage()));
	FindFProperty<FBoolProperty>(UInventoryComponent::StaticClass(), TEXT("bLegacyInventoryMode"))
		->SetPropertyValue_InContainer(Inventory.Get(), true);
	TestTrue(TEXT("Missing legacy array rejects import"), Inventory->InitializeFromLegacyInventory().Result == EInventoryOperationResult::NotInitialized);
	TestTrue(TEXT("Missing schema enters explicit failed state"), Inventory->GetInitializationState() == EInventoryInitializationState::Failed);
	TestEqual(TEXT("Missing schema imports no items"), Inventory->GetItemCount(), 0);
	TestEqual(TEXT("Missing schema commits no revision"), Inventory->GetRevision(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryBlueprintNativeAddItemTest,
	"NullTide.Inventory.M4.BlueprintLegacyAddItemRoutesNative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventoryBlueprintNativeAddItemTest::RunTest(const FString& Parameters)
{
	UClass* ManagerClass = LoadClass<UInventoryComponent>(nullptr,
		TEXT("/Game/LevelPrototyping/InventorySystem/InventoryManagerComponent.InventoryManagerComponent_C"));
	if (!TestNotNull(TEXT("Production manager loads"), ManagerClass)) { return false; }
	const TStrongObjectPtr<UInventoryComponent> Inventory(NewObject<UInventoryComponent>(GetTransientPackage(), ManagerClass));
	if (!TestTrue(TEXT("Actual Blueprint seed initializes"), Inventory->InitializeFromLegacyInventory().IsSuccess())) { return false; }
	UFunction* AddItem = ManagerClass->FindFunctionByName(TEXT("AddItem"));
	if (!TestNotNull(TEXT("Original AddItem exists"), AddItem)) { return false; }
	FClassProperty* Parameter = FindFProperty<FClassProperty>(AddItem, TEXT("ItemDefinitions"));
	if (!TestNotNull(TEXT("Original class parameter exists"), Parameter)) { return false; }
	for (const TCHAR* Path : { TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Wood.Item_Wood_C"),
		TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_ShortSword.Item_ShortSword_C") })
	{
		UClass* Definition = LoadClass<UItemDefinition>(nullptr, Path);
		if (!TestNotNull(TEXT("Production definition loads"), Definition)) { return false; }
		FStructOnScope Arguments(AddItem);
		Parameter->SetObjectPropertyValue_InContainer(Arguments.GetStructMemory(), Definition);
		Inventory->ProcessEvent(AddItem, Arguments.GetStructMemory());
	}
	const auto Items = Inventory->GetItemsSnapshot();
	if (!TestEqual(TEXT("Old Blueprint AddItem now awards two native units"), Items.Num(), 2)) { return false; }
	TestTrue(TEXT("Old API produces distinct native identities"), Items[0]->GetInstanceId() != Items[1]->GetInstanceId());
	TestEqual(TEXT("Old API preserves Wood then Sword order"), Items[0]->GetDefinitionClass()->GetName(), FString(TEXT("Item_Wood_C")));
	TestEqual(TEXT("Old API preserves Short Sword second"), Items[1]->GetDefinitionClass()->GetName(), FString(TEXT("Item_ShortSword_C")));
	TestEqual(TEXT("Empty cutover plus two old-API calls commit three revisions"), Inventory->GetRevision(), 3);
	EInventoryOperationResult QueryResult;
	const auto Projection = ULegacyInventoryCompatibilityLibrary::GetLegacyItemsSnapshot(Inventory.Get(), QueryResult);
	TestTrue(TEXT("Old readers have two native-derived entries"), QueryResult == EInventoryOperationResult::Success
		&& Projection.Num() == 2 && Projection[0] == Items[0]->GetDefinitionClass() && Projection[1] == Items[1]->GetDefinitionClass());
	return true;
}
#endif
