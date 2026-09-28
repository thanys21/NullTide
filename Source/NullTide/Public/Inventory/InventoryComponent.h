// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/InventoryTypes.h"
#include "InventoryComponent.generated.h"

class UItemDefinition;
class UItemInstance;
class AActor;
class APawn;
class FArrayProperty;
class FClassProperty;

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

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	FInventoryOperationResult TryRemoveItem(FGuid ItemId);

	/** Spawns the generic pickup first, then commits the exact-ID removal on success. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	FInventoryWorldDropResult TryDropItemToWorld(APawn* OwningPawn, FGuid ItemId);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<UItemInstance*> GetItemsSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UItemInstance* FindItemById(FGuid ItemId) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool ContainsItem(FGuid ItemId) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount() const { return Items.Num(); }

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
		const FTransform& DropTransform);
	virtual void RollbackWorldDrop(AActor* SpawnedPickup);
	virtual FInventoryOperationResult CommitWorldDropRemoval(FGuid ItemId);

private:
	bool CanOperate() const;
	bool IsDefinitionDataValid(const UItemDefinition* Definition) const;
	bool HasItemId(const FGuid& ItemId) const;
	void RebuildLegacyProjection(FArrayProperty* ArrayProperty, FClassProperty* ClassProperty);

	UPROPERTY()
	TArray<TObjectPtr<UItemInstance>> Items;

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
