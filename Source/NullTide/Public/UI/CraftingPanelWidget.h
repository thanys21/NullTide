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
 * Crafting panel. Observes the owning pawn's (or an explicitly set) UCraftingComponent and its inventory,
 * lists the registered recipes, shows the selected recipe's requirements and exact craft status, and
 * crafts the selected recipe through UCraftingComponent::TryCraftRecipe when CRAFT is clicked. Keeps no
 * crafting or inventory state besides the selected recipe class and the last result message.
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

	/** Binds the panel to this crafting component (for example a workstation's). Null follows the owning pawn again. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
	void SetCraftingSource(UCraftingComponent* InCrafting);

	/**
	 * What the CRAFT button does: crafts the selected recipe once through UCraftingComponent::TryCraftRecipe.
	 * Feedback arrives through OnCraftingResult. Reentrant requests are ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
	void RequestCraftSelectedRecipe();

	/** Clears the last craft result message and shows the normal status again. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
	void ClearCraftFeedback();

	UFUNCTION(BlueprintPure, Category = "Crafting|UI")
	bool HasCraftFeedback() const { return bHasFeedback; }

	UFUNCTION(BlueprintPure, Category = "Crafting|UI")
	FText GetCraftFeedback() const { return bHasFeedback ? FeedbackText : FText::GetEmpty(); }

	UFUNCTION(BlueprintPure, Category = "Crafting|UI")
	bool IsCraftRequestInProgress() const { return bCraftRequestInProgress; }

protected:
	virtual void NativeOnInitialized() override;
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

	/** How long a craft result message stays before the normal status returns (real seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "Crafting|UI", meta = (ClampMin = "0.0", Units = "s"))
	float FeedbackDisplaySeconds = 3.f;

private:
	UFUNCTION()
	void HandleCraftButtonClicked();

	void RebindToCrafting(APawn* Pawn, UCraftingComponent* Crafting);
	void ShowFeedback(const FText& Text, bool bSuccess);
	bool ExpireFeedback();
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

	/** Explicit source from SetCraftingSource; when set, the panel does not follow the owning pawn. */
	UPROPERTY(Transient)
	TObjectPtr<UCraftingComponent> ExplicitCrafting;

	TArray<TSubclassOf<UCraftingRecipeDefinition>> ListedRecipes;
	TSubclassOf<UCraftingRecipeDefinition> SelectedRecipe;
	FCraftingRecipeView SelectedView;

	FText FeedbackText;
	bool bHasFeedback = false;
	bool bFeedbackSuccess = false;
	double FeedbackExpireSeconds = 0.0;
	bool bCraftRequestInProgress = false;
};
