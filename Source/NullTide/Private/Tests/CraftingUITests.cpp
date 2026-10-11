#include "CraftingTestTypes.h"

#include "InventoryComponentTestTypes.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Crafting/CraftingComponent.h"
#include "Crafting/CraftingRecipeDefinition.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "Misc/AutomationTest.h"
#include "UI/CraftingPanelWidget.h"
#include "UI/CraftingRecipeEntryWidget.h"
#include "UI/CraftingRequirementRowWidget.h"
#include "UI/CraftingViewLibrary.h"
#include "UI/InventoryScreenWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using FC6RecipeList = TArray<TSubclassOf<UCraftingRecipeDefinition>>;

const TCHAR* C6CraftingUIPath = TEXT("/Game/LevelPrototyping/CraftingSystem/UI");

UClass* C6LoadItem(const TCHAR* Id)
{
	return LoadClass<UItemDefinition>(nullptr,
		*FString::Printf(TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_%s.Item_%s_C"), Id, Id));
}

UClass* C6LoadRecipe(const TCHAR* Id)
{
	return LoadClass<UCraftingRecipeDefinition>(nullptr,
		*FString::Printf(TEXT("/Game/LevelPrototyping/CraftingSystem/Recipes/Recipe_%s.Recipe_%s_C"), Id, Id));
}

UWidgetBlueprintGeneratedClass* C6LoadWidgetClass(const FString& PackagePath, const TCHAR* Name)
{
	return Cast<UWidgetBlueprintGeneratedClass>(
		LoadClass<UUserWidget>(nullptr, *FString::Printf(TEXT("%s/%s.%s_C"), *PackagePath, Name, Name)));
}

void C6SetRecipes(UCraftingComponent* Crafting, const FC6RecipeList& Recipes)
{
	FArrayProperty* Property = FindFProperty<FArrayProperty>(UCraftingComponent::StaticClass(), TEXT("AvailableRecipes"));
	check(Property);
	*Property->ContainerPtrToValuePtr<FC6RecipeList>(Crafting) = Recipes;
}

/** Owner actor with inventory and crafting component, plus listeners for both notifications. */
struct FC6Fixture
{
	TStrongObjectPtr<AActor> Owner;
	TStrongObjectPtr<UInventoryComponent> Inventory;
	TStrongObjectPtr<UCraftingComponent> Crafting;
	TStrongObjectPtr<UCraftingComponentTestListener> Listener;

	explicit FC6Fixture(const FC6RecipeList& Recipes)
		: Owner(NewObject<AActor>(GetTransientPackage()))
		, Inventory(NewObject<UInventoryComponent>(Owner.Get()))
		, Crafting(NewObject<UCraftingComponent>(Owner.Get()))
		, Listener(NewObject<UCraftingComponentTestListener>(GetTransientPackage()))
	{
		Owner->AddInstanceComponent(Inventory.Get());
		Owner->AddInstanceComponent(Crafting.Get());
		C6SetRecipes(Crafting.Get(), Recipes);
	}

	void StartObserving()
	{
		Inventory->OnInventoryChanged.AddDynamic(Listener.Get(), &UCraftingComponentTestListener::HandleInventoryChanged);
		Crafting->OnCraftingResult.AddDynamic(Listener.Get(), &UCraftingComponentTestListener::HandleCraftingResult);
	}
};

bool C6Add(FAutomationTestBase& Test, UInventoryComponent* Inventory, TSubclassOf<UItemDefinition> Definition, int32 Quantity)
{
	return Test.TestTrue(FString::Printf(TEXT("Setup adds %d of %s"), Quantity, *GetNameSafe(Definition.Get())),
		Inventory->TryAddDefinitionQuantity(Definition, Quantity).IsSuccess());
}

TArray<TPair<FGuid, int32>> C6Capture(const UInventoryComponent* Inventory)
{
	TArray<TPair<FGuid, int32>> Stacks;
	for (const UItemInstance* Item : Inventory->GetItemsSnapshot())
	{
		Stacks.Emplace(Item ? Item->GetInstanceId() : FGuid(), Item ? Item->GetQuantity() : 0);
	}
	return Stacks;
}

/** Checks that a widget with this name and type exists in a generated widget class's tree. */
template <typename WidgetType>
WidgetType* C6FindWidget(FAutomationTestBase& Test, UWidgetBlueprintGeneratedClass* Class, const TCHAR* Name)
{
	UWidgetTree* Tree = Class ? Class->GetWidgetTreeArchetype() : nullptr;
	WidgetType* Widget = Tree ? Cast<WidgetType>(Tree->FindWidget(FName(Name))) : nullptr;
	Test.TestNotNull(FString::Printf(TEXT("%s has %s of type %s"), *GetNameSafe(Class), Name, *WidgetType::StaticClass()->GetName()),
		Widget);
	return Widget;
}

TArray<FString> C6ChildNames(const UPanelWidget* Panel)
{
	TArray<FString> Names;
	for (int32 Index = 0; Panel && Index < Panel->GetChildrenCount(); ++Index)
	{
		Names.Add(GetNameSafe(Panel->GetChildAt(Index)));
	}
	return Names;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC6ARecipeViewTest,
	"NullTide.Crafting.C6A.RecipeView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC6ARecipeViewTest::RunTest(const FString& Parameters)
{
	UClass* Wood = C6LoadItem(TEXT("Wood"));
	UClass* Stone = C6LoadItem(TEXT("Stone"));
	UClass* Axe = C6LoadItem(TEXT("Axe"));
	UClass* WoodArrow = C6LoadItem(TEXT("WoodArrow"));
	UClass* RecipeAxe = C6LoadRecipe(TEXT("Axe"));
	UClass* RecipeWoodArrow = C6LoadRecipe(TEXT("WoodArrow"));
	if (!TestTrue(TEXT("Production items and recipes load"), Wood && Stone && Axe && WoodArrow && RecipeAxe && RecipeWoodArrow))
	{
		return false;
	}

	FC6Fixture Fixture({ RecipeAxe, RecipeWoodArrow });
	if (!C6Add(*this, Fixture.Inventory.Get(), Wood, 8) || !C6Add(*this, Fixture.Inventory.Get(), Stone, 2)) { return false; }

	const FCraftingRecipeView Missing = UCraftingViewLibrary::BuildRecipeView(Fixture.Crafting.Get(), RecipeAxe);
	TestTrue(TEXT("Axe view is valid"), Missing.bIsValid);
	TestEqual(TEXT("Recipe name"), Missing.RecipeName.ToString(), FString(TEXT("Axe")));
	TestEqual(TEXT("Recipe description"), Missing.RecipeDescription.ToString(), FString(TEXT("Craft a basic woodcutting tool.")));
	TestTrue(TEXT("Recipe icon"), Missing.RecipeIcon == LoadObject<UTexture2D>(nullptr, TEXT("/Game/Items/Icons/T_Icon_Axe.T_Icon_Axe")));
	if (TestEqual(TEXT("Two ingredient lines"), Missing.Ingredients.Num(), 2))
	{
		TestTrue(TEXT("Wood line"), Missing.Ingredients[0].ItemDefinition == Wood && Missing.Ingredients[0].OwnedQuantity == 8
			&& Missing.Ingredients[0].RequiredQuantity == 3 && Missing.Ingredients[0].bSatisfied);
		TestEqual(TEXT("Wood line name"), Missing.Ingredients[0].ItemName.ToString(), FString(TEXT("Wood")));
		TestNotNull(TEXT("Wood line icon"), Missing.Ingredients[0].ItemIcon.Get());
		TestTrue(TEXT("Stone line is short"), Missing.Ingredients[1].ItemDefinition == Stone && Missing.Ingredients[1].OwnedQuantity == 2
			&& Missing.Ingredients[1].RequiredQuantity == 5 && !Missing.Ingredients[1].bSatisfied);
	}
	if (TestEqual(TEXT("One output line"), Missing.Outputs.Num(), 1))
	{
		TestTrue(TEXT("Axe output line"), Missing.Outputs[0].ItemDefinition == Axe && Missing.Outputs[0].RequiredQuantity == 1
			&& Missing.Outputs[0].bSatisfied);
	}
	TestTrue(TEXT("Craft result is the exact component preflight"),
		Missing.CraftResult.Result == ECraftingTransactionResult::InsufficientIngredients && Missing.CraftResult.EntryIndex == 1);
	TestFalse(TEXT("Not craftable"), Missing.bCanCraft);
	TestEqual(TEXT("Status names the missing ingredient"), Missing.StatusText.ToString(), FString(TEXT("Missing Stone (2 / 5).")));

	if (!C6Add(*this, Fixture.Inventory.Get(), Stone, 3)) { return false; }
	const FCraftingRecipeView Ready = UCraftingViewLibrary::BuildRecipeView(Fixture.Crafting.Get(), RecipeAxe);
	TestTrue(TEXT("Craftable once Stone is 5"), Ready.bCanCraft);
	TestTrue(TEXT("Stone line satisfied"), Ready.Ingredients.Num() == 2 && Ready.Ingredients[1].bSatisfied);
	TestEqual(TEXT("Ready status"), Ready.StatusText.ToString(), FString(TEXT("Ready to craft.")));

	const FCraftingRecipeView Arrow = UCraftingViewLibrary::BuildRecipeView(Fixture.Crafting.Get(), RecipeWoodArrow);
	TestTrue(TEXT("WoodArrow output x5"), Arrow.Outputs.Num() == 1 && Arrow.Outputs[0].ItemDefinition == WoodArrow
		&& Arrow.Outputs[0].RequiredQuantity == 5);
	TestTrue(TEXT("WoodArrow craftable with Wood 8"), Arrow.bCanCraft);

	const FCraftingRecipeView NoComponent = UCraftingViewLibrary::BuildRecipeView(nullptr, RecipeAxe);
	TestTrue(TEXT("Without a component the recipe data still shows"), NoComponent.bIsValid && NoComponent.Ingredients.Num() == 2);
	TestTrue(TEXT("Without a component owned counts are zero"), NoComponent.Ingredients[0].OwnedQuantity == 0);
	TestTrue(TEXT("Without a component nothing is craftable"),
		!NoComponent.bCanCraft && NoComponent.CraftResult.Result == ECraftingTransactionResult::InvalidInventory);

	TestFalse(TEXT("Null recipe view is invalid"), UCraftingViewLibrary::BuildRecipeView(Fixture.Crafting.Get(), nullptr).bIsValid);
	TestFalse(TEXT("Abstract recipe view is invalid"),
		UCraftingViewLibrary::BuildRecipeView(Fixture.Crafting.Get(), UCraftingRecipeDefinition::StaticClass()).bIsValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC6AExactStatusTest,
	"NullTide.Crafting.C6A.ExactStatus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC6AExactStatusTest::RunTest(const FString& Parameters)
{
	// Full inventory with a 50/50 stack: the exact preflight allows x10 -> x5.
	FC6Fixture Fixture({ UCraftingOverlapTestRecipe::StaticClass(), UCraftingOutputHeavyTestRecipe::StaticClass() });
	if (!C6Add(*this, Fixture.Inventory.Get(), UInventoryStackableTestItemDefinition::StaticClass(), 50)
		|| !C6Add(*this, Fixture.Inventory.Get(), UInventoryOtherTestItemDefinition::StaticClass(), 23)) { return false; }

	const FCraftingRecipeView Overlap = UCraftingViewLibrary::BuildRecipeView(Fixture.Crafting.Get(), UCraftingOverlapTestRecipe::StaticClass());
	TestTrue(TEXT("Overlap recipe is craftable in a full inventory (exact preflight)"), Overlap.bCanCraft);

	const FCraftingRecipeView Heavy = UCraftingViewLibrary::BuildRecipeView(Fixture.Crafting.Get(), UCraftingOutputHeavyTestRecipe::StaticClass());
	TestFalse(TEXT("Two new outputs do not fit"), Heavy.bCanCraft);
	TestEqual(TEXT("Full status"), Heavy.StatusText.ToString(), FString(TEXT("Not enough inventory space for the output.")));

	const FCraftingRecipeView Unregistered = UCraftingViewLibrary::BuildRecipeView(Fixture.Crafting.Get(), UCraftingTestRecipeDefinition::StaticClass());
	TestTrue(TEXT("Unregistered recipe shows its data"), Unregistered.bIsValid);
	TestEqual(TEXT("Unregistered status"), Unregistered.StatusText.ToString(), FString(TEXT("This recipe is not available.")));

	FCraftingRequirementView Line;
	Line.OwnedQuantity = 8;
	Line.RequiredQuantity = 3;
	TestEqual(TEXT("Ingredient quantity format"), UCraftingRequirementRowWidget::FormatQuantity(Line, false).ToString(), FString(TEXT("8 / 3")));
	Line.RequiredQuantity = 5;
	TestEqual(TEXT("Output quantity format"), UCraftingRequirementRowWidget::FormatQuantity(Line, true).ToString(), FString(TEXT("x5")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC6AReadOnlyTest,
	"NullTide.Crafting.C6A.ViewIsReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC6AReadOnlyTest::RunTest(const FString& Parameters)
{
	FC6Fixture Fixture({ UCraftingTestRecipeDefinition::StaticClass(), UCraftingOverlapTestRecipe::StaticClass() });
	if (!C6Add(*this, Fixture.Inventory.Get(), UInventoryStackableTestItemDefinition::StaticClass(), 20)
		|| !C6Add(*this, Fixture.Inventory.Get(), UInventoryTestItemDefinition::StaticClass(), 2)) { return false; }
	Fixture.StartObserving();
	const TArray<TPair<FGuid, int32>> Before = C6Capture(Fixture.Inventory.Get());
	const int32 Revision = Fixture.Inventory->GetRevision();
	for (int32 Index = 0; Index < 5; ++Index)
	{
		for (const TSubclassOf<UCraftingRecipeDefinition>& Recipe : Fixture.Crafting->GetAvailableRecipes())
		{
			UCraftingViewLibrary::BuildRecipeView(Fixture.Crafting.Get(), Recipe);
		}
	}
	TestTrue(TEXT("Contents and ItemIds unchanged"), C6Capture(Fixture.Inventory.Get()) == Before);
	TestEqual(TEXT("Revision unchanged"), Fixture.Inventory->GetRevision(), Revision);
	TestEqual(TEXT("No inventory event"), Fixture.Listener->InventoryEvents, 0);
	TestEqual(TEXT("No crafting notification"), Fixture.Listener->CraftingEvents, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC6AWidgetAssetsTest,
	"NullTide.Crafting.C6A.WidgetAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC6AWidgetAssetsTest::RunTest(const FString& Parameters)
{
	UWidgetBlueprintGeneratedClass* Row = C6LoadWidgetClass(C6CraftingUIPath, TEXT("W_CraftingRequirementRow"));
	UWidgetBlueprintGeneratedClass* Entry = C6LoadWidgetClass(C6CraftingUIPath, TEXT("W_CraftingRecipeEntry"));
	UWidgetBlueprintGeneratedClass* Panel = C6LoadWidgetClass(C6CraftingUIPath, TEXT("W_CraftingPanel"));
	if (!TestTrue(TEXT("Crafting widget assets load"), Row && Entry && Panel)) { return false; }
	TestTrue(TEXT("Row parent is UCraftingRequirementRowWidget"), Row->IsChildOf(UCraftingRequirementRowWidget::StaticClass()));
	TestTrue(TEXT("Entry parent is UCraftingRecipeEntryWidget"), Entry->IsChildOf(UCraftingRecipeEntryWidget::StaticClass()));
	TestTrue(TEXT("Panel parent is UCraftingPanelWidget"), Panel->IsChildOf(UCraftingPanelWidget::StaticClass()));

	C6FindWidget<UImage>(*this, Row, TEXT("ItemIcon"));
	C6FindWidget<UTextBlock>(*this, Row, TEXT("ItemName"));
	C6FindWidget<UTextBlock>(*this, Row, TEXT("QuantityText"));

	C6FindWidget<UButton>(*this, Entry, TEXT("EntryButton"));
	C6FindWidget<UImage>(*this, Entry, TEXT("RecipeIcon"));
	C6FindWidget<UTextBlock>(*this, Entry, TEXT("RecipeName"));
	C6FindWidget<UTextBlock>(*this, Entry, TEXT("ReadyText"));

	C6FindWidget<UPanelWidget>(*this, Panel, TEXT("RecipeList"));
	C6FindWidget<UImage>(*this, Panel, TEXT("SelectedRecipeIcon"));
	C6FindWidget<UTextBlock>(*this, Panel, TEXT("SelectedRecipeName"));
	C6FindWidget<UTextBlock>(*this, Panel, TEXT("SelectedRecipeDescription"));
	C6FindWidget<UPanelWidget>(*this, Panel, TEXT("IngredientList"));
	C6FindWidget<UPanelWidget>(*this, Panel, TEXT("OutputList"));
	C6FindWidget<UTextBlock>(*this, Panel, TEXT("CraftStatusText"));
	if (UButton* CraftButton = C6FindWidget<UButton>(*this, Panel, TEXT("CraftButton")))
	{
		TestFalse(TEXT("CRAFT starts disabled until the panel finds a craftable recipe"), CraftButton->GetIsEnabled());
	}

	const UCraftingPanelWidget* PanelDefaults = Panel->GetDefaultObject<UCraftingPanelWidget>();
	const FClassProperty* EntryClassProperty = FindFProperty<FClassProperty>(UCraftingPanelWidget::StaticClass(), TEXT("RecipeEntryClass"));
	const FClassProperty* RowClassProperty = FindFProperty<FClassProperty>(UCraftingPanelWidget::StaticClass(), TEXT("RequirementRowClass"));
	if (TestTrue(TEXT("Panel class properties exist"), EntryClassProperty && RowClassProperty))
	{
		TestTrue(TEXT("Panel uses W_CraftingRecipeEntry"), EntryClassProperty->GetObjectPropertyValue_InContainer(PanelDefaults) == Entry);
		TestTrue(TEXT("Panel uses W_CraftingRequirementRow"), RowClassProperty->GetObjectPropertyValue_InContainer(PanelDefaults) == Row);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC6AInventoryLayoutTest,
	"NullTide.Crafting.C6A.InventoryLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC6AInventoryLayoutTest::RunTest(const FString& Parameters)
{
	UWidgetBlueprintGeneratedClass* Inventory =
		C6LoadWidgetClass(TEXT("/Game/LevelPrototyping/InventorySystem/UI/Widgets"), TEXT("W_Inventory"));
	if (!TestNotNull(TEXT("W_Inventory loads"), Inventory)) { return false; }
	TestTrue(TEXT("W_Inventory still derives from UInventoryScreenWidget"), Inventory->IsChildOf(UInventoryScreenWidget::StaticClass()));

	// Every widget UInventoryScreenWidget binds keeps its name and type.
	C6FindWidget<UWrapBox>(*this, Inventory, TEXT("InventoryContainerWrapBox"));
	for (const TCHAR* Name : { TEXT("CloseButton"), TEXT("TabAllButton"), TEXT("TabResourceButton"), TEXT("TabConsumableButton"),
		TEXT("TabWeaponButton"), TEXT("TabAmmoButton"), TEXT("EquipToolButton") })
	{
		C6FindWidget<UButton>(*this, Inventory, Name);
	}
	for (const TCHAR* Name : { TEXT("DetailsIcon"), TEXT("AxeToolIcon"), TEXT("PickaxeToolIcon") })
	{
		C6FindWidget<UImage>(*this, Inventory, Name);
	}
	for (const TCHAR* Name : { TEXT("DetailsItemName"), TEXT("DetailsItemDescription"), TEXT("DetailsCategory"), TEXT("CapabilitySummary"),
		TEXT("FooterCount"), TEXT("AxeToolName"), TEXT("PickaxeToolName"), TEXT("ToolActionButtonText"), TEXT("ToolActionText") })
	{
		C6FindWidget<UTextBlock>(*this, Inventory, Name);
	}

	// Three columns: inventory (backpack + details), character, crafting.
	const UPanelWidget* MainContent = C6FindWidget<UPanelWidget>(*this, Inventory, TEXT("MainContent"));
	TestTrue(TEXT("Columns are InventorySection, EquipmentPanel, CraftingPanel"),
		C6ChildNames(MainContent) == TArray<FString>({ TEXT("InventorySection"), TEXT("EquipmentPanel"), TEXT("CraftingPanel") }));
	const UPanelWidget* InventorySection = C6FindWidget<UPanelWidget>(*this, Inventory, TEXT("InventorySection"));
	TestTrue(TEXT("Inventory column holds backpack then details"),
		C6ChildNames(InventorySection) == TArray<FString>({ TEXT("CenterPanel"), TEXT("DetailsOuter") }));
	const UPanelWidget* Character = C6FindWidget<UPanelWidget>(*this, Inventory, TEXT("EquipmentPanel"));
	TestTrue(TEXT("Character column starts with the CHARACTER heading"),
		Character && Character->GetChildrenCount() > 0 && GetNameSafe(Character->GetChildAt(0)) == TEXT("CharacterTitle"));
	UUserWidget* CraftingPanel = C6FindWidget<UUserWidget>(*this, Inventory, TEXT("CraftingPanel"));
	TestTrue(TEXT("Crafting column is a W_CraftingPanel instance"),
		CraftingPanel && CraftingPanel->GetClass() == C6LoadWidgetClass(C6CraftingUIPath, TEXT("W_CraftingPanel")));
	if (const USizeBox* Window = C6FindWidget<USizeBox>(*this, Inventory, TEXT("WindowSize")))
	{
		TestEqual(TEXT("Window width"), Window->GetWidthOverride(), 1680.f);
		TestEqual(TEXT("Window height"), Window->GetHeightOverride(), 760.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC6AInputMappingTest,
	"NullTide.Crafting.C6A.InputMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC6AInputMappingTest::RunTest(const FString& Parameters)
{
	UObject* MappingContext = LoadObject<UObject>(nullptr, TEXT("/Game/TopDown/Input/IMC_Default.IMC_Default"));
	if (!TestNotNull(TEXT("IMC_Default loads"), MappingContext)) { return false; }
	const FStructProperty* DefaultMappings = FindFProperty<FStructProperty>(MappingContext->GetClass(), TEXT("DefaultKeyMappings"));
	const FArrayProperty* Mappings = DefaultMappings ? FindFProperty<FArrayProperty>(DefaultMappings->Struct, TEXT("Mappings")) : nullptr;
	const FStructProperty* MappingStruct = Mappings ? CastField<FStructProperty>(Mappings->Inner) : nullptr;
	if (!TestTrue(TEXT("Mapping array is reflected"), DefaultMappings && Mappings && MappingStruct)) { return false; }

	FScriptArrayHelper Array(Mappings, Mappings->ContainerPtrToValuePtr<void>(DefaultMappings->ContainerPtrToValuePtr<void>(MappingContext)));
	int32 InventoryOnI = 0;
	int32 InventoryOnTab = 0;
	for (int32 Index = 0; Index < Array.Num(); ++Index)
	{
		FString Text;
		MappingStruct->Struct->ExportText(Text, Array.GetRawPtr(Index), nullptr, nullptr, PPF_None, nullptr);
		if (Text.Contains(TEXT("IA_OpeningInventory")))
		{
			InventoryOnI += Text.Contains(TEXT("Key=I,")) || Text.Contains(TEXT("Key=I)")) ? 1 : 0;
			InventoryOnTab += Text.Contains(TEXT("Key=Tab")) ? 1 : 0;
		}
	}
	TestEqual(TEXT("All mappings kept plus Tab"), Array.Num(), 9);
	TestEqual(TEXT("IA_OpeningInventory stays on I"), InventoryOnI, 1);
	TestEqual(TEXT("IA_OpeningInventory is also on Tab"), InventoryOnTab, 1);
	return true;
}

#endif
