#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/CraftingViewLibrary.h"
#include "CraftingPanelWidget.generated.h"

class APawn;
class UButton;
class UCraftingComponent;
class UCraftingRecipeDefinition;
class UCraftingRecipeEntryWidget;
class UCraftingRequirementRowWidget;
class UImage;
class UInventoryComponent;
class UPanelWidget;
class UTextBlock;

/**
 * Read-only crafting panel. Observes the owning pawn's UCraftingComponent and inventory, lists the
 * registered recipes, and shows the selected recipe's requirements and exact craft status. Keeps no
 * crafting or inventory state besides the selected recipe class. C6A: the CRAFT button only reflects
 * CanCraftRecipe; it does not craft.
 */
UCLASS(Abstract, Blueprintable)
class NULLTIDE_API UCraftingPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
	void RefreshCraftingView();

	/** Selects a registered recipe; unknown classes fall back to the first registered recipe. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
	void SelectRecipe(TSubclassOf<UCraftingRecipeDefinition> RecipeClass);

	UFUNCTION(BlueprintPure, Category = "Crafting|UI")
	TSubclassOf<UCraftingRecipeDefinition> GetSelectedRecipe() const { return SelectedRecipe; }

	UFUNCTION(BlueprintPure, Category = "Crafting|UI")
	FCraftingRecipeView GetSelectedRecipeView() const { return SelectedView; }

	UFUNCTION(BlueprintPure, Category = "Crafting|UI")
	UCraftingComponent* GetObservedCrafting() const { return ObservedCrafting; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Container for recipe entries (for example a VerticalBox inside a ScrollBox). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> RecipeList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SelectedRecipeIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SelectedRecipeName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SelectedRecipeDescription;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> IngredientList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> OutputList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CraftStatusText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CraftButton;

	UPROPERTY(EditDefaultsOnly, Category = "Crafting|UI")
	TSubclassOf<UCraftingRecipeEntryWidget> RecipeEntryClass;

	UPROPERTY(EditDefaultsOnly, Category = "Crafting|UI")
	TSubclassOf<UCraftingRequirementRowWidget> RequirementRowClass;

private:
	UFUNCTION()
	void HandleInventoryChanged(int32 NewRevision);

	UFUNCTION()
	void HandleCraftingResult(TSubclassOf<UCraftingRecipeDefinition> RecipeClass, const FCraftingTransactionResult& Result);

	UFUNCTION()
	void HandleRecipeEntrySelected(TSubclassOf<UCraftingRecipeDefinition> RecipeClass);

	void RebindToPawn(APawn* Pawn);
	void ReleaseBindings();
	void RebuildRecipeEntries(const TArray<TSubclassOf<UCraftingRecipeDefinition>>& Recipes);
	void RefreshDetails();
	void FillRequirementList(UPanelWidget* List, const TArray<FCraftingRequirementView>& Requirements, bool bIsOutput);

	UPROPERTY(Transient)
	TObjectPtr<APawn> ObservedPawn;

	UPROPERTY(Transient)
	TObjectPtr<UCraftingComponent> ObservedCrafting;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> ObservedInventory;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCraftingRecipeEntryWidget>> RecipeEntries;

	TArray<TSubclassOf<UCraftingRecipeDefinition>> ListedRecipes;
	TSubclassOf<UCraftingRecipeDefinition> SelectedRecipe;
	FCraftingRecipeView SelectedView;
};
