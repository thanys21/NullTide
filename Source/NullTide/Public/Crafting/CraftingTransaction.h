// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryTypes.h"
#include "CraftingTransaction.generated.h"

class UCraftingRecipeDefinition;
class UInventoryComponent;

UENUM(BlueprintType)
enum class ECraftingTransactionResult : uint8
{
	Success,
	InvalidInventory,
	InvalidRecipe,
	InsufficientIngredients,
	OutputCapacityFull,
	InventoryBusy,
	/** The inventory refused a request for another reason; see InventoryResult. */
	InventoryRejected
};

/** Result of one atomic craft. */
USTRUCT(BlueprintType)
struct NULLTIDE_API FCraftingTransactionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	ECraftingTransactionResult Result = ECraftingTransactionResult::InvalidRecipe;

	/** Index into the recipe's Ingredients (ingredient failures) or Outputs (output failures), or INDEX_NONE. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	int32 EntryIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	EInventoryOperationResult InventoryResult = EInventoryOperationResult::NotInitialized;

	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	FString Diagnostic;

	bool IsSuccess() const
	{
		return Result == ECraftingTransactionResult::Success;
	}
};

/**
 * Executes a recipe as one atomic inventory transaction. This is the authoritative craft path;
 * UCraftingQueryLibrary checks are advisory. Native only: the future UCraftingComponent owns it.
 */
class NULLTIDE_API FCraftingTransaction
{
public:
	static FCraftingTransactionResult TryCraftRecipeAtomic(UInventoryComponent* Inventory,
		TSubclassOf<UCraftingRecipeDefinition> RecipeClass);
	static FCraftingTransactionResult TryCraftRecipeAtomic(UInventoryComponent* Inventory,
		const UCraftingRecipeDefinition* Recipe);
};
