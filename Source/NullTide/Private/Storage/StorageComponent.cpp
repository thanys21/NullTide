#include "Storage/StorageComponent.h"

#include "Inventory/InventoryComponent.h"
#include "Items/ItemInstance.h"
#include "Storage/StorageTransferLibrary.h"

UStorageComponent::UStorageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FInventoryStorageTransferResult UStorageComponent::TryTransferItemToInventory(
	UInventoryComponent* Inventory, const FGuid ItemId)
{
	return UStorageTransferLibrary::TransferStorageToInventory(this, Inventory, ItemId);
}

TArray<UItemInstance*> UStorageComponent::GetItemsSnapshot() const
{
	TArray<UItemInstance*> Snapshot;
	Snapshot.Reserve(Items.Num());
	for (UItemInstance* Item : Items)
	{
		Snapshot.Add(Item);
	}
	return Snapshot;
}

UItemInstance* UStorageComponent::FindItemById(const FGuid ItemId) const
{
	if (!ItemId.IsValid())
	{
		return nullptr;
	}
	const TObjectPtr<UItemInstance>* FoundItem = Items.FindByPredicate(
		[&ItemId](const TObjectPtr<UItemInstance>& Item)
		{
			return IsValid(Item) && Item->GetInstanceId() == ItemId;
		});
	return FoundItem ? FoundItem->Get() : nullptr;
}

bool UStorageComponent::ContainsItem(const FGuid ItemId) const
{
	return FindItemById(ItemId) != nullptr;
}

bool UStorageComponent::CanOperate() const
{
	return !HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed | RF_FinishDestroyed)
		&& Capacity > 0;
}
