#include "CraftingTestTypes.h"

#include "InventoryComponentTestTypes.h"
#include "Crafting/CraftingComponent.h"
#include "Crafting/CraftingQueryLibrary.h"
#include "Crafting/CraftingRecipeDefinition.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
FCraftingItemQuantity C5Entry(TSubclassOf<UItemDefinition> Definition, int32 Quantity)
{
	FCraftingItemQuantity Entry;
	Entry.ItemDefinition = Definition;
	Entry.Quantity = Quantity;
	return Entry;
}
}

UCraftingOverlapTestRecipe::UCraftingOverlapTestRecipe()
{
	Ingredients.Add(C5Entry(UInventoryStackableTestItemDefinition::StaticClass(), 10));
	Outputs.Add(C5Entry(UInventoryStackableTestItemDefinition::StaticClass(), 5));
}

UCraftingOutputHeavyTestRecipe::UCraftingOutputHeavyTestRecipe()
{
	Ingredients.Add(C5Entry(UInventoryStackableTestItemDefinition::StaticClass(), 1));
	Outputs.Add(C5Entry(UInventoryOtherTestItemDefinition::StaticClass(), 2));
}

UCraftingInvalidTestRecipe::UCraftingInvalidTestRecipe()
{
	Ingredients.Add(C5Entry(UInventoryStackableTestItemDefinition::StaticClass(), 1));
}

void UCraftingComponentTestListener::HandleInventoryChanged(int32 NewRevision)
{
	++InventoryEvents;
	if (bCraftOnInventoryChanged && Crafting)
	{
		ReentrantResults.Add(Crafting->TryCraftRecipe(ReentrantRecipe));
	}
}

void UCraftingComponentTestListener::HandleCraftingResult(TSubclassOf<UCraftingRecipeDefinition> RecipeClass,
	const FCraftingTransactionResult& Result)
{
	++CraftingEvents;
	LastRecipe = RecipeClass;
	LastResult = Result;
	if (bCraftOnCraftingResult && Crafting)
	{
		ReentrantResults.Add(Crafting->TryCraftRecipe(ReentrantRecipe));
	}
}

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using FC5RecipeList = TArray<TSubclassOf<UCraftingRecipeDefinition>>;

TSubclassOf<UItemDefinition> C5Big() { return UInventoryStackableTestItemDefinition::StaticClass(); }
TSubclassOf<UItemDefinition> C5Single() { return UInventoryTestItemDefinition::StaticClass(); }
TSubclassOf<UItemDefinition> C5Other() { return UInventoryOtherTestItemDefinition::StaticClass(); }
TSubclassOf<UCraftingRecipeDefinition> C5TestRecipe() { return UCraftingTestRecipeDefinition::StaticClass(); }      // Big 3 + Single 2 -> Other 1
TSubclassOf<UCraftingRecipeDefinition> C5Overlap() { return UCraftingOverlapTestRecipe::StaticClass(); }            // Big 10 -> Big 5
TSubclassOf<UCraftingRecipeDefinition> C5OutputHeavy() { return UCraftingOutputHeavyTestRecipe::StaticClass(); }    // Big 1 -> Other 2
TSubclassOf<UCraftingRecipeDefinition> C5Invalid() { return UCraftingInvalidTestRecipe::StaticClass(); }

void C5SetRecipes(UCraftingComponent* Crafting, const FC5RecipeList& Recipes)
{
	FArrayProperty* Property = FindFProperty<FArrayProperty>(UCraftingComponent::StaticClass(), TEXT("AvailableRecipes"));
	check(Property);
	*Property->ContainerPtrToValuePtr<FC5RecipeList>(Crafting) = Recipes;
}

struct FC5Stack
{
	FGuid Id;
	TSubclassOf<UItemDefinition> Definition;
	int32 Quantity = 0;

	bool operator==(const FC5Stack& Other) const
	{
		return Id == Other.Id && Definition == Other.Definition && Quantity == Other.Quantity;
	}
};

TArray<FC5Stack> C5Capture(const UInventoryComponent* Inventory)
{
	TArray<FC5Stack> Stacks;
	for (const UItemInstance* Item : Inventory->GetItemsSnapshot())
	{
		FC5Stack& Stack = Stacks.AddDefaulted_GetRef();
		if (Item)
		{
			Stack.Id = Item->GetInstanceId();
			Stack.Definition = Item->GetDefinitionClass();
			Stack.Quantity = Item->GetQuantity();
		}
	}
	return Stacks;
}

/** Owner actor with an inventory and a crafting component, plus a listener on both notifications. */
struct FC5Fixture
{
	TStrongObjectPtr<AActor> Owner;
	TStrongObjectPtr<UInventoryComponent> Inventory;
	TStrongObjectPtr<UCraftingComponent> Crafting;
	TStrongObjectPtr<UCraftingComponentTestListener> Listener;

	explicit FC5Fixture(const FC5RecipeList& Recipes, const bool bWithInventory = true)
		: Owner(NewObject<AActor>(GetTransientPackage()))
		, Crafting(NewObject<UCraftingComponent>(Owner.Get()))
		, Listener(NewObject<UCraftingComponentTestListener>(GetTransientPackage()))
	{
		if (bWithInventory)
		{
			Inventory.Reset(NewObject<UInventoryComponent>(Owner.Get()));
			Owner->AddInstanceComponent(Inventory.Get());
		}
		Owner->AddInstanceComponent(Crafting.Get());
		C5SetRecipes(Crafting.Get(), Recipes);
		Listener->Crafting = Crafting.Get();
	}

	/** Call after setup so only crafting-time notifications count. */
	void StartObserving()
	{
		if (Inventory)
		{
			Inventory->OnInventoryChanged.AddDynamic(Listener.Get(), &UCraftingComponentTestListener::HandleInventoryChanged);
		}
		Crafting->OnCraftingResult.AddDynamic(Listener.Get(), &UCraftingComponentTestListener::HandleCraftingResult);
	}
};

/** Verifies contents, ItemIds, revision and inventory events are unchanged. */
struct FC5UnchangedScope
{
	FAutomationTestBase& Test;
	const FC5Fixture& Fixture;
	const TCHAR* Label;
	TArray<FC5Stack> Stacks;
	int32 Revision;
	int32 InventoryEvents;

	FC5UnchangedScope(FAutomationTestBase& InTest, const FC5Fixture& InFixture, const TCHAR* InLabel)
		: Test(InTest), Fixture(InFixture), Label(InLabel), Stacks(C5Capture(InFixture.Inventory.Get()))
		, Revision(InFixture.Inventory->GetRevision()), InventoryEvents(InFixture.Listener->InventoryEvents)
	{
	}

	~FC5UnchangedScope()
	{
		Test.TestTrue(FString::Printf(TEXT("%s: contents and ItemIds unchanged"), Label), C5Capture(Fixture.Inventory.Get()) == Stacks);
		Test.TestEqual(FString::Printf(TEXT("%s: revision unchanged"), Label), Fixture.Inventory->GetRevision(), Revision);
		Test.TestEqual(FString::Printf(TEXT("%s: no inventory event"), Label), Fixture.Listener->InventoryEvents, InventoryEvents);
	}
};

bool C5Add(FAutomationTestBase& Test, UInventoryComponent* Inventory, TSubclassOf<UItemDefinition> Definition, int32 Quantity)
{
	return Test.TestTrue(FString::Printf(TEXT("Setup adds %d of %s"), Quantity, *GetNameSafe(Definition.Get())),
		Inventory->TryAddDefinitionQuantity(Definition, Quantity).IsSuccess());
}

bool C5Is(const FCraftingTransactionResult& Result, ECraftingTransactionResult Expected)
{
	return Result.Result == Expected;
}

UClass* C5LoadItem(const TCHAR* Id)
{
	return LoadClass<UItemDefinition>(nullptr,
		*FString::Printf(TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_%s.Item_%s_C"), Id, Id));
}

UClass* C5LoadRecipe(const TCHAR* Id)
{
	return LoadClass<UCraftingRecipeDefinition>(nullptr,
		*FString::Printf(TEXT("/Game/LevelPrototyping/CraftingSystem/Recipes/Recipe_%s.Recipe_%s_C"), Id, Id));
}

bool C5HasFunctionFlags(const TCHAR* Name, EFunctionFlags Flags)
{
	const UFunction* Function = UCraftingComponent::StaticClass()->FindFunctionByName(Name);
	return Function && Function->HasAllFunctionFlags(Flags);
}

/** Calls TryCraftRecipe through the reflected Blueprint entry point. */
FCraftingTransactionResult C5CallTryCraftThroughReflection(UCraftingComponent* Crafting, UClass* RecipeClass)
{
	UFunction* Function = UCraftingComponent::StaticClass()->FindFunctionByName(TEXT("TryCraftRecipe"));
	check(Function);
	uint8* Params = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
	FMemory::Memzero(Params, Function->ParmsSize);
	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->InitializeValue_InContainer(Params);
	}
	CastFieldChecked<FClassProperty>(Function->FindPropertyByName(TEXT("RecipeClass")))->SetObjectPropertyValue_InContainer(Params, RecipeClass);
	Crafting->ProcessEvent(Function, Params);
	const FCraftingTransactionResult Result =
		*CastFieldChecked<FStructProperty>(Function->GetReturnProperty())->ContainerPtrToValuePtr<FCraftingTransactionResult>(Params);
	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(Params);
	}
	return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC5InventoryResolutionTest,
	"NullTide.Crafting.C5.InventoryResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC5InventoryResolutionTest::RunTest(const FString& Parameters)
{
	FC5Fixture WithInventory({ C5TestRecipe() });
	TestTrue(TEXT("Resolves the owner's inventory component"), WithInventory.Crafting->GetInventory() == WithInventory.Inventory.Get());

	FC5Fixture NoInventory({ C5TestRecipe() }, false);
	NoInventory.StartObserving();
	TestNull(TEXT("Owner without inventory resolves nothing"), NoInventory.Crafting->GetInventory());
	TestTrue(TEXT("Preflight without inventory is InvalidInventory"),
		C5Is(NoInventory.Crafting->CanCraftRecipe(C5TestRecipe()), ECraftingTransactionResult::InvalidInventory));
	TestEqual(TEXT("Preflight does not notify"), NoInventory.Listener->CraftingEvents, 0);
	TestTrue(TEXT("Craft without inventory is InvalidInventory"),
		C5Is(NoInventory.Crafting->TryCraftRecipe(C5TestRecipe()), ECraftingTransactionResult::InvalidInventory));
	TestEqual(TEXT("A failed craft notifies once"), NoInventory.Listener->CraftingEvents, 1);

	const TStrongObjectPtr<UCraftingComponent> Unowned(NewObject<UCraftingComponent>(GetTransientPackage()));
	C5SetRecipes(Unowned.Get(), { C5TestRecipe() });
	TestNull(TEXT("Unowned component resolves nothing"), Unowned->GetInventory());
	TestTrue(TEXT("Unowned component cannot craft"),
		C5Is(Unowned->TryCraftRecipe(C5TestRecipe()), ECraftingTransactionResult::InvalidInventory));

	FC5Fixture Legacy({ C5TestRecipe() });
	FBoolProperty* LegacyModeProperty = FindFProperty<FBoolProperty>(UInventoryComponent::StaticClass(), TEXT("bLegacyInventoryMode"));
	if (!TestNotNull(TEXT("Legacy mode flag exists"), LegacyModeProperty)) { return false; }
	LegacyModeProperty->SetPropertyValue_InContainer(Legacy.Inventory.Get(), true);
	TestTrue(TEXT("Inactive inventory authority is InvalidInventory"),
		C5Is(Legacy.Crafting->CanCraftRecipe(C5TestRecipe()), ECraftingTransactionResult::InvalidInventory));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC5RegistrationTest,
	"NullTide.Crafting.C5.RecipeRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC5RegistrationTest::RunTest(const FString& Parameters)
{
	FC5Fixture Fixture({ C5TestRecipe(), nullptr, C5Overlap(), C5TestRecipe(), UCraftingRecipeDefinition::StaticClass(),
		C5Invalid(), C5OutputHeavy() });
	const FC5RecipeList Listed = Fixture.Crafting->GetAvailableRecipes();
	TestTrue(TEXT("Listing keeps authored order and drops null, duplicate, abstract and invalid entries"),
		Listed == FC5RecipeList({ C5TestRecipe(), C5Overlap(), C5OutputHeavy() }));
	TestTrue(TEXT("Listing is stable across calls"), Fixture.Crafting->GetAvailableRecipes() == Listed);
	TestTrue(TEXT("Registered valid recipe is available"), Fixture.Crafting->IsRecipeAvailable(C5Overlap()));
	TestFalse(TEXT("Registered invalid recipe is not available"), Fixture.Crafting->IsRecipeAvailable(C5Invalid()));
	TestFalse(TEXT("Null is not available"), Fixture.Crafting->IsRecipeAvailable(nullptr));

#if WITH_EDITOR
	FDataValidationContext Context;
	TestTrue(TEXT("Editor validation rejects the bad registration list"),
		Fixture.Crafting->IsDataValid(Context) == EDataValidationResult::Invalid);
	TestEqual(TEXT("One error each for null, duplicate, abstract and invalid entries"), static_cast<int32>(Context.GetNumErrors()), 4);

	FC5Fixture Clean({ C5TestRecipe(), C5Overlap() });
	FDataValidationContext CleanContext;
	TestTrue(TEXT("Editor validation accepts a clean registration list"),
		Clean.Crafting->IsDataValid(CleanContext) == EDataValidationResult::Valid);
#endif

	// Production registration on the player Blueprint.
	UBlueprint* PlayerBlueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/TopDown/Blueprints/BP_TopDownCharacter.BP_TopDownCharacter"));
	if (!TestNotNull(TEXT("Production player Blueprint loads"), PlayerBlueprint)) { return false; }
	TArray<const UCraftingComponent*> Templates;
	if (const USimpleConstructionScript* ConstructionScript = PlayerBlueprint->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : ConstructionScript->GetAllNodes())
		{
			if (const UCraftingComponent* Template = Node ? Cast<UCraftingComponent>(Node->ComponentTemplate) : nullptr)
			{
				Templates.Add(Template);
			}
		}
	}
	if (!TestEqual(TEXT("Production player has exactly one CraftingComponent"), Templates.Num(), 1)) { return false; }
	UClass* RecipeAxe = C5LoadRecipe(TEXT("Axe"));
	UClass* RecipeWoodArrow = C5LoadRecipe(TEXT("WoodArrow"));
	UClass* RecipePickaxe = C5LoadRecipe(TEXT("Pickaxe"));
	TestTrue(TEXT("Production recipes load"), RecipeAxe && RecipeWoodArrow && RecipePickaxe);
	TestTrue(TEXT("Production registration is Recipe_Axe, Recipe_WoodArrow, Recipe_Pickaxe in that order"),
		Templates[0]->GetAvailableRecipes() == FC5RecipeList({ RecipeAxe, RecipeWoodArrow, RecipePickaxe }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC5RejectionTest,
	"NullTide.Crafting.C5.RequestRejection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC5RejectionTest::RunTest(const FString& Parameters)
{
	FC5Fixture Fixture({ C5TestRecipe(), C5Invalid() });
	if (!C5Add(*this, Fixture.Inventory.Get(), C5Big(), 20) || !C5Add(*this, Fixture.Inventory.Get(), C5Single(), 2)) { return false; }
	Fixture.StartObserving();
	{
		FC5UnchangedScope Unchanged(*this, Fixture, TEXT("Rejected requests"));
		const FCraftingTransactionResult Null = Fixture.Crafting->TryCraftRecipe(nullptr);
		TestTrue(TEXT("Null recipe is InvalidRecipe"), C5Is(Null, ECraftingTransactionResult::InvalidRecipe));
		const FCraftingTransactionResult Abstract = Fixture.Crafting->TryCraftRecipe(UCraftingRecipeDefinition::StaticClass());
		TestTrue(TEXT("Abstract recipe is InvalidRecipe"), C5Is(Abstract, ECraftingTransactionResult::InvalidRecipe));
		const FCraftingTransactionResult Invalid = Fixture.Crafting->TryCraftRecipe(C5Invalid());
		TestTrue(TEXT("Registered but invalid recipe is InvalidRecipe"), C5Is(Invalid, ECraftingTransactionResult::InvalidRecipe));
		TestTrue(TEXT("Invalid recipe carries the validation diagnostic"), Invalid.Diagnostic.Contains(TEXT("Outputs is empty")));
		const FCraftingTransactionResult Unregistered = Fixture.Crafting->TryCraftRecipe(C5Overlap());
		TestTrue(TEXT("Valid but unregistered recipe is RecipeNotAvailable"),
			C5Is(Unregistered, ECraftingTransactionResult::RecipeNotAvailable));
		TestFalse(TEXT("Unregistered result carries a diagnostic"), Unregistered.Diagnostic.IsEmpty());
		TestTrue(TEXT("Preflight agrees for unregistered recipes"),
			C5Is(Fixture.Crafting->CanCraftRecipe(C5Overlap()), ECraftingTransactionResult::RecipeNotAvailable));
		TestTrue(TEXT("Preflight agrees for null recipes"),
			C5Is(Fixture.Crafting->CanCraftRecipe(nullptr), ECraftingTransactionResult::InvalidRecipe));
	}
	TestEqual(TEXT("Each rejected craft notified once; preflights never notify"), Fixture.Listener->CraftingEvents, 4);
	TestTrue(TEXT("The last notification names the requested recipe"), Fixture.Listener->LastRecipe == C5Overlap());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC5PreflightTest,
	"NullTide.Crafting.C5.ExactPreflight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC5PreflightTest::RunTest(const FString& Parameters)
{
	// Overlap: Big 50/50 in a full inventory, recipe Big x10 -> Big x5.
	FC5Fixture Overlap({ C5Overlap(), C5TestRecipe(), C5OutputHeavy() });
	if (!C5Add(*this, Overlap.Inventory.Get(), C5Big(), 50) || !C5Add(*this, Overlap.Inventory.Get(), C5Other(), 23)) { return false; }
	Overlap.StartObserving();
	{
		FC5UnchangedScope Unchanged(*this, Overlap, TEXT("Exact preflight"));
		TestTrue(TEXT("Conservative C2 output check says full"),
			UCraftingQueryLibrary::CheckRecipeOutputCapacity(Overlap.Inventory.Get(), C5Overlap()).Result
				== ECraftingQueryResult::OutputCapacityFull);
		TestTrue(TEXT("Exact CanCraft accounts for consumed ingredients"), Overlap.Crafting->CanCraftRecipe(C5Overlap()).IsSuccess());

		const FCraftingTransactionResult Missing = Overlap.Crafting->CanCraftRecipe(C5TestRecipe());
		TestTrue(TEXT("Missing ingredients are reported exactly"), C5Is(Missing, ECraftingTransactionResult::InsufficientIngredients));
		TestEqual(TEXT("Single is ingredient index 1"), Missing.EntryIndex, 1);

		const FCraftingTransactionResult NoRoom = Overlap.Crafting->CanCraftRecipe(C5OutputHeavy());
		TestTrue(TEXT("Outputs that do not fit after consumption are reported"), C5Is(NoRoom, ECraftingTransactionResult::OutputCapacityFull));
		TestEqual(TEXT("Output index is reported"), NoRoom.EntryIndex, 0);
	}
	TestEqual(TEXT("Preflights never notify"), Overlap.Listener->CraftingEvents, 0);

	// The commit agrees with every preflight.
	for (const TSubclassOf<UCraftingRecipeDefinition>& Recipe : FC5RecipeList({ C5TestRecipe(), C5OutputHeavy(), C5Overlap() }))
	{
		const ECraftingTransactionResult Predicted = Overlap.Crafting->CanCraftRecipe(Recipe).Result;
		TestTrue(FString::Printf(TEXT("TryCraft agrees with CanCraft for %s"), *GetNameSafe(Recipe.Get())),
			Overlap.Crafting->TryCraftRecipe(Recipe).Result == Predicted);
	}
	TestEqual(TEXT("The overlap craft left 45 in the same stack"), Overlap.Inventory->GetTotalQuantity(C5Big()), 45);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC5FailureTest,
	"NullTide.Crafting.C5.CraftFailures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC5FailureTest::RunTest(const FString& Parameters)
{
	FC5Fixture Fixture({ C5TestRecipe(), C5OutputHeavy() });
	if (!C5Add(*this, Fixture.Inventory.Get(), C5Big(), 5) || !C5Add(*this, Fixture.Inventory.Get(), C5Single(), 23)) { return false; }
	Fixture.StartObserving();
	{
		// Big 5 -> 4 keeps its stack, so nothing is freed for the two Other outputs.
		FC5UnchangedScope Unchanged(*this, Fixture, TEXT("Output capacity"));
		const FCraftingTransactionResult Result = Fixture.Crafting->TryCraftRecipe(C5OutputHeavy());
		TestTrue(TEXT("Two outputs cannot fit a full inventory"), C5Is(Result, ECraftingTransactionResult::OutputCapacityFull));
		TestEqual(TEXT("Failure notification carries the same result"), static_cast<int32>(Fixture.Listener->LastResult.Result),
			static_cast<int32>(ECraftingTransactionResult::OutputCapacityFull));
	}

	FC5Fixture Short({ C5TestRecipe() });
	if (!C5Add(*this, Short.Inventory.Get(), C5Big(), 2)) { return false; }
	Short.StartObserving();
	{
		FC5UnchangedScope Unchanged(*this, Short, TEXT("Insufficient ingredients"));
		const FCraftingTransactionResult Result = Short.Crafting->TryCraftRecipe(C5TestRecipe());
		TestTrue(TEXT("Missing ingredients are rejected"), C5Is(Result, ECraftingTransactionResult::InsufficientIngredients));
		TestEqual(TEXT("Big is ingredient index 0"), Result.EntryIndex, 0);
	}
	TestEqual(TEXT("One failure notification"), Short.Listener->CraftingEvents, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC5ProductionTest,
	"NullTide.Crafting.C5.ProductionRecipes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC5ProductionTest::RunTest(const FString& Parameters)
{
	UClass* Wood = C5LoadItem(TEXT("Wood"));
	UClass* Stone = C5LoadItem(TEXT("Stone"));
	UClass* Axe = C5LoadItem(TEXT("Axe"));
	UClass* WoodArrow = C5LoadItem(TEXT("WoodArrow"));
	UClass* RecipeAxe = C5LoadRecipe(TEXT("Axe"));
	UClass* RecipeWoodArrow = C5LoadRecipe(TEXT("WoodArrow"));
	if (!TestTrue(TEXT("Production items and recipes load"), Wood && Stone && Axe && WoodArrow && RecipeAxe && RecipeWoodArrow))
	{
		return false;
	}

	FC5Fixture Fixture({ RecipeAxe, RecipeWoodArrow });
	if (!C5Add(*this, Fixture.Inventory.Get(), Wood, 10) || !C5Add(*this, Fixture.Inventory.Get(), Stone, 5)
		|| !C5Add(*this, Fixture.Inventory.Get(), WoodArrow, 48)) { return false; }
	const TArray<FC5Stack> Before = C5Capture(Fixture.Inventory.Get());
	Fixture.StartObserving();

	TestTrue(TEXT("Recipe_Axe is craftable"), Fixture.Crafting->CanCraftRecipe(RecipeAxe).IsSuccess());
	TestTrue(TEXT("Recipe_Axe crafts"), Fixture.Crafting->TryCraftRecipe(RecipeAxe).IsSuccess());
	const TArray<FC5Stack> AfterAxe = C5Capture(Fixture.Inventory.Get());
	// [Wood 10, Stone 5, WoodArrow 48] -> [Wood 7, WoodArrow 48, Axe 1]
	if (TestEqual(TEXT("Three stacks after Recipe_Axe"), AfterAxe.Num(), 3))
	{
		TestTrue(TEXT("Wood 7 keeps its ItemId"), AfterAxe[0].Id == Before[0].Id && AfterAxe[0].Quantity == 7);
		TestTrue(TEXT("WoodArrow is untouched"), AfterAxe[1] == Before[2]);
		TestTrue(TEXT("Axe is a new stack"), AfterAxe[2].Definition == Axe && AfterAxe[2].Quantity == 1
			&& !Before.ContainsByPredicate([&AfterAxe](const FC5Stack& Stack) { return Stack.Id == AfterAxe[2].Id; }));
	}
	TestFalse(TEXT("Stone stack ItemId is gone"),
		AfterAxe.ContainsByPredicate([&Before](const FC5Stack& Stack) { return Stack.Id == Before[1].Id; }));

	TestTrue(TEXT("Recipe_WoodArrow crafts"), Fixture.Crafting->TryCraftRecipe(RecipeWoodArrow).IsSuccess());
	const TArray<FC5Stack> AfterArrow = C5Capture(Fixture.Inventory.Get());
	// -> [Wood 6, WoodArrow 50, Axe 1, WoodArrow 3]
	if (TestEqual(TEXT("Four stacks after Recipe_WoodArrow"), AfterArrow.Num(), 4))
	{
		TestTrue(TEXT("Wood 6 keeps its ItemId"), AfterArrow[0].Id == Before[0].Id && AfterArrow[0].Quantity == 6);
		TestTrue(TEXT("WoodArrow merges to 50 in the same stack"), AfterArrow[1].Id == Before[2].Id && AfterArrow[1].Quantity == 50);
		TestTrue(TEXT("Axe is untouched"), AfterArrow[2] == AfterAxe[2]);
		TestTrue(TEXT("WoodArrow overflow is a new stack of 3"), AfterArrow[3].Definition == WoodArrow && AfterArrow[3].Quantity == 3
			&& !AfterAxe.ContainsByPredicate([&AfterArrow](const FC5Stack& Stack) { return Stack.Id == AfterArrow[3].Id; }));
	}
	TestEqual(TEXT("Two crafts, two inventory events"), Fixture.Listener->InventoryEvents, 2);
	TestEqual(TEXT("Two crafts, two crafting notifications"), Fixture.Listener->CraftingEvents, 2);
	TestTrue(TEXT("Last notification names Recipe_WoodArrow"), Fixture.Listener->LastRecipe == RecipeWoodArrow);
	{
		FC5UnchangedScope Unchanged(*this, Fixture, TEXT("Recipe_Axe without Stone"));
		const FCraftingTransactionResult Again = Fixture.Crafting->TryCraftRecipe(RecipeAxe);
		TestTrue(TEXT("Second Recipe_Axe lacks Stone"), C5Is(Again, ECraftingTransactionResult::InsufficientIngredients));
		TestEqual(TEXT("Stone is ingredient index 1"), Again.EntryIndex, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC5EventContractTest,
	"NullTide.Crafting.C5.RevisionAndEventContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC5EventContractTest::RunTest(const FString& Parameters)
{
	FC5Fixture Fixture({ C5TestRecipe() });
	if (!C5Add(*this, Fixture.Inventory.Get(), C5Big(), 20) || !C5Add(*this, Fixture.Inventory.Get(), C5Single(), 4)) { return false; }
	Fixture.StartObserving();
	const int32 RevisionBefore = Fixture.Inventory->GetRevision();

	const FCraftingTransactionResult Result = Fixture.Crafting->TryCraftRecipe(C5TestRecipe());
	TestTrue(TEXT("Craft succeeds"), Result.IsSuccess());
	TestEqual(TEXT("Exactly one revision increment"), Fixture.Inventory->GetRevision(), RevisionBefore + 1);
	TestEqual(TEXT("Exactly one inventory broadcast"), Fixture.Listener->InventoryEvents, 1);
	TestEqual(TEXT("Exactly one crafting notification"), Fixture.Listener->CraftingEvents, 1);
	TestTrue(TEXT("Notification names the recipe"), Fixture.Listener->LastRecipe == C5TestRecipe());
	TestTrue(TEXT("Notification carries the success result"), Fixture.Listener->LastResult.IsSuccess());

	for (int32 Index = 0; Index < 3; ++Index)
	{
		Fixture.Crafting->CanCraftRecipe(C5TestRecipe());
	}
	TestEqual(TEXT("Preflight changes no revision"), Fixture.Inventory->GetRevision(), RevisionBefore + 1);
	TestEqual(TEXT("Preflight emits no inventory event"), Fixture.Listener->InventoryEvents, 1);
	TestEqual(TEXT("Preflight emits no crafting notification"), Fixture.Listener->CraftingEvents, 1);

	TestTrue(TEXT("Second craft succeeds (Big 17, Single 2)"), Fixture.Crafting->TryCraftRecipe(C5TestRecipe()).IsSuccess());
	{
		FC5UnchangedScope Unchanged(*this, Fixture, TEXT("Third craft without Single"));
		TestTrue(TEXT("Third craft fails"), C5Is(Fixture.Crafting->TryCraftRecipe(C5TestRecipe()),
			ECraftingTransactionResult::InsufficientIngredients));
	}
	TestEqual(TEXT("Two successes, two revisions"), Fixture.Inventory->GetRevision(), RevisionBefore + 2);
	TestEqual(TEXT("Three crafts, three notifications"), Fixture.Listener->CraftingEvents, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC5ReentrancyTest,
	"NullTide.Crafting.C5.Reentrancy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC5ReentrancyTest::RunTest(const FString& Parameters)
{
	// Reentrant craft from the inventory broadcast of this craft's commit.
	FC5Fixture FromInventory({ C5TestRecipe() });
	if (!C5Add(*this, FromInventory.Inventory.Get(), C5Big(), 20) || !C5Add(*this, FromInventory.Inventory.Get(), C5Single(), 4)) { return false; }
	FromInventory.Listener->bCraftOnInventoryChanged = true;
	FromInventory.Listener->ReentrantRecipe = C5TestRecipe();
	FromInventory.StartObserving();
	TestTrue(TEXT("Outer craft succeeds"), FromInventory.Crafting->TryCraftRecipe(C5TestRecipe()).IsSuccess());
	TestTrue(TEXT("Reentrant craft from the inventory event is CraftInProgress"),
		FromInventory.Listener->ReentrantResults.Num() == 1
			&& C5Is(FromInventory.Listener->ReentrantResults[0], ECraftingTransactionResult::CraftInProgress));
	TestEqual(TEXT("Only the outer craft consumed ingredients"), FromInventory.Inventory->GetTotalQuantity(C5Big()), 17);
	TestEqual(TEXT("Only the outer craft notified"), FromInventory.Listener->CraftingEvents, 1);
	TestEqual(TEXT("Only one inventory event"), FromInventory.Listener->InventoryEvents, 1);

	// Reentrant craft from the crafting notification.
	FC5Fixture FromNotification({ C5TestRecipe() });
	if (!C5Add(*this, FromNotification.Inventory.Get(), C5Big(), 20) || !C5Add(*this, FromNotification.Inventory.Get(), C5Single(), 4)) { return false; }
	FromNotification.Listener->bCraftOnCraftingResult = true;
	FromNotification.Listener->ReentrantRecipe = C5TestRecipe();
	FromNotification.StartObserving();
	TestTrue(TEXT("Outer craft succeeds"), FromNotification.Crafting->TryCraftRecipe(C5TestRecipe()).IsSuccess());
	TestTrue(TEXT("Reentrant craft from the crafting notification is CraftInProgress"),
		FromNotification.Listener->ReentrantResults.Num() == 1
			&& C5Is(FromNotification.Listener->ReentrantResults[0], ECraftingTransactionResult::CraftInProgress));
	TestEqual(TEXT("No nested notification"), FromNotification.Listener->CraftingEvents, 1);
	TestEqual(TEXT("One craft applied"), FromNotification.Inventory->GetTotalQuantity(C5Single()), 2);
	TestTrue(TEXT("The guard releases after the call"), FromNotification.Crafting->CanCraftRecipe(C5TestRecipe()).IsSuccess());

	// A craft attempted while the inventory applies an unrelated mutation is a real, failed attempt.
	FC5Fixture DuringMutation({ C5TestRecipe() });
	if (!C5Add(*this, DuringMutation.Inventory.Get(), C5Big(), 20) || !C5Add(*this, DuringMutation.Inventory.Get(), C5Single(), 4)) { return false; }
	DuringMutation.Listener->bCraftOnInventoryChanged = true;
	DuringMutation.Listener->ReentrantRecipe = C5TestRecipe();
	DuringMutation.StartObserving();
	TestTrue(TEXT("Unrelated add succeeds"), DuringMutation.Inventory->TryAddDefinitionQuantity(C5Big(), 1).IsSuccess());
	TestTrue(TEXT("Craft during the inventory broadcast is InventoryBusy"),
		DuringMutation.Listener->ReentrantResults.Num() == 1
			&& C5Is(DuringMutation.Listener->ReentrantResults[0], ECraftingTransactionResult::InventoryBusy));
	TestEqual(TEXT("The busy attempt notified once"), DuringMutation.Listener->CraftingEvents, 1);
	TestEqual(TEXT("Only the add changed the inventory"), DuringMutation.Inventory->GetTotalQuantity(C5Big()), 21);
	TestEqual(TEXT("Only the add broadcast"), DuringMutation.Listener->InventoryEvents, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC5BlueprintApiTest,
	"NullTide.Crafting.C5.BlueprintApi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC5BlueprintApiTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("GetAvailableRecipes is BlueprintPure"),
		C5HasFunctionFlags(TEXT("GetAvailableRecipes"), FUNC_BlueprintCallable | FUNC_BlueprintPure));
	TestTrue(TEXT("CanCraftRecipe is BlueprintPure"), C5HasFunctionFlags(TEXT("CanCraftRecipe"), FUNC_BlueprintCallable | FUNC_BlueprintPure));
	TestTrue(TEXT("TryCraftRecipe is BlueprintCallable"), C5HasFunctionFlags(TEXT("TryCraftRecipe"), FUNC_BlueprintCallable));
	TestFalse(TEXT("TryCraftRecipe is not pure"), C5HasFunctionFlags(TEXT("TryCraftRecipe"), FUNC_BlueprintPure));
	const FMulticastDelegateProperty* Notification =
		FindFProperty<FMulticastDelegateProperty>(UCraftingComponent::StaticClass(), TEXT("OnCraftingResult"));
	TestTrue(TEXT("OnCraftingResult is BlueprintAssignable"), Notification && Notification->HasAnyPropertyFlags(CPF_BlueprintAssignable));
	TestNull(TEXT("Inventory transaction commit is not Blueprint-exposed"),
		UInventoryComponent::StaticClass()->FindFunctionByName(TEXT("TryApplyDefinitionTransaction")));
	TestNull(TEXT("Inventory transaction preflight is not Blueprint-exposed"),
		UInventoryComponent::StaticClass()->FindFunctionByName(TEXT("CanApplyDefinitionTransaction")));

	FC5Fixture Fixture({ C5TestRecipe() });
	if (!C5Add(*this, Fixture.Inventory.Get(), C5Big(), 3) || !C5Add(*this, Fixture.Inventory.Get(), C5Single(), 2)) { return false; }
	Fixture.StartObserving();
	const FCraftingTransactionResult Result = C5CallTryCraftThroughReflection(Fixture.Crafting.Get(), C5TestRecipe());
	TestTrue(TEXT("The reflected Blueprint entry point crafts"), Result.IsSuccess());
	TestEqual(TEXT("Ingredients consumed through the Blueprint path"), Fixture.Inventory->GetTotalQuantity(C5Big()), 0);
	TestEqual(TEXT("Output created through the Blueprint path"), Fixture.Inventory->GetTotalQuantity(C5Other()), 1);
	TestEqual(TEXT("Blueprint path notifies once"), Fixture.Listener->CraftingEvents, 1);
	const FCraftingTransactionResult Rejected = C5CallTryCraftThroughReflection(Fixture.Crafting.Get(), C5Overlap());
	TestTrue(TEXT("The Blueprint path rejects unregistered recipes"), C5Is(Rejected, ECraftingTransactionResult::RecipeNotAvailable));
	return true;
}

#endif
