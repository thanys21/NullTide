#include "ResourceNodeTestTypes.h"

#include "Inventory/InventoryComponent.h"
#include "InventoryComponentTestTypes.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FResourceInventoryFixture
{
	TStrongObjectPtr<AActor> Interactor;
	TStrongObjectPtr<UInventoryComponent> Inventory;
	TStrongObjectPtr<AResourceNodeTestActor> Node;

	FResourceInventoryFixture(TSubclassOf<UItemDefinition> OutputDefinition, const int32 MaxYield)
		: Interactor(NewObject<AActor>(GetTransientPackage()))
		, Inventory(NewObject<UInventoryComponent>(Interactor.Get()))
		, Node(NewObject<AResourceNodeTestActor>(GetTransientPackage()))
	{
		Interactor->AddInstanceComponent(Inventory.Get());
		Node->Configure(OutputDefinition, MaxYield);
	}
};

struct FProductionReward
{
	const TCHAR* Name;
	const TCHAR* ItemPath;
};

const FProductionReward ProductionRewards[] = {
	{ TEXT("Tree"), TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Wood.Item_Wood_C") },
	{ TEXT("Rock"), TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Stone.Item_Stone_C") },
	{ TEXT("Scrap"), TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Scrap.Item_Scrap_C") }
};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FRG5ConfiguredGatherRewardTest,
	"NullTide.ResourceGathering.RG5.ConfiguredRewards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FRG5ConfiguredGatherRewardTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (const FProductionReward& Reward : ProductionRewards)
	{
		Names.Add(Reward.Name);
		Commands.Add(Reward.Name);
	}
}

bool FRG5ConfiguredGatherRewardTest::RunTest(const FString& Parameters)
{
	const FProductionReward* Expected = nullptr;
	for (const FProductionReward& Reward : ProductionRewards)
	{
		if (Parameters == Reward.Name)
		{
			Expected = &Reward;
			break;
		}
	}
	if (!TestNotNull(TEXT("Known production reward"), Expected))
	{
		return false;
	}

	UClass* OutputDefinition = LoadClass<UItemDefinition>(nullptr, Expected->ItemPath);
	if (!TestNotNull(TEXT("Production output definition loads"), OutputDefinition))
	{
		return false;
	}

	FResourceInventoryFixture Fixture(OutputDefinition, 1);
	TestTrue(TEXT("Gather begins"), Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	Fixture.Node->CompleteForTest();

	const TArray<UItemInstance*> Snapshot = Fixture.Inventory->GetItemsSnapshot();
	TestEqual(TEXT("Successful gather adds exactly one inventory item"), Snapshot.Num(), 1);
	TestTrue(TEXT("Gathered item uses the configured production definition"),
		Snapshot.Num() == 1 && Snapshot[0] && Snapshot[0]->GetDefinitionClass().Get() == OutputDefinition);
	TestEqual(TEXT("Successful gather consumes exactly one yield"), Fixture.Node->GetRemainingYield(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRG5FullInventoryRetryTest,
	"NullTide.ResourceGathering.RG5.FullInventoryRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRG5FullInventoryRetryTest::RunTest(const FString& Parameters)
{
	UClass* WoodDefinition = LoadClass<UItemDefinition>(nullptr,
		TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Wood.Item_Wood_C"));
	if (!TestNotNull(TEXT("Wood definition loads"), WoodDefinition))
	{
		return false;
	}

	FResourceInventoryFixture Fixture(WoodDefinition, 1);
	for (int32 Index = 0; Index < Fixture.Inventory->GetCapacity(); ++Index)
	{
		if (!Fixture.Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass()).IsSuccess())
		{
			AddError(TEXT("Failed to prepare full inventory"));
			return false;
		}
	}

	const int32 RevisionBeforeFailure = Fixture.Inventory->GetRevision();
	TestTrue(TEXT("Gather begins before completion-time capacity rejection"),
		Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	Fixture.Node->CompleteForTest();
	TestEqual(TEXT("Full inventory receives no reward"), Fixture.Inventory->GetItemCount(), Fixture.Inventory->GetCapacity());
	TestEqual(TEXT("Full inventory failure preserves revision"), Fixture.Inventory->GetRevision(), RevisionBeforeFailure);
	TestEqual(TEXT("Full inventory failure preserves yield"), Fixture.Node->GetRemainingYield(), 1);
	TestFalse(TEXT("Failed completion clears gathering"), Fixture.Node->IsGathering());
	TestFalse(TEXT("Failed award does not deplete the node"), Fixture.Node->IsDepleted());

	const TArray<UItemInstance*> FullSnapshot = Fixture.Inventory->GetItemsSnapshot();
	if (!TestTrue(TEXT("Full inventory has a removable item"), FullSnapshot.Num() == Fixture.Inventory->GetCapacity() && FullSnapshot[0]))
	{
		return false;
	}
	TestTrue(TEXT("Freeing one slot succeeds"), Fixture.Inventory->TryRemoveItem(FullSnapshot[0]->GetInstanceId()).IsSuccess());
	TestTrue(TEXT("Gather can retry after space is freed"), Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	Fixture.Node->CompleteForTest();

	const TArray<UItemInstance*> RetriedSnapshot = Fixture.Inventory->GetItemsSnapshot();
	int32 WoodCount = 0;
	for (const UItemInstance* Item : RetriedSnapshot)
	{
		if (Item && Item->GetDefinitionClass().Get() == WoodDefinition)
		{
			++WoodCount;
		}
	}
	TestEqual(TEXT("Retry adds exactly one reward"), WoodCount, 1);
	TestEqual(TEXT("Retry restores inventory to capacity"), Fixture.Inventory->GetItemCount(), Fixture.Inventory->GetCapacity());
	TestEqual(TEXT("Retry consumes exactly one yield"), Fixture.Node->GetRemainingYield(), 0);
	TestEqual(TEXT("Only the remove and successful retry change revision"), Fixture.Inventory->GetRevision(), RevisionBeforeFailure + 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRG5CancellationAndRepeatedInteractionTest,
	"NullTide.ResourceGathering.RG5.CancellationAndRepeatedInteraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRG5CancellationAndRepeatedInteractionTest::RunTest(const FString& Parameters)
{
	FResourceInventoryFixture Fixture(UInventoryTestItemDefinition::StaticClass(), 2);
	TestTrue(TEXT("First gather begins"), Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	TestFalse(TEXT("Repeated interaction cannot start a second gather"), Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	Fixture.Node->CancelGather();
	TestEqual(TEXT("Cancellation awards nothing"), Fixture.Inventory->GetItemCount(), 0);
	TestEqual(TEXT("Cancellation preserves yield"), Fixture.Node->GetRemainingYield(), 2);

	TestTrue(TEXT("Gather can begin after cancellation"), Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	Fixture.Node->CompleteForTest();
	TestEqual(TEXT("One successful completion awards one item"), Fixture.Inventory->GetItemCount(), 1);
	TestEqual(TEXT("Successful completion consumes one yield"), Fixture.Node->GetRemainingYield(), 1);
	return true;
}
#endif
