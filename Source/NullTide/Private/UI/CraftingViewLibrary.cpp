#include "UI/CraftingViewLibrary.h"

#include "Crafting/CraftingComponent.h"
#include "Crafting/CraftingRecipeDefinition.h"
#include "Crafting/CraftingStationComponent.h"
#include "Crafting/CraftingStationDefinition.h"
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

FText GetStationRequiredText(const FText& StationName)
{
	return StationName.IsEmpty()
		? LOCTEXT("StationRequiredGeneric", "Requires a workstation.")
		: FText::Format(LOCTEXT("StationRequired", "Requires {0}."), StationName);
}

FText GetStationOutOfRangeText(const FText& StationName)
{
	return StationName.IsEmpty()
		? LOCTEXT("StationOutOfRangeGeneric", "Too far from the workstation.")
		: FText::Format(LOCTEXT("StationOutOfRange", "Too far from the {0}."), StationName);
}
}

FCraftingRecipeView UCraftingViewLibrary::BuildRecipeView(const UCraftingComponent* Crafting,
	TSubclassOf<UCraftingRecipeDefinition> RecipeClass)
{
	return BuildRecipeViewAtStation(Crafting, RecipeClass, nullptr);
}

FText UCraftingViewLibrary::GetRequiredStationName(TSubclassOf<UCraftingRecipeDefinition> RecipeClass)
{
	const UClass* Class = RecipeClass.Get();
	const UCraftingRecipeDefinition* Recipe = Class && !Class->HasAnyClassFlags(CLASS_Abstract)
		? Class->GetDefaultObject<UCraftingRecipeDefinition>()
		: nullptr;
	const UClass* StationClass = IsValid(Recipe) ? Recipe->RequiredStation.Get() : nullptr;
	if (!StationClass)
	{
		return FText::GetEmpty();
	}
	const UCraftingStationDefinition* Station = StationClass->GetDefaultObject<UCraftingStationDefinition>();
	return IsValid(Station) && !Station->StationName.IsEmpty() ? Station->StationName : FText::FromString(StationClass->GetName());
}

FCraftingRecipeView UCraftingViewLibrary::BuildRecipeViewAtStation(const UCraftingComponent* Crafting,
	TSubclassOf<UCraftingRecipeDefinition> RecipeClass, UCraftingStationComponent* Station)
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
	View.RequiredStationName = GetRequiredStationName(RecipeClass);

	if (IsValid(Crafting))
	{
		View.CraftResult = Crafting->CanCraftRecipeAtStation(RecipeClass, Station);
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
	case ECraftingTransactionResult::Started:
		return LOCTEXT("StatusStarted", "Crafting...");
	case ECraftingTransactionResult::TimedCraftRequired:
		return LOCTEXT("StatusTimed", "This recipe takes time to craft.");
	case ECraftingTransactionResult::InventoryRejected:
		return LOCTEXT("StatusRejected", "The inventory cannot accept this craft.");
	case ECraftingTransactionResult::StationRequired:
		return GetStationRequiredText(View.RequiredStationName);
	case ECraftingTransactionResult::StationOutOfRange:
		return GetStationOutOfRangeText(View.RequiredStationName);
	case ECraftingTransactionResult::InvalidRecipe:
	default:
		return LOCTEXT("StatusInvalid", "This recipe cannot be crafted.");
	}
}

FText UCraftingViewLibrary::GetCraftFeedbackText(const UCraftingComponent* Crafting,
	TSubclassOf<UCraftingRecipeDefinition> RecipeClass, const FCraftingTransactionResult& Result)
{
	const FCraftingRecipeView View = BuildRecipeView(Crafting, RecipeClass);
	switch (Result.Result)
	{
	case ECraftingTransactionResult::Success:
	{
		TArray<FText> Parts;
		for (const FCraftingRequirementView& Output : View.Outputs)
		{
			Parts.Add(FText::Format(LOCTEXT("FeedbackOutput", "{0} x{1}"), Output.ItemName, FText::AsNumber(Output.RequiredQuantity)));
		}
		return Parts.IsEmpty()
			? LOCTEXT("FeedbackCraftedGeneric", "Crafted.")
			: FText::Format(LOCTEXT("FeedbackCrafted", "Crafted {0}."), FText::Join(LOCTEXT("FeedbackSeparator", ", "), Parts));
	}
	case ECraftingTransactionResult::InsufficientIngredients:
		if (View.Ingredients.IsValidIndex(Result.EntryIndex))
		{
			const FCraftingRequirementView& Missing = View.Ingredients[Result.EntryIndex];
			return FText::Format(LOCTEXT("FeedbackMissing", "Not enough {0} ({1} / {2})."),
				Missing.ItemName, FText::AsNumber(Missing.OwnedQuantity), FText::AsNumber(Missing.RequiredQuantity));
		}
		return LOCTEXT("FeedbackMissingGeneric", "Not enough ingredients.");
	case ECraftingTransactionResult::OutputCapacityFull:
		return LOCTEXT("FeedbackFull", "Not enough inventory space.");
	case ECraftingTransactionResult::InvalidRecipe:
	case ECraftingTransactionResult::RecipeNotAvailable:
		return LOCTEXT("FeedbackUnavailable", "Recipe unavailable.");
	case ECraftingTransactionResult::InventoryBusy:
	case ECraftingTransactionResult::CraftInProgress:
		return LOCTEXT("FeedbackBusy", "Inventory is busy.");
	case ECraftingTransactionResult::InvalidInventory:
		return LOCTEXT("FeedbackNoInventory", "No inventory available.");
	case ECraftingTransactionResult::Started:
		return FText::Format(LOCTEXT("FeedbackStarted", "Crafting {0}..."), View.RecipeName);
	case ECraftingTransactionResult::TimedCraftRequired:
		return LOCTEXT("FeedbackTimed", "This recipe takes time to craft.");
	case ECraftingTransactionResult::StationRequired:
		return GetStationRequiredText(View.RequiredStationName);
	case ECraftingTransactionResult::StationOutOfRange:
		return GetStationOutOfRangeText(View.RequiredStationName);
	case ECraftingTransactionResult::InventoryRejected:
	default:
		return LOCTEXT("FeedbackFailed", "Crafting failed.");
	}
}

FText UCraftingViewLibrary::GetCraftCancelledText(const ECraftCancelReason Reason)
{
	return GetCraftCancelledTextForRecipe(Reason, nullptr);
}

FText UCraftingViewLibrary::GetCraftCancelledTextForRecipe(const ECraftCancelReason Reason,
	TSubclassOf<UCraftingRecipeDefinition> RecipeClass)
{
	switch (Reason)
	{
	case ECraftCancelReason::Movement:
		return LOCTEXT("CancelledMovement", "Crafting cancelled: you moved.");
	case ECraftCancelReason::Jump:
		return LOCTEXT("CancelledJump", "Crafting cancelled: you jumped.");
	case ECraftCancelReason::StationLost:
	{
		const FText StationName = GetRequiredStationName(RecipeClass);
		return StationName.IsEmpty()
			? LOCTEXT("CancelledStationLostGeneric", "Crafting cancelled: the workstation is gone.")
			: FText::Format(LOCTEXT("CancelledStationLost", "Crafting cancelled: the {0} is gone."), StationName);
	}
	case ECraftCancelReason::Explicit:
	default:
		return LOCTEXT("CancelledExplicit", "Crafting cancelled.");
	}
}

FText UCraftingViewLibrary::GetActiveCraftStatusText(const UCraftingComponent* Crafting)
{
	if (!IsValid(Crafting) || !Crafting->IsCrafting())
	{
		return FText::GetEmpty();
	}
	const FCraftingRecipeView View = BuildRecipeView(Crafting, Crafting->GetActiveCraftRecipe());
	FNumberFormattingOptions OneDecimal;
	OneDecimal.MinimumFractionalDigits = 1;
	OneDecimal.MaximumFractionalDigits = 1;
	return FText::Format(LOCTEXT("StatusCraftingTimed", "Crafting {0}... {1} s"),
		View.RecipeName, FText::AsNumber(Crafting->GetActiveCraftRemainingSeconds(), &OneDecimal));
}

#undef LOCTEXT_NAMESPACE
