#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "Inventory/InventoryComponent.h"
#include "Items/Fragments/ItemFragment_Ammo.h"
#include "Items/Fragments/ItemFragment_Consumable.h"
#include "Items/Fragments/ItemFragment_Drink.h"
#include "Items/Fragments/ItemFragment_Durability.h"
#include "Items/Fragments/ItemFragment_Equippable.h"
#include "Items/Fragments/ItemFragment_Food.h"
#include "Items/Fragments/ItemFragment_Healing.h"
#include "Items/Fragments/ItemFragment_Resource.h"
#include "Items/Fragments/ItemFragment_Tool.h"
#include "Items/Fragments/ItemFragment_Weapon.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FCatalogEntry
{
	const TCHAR* Id;
	const TCHAR* Name;
	const TCHAR* Description;
	int32 FragmentCount;
};

const FCatalogEntry Catalog[] = {
	{ TEXT("HealthPotion"), TEXT("Health Potion"), TEXT("Restores health."), 2 },
	{ TEXT("ShortSword"), TEXT("Short Sword"), TEXT("A light one-handed sword."), 3 },
	{ TEXT("Wood"), TEXT("Wood"), TEXT("A basic crafting resource."), 1 },
	{ TEXT("Sandwich"), TEXT("Sandwich"), TEXT("Simple prepared food."), 2 },
	{ TEXT("Stone"), TEXT("Stone"), TEXT("A basic crafting resource."), 1 },
	{ TEXT("Scrap"), TEXT("Scrap"), TEXT("A salvaged scrap material."), 1 },
	{ TEXT("Axe"), TEXT("Axe"), TEXT("A basic woodcutting tool."), 1 },
	{ TEXT("Pickaxe"), TEXT("Pickaxe"), TEXT("A basic rock-breaking tool."), 1 },
	{ TEXT("Meat"), TEXT("Meat"), TEXT("Raw food."), 2 },
	{ TEXT("LongSword"), TEXT("Long Sword"), TEXT("A longer melee weapon."), 3 },
	{ TEXT("WoodBow"), TEXT("Wood Bow"), TEXT("A simple wooden bow."), 3 },
	{ TEXT("WoodArrow"), TEXT("Wood Arrow"), TEXT("Basic ammunition for bows."), 1 },
	{ TEXT("WaterBottle"), TEXT("Water Bottle"), TEXT("A bottle of drinking water."), 2 }
};

// Snapshot reflected data, including every capability field, across acquisition.
FString TemplateState(const UObject* Object)
{
	FString Result;
	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		FString Value;
		It->ExportText_InContainer(0, Value, Object, Object, nullptr, PPF_None);
		Result += It->GetName() + TEXT("=") + Value + TEXT("\n");
	}
	return Result;
}
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FItemCatalogTest, "NullTide.Gameplay.G1.Catalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FItemCatalogTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (const FCatalogEntry& Entry : Catalog)
	{
		Names.Add(Entry.Id);
		Commands.Add(Entry.Id);
	}
}

bool FItemCatalogTest::RunTest(const FString& Parameters)
{
	const FCatalogEntry* Entry = nullptr;
	for (const FCatalogEntry& Candidate : Catalog)
	{
		if (Parameters == Candidate.Id) { Entry = &Candidate; break; }
	}
	if (!TestNotNull(TEXT("Known catalog entry"), Entry)) { return false; }
	const FString Path = FString::Printf(TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_%s.Item_%s_C"), Entry->Id, Entry->Id);
	UClass* Class = LoadClass<UItemDefinition>(nullptr, *Path);
	if (!TestNotNull(TEXT("Saved production definition loads"), Class)) { return false; }
	TestEqual(TEXT("Direct native parent; no item-specific native subclass"), Class->GetSuperClass(), UItemDefinition::StaticClass());
	const UItemDefinition* Definition = Class->GetDefaultObject<UItemDefinition>();
	TestEqual(TEXT("Display name"), Definition->ItemName.ToString(), FString(Entry->Name));
	TestEqual(TEXT("Description"), Definition->ItemDescription.ToString(), FString(Entry->Description));
	const FString IconPath = FString::Printf(TEXT("/Game/Items/Icons/T_Icon_%s.T_Icon_%s"), Entry->Id, Entry->Id);
	UTexture2D* Icon = LoadObject<UTexture2D>(nullptr, *IconPath);
	TestNotNull(TEXT("Existing icon texture loads"), Icon);
	TestEqual(TEXT("Exact catalog icon"), Definition->ItemIcon.Get(), Icon);
	FString Diagnostic;
	const bool bValid = Definition->ValidateFragments(Diagnostic);
	TestTrue(TEXT("Canonical fragments validate: ") + Diagnostic, bValid);
	TestTrue(TEXT("No legacy authoring"), Definition->Fragments.IsEmpty());
	const auto Templates = Definition->GetResolvedFragments();
	const int32 ExpectedTotalFragmentCount = Entry->FragmentCount + 1;
	TestEqual(TEXT("Exact G1 capabilities plus world presentation"), Templates.Num(), ExpectedTotalFragmentCount);
	TArray<UObject*> Children;
	GetObjectsWithOuter(Definition, Children, EGetObjectsFlags::None);
	int32 OwnedCount = 0;
	for (const UObject* Child : Children) { if (Child->IsA<UItemFragment>()) { ++OwnedCount; } }
	TestEqual(TEXT("No orphan or duplicate owned templates"), OwnedCount, ExpectedTotalFragmentCount);
	TArray<FString> Before;
	for (const UItemFragment* Fragment : Templates)
	{
		if (!TestNotNull(TEXT("No null fragment"), Fragment)) { return false; }
		TestEqual(TEXT("CDO directly owns template"), static_cast<const UObject*>(Fragment->GetOuter()), static_cast<const UObject*>(Definition));
		TestTrue(TEXT("Static inline capability"), Fragment->GetClass()->HasAllClassFlags(CLASS_Const | CLASS_DefaultToInstanced | CLASS_EditInlineNew));
		Before.Add(TemplateState(Fragment));
	}
	const bool bWeapon = Parameters == TEXT("ShortSword") || Parameters == TEXT("LongSword") || Parameters == TEXT("WoodBow");
	if (bWeapon)
	{
		const auto* Weapon = Cast<UItemFragment_Weapon>(Definition->FindFragmentByClass(UItemFragment_Weapon::StaticClass()));
		const auto* Equip = Cast<UItemFragment_Equippable>(Definition->FindFragmentByClass(UItemFragment_Equippable::StaticClass()));
		const auto* Durability = Cast<UItemFragment_Durability>(Definition->FindFragmentByClass(UItemFragment_Durability::StaticClass()));
		const bool bShort = Parameters == TEXT("ShortSword");
		const bool bBow = Parameters == TEXT("WoodBow");
		TestTrue(TEXT("Exact weapon configuration"), Weapon && Weapon->WeaponType == (bBow ? EWeaponType::Bow : EWeaponType::Sword)
			&& Weapon->Damage == (bShort ? 10.0f : bBow ? 8.0f : 18.0f)
			&& Weapon->AttackSpeed == (bShort ? 1.0f : 0.8f)
			&& Weapon->AttackRange == (bShort ? 150.0f : bBow ? 800.0f : 190.0f));
		TestTrue(TEXT("MainHand equipment"), Equip && Equip->EquipmentSlot == EEquipmentSlot::MainHand);
		TestTrue(TEXT("Exact maximum durability"), Durability && Durability->MaxDurability == (bShort ? 100.0f : bBow ? 80.0f : 150.0f));
	}
	else if (Parameters == TEXT("Wood") || Parameters == TEXT("Stone") || Parameters == TEXT("Scrap"))
	{
		const auto* Resource = Cast<UItemFragment_Resource>(Definition->FindFragmentByClass(UItemFragment_Resource::StaticClass()));
		TestTrue(TEXT("Resource type"), Resource && Resource->ResourceType == FName(Entry->Id));
	}
	else if (Parameters == TEXT("Axe") || Parameters == TEXT("Pickaxe"))
	{
		const auto* Tool = Cast<UItemFragment_Tool>(Definition->FindFragmentByClass(UItemFragment_Tool::StaticClass()));
		const EToolType ExpectedType = Parameters == TEXT("Axe") ? EToolType::Axe : EToolType::Pickaxe;
		TestTrue(TEXT("Exact passive tool configuration"), Tool && Tool->ToolType == ExpectedType && Tool->Efficiency == 1.0f);
		TestFalse(TEXT("Tool is not consumable"), Definition->HasFragmentByClass(UItemFragment_Consumable::StaticClass()));
		TestFalse(TEXT("Tool is not a weapon"), Definition->HasFragmentByClass(UItemFragment_Weapon::StaticClass()));
		TestFalse(TEXT("Tool has no durability yet"), Definition->HasFragmentByClass(UItemFragment_Durability::StaticClass()));
	}
	else if (Parameters == TEXT("WoodArrow"))
	{
		const auto* Ammo = Cast<UItemFragment_Ammo>(Definition->FindFragmentByClass(UItemFragment_Ammo::StaticClass()));
		TestTrue(TEXT("Arrow ammo configuration"), Ammo && Ammo->AmmoType == EAmmoType::Arrow && Ammo->DamageModifier == 1.0f);
	}
	else
	{
		TestTrue(TEXT("Consumable marker"), Definition->HasFragmentByClass(UItemFragment_Consumable::StaticClass()));
		if (Parameters == TEXT("HealthPotion"))
		{
			const auto* Healing = Cast<UItemFragment_Healing>(Definition->FindFragmentByClass(UItemFragment_Healing::StaticClass()));
			TestTrue(TEXT("Heal amount"), Healing && Healing->HealAmount == 50.0f);
		}
		else if (Parameters == TEXT("WaterBottle"))
		{
			const auto* Drink = Cast<UItemFragment_Drink>(Definition->FindFragmentByClass(UItemFragment_Drink::StaticClass()));
			TestTrue(TEXT("Hydration amount"), Drink && Drink->HydrationRestore == 30.0f);
		}
		else
		{
			const auto* Food = Cast<UItemFragment_Food>(Definition->FindFragmentByClass(UItemFragment_Food::StaticClass()));
			TestTrue(TEXT("Hunger amount"), Food && Food->HungerRestore == (Parameters == TEXT("Sandwich") ? 30.0f : 20.0f));
		}
	}
	const FString DefinitionBefore = TemplateState(Definition);
	const TStrongObjectPtr<UInventoryComponent> Inventory(NewObject<UInventoryComponent>());
	const auto First = Inventory->TryAddDefinition(Class);
	const auto Second = Inventory->TryAddDefinition(Class);
	if (!TestTrue(TEXT("Two independent acquired instances"), First.IsSuccess() && Second.IsSuccess()
		&& First.Item && Second.Item && First.ItemId.IsValid() && Second.ItemId.IsValid() && First.ItemId != Second.ItemId)) { return false; }
	TestEqual(TEXT("Instance owned by inventory"), First.Item->GetOuter(), static_cast<UObject*>(Inventory.Get()));
	TestEqual(TEXT("Instance resolves production definition"), First.Item->GetDefinitionClass().Get(), Class);
	TestEqual(TEXT("One revision per acquisition"), Inventory->GetRevision(), 2);
	TestTrue(TEXT("Template identities unchanged"), Templates == Definition->GetResolvedFragments());
	TestEqual(TEXT("Definition data unchanged"), TemplateState(Definition), DefinitionBefore);
	for (int32 Index = 0; Index < Templates.Num(); ++Index)
	{
		TestEqual(TEXT("All fragment data unchanged"), TemplateState(Templates[Index]), Before[Index]);
	}
	TestEqual(TEXT("Existing None enum value"), static_cast<uint8>(EWeaponType::None), uint8(0));
	TestEqual(TEXT("Existing Sword enum value"), static_cast<uint8>(EWeaponType::Sword), uint8(1));
	TestEqual(TEXT("Appended Bow enum value"), static_cast<uint8>(EWeaponType::Bow), uint8(2));
	return true;
}
#endif
