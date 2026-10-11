#include "CraftingTestTypes.h"

#include "InventoryComponentTestTypes.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Crafting/CraftingComponent.h"
#include "Crafting/CraftingRecipeDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "Misc/AutomationTest.h"
#include "UI/CraftingPanelWidget.h"
#include "UI/CraftingRecipeEntryWidget.h"
#include "UI/CraftingRequirementRowWidget.h"
#include "UI/CraftingViewLibrary.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

void UCraftingPanelTestListener::HandleInventoryChanged(int32 NewRevision)
{
	++InventoryEvents;
	if (bRequestCraftOnInventoryChanged && Panel)
	{
		Panel->RequestCraftSelectedRecipe();
	}
}

void UCraftingPanelTestListener::HandleCraftingResult(TSubclassOf<UCraftingRecipeDefinition> RecipeClass,
	const FCraftingTransactionResult& Result)
{
	++CraftingEvents;
	LastResult = Result;
}

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using FC6BRecipeList = TArray<TSubclassOf<UCraftingRecipeDefinition>>;

UClass* C6BLoadItem(const TCHAR* Id)
{
	return LoadClass<UItemDefinition>(nullptr,
		*FString::Printf(TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_%s.Item_%s_C"), Id, Id));
}

UClass* C6BLoadRecipe(const TCHAR* Id)
{
	return LoadClass<UCraftingRecipeDefinition>(nullptr,
		*FString::Printf(TEXT("/Game/LevelPrototyping/CraftingSystem/Recipes/Recipe_%s.Recipe_%s_C"), Id, Id));
}

void C6BSetRecipes(UCraftingComponent* Crafting, const FC6BRecipeList& Recipes)
{
	FArrayProperty* Property = FindFProperty<FArrayProperty>(UCraftingComponent::StaticClass(), TEXT("AvailableRecipes"));
	check(Property);
	*Property->ContainerPtrToValuePtr<FC6BRecipeList>(Crafting) = Recipes;
}

TArray<TPair<FGuid, int32>> C6BCapture(const UInventoryComponent* Inventory)
{
	TArray<TPair<FGuid, int32>> Stacks;
	for (const UItemInstance* Item : Inventory->GetItemsSnapshot())
	{
		Stacks.Emplace(Item ? Item->GetInstanceId() : FGuid(), Item ? Item->GetQuantity() : 0);
	}
	return Stacks;
}

template <typename DelegateType>
int32 C6BBindingCount(const DelegateType& Delegate, const UObject* Object)
{
	int32 Count = 0;
	for (const UObject* Bound : Delegate.GetAllObjects())
	{
		Count += Bound == Object ? 1 : 0;
	}
	return Count;
}

/**
 * A real W_CraftingPanel created in a transient world and bound to an owner actor's crafting component.
 * Clicks go through the panel's CraftButton OnClicked binding.
 */
struct FC6BFixture
{
	UWorld* World = nullptr;
	TStrongObjectPtr<AActor> Owner;
	TStrongObjectPtr<UInventoryComponent> Inventory;
	TStrongObjectPtr<UCraftingComponent> Crafting;
	TStrongObjectPtr<UCraftingPanelWidget> Panel;
	TStrongObjectPtr<UCraftingPanelTestListener> Listener;

	explicit FC6BFixture(const FC6BRecipeList& Recipes, const bool bBindSource = true)
		: Owner(NewObject<AActor>(GetTransientPackage()))
		, Inventory(NewObject<UInventoryComponent>(Owner.Get()))
		, Crafting(NewObject<UCraftingComponent>(Owner.Get()))
		, Listener(NewObject<UCraftingPanelTestListener>(GetTransientPackage()))
	{
		Owner->AddInstanceComponent(Inventory.Get());
		Owner->AddInstanceComponent(Crafting.Get());
		C6BSetRecipes(Crafting.Get(), Recipes);
		World = UWorld::CreateWorld(EWorldType::Game, false);
		UClass* PanelClass = LoadClass<UCraftingPanelWidget>(nullptr,
			TEXT("/Game/LevelPrototyping/CraftingSystem/UI/W_CraftingPanel.W_CraftingPanel_C"));
		if (World && PanelClass)
		{
			Panel.Reset(CreateWidget<UCraftingPanelWidget>(World, PanelClass));
		}
		if (Panel && bBindSource)
		{
			Panel->SetCraftingSource(Crafting.Get());
		}
		Listener->Panel = Panel.Get();
	}

	~FC6BFixture()
	{
		Panel.Reset();
		if (World)
		{
			World->DestroyWorld(false);
		}
	}

	bool IsValid(FAutomationTestBase& Test) const
	{
		return Test.TestTrue(TEXT("Panel fixture is ready"), World && Panel && Find<UButton>(TEXT("CraftButton")) && Find<UTextBlock>(TEXT("CraftStatusText")));
	}

	template <typename WidgetType>
	WidgetType* Find(const TCHAR* Name) const
	{
		return Panel && Panel->WidgetTree ? Cast<WidgetType>(Panel->WidgetTree->FindWidget(FName(Name))) : nullptr;
	}

	void Click() const
	{
		if (UButton* Button = Find<UButton>(TEXT("CraftButton")))
		{
			Button->OnClicked.Broadcast();
		}
	}

	FString Status() const
	{
		const UTextBlock* Text = Find<UTextBlock>(TEXT("CraftStatusText"));
		return Text ? Text->GetText().ToString() : FString();
	}

	bool IsCraftEnabled() const
	{
		const UButton* Button = Find<UButton>(TEXT("CraftButton"));
		return Button && Button->GetIsEnabled();
	}

	TArray<UCraftingRequirementRowWidget*> IngredientRows() const
	{
		TArray<UCraftingRequirementRowWidget*> Rows;
		if (const UPanelWidget* List = Find<UPanelWidget>(TEXT("IngredientList")))
		{
			for (int32 Index = 0; Index < List->GetChildrenCount(); ++Index)
			{
				Rows.Add(Cast<UCraftingRequirementRowWidget>(List->GetChildAt(Index)));
			}
		}
		return Rows;
	}

	FString ReadyTextOf(TSubclassOf<UCraftingRecipeDefinition> Recipe) const
	{
		const UPanelWidget* List = Find<UPanelWidget>(TEXT("RecipeList"));
		for (int32 Index = 0; List && Index < List->GetChildrenCount(); ++Index)
		{
			const UCraftingRecipeEntryWidget* Entry = Cast<UCraftingRecipeEntryWidget>(List->GetChildAt(Index));
			if (Entry && Entry->GetRecipeClass() == Recipe && Entry->WidgetTree)
			{
				const UTextBlock* Ready = Cast<UTextBlock>(Entry->WidgetTree->FindWidget(TEXT("ReadyText")));
				return Ready ? Ready->GetText().ToString() : FString(TEXT("<no ReadyText>"));
			}
		}
		return TEXT("<no entry>");
	}

	void StartObserving()
	{
		Inventory->OnInventoryChanged.AddDynamic(Listener.Get(), &UCraftingPanelTestListener::HandleInventoryChanged);
		Crafting->OnCraftingResult.AddDynamic(Listener.Get(), &UCraftingPanelTestListener::HandleCraftingResult);
	}
};

bool C6BAdd(FAutomationTestBase& Test, UInventoryComponent* Inventory, TSubclassOf<UItemDefinition> Definition, int32 Quantity)
{
	return Test.TestTrue(FString::Printf(TEXT("Setup adds %d of %s"), Quantity, *GetNameSafe(Definition.Get())),
		Inventory->TryAddDefinitionQuantity(Definition, Quantity).IsSuccess());
}

struct FC6BProduction
{
	UClass* Wood = C6BLoadItem(TEXT("Wood"));
	UClass* Stone = C6BLoadItem(TEXT("Stone"));
	UClass* Axe = C6BLoadItem(TEXT("Axe"));
	UClass* WoodArrow = C6BLoadItem(TEXT("WoodArrow"));
	UClass* RecipeAxe = C6BLoadRecipe(TEXT("Axe"));
	UClass* RecipeWoodArrow = C6BLoadRecipe(TEXT("WoodArrow"));

	bool IsValid(FAutomationTestBase& Test) const
	{
		return Test.TestTrue(TEXT("Production items and recipes load"), Wood && Stone && Axe && WoodArrow && RecipeAxe && RecipeWoodArrow);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC6BClickRoutingTest,
	"NullTide.Crafting.C6B.ClickRoutesThroughComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC6BClickRoutingTest::RunTest(const FString& Parameters)
{
	const FC6BProduction Production;
	if (!Production.IsValid(*this)) { return false; }
	FC6BFixture Fixture({ Production.RecipeAxe, Production.RecipeWoodArrow });
	if (!Fixture.IsValid(*this)) { return false; }
	if (!C6BAdd(*this, Fixture.Inventory.Get(), Production.Wood, 3) || !C6BAdd(*this, Fixture.Inventory.Get(), Production.Stone, 5)) { return false; }
	Fixture.StartObserving();
	TestTrue(TEXT("The first registered recipe is selected"), Fixture.Panel->GetSelectedRecipe() == Production.RecipeAxe);
	TestTrue(TEXT("CRAFT is enabled for a craftable recipe"), Fixture.IsCraftEnabled());
	const int32 RevisionBefore = Fixture.Inventory->GetRevision();

	Fixture.Click();
	TestEqual(TEXT("Wood consumed"), Fixture.Inventory->GetTotalQuantity(Production.Wood), 0);
	TestEqual(TEXT("Stone consumed"), Fixture.Inventory->GetTotalQuantity(Production.Stone), 0);
	TestEqual(TEXT("Axe created"), Fixture.Inventory->GetTotalQuantity(Production.Axe), 1);
	TestEqual(TEXT("Only the Axe stack remains"), Fixture.Inventory->GetItemCount(), 1);
	TestEqual(TEXT("One click, one crafting notification"), Fixture.Listener->CraftingEvents, 1);
	TestEqual(TEXT("One inventory broadcast"), Fixture.Listener->InventoryEvents, 1);
	TestEqual(TEXT("One revision"), Fixture.Inventory->GetRevision(), RevisionBefore + 1);
	TestTrue(TEXT("The craft went through UCraftingComponent"), Fixture.Listener->LastResult.IsSuccess());
	TestEqual(TEXT("Success feedback"), Fixture.Status(), FString(TEXT("Crafted Axe x1.")));
	TestTrue(TEXT("Feedback is the success kind"), Fixture.Panel->HasCraftFeedback());
	TestTrue(TEXT("Selection is kept after crafting"), Fixture.Panel->GetSelectedRecipe() == Production.RecipeAxe);
	TestFalse(TEXT("CRAFT is disabled once ingredients are gone"), Fixture.IsCraftEnabled());
	TestFalse(TEXT("No request is left running"), Fixture.Panel->IsCraftRequestInProgress());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC6BRefreshTest,
	"NullTide.Crafting.C6B.RefreshAfterCraft",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC6BRefreshTest::RunTest(const FString& Parameters)
{
	const FC6BProduction Production;
	if (!Production.IsValid(*this)) { return false; }
	FC6BFixture Fixture({ Production.RecipeAxe, Production.RecipeWoodArrow });
	if (!Fixture.IsValid(*this)) { return false; }
	if (!C6BAdd(*this, Fixture.Inventory.Get(), Production.Wood, 2) || !C6BAdd(*this, Fixture.Inventory.Get(), Production.WoodArrow, 48)) { return false; }
	const FGuid ArrowStack = C6BCapture(Fixture.Inventory.Get())[1].Key;
	Fixture.StartObserving();
	Fixture.Panel->SelectRecipe(Production.RecipeWoodArrow);
	TestEqual(TEXT("WoodArrow is READY"), Fixture.ReadyTextOf(Production.RecipeWoodArrow), FString(TEXT("READY")));
	TestEqual(TEXT("Axe is not READY"), Fixture.ReadyTextOf(Production.RecipeAxe), FString());

	Fixture.Click();
	const TArray<TPair<FGuid, int32>> AfterFirst = C6BCapture(Fixture.Inventory.Get());
	TestTrue(TEXT("Wood 1, WoodArrow 50 (same stack), WoodArrow 3"), AfterFirst.Num() == 3 && AfterFirst[0].Value == 1
		&& AfterFirst[1].Key == ArrowStack && AfterFirst[1].Value == 50 && AfterFirst[2].Value == 3);
	TestEqual(TEXT("Feedback names the output"), Fixture.Status(), FString(TEXT("Crafted Wood Arrow x5.")));
	const TArray<UCraftingRequirementRowWidget*> Rows = Fixture.IngredientRows();
	TestTrue(TEXT("Ingredient row shows Wood 1 / 1"), Rows.Num() == 1 && Rows[0]
		&& Rows[0]->GetRequirement().OwnedQuantity == 1 && Rows[0]->GetRequirement().RequiredQuantity == 1);
	TestTrue(TEXT("Still craftable with one Wood"), Fixture.IsCraftEnabled());
	TestEqual(TEXT("WoodArrow stays READY"), Fixture.ReadyTextOf(Production.RecipeWoodArrow), FString(TEXT("READY")));

	Fixture.Click();
	TestEqual(TEXT("Second click crafts again"), Fixture.Inventory->GetTotalQuantity(Production.WoodArrow), 58);
	TestEqual(TEXT("Wood is gone"), Fixture.Inventory->GetTotalQuantity(Production.Wood), 0);
	TestFalse(TEXT("CRAFT disables when Wood runs out"), Fixture.IsCraftEnabled());
	TestEqual(TEXT("WoodArrow is no longer READY"), Fixture.ReadyTextOf(Production.RecipeWoodArrow), FString());
	TestTrue(TEXT("Ingredient row shows Wood 0 / 1"), Fixture.IngredientRows().Num() == 1
		&& Fixture.IngredientRows()[0]->GetRequirement().OwnedQuantity == 0);
	TestTrue(TEXT("Selection persists"), Fixture.Panel->GetSelectedRecipe() == Production.RecipeWoodArrow);
	TestEqual(TEXT("Two crafts, two inventory broadcasts"), Fixture.Listener->InventoryEvents, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC6BFailureTest,
	"NullTide.Crafting.C6B.FailureFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC6BFailureTest::RunTest(const FString& Parameters)
{
	const FC6BProduction Production;
	if (!Production.IsValid(*this)) { return false; }

	// Missing ingredient.
	{
		FC6BFixture Fixture({ Production.RecipeAxe });
		if (!Fixture.IsValid(*this)) { return false; }
		if (!C6BAdd(*this, Fixture.Inventory.Get(), Production.Wood, 2) || !C6BAdd(*this, Fixture.Inventory.Get(), Production.Stone, 5)) { return false; }
		Fixture.StartObserving();
		const TArray<TPair<FGuid, int32>> Before = C6BCapture(Fixture.Inventory.Get());
		const int32 Revision = Fixture.Inventory->GetRevision();
		Fixture.Panel->RequestCraftSelectedRecipe();
		TestEqual(TEXT("Missing feedback"), Fixture.Status(), FString(TEXT("Not enough Wood (2 / 3).")));
		TestTrue(TEXT("Missing: contents and ItemIds unchanged"), C6BCapture(Fixture.Inventory.Get()) == Before);
		TestEqual(TEXT("Missing: revision unchanged"), Fixture.Inventory->GetRevision(), Revision);
		TestEqual(TEXT("Missing: no inventory event"), Fixture.Listener->InventoryEvents, 0);
		TestEqual(TEXT("Missing: one crafting notification"), Fixture.Listener->CraftingEvents, 1);
	}

	// No room for the output: the remaining Wood and Stone stacks keep their slots.
	{
		FC6BFixture Fixture({ Production.RecipeAxe });
		if (!Fixture.IsValid(*this)) { return false; }
		if (!C6BAdd(*this, Fixture.Inventory.Get(), Production.Wood, 10) || !C6BAdd(*this, Fixture.Inventory.Get(), Production.Stone, 10)
			|| !C6BAdd(*this, Fixture.Inventory.Get(), UInventoryTestItemDefinition::StaticClass(), 22)) { return false; }
		Fixture.StartObserving();
		const TArray<TPair<FGuid, int32>> Before = C6BCapture(Fixture.Inventory.Get());
		const int32 Revision = Fixture.Inventory->GetRevision();
		Fixture.Panel->RequestCraftSelectedRecipe();
		TestEqual(TEXT("Capacity feedback"), Fixture.Status(), FString(TEXT("Not enough inventory space.")));
		TestTrue(TEXT("Capacity: contents and ItemIds unchanged"), C6BCapture(Fixture.Inventory.Get()) == Before);
		TestEqual(TEXT("Capacity: revision unchanged"), Fixture.Inventory->GetRevision(), Revision);
		TestEqual(TEXT("Capacity: no inventory event"), Fixture.Listener->InventoryEvents, 0);
	}

	// Recipe no longer registered when the click arrives.
	{
		FC6BFixture Fixture({ Production.RecipeAxe });
		if (!Fixture.IsValid(*this)) { return false; }
		if (!C6BAdd(*this, Fixture.Inventory.Get(), Production.Wood, 3) || !C6BAdd(*this, Fixture.Inventory.Get(), Production.Stone, 5)) { return false; }
		Fixture.StartObserving();
		C6BSetRecipes(Fixture.Crafting.Get(), { Production.RecipeWoodArrow });
		Fixture.Panel->RequestCraftSelectedRecipe();
		TestTrue(TEXT("Unregistered recipe is rejected by the component"),
			Fixture.Listener->LastResult.Result == ECraftingTransactionResult::RecipeNotAvailable);
		TestEqual(TEXT("Unavailable feedback"), Fixture.Status(), FString(TEXT("Recipe unavailable.")));
		TestEqual(TEXT("Unavailable: nothing consumed"), Fixture.Inventory->GetTotalQuantity(Production.Wood), 3);
		TestEqual(TEXT("Unavailable: no inventory event"), Fixture.Listener->InventoryEvents, 0);
	}

	// No crafting component: nothing to call, local feedback only.
	{
		FC6BFixture Fixture({ Production.RecipeAxe }, false);
		if (!Fixture.IsValid(*this)) { return false; }
		Fixture.StartObserving();
		Fixture.Panel->RequestCraftSelectedRecipe();
		TestEqual(TEXT("No crafting source feedback"), Fixture.Status(), FString(TEXT("Crafting unavailable.")));
		TestEqual(TEXT("No component call was made"), Fixture.Listener->CraftingEvents, 0);
		TestNull(TEXT("Panel observes no component"), Fixture.Panel->GetObservedCrafting());
	}

	// Message mapping for the remaining result codes.
	FCraftingTransactionResult Result;
	const TSubclassOf<UCraftingRecipeDefinition> Recipe = Production.RecipeAxe;
	Result.Result = ECraftingTransactionResult::InvalidRecipe;
	TestEqual(TEXT("Invalid recipe message"), UCraftingViewLibrary::GetCraftFeedbackText(nullptr, Recipe, Result).ToString(), FString(TEXT("Recipe unavailable.")));
	Result.Result = ECraftingTransactionResult::InventoryBusy;
	TestEqual(TEXT("Busy message"), UCraftingViewLibrary::GetCraftFeedbackText(nullptr, Recipe, Result).ToString(), FString(TEXT("Inventory is busy.")));
	Result.Result = ECraftingTransactionResult::CraftInProgress;
	TestEqual(TEXT("Craft in progress message"), UCraftingViewLibrary::GetCraftFeedbackText(nullptr, Recipe, Result).ToString(), FString(TEXT("Inventory is busy.")));
	Result.Result = ECraftingTransactionResult::InvalidInventory;
	TestEqual(TEXT("No inventory message"), UCraftingViewLibrary::GetCraftFeedbackText(nullptr, Recipe, Result).ToString(), FString(TEXT("No inventory available.")));
	Result.Result = ECraftingTransactionResult::InventoryRejected;
	TestEqual(TEXT("Generic failure message"), UCraftingViewLibrary::GetCraftFeedbackText(nullptr, Recipe, Result).ToString(), FString(TEXT("Crafting failed.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC6BDuplicateTest,
	"NullTide.Crafting.C6B.DuplicateAndReentrantClicks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC6BDuplicateTest::RunTest(const FString& Parameters)
{
	const FC6BProduction Production;
	if (!Production.IsValid(*this)) { return false; }
	FC6BFixture Fixture({ Production.RecipeAxe });
	if (!Fixture.IsValid(*this)) { return false; }
	if (!C6BAdd(*this, Fixture.Inventory.Get(), Production.Wood, 3) || !C6BAdd(*this, Fixture.Inventory.Get(), Production.Stone, 5)) { return false; }
	Fixture.Listener->bRequestCraftOnInventoryChanged = true;
	Fixture.StartObserving();
	const int32 RevisionBefore = Fixture.Inventory->GetRevision();

	Fixture.Click();
	TestEqual(TEXT("The reentrant request during the commit was ignored: one Axe"), Fixture.Inventory->GetTotalQuantity(Production.Axe), 1);
	TestEqual(TEXT("One crafting notification"), Fixture.Listener->CraftingEvents, 1);
	TestEqual(TEXT("One inventory broadcast"), Fixture.Listener->InventoryEvents, 1);
	TestFalse(TEXT("Request guard released"), Fixture.Panel->IsCraftRequestInProgress());
	TestEqual(TEXT("Success feedback shown once"), Fixture.Status(), FString(TEXT("Crafted Axe x1.")));

	Fixture.Listener->bRequestCraftOnInventoryChanged = false;
	Fixture.Click();
	TestEqual(TEXT("A repeated click is a separate attempt and fails cleanly"), Fixture.Status(), FString(TEXT("Not enough Wood (0 / 3).")));
	TestEqual(TEXT("Repeated click adds one crafting notification"), Fixture.Listener->CraftingEvents, 2);
	TestEqual(TEXT("Repeated failed click adds no inventory event"), Fixture.Listener->InventoryEvents, 1);
	TestEqual(TEXT("Only the successful craft changed the revision"), Fixture.Inventory->GetRevision(), RevisionBefore + 1);
	TestEqual(TEXT("Still exactly one Axe"), Fixture.Inventory->GetTotalQuantity(Production.Axe), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC6BBindingTest,
	"NullTide.Crafting.C6B.NoDuplicateBindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC6BBindingTest::RunTest(const FString& Parameters)
{
	const FC6BProduction Production;
	if (!Production.IsValid(*this)) { return false; }
	FC6BFixture Fixture({ Production.RecipeAxe });
	if (!Fixture.IsValid(*this)) { return false; }
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Fixture.Panel->SetCraftingSource(Fixture.Crafting.Get());
		Fixture.Panel->RefreshCraftingView();
	}
	const UObject* Panel = Fixture.Panel.Get();
	TestEqual(TEXT("One OnCraftingResult binding"), C6BBindingCount(Fixture.Crafting->OnCraftingResult, Panel), 1);
	TestEqual(TEXT("One OnInventoryChanged binding"), C6BBindingCount(Fixture.Inventory->OnInventoryChanged, Panel), 1);
	TestEqual(TEXT("One CRAFT click binding"), C6BBindingCount(Fixture.Find<UButton>(TEXT("CraftButton"))->OnClicked, Panel), 1);

	// Switching source releases the old component.
	FC6BFixture Other({ Production.RecipeAxe });
	if (!Other.IsValid(*this)) { return false; }
	Fixture.Panel->SetCraftingSource(Other.Crafting.Get());
	TestEqual(TEXT("Old component unbound (crafting)"), C6BBindingCount(Fixture.Crafting->OnCraftingResult, Panel), 0);
	TestEqual(TEXT("Old component unbound (inventory)"), C6BBindingCount(Fixture.Inventory->OnInventoryChanged, Panel), 0);
	TestEqual(TEXT("New component bound once"), C6BBindingCount(Other.Crafting->OnCraftingResult, Panel), 1);
	TestTrue(TEXT("Panel observes the new component"), Fixture.Panel->GetObservedCrafting() == Other.Crafting.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC6BSelectionTest,
	"NullTide.Crafting.C6B.SelectionAndFeedbackLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC6BSelectionTest::RunTest(const FString& Parameters)
{
	const FC6BProduction Production;
	if (!Production.IsValid(*this)) { return false; }
	FC6BFixture Fixture({ Production.RecipeAxe, Production.RecipeWoodArrow });
	if (!Fixture.IsValid(*this)) { return false; }
	if (!C6BAdd(*this, Fixture.Inventory.Get(), Production.Wood, 5)) { return false; }
	Fixture.StartObserving();
	Fixture.Panel->SelectRecipe(Production.RecipeWoodArrow);
	Fixture.Click();
	TestTrue(TEXT("Second recipe stays selected after crafting"), Fixture.Panel->GetSelectedRecipe() == Production.RecipeWoodArrow);
	TestEqual(TEXT("Feedback shown"), Fixture.Status(), FString(TEXT("Crafted Wood Arrow x5.")));

	// An unrelated inventory change refreshes the panel but keeps the feedback visible.
	if (!C6BAdd(*this, Fixture.Inventory.Get(), Production.Stone, 1)) { return false; }
	TestEqual(TEXT("Feedback survives the refresh"), Fixture.Status(), FString(TEXT("Crafted Wood Arrow x5.")));

	// Selecting another recipe clears the feedback and shows the normal status.
	Fixture.Panel->SelectRecipe(Production.RecipeAxe);
	TestFalse(TEXT("Feedback cleared on selection change"), Fixture.Panel->HasCraftFeedback());
	TestEqual(TEXT("Normal status for the new selection"), Fixture.Status(), FString(TEXT("Missing Stone (1 / 5).")));

	// With a zero display time the feedback expires on the next refresh.
	FFloatProperty* Duration = FindFProperty<FFloatProperty>(UCraftingPanelWidget::StaticClass(), TEXT("FeedbackDisplaySeconds"));
	if (!TestNotNull(TEXT("Feedback duration property"), Duration)) { return false; }
	Duration->SetPropertyValue_InContainer(Fixture.Panel.Get(), 0.f);
	Fixture.Panel->SelectRecipe(Production.RecipeWoodArrow);
	Fixture.Click();
	TestFalse(TEXT("Expired feedback is gone"), Fixture.Panel->HasCraftFeedback());
	TestEqual(TEXT("Normal status returns after expiry"), Fixture.Status(), FString(TEXT("Ready to craft.")));
	TestEqual(TEXT("The craft itself still happened"), Fixture.Inventory->GetTotalQuantity(Production.Wood), 3);

	Duration->SetPropertyValue_InContainer(Fixture.Panel.Get(), 3.f);
	Fixture.Click();
	TestTrue(TEXT("Feedback present again"), Fixture.Panel->HasCraftFeedback());
	Fixture.Panel->ClearCraftFeedback();
	TestEqual(TEXT("ClearCraftFeedback restores the normal status"), Fixture.Status(), FString(TEXT("Ready to craft.")));
	return true;
}

#endif
