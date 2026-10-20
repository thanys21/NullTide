#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Inventory/InventoryTypes.h"
#include "StorageTransferLibrary.generated.h"

class FArrayProperty;
class FClassProperty;
class UInventoryComponent;
class UItemDefinition;
class UStorageComponent;

/** Stateless coordinator for atomic inventory/storage ownership transfers. */
UCLASS()
class NULLTIDE_API UStorageTransferLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static FInventoryStorageTransferResult TransferInventoryToStorage(
		UInventoryComponent* Inventory, UStorageComponent* Storage, FGuid ItemId);

	static FInventoryStorageTransferResult TransferStorageToInventory(
		UStorageComponent* Storage, UInventoryComponent* Inventory, FGuid ItemId);

	/**
	 * Moves one stack as much as fits (G9).
	 * - If the whole stack fits, this is the exact transfer above: the identity moves.
	 * - Otherwise it moves the largest part the destination's stacking rules accept. The source keeps its ItemId with
	 *   the remainder; the destination merges first, then adds new stacks.
	 * Nothing fits: DestinationFull, and nothing changes. One revision and one broadcast per side.
	 */
	static FInventoryStorageTransferResult MoveInventoryStackToStorage(
		UInventoryComponent* Inventory, UStorageComponent* Storage, FGuid ItemId);

	static FInventoryStorageTransferResult MoveStorageStackToInventory(
		UStorageComponent* Storage, UInventoryComponent* Inventory, FGuid ItemId);

	/**
	 * Moves every source stack, or only stacks of exactly OnlyDefinition, as much as fits each (G9), in source order,
	 * under one mutation guard. Leftovers stay. Each stack move is atomic; when anything moved, Revision rises once and
	 * one broadcast fires per side. When nothing moved: NothingToMove or DestinationFull, and no event.
	 */
	static FStorageTransferBatchResult MoveAllInventoryToStorage(
		UInventoryComponent* Inventory, UStorageComponent* Storage, TSubclassOf<UItemDefinition> OnlyDefinition = nullptr);

	static FStorageTransferBatchResult MoveAllStorageToInventory(
		UStorageComponent* Storage, UInventoryComponent* Inventory, TSubclassOf<UItemDefinition> OnlyDefinition = nullptr);

private:
	static FInventoryStorageTransferResult RunStackMove(
		UInventoryComponent* Inventory, UStorageComponent* Storage, bool bToStorage, FGuid ItemId);

	static FStorageTransferBatchResult RunBatchMove(
		UInventoryComponent* Inventory, UStorageComponent* Storage, bool bToStorage, TSubclassOf<UItemDefinition> OnlyDefinition);

	/** The shared preflight of every move: valid sides, no mutation in progress, a legacy projection when needed. */
	static EInventoryStorageTransferResult PreflightMove(UInventoryComponent* Inventory, UStorageComponent* Storage,
		bool bToStorage, FArrayProperty*& OutProjectionArray, FClassProperty*& OutProjectionClass);

	/** Bumps both revisions and broadcasts once per side, source first. */
	static void CommitNotifications(UInventoryComponent* Inventory, UStorageComponent* Storage, bool bToStorage,
		FArrayProperty* ProjectionArray, FClassProperty* ProjectionClass);
};
