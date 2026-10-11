#include "CraftingTestTypes.h"

#include "InventoryComponentTestTypes.h"
#include "Crafting/CraftingRecipeDefinition.h"
#include "Items/ItemDefinition.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
FCraftingItemQuantity MakeEntry(TSubclassOf<UItemDefinition> ItemDefinition, int32 Quantity)
{
	FCraftingItemQuantity Entry;
	Entry.ItemDefinition = ItemDefinition;
	Entry.Quantity = Quantity;
	return Entry;
}
}

UCraftingTestRecipeDefinition::UCraftingTestRecipeDefinition()
{
	RecipeName = FText::FromString(TEXT("Test Recipe"));
	RecipeDescription = FText::FromString(TEXT("Native crafting fixture."));
	Ingredients.Add(MakeEntry(UInventoryStackableTestItemDefinition::StaticClass(), 3));
	Ingredients.Add(MakeEntry(UInventoryTestItemDefinition::StaticClass(), 2));
	Outputs.Add(MakeEntry(UInventoryOtherTestItemDefinition::StaticClass(), 1));
	CraftDuration = 0.f;
}

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
TStrongObjectPtr<UCraftingTestRecipeDefinition> MakeTestRecipe()
{
	return TStrongObjectPtr<UCraftingTestRecipeDefinition>(NewObject<UCraftingTestRecipeDefinition>(GetTransientPackage()));
}

struct FInvalidRecipeCase
{
	const TCHAR* Name;
	TFunction<void(UCraftingRecipeDefinition&)> Mutate;
	const TCHAR* ExpectedDiagnostic;
};

struct FProductionRecipe
{
	const TCHAR* Id;
	const TCHAR* Name;
	const TCHAR* Description;
	TArray<TPair<const TCHAR*, int32>> Ingredients;
	TArray<TPair<const TCHAR*, int32>> Outputs;
	const TCHAR* IconId;
	float CraftDuration = 0.f;
};

const TArray<FProductionRecipe>& ProductionRecipes()
{
	static const TArray<FProductionRecipe> Recipes = {
		{ TEXT("Axe"), TEXT("Axe"), TEXT("Craft a basic woodcutting tool."),
			{ { TEXT("Wood"), 3 }, { TEXT("Stone"), 5 } }, { { TEXT("Axe"), 1 } }, TEXT("Axe") },
		{ TEXT("WoodArrow"), TEXT("Wood Arrow"), TEXT("Craft basic ammunition for bows."),
			{ { TEXT("Wood"), 1 } }, { { TEXT("WoodArrow"), 5 } }, TEXT("WoodArrow") },
		{ TEXT("Pickaxe"), TEXT("Pickaxe"), TEXT("Craft a basic rock-breaking tool."),
			{ { TEXT("Wood"), 2 }, { TEXT("Stone"), 3 } }, { { TEXT("Pickaxe"), 1 } }, TEXT("Pickaxe"), 3.f }
	};
	return Recipes;
}

UClass* LoadItemDefinition(const TCHAR* Id)
{
	const FString Path = FString::Printf(TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_%s.Item_%s_C"), Id, Id);
	return LoadClass<UItemDefinition>(nullptr, *Path);
}

void TestEntries(FAutomationTestBase& Test, const TCHAR* Source, const TArray<FCraftingItemQuantity>& Actual,
	const TArray<TPair<const TCHAR*, int32>>& Expected)
{
	if (!Test.TestEqual(FString::Printf(TEXT("%s count"), Source), Actual.Num(), Expected.Num())) { return; }
	for (int32 Index = 0; Index < Expected.Num(); ++Index)
	{
		Test.TestEqual(FString::Printf(TEXT("%s[%d] item"), Source, Index),
			Actual[Index].ItemDefinition.Get(), LoadItemDefinition(Expected[Index].Key));
		Test.TestEqual(FString::Printf(TEXT("%s[%d] quantity"), Source, Index),
			Actual[Index].Quantity, Expected[Index].Value);
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingRecipeContractTest,
	"NullTide.Crafting.C1.RecipeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingRecipeContractTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Native recipe root is abstract"),
		UCraftingRecipeDefinition::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	TestTrue(TEXT("Native recipe root is const static data"),
		UCraftingRecipeDefinition::StaticClass()->HasAnyClassFlags(CLASS_Const));

	const UCraftingRecipeDefinition* Recipe = UCraftingTestRecipeDefinition::StaticClass()->GetDefaultObject<UCraftingRecipeDefinition>();
	if (!TestNotNull(TEXT("Recipe default object"), Recipe)) { return false; }
	FString Diagnostic;
	TestTrue(TEXT("Valid recipe passes validation"), Recipe->ValidateRecipe(Diagnostic));
	TestTrue(TEXT("Valid recipe has no diagnostic"), Diagnostic.IsEmpty());
	TestEqual(TEXT("Recipe name"), Recipe->RecipeName.ToString(), FString(TEXT("Test Recipe")));
	TestEqual(TEXT("Ingredient count"), Recipe->Ingredients.Num(), 2);
	TestEqual(TEXT("Output count"), Recipe->Outputs.Num(), 1);
	if (Recipe->Ingredients.Num() == 2 && Recipe->Outputs.Num() == 1)
	{
		TestEqual(TEXT("Ingredient order and class"), Recipe->Ingredients[0].ItemDefinition.Get(),
			UInventoryStackableTestItemDefinition::StaticClass());
		TestEqual(TEXT("Ingredient quantity"), Recipe->Ingredients[0].Quantity, 3);
		TestEqual(TEXT("Second ingredient quantity"), Recipe->Ingredients[1].Quantity, 2);
		TestEqual(TEXT("Output class"), Recipe->Outputs[0].ItemDefinition.Get(),
			UInventoryOtherTestItemDefinition::StaticClass());
		TestEqual(TEXT("Output quantity"), Recipe->Outputs[0].Quantity, 1);
	}
	TestEqual(TEXT("Craft duration (instant fixture; timed fixtures are covered by C7)"), Recipe->CraftDuration, 0.f);
	TestNull(TEXT("Hand recipe: no RequiredStation (station fixtures are covered by C8A)"), Recipe->RequiredStation.Get());

	FCraftingItemQuantity DefaultEntry;
	TestNull(TEXT("Default entry has no item"), DefaultEntry.ItemDefinition.Get());
	TestEqual(TEXT("Default entry quantity"), DefaultEntry.Quantity, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingRecipeValidationTest,
	"NullTide.Crafting.C1.ValidationRejectsInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingRecipeValidationTest::RunTest(const FString& Parameters)
{
	const TArray<FInvalidRecipeCase> Cases = {
		{ TEXT("No ingredients"), [](UCraftingRecipeDefinition& R) { R.Ingredients.Reset(); },
			TEXT("Ingredients is empty.") },
		{ TEXT("No outputs"), [](UCraftingRecipeDefinition& R) { R.Outputs.Reset(); },
			TEXT("Outputs is empty.") },
		{ TEXT("Null ingredient"), [](UCraftingRecipeDefinition& R) { R.Ingredients[1].ItemDefinition = nullptr; },
			TEXT("Ingredients[1] has a null or abstract item definition.") },
		{ TEXT("Abstract output"), [](UCraftingRecipeDefinition& R) { R.Outputs[0].ItemDefinition = UItemDefinition::StaticClass(); },
			TEXT("Outputs[0] has a null or abstract item definition.") },
		{ TEXT("Invalid item data"),
			[](UCraftingRecipeDefinition& R) { R.Ingredients[0].ItemDefinition = UInventoryInvalidDataTestDefinition::StaticClass(); },
			TEXT("has invalid item data") },
		{ TEXT("Zero ingredient quantity"), [](UCraftingRecipeDefinition& R) { R.Ingredients[0].Quantity = 0; },
			TEXT("Ingredients[0] quantity 0 is below 1.") },
		{ TEXT("Negative output quantity"), [](UCraftingRecipeDefinition& R) { R.Outputs[0].Quantity = -2; },
			TEXT("Outputs[0] quantity -2 is below 1.") },
		{ TEXT("Duplicate ingredient"),
			[](UCraftingRecipeDefinition& R) { R.Ingredients[1].ItemDefinition = R.Ingredients[0].ItemDefinition; },
			TEXT("Ingredients[0] and Ingredients[1] duplicate item definition") },
		{ TEXT("Duplicate output"), [](UCraftingRecipeDefinition& R) { const FCraftingItemQuantity Copy = R.Outputs[0]; R.Outputs.Add(Copy); },
			TEXT("Outputs[0] and Outputs[1] duplicate item definition") },
		{ TEXT("Negative duration"), [](UCraftingRecipeDefinition& R) { R.CraftDuration = -1.f; },
			TEXT("CraftDuration -1.000 is negative.") }
	};

	for (const FInvalidRecipeCase& Case : Cases)
	{
		TStrongObjectPtr<UCraftingTestRecipeDefinition> Recipe = MakeTestRecipe();
		Case.Mutate(*Recipe);
		FString Diagnostic;
		TestFalse(FString::Printf(TEXT("%s is rejected"), Case.Name), Recipe->ValidateRecipe(Diagnostic));
		TestTrue(FString::Printf(TEXT("%s diagnostic '%s' contains '%s'"), Case.Name, *Diagnostic, Case.ExpectedDiagnostic),
			Diagnostic.Contains(Case.ExpectedDiagnostic));
	}

	TStrongObjectPtr<UCraftingTestRecipeDefinition> SharedItem = MakeTestRecipe();
	SharedItem->Outputs[0].ItemDefinition = SharedItem->Ingredients[0].ItemDefinition;
	FString SharedDiagnostic;
	TestTrue(TEXT("An item may be both an ingredient and an output"), SharedItem->ValidateRecipe(SharedDiagnostic));

#if WITH_EDITOR
	TStrongObjectPtr<UCraftingTestRecipeDefinition> Valid = MakeTestRecipe();
	FDataValidationContext ValidContext;
	TestTrue(TEXT("Editor validation accepts a valid recipe"), Valid->IsDataValid(ValidContext) == EDataValidationResult::Valid);

	TStrongObjectPtr<UCraftingTestRecipeDefinition> Invalid = MakeTestRecipe();
	Invalid->Outputs.Reset();
	FDataValidationContext InvalidContext;
	TestTrue(TEXT("Editor validation rejects an invalid recipe"), Invalid->IsDataValid(InvalidContext) == EDataValidationResult::Invalid);
	TestEqual(TEXT("Editor validation reports one error"), static_cast<int32>(InvalidContext.GetNumErrors()), 1);
#endif
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCraftingProductionRecipesTest, "NullTide.Crafting.C1.ProductionRecipes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FCraftingProductionRecipesTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (const FProductionRecipe& Recipe : ProductionRecipes())
	{
		Names.Add(Recipe.Id);
		Commands.Add(Recipe.Id);
	}
}

bool FCraftingProductionRecipesTest::RunTest(const FString& Parameters)
{
	const FProductionRecipe* Entry = ProductionRecipes().FindByPredicate(
		[&Parameters](const FProductionRecipe& Candidate) { return Parameters == Candidate.Id; });
	if (!TestNotNull(TEXT("Known production recipe"), Entry)) { return false; }
	const FString Path = FString::Printf(TEXT("/Game/LevelPrototyping/CraftingSystem/Recipes/Recipe_%s.Recipe_%s_C"),
		Entry->Id, Entry->Id);
	UClass* Class = LoadClass<UCraftingRecipeDefinition>(nullptr, *Path);
	if (!TestNotNull(TEXT("Saved production recipe loads"), Class)) { return false; }
	TestEqual(TEXT("Direct native parent; no recipe-specific native subclass"), Class->GetSuperClass(),
		UCraftingRecipeDefinition::StaticClass());

	const UCraftingRecipeDefinition* Recipe = Class->GetDefaultObject<UCraftingRecipeDefinition>();
	FString Diagnostic;
	const bool bValid = Recipe->ValidateRecipe(Diagnostic);
	TestTrue(FString::Printf(TEXT("Production recipe is valid: %s"), *Diagnostic), bValid);
	TestEqual(TEXT("Recipe name"), Recipe->RecipeName.ToString(), FString(Entry->Name));
	TestEqual(TEXT("Recipe description"), Recipe->RecipeDescription.ToString(), FString(Entry->Description));
	const FString IconPath = FString::Printf(TEXT("/Game/Items/Icons/T_Icon_%s.T_Icon_%s"), Entry->IconId, Entry->IconId);
	TestEqual(TEXT("Recipe icon reuses the output item icon"), Recipe->RecipeIcon.Get(), LoadObject<UTexture2D>(nullptr, *IconPath));
	TestEntries(*this, TEXT("Ingredients"), Recipe->Ingredients, Entry->Ingredients);
	TestEntries(*this, TEXT("Outputs"), Recipe->Outputs, Entry->Outputs);
	TestEqual(TEXT("Authored craft duration (Axe and WoodArrow instant, Pickaxe 3 s)"), Recipe->CraftDuration, Entry->CraftDuration);
	return true;
}

#endif
