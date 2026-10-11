// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Crafting/CraftingRecipeDefinition.h"
#include "Crafting/CraftingTransaction.h"
#include "Inventory/InventoryTypes.h"
#include "Items/ItemDefinition.h"
#include "CraftingTestTypes.generated.h"

class UInventoryComponent;

/** Concrete valid recipe used only by native crafting automation tests. */
UCLASS(Transient, NotBlueprintable)
class UCraftingTestRecipeDefinition : public UCraftingRecipeDefinition
{
	GENERATED_BODY()

public:
	UCraftingTestRecipeDefinition();
};

/** Second stackable definition (MaxStackSize 20) used only by crafting transaction tests. */
UCLASS(Transient, NotBlueprintable)
class UCraftingSmallStackTestItemDefinition : public UItemDefinition
{
	GENERATED_BODY()

public:
	UCraftingSmallStackTestItemDefinition();
};

/** Change listener that can attempt a reentrant transaction or craft while the inventory broadcasts. */
UCLASS(Transient, NotBlueprintable)
class UCraftingTransactionTestListener : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleInventoryChanged(int32 NewRevision);

	UPROPERTY()
	TObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY()
	TObjectPtr<UCraftingRecipeDefinition> ReentrantRecipe;

	TArray<FInventoryDefinitionQuantity> ReentrantRemovals;
	TArray<FInventoryDefinitionQuantity> ReentrantAdditions;
	bool bAttemptReentrantTransaction = false;
	FInventoryTransactionResult ReentrantTransactionResult;
	FCraftingTransactionResult ReentrantCraftResult;
	int32 EventCount = 0;
	int32 LastRevision = 0;
	int32 ObservedItemCount = 0;
};
