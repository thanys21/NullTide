// Fill out your copyright notice in the Description page of Project Settings.


#include "Crafting/CraftingQueryLibrary.h"

#include "Crafting/CraftingRecipeDefinition.h"
#include "Inventory/InventoryComponent.h"

namespace
{
FCraftingQueryResult MakeCraftingQueryResult(const ECraftingQueryResult Result, const int32 EntryIndex = INDEX_NONE,
	const EInventoryOperationResult InventoryResult = EInventoryOperationResult::NotInitialized)
{
	FCraftingQueryResult QueryResult;
	QueryResult.Result = Result;
	QueryResult.EntryIndex = EntryIndex;
	QueryResult.InventoryResult = InventoryResult;
	return QueryResult;
}

const UCraftingRecipeDefinition* ResolveRecipe(const TSubclassOf<UCraftingRecipeDefinition> RecipeClass)
{
	const UClass* Class = RecipeClass.Get();
	if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return nullptr;
	}
	return Class->GetDefaultObject<UCraftingRecipeDefinition>();
}

// Shared input checks; returns false with OutResult set when the query cannot run.
bool ValidateQueryInputs(const UInventoryComponent* Inventory, const UCraftingRecipeDefinition* Recipe,
	FCraftingQueryResult& OutResult)
{
	if (!IsValid(Inventory))
	{
		OutResult = MakeCraftingQueryResult(ECraftingQueryResult::InvalidInventory);
		return false;
	}
	FString Diagnostic;
	if (!IsValid(Recipe) || !Recipe->ValidateRecipe(Diagnostic))
	{
		OutResult = MakeCraftingQueryResult(ECraftingQueryResult::InvalidRecipe);
		return false;
	}
	return true;
}
}

FCraftingQueryResult UCraftingQueryLibrary::CheckRecipeIngredients(const UInventoryComponent* Inventory,
	TSubclassOf<UCraftingRecipeDefinition> RecipeClass)
{
	return CheckIngredients(Inventory, ResolveRecipe(RecipeClass));
}

FCraftingQueryResult UCraftingQueryLibrary::CheckRecipeOutputCapacity(const UInventoryComponent* Inventory,
	TSubclassOf<UCraftingRecipeDefinition> RecipeClass)
{
	return CheckOutputCapacity(Inventory, ResolveRecipe(RecipeClass));
}

FCraftingQueryResult UCraftingQueryLibrary::CheckIngredients(const UInventoryComponent* Inventory,
	const UCraftingRecipeDefinition* Recipe)
{
	FCraftingQueryResult Result;
	if (!ValidateQueryInputs(Inventory, Recipe, Result))
	{
		return Result;
	}
	// ValidateRecipe rejects duplicate ingredients, so checking each entry alone covers the recipe total.
	for (int32 Index = 0; Index < Recipe->Ingredients.Num(); ++Index)
	{
		const FCraftingItemQuantity& Ingredient = Recipe->Ingredients[Index];
		const FInventoryOperationResult Query = Inventory->CanRemoveDefinitionQuantity(Ingredient.ItemDefinition, Ingredient.Quantity);
		if (Query.Result == EInventoryOperationResult::InsufficientQuantity)
		{
			return MakeCraftingQueryResult(ECraftingQueryResult::InsufficientIngredients, Index, Query.Result);
		}
		if (!Query.IsSuccess())
		{
			const ECraftingQueryResult Failure = Query.Result == EInventoryOperationResult::NotInitialized
				? ECraftingQueryResult::InvalidInventory
				: ECraftingQueryResult::InvalidRecipe;
			return MakeCraftingQueryResult(Failure, Index, Query.Result);
		}
	}
	return MakeCraftingQueryResult(ECraftingQueryResult::Success, INDEX_NONE, EInventoryOperationResult::Success);
}

FCraftingQueryResult UCraftingQueryLibrary::CheckOutputCapacity(const UInventoryComponent* Inventory,
	const UCraftingRecipeDefinition* Recipe)
{
	FCraftingQueryResult Result;
	if (!ValidateQueryInputs(Inventory, Recipe, Result))
	{
		return Result;
	}
	TArray<FInventoryDefinitionQuantity> Requests;
	Requests.Reserve(Recipe->Outputs.Num());
	for (const FCraftingItemQuantity& Output : Recipe->Outputs)
	{
		FInventoryDefinitionQuantity& Request = Requests.AddDefaulted_GetRef();
		Request.DefinitionClass = Output.ItemDefinition;
		Request.Quantity = Output.Quantity;
	}
	const FInventoryOperationResult Query = Inventory->CanAddDefinitionQuantities(Requests);
	if (Query.IsSuccess())
	{
		return MakeCraftingQueryResult(ECraftingQueryResult::Success, INDEX_NONE, Query.Result);
	}
	ECraftingQueryResult Failure = ECraftingQueryResult::InvalidRecipe;
	if (Query.Result == EInventoryOperationResult::CapacityFull)
	{
		Failure = ECraftingQueryResult::OutputCapacityFull;
	}
	else if (Query.Result == EInventoryOperationResult::NotInitialized)
	{
		Failure = ECraftingQueryResult::InvalidInventory;
	}
	return MakeCraftingQueryResult(Failure, INDEX_NONE, Query.Result);
}
