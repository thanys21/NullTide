#include "ResourceNodeTestTypes.h"

#include "Inventory/InventoryComponent.h"
#include "InventoryComponentTestTypes.h"
#include "Items/ItemDefinition.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FResourceYieldFixture
{
	TStrongObjectPtr<AActor> Interactor;
	TStrongObjectPtr<UInventoryComponent> Inventory;
	TStrongObjectPtr<AResourceNodeTestActor> Node;

	FResourceYieldFixture(int32 MaxYield, EToolType RequiredToolType = EToolType::None)
		: Interactor(NewObject<AActor>(GetTransientPackage()))
		, Inventory(NewObject<UInventoryComponent>(Interactor.Get()))
		, Node(NewObject<AResourceNodeTestActor>(GetTransientPackage()))
	{
		Interactor->AddInstanceComponent(Inventory.Get());
		Node->Configure(UInventoryTestItemDefinition::StaticClass(), MaxYield, RequiredToolType);
	}
};

struct FProductionNodeDefaults
{
	const TCHAR* Name;
	const TCHAR* OutputItem;
	int32 MaxYield;
	float GatherDuration;
	EToolType RequiredTool;
};

const FProductionNodeDefaults ProductionNodes[] = {
	{ TEXT("Tree"), TEXT("Wood"), 5, 2.0f, EToolType::Axe },
	{ TEXT("Rock"), TEXT("Stone"), 5, 2.5f, EToolType::Pickaxe },
	{ TEXT("Scrap"), TEXT("Scrap"), 4, 1.5f, EToolType::None }
};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FRG4ProductionNodeDefaultsTest,
	"NullTide.ResourceGathering.RG4.ProductionNodeDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FRG4ProductionNodeDefaultsTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (const FProductionNodeDefaults& Node : ProductionNodes)
	{
		Names.Add(Node.Name);
		Commands.Add(Node.Name);
	}
}

bool FRG4ProductionNodeDefaultsTest::RunTest(const FString& Parameters)
{
	const FProductionNodeDefaults* Expected = nullptr;
	for (const FProductionNodeDefaults& Node : ProductionNodes)
	{
		if (Parameters == Node.Name)
		{
			Expected = &Node;
			break;
		}
	}

	if (!TestNotNull(TEXT("Known production resource node"), Expected))
	{
		return false;
	}

	const FString NodePath = FString::Printf(
		TEXT("/Game/LevelPrototyping/ResourceGathering/BP_Resource%s.BP_Resource%s_C"), Expected->Name, Expected->Name);
	const FString DefinitionPath = FString::Printf(
		TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_%s.Item_%s_C"), Expected->OutputItem, Expected->OutputItem);
	UClass* NodeClass = LoadClass<AResourceNodeActor>(nullptr, *NodePath);
	UClass* OutputDefinition = LoadClass<UItemDefinition>(nullptr, *DefinitionPath);
	if (!TestTrue(TEXT("Production node and output definition load"), NodeClass && OutputDefinition))
	{
		return false;
	}

	const AResourceNodeActor* Node = NodeClass->GetDefaultObject<AResourceNodeActor>();
	TestEqual(TEXT("Output definition"), Node->GetOutputItemDefinition().Get(), OutputDefinition);
	TestEqual(TEXT("Max yield"), Node->GetMaxYield(), Expected->MaxYield);
	TestEqual(TEXT("Gather duration"), Node->GetGatherDuration(), Expected->GatherDuration);
	TestEqual(TEXT("Required tool"), Node->GetRequiredToolType(), Expected->RequiredTool);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRG4YieldAndDepletionTest,
	"NullTide.ResourceGathering.RG4.YieldAndDepletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRG4YieldAndDepletionTest::RunTest(const FString& Parameters)
{
	FResourceYieldFixture Fixture(2);
	TestEqual(TEXT("Remaining yield initializes from MaxYield"), Fixture.Node->GetRemainingYield(), 2);
	TestTrue(TEXT("Node starts idle when yield remains"), Fixture.Node->GetGatherState() == EResourceNodeState::Idle);

	TestTrue(TEXT("First gather begins"), Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	TestFalse(TEXT("Repeated interaction cannot start a second gather"), Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	Fixture.Node->CompleteForTest();
	TestEqual(TEXT("First successful award consumes one yield"), Fixture.Node->GetRemainingYield(), 1);
	TestEqual(TEXT("First successful award adds one item"), Fixture.Inventory->GetItemCount(), 1);

	TestTrue(TEXT("Final gather begins"), Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	Fixture.Node->CompleteForTest();
	TestEqual(TEXT("Final successful award consumes final yield"), Fixture.Node->GetRemainingYield(), 0);
	TestEqual(TEXT("Each successful gather awards one item"), Fixture.Inventory->GetItemCount(), 2);
	TestTrue(TEXT("Zero yield transitions to Depleted"), Fixture.Node->IsDepleted());
	TestFalse(TEXT("Depleted node rejects a later gather"), Fixture.Node->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	TestEqual(TEXT("Depleted node never goes negative"), Fixture.Node->GetRemainingYield(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRG4RejectedGatherPreservesYieldTest,
	"NullTide.ResourceGathering.RG4.RejectionsPreserveYield",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRG4RejectedGatherPreservesYieldTest::RunTest(const FString& Parameters)
{
	FResourceYieldFixture Cancelled(3);
	TestTrue(TEXT("Gather can begin before cancellation"), Cancelled.Node->BeginGatherFromInteractor(Cancelled.Interactor.Get()));
	Cancelled.Node->CancelGather();
	TestEqual(TEXT("Cancellation consumes zero yield"), Cancelled.Node->GetRemainingYield(), 3);
	TestEqual(TEXT("Cancellation awards no item"), Cancelled.Inventory->GetItemCount(), 0);

	FResourceYieldFixture FullInventory(1);
	for (int32 Index = 0; Index < FullInventory.Inventory->GetCapacity(); ++Index)
	{
		if (!FullInventory.Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass()).IsSuccess())
		{
			AddError(TEXT("Failed to prepare a full inventory"));
			return false;
		}
	}
	TestTrue(TEXT("Gather can begin before the inventory rejects its award"), FullInventory.Node->BeginGatherFromInteractor(FullInventory.Interactor.Get()));
	FullInventory.Node->CompleteForTest();
	TestEqual(TEXT("Inventory rejection consumes zero yield"), FullInventory.Node->GetRemainingYield(), 1);

	FResourceYieldFixture MissingTool(5, EToolType::Axe);
	TestFalse(TEXT("Missing required tool rejects before gathering"), MissingTool.Node->BeginGatherFromInteractor(MissingTool.Interactor.Get()));
	TestFalse(TEXT("Missing required tool starts no timer state"), MissingTool.Node->IsGathering());
	TestEqual(TEXT("Missing required tool consumes zero yield"), MissingTool.Node->GetRemainingYield(), 5);
	TestTrue(TEXT("Active resource node keeps its interaction prompt"), AResourceNodeActor::ShouldShowInteractionPromptFor(MissingTool.Node.Get()));

	FResourceYieldFixture Depleted(0);
	TestTrue(TEXT("Zero-yield node initializes Depleted"), Depleted.Node->IsDepleted());
	TestFalse(TEXT("Depleted resource node suppresses its interaction prompt"), AResourceNodeActor::ShouldShowInteractionPromptFor(Depleted.Node.Get()));
	TestTrue(TEXT("Non-resource interactable keeps its interaction prompt"), AResourceNodeActor::ShouldShowInteractionPromptFor(Depleted.Interactor.Get()));
	return true;
}
#endif
