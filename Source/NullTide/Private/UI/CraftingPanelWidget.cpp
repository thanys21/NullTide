#include "UI/CraftingPanelWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Crafting/CraftingComponent.h"
#include "Crafting/CraftingRecipeDefinition.h"
#include "Crafting/CraftingStationComponent.h"
#include "Crafting/CraftingStationDefinition.h"
#include "GameFramework/Pawn.h"
#include "Inventory/InventoryComponent.h"
#include "UI/CraftingRecipeEntryWidget.h"
#include "UI/CraftingRequirementRowWidget.h"

#define LOCTEXT_NAMESPACE "NullTideCraftingUI"

namespace
{
const FLinearColor StatusTextColor(0.72f, 0.74f, 0.70f, 1.f);
const FLinearColor FeedbackSuccessColor(0.55f, 0.85f, 0.45f, 1.f);
const FLinearColor FeedbackFailureColor(0.95f, 0.42f, 0.36f, 1.f);
}

void UCraftingPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Bound once per widget instance; reconstruction does not add a second binding.
	if (CraftButton)
	{
		CraftButton->OnClicked.AddUniqueDynamic(this, &UCraftingPanelWidget::HandleCraftButtonClicked);
	}
}

void UCraftingPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ExplicitCrafting)
	{
		RebindToCrafting(nullptr, ExplicitCrafting);
	}
	else
	{
		RebindToPawn(GetOwningPlayerPawn());
	}
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
	if (ExpireFeedback())
	{
		RefreshDetails();
	}
	if (ObservedCrafting && ObservedCrafting->IsCrafting())
	{
		RefreshCraftProgress();
	}
	if (IsStationContextLost() && !ListedRecipes.IsEmpty())
	{
		// The station went away under an open panel: stop offering its recipes.
		RefreshCraftingView();
	}
	if (ExplicitCrafting)
	{
		if (ExplicitCrafting != ObservedCrafting || ExplicitCrafting->GetInventory() != ObservedInventory)
		{
			RebindToCrafting(nullptr, ExplicitCrafting);
		}
		return;
	}
	APawn* CurrentPawn = GetOwningPlayerPawn();
	UCraftingComponent* CurrentCrafting = CurrentPawn ? CurrentPawn->FindComponentByClass<UCraftingComponent>() : nullptr;
	UInventoryComponent* CurrentInventory = CurrentCrafting ? CurrentCrafting->GetInventory() : nullptr;
	if (CurrentPawn != ObservedPawn || CurrentCrafting != ObservedCrafting || CurrentInventory != ObservedInventory)
	{
		RebindToPawn(CurrentPawn);
	}
}

void UCraftingPanelWidget::SetCraftingSource(UCraftingComponent* InCrafting)
{
	ExplicitCrafting = InCrafting;
	if (ExplicitCrafting)
	{
		RebindToCrafting(nullptr, ExplicitCrafting);
	}
	else
	{
		RebindToPawn(GetOwningPlayerPawn());
	}
}

void UCraftingPanelWidget::SetCraftingStation(UCraftingStationComponent* InStation)
{
	UCraftingStationComponent* Station = ::IsValid(InStation) ? InStation : nullptr;
	if (bHasStationContext != (Station != nullptr) || StationContext.Get() != Station)
	{
		// A different context starts fresh: no previous message, and the selection becomes the active craft when it
		// is listed, otherwise the first listed recipe (a station's own recipe comes first).
		bHasFeedback = false;
		FeedbackText = FText::GetEmpty();
		SelectedRecipe = nullptr;
	}
	StationContext = Station;
	bHasStationContext = Station != nullptr;
	RefreshCraftingView();
}

UCraftingStationComponent* UCraftingPanelWidget::GetCraftingStation() const
{
	UCraftingStationComponent* Station = StationContext.Get();
	return ::IsValid(Station) && Station->IsStationUsable() ? Station : nullptr;
}

bool UCraftingPanelWidget::IsStationContextLost() const
{
	return bHasStationContext && !GetCraftingStation();
}

FCraftingRecipeView UCraftingPanelWidget::BuildView(TSubclassOf<UCraftingRecipeDefinition> RecipeClass) const
{
	return UCraftingViewLibrary::BuildRecipeViewAtStation(ObservedCrafting, RecipeClass, GetCraftingStation());
}

void UCraftingPanelWidget::RefreshTitle()
{
	if (!PanelTitle)
	{
		return;
	}
	const UCraftingStationComponent* Station = GetCraftingStation();
	const UCraftingStationDefinition* Definition = Station ? Station->GetValidStationDefinition() : nullptr;
	PanelTitle->SetText(Definition && !Definition->StationName.IsEmpty()
		? Definition->StationName.ToUpper()
		: LOCTEXT("CraftingTitle", "CRAFTING"));
}

void UCraftingPanelWidget::RebindToPawn(APawn* Pawn)
{
	RebindToCrafting(Pawn, Pawn ? Pawn->FindComponentByClass<UCraftingComponent>() : nullptr);
}

void UCraftingPanelWidget::RebindToCrafting(APawn* Pawn, UCraftingComponent* Crafting)
{
	ReleaseBindings();
	ObservedPawn = Pawn;
	ObservedCrafting = Crafting;
	ObservedInventory = ObservedCrafting ? ObservedCrafting->GetInventory() : nullptr;
	if (ObservedInventory)
	{
		ObservedInventory->OnInventoryChanged.AddUniqueDynamic(this, &UCraftingPanelWidget::HandleInventoryChanged);
	}
	if (ObservedCrafting)
	{
		ObservedCrafting->OnCraftingResult.AddUniqueDynamic(this, &UCraftingPanelWidget::HandleCraftingResult);
		ObservedCrafting->OnCraftStarted.AddUniqueDynamic(this, &UCraftingPanelWidget::HandleCraftStarted);
		ObservedCrafting->OnCraftCancelled.AddUniqueDynamic(this, &UCraftingPanelWidget::HandleCraftCancelled);
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
		ObservedCrafting->OnCraftStarted.RemoveDynamic(this, &UCraftingPanelWidget::HandleCraftStarted);
		ObservedCrafting->OnCraftCancelled.RemoveDynamic(this, &UCraftingPanelWidget::HandleCraftCancelled);
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
	// The canonical feedback path: one message per crafting notification.
	ShowFeedback(UCraftingViewLibrary::GetCraftFeedbackText(ObservedCrafting, RecipeClass, Result), Result.IsSuccess());
	RefreshCraftingView();
}

void UCraftingPanelWidget::HandleCraftStarted(TSubclassOf<UCraftingRecipeDefinition> RecipeClass, float Duration)
{
	// Progress replaces feedback while a timed craft runs; no success message until it completes.
	bHasFeedback = false;
	FeedbackText = FText::GetEmpty();
	RefreshCraftingView();
}

void UCraftingPanelWidget::HandleCraftCancelled(TSubclassOf<UCraftingRecipeDefinition> RecipeClass, ECraftCancelReason Reason)
{
	ShowFeedback(UCraftingViewLibrary::GetCraftCancelledTextForRecipe(Reason, RecipeClass), false);
	RefreshCraftingView();
}

void UCraftingPanelWidget::HandleCraftButtonClicked()
{
	// The same button cancels while a timed craft runs.
	if (ObservedCrafting && ObservedCrafting->IsCrafting())
	{
		ObservedCrafting->CancelCraft();
		return;
	}
	RequestCraftSelectedRecipe();
}

void UCraftingPanelWidget::RequestCraftSelectedRecipe()
{
	if (bCraftRequestInProgress)
	{
		return;
	}
	if (!ObservedCrafting)
	{
		ShowFeedback(LOCTEXT("FeedbackNoCrafting", "Crafting unavailable."), false);
		RefreshDetails();
		return;
	}
	if (IsStationContextLost())
	{
		ShowFeedback(LOCTEXT("FeedbackStationLost", "The workstation is no longer available."), false);
		RefreshDetails();
		return;
	}
	if (!SelectedRecipe)
	{
		ShowFeedback(LOCTEXT("FeedbackNoSelection", "Select a recipe."), false);
		RefreshDetails();
		return;
	}
	{
		TGuardValue<bool> RequestGuard(bCraftRequestInProgress, true);
		ClearCraftFeedback();
		if (CraftButton)
		{
			CraftButton->SetIsEnabled(false);
		}
		// Feedback comes from OnCraftingResult; the returned result is intentionally not displayed.
		if (UCraftingStationComponent* Station = GetCraftingStation())
		{
			ObservedCrafting->TryStartCraftAtStation(SelectedRecipe, Station);
		}
		else
		{
			ObservedCrafting->TryStartCraft(SelectedRecipe);
		}
	}
	RefreshDetails();
}

void UCraftingPanelWidget::ShowFeedback(const FText& Text, const bool bSuccess)
{
	FeedbackText = Text;
	bFeedbackSuccess = bSuccess;
	bHasFeedback = true;
	FeedbackExpireSeconds = FPlatformTime::Seconds() + FMath::Max(0.f, FeedbackDisplaySeconds);
}

void UCraftingPanelWidget::ClearCraftFeedback()
{
	bHasFeedback = false;
	FeedbackText = FText::GetEmpty();
	RefreshDetails();
}

bool UCraftingPanelWidget::ExpireFeedback()
{
	if (bHasFeedback && FPlatformTime::Seconds() >= FeedbackExpireSeconds)
	{
		bHasFeedback = false;
		FeedbackText = FText::GetEmpty();
		return true;
	}
	return false;
}

void UCraftingPanelWidget::HandleRecipeEntrySelected(TSubclassOf<UCraftingRecipeDefinition> RecipeClass)
{
	SelectRecipe(RecipeClass);
}

void UCraftingPanelWidget::SelectRecipe(TSubclassOf<UCraftingRecipeDefinition> RecipeClass)
{
	if (RecipeClass != SelectedRecipe)
	{
		bHasFeedback = false;
		FeedbackText = FText::GetEmpty();
	}
	SelectedRecipe = RecipeClass;
	RefreshCraftingView();
}

void UCraftingPanelWidget::RefreshCraftingView()
{
	// Hand context lists hand recipes only; a station lists its own recipes first, then hand recipes.
	// A station context whose station is gone lists nothing.
	const TArray<TSubclassOf<UCraftingRecipeDefinition>> Recipes = ObservedCrafting && !IsStationContextLost()
		? ObservedCrafting->GetRecipesForStation(GetCraftingStation())
		: TArray<TSubclassOf<UCraftingRecipeDefinition>>();
	RefreshTitle();
	// While a timed craft runs (also after reopening the screen), show the recipe being crafted when it is listed.
	// An active craft that is not listed here (a station craft seen by hand) keeps its progress and CANCEL below.
	const TSubclassOf<UCraftingRecipeDefinition> ActiveRecipe = ObservedCrafting ? ObservedCrafting->GetActiveCraftRecipe() : nullptr;
	if (ActiveRecipe && Recipes.Contains(ActiveRecipe))
	{
		SelectedRecipe = ActiveRecipe;
	}
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
			Entry->SetRecipe(BuildView(Entry->GetRecipeClass()),
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
		Entry->SetRecipe(BuildView(RecipeClass), RecipeClass == SelectedRecipe);
		Entry->OnRecipeSelected.AddUniqueDynamic(this, &UCraftingPanelWidget::HandleRecipeEntrySelected);
		RecipeList->AddChild(Entry);
		RecipeEntries.Add(Entry);
	}
}

void UCraftingPanelWidget::RefreshDetails()
{
	SelectedView = SelectedRecipe
		? BuildView(SelectedRecipe)
		: FCraftingRecipeView();
	const bool bHasRecipe = SelectedView.bIsValid;
	if (SelectedRecipeIcon)
	{
		SelectedRecipeIcon->SetBrushFromTexture(SelectedView.RecipeIcon);
		SelectedRecipeIcon->SetVisibility(SelectedView.RecipeIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (SelectedRecipeName)
	{
		SelectedRecipeName->SetText(bHasRecipe ? SelectedView.RecipeName
			: IsStationContextLost() ? LOCTEXT("StationUnavailable", "Workstation unavailable") : LOCTEXT("NoRecipe", "No recipes available"));
	}
	if (SelectedRecipeDescription)
	{
		SelectedRecipeDescription->SetText(bHasRecipe ? SelectedView.RecipeDescription : FText::GetEmpty());
	}
	FillRequirementList(IngredientList, SelectedView.Ingredients, false);
	FillRequirementList(OutputList, SelectedView.Outputs, true);
	ExpireFeedback();
	if (CraftStatusText)
	{
		// A craft result message wins over the normal status until it expires, so the synchronous
		// inventory refresh after a craft does not overwrite it.
		if (bHasFeedback)
		{
			CraftStatusText->SetText(FeedbackText);
			CraftStatusText->SetColorAndOpacity(FSlateColor(bFeedbackSuccess ? FeedbackSuccessColor : FeedbackFailureColor));
		}
		else
		{
			CraftStatusText->SetText(bHasRecipe ? SelectedView.StatusText : FText::GetEmpty());
			CraftStatusText->SetColorAndOpacity(FSlateColor(StatusTextColor));
		}
	}
	if (CraftButton)
	{
		CraftButton->SetIsEnabled(bHasRecipe && SelectedView.bCanCraft && !bCraftRequestInProgress);
	}
	RefreshCraftProgress();
}

void UCraftingPanelWidget::RefreshCraftProgress()
{
	const bool bCrafting = ObservedCrafting && ObservedCrafting->IsCrafting();
	if (CraftProgressBar)
	{
		CraftProgressBar->SetPercent(bCrafting ? ObservedCrafting->GetActiveCraftProgress() : 0.f);
		CraftProgressBar->SetVisibility(bCrafting ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (CraftButtonText)
	{
		CraftButtonText->SetText(bCrafting ? LOCTEXT("CancelLabel", "CANCEL") : LOCTEXT("CraftLabel", "CRAFT"));
	}
	if (!bCrafting)
	{
		return;
	}
	// While crafting, the button cancels and the status shows authoritative progress (unless a message is up).
	if (CraftButton)
	{
		CraftButton->SetIsEnabled(!bCraftRequestInProgress);
	}
	if (CraftStatusText && !bHasFeedback)
	{
		CraftStatusText->SetText(UCraftingViewLibrary::GetActiveCraftStatusText(ObservedCrafting));
		CraftStatusText->SetColorAndOpacity(FSlateColor(StatusTextColor));
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
