#include "UI/InventoryUICategory.h"

#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "Items/Fragments/ItemFragment_Ammo.h"
#include "Items/Fragments/ItemFragment_Consumable.h"
#include "Items/Fragments/ItemFragment_Drink.h"
#include "Items/Fragments/ItemFragment_Durability.h"
#include "Items/Fragments/ItemFragment_Equippable.h"
#include "Items/Fragments/ItemFragment_Food.h"
#include "Items/Fragments/ItemFragment_Healing.h"
#include "Items/Fragments/ItemFragment_Resource.h"
#include "Items/Fragments/ItemFragment_Weapon.h"

namespace
{
FString CompactNumber(const float Value)
{
	return FMath::IsNearlyEqual(Value, FMath::RoundToFloat(Value))
		? FString::FromInt(FMath::RoundToInt(Value))
		: FString::Printf(TEXT("%.1f"), Value);
}

FString EquipmentSlotName(const EEquipmentSlot Slot)
{
	switch (Slot)
	{
	case EEquipmentSlot::MainHand: return TEXT("Main Hand");
	case EEquipmentSlot::OffHand: return TEXT("Off Hand");
	default: return TEXT("Unassigned Slot");
	}
}

FString AmmoTypeName(const EAmmoType Type)
{
	return Type == EAmmoType::Arrow ? TEXT("Arrow") : TEXT("Unspecified");
}
}

const UItemDefinition* UInventoryUIFunctionLibrary::GetDefinition(const TSubclassOf<UItemDefinition> DefinitionClass)
{
	return DefinitionClass ? DefinitionClass->GetDefaultObject<UItemDefinition>() : nullptr;
}

EInventoryCategory UInventoryUIFunctionLibrary::ResolveCategory(const TSubclassOf<UItemDefinition> DefinitionClass)
{
	const UItemDefinition* Definition = GetDefinition(DefinitionClass);
	if (!Definition)
	{
		return EInventoryCategory::Misc;
	}
	if (Definition->HasFragmentByClass(UItemFragment_Weapon::StaticClass()))
	{
		return EInventoryCategory::Weapon;
	}
	if (Definition->HasFragmentByClass(UItemFragment_Ammo::StaticClass()))
	{
		return EInventoryCategory::Ammo;
	}
	if (Definition->HasFragmentByClass(UItemFragment_Resource::StaticClass()))
	{
		return EInventoryCategory::Resource;
	}
	if (Definition->HasFragmentByClass(UItemFragment_Consumable::StaticClass()))
	{
		return EInventoryCategory::Consumable;
	}
	return EInventoryCategory::Misc;
}

EInventoryCategory UInventoryUIFunctionLibrary::ResolveItemCategory(const UItemInstance* Item)
{
	return Item ? ResolveCategory(Item->GetDefinitionClass()) : EInventoryCategory::Misc;
}

FText UInventoryUIFunctionLibrary::GetCategoryDisplayName(const EInventoryCategory Category)
{
	switch (Category)
	{
	case EInventoryCategory::All: return FText::FromString(TEXT("All"));
	case EInventoryCategory::Resource: return FText::FromString(TEXT("Resource"));
	case EInventoryCategory::Consumable: return FText::FromString(TEXT("Consumable"));
	case EInventoryCategory::Weapon: return FText::FromString(TEXT("Weapon"));
	case EInventoryCategory::Ammo: return FText::FromString(TEXT("Ammo"));
	default: return FText::FromString(TEXT("Misc"));
	}
}

FText UInventoryUIFunctionLibrary::BuildCapabilitySummary(const TSubclassOf<UItemDefinition> DefinitionClass)
{
	const UItemDefinition* Definition = GetDefinition(DefinitionClass);
	if (!Definition)
	{
		return FText::GetEmpty();
	}

	TArray<FString> Lines;
	if (const auto* Weapon = Cast<UItemFragment_Weapon>(Definition->FindFragmentByClass(UItemFragment_Weapon::StaticClass())))
	{
		Lines.Add(FString::Printf(TEXT("Damage: %s"), *CompactNumber(Weapon->Damage)));
		Lines.Add(FString::Printf(TEXT("Attack Speed: %.1f"), Weapon->AttackSpeed));
		Lines.Add(FString::Printf(TEXT("Range: %s"), *CompactNumber(Weapon->AttackRange)));
	}
	if (const auto* Ammo = Cast<UItemFragment_Ammo>(Definition->FindFragmentByClass(UItemFragment_Ammo::StaticClass())))
	{
		Lines.Add(FString::Printf(TEXT("Type: %s"), *AmmoTypeName(Ammo->AmmoType)));
		Lines.Add(FString::Printf(TEXT("Damage Modifier: %.1f"), Ammo->DamageModifier));
	}
	if (const auto* Resource = Cast<UItemFragment_Resource>(Definition->FindFragmentByClass(UItemFragment_Resource::StaticClass())))
	{
		Lines.Add(FString::Printf(TEXT("Type: %s"), *Resource->ResourceType.ToString()));
	}
	if (const auto* Healing = Cast<UItemFragment_Healing>(Definition->FindFragmentByClass(UItemFragment_Healing::StaticClass())))
	{
		Lines.Add(FString::Printf(TEXT("Healing: %s"), *CompactNumber(Healing->HealAmount)));
	}
	if (const auto* Food = Cast<UItemFragment_Food>(Definition->FindFragmentByClass(UItemFragment_Food::StaticClass())))
	{
		Lines.Add(FString::Printf(TEXT("Hunger: %s"), *CompactNumber(Food->HungerRestore)));
	}
	if (const auto* Drink = Cast<UItemFragment_Drink>(Definition->FindFragmentByClass(UItemFragment_Drink::StaticClass())))
	{
		Lines.Add(FString::Printf(TEXT("Hydration: %s"), *CompactNumber(Drink->HydrationRestore)));
	}
	if (const auto* Durability = Cast<UItemFragment_Durability>(Definition->FindFragmentByClass(UItemFragment_Durability::StaticClass())))
	{
		Lines.Add(FString::Printf(TEXT("Durability: %s"), *CompactNumber(Durability->MaxDurability)));
	}
	if (const auto* Equippable = Cast<UItemFragment_Equippable>(Definition->FindFragmentByClass(UItemFragment_Equippable::StaticClass())))
	{
		Lines.Add(EquipmentSlotName(Equippable->EquipmentSlot));
	}

	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

bool UInventoryUIFunctionLibrary::IsCurrentItem(const UInventoryComponent* Inventory, const UItemInstance* Item)
{
	if (!Inventory || !Item)
	{
		return false;
	}
	const FGuid ItemId = Item->GetInstanceId();
	return ItemId.IsValid() && Inventory->ContainsItem(ItemId) && Inventory->FindItemById(ItemId) == Item;
}

bool UInventoryUIFunctionLibrary::IsItemVisible(const UInventoryComponent* Inventory, const UItemInstance* Item, const EInventoryCategory Category)
{
	return IsCurrentItem(Inventory, Item)
		&& (Category == EInventoryCategory::All || ResolveItemCategory(Item) == Category);
}

bool UInventoryUIFunctionLibrary::IsSelectedItemValid(const UInventoryComponent* Inventory, const FGuid SelectedItemId, const EInventoryCategory Category)
{
	if (!Inventory || !SelectedItemId.IsValid())
	{
		return false;
	}
	return IsItemVisible(Inventory, Inventory->FindItemById(SelectedItemId), Category);
}

bool UInventoryUIFunctionLibrary::IsDragPayloadValid(const UInventoryComponent* Inventory, const FGuid ItemId, const UItemInstance* Item)
{
	return Item && ItemId.IsValid() && Item->GetInstanceId() == ItemId && IsCurrentItem(Inventory, Item);
}
