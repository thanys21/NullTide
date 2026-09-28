#include "Storage/StorageTransferLibrary.h"

#include "Inventory/InventoryComponent.h"
#include "Inventory/LegacyInventoryStorage.h"
#include "Items/ItemInstance.h"
#include "Storage/StorageComponent.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
FInventoryStorageTransferResult MakeTransferResult(
	const EInventoryStorageTransferResult Result, const FGuid ItemId, UItemInstance* Item = nullptr)
{
	FInventoryStorageTransferResult TransferResult;
	TransferResult.Result = Result;
	TransferResult.ItemId = ItemId;
	TransferResult.Item = Item;
	return TransferResult;
}

bool PrepareInventoryProjection(UInventoryComponent* Inventory, FArrayProperty*& OutArray, FClassProperty*& OutClass)
{
	OutArray = nullptr;
	OutClass = nullptr;
	return Inventory->GetInitializationState() != EInventoryInitializationState::NativeReady
		|| LegacyInventoryStorage::FindArray(Inventory, OutArray, OutClass);
}

FInventoryStorageTransferResult TransferExactItem(
	TArray<TObjectPtr<UItemInstance>>& SourceItems,
	TArray<TObjectPtr<UItemInstance>>& DestinationItems,
	UActorComponent* SourceOwner,
	UActorComponent* DestinationOwner,
	FGuid ItemId)
{
	if (!ItemId.IsValid())
	{
		return MakeTransferResult(EInventoryStorageTransferResult::InvalidItemId, ItemId);
	}
	const int32 SourceIndex = SourceItems.IndexOfByPredicate(
		[&ItemId](const TObjectPtr<UItemInstance>& Item)
		{
			return IsValid(Item) && Item->GetInstanceId() == ItemId;
		});
	if (SourceIndex == INDEX_NONE)
	{
		return MakeTransferResult(EInventoryStorageTransferResult::InvalidItemId, ItemId);
	}
	if (DestinationItems.ContainsByPredicate(
		[&ItemId](const TObjectPtr<UItemInstance>& Item)
		{
			return IsValid(Item) && Item->GetInstanceId() == ItemId;
		}))
	{
		return MakeTransferResult(EInventoryStorageTransferResult::TransferFailed, ItemId);
	}

	UItemInstance* Item = SourceItems[SourceIndex];
	if (!IsValid(Item) || !Item->Rename(nullptr, DestinationOwner, REN_DontCreateRedirectors | REN_NonTransactional))
	{
		return MakeTransferResult(EInventoryStorageTransferResult::TransferFailed, ItemId);
	}

	// Rename is the only fallible commit step. Membership changes follow together on the game thread.
	SourceItems.RemoveAt(SourceIndex);
	DestinationItems.Add(Item);
	return MakeTransferResult(EInventoryStorageTransferResult::Success, ItemId, Item);
}
}

FInventoryStorageTransferResult UStorageTransferLibrary::TransferInventoryToStorage(
	UInventoryComponent* Inventory, UStorageComponent* Storage, const FGuid ItemId)
{
	if (!IsValid(Inventory) || !Inventory->CanOperate())
	{
		return MakeTransferResult(EInventoryStorageTransferResult::InvalidSource, ItemId);
	}
	if (!IsValid(Storage) || !Storage->CanOperate())
	{
		return MakeTransferResult(EInventoryStorageTransferResult::InvalidDestination, ItemId);
	}
	if (Inventory->bMutationInProgress || Storage->bMutationInProgress)
	{
		return MakeTransferResult(EInventoryStorageTransferResult::Busy, ItemId);
	}
	if (Storage->IsFull())
	{
		return MakeTransferResult(EInventoryStorageTransferResult::DestinationFull, ItemId);
	}

	FArrayProperty* ProjectionArray = nullptr;
	FClassProperty* ProjectionClass = nullptr;
	if (!PrepareInventoryProjection(Inventory, ProjectionArray, ProjectionClass))
	{
		return MakeTransferResult(EInventoryStorageTransferResult::InvalidSource, ItemId);
	}

	TGuardValue<bool> InventoryGuard(Inventory->bMutationInProgress, true);
	TGuardValue<bool> StorageGuard(Storage->bMutationInProgress, true);
	const FInventoryStorageTransferResult Result = TransferExactItem(
		Inventory->Items, Storage->Items, Inventory, Storage, ItemId);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (ProjectionArray)
	{
		Inventory->RebuildLegacyProjection(ProjectionArray, ProjectionClass);
	}
	++Inventory->Revision;
	++Storage->Revision;
	Inventory->OnInventoryChanged.Broadcast(Inventory->Revision);
	Storage->OnStorageChanged.Broadcast(Storage->Revision);
	return Result;
}

FInventoryStorageTransferResult UStorageTransferLibrary::TransferStorageToInventory(
	UStorageComponent* Storage, UInventoryComponent* Inventory, const FGuid ItemId)
{
	if (!IsValid(Storage) || !Storage->CanOperate())
	{
		return MakeTransferResult(EInventoryStorageTransferResult::InvalidSource, ItemId);
	}
	if (!IsValid(Inventory) || !Inventory->CanOperate())
	{
		return MakeTransferResult(EInventoryStorageTransferResult::InvalidDestination, ItemId);
	}
	if (Storage->bMutationInProgress || Inventory->bMutationInProgress)
	{
		return MakeTransferResult(EInventoryStorageTransferResult::Busy, ItemId);
	}
	if (Inventory->IsFull())
	{
		return MakeTransferResult(EInventoryStorageTransferResult::DestinationFull, ItemId);
	}

	FArrayProperty* ProjectionArray = nullptr;
	FClassProperty* ProjectionClass = nullptr;
	if (!PrepareInventoryProjection(Inventory, ProjectionArray, ProjectionClass))
	{
		return MakeTransferResult(EInventoryStorageTransferResult::InvalidDestination, ItemId);
	}

	TGuardValue<bool> StorageGuard(Storage->bMutationInProgress, true);
	TGuardValue<bool> InventoryGuard(Inventory->bMutationInProgress, true);
	const FInventoryStorageTransferResult Result = TransferExactItem(
		Storage->Items, Inventory->Items, Storage, Inventory, ItemId);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (ProjectionArray)
	{
		Inventory->RebuildLegacyProjection(ProjectionArray, ProjectionClass);
	}
	++Storage->Revision;
	++Inventory->Revision;
	Storage->OnStorageChanged.Broadcast(Storage->Revision);
	Inventory->OnInventoryChanged.Broadcast(Inventory->Revision);
	return Result;
}
