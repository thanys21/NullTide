#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/CraftingViewLibrary.h"
#include "CraftingRecipeEntryWidget.generated.h"

class UButton;
class UCraftingRecipeDefinition;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCraftingRecipeEntrySelected, TSubclassOf<UCraftingRecipeDefinition>, RecipeClass);

/** One selectable recipe in the crafting panel's recipe list. */
UCLASS(Abstract, Blueprintable)
class NULLTIDE_API UCraftingRecipeEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
	void SetRecipe(const FCraftingRecipeView& InView, bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "Crafting|UI")
	TSubclassOf<UCraftingRecipeDefinition> GetRecipeClass() const { return View.RecipeClass; }

	UFUNCTION(BlueprintPure, Category = "Crafting|UI")
	bool IsSelected() const { return bSelected; }

	UPROPERTY(BlueprintAssignable, Category = "Crafting|UI")
	FOnCraftingRecipeEntrySelected OnRecipeSelected;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> EntryButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> RecipeIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RecipeName;

	/** Short readiness marker ("READY" when CanCraftRecipe succeeds). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReadyText;

private:
	UFUNCTION()
	void HandleEntryClicked();

	FCraftingRecipeView View;
	bool bSelected = false;
};
