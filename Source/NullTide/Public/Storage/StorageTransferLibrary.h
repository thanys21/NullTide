#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Inventory/InventoryTypes.h"
#include "StorageTransferLibrary.generated.h"

class UInventoryComponent;
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
};
