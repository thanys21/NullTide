#include "Storage/StorageTransferLibrary.h"

#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryStacking.h"
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
	const int32 DestinationCapacity,
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
	if (!IsValid(Item))
	{
		return MakeTransferResult(EInventoryStorageTransferResult::TransferFailed, ItemId);
	}

	FInventoryStackPlan StackPlan;
	if (!FInventoryStacking::BuildAddPlan(
		DestinationItems, DestinationCapacity, Item->GetDefinitionClass(), Item->GetQuantity(), StackPlan))
	{
		return MakeTransferResult(EInventoryStorageTransferResult::DestinationFull, ItemId);
	}

	TSet<FGuid> ReservedIds;
	for (const UItemInstance* SourceItem : SourceItems)
	{
		if (IsValid(SourceItem))
		{
			ReservedIds.Add(SourceItem->GetInstanceId());
		}
	}
	for (const UItemInstance* DestinationItem : DestinationItems)
	{
		if (IsValid(DestinationItem))
		{
			ReservedIds.Add(DestinationItem->GetInstanceId());
		}
	}

	TArray<TStrongObjectPtr<UItemInstance>> StagedNewStacks;
	TArray<int32> AdditionalStackQuantities = StackPlan.NewStackQuantities;
	const bool bMovesSourceIdentity = !AdditionalStackQuantities.IsEmpty();
	if (bMovesSourceIdentity)
	{
		AdditionalStackQuantities.RemoveAt(0);
	}
	if (!FInventoryStacking::StageNewStacks(
		DestinationOwner, Item->GetDefinitionClass(), AdditionalStackQuantities, ReservedIds, StagedNewStacks))
	{
		return MakeTransferResult(EInventoryStorageTransferResult::TransferFailed, ItemId);
	}

	if (bMovesSourceIdentity
		&& !Item->Rename(nullptr, DestinationOwner, REN_DontCreateRedirectors | REN_NonTransactional))
	{
		return MakeTransferResult(EInventoryStorageTransferResult::TransferFailed, ItemId);
	}

	// Rename is the only fallible commit step. All membership and quantity changes follow together.
	FInventoryStacking::ApplyMerges(StackPlan);
	SourceItems.RemoveAt(SourceIndex);
	if (bMovesSourceIdentity)
	{
		check(FInventoryStacking::SetStackQuantity(Item, StackPlan.NewStackQuantities[0]));
		DestinationItems.Add(Item);
	}
	for (const TStrongObjectPtr<UItemInstance>& NewStack : StagedNewStacks)
	{
		DestinationItems.Add(NewStack.Get());
	}

	UItemInstance* ResultItem = bMovesSourceIdentity
		? Item
		: (StackPlan.Merges.IsEmpty() ? nullptr : StackPlan.Merges[0].Item.Get());
	return MakeTransferResult(EInventoryStorageTransferResult::Success, ItemId, ResultItem);
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
	FArrayProperty* ProjectionArray = nullptr;
	FClassProperty* ProjectionClass = nullptr;
	if (!PrepareInventoryProjection(Inventory, ProjectionArray, ProjectionClass))
	{
		return MakeTransferResult(EInventoryStorageTransferResult::InvalidSource, ItemId);
	}

	TGuardValue<bool> InventoryGuard(Inventory->bMutationInProgress, true);
	TGuardValue<bool> StorageGuard(Storage->bMutationInProgress, true);
	const FInventoryStorageTransferResult Result = TransferExactItem(
		Inventory->Items, Storage->Items, Inventory, Storage, Storage->Capacity, ItemId);
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
	FArrayProperty* ProjectionArray = nullptr;
	FClassProperty* ProjectionClass = nullptr;
	if (!PrepareInventoryProjection(Inventory, ProjectionArray, ProjectionClass))
	{
		return MakeTransferResult(EInventoryStorageTransferResult::InvalidDestination, ItemId);
	}

	TGuardValue<bool> StorageGuard(Storage->bMutationInProgress, true);
	TGuardValue<bool> InventoryGuard(Inventory->bMutationInProgress, true);
	const FInventoryStorageTransferResult Result = TransferExactItem(
		Storage->Items, Inventory->Items, Storage, Inventory, Inventory->Capacity, ItemId);
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
