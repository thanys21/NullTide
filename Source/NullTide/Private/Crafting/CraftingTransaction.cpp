// Fill out your copyright notice in the Description page of Project Settings.


#include "Crafting/CraftingTransaction.h"

#include "Crafting/CraftingRecipeDefinition.h"
#include "Inventory/InventoryComponent.h"

namespace
{
FCraftingTransactionResult MakeCraftingTransactionResult(const ECraftingTransactionResult Result,
	const int32 EntryIndex = INDEX_NONE,
	const EInventoryOperationResult InventoryResult = EInventoryOperationResult::NotInitialized,
	const FString& Diagnostic = FString())
{
	FCraftingTransactionResult TransactionResult;
	TransactionResult.Result = Result;
	TransactionResult.EntryIndex = EntryIndex;
	TransactionResult.InventoryResult = InventoryResult;
	TransactionResult.Diagnostic = Diagnostic;
	return TransactionResult;
}

void AppendRequests(const TArray<FCraftingItemQuantity>& Entries, TArray<FInventoryDefinitionQuantity>& OutRequests)
{
	OutRequests.Reserve(Entries.Num());
	for (const FCraftingItemQuantity& Entry : Entries)
	{
		FInventoryDefinitionQuantity& Request = OutRequests.AddDefaulted_GetRef();
		Request.DefinitionClass = Entry.ItemDefinition;
		Request.Quantity = Entry.Quantity;
	}
}
}

FCraftingTransactionResult FCraftingTransaction::TryCraftRecipeAtomic(UInventoryComponent* Inventory,
	TSubclassOf<UCraftingRecipeDefinition> RecipeClass)
{
	const UClass* Class = RecipeClass.Get();
	if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return MakeCraftingTransactionResult(ECraftingTransactionResult::InvalidRecipe, INDEX_NONE,
			EInventoryOperationResult::NotInitialized, TEXT("Recipe class is null or abstract."));
	}
	return TryCraftRecipeAtomic(Inventory, Class->GetDefaultObject<UCraftingRecipeDefinition>());
}

FCraftingTransactionResult FCraftingTransaction::TryCraftRecipeAtomic(UInventoryComponent* Inventory,
	const UCraftingRecipeDefinition* Recipe)
{
	if (!IsValid(Inventory))
	{
		return MakeCraftingTransactionResult(ECraftingTransactionResult::InvalidInventory);
	}
	FString Diagnostic;
	if (!IsValid(Recipe) || !Recipe->ValidateRecipe(Diagnostic))
	{
		return MakeCraftingTransactionResult(ECraftingTransactionResult::InvalidRecipe, INDEX_NONE,
			EInventoryOperationResult::NotInitialized, IsValid(Recipe) ? Diagnostic : FString(TEXT("Recipe is null.")));
	}

	// ValidateRecipe rejects duplicate entries, so request indices equal recipe entry indices.
	// The inventory primitive still sums duplicates itself for generic callers.
	TArray<FInventoryDefinitionQuantity> Removals;
	TArray<FInventoryDefinitionQuantity> Additions;
	AppendRequests(Recipe->Ingredients, Removals);
	AppendRequests(Recipe->Outputs, Additions);

	const FInventoryTransactionResult Transaction = Inventory->TryApplyDefinitionTransaction(Removals, Additions);
	switch (Transaction.Result)
	{
	case EInventoryOperationResult::Success:
		return MakeCraftingTransactionResult(ECraftingTransactionResult::Success, INDEX_NONE, Transaction.Result);
	case EInventoryOperationResult::InsufficientQuantity:
		return MakeCraftingTransactionResult(ECraftingTransactionResult::InsufficientIngredients,
			Transaction.FailedIndex, Transaction.Result, TEXT("An ingredient is short after summing all its stacks."));
	case EInventoryOperationResult::CapacityFull:
		return MakeCraftingTransactionResult(ECraftingTransactionResult::OutputCapacityFull,
			Transaction.FailedIndex, Transaction.Result, TEXT("Outputs do not fit after the ingredients are consumed."));
	case EInventoryOperationResult::NotInitialized:
		return MakeCraftingTransactionResult(ECraftingTransactionResult::InvalidInventory,
			INDEX_NONE, Transaction.Result, TEXT("Inventory native authority is not active."));
	case EInventoryOperationResult::Busy:
		return MakeCraftingTransactionResult(ECraftingTransactionResult::InventoryBusy,
			INDEX_NONE, Transaction.Result, TEXT("Inventory is already applying a mutation."));
	default:
		return MakeCraftingTransactionResult(ECraftingTransactionResult::InventoryRejected,
			Transaction.FailedIndex, Transaction.Result, TEXT("Inventory rejected a craft request."));
	}
}
