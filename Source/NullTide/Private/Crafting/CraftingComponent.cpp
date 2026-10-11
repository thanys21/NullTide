// Fill out your copyright notice in the Description page of Project Settings.


#include "Crafting/CraftingComponent.h"

#include "Crafting/CraftingRecipeDefinition.h"
#include "Inventory/InventoryComponent.h"
#include "GameFramework/Actor.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
FCraftingTransactionResult MakeCraftingRequestResult(const ECraftingTransactionResult Result, const FString& Diagnostic)
{
	FCraftingTransactionResult RequestResult;
	RequestResult.Result = Result;
	RequestResult.Diagnostic = Diagnostic;
	return RequestResult;
}

/** Returns the recipe default object when the class is concrete and its data is valid. */
const UCraftingRecipeDefinition* ResolveValidRecipe(const TSubclassOf<UCraftingRecipeDefinition> RecipeClass, FString& OutDiagnostic)
{
	const UClass* Class = RecipeClass.Get();
	if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		OutDiagnostic = TEXT("Recipe class is null or abstract.");
		return nullptr;
	}
	const UCraftingRecipeDefinition* Recipe = Class->GetDefaultObject<UCraftingRecipeDefinition>();
	if (!IsValid(Recipe) || !Recipe->ValidateRecipe(OutDiagnostic))
	{
		return nullptr;
	}
	return Recipe;
}
}

UCraftingComponent::UCraftingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

TArray<TSubclassOf<UCraftingRecipeDefinition>> UCraftingComponent::GetAvailableRecipes() const
{
	TArray<TSubclassOf<UCraftingRecipeDefinition>> Recipes;
	for (const TSubclassOf<UCraftingRecipeDefinition>& RecipeClass : AvailableRecipes)
	{
		FString Diagnostic;
		if (!Recipes.Contains(RecipeClass) && ResolveValidRecipe(RecipeClass, Diagnostic))
		{
			Recipes.Add(RecipeClass);
		}
	}
	return Recipes;
}

bool UCraftingComponent::IsRecipeAvailable(TSubclassOf<UCraftingRecipeDefinition> RecipeClass) const
{
	FString Diagnostic;
	return RecipeClass && AvailableRecipes.Contains(RecipeClass) && ResolveValidRecipe(RecipeClass, Diagnostic);
}

UInventoryComponent* UCraftingComponent::GetInventory() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
}

bool UCraftingComponent::ValidateRequest(
	TSubclassOf<UCraftingRecipeDefinition> RecipeClass,
	const UCraftingRecipeDefinition*& OutRecipe,
	UInventoryComponent*& OutInventory,
	FCraftingTransactionResult& OutFailure) const
{
	FString Diagnostic;
	OutRecipe = ResolveValidRecipe(RecipeClass, Diagnostic);
	if (!OutRecipe)
	{
		OutFailure = MakeCraftingRequestResult(ECraftingTransactionResult::InvalidRecipe, Diagnostic);
		return false;
	}
	if (!AvailableRecipes.Contains(RecipeClass))
	{
		OutFailure = MakeCraftingRequestResult(ECraftingTransactionResult::RecipeNotAvailable,
			FString::Printf(TEXT("%s is not registered on this crafting component."), *GetNameSafe(RecipeClass.Get())));
		return false;
	}
	OutInventory = GetInventory();
	if (!IsValid(OutInventory))
	{
		OutFailure = MakeCraftingRequestResult(ECraftingTransactionResult::InvalidInventory,
			TEXT("The crafting component's owner has no inventory component."));
		return false;
	}
	return true;
}

FCraftingTransactionResult UCraftingComponent::CanCraftRecipe(TSubclassOf<UCraftingRecipeDefinition> RecipeClass) const
{
	const UCraftingRecipeDefinition* Recipe = nullptr;
	UInventoryComponent* Inventory = nullptr;
	FCraftingTransactionResult Failure;
	if (!ValidateRequest(RecipeClass, Recipe, Inventory, Failure))
	{
		return Failure;
	}
	return FCraftingTransaction::CanCraftRecipe(Inventory, Recipe);
}

FCraftingTransactionResult UCraftingComponent::TryCraftRecipe(TSubclassOf<UCraftingRecipeDefinition> RecipeClass)
{
	// Reentrant calls (from inventory or crafting notifications of this craft) neither mutate nor notify.
	if (bCraftInProgress)
	{
		return MakeCraftingRequestResult(ECraftingTransactionResult::CraftInProgress,
			TEXT("A craft is already in progress on this crafting component."));
	}
	TGuardValue<bool> CraftGuard(bCraftInProgress, true);

	const UCraftingRecipeDefinition* Recipe = nullptr;
	UInventoryComponent* Inventory = nullptr;
	FCraftingTransactionResult Result;
	if (ValidateRequest(RecipeClass, Recipe, Inventory, Result))
	{
		Result = FCraftingTransaction::TryCraftRecipeAtomic(Inventory, Recipe);
	}
	OnCraftingResult.Broadcast(RecipeClass, Result);
	return Result;
}

#if WITH_EDITOR
EDataValidationResult UCraftingComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	for (int32 Index = 0; Index < AvailableRecipes.Num(); ++Index)
	{
		FString Diagnostic;
		if (!ResolveValidRecipe(AvailableRecipes[Index], Diagnostic))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("AvailableRecipes[%d]: %s"), Index, *Diagnostic)));
			Result = EDataValidationResult::Invalid;
		}
		else if (AvailableRecipes.IndexOfByKey(AvailableRecipes[Index]) != Index)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("AvailableRecipes[%d] duplicates %s."), Index,
				*GetNameSafe(AvailableRecipes[Index].Get()))));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif
