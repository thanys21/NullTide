#pragma once

#include "CoreMinimal.h"
#include "Crafting/CraftingTransaction.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CraftingViewLibrary.generated.h"

class UCraftingComponent;
class UCraftingRecipeDefinition;
class UItemDefinition;
class UTexture2D;

/** Display data for one ingredient or output line. */
USTRUCT(BlueprintType)
struct NULLTIDE_API FCraftingRequirementView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crafting|UI")
	TSubclassOf<UItemDefinition> ItemDefinition;

	UPROPERTY(BlueprintReadOnly, Category = "Crafting|UI")
	FText ItemName;

	UPROPERTY(BlueprintReadOnly, Category = "Crafting|UI")
	TObjectPtr<UTexture2D> ItemIcon = nullptr;

	/** Total quantity of this definition in the inventory now. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|UI")
	int32 OwnedQuantity = 0;

	/** Ingredient: quantity consumed. Output: quantity produced. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|UI")
	int32 RequiredQuantity = 0;

	/** Ingredient: owned covers required. Always true for outputs. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|UI")
	bool bSatisfied = false;
};

/** Read-only display data for one recipe, built from the crafting component and its inventory. */
USTRUCT(BlueprintType)
struct NULLTIDE_API FCraftingRecipeView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crafting|UI")
	TSubclassOf<UCraftingRecipeDefinition> RecipeClass;

	/** False when the recipe class or its data cannot be shown. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|UI")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Crafting|UI")
	FText RecipeName;

	UPROPERTY(BlueprintReadOnly, Category = "Crafting|UI")
	FText RecipeDescription;

	/** Recipe icon, or the first output's item icon when the recipe has none. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|UI")
	TObjectPtr<UTexture2D> RecipeIcon = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Crafting|UI")
	TArray<FCraftingRequirementView> Ingredients;

	UPROPERTY(BlueprintReadOnly, Category = "Crafting|UI")
	TArray<FCraftingRequirementView> Outputs;

	/** Exact UCraftingComponent::CanCraftRecipe result. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|UI")
	FCraftingTransactionResult CraftResult;

	UPROPERTY(BlueprintReadOnly, Category = "Crafting|UI")
	bool bCanCraft = false;

	/** Player-facing status line for CraftResult. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|UI")
	FText StatusText;
};

/** Builds crafting display data. Reads only; never crafts or changes inventory. */
UCLASS()
class NULLTIDE_API UCraftingViewLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Crafting|UI")
	static FCraftingRecipeView BuildRecipeView(const UCraftingComponent* Crafting, TSubclassOf<UCraftingRecipeDefinition> RecipeClass);

	/** Status line for a craft result; names the missing ingredient when the view has it. */
	UFUNCTION(BlueprintPure, Category = "Crafting|UI")
	static FText GetCraftStatusText(const FCraftingRecipeView& View);
};
