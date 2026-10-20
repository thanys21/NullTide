#include "Storage/StorageComponent.h"

#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryStacking.h"
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

FInventoryOperationResult UStorageComponent::TrySplitStack(const FGuid ItemId)
{
	FInventoryOperationResult Result;
	Result.ItemId = ItemId;
	if (!CanOperate())
	{
		Result.Result = EInventoryOperationResult::NotInitialized;
		return Result;
	}
	if (bMutationInProgress)
	{
		Result.Result = EInventoryOperationResult::Busy;
		return Result;
	}
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	UItemInstance* NewStack = nullptr;
	Result.Result = FInventoryStacking::SplitStack(Items, Capacity, this, ItemId, NewStack);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	++Revision;
	Result.Item = NewStack;
	Result.ItemId = NewStack->GetInstanceId();
	Result.Quantity = NewStack->GetQuantity();
	OnStorageChanged.Broadcast(Revision);
	return Result;
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
