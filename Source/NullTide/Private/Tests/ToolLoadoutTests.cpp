#include "ResourceNodeTestTypes.h"

#include "Inventory/InventoryComponent.h"
#include "InventoryComponentTestTypes.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "Misc/AutomationTest.h"
#include "ToolLoadout/ToolLoadoutComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
UClass* LoadProductionDefinition(const TCHAR* Id)
{
	const FString Path = FString::Printf(
		TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_%s.Item_%s_C"), Id, Id);
	return LoadClass<UItemDefinition>(nullptr, *Path);
}

struct FToolLoadoutTestFixture
{
	TStrongObjectPtr<AActor> Interactor;
	TStrongObjectPtr<UInventoryComponent> Inventory;
	TStrongObjectPtr<UToolLoadoutComponent> Loadout;

	FToolLoadoutTestFixture()
		: Interactor(NewObject<AActor>(GetTransientPackage()))
		, Inventory(NewObject<UInventoryComponent>(Interactor.Get()))
		, Loadout(NewObject<UToolLoadoutComponent>(Interactor.Get()))
	{
		Interactor->AddInstanceComponent(Inventory.Get());
		Interactor->AddInstanceComponent(Loadout.Get());
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FToolLoadoutIdentityAndStaleSlotTest,
	"NullTide.ResourceGathering.RG3.ToolLoadout.IdentityAndStaleSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FToolLoadoutIdentityAndStaleSlotTest::RunTest(const FString& Parameters)
{
	FToolLoadoutTestFixture Fixture;
	UClass* AxeClass = LoadProductionDefinition(TEXT("Axe"));
	UClass* PickaxeClass = LoadProductionDefinition(TEXT("Pickaxe"));
	if (!TestTrue(TEXT("Production tool definitions load"), AxeClass && PickaxeClass))
	{
		return false;
	}

	const FInventoryOperationResult Axe = Fixture.Inventory->TryAddDefinition(AxeClass);
	const FInventoryOperationResult Pickaxe = Fixture.Inventory->TryAddDefinition(PickaxeClass);
	if (!TestTrue(TEXT("Tool instances are acquired"), Axe.IsSuccess() && Pickaxe.IsSuccess() && Axe.Item && Pickaxe.Item))
	{
		return false;
	}

	TestTrue(TEXT("Axe assigns to the Axe slot"), Fixture.Loadout->EquipTool(EToolType::Axe, Axe.ItemId));
	TestFalse(TEXT("Axe cannot assign to the Pickaxe slot"), Fixture.Loadout->EquipTool(EToolType::Pickaxe, Axe.ItemId));
	TestTrue(TEXT("Pickaxe assigns to the Pickaxe slot"), Fixture.Loadout->EquipTool(EToolType::Pickaxe, Pickaxe.ItemId));
	TestEqual(TEXT("Axe slot retains exact instance identity"), Fixture.Loadout->GetEquippedTool(EToolType::Axe), Axe.Item.Get());
	TestEqual(TEXT("Pickaxe slot retains exact instance identity"), Fixture.Loadout->GetEquippedTool(EToolType::Pickaxe), Pickaxe.Item.Get());
	TestEqual(TEXT("Exact Axe ItemId is retained"), Fixture.Loadout->GetEquippedTool(EToolType::Axe)->GetInstanceId(), Axe.ItemId);

	TestTrue(TEXT("Equipped Axe can be removed through inventory authority"), Fixture.Inventory->TryRemoveItem(Axe.ItemId).IsSuccess());
	TestNull(TEXT("Removed equipped item no longer resolves"), Fixture.Loadout->GetEquippedTool(EToolType::Axe));
	TestFalse(TEXT("Removed equipped item clears the stale Axe slot"), Fixture.Loadout->HasEquippedTool(EToolType::Axe));
	TestTrue(TEXT("Other tool slot remains valid"), Fixture.Loadout->HasEquippedTool(EToolType::Pickaxe));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FResourceNodeToolRequirementTest,
	"NullTide.ResourceGathering.RG3.ToolRequirements",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FResourceNodeToolRequirementTest::RunTest(const FString& Parameters)
{
	UClass* AxeClass = LoadProductionDefinition(TEXT("Axe"));
	UClass* PickaxeClass = LoadProductionDefinition(TEXT("Pickaxe"));
	if (!TestTrue(TEXT("Production tool definitions load"), AxeClass && PickaxeClass))
	{
		return false;
	}

	FToolLoadoutTestFixture Fixture;
	const TStrongObjectPtr<AResourceNodeTestActor> Tree(NewObject<AResourceNodeTestActor>(GetTransientPackage()));
	const TStrongObjectPtr<AResourceNodeTestActor> Rock(NewObject<AResourceNodeTestActor>(GetTransientPackage()));
	const TStrongObjectPtr<AResourceNodeTestActor> Scrap(NewObject<AResourceNodeTestActor>(GetTransientPackage()));
	Tree->Configure(UInventoryTestItemDefinition::StaticClass(), 1, EToolType::Axe);
	Rock->Configure(UInventoryTestItemDefinition::StaticClass(), 1, EToolType::Pickaxe);
	Scrap->Configure(UInventoryTestItemDefinition::StaticClass(), 1, EToolType::None);

	TestFalse(TEXT("Tree rejects an interactor with no Axe"), Tree->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	TestFalse(TEXT("Rock rejects an interactor with no Pickaxe"), Rock->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	TestEqual(TEXT("Missing tools preserve Tree yield"), Tree->GetRemainingYield(), 1);
	TestEqual(TEXT("Missing tools preserve Rock yield"), Rock->GetRemainingYield(), 1);
	TestFalse(TEXT("Missing tools do not begin Tree gathering"), Tree->IsGathering());
	TestFalse(TEXT("Missing tools do not begin Rock gathering"), Rock->IsGathering());
	TestTrue(TEXT("Scrap remains gatherable without a tool"), Scrap->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	Scrap->CancelGather();

	const FInventoryOperationResult Axe = Fixture.Inventory->TryAddDefinition(AxeClass);
	const FInventoryOperationResult Pickaxe = Fixture.Inventory->TryAddDefinition(PickaxeClass);
	if (!TestTrue(TEXT("Required tools are acquired"), Axe.IsSuccess() && Pickaxe.IsSuccess()))
	{
		return false;
	}
	TestTrue(TEXT("Exact Axe assignment succeeds"), Fixture.Loadout->EquipTool(EToolType::Axe, Axe.ItemId));
	TestTrue(TEXT("Tree begins with an equipped Axe"), Tree->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	Tree->CancelGather();
	TestFalse(TEXT("Axe alone cannot satisfy Rock's Pickaxe requirement"), Rock->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	TestTrue(TEXT("Exact Pickaxe assignment succeeds"), Fixture.Loadout->EquipTool(EToolType::Pickaxe, Pickaxe.ItemId));
	TestTrue(TEXT("Rock begins with an equipped Pickaxe"), Rock->BeginGatherFromInteractor(Fixture.Interactor.Get()));
	Rock->CancelGather();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRG3ProductionToolIntegrationTest,
	"NullTide.ResourceGathering.RG3.ProductionToolIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRG3ProductionToolIntegrationTest::RunTest(const FString& Parameters)
{
	UBlueprint* PlayerBlueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/TopDown/Blueprints/BP_TopDownCharacter.BP_TopDownCharacter"));
	if (!TestNotNull(TEXT("Production player Blueprint loads"), PlayerBlueprint))
	{
		return false;
	}

	int32 ToolLoadoutCount = 0;
	if (const USimpleConstructionScript* ConstructionScript = PlayerBlueprint->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : ConstructionScript->GetAllNodes())
		{
			ToolLoadoutCount += Node->ComponentClass && Node->ComponentClass->IsChildOf<UToolLoadoutComponent>() ? 1 : 0;
		}
	}
	TestEqual(TEXT("Production player has exactly one ToolLoadoutComponent"), ToolLoadoutCount, 1);

	auto TestRequirement = [this](const TCHAR* Name, const EToolType Expected)
	{
		const FString Path = FString::Printf(TEXT("/Game/LevelPrototyping/ResourceGathering/BP_Resource%s.BP_Resource%s_C"), Name, Name);
		UClass* NodeClass = LoadClass<AResourceNodeActor>(nullptr, *Path);
		if (!TestNotNull(FString::Printf(TEXT("Production %s Blueprint loads"), Name), NodeClass))
		{
			return;
		}
		TestEqual(FString::Printf(TEXT("Production %s requirement"), Name), NodeClass->GetDefaultObject<AResourceNodeActor>()->GetRequiredToolType(), Expected);
	};

	TestRequirement(TEXT("Tree"), EToolType::Axe);
	TestRequirement(TEXT("Rock"), EToolType::Pickaxe);
	TestRequirement(TEXT("Scrap"), EToolType::None);
	return true;
}
#endif
