// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/InventoryTypes.h"
#include "InventoryComponent.generated.h"

class UItemDefinition;
class UItemInstance;
class UStorageComponent;
class UStorageTransferLibrary;
class AActor;
class APawn;
class FArrayProperty;
class FClassProperty;
struct FInventoryStackPlan;
struct FInventoryTransactionPlan;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryChanged, int32, NewRevision);

/** Owns item membership and all inventory mutations. */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class NULLTIDE_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	FInventoryOperationResult TryAddDefinition(TSubclassOf<UItemDefinition> DefinitionClass);

	/** Adds an entire requested quantity atomically, filling compatible stacks before allocating slots. */
	UFUNCTION(BlueprintCallable, Category = "Inventory", meta = (ClampMin = "1"))
	FInventoryOperationResult TryAddDefinitionQuantity(TSubclassOf<UItemDefinition> DefinitionClass, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	FInventoryOperationResult TryRemoveItem(FGuid ItemId);

	/** Removes quantity from one exact stack identity. A zero remainder removes that stack entry. */
	UFUNCTION(BlueprintCallable, Category = "Inventory", meta = (ClampMin = "1"))
	FInventoryOperationResult TryRemoveItemQuantity(FGuid ItemId, int32 Quantity);

	/**
	 * Atomically removes and adds quantities by exact definition class. Removals consume the earliest
	 * stacks first; additions are planned against the post-removal contents with the normal stacking
	 * rules. On success Revision rises once and OnInventoryChanged fires once; on failure nothing changes.
	 * Duplicate definitions within one list are summed. Native only until a crafting authority exposes it.
	 */
	FInventoryTransactionResult TryApplyDefinitionTransaction(const TArray<FInventoryDefinitionQuantity>& Removals,
		const TArray<FInventoryDefinitionQuantity>& Additions);

	/** Spawns the generic pickup first, then commits the exact-ID removal on success. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	FInventoryWorldDropResult TryDropItemToWorld(APawn* OwningPawn, FGuid ItemId);

	/** Moves the exact runtime instance to storage without changing its ItemId. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Storage")
	FInventoryStorageTransferResult TryTransferItemToStorage(UStorageComponent* Storage, FGuid ItemId);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<UItemInstance*> GetItemsSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UItemInstance* FindItemById(FGuid ItemId) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool ContainsItem(FGuid ItemId) const;

	/** Sums Quantity over every stack of exactly this definition class. Null or absent definitions return 0. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Queries")
	int32 GetTotalQuantity(TSubclassOf<UItemDefinition> DefinitionClass) const;

	/** Read-only: succeeds when the total quantity across stacks covers the request. Removes nothing. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Queries")
	FInventoryOperationResult CanRemoveDefinitionQuantity(TSubclassOf<UItemDefinition> DefinitionClass, int32 Quantity) const;

	/** Read-only: returns the result TryAddDefinitionQuantity would return for the current contents. Adds nothing. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Queries")
	FInventoryOperationResult CanAddDefinitionQuantity(TSubclassOf<UItemDefinition> DefinitionClass, int32 Quantity) const;

	/** Read-only: checks that every request fits at the same time. Requests for the same definition are summed. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Queries")
	FInventoryOperationResult CanAddDefinitionQuantities(const TArray<FInventoryDefinitionQuantity>& Requests) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount() const { return Items.Num(); }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetCapacity() const { return Capacity; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsFull() const { return Items.Num() >= Capacity; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetRevision() const { return Revision; }

	/** Legacy Blueprint subclasses keep their existing array authoritative until cutover. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Compatibility")
	bool IsLegacyInventoryMode() const { return bLegacyInventoryMode; }

	/** Imports the serialized legacy seed once. A failed attempt requires a fresh instance. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Compatibility")
	FInventoryOperationResult InitializeFromLegacyInventory();

	UFUNCTION(BlueprintPure, Category = "Inventory|Compatibility")
	EInventoryInitializationState GetInitializationState() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Compatibility")
	EInventoryOperationResult GetLastInitializationResult() const { return LastInitializationResult; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Compatibility")
	bool IsNativeAuthorityActive() const { return CanOperate(); }

	bool ShouldBlockLegacyWrites() const
	{
		return bImportLegacyInventoryOnBeginPlay || InitializationState == EInventoryInitializationState::Failed;
	}

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

protected:
	virtual void BeginPlay() override;

	/** Engine seams keep the transaction testable while the component retains commit authority. */
	virtual bool ResolveWorldDropTransform(const APawn* OwningPawn, TSubclassOf<UItemDefinition> DefinitionClass,
		FTransform& OutTransform) const;
	virtual AActor* SpawnAndConfigureWorldDrop(APawn* OwningPawn, TSubclassOf<UItemDefinition> DefinitionClass,
		int32 Quantity, const FTransform& DropTransform);
	virtual void RollbackWorldDrop(AActor* SpawnedPickup);
	virtual FInventoryOperationResult CommitWorldDropRemoval(FGuid ItemId);

private:
	friend class UStorageTransferLibrary;

	bool CanOperate() const;
	/** Shared add validation and stack plan used by both TryAddDefinitionQuantity and the add queries. */
	EInventoryOperationResult PreflightAdd(TSubclassOf<UItemDefinition> DefinitionClass, int32 Quantity,
		FInventoryStackPlan& OutPlan, FArrayProperty*& OutProjectionArray, FClassProperty*& OutProjectionClass) const;
	/** Definition checks shared by single adds and transaction additions (class flags and fragment data). */
	EInventoryOperationResult ValidateAddDefinition(TSubclassOf<UItemDefinition> DefinitionClass) const;
	/** Plans a whole definition transaction from the live contents without changing them. */
	FInventoryTransactionResult PreflightTransaction(const TArray<FInventoryDefinitionQuantity>& Removals,
		const TArray<FInventoryDefinitionQuantity>& Additions, FInventoryTransactionPlan& OutPlan,
		FArrayProperty*& OutProjectionArray, FClassProperty*& OutProjectionClass) const;
	bool IsDefinitionDataValid(const UItemDefinition* Definition) const;
	bool HasItemId(const FGuid& ItemId) const;
	void RebuildLegacyProjection(FArrayProperty* ArrayProperty, FClassProperty* ClassProperty);

	UPROPERTY()
	TArray<TObjectPtr<UItemInstance>> Items;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 Capacity = 24;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	int32 Revision = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Compatibility", meta = (AllowPrivateAccess = "true"))
	bool bLegacyInventoryMode = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Compatibility", meta = (AllowPrivateAccess = "true"))
	bool bImportLegacyInventoryOnBeginPlay = false;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Inventory|Compatibility", meta = (AllowPrivateAccess = "true"))
	EInventoryInitializationState InitializationState = EInventoryInitializationState::NotRequired;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Inventory|Compatibility", meta = (AllowPrivateAccess = "true"))
	EInventoryOperationResult LastInitializationResult = EInventoryOperationResult::NotInitialized;

	bool bMutationInProgress = false;
};
