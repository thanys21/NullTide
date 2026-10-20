#include "Storage/StorageTransferLibrary.h"

#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryStacking.h"
#include "Inventory/LegacyInventoryStorage.h"
#include "Items/ItemDefinition.h"
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
	const int32 MovedQuantity = Item->GetQuantity();
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
	FInventoryStorageTransferResult Result = MakeTransferResult(EInventoryStorageTransferResult::Success, ItemId, ResultItem);
	Result.MovedQuantity = MovedQuantity;
	return Result;
}

/**
 * The largest quantity, at most Quantity, of DefinitionClass that the destination's stacking rules accept, or 0.
 * Found by searching over BuildAddPlan, so the planner stays the only capacity and stacking rule set (fewer units
 * never need more room, so the answer is monotonic).
 */
int32 FindLargestFit(const TArray<TObjectPtr<UItemInstance>>& DestinationItems, const int32 DestinationCapacity,
	const TSubclassOf<UItemDefinition> DefinitionClass, const int32 Quantity)
{
	FInventoryStackPlan Plan;
	if (FInventoryStacking::BuildAddPlan(DestinationItems, DestinationCapacity, DefinitionClass, Quantity, Plan))
	{
		return Quantity;
	}
	int32 Low = 0;
	int32 High = Quantity - 1;
	while (Low < High)
	{
		const int32 Mid = Low + (High - Low + 1) / 2;
		if (FInventoryStacking::BuildAddPlan(DestinationItems, DestinationCapacity, DefinitionClass, Mid, Plan))
		{
			Low = Mid;
		}
		else
		{
			High = Mid - 1;
		}
	}
	return Low;
}

/**
 * Moves part of a stack: Quantity (less than the stack) into the destination, planned there with the stacking rules.
 * The source keeps its identity with the remainder; the moved part merges or gets new ids. Staging is the only
 * fallible step, before any change.
 */
FInventoryStorageTransferResult MovePartOfStack(
	TArray<TObjectPtr<UItemInstance>>& SourceItems,
	TArray<TObjectPtr<UItemInstance>>& DestinationItems,
	UActorComponent* DestinationOwner,
	const int32 DestinationCapacity,
	UItemInstance* Item,
	const int32 Quantity)
{
	const FGuid ItemId = Item->GetInstanceId();
	FInventoryStackPlan StackPlan;
	if (Quantity < 1 || Quantity >= Item->GetQuantity()
		|| !FInventoryStacking::BuildAddPlan(DestinationItems, DestinationCapacity, Item->GetDefinitionClass(), Quantity, StackPlan))
	{
		return MakeTransferResult(EInventoryStorageTransferResult::TransferFailed, ItemId);
	}
	TSet<FGuid> ReservedIds;
	for (const TArray<TObjectPtr<UItemInstance>>* List : { &SourceItems, &DestinationItems })
	{
		for (const UItemInstance* Existing : *List)
		{
			if (IsValid(Existing))
			{
				ReservedIds.Add(Existing->GetInstanceId());
			}
		}
	}
	TArray<TStrongObjectPtr<UItemInstance>> StagedNewStacks;
	if (!FInventoryStacking::StageNewStacks(
		DestinationOwner, Item->GetDefinitionClass(), StackPlan.NewStackQuantities, ReservedIds, StagedNewStacks))
	{
		return MakeTransferResult(EInventoryStorageTransferResult::TransferFailed, ItemId);
	}

	FInventoryStacking::ApplyMerges(StackPlan);
	check(FInventoryStacking::SetStackQuantity(Item, Item->GetQuantity() - Quantity));
	for (const TStrongObjectPtr<UItemInstance>& NewStack : StagedNewStacks)
	{
		DestinationItems.Add(NewStack.Get());
	}
	UItemInstance* ResultItem = !StagedNewStacks.IsEmpty() ? StagedNewStacks[0].Get()
		: (StackPlan.Merges.IsEmpty() ? nullptr : StackPlan.Merges[0].Item.Get());
	FInventoryStorageTransferResult Result = MakeTransferResult(EInventoryStorageTransferResult::Success, ItemId, ResultItem);
	Result.MovedQuantity = Quantity;
	return Result;
}

/** One stack, as much as fits: the exact transfer when it all fits, else the largest part. */
FInventoryStorageTransferResult MoveStackAsMuchAsFits(
	TArray<TObjectPtr<UItemInstance>>& SourceItems,
	TArray<TObjectPtr<UItemInstance>>& DestinationItems,
	UActorComponent* SourceOwner,
	UActorComponent* DestinationOwner,
	const int32 DestinationCapacity,
	const FGuid ItemId)
{
	const TObjectPtr<UItemInstance>* Found = ItemId.IsValid()
		? SourceItems.FindByPredicate([&ItemId](const TObjectPtr<UItemInstance>& Candidate)
		{
			return IsValid(Candidate) && Candidate->GetInstanceId() == ItemId;
		})
		: nullptr;
	UItemInstance* Item = Found ? Found->Get() : nullptr;
	if (!Item)
	{
		return MakeTransferResult(EInventoryStorageTransferResult::InvalidItemId, ItemId);
	}
	const int32 Fit = FindLargestFit(DestinationItems, DestinationCapacity, Item->GetDefinitionClass(), Item->GetQuantity());
	if (Fit < 1)
	{
		return MakeTransferResult(EInventoryStorageTransferResult::DestinationFull, ItemId);
	}
	if (Fit == Item->GetQuantity())
	{
		FInventoryStorageTransferResult Result = TransferExactItem(
			SourceItems, DestinationItems, SourceOwner, DestinationOwner, DestinationCapacity, ItemId);
		Result.MovedQuantity = Result.IsSuccess() ? Fit : 0;
		return Result;
	}
	return MovePartOfStack(SourceItems, DestinationItems, DestinationOwner, DestinationCapacity, Item, Fit);
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

FInventoryStorageTransferResult UStorageTransferLibrary::MoveInventoryStackToStorage(
	UInventoryComponent* Inventory, UStorageComponent* Storage, const FGuid ItemId)
{
	return RunStackMove(Inventory, Storage, true, ItemId);
}

FInventoryStorageTransferResult UStorageTransferLibrary::MoveStorageStackToInventory(
	UStorageComponent* Storage, UInventoryComponent* Inventory, const FGuid ItemId)
{
	return RunStackMove(Inventory, Storage, false, ItemId);
}

FStorageTransferBatchResult UStorageTransferLibrary::MoveAllInventoryToStorage(
	UInventoryComponent* Inventory, UStorageComponent* Storage, const TSubclassOf<UItemDefinition> OnlyDefinition)
{
	return RunBatchMove(Inventory, Storage, true, OnlyDefinition);
}

FStorageTransferBatchResult UStorageTransferLibrary::MoveAllStorageToInventory(
	UStorageComponent* Storage, UInventoryComponent* Inventory, const TSubclassOf<UItemDefinition> OnlyDefinition)
{
	return RunBatchMove(Inventory, Storage, false, OnlyDefinition);
}

EInventoryStorageTransferResult UStorageTransferLibrary::PreflightMove(UInventoryComponent* Inventory,
	UStorageComponent* Storage, const bool bToStorage, FArrayProperty*& OutProjectionArray, FClassProperty*& OutProjectionClass)
{
	const bool bInventoryValid = IsValid(Inventory) && Inventory->CanOperate();
	const bool bStorageValid = IsValid(Storage) && Storage->CanOperate();
	if (!(bToStorage ? bInventoryValid : bStorageValid))
	{
		return EInventoryStorageTransferResult::InvalidSource;
	}
	if (!(bToStorage ? bStorageValid : bInventoryValid))
	{
		return EInventoryStorageTransferResult::InvalidDestination;
	}
	if (Inventory->bMutationInProgress || Storage->bMutationInProgress)
	{
		return EInventoryStorageTransferResult::Busy;
	}
	if (!PrepareInventoryProjection(Inventory, OutProjectionArray, OutProjectionClass))
	{
		return bToStorage ? EInventoryStorageTransferResult::InvalidSource : EInventoryStorageTransferResult::InvalidDestination;
	}
	return EInventoryStorageTransferResult::Success;
}

void UStorageTransferLibrary::CommitNotifications(UInventoryComponent* Inventory, UStorageComponent* Storage,
	const bool bToStorage, FArrayProperty* ProjectionArray, FClassProperty* ProjectionClass)
{
	if (ProjectionArray)
	{
		Inventory->RebuildLegacyProjection(ProjectionArray, ProjectionClass);
	}
	++Inventory->Revision;
	++Storage->Revision;
	if (bToStorage)
	{
		Inventory->OnInventoryChanged.Broadcast(Inventory->Revision);
		Storage->OnStorageChanged.Broadcast(Storage->Revision);
	}
	else
	{
		Storage->OnStorageChanged.Broadcast(Storage->Revision);
		Inventory->OnInventoryChanged.Broadcast(Inventory->Revision);
	}
}

FInventoryStorageTransferResult UStorageTransferLibrary::RunStackMove(
	UInventoryComponent* Inventory, UStorageComponent* Storage, const bool bToStorage, const FGuid ItemId)
{
	FArrayProperty* ProjectionArray = nullptr;
	FClassProperty* ProjectionClass = nullptr;
	const EInventoryStorageTransferResult Preflight = PreflightMove(Inventory, Storage, bToStorage, ProjectionArray, ProjectionClass);
	if (Preflight != EInventoryStorageTransferResult::Success)
	{
		return MakeTransferResult(Preflight, ItemId);
	}
	FInventoryStorageTransferResult Result;
	{
		TGuardValue<bool> InventoryGuard(Inventory->bMutationInProgress, true);
		TGuardValue<bool> StorageGuard(Storage->bMutationInProgress, true);
		Result = bToStorage
			? MoveStackAsMuchAsFits(Inventory->Items, Storage->Items, Inventory, Storage, Storage->Capacity, ItemId)
			: MoveStackAsMuchAsFits(Storage->Items, Inventory->Items, Storage, Inventory, Inventory->Capacity, ItemId);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		// Notified inside the guards, as the exact transfers do: a listener cannot start a nested move.
		CommitNotifications(Inventory, Storage, bToStorage, ProjectionArray, ProjectionClass);
	}
	return Result;
}

FStorageTransferBatchResult UStorageTransferLibrary::RunBatchMove(UInventoryComponent* Inventory,
	UStorageComponent* Storage, const bool bToStorage, const TSubclassOf<UItemDefinition> OnlyDefinition)
{
	FStorageTransferBatchResult Batch;
	FArrayProperty* ProjectionArray = nullptr;
	FClassProperty* ProjectionClass = nullptr;
	Batch.Result = PreflightMove(Inventory, Storage, bToStorage, ProjectionArray, ProjectionClass);
	if (Batch.Result != EInventoryStorageTransferResult::Success)
	{
		return Batch;
	}

	TGuardValue<bool> InventoryGuard(Inventory->bMutationInProgress, true);
	TGuardValue<bool> StorageGuard(Storage->bMutationInProgress, true);
	TArray<TObjectPtr<UItemInstance>>& SourceItems = bToStorage ? Inventory->Items : Storage->Items;
	TArray<TObjectPtr<UItemInstance>>& DestinationItems = bToStorage ? Storage->Items : Inventory->Items;
	UActorComponent* SourceOwner = bToStorage ? static_cast<UActorComponent*>(Inventory) : Storage;
	UActorComponent* DestinationOwner = bToStorage ? static_cast<UActorComponent*>(Storage) : Inventory;
	const int32 DestinationCapacity = bToStorage ? Storage->Capacity : Inventory->Capacity;
	auto Matches = [&OnlyDefinition](const UItemInstance* Item)
	{
		return IsValid(Item) && (!OnlyDefinition || Item->GetDefinitionClass() == OnlyDefinition);
	};

	// The candidate ids, in source order, taken before anything moves.
	TArray<FGuid> Candidates;
	for (const UItemInstance* Item : SourceItems)
	{
		if (Matches(Item))
		{
			Candidates.Add(Item->GetInstanceId());
		}
	}
	EInventoryStorageTransferResult FirstFailure = EInventoryStorageTransferResult::DestinationFull;
	for (const FGuid& ItemId : Candidates)
	{
		const FInventoryStorageTransferResult Move = MoveStackAsMuchAsFits(
			SourceItems, DestinationItems, SourceOwner, DestinationOwner, DestinationCapacity, ItemId);
		if (Move.IsSuccess())
		{
			Batch.MovedQuantity += Move.MovedQuantity;
			++Batch.MovedStacks;
		}
		else if (Move.Result != EInventoryStorageTransferResult::DestinationFull)
		{
			// An unexpected failure (staging) stops the batch; every move so far is complete and stays.
			FirstFailure = Move.Result;
			break;
		}
	}
	for (const UItemInstance* Item : SourceItems)
	{
		Batch.StacksLeft += Matches(Item) ? 1 : 0;
	}
	if (Batch.MovedStacks == 0)
	{
		Batch.Result = Candidates.IsEmpty() ? EInventoryStorageTransferResult::NothingToMove : FirstFailure;
		return Batch;
	}
	Batch.Result = EInventoryStorageTransferResult::Success;
	CommitNotifications(Inventory, Storage, bToStorage, ProjectionArray, ProjectionClass);
	return Batch;
}
