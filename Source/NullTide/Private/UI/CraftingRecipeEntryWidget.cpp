#include "UI/CraftingRecipeEntryWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Crafting/CraftingRecipeDefinition.h"

#define LOCTEXT_NAMESPACE "NullTideCraftingUI"

namespace
{
const FLinearColor SelectedEntryColor(0.26f, 0.38f, 0.24f, 1.f);
const FLinearColor IdleEntryColor(0.095f, 0.105f, 0.097f, 1.f);
}

void UCraftingRecipeEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (EntryButton)
	{
		EntryButton->OnClicked.AddUniqueDynamic(this, &UCraftingRecipeEntryWidget::HandleEntryClicked);
	}
}

void UCraftingRecipeEntryWidget::SetRecipe(const FCraftingRecipeView& InView, const bool bInSelected)
{
	View = InView;
	bSelected = bInSelected;
	if (EntryButton)
	{
		EntryButton->SetBackgroundColor(bSelected ? SelectedEntryColor : IdleEntryColor);
	}
	if (RecipeIcon)
	{
		RecipeIcon->SetBrushFromTexture(View.RecipeIcon);
		RecipeIcon->SetVisibility(View.RecipeIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (RecipeName)
	{
		RecipeName->SetText(View.RecipeName);
	}
	if (ReadyText)
	{
		ReadyText->SetText(View.bCanCraft ? LOCTEXT("EntryReady", "READY") : FText::GetEmpty());
	}
}

void UCraftingRecipeEntryWidget::HandleEntryClicked()
{
	OnRecipeSelected.Broadcast(View.RecipeClass);
}

#undef LOCTEXT_NAMESPACE
