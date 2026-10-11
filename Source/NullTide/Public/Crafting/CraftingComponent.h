// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Crafting/CraftingTransaction.h"
#include "CraftingComponent.generated.h"

class UCraftingRecipeDefinition;
class UInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCraftingResult,
	TSubclassOf<UCraftingRecipeDefinition>, RecipeClass, const FCraftingTransactionResult&, Result);

/**
 * The player's crafting authority. Holds the registered recipes and crafts them against the owner's
 * UInventoryComponent through one atomic inventory transaction. Stores no inventory contents.
 */
UCLASS(ClassGroup = (Crafting), meta = (BlueprintSpawnableComponent))
class NULLTIDE_API UCraftingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCraftingComponent();

	/** Registered recipes in authored order, without null, abstract, invalid or duplicate entries. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	TArray<TSubclassOf<UCraftingRecipeDefinition>> GetAvailableRecipes() const;

	UFUNCTION(BlueprintPure, Category = "Crafting")
	bool IsRecipeAvailable(TSubclassOf<UCraftingRecipeDefinition> RecipeClass) const;

	/** Exact read-only preflight: the result TryCraftRecipe would return now, without changing anything. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	FCraftingTransactionResult CanCraftRecipe(TSubclassOf<UCraftingRecipeDefinition> RecipeClass) const;

	/** Crafts one registered recipe atomically. Broadcasts OnCraftingResult once unless the call is reentrant. */
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	FCraftingTransactionResult TryCraftRecipe(TSubclassOf<UCraftingRecipeDefinition> RecipeClass);

	/** The owner's inventory authority, or null when the owner has none. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	UInventoryComponent* GetInventory() const;

	/** Fires after every non-reentrant TryCraftRecipe call, for success and for failure. */
	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FOnCraftingResult OnCraftingResult;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	/** Validates the request up to the inventory; returns false with OutFailure set when it cannot proceed. */
	bool ValidateRequest(TSubclassOf<UCraftingRecipeDefinition> RecipeClass, const UCraftingRecipeDefinition*& OutRecipe,
		UInventoryComponent*& OutInventory, FCraftingTransactionResult& OutFailure) const;

	/** Authored recipe list. Order is the listing order; only these recipes can be crafted. */
	UPROPERTY(EditDefaultsOnly, Category = "Crafting", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<UCraftingRecipeDefinition>> AvailableRecipes;

	bool bCraftInProgress = false;
};
