#include "UI/CraftingViewLibrary.h"

#include "Crafting/CraftingComponent.h"
#include "Crafting/CraftingRecipeDefinition.h"
#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"

#define LOCTEXT_NAMESPACE "NullTideCraftingUI"

namespace
{
FCraftingRequirementView MakeRequirementView(const FCraftingItemQuantity& Entry, const UInventoryComponent* Inventory,
	const bool bIsOutput)
{
	FCraftingRequirementView View;
	View.ItemDefinition = Entry.ItemDefinition;
	View.RequiredQuantity = Entry.Quantity;
	if (const UItemDefinition* Definition = Entry.ItemDefinition ? Entry.ItemDefinition->GetDefaultObject<UItemDefinition>() : nullptr)
	{
		View.ItemName = Definition->ItemName.IsEmpty()
			? FText::FromString(Entry.ItemDefinition->GetName())
			: Definition->ItemName;
		View.ItemIcon = Definition->ItemIcon;
	}
	View.OwnedQuantity = Inventory ? Inventory->GetTotalQuantity(Entry.ItemDefinition) : 0;
	View.bSatisfied = bIsOutput || View.OwnedQuantity >= View.RequiredQuantity;
	return View;
}
}

FCraftingRecipeView UCraftingViewLibrary::BuildRecipeView(const UCraftingComponent* Crafting,
	TSubclassOf<UCraftingRecipeDefinition> RecipeClass)
{
	FCraftingRecipeView View;
	View.RecipeClass = RecipeClass;
	const UClass* Class = RecipeClass.Get();
	const UCraftingRecipeDefinition* Recipe = Class && !Class->HasAnyClassFlags(CLASS_Abstract)
		? Class->GetDefaultObject<UCraftingRecipeDefinition>()
		: nullptr;
	if (!IsValid(Recipe))
	{
		View.CraftResult.Result = ECraftingTransactionResult::InvalidRecipe;
		View.StatusText = GetCraftStatusText(View);
		return View;
	}

	const UInventoryComponent* Inventory = IsValid(Crafting) ? Crafting->GetInventory() : nullptr;
	View.bIsValid = true;
	View.RecipeName = Recipe->RecipeName.IsEmpty() ? FText::FromString(Class->GetName()) : Recipe->RecipeName;
	View.RecipeDescription = Recipe->RecipeDescription;
	for (const FCraftingItemQuantity& Ingredient : Recipe->Ingredients)
	{
		View.Ingredients.Add(MakeRequirementView(Ingredient, Inventory, false));
	}
	for (const FCraftingItemQuantity& Output : Recipe->Outputs)
	{
		View.Outputs.Add(MakeRequirementView(Output, Inventory, true));
	}
	View.RecipeIcon = Recipe->RecipeIcon ? Recipe->RecipeIcon.Get() : (View.Outputs.IsEmpty() ? nullptr : View.Outputs[0].ItemIcon.Get());

	if (IsValid(Crafting))
	{
		View.CraftResult = Crafting->CanCraftRecipe(RecipeClass);
	}
	else
	{
		View.CraftResult.Result = ECraftingTransactionResult::InvalidInventory;
	}
	View.bCanCraft = View.CraftResult.IsSuccess();
	View.StatusText = GetCraftStatusText(View);
	return View;
}

FText UCraftingViewLibrary::GetCraftStatusText(const FCraftingRecipeView& View)
{
	switch (View.CraftResult.Result)
	{
	case ECraftingTransactionResult::Success:
		return LOCTEXT("StatusReady", "Ready to craft.");
	case ECraftingTransactionResult::InsufficientIngredients:
		if (View.Ingredients.IsValidIndex(View.CraftResult.EntryIndex))
		{
			const FCraftingRequirementView& Missing = View.Ingredients[View.CraftResult.EntryIndex];
			return FText::Format(LOCTEXT("StatusMissingNamed", "Missing {0} ({1} / {2})."),
				Missing.ItemName, FText::AsNumber(Missing.OwnedQuantity), FText::AsNumber(Missing.RequiredQuantity));
		}
		return LOCTEXT("StatusMissing", "Missing ingredients.");
	case ECraftingTransactionResult::OutputCapacityFull:
		return LOCTEXT("StatusFull", "Not enough inventory space for the output.");
	case ECraftingTransactionResult::InvalidInventory:
		return LOCTEXT("StatusNoInventory", "No inventory available.");
	case ECraftingTransactionResult::RecipeNotAvailable:
		return LOCTEXT("StatusUnavailable", "This recipe is not available.");
	case ECraftingTransactionResult::InventoryBusy:
	case ECraftingTransactionResult::CraftInProgress:
		return LOCTEXT("StatusBusy", "Busy. Try again.");
	case ECraftingTransactionResult::InventoryRejected:
		return LOCTEXT("StatusRejected", "The inventory cannot accept this craft.");
	case ECraftingTransactionResult::InvalidRecipe:
	default:
		return LOCTEXT("StatusInvalid", "This recipe cannot be crafted.");
	}
}

#undef LOCTEXT_NAMESPACE
