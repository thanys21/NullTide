#include "ResourceNodeTestTypes.h"

#include "Inventory/InventoryComponent.h"
#include "InventoryComponentTestTypes.h"
#include "Items/ItemInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
struct FResourceNodeTestFixture
{
	TStrongObjectPtr<AActor> Interactor;
	TStrongObjectPtr<UInventoryComponent> Inventory;
	TStrongObjectPtr<AResourceNodeTestActor> Node;

	FResourceNodeTestFixture(TSubclassOf<UItemDefinition> OutputDefinition, int32 MaxYield)
		: Interactor(NewObject<AActor>(GetTransientPackage()))
		, Inventory(NewObject<UInventoryComponent>(Interactor.Get()))
		, Node(NewObject<AResourceNodeTestActor>(GetTransientPackage()))
	{
		Interactor->AddInstanceComponent(Inventory.Get());
		Node->Configure(OutputDefinition, MaxYield);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FResourceNodeBeginAndCompleteTest,
	"NullTide.ResourceGathering.RG1.BeginAndComplete",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FResourceNodeBeginAndCompleteTest::RunTest(const FString& Parameters)
{
	FResourceNodeTestFixture Fixture(UInventoryTestItemDefinition::StaticClass(), 2);

	TestTrue(TEXT("Valid interaction begins gathering"), Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	TestTrue(TEXT("Node enters Gathering state"), Fixture.Node->IsGathering());
	TestEqual(TEXT("Beginning gathers no item immediately"), Fixture.Inventory->GetItemCount(), 0);
	TestEqual(TEXT("Beginning preserves the yield"), Fixture.Node->GetRemainingYield(), 2);
	TestFalse(TEXT("Repeated begin does not create a second action"), Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get()));

	Fixture.Node->CompleteForTest();
	const TArray<UItemInstance*> Snapshot = Fixture.Inventory->GetItemsSnapshot();
	TestEqual(TEXT("Completion awards one item"), Snapshot.Num(), 1);
	TestEqual(TEXT("Completion awards the configured definition"), Snapshot[0]->GetDefinitionClass().Get(), UInventoryTestItemDefinition::StaticClass());
	TestEqual(TEXT("Completion consumes exactly one yield"), Fixture.Node->GetRemainingYield(), 1);
	TestFalse(TEXT("Successful completion clears gathering"), Fixture.Node->IsGathering());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FResourceNodeDepletionTest,
	"NullTide.ResourceGathering.RG1.Depletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FResourceNodeDepletionTest::RunTest(const FString& Parameters)
{
	FResourceNodeTestFixture Fixture(UInventoryTestItemDefinition::StaticClass(), 2);
	Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get());
	Fixture.Node->CompleteForTest();
	Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get());
	Fixture.Node->CompleteForTest();

	TestEqual(TEXT("Repeated successful gathers consume all yield"), Fixture.Node->GetRemainingYield(), 0);
	TestEqual(TEXT("Each successful completion awards exactly one item"), Fixture.Inventory->GetItemCount(), 2);
	TestTrue(TEXT("Zero yield enters Depleted state"), Fixture.Node->IsDepleted());
	TestFalse(TEXT("Depleted nodes reject gathering"), Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	TestEqual(TEXT("Yield never goes negative"), Fixture.Node->GetRemainingYield(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FResourceNodeCancellationTest,
	"NullTide.ResourceGathering.RG1.Cancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FResourceNodeCancellationTest::RunTest(const FString& Parameters)
{
	FResourceNodeTestFixture Fixture(UInventoryTestItemDefinition::StaticClass(), 3);
	Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get());
	Fixture.Node->CancelGather();
	TestFalse(TEXT("Direct cancel clears gathering"), Fixture.Node->IsGathering());
	TestEqual(TEXT("Direct cancel awards nothing"), Fixture.Inventory->GetItemCount(), 0);
	TestEqual(TEXT("Direct cancel preserves yield"), Fixture.Node->GetRemainingYield(), 3);

	Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get());
	TestTrue(TEXT("Movement cancellation route finds the active node"), AResourceNodeActor::CancelActiveGatherForInteractor(Fixture.Interactor.Get()));
	TestFalse(TEXT("Movement cancellation clears gathering"), Fixture.Node->IsGathering());
	TestEqual(TEXT("Movement cancellation preserves yield"), Fixture.Node->GetRemainingYield(), 3);

	Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get());
	TestTrue(TEXT("Jump cancellation route finds the active node"), AResourceNodeActor::CancelActiveGatherForInteractor(Fixture.Interactor.Get()));
	TestFalse(TEXT("Jump cancellation clears gathering"), Fixture.Node->IsGathering());
	TestEqual(TEXT("Jump cancellation awards nothing"), Fixture.Inventory->GetItemCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FResourceNodeCompletionFailureTest,
	"NullTide.ResourceGathering.RG1.CompletionFailureIsAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FResourceNodeCompletionFailureTest::RunTest(const FString& Parameters)
{
	FResourceNodeTestFixture Fixture(UInventoryTestItemDefinition::StaticClass(), 1);
	for (int32 Index = 0; Index < Fixture.Inventory->GetCapacity(); ++Index)
	{
		if (!Fixture.Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass()).IsSuccess())
		{
			AddError(TEXT("Failed to prepare a full inventory"));
			return false;
		}
	}

	const int32 RevisionBefore = Fixture.Inventory->GetRevision();
	TestTrue(TEXT("Gather can begin before a completion-time capacity check"), Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	Fixture.Node->CompleteForTest();
	TestEqual(TEXT("Full inventory rejects the award"), Fixture.Inventory->GetItemCount(), Fixture.Inventory->GetCapacity());
	TestEqual(TEXT("Failed award does not consume yield"), Fixture.Node->GetRemainingYield(), 1);
	TestEqual(TEXT("Failed award does not change inventory revision"), Fixture.Inventory->GetRevision(), RevisionBefore);
	TestFalse(TEXT("Failed completion still clears gathering"), Fixture.Node->IsGathering());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FResourceNodeInvalidInputTest,
	"NullTide.ResourceGathering.RG1.InvalidInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FResourceNodeInvalidInputTest::RunTest(const FString& Parameters)
{
	FResourceNodeTestFixture ValidFixture(UInventoryTestItemDefinition::StaticClass(), 1);
	TestFalse(TEXT("Null interactor is rejected safely"), ValidFixture.Node->BeginGatherFromInteractor(nullptr));

	const TStrongObjectPtr<AActor> ActorWithoutInventory(NewObject<AActor>(GetTransientPackage()));
	TestFalse(TEXT("Interactor without an inventory is rejected safely"), ValidFixture.Node->BeginGatherFromInteractor(ActorWithoutInventory.Get()));

	FResourceNodeTestFixture InvalidDefinitionFixture(nullptr, 1);
	TestFalse(TEXT("Invalid output definition is rejected safely"), InvalidDefinitionFixture.Node->BeginGatherFromInteractor(InvalidDefinitionFixture.Interactor.Get()));
	TestEqual(TEXT("Invalid definition leaves yield unchanged"), InvalidDefinitionFixture.Node->GetRemainingYield(), 1);
	TestEqual(TEXT("Invalid definition awards nothing"), InvalidDefinitionFixture.Inventory->GetItemCount(), 0);
	return true;
}

#endif
