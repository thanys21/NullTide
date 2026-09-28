#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/InventoryTypes.h"
#include "StorageComponent.generated.h"

class UInventoryComponent;
class UItemInstance;
class UStorageTransferLibrary;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStorageChanged, int32, NewRevision);

/** Owns ordered, non-stacking runtime item instances for one world container. */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class NULLTIDE_API UStorageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStorageComponent();

	/** Moves the exact runtime instance to player inventory without changing its ItemId. */
	UFUNCTION(BlueprintCallable, Category = "Storage")
	FInventoryStorageTransferResult TryTransferItemToInventory(UInventoryComponent* Inventory, FGuid ItemId);

	UFUNCTION(BlueprintPure, Category = "Storage")
	TArray<UItemInstance*> GetItemsSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Storage")
	UItemInstance* FindItemById(FGuid ItemId) const;

	UFUNCTION(BlueprintPure, Category = "Storage")
	bool ContainsItem(FGuid ItemId) const;

	UFUNCTION(BlueprintPure, Category = "Storage")
	int32 GetItemCount() const { return Items.Num(); }

	UFUNCTION(BlueprintPure, Category = "Storage")
	int32 GetCapacity() const { return Capacity; }

	UFUNCTION(BlueprintPure, Category = "Storage")
	bool IsFull() const { return Items.Num() >= Capacity; }

	UFUNCTION(BlueprintPure, Category = "Storage")
	int32 GetRevision() const { return Revision; }

	UPROPERTY(BlueprintAssignable, Category = "Storage")
	FOnStorageChanged OnStorageChanged;

private:
	friend class UStorageTransferLibrary;

	bool CanOperate() const;

	UPROPERTY()
	TArray<TObjectPtr<UItemInstance>> Items;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Storage", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 Capacity = 24;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Storage", meta = (AllowPrivateAccess = "true"))
	int32 Revision = 0;

	bool bMutationInProgress = false;
};
