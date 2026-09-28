// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryTypes.h"
#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"
#include "Components/ActorComponent.h"
#include "InventoryComponentTestTypes.generated.h"

class UInventoryComponent;
class AActor;
class APawn;

/** Legacy array fixture used only by native compatibility automation tests. */
UCLASS(Transient, NotBlueprintable)
class UInventoryLegacyTestManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<TSubclassOf<UItemDefinition>> InventoryItems;
};

/** Native-subclass legacy seed fixture for atomic cutover tests. */
UCLASS(Transient, NotBlueprintable)
class UInventoryCutoverTestManager : public UInventoryComponent
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<TSubclassOf<UItemDefinition>> InventoryItems;
};

/** Test-only transaction fixture. Production spawning remains in UInventoryComponent. */
UCLASS(Transient, NotBlueprintable)
class UInventoryWorldDropTestComponent : public UInventoryComponent
{
	GENERATED_BODY()

public:
	bool bResolveTransformSucceeds = true;
	bool bSpawnSucceeds = true;
	bool bCommitSucceeds = true;
	int32 SpawnAttempts = 0;
	int32 RollbackCount = 0;
	int32 CommitAttempts = 0;
	TObjectPtr<AActor> LastSpawnedPickup;
	TObjectPtr<AActor> LastRolledBackPickup;

	UPROPERTY()
	TArray<TSubclassOf<UItemDefinition>> InventoryItems;

protected:
	virtual bool ResolveWorldDropTransform(const APawn* OwningPawn, TSubclassOf<UItemDefinition> DefinitionClass,
		FTransform& OutTransform) const override;
	virtual AActor* SpawnAndConfigureWorldDrop(APawn* OwningPawn, TSubclassOf<UItemDefinition> DefinitionClass,
		const FTransform& DropTransform) override;
	virtual void RollbackWorldDrop(AActor* SpawnedPickup) override;
	virtual FInventoryOperationResult CommitWorldDropRemoval(FGuid ItemId) override;
};

UCLASS(Transient, NotBlueprintable)
class UInventoryOtherTestItemDefinition : public UItemDefinition
{
	GENERATED_BODY()
};

UCLASS(Transient, NotBlueprintable)
class UInventoryInvalidDataTestDefinition : public UItemDefinition
{
	GENERATED_BODY()
public:
	UInventoryInvalidDataTestDefinition() { Fragments.Add(nullptr); }
};

/** Concrete definition used only by native inventory automation tests. */
UCLASS(Transient, NotBlueprintable)
class UInventoryTestItemDefinition : public UItemDefinition
{
	GENERATED_BODY()
};

/** Dynamic delegate listener used only by native inventory automation tests. */
UCLASS(Transient, NotBlueprintable)
class UInventoryComponentTestListener : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleInventoryChanged(int32 NewRevision);

	UPROPERTY()
	TObjectPtr<UInventoryComponent> Inventory;

	TSubclassOf<UItemDefinition> ReentrantDefinitionClass;
	FInventoryOperationResult ReentrantResult;
	int32 EventCount = 0;
	int32 LastRevision = 0;
	bool bAttemptReentrantMutation = false;
	bool bAttemptReentrantInitialization = false;
	FInventoryOperationResult ReentrantInitializationResult;
	int32 ObservedItemCount = 0;
	TArray<TSubclassOf<UItemDefinition>> ObservedDefinitions;
};
