#include "UI/CraftingPanelWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Crafting/CraftingComponent.h"
#include "Crafting/CraftingRecipeDefinition.h"
#include "GameFramework/Pawn.h"
#include "Inventory/InventoryComponent.h"
#include "UI/CraftingRecipeEntryWidget.h"
#include "UI/CraftingRequirementRowWidget.h"

#define LOCTEXT_NAMESPACE "NullTideCraftingUI"

void UCraftingPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RebindToPawn(GetOwningPlayerPawn());
}

void UCraftingPanelWidget::NativeDestruct()
{
	ReleaseBindings();
	ObservedPawn = nullptr;
	Super::NativeDestruct();
}

void UCraftingPanelWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	APawn* CurrentPawn = GetOwningPlayerPawn();
	UCraftingComponent* CurrentCrafting = CurrentPawn ? CurrentPawn->FindComponentByClass<UCraftingComponent>() : nullptr;
	UInventoryComponent* CurrentInventory = CurrentCrafting ? CurrentCrafting->GetInventory() : nullptr;
	if (CurrentPawn != ObservedPawn || CurrentCrafting != ObservedCrafting || CurrentInventory != ObservedInventory)
	{
		RebindToPawn(CurrentPawn);
	}
}

void UCraftingPanelWidget::RebindToPawn(APawn* Pawn)
{
	ReleaseBindings();
	ObservedPawn = Pawn;
	ObservedCrafting = Pawn ? Pawn->FindComponentByClass<UCraftingComponent>() : nullptr;
	ObservedInventory = ObservedCrafting ? ObservedCrafting->GetInventory() : nullptr;
	if (ObservedInventory)
	{
		ObservedInventory->OnInventoryChanged.AddUniqueDynamic(this, &UCraftingPanelWidget::HandleInventoryChanged);
	}
	if (ObservedCrafting)
	{
		ObservedCrafting->OnCraftingResult.AddUniqueDynamic(this, &UCraftingPanelWidget::HandleCraftingResult);
	}
	RefreshCraftingView();
}

void UCraftingPanelWidget::ReleaseBindings()
{
	if (ObservedInventory)
	{
		ObservedInventory->OnInventoryChanged.RemoveDynamic(this, &UCraftingPanelWidget::HandleInventoryChanged);
	}
	if (ObservedCrafting)
	{
		ObservedCrafting->OnCraftingResult.RemoveDynamic(this, &UCraftingPanelWidget::HandleCraftingResult);
	}
	ObservedInventory = nullptr;
	ObservedCrafting = nullptr;
}

void UCraftingPanelWidget::HandleInventoryChanged(int32 NewRevision)
{
	RefreshCraftingView();
}

void UCraftingPanelWidget::HandleCraftingResult(TSubclassOf<UCraftingRecipeDefinition> RecipeClass,
	const FCraftingTransactionResult& Result)
{
	RefreshCraftingView();
}

void UCraftingPanelWidget::HandleRecipeEntrySelected(TSubclassOf<UCraftingRecipeDefinition> RecipeClass)
{
	SelectRecipe(RecipeClass);
}

void UCraftingPanelWidget::SelectRecipe(TSubclassOf<UCraftingRecipeDefinition> RecipeClass)
{
	SelectedRecipe = RecipeClass;
	RefreshCraftingView();
}

void UCraftingPanelWidget::RefreshCraftingView()
{
	const TArray<TSubclassOf<UCraftingRecipeDefinition>> Recipes = ObservedCrafting
		? ObservedCrafting->GetAvailableRecipes()
		: TArray<TSubclassOf<UCraftingRecipeDefinition>>();
	if (!Recipes.Contains(SelectedRecipe))
	{
		SelectedRecipe = Recipes.IsEmpty() ? nullptr : Recipes[0];
	}
	// Entries are rebuilt only when the registered set changes, never from inside an entry's click.
	if (Recipes != ListedRecipes)
	{
		RebuildRecipeEntries(Recipes);
	}
	for (UCraftingRecipeEntryWidget* Entry : RecipeEntries)
	{
		if (Entry)
		{
			Entry->SetRecipe(UCraftingViewLibrary::BuildRecipeView(ObservedCrafting, Entry->GetRecipeClass()),
				Entry->GetRecipeClass() == SelectedRecipe);
		}
	}
	RefreshDetails();
}

void UCraftingPanelWidget::RebuildRecipeEntries(const TArray<TSubclassOf<UCraftingRecipeDefinition>>& Recipes)
{
	for (UCraftingRecipeEntryWidget* Entry : RecipeEntries)
	{
		if (Entry)
		{
			Entry->OnRecipeSelected.RemoveDynamic(this, &UCraftingPanelWidget::HandleRecipeEntrySelected);
		}
	}
	RecipeEntries.Reset();
	ListedRecipes = Recipes;
	if (RecipeList)
	{
		RecipeList->ClearChildren();
	}
	if (!RecipeList || !RecipeEntryClass)
	{
		return;
	}
	for (const TSubclassOf<UCraftingRecipeDefinition>& RecipeClass : Recipes)
	{
		UCraftingRecipeEntryWidget* Entry = CreateWidget<UCraftingRecipeEntryWidget>(this, RecipeEntryClass);
		if (!Entry)
		{
			continue;
		}
		// Seed the entry with its recipe so it knows its class before the first refresh.
		Entry->SetRecipe(UCraftingViewLibrary::BuildRecipeView(ObservedCrafting, RecipeClass), RecipeClass == SelectedRecipe);
		Entry->OnRecipeSelected.AddUniqueDynamic(this, &UCraftingPanelWidget::HandleRecipeEntrySelected);
		RecipeList->AddChild(Entry);
		RecipeEntries.Add(Entry);
	}
}

void UCraftingPanelWidget::RefreshDetails()
{
	SelectedView = SelectedRecipe
		? UCraftingViewLibrary::BuildRecipeView(ObservedCrafting, SelectedRecipe)
		: FCraftingRecipeView();
	const bool bHasRecipe = SelectedView.bIsValid;
	if (SelectedRecipeIcon)
	{
		SelectedRecipeIcon->SetBrushFromTexture(SelectedView.RecipeIcon);
		SelectedRecipeIcon->SetVisibility(SelectedView.RecipeIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (SelectedRecipeName)
	{
		SelectedRecipeName->SetText(bHasRecipe ? SelectedView.RecipeName : LOCTEXT("NoRecipe", "No recipes available"));
	}
	if (SelectedRecipeDescription)
	{
		SelectedRecipeDescription->SetText(bHasRecipe ? SelectedView.RecipeDescription : FText::GetEmpty());
	}
	FillRequirementList(IngredientList, SelectedView.Ingredients, false);
	FillRequirementList(OutputList, SelectedView.Outputs, true);
	if (CraftStatusText)
	{
		CraftStatusText->SetText(bHasRecipe ? SelectedView.StatusText : FText::GetEmpty());
	}
	if (CraftButton)
	{
		CraftButton->SetIsEnabled(bHasRecipe && SelectedView.bCanCraft);
	}
}

void UCraftingPanelWidget::FillRequirementList(UPanelWidget* List, const TArray<FCraftingRequirementView>& Requirements,
	const bool bIsOutput)
{
	if (!List)
	{
		return;
	}
	List->ClearChildren();
	if (!RequirementRowClass)
	{
		return;
	}
	for (const FCraftingRequirementView& Requirement : Requirements)
	{
		if (UCraftingRequirementRowWidget* Row = CreateWidget<UCraftingRequirementRowWidget>(this, RequirementRowClass))
		{
			Row->SetRequirement(Requirement, bIsOutput);
			List->AddChild(Row);
		}
	}
}

#undef LOCTEXT_NAMESPACE
