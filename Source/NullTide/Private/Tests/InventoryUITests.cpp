#include "Items/ItemDefinition.h"
#include "Items/Fragments/ItemFragment_Consumable.h"
#include "Items/Fragments/ItemFragment_Weapon.h"
#include "Inventory/InventoryComponent.h"
#include "InventoryUITestTypes.h"
#include "Misc/AutomationTest.h"
#include "UI/InventoryUICategory.h"
#include "UObject/StrongObjectPtr.h"

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
	{ TEXT("Meat"), EInventoryCategory::Consumable, TEXT("Hunger: 20") },
	{ TEXT("LongSword"), EInventoryCategory::Weapon, TEXT("Durability: 150") },
	{ TEXT("WoodBow"), EInventoryCategory::Weapon, TEXT("Range: 800") },
	{ TEXT("WoodArrow"), EInventoryCategory::Ammo, TEXT("Type: Arrow") },
	{ TEXT("WaterBottle"), EInventoryCategory::Consumable, TEXT("Hydration: 30") }
};
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

	const FString ClassPath = FString::Printf(TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_%s.Item_%s_C"), TestCase->Id, TestCase->Id);
	UClass* DefinitionClass = LoadClass<UItemDefinition>(nullptr, *ClassPath);
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

	UClass* WoodClass = LoadClass<UItemDefinition>(nullptr,
		TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Wood.Item_Wood_C"));
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
#endif
