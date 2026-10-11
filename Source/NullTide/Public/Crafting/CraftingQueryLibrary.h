// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CraftingQueryLibrary.generated.h"

class UCraftingRecipeDefinition;
class UInventoryComponent;

UENUM(BlueprintType, meta = (ScriptName = "CraftingQueryResultCode"))
enum class ECraftingQueryResult : uint8
{
	Success,
	InvalidInventory,
	InvalidRecipe,
	InsufficientIngredients,
	OutputCapacityFull
};

/** Result of a read-only recipe query against one inventory. */
USTRUCT(BlueprintType)
struct NULLTIDE_API FCraftingQueryResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	ECraftingQueryResult Result = ECraftingQueryResult::InvalidRecipe;

	/** First failing ingredient index, or INDEX_NONE when the failure is not tied to one ingredient. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	int32 EntryIndex = INDEX_NONE;

	/** The underlying inventory query result, when an inventory query decided the outcome. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	EInventoryOperationResult InventoryResult = EInventoryOperationResult::NotInitialized;

	bool IsSuccess() const
	{
		return Result == ECraftingQueryResult::Success;
	}
};

/**
 * Read-only recipe queries built on the generic UInventoryComponent queries.
 * Recipe knowledge stays here so the inventory authority never depends on crafting types.
 */
UCLASS()
class NULLTIDE_API UCraftingQueryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Succeeds when the inventory currently holds every ingredient quantity. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Queries")
	static FCraftingQueryResult CheckRecipeIngredients(const UInventoryComponent* Inventory,
		TSubclassOf<UCraftingRecipeDefinition> RecipeClass);

	/**
	 * Succeeds when all outputs fit together in the inventory as it is now, before any ingredient is consumed.
	 * Consumption can only free space, so success stays true after consumption. A full inventory can still be
	 * rejected even though consuming the ingredients would make room; the exact check belongs to the craft
	 * transaction preflight (C3/C4).
	 */
	UFUNCTION(BlueprintPure, Category = "Crafting|Queries")
	static FCraftingQueryResult CheckRecipeOutputCapacity(const UInventoryComponent* Inventory,
		TSubclassOf<UCraftingRecipeDefinition> RecipeClass);

	/** Native forms that take a recipe object, such as a class default object. */
	static FCraftingQueryResult CheckIngredients(const UInventoryComponent* Inventory, const UCraftingRecipeDefinition* Recipe);
	static FCraftingQueryResult CheckOutputCapacity(const UInventoryComponent* Inventory, const UCraftingRecipeDefinition* Recipe);
};
