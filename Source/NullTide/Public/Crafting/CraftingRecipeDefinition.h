// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CraftingRecipeDefinition.generated.h"

class UItemDefinition;
class UTexture2D;

/** One static recipe entry: an item definition and the quantity a craft needs or produces. */
USTRUCT(BlueprintType)
struct NULLTIDE_API FCraftingItemQuantity
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting")
	TSubclassOf<UItemDefinition> ItemDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "1"))
	int32 Quantity = 1;
};

/**
 * Static recipe data. Each recipe asset is a data-only Blueprint subclass, referenced by class
 * and read from its default object. Runtime crafting state never lives here.
 */
UCLASS(Blueprintable, BlueprintType, Abstract, Const)
class NULLTIDE_API UCraftingRecipeDefinition : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText RecipeName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText RecipeDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TObjectPtr<UTexture2D> RecipeIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting")
	TArray<FCraftingItemQuantity> Ingredients;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting")
	TArray<FCraftingItemQuantity> Outputs;

	/** Seconds per craft. Zero means instant. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "0.0", Units = "s"))
	float CraftDuration = 0.f;

	/** Reports the first invalid entry as an indexed diagnostic. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	bool ValidateRecipe(FString& OutDiagnostic) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
