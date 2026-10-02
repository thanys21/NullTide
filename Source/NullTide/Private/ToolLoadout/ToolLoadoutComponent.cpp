#include "ToolLoadout/ToolLoadoutComponent.h"

#include "Inventory/InventoryComponent.h"
#include "Items/Fragments/ItemFragment_Tool.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"

UToolLoadoutComponent::UToolLoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UToolLoadoutComponent::EquipTool(const EToolType ToolType, const FGuid ItemId)
{
	UInventoryComponent* Inventory = ResolveInventory();
	UItemInstance* Item = Inventory && ItemId.IsValid() ? Inventory->FindItemById(ItemId) : nullptr;
	if (ToolType == EToolType::None || !IsCompatibleTool(Item, ToolType))
	{
		return false;
	}

	EquippedItemIds.Add(ToolType, ItemId);
	return true;
}

bool UToolLoadoutComponent::UnequipTool(const EToolType ToolType)
{
	return ToolType != EToolType::None && EquippedItemIds.Remove(ToolType) > 0;
}

UItemInstance* UToolLoadoutComponent::GetEquippedTool(const EToolType ToolType)
{
	const FGuid* ItemId = EquippedItemIds.Find(ToolType);
	UInventoryComponent* Inventory = ResolveInventory();
	UItemInstance* Item = ItemId && Inventory ? Inventory->FindItemById(*ItemId) : nullptr;
	if (ToolType == EToolType::None || !IsCompatibleTool(Item, ToolType))
	{
		EquippedItemIds.Remove(ToolType);
		return nullptr;
	}

	return Item;
}

bool UToolLoadoutComponent::HasEquippedTool(const EToolType ToolType)
{
	return GetEquippedTool(ToolType) != nullptr;
}

UInventoryComponent* UToolLoadoutComponent::ResolveInventory() const
{
	AActor* Owner = GetOwner();
	return IsValid(Owner) ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
}

bool UToolLoadoutComponent::IsCompatibleTool(const UItemInstance* Item, const EToolType ToolType) const
{
	if (!IsValid(Item) || ToolType == EToolType::None)
	{
		return false;
	}

	const TSubclassOf<UItemDefinition> DefinitionClass = Item->GetDefinitionClass();
	const UItemDefinition* Definition = DefinitionClass ? DefinitionClass->GetDefaultObject<UItemDefinition>() : nullptr;
	const UItemFragment_Tool* Tool = Definition
		? Cast<UItemFragment_Tool>(Definition->FindFragmentByClass(UItemFragment_Tool::StaticClass()))
		: nullptr;
	return Tool && Tool->ToolType == ToolType;
}
