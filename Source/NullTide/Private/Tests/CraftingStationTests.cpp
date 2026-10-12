#include "CraftingTestTypes.h"

#include "InventoryComponentTestTypes.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Crafting/CraftingComponent.h"
#include "Crafting/CraftingRecipeDefinition.h"
#include "Crafting/CraftingStationActor.h"
#include "Crafting/CraftingStationComponent.h"
#include "Crafting/CraftingStationDefinition.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/PlayerController.h"
#include "UI/InventoryScreenWidget.h"
#include "UObject/UObjectIterator.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
FCraftingItemQuantity C8AEntry(TSubclassOf<UItemDefinition> Definition, int32 Quantity)
{
	FCraftingItemQuantity Entry;
	Entry.ItemDefinition = Definition;
	Entry.Quantity = Quantity;
	return Entry;
}
}

UCraftingTestStationDefinition::UCraftingTestStationDefinition()
{
	StationName = FText::FromString(TEXT("Test Bench"));
}

UCraftingOtherTestStationDefinition::UCraftingOtherTestStationDefinition()
{
	StationName = FText::FromString(TEXT("Test Forge"));
}

UCraftingInvalidTestStationDefinition::UCraftingInvalidTestStationDefinition()
{
	StationName = FText::FromString(TEXT("Broken Station"));
	UseRange = 0.f;
}

UCraftingStationTimedTestRecipe::UCraftingStationTimedTestRecipe()
{
	Ingredients.Add(C8AEntry(UInventoryStackableTestItemDefinition::StaticClass(), 3));
	Ingredients.Add(C8AEntry(UInventoryTestItemDefinition::StaticClass(), 1));
	Outputs.Add(C8AEntry(UInventoryOtherTestItemDefinition::StaticClass(), 1));
	CraftDuration = 2.f;
	RequiredStation = UCraftingTestStationDefinition::StaticClass();
}

UCraftingStationInstantTestRecipe::UCraftingStationInstantTestRecipe()
{
	Ingredients.Add(C8AEntry(UInventoryStackableTestItemDefinition::StaticClass(), 2));
	Outputs.Add(C8AEntry(UInventoryOtherTestItemDefinition::StaticClass(), 1));
	RequiredStation = UCraftingTestStationDefinition::StaticClass();
}

UCraftingOtherStationTestRecipe::UCraftingOtherStationTestRecipe()
{
	Ingredients.Add(C8AEntry(UInventoryStackableTestItemDefinition::StaticClass(), 1));
	Outputs.Add(C8AEntry(UInventoryOtherTestItemDefinition::StaticClass(), 1));
	RequiredStation = UCraftingOtherTestStationDefinition::StaticClass();
}

UCraftingAbstractStationTestRecipe::UCraftingAbstractStationTestRecipe()
{
	Ingredients.Add(C8AEntry(UInventoryStackableTestItemDefinition::StaticClass(), 1));
	Outputs.Add(C8AEntry(UInventoryOtherTestItemDefinition::StaticClass(), 1));
	RequiredStation = UCraftingStationDefinition::StaticClass();
}

UCraftingInvalidStationTestRecipe::UCraftingInvalidStationTestRecipe()
{
	Ingredients.Add(C8AEntry(UInventoryStackableTestItemDefinition::StaticClass(), 1));
	Outputs.Add(C8AEntry(UInventoryOtherTestItemDefinition::StaticClass(), 1));
	RequiredStation = UCraftingInvalidTestStationDefinition::StaticClass();
}

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using FC8ARecipeList = TArray<TSubclassOf<UCraftingRecipeDefinition>>;
using FC8AStacks = TArray<TPair<FGuid, int32>>;

TSubclassOf<UItemDefinition> C8ABig() { return UInventoryStackableTestItemDefinition::StaticClass(); }
TSubclassOf<UItemDefinition> C8ASingle() { return UInventoryTestItemDefinition::StaticClass(); }
TSubclassOf<UItemDefinition> C8AOther() { return UInventoryOtherTestItemDefinition::StaticClass(); }
TSubclassOf<UCraftingStationDefinition> C8ABench() { return UCraftingTestStationDefinition::StaticClass(); }
TSubclassOf<UCraftingStationDefinition> C8AForge() { return UCraftingOtherTestStationDefinition::StaticClass(); }
TSubclassOf<UCraftingStationDefinition> C8ABrokenStation() { return UCraftingInvalidTestStationDefinition::StaticClass(); }
TSubclassOf<UCraftingRecipeDefinition> C8ABenchTimed() { return UCraftingStationTimedTestRecipe::StaticClass(); }     // Bench, 2 s
TSubclassOf<UCraftingRecipeDefinition> C8ABenchInstant() { return UCraftingStationInstantTestRecipe::StaticClass(); } // Bench, instant
TSubclassOf<UCraftingRecipeDefinition> C8AForgeRecipe() { return UCraftingOtherStationTestRecipe::StaticClass(); }  // Forge, instant
TSubclassOf<UCraftingRecipeDefinition> C8AHandTimed() { return UCraftingTimedTestRecipe::StaticClass(); }          // hand, 2 s
TSubclassOf<UCraftingRecipeDefinition> C8AHandInstant() { return UCraftingTestRecipeDefinition::StaticClass(); }   // hand, instant

const FName C8AStationEndPlayHandler(TEXT("HandleActiveStationEndPlay"));

void C8ASetRecipes(UCraftingComponent* Crafting, const FC8ARecipeList& Recipes)
{
	FArrayProperty* Property = FindFProperty<FArrayProperty>(UCraftingComponent::StaticClass(), TEXT("AvailableRecipes"));
	check(Property);
	*Property->ContainerPtrToValuePtr<FC8ARecipeList>(Crafting) = Recipes;
}

void C8ASetStationDefinition(UCraftingStationComponent* Station, TSubclassOf<UCraftingStationDefinition> Definition)
{
	FClassProperty* Property = FindFProperty<FClassProperty>(UCraftingStationComponent::StaticClass(), TEXT("StationDefinition"));
	check(Property);
	*Property->ContainerPtrToValuePtr<TSubclassOf<UCraftingStationDefinition>>(Station) = Definition;
}

FC8AStacks C8ACapture(const UInventoryComponent* Inventory)
{
	FC8AStacks Stacks;
	for (const UItemInstance* Item : Inventory->GetItemsSnapshot())
	{
		Stacks.Emplace(Item ? Item->GetInstanceId() : FGuid(), Item ? Item->GetQuantity() : 0);
	}
	return Stacks;
}

bool C8AIs(const FCraftingTransactionResult& Result, ECraftingTransactionResult Expected)
{
	return Result.Result == Expected;
}

/**
 * The C7 fixture plus stations: a character at (0, 0, 100) with inventory and crafting components in a
 * transient world. Stations are plain actors with a scene root and a UCraftingStationComponent.
 * Unlike C7, the world has a world context and initialized actors (the engine's CQTest spawner setup), so
 * destroying a station actor routes EndPlay and OnEndPlay as in a running game.
 */
struct FC8AFixture
{
	UWorld* World = nullptr;
	ACharacter* Character = nullptr;
	TStrongObjectPtr<UInventoryComponent> Inventory;
	TStrongObjectPtr<UCraftingComponent> Crafting;
	TStrongObjectPtr<UCraftingTimedTestListener> Listener;

	explicit FC8AFixture(const FC8ARecipeList& Recipes)
		: Listener(NewObject<UCraftingTimedTestListener>(GetTransientPackage()))
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!World)
		{
			return;
		}
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Character = World->SpawnActor<ACharacter>(ACharacter::StaticClass(), FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
		if (!Character)
		{
			return;
		}
		Inventory.Reset(NewObject<UInventoryComponent>(Character));
		Character->AddInstanceComponent(Inventory.Get());
		Inventory->RegisterComponent();
		Crafting.Reset(NewObject<UCraftingComponent>(Character));
		Character->AddInstanceComponent(Crafting.Get());
		Crafting->RegisterComponent();
		C8ASetRecipes(Crafting.Get(), Recipes);
		Listener->Crafting = Crafting.Get();
	}

	~FC8AFixture()
	{
		Crafting.Reset();
		Inventory.Reset();
		if (World)
		{
			// Same teardown as the engine's CQTest actor spawner.
			for (AActor* Actor : FActorRange(World))
			{
				if (Actor)
				{
					Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
				}
			}
			GEngine->ShutdownWorldNetDriver(World);
			World->DestroyWorld(true);
			GEngine->DestroyWorldContext(World);
		}
	}

	bool IsValid(FAutomationTestBase& Test) const
	{
		return Test.TestTrue(TEXT("Station crafting fixture is ready"), World && Character && Inventory && Crafting);
	}

	void StartObserving()
	{
		Crafting->OnCraftStarted.AddDynamic(Listener.Get(), &UCraftingTimedTestListener::HandleStarted);
		Crafting->OnCraftCancelled.AddDynamic(Listener.Get(), &UCraftingTimedTestListener::HandleCancelled);
		Crafting->OnCraftingResult.AddDynamic(Listener.Get(), &UCraftingTimedTestListener::HandleResult);
		Inventory->OnInventoryChanged.AddDynamic(Listener.Get(), &UCraftingTimedTestListener::HandleInventoryChanged);
	}

	/**
	 * Spawns a station actor. With bBeginPlay the actor has begun play, so destroying it routes EndPlay and
	 * OnEndPlay exactly as in a running game; without it, destruction only invalidates the station.
	 */
	UCraftingStationComponent* SpawnStation(TSubclassOf<UCraftingStationDefinition> Definition, const FVector& Location,
		bool bBeginPlay = true) const
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		if (!Actor)
		{
			return nullptr;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("StationRoot"));
		Actor->SetRootComponent(Root);
		Actor->AddInstanceComponent(Root);
		Root->RegisterComponent();
		Actor->SetActorLocation(Location);
		UCraftingStationComponent* Station = NewObject<UCraftingStationComponent>(Actor);
		Actor->AddInstanceComponent(Station);
		Station->RegisterComponent();
		C8ASetStationDefinition(Station, Definition);
		if (bBeginPlay)
		{
			Actor->DispatchBeginPlay();
		}
		return Station;
	}

	/** Same frame-accurate timer advance as C7. */
	void AdvanceTime(float Seconds) const
	{
		FTimerManager& Timers = World->GetTimerManager();
		++GFrameCounter;
		Timers.Tick(0.f);
		++GFrameCounter;
		Timers.Tick(Seconds);
	}

	void MoveCharacterTo(const FVector& Location) const
	{
		const FVector Old = Character->GetActorLocation();
		Character->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		Character->OnCharacterMovementUpdated.Broadcast(0.016f, Old, FVector::ZeroVector);
	}

	bool IsStationEndPlayBound(const AActor* StationActor) const
	{
		return StationActor && StationActor->OnEndPlay.Contains(Crafting.Get(), C8AStationEndPlayHandler);
	}
};

bool C8AAdd(FAutomationTestBase& Test, UInventoryComponent* Inventory, TSubclassOf<UItemDefinition> Definition, int32 Quantity)
{
	return Test.TestTrue(FString::Printf(TEXT("Setup adds %d of %s"), Quantity, *GetNameSafe(Definition.Get())),
		Inventory->TryAddDefinitionQuantity(Definition, Quantity).IsSuccess());
}

/** Snapshot of everything a refused request must leave untouched. */
struct FC8AUnchanged
{
	FC8AStacks Stacks;
	int32 Revision = 0;
	int32 InventoryEvents = 0;

	explicit FC8AUnchanged(const FC8AFixture& Fixture)
		: Stacks(C8ACapture(Fixture.Inventory.Get()))
		, Revision(Fixture.Inventory->GetRevision())
		, InventoryEvents(Fixture.Listener->InventoryEvents)
	{
	}

	void Check(FAutomationTestBase& Test, const FC8AFixture& Fixture, const FString& What) const
	{
		Test.TestTrue(What + TEXT(": ItemIds and quantities unchanged"), C8ACapture(Fixture.Inventory.Get()) == Stacks);
		Test.TestEqual(What + TEXT(": no revision"), Fixture.Inventory->GetRevision(), Revision);
		Test.TestEqual(What + TEXT(": no inventory event"), Fixture.Listener->InventoryEvents, InventoryEvents);
	}
};

const FVector C8AInRange(200.f, 0.f, 0.f);
const FVector C8AOutOfRange(400.f, 0.f, 0.f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC8AStationDefinitionContractTest,
	"NullTide.Crafting.C8A.StationDefinitionContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC8AStationDefinitionContractTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Native station root is abstract"), UCraftingStationDefinition::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	TestTrue(TEXT("Native station root is const static data"), UCraftingStationDefinition::StaticClass()->HasAnyClassFlags(CLASS_Const));
	TestEqual(TEXT("Default UseRange is 300 cm"), GetDefault<UCraftingStationDefinition>()->UseRange, 300.f);

	const UCraftingStationDefinition* Bench = GetDefault<UCraftingTestStationDefinition>();
	FString Diagnostic;
	TestTrue(TEXT("Valid station passes validation"), Bench->ValidateStation(Diagnostic) && Diagnostic.IsEmpty());
	TestEqual(TEXT("Station name"), Bench->StationName.ToString(), FString(TEXT("Test Bench")));
	TestTrue(TEXT("ResolveValidStation returns the default object"),
		UCraftingStationDefinition::ResolveValidStation(C8ABench(), Diagnostic) == Bench);

	TestFalse(TEXT("UseRange 0 fails validation"), GetDefault<UCraftingInvalidTestStationDefinition>()->ValidateStation(Diagnostic));
	TestTrue(TEXT("UseRange diagnostic"), Diagnostic.Contains(TEXT("UseRange")));
	TestNull(TEXT("Null station class resolves to null"), UCraftingStationDefinition::ResolveValidStation(nullptr, Diagnostic));
	TestNull(TEXT("Abstract station class resolves to null"),
		UCraftingStationDefinition::ResolveValidStation(UCraftingStationDefinition::StaticClass(), Diagnostic));
	TestNull(TEXT("Invalid station class resolves to null"), UCraftingStationDefinition::ResolveValidStation(C8ABrokenStation(), Diagnostic));

	// Component: context only, usable only with a valid definition on a live owner; range is horizontal.
	FC8AFixture Fixture({});
	if (!Fixture.IsValid(*this)) { return false; }
	UCraftingStationComponent* Unset = Fixture.SpawnStation(nullptr, C8AInRange);
	if (!TestNotNull(TEXT("Station spawns"), Unset)) { return false; }
	TestFalse(TEXT("No definition: not usable"), Unset->IsStationUsable());
	TestFalse(TEXT("No definition: nobody is in range"), Unset->IsActorInUseRange(Fixture.Character));

	UCraftingStationComponent* Station = Fixture.SpawnStation(C8ABench(), FVector(300.f, 0.f, 0.f));
	TestTrue(TEXT("Valid definition: usable"), Station->IsStationUsable());
	TestTrue(TEXT("Definition getter"), Station->GetStationDefinition() == C8ABench());
	TestTrue(TEXT("Exactly 300 cm is in range"), Station->IsActorInUseRange(Fixture.Character));
	Station->GetOwner()->SetActorLocation(FVector(300.5f, 0.f, 0.f));
	TestFalse(TEXT("300.5 cm is out of range"), Station->IsActorInUseRange(Fixture.Character));
	Station->GetOwner()->SetActorLocation(FVector(100.f, 0.f, 900.f));
	TestTrue(TEXT("Height difference is ignored"), Station->IsActorInUseRange(Fixture.Character));
	TestFalse(TEXT("Null actor is never in range"), Station->IsActorInUseRange(nullptr));

	UCraftingStationComponent* Broken = Fixture.SpawnStation(C8ABrokenStation(), C8AInRange);
	TestFalse(TEXT("Invalid definition: not usable"), Broken->IsStationUsable());
	TestFalse(TEXT("Invalid definition: nobody is in range"), Broken->IsActorInUseRange(Fixture.Character));

	Station->GetOwner()->Destroy();
	TestFalse(TEXT("Destroyed station is not usable"), ::IsValid(Station) && Station->IsStationUsable());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC8ARecipeStationValidationTest,
	"NullTide.Crafting.C8A.RecipeStationValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC8ARecipeStationValidationTest::RunTest(const FString& Parameters)
{
	FString Diagnostic;
	const UCraftingRecipeDefinition* Hand = GetDefault<UCraftingTestRecipeDefinition>();
	TestNull(TEXT("Hand recipe has no RequiredStation"), Hand->RequiredStation.Get());
	TestTrue(TEXT("Hand recipe is valid"), Hand->ValidateRecipe(Diagnostic));

	const UCraftingRecipeDefinition* Bench = GetDefault<UCraftingStationTimedTestRecipe>();
	TestTrue(TEXT("Station recipe names its station"), Bench->RequiredStation == C8ABench());
	TestTrue(TEXT("Station recipe is valid"), Bench->ValidateRecipe(Diagnostic) && Diagnostic.IsEmpty());

	TestFalse(TEXT("Abstract RequiredStation is invalid"), GetDefault<UCraftingAbstractStationTestRecipe>()->ValidateRecipe(Diagnostic));
	TestTrue(TEXT("Abstract station diagnostic names RequiredStation"), Diagnostic.Contains(TEXT("RequiredStation")));
	TestFalse(TEXT("Invalid RequiredStation is invalid"), GetDefault<UCraftingInvalidStationTestRecipe>()->ValidateRecipe(Diagnostic));
	TestTrue(TEXT("Invalid station diagnostic explains UseRange"),
		Diagnostic.Contains(TEXT("RequiredStation")) && Diagnostic.Contains(TEXT("UseRange")));

	// Registration filters recipes with invalid station data like any other invalid recipe.
	FC8AFixture Fixture({ C8AHandInstant(), UCraftingAbstractStationTestRecipe::StaticClass(),
		UCraftingInvalidStationTestRecipe::StaticClass(), C8ABenchTimed() });
	if (!Fixture.IsValid(*this)) { return false; }
	TestTrue(TEXT("Invalid station recipes are not available"),
		Fixture.Crafting->GetAvailableRecipes() == FC8ARecipeList({ C8AHandInstant(), C8ABenchTimed() }));
	TestTrue(TEXT("Requesting one is InvalidRecipe"),
		C8AIs(Fixture.Crafting->CanCraftRecipe(UCraftingAbstractStationTestRecipe::StaticClass()), ECraftingTransactionResult::InvalidRecipe));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC8ARecipeFilteringTest,
	"NullTide.Crafting.C8A.RecipeFiltering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC8ARecipeFilteringTest::RunTest(const FString& Parameters)
{
	FC8AFixture Fixture({ C8ABenchTimed(), C8AHandInstant(), C8AForgeRecipe(), C8AHandTimed(), C8ABenchInstant() });
	if (!Fixture.IsValid(*this)) { return false; }
	UCraftingStationComponent* Bench = Fixture.SpawnStation(C8ABench(), C8AInRange);
	UCraftingStationComponent* Forge = Fixture.SpawnStation(C8AForge(), C8AInRange);
	UCraftingStationComponent* FarBench = Fixture.SpawnStation(C8ABench(), C8AOutOfRange);
	UCraftingStationComponent* Unset = Fixture.SpawnStation(nullptr, C8AInRange);
	UCraftingStationComponent* Broken = Fixture.SpawnStation(C8ABrokenStation(), C8AInRange);
	if (!TestTrue(TEXT("Stations spawn"), Bench && Forge && FarBench && Unset && Broken)) { return false; }

	const FC8ARecipeList HandOnly({ C8AHandInstant(), C8AHandTimed() });
	TestTrue(TEXT("GetAvailableRecipes still lists every registered recipe in authored order"),
		Fixture.Crafting->GetAvailableRecipes()
			== FC8ARecipeList({ C8ABenchTimed(), C8AHandInstant(), C8AForgeRecipe(), C8AHandTimed(), C8ABenchInstant() }));
	TestTrue(TEXT("No station: hand recipes only"), Fixture.Crafting->GetRecipesForStation(nullptr) == HandOnly);
	TestTrue(TEXT("Bench: bench recipes first (authored order), then hand recipes"),
		Fixture.Crafting->GetRecipesForStation(Bench)
			== FC8ARecipeList({ C8ABenchTimed(), C8ABenchInstant(), C8AHandInstant(), C8AHandTimed() }));
	TestTrue(TEXT("Forge: exact kind only, bench recipes excluded"),
		Fixture.Crafting->GetRecipesForStation(Forge) == FC8ARecipeList({ C8AForgeRecipe(), C8AHandInstant(), C8AHandTimed() }));
	TestTrue(TEXT("Listing ignores range"),
		Fixture.Crafting->GetRecipesForStation(FarBench) == Fixture.Crafting->GetRecipesForStation(Bench));
	TestTrue(TEXT("Station without a definition: hand recipes only"), Fixture.Crafting->GetRecipesForStation(Unset) == HandOnly);
	TestTrue(TEXT("Station with an invalid definition: hand recipes only"), Fixture.Crafting->GetRecipesForStation(Broken) == HandOnly);

	Bench->GetOwner()->Destroy();
	TestTrue(TEXT("Destroyed station: hand recipes only"), Fixture.Crafting->GetRecipesForStation(Bench) == HandOnly);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC8AHandContextTest,
	"NullTide.Crafting.C8A.HandContextRejectsStationRecipe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC8AHandContextTest::RunTest(const FString& Parameters)
{
	FC8AFixture Fixture({ C8ABenchTimed(), C8ABenchInstant(), C8AHandInstant() });
	if (!Fixture.IsValid(*this) || !C8AAdd(*this, Fixture.Inventory.Get(), C8ABig(), 20)
		|| !C8AAdd(*this, Fixture.Inventory.Get(), C8ASingle(), 4)) { return false; }
	Fixture.StartObserving();
	const FC8AUnchanged Before(Fixture);

	TestTrue(TEXT("CanCraftRecipe: StationRequired"),
		C8AIs(Fixture.Crafting->CanCraftRecipe(C8ABenchTimed()), ECraftingTransactionResult::StationRequired));
	TestTrue(TEXT("CanCraftRecipeAtStation(null): StationRequired"),
		C8AIs(Fixture.Crafting->CanCraftRecipeAtStation(C8ABenchInstant(), nullptr), ECraftingTransactionResult::StationRequired));
	TestEqual(TEXT("Queries notify nothing"), Fixture.Listener->ResultEvents, 0);

	const FCraftingTransactionResult Start = Fixture.Crafting->TryStartCraft(C8ABenchTimed());
	TestTrue(TEXT("TryStartCraft: StationRequired"), C8AIs(Start, ECraftingTransactionResult::StationRequired));
	TestTrue(TEXT("Diagnostic names the station"), Start.Diagnostic.Contains(TEXT("CraftingTestStationDefinition")));
	TestFalse(TEXT("Nothing started"), Fixture.Crafting->IsCrafting());
	TestTrue(TEXT("TryCraftRecipe: StationRequired"),
		C8AIs(Fixture.Crafting->TryCraftRecipe(C8ABenchInstant()), ECraftingTransactionResult::StationRequired));
	TestTrue(TEXT("TryStartCraftAtStation(null): StationRequired"),
		C8AIs(Fixture.Crafting->TryStartCraftAtStation(C8ABenchInstant(), nullptr), ECraftingTransactionResult::StationRequired));
	TestEqual(TEXT("Each refused request notified once"), Fixture.Listener->ResultEvents, 3);
	TestEqual(TEXT("No started event"), Fixture.Listener->StartedEvents, 0);
	Before.Check(*this, Fixture, TEXT("Hand-context refusals"));

	TestTrue(TEXT("Hand recipes still craft in the hand context"), Fixture.Crafting->TryStartCraft(C8AHandInstant()).IsSuccess());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC8AMismatchRangeTest,
	"NullTide.Crafting.C8A.StationMismatchAndRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC8AMismatchRangeTest::RunTest(const FString& Parameters)
{
	FC8AFixture Fixture({ C8ABenchTimed(), C8ABenchInstant(), C8AHandInstant() });
	if (!Fixture.IsValid(*this) || !C8AAdd(*this, Fixture.Inventory.Get(), C8ABig(), 20)
		|| !C8AAdd(*this, Fixture.Inventory.Get(), C8ASingle(), 4)) { return false; }
	UCraftingStationComponent* Bench = Fixture.SpawnStation(C8ABench(), C8AInRange);
	UCraftingStationComponent* Forge = Fixture.SpawnStation(C8AForge(), C8AInRange);
	UCraftingStationComponent* FarBench = Fixture.SpawnStation(C8ABench(), C8AOutOfRange);
	UCraftingStationComponent* Unset = Fixture.SpawnStation(nullptr, C8AInRange);
	UCraftingStationComponent* Broken = Fixture.SpawnStation(C8ABrokenStation(), C8AInRange);
	UCraftingStationComponent* Doomed = Fixture.SpawnStation(C8ABench(), C8AInRange);
	if (!TestTrue(TEXT("Stations spawn"), Bench && Forge && FarBench && Unset && Broken && Doomed)) { return false; }
	Fixture.StartObserving();
	const FC8AUnchanged Before(Fixture);

	struct FCase { const TCHAR* Name; UCraftingStationComponent* Station; ECraftingTransactionResult Expected; };
	Doomed->GetOwner()->Destroy();
	const FCase Cases[] = {
		{ TEXT("Wrong station kind"), Forge, ECraftingTransactionResult::StationRequired },
		{ TEXT("Station without a definition"), Unset, ECraftingTransactionResult::StationRequired },
		{ TEXT("Station with an invalid definition"), Broken, ECraftingTransactionResult::StationRequired },
		{ TEXT("Destroyed station"), Doomed, ECraftingTransactionResult::StationRequired },
		{ TEXT("Right kind, out of range"), FarBench, ECraftingTransactionResult::StationOutOfRange },
	};
	int32 Notifications = 0;
	for (const FCase& Case : Cases)
	{
		TestTrue(FString(Case.Name) + TEXT(": CanCraftRecipeAtStation"),
			C8AIs(Fixture.Crafting->CanCraftRecipeAtStation(C8ABenchTimed(), Case.Station), Case.Expected));
		TestTrue(FString(Case.Name) + TEXT(": TryStartCraftAtStation (timed)"),
			C8AIs(Fixture.Crafting->TryStartCraftAtStation(C8ABenchTimed(), Case.Station), Case.Expected));
		TestTrue(FString(Case.Name) + TEXT(": TryStartCraftAtStation (instant)"),
			C8AIs(Fixture.Crafting->TryStartCraftAtStation(C8ABenchInstant(), Case.Station), Case.Expected));
		Notifications += 2;
		TestFalse(FString(Case.Name) + TEXT(": nothing started"), Fixture.Crafting->IsCrafting());
	}
	TestEqual(TEXT("Each refused start notified once"), Fixture.Listener->ResultEvents, Notifications);
	Before.Check(*this, Fixture, TEXT("Station refusals"));

	// Hand recipes are craftable anywhere, whatever station context is passed.
	TestTrue(TEXT("Hand recipe with the wrong station"), Fixture.Crafting->CanCraftRecipeAtStation(C8AHandInstant(), Forge).IsSuccess());
	TestTrue(TEXT("Hand recipe with a far station"), Fixture.Crafting->CanCraftRecipeAtStation(C8AHandInstant(), FarBench).IsSuccess());
	TestTrue(TEXT("Hand recipe with a destroyed station"), Fixture.Crafting->CanCraftRecipeAtStation(C8AHandInstant(), Doomed).IsSuccess());

	// Right kind in range passes; moving the station out of range or the crafter away is re-evaluated each query.
	TestTrue(TEXT("Right kind in range"), Fixture.Crafting->CanCraftRecipeAtStation(C8ABenchTimed(), Bench).IsSuccess());
	Bench->GetOwner()->SetActorLocation(C8AOutOfRange);
	TestTrue(TEXT("Station moved away: StationOutOfRange"),
		C8AIs(Fixture.Crafting->CanCraftRecipeAtStation(C8ABenchTimed(), Bench), ECraftingTransactionResult::StationOutOfRange));
	Fixture.MoveCharacterTo(FVector(250.f, 0.f, 100.f));
	TestTrue(TEXT("Crafter walked to the station: in range again"), Fixture.Crafting->CanCraftRecipeAtStation(C8ABenchTimed(), Bench).IsSuccess());

	// Station checks run before inventory checks.
	FC8AFixture Empty({ C8ABenchTimed() });
	if (!Empty.IsValid(*this)) { return false; }
	UCraftingStationComponent* EmptyFar = Empty.SpawnStation(C8ABench(), C8AOutOfRange);
	TestTrue(TEXT("Out of range is reported before missing ingredients"),
		C8AIs(Empty.Crafting->CanCraftRecipeAtStation(C8ABenchTimed(), EmptyFar), ECraftingTransactionResult::StationOutOfRange));
	EmptyFar->GetOwner()->SetActorLocation(C8AInRange);
	TestTrue(TEXT("In range with no ingredients: InsufficientIngredients"),
		C8AIs(Empty.Crafting->CanCraftRecipeAtStation(C8ABenchTimed(), EmptyFar), ECraftingTransactionResult::InsufficientIngredients));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC8AInstantTimedTest,
	"NullTide.Crafting.C8A.StationInstantAndTimedCraft",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC8AInstantTimedTest::RunTest(const FString& Parameters)
{
	FC8AFixture Fixture({ C8ABenchTimed(), C8ABenchInstant() });
	if (!Fixture.IsValid(*this) || !C8AAdd(*this, Fixture.Inventory.Get(), C8ABig(), 10)
		|| !C8AAdd(*this, Fixture.Inventory.Get(), C8ASingle(), 2)) { return false; }
	UCraftingStationComponent* Bench = Fixture.SpawnStation(C8ABench(), C8AInRange);
	if (!TestNotNull(TEXT("Bench spawns"), Bench)) { return false; }
	Fixture.StartObserving();

	// Instant station recipe: crafts immediately, one revision and one event; no station dependency remains.
	const FGuid BigStack = C8ACapture(Fixture.Inventory.Get())[0].Key;
	int32 Revision = Fixture.Inventory->GetRevision();
	TestTrue(TEXT("Instant station craft succeeds"), Fixture.Crafting->TryStartCraftAtStation(C8ABenchInstant(), Bench).IsSuccess());
	TestEqual(TEXT("Instant: Big 10 - 2"), Fixture.Inventory->GetTotalQuantity(C8ABig()), 8);
	TestEqual(TEXT("Instant: Other created"), Fixture.Inventory->GetTotalQuantity(C8AOther()), 1);
	TestTrue(TEXT("Instant: Big consumed from the same stack"), C8ACapture(Fixture.Inventory.Get())[0].Key == BigStack);
	TestEqual(TEXT("Instant: one revision"), Fixture.Inventory->GetRevision(), Revision + 1);
	TestEqual(TEXT("Instant: one inventory event"), Fixture.Listener->InventoryEvents, 1);
	TestFalse(TEXT("Instant: idle"), Fixture.Crafting->IsCrafting());
	TestNull(TEXT("Instant: no active station"), Fixture.Crafting->GetActiveCraftStation());
	TestFalse(TEXT("Instant: station lifetime not bound"), Fixture.IsStationEndPlayBound(Bench->GetOwner()));

	// Timed station recipe: consume at completion.
	const FC8AUnchanged BeforeStart(Fixture);
	Revision = Fixture.Inventory->GetRevision();
	TestTrue(TEXT("Timed station start"), C8AIs(Fixture.Crafting->TryStartCraftAtStation(C8ABenchTimed(), Bench), ECraftingTransactionResult::Started));
	TestTrue(TEXT("Active recipe"), Fixture.Crafting->GetActiveCraftRecipe() == C8ABenchTimed());
	TestTrue(TEXT("Active station"), Fixture.Crafting->GetActiveCraftStation() == Bench);
	TestTrue(TEXT("Station lifetime bound while crafting"), Fixture.IsStationEndPlayBound(Bench->GetOwner()));
	TestEqual(TEXT("One started event"), Fixture.Listener->StartedEvents, 1);
	BeforeStart.Check(*this, Fixture, TEXT("Timed station start"));

	Fixture.AdvanceTime(1.f);
	TestEqual(TEXT("Halfway progress"), Fixture.Crafting->GetActiveCraftProgress(), 0.5f, 0.01f);
	BeforeStart.Check(*this, Fixture, TEXT("Timed station halfway"));

	Fixture.AdvanceTime(1.1f);
	TestFalse(TEXT("Idle after completion"), Fixture.Crafting->IsCrafting());
	TestTrue(TEXT("Completion succeeded"), Fixture.Listener->LastResult.IsSuccess());
	TestEqual(TEXT("Timed: Big 8 - 3"), Fixture.Inventory->GetTotalQuantity(C8ABig()), 5);
	TestEqual(TEXT("Timed: Single 2 - 1"), Fixture.Inventory->GetTotalQuantity(C8ASingle()), 1);
	TestEqual(TEXT("Timed: second Other"), Fixture.Inventory->GetTotalQuantity(C8AOther()), 2);
	TestEqual(TEXT("Timed completion: exactly one revision"), Fixture.Inventory->GetRevision(), Revision + 1);
	TestEqual(TEXT("Timed completion: exactly one inventory event"), Fixture.Listener->InventoryEvents, 2);
	TestEqual(TEXT("Two result notifications in total"), Fixture.Listener->ResultEvents, 2);
	TestNull(TEXT("Active station cleared"), Fixture.Crafting->GetActiveCraftStation());
	TestFalse(TEXT("Station lifetime unbound after completion"), Fixture.IsStationEndPlayBound(Bench->GetOwner()));

	// Destroying the station after completion notifies nothing.
	Bench->GetOwner()->Destroy();
	TestEqual(TEXT("No cancel after completion"), Fixture.Listener->CancelledEvents, 0);
	TestEqual(TEXT("No extra result after completion"), Fixture.Listener->ResultEvents, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC8AStationLostTest,
	"NullTide.Crafting.C8A.StationLostCancels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC8AStationLostTest::RunTest(const FString& Parameters)
{
	// A station recipe is cancelled the moment its station actor is destroyed.
	{
		FC8AFixture Fixture({ C8ABenchTimed() });
		if (!Fixture.IsValid(*this) || !C8AAdd(*this, Fixture.Inventory.Get(), C8ABig(), 5)
			|| !C8AAdd(*this, Fixture.Inventory.Get(), C8ASingle(), 2)) { return false; }
		UCraftingStationComponent* Bench = Fixture.SpawnStation(C8ABench(), C8AInRange);
		if (!TestNotNull(TEXT("Bench spawns"), Bench)) { return false; }
		Fixture.StartObserving();
		const FC8AUnchanged Before(Fixture);
		if (!TestTrue(TEXT("Start"), C8AIs(Fixture.Crafting->TryStartCraftAtStation(C8ABenchTimed(), Bench), ECraftingTransactionResult::Started))) { return false; }
		Fixture.AdvanceTime(1.f);

		Bench->GetOwner()->Destroy();
		TestFalse(TEXT("Destroying the station cancels immediately"), Fixture.Crafting->IsCrafting());
		TestEqual(TEXT("Exactly one cancel notification"), Fixture.Listener->CancelledEvents, 1);
		TestTrue(TEXT("Reason StationLost"), Fixture.Listener->LastCancelReason == ECraftCancelReason::StationLost);
		TestEqual(TEXT("No result notification"), Fixture.Listener->ResultEvents, 0);
		TestNull(TEXT("Active station cleared"), Fixture.Crafting->GetActiveCraftStation());
		Before.Check(*this, Fixture, TEXT("StationLost"));
		Fixture.AdvanceTime(5.f);
		TestEqual(TEXT("Cancelled timer never completes"), Fixture.Listener->ResultEvents, 0);
		TestEqual(TEXT("Still one cancel notification"), Fixture.Listener->CancelledEvents, 1);
		Before.Check(*this, Fixture, TEXT("After StationLost"));
		TestTrue(TEXT("A new hand-context request works afterwards"),
			C8AIs(Fixture.Crafting->TryStartCraft(C8ABenchTimed()), ECraftingTransactionResult::StationRequired));
	}

	// A hand recipe started from a station context does not depend on the station.
	{
		FC8AFixture Fixture({ C8AHandTimed() });
		if (!Fixture.IsValid(*this) || !C8AAdd(*this, Fixture.Inventory.Get(), C8ABig(), 5)
			|| !C8AAdd(*this, Fixture.Inventory.Get(), C8ASingle(), 2)) { return false; }
		UCraftingStationComponent* Bench = Fixture.SpawnStation(C8ABench(), C8AInRange);
		Fixture.StartObserving();
		TestTrue(TEXT("Hand recipe starts at the station"),
			C8AIs(Fixture.Crafting->TryStartCraftAtStation(C8AHandTimed(), Bench), ECraftingTransactionResult::Started));
		TestNull(TEXT("Hand recipe records no station"), Fixture.Crafting->GetActiveCraftStation());
		TestFalse(TEXT("Hand recipe does not bind the station"), Fixture.IsStationEndPlayBound(Bench->GetOwner()));
		Bench->GetOwner()->Destroy();
		TestTrue(TEXT("Station destroyed: hand recipe keeps crafting"), Fixture.Crafting->IsCrafting());
		TestEqual(TEXT("No cancel notification"), Fixture.Listener->CancelledEvents, 0);
		Fixture.AdvanceTime(2.1f);
		TestTrue(TEXT("Hand recipe completes"), Fixture.Listener->LastResult.IsSuccess());
		TestEqual(TEXT("Hand recipe consumed once"), Fixture.Inventory->GetTotalQuantity(C8ABig()), 2);
	}

	// The whole world ending (not a destroyed station) clears silently, like the crafter's own EndPlay.
	{
		FC8AFixture Fixture({ C8ABenchTimed() });
		if (!Fixture.IsValid(*this) || !C8AAdd(*this, Fixture.Inventory.Get(), C8ABig(), 5)
			|| !C8AAdd(*this, Fixture.Inventory.Get(), C8ASingle(), 2)) { return false; }
		UCraftingStationComponent* Bench = Fixture.SpawnStation(C8ABench(), C8AInRange);
		Fixture.StartObserving();
		Fixture.Crafting->TryStartCraftAtStation(C8ABenchTimed(), Bench);
		Bench->GetOwner()->RouteEndPlay(EEndPlayReason::Quit);
		TestFalse(TEXT("World teardown clears the craft"), Fixture.Crafting->IsCrafting());
		TestEqual(TEXT("World teardown sends no cancel"), Fixture.Listener->CancelledEvents, 0);
		TestEqual(TEXT("World teardown sends no result"), Fixture.Listener->ResultEvents, 0);
		TestEqual(TEXT("Nothing consumed"), Fixture.Inventory->GetTotalQuantity(C8ABig()), 5);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC8ACompletionRevalidationTest,
	"NullTide.Crafting.C8A.CompletionRevalidatesStation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC8ACompletionRevalidationTest::RunTest(const FString& Parameters)
{
	auto RunFailingCompletion = [this](const TCHAR* Name, TFunctionRef<void(FC8AFixture&, UCraftingStationComponent*)> Change,
		ECraftingTransactionResult Expected, bool bBeginPlay)
	{
		FC8AFixture Fixture({ C8ABenchTimed() });
		if (!Fixture.IsValid(*this) || !C8AAdd(*this, Fixture.Inventory.Get(), C8ABig(), 5)
			|| !C8AAdd(*this, Fixture.Inventory.Get(), C8ASingle(), 2)) { return; }
		UCraftingStationComponent* Bench = Fixture.SpawnStation(C8ABench(), C8AInRange, bBeginPlay);
		if (!TestNotNull(TEXT("Bench spawns"), Bench)) { return; }
		Fixture.StartObserving();
		if (!TestTrue(FString(Name) + TEXT(": start"),
			C8AIs(Fixture.Crafting->TryStartCraftAtStation(C8ABenchTimed(), Bench), ECraftingTransactionResult::Started))) { return; }
		Change(Fixture, Bench);
		TestTrue(FString(Name) + TEXT(": no polling, still crafting until completion"), Fixture.Crafting->IsCrafting());
		const FC8AUnchanged Before(Fixture);
		Fixture.AdvanceTime(2.1f);
		TestFalse(FString(Name) + TEXT(": idle after completion"), Fixture.Crafting->IsCrafting());
		TestEqual(FString(Name) + TEXT(": one result notification"), Fixture.Listener->ResultEvents, 1);
		TestTrue(FString(Name) + TEXT(": completion result"), C8AIs(Fixture.Listener->LastResult, Expected));
		TestEqual(FString(Name) + TEXT(": no cancel notification"), Fixture.Listener->CancelledEvents, 0);
		Before.Check(*this, Fixture, FString(Name) + TEXT(" completion"));
		TestEqual(FString(Name) + TEXT(": nothing consumed"), Fixture.Inventory->GetTotalQuantity(C8ABig()), 5);
	};

	RunFailingCompletion(TEXT("Station moved out of range"),
		[](FC8AFixture&, UCraftingStationComponent* Bench) { Bench->GetOwner()->SetActorLocation(C8AOutOfRange); },
		ECraftingTransactionResult::StationOutOfRange, true);

	RunFailingCompletion(TEXT("Station definition cleared"),
		[](FC8AFixture&, UCraftingStationComponent* Bench) { C8ASetStationDefinition(Bench, nullptr); },
		ECraftingTransactionResult::StationRequired, true);

	RunFailingCompletion(TEXT("Station changed kind"),
		[](FC8AFixture&, UCraftingStationComponent* Bench) { C8ASetStationDefinition(Bench, C8AForge()); },
		ECraftingTransactionResult::StationRequired, true);

	// The station component alone is destroyed: the actor never ends play, so only completion notices.
	RunFailingCompletion(TEXT("Station component destroyed"),
		[](FC8AFixture&, UCraftingStationComponent* Bench) { Bench->DestroyComponent(); },
		ECraftingTransactionResult::StationRequired, true);

	// A station actor that never began play sends no OnEndPlay; the stale weak pointer is rejected at completion.
	RunFailingCompletion(TEXT("Stale station reference"),
		[this](FC8AFixture& Fixture, UCraftingStationComponent* Bench)
		{
			Bench->GetOwner()->Destroy();
			TestNull(TEXT("Stale reference: GetActiveCraftStation is null"), Fixture.Crafting->GetActiveCraftStation());
		},
		ECraftingTransactionResult::StationRequired, false);

	// Control: the station back in range before completion succeeds.
	{
		FC8AFixture Fixture({ C8ABenchTimed() });
		if (!Fixture.IsValid(*this) || !C8AAdd(*this, Fixture.Inventory.Get(), C8ABig(), 5)
			|| !C8AAdd(*this, Fixture.Inventory.Get(), C8ASingle(), 2)) { return false; }
		UCraftingStationComponent* Bench = Fixture.SpawnStation(C8ABench(), C8AInRange);
		Fixture.StartObserving();
		Fixture.Crafting->TryStartCraftAtStation(C8ABenchTimed(), Bench);
		Bench->GetOwner()->SetActorLocation(C8AOutOfRange);
		Bench->GetOwner()->SetActorLocation(FVector(0.f, 250.f, 0.f));
		const int32 Revision = Fixture.Inventory->GetRevision();
		Fixture.AdvanceTime(2.1f);
		TestTrue(TEXT("Back in range: completion succeeds"), Fixture.Listener->LastResult.IsSuccess());
		TestEqual(TEXT("Back in range: one revision"), Fixture.Inventory->GetRevision(), Revision + 1);
		TestEqual(TEXT("Back in range: one inventory event"), Fixture.Listener->InventoryEvents, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC8AOneActiveCraftTest,
	"NullTide.Crafting.C8A.OneActiveCraftAcrossContexts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC8AOneActiveCraftTest::RunTest(const FString& Parameters)
{
	FC8AFixture Fixture({ C8AHandTimed(), C8AHandInstant(), C8ABenchTimed(), C8ABenchInstant() });
	if (!Fixture.IsValid(*this) || !C8AAdd(*this, Fixture.Inventory.Get(), C8ABig(), 30)
		|| !C8AAdd(*this, Fixture.Inventory.Get(), C8ASingle(), 6)) { return false; }
	UCraftingStationComponent* Bench = Fixture.SpawnStation(C8ABench(), C8AInRange);
	UCraftingStationComponent* SecondBench = Fixture.SpawnStation(C8ABench(), FVector(-200.f, 0.f, 0.f));
	if (!TestTrue(TEXT("Benches spawn"), Bench && SecondBench)) { return false; }
	Fixture.StartObserving();

	// A hand craft blocks station crafts.
	TestTrue(TEXT("Hand timed start"), C8AIs(Fixture.Crafting->TryStartCraft(C8AHandTimed()), ECraftingTransactionResult::Started));
	const FC8AUnchanged DuringHand(Fixture);
	TestTrue(TEXT("Station query while hand crafting"),
		C8AIs(Fixture.Crafting->CanCraftRecipeAtStation(C8ABenchTimed(), Bench), ECraftingTransactionResult::CraftInProgress));
	TestTrue(TEXT("Station timed start while hand crafting"),
		C8AIs(Fixture.Crafting->TryStartCraftAtStation(C8ABenchTimed(), Bench), ECraftingTransactionResult::CraftInProgress));
	TestTrue(TEXT("Station instant start while hand crafting"),
		C8AIs(Fixture.Crafting->TryStartCraftAtStation(C8ABenchInstant(), Bench), ECraftingTransactionResult::CraftInProgress));
	TestTrue(TEXT("The hand craft is still active"), Fixture.Crafting->GetActiveCraftRecipe() == C8AHandTimed());
	TestNull(TEXT("No station recorded"), Fixture.Crafting->GetActiveCraftStation());
	DuringHand.Check(*this, Fixture, TEXT("Refused while hand crafting"));
	Fixture.Crafting->CancelCraft();

	// A station craft blocks hand crafts and crafts at any other station.
	TestTrue(TEXT("Station timed start"), C8AIs(Fixture.Crafting->TryStartCraftAtStation(C8ABenchTimed(), Bench), ECraftingTransactionResult::Started));
	const FC8AUnchanged DuringStation(Fixture);
	TestTrue(TEXT("Hand start while station crafting"),
		C8AIs(Fixture.Crafting->TryStartCraft(C8AHandTimed()), ECraftingTransactionResult::CraftInProgress));
	TestTrue(TEXT("Hand instant while station crafting"),
		C8AIs(Fixture.Crafting->TryCraftRecipe(C8AHandInstant()), ECraftingTransactionResult::CraftInProgress));
	TestTrue(TEXT("Second bench while station crafting"),
		C8AIs(Fixture.Crafting->TryStartCraftAtStation(C8ABenchTimed(), SecondBench), ECraftingTransactionResult::CraftInProgress));
	TestTrue(TEXT("Hand query while station crafting"),
		C8AIs(Fixture.Crafting->CanCraftRecipe(C8AHandInstant()), ECraftingTransactionResult::CraftInProgress));
	TestTrue(TEXT("The first station stays the active one"), Fixture.Crafting->GetActiveCraftStation() == Bench);
	TestFalse(TEXT("The second bench is not bound"), Fixture.IsStationEndPlayBound(SecondBench->GetOwner()));
	TestEqual(TEXT("Two started events in total"), Fixture.Listener->StartedEvents, 2);
	DuringStation.Check(*this, Fixture, TEXT("Refused while station crafting"));

	// Re-entrancy: a start from inside the completion notification is refused without recursion.
	Fixture.Listener->NestedRecipe = C8AHandTimed();
	Fixture.Listener->bStartFromResult = true;
	const int32 Revision = Fixture.Inventory->GetRevision();
	Fixture.AdvanceTime(2.1f);
	TestTrue(TEXT("Station completion succeeded"), Fixture.Listener->LastResult.IsSuccess());
	TestTrue(TEXT("Nested start from OnCraftingResult is CraftInProgress"),
		Fixture.Listener->NestedResults.Num() == 1 && C8AIs(Fixture.Listener->NestedResults[0], ECraftingTransactionResult::CraftInProgress));
	TestFalse(TEXT("No recursive craft started"), Fixture.Crafting->IsCrafting());
	TestEqual(TEXT("One revision for the completion"), Fixture.Inventory->GetRevision(), Revision + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC8AMovementCancelTest,
	"NullTide.Crafting.C8A.MovementCancelAtStation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC8AMovementCancelTest::RunTest(const FString& Parameters)
{
	FC8AFixture Fixture({ C8ABenchTimed() });
	if (!Fixture.IsValid(*this) || !C8AAdd(*this, Fixture.Inventory.Get(), C8ABig(), 5)
		|| !C8AAdd(*this, Fixture.Inventory.Get(), C8ASingle(), 2)) { return false; }
	UCraftingStationComponent* Bench = Fixture.SpawnStation(C8ABench(), C8AInRange);
	if (!TestNotNull(TEXT("Bench spawns"), Bench)) { return false; }
	Fixture.StartObserving();
	const FC8AUnchanged Before(Fixture);
	const FVector Start = Fixture.Character->GetActorLocation();

	// Walking away cancels through the C7 movement rule long before the 300 cm range matters.
	Fixture.Crafting->TryStartCraftAtStation(C8ABenchTimed(), Bench);
	Fixture.MoveCharacterTo(Start + FVector(15.f, 0.f, 0.f));
	TestFalse(TEXT("15 cm cancels a station craft"), Fixture.Crafting->IsCrafting());
	TestTrue(TEXT("Reason Movement"), Fixture.Listener->CancelledEvents == 1 && Fixture.Listener->LastCancelReason == ECraftCancelReason::Movement);
	TestFalse(TEXT("Station lifetime unbound after the cancel"), Fixture.IsStationEndPlayBound(Bench->GetOwner()));
	TestNull(TEXT("Active station cleared"), Fixture.Crafting->GetActiveCraftStation());
	Before.Check(*this, Fixture, TEXT("Movement cancel"));

	// Jump also cancels.
	Fixture.MoveCharacterTo(Start);
	Fixture.Crafting->TryStartCraftAtStation(C8ABenchTimed(), Bench);
	UCharacterMovementComponent* Movement = Fixture.Character->GetCharacterMovement();
	if (!TestNotNull(TEXT("Character movement"), Movement)) { return false; }
	Movement->SetMovementMode(MOVE_Falling);
	TestTrue(TEXT("Falling cancels a station craft"),
		!Fixture.Crafting->IsCrafting() && Fixture.Listener->CancelledEvents == 2 && Fixture.Listener->LastCancelReason == ECraftCancelReason::Jump);
	TestFalse(TEXT("Station lifetime unbound after the jump cancel"), Fixture.IsStationEndPlayBound(Bench->GetOwner()));

	// Explicit cancel also unbinds; destroying the station afterwards adds no notification.
	Movement->SetMovementMode(MOVE_Walking);
	Fixture.Crafting->TryStartCraftAtStation(C8ABenchTimed(), Bench);
	TestTrue(TEXT("Explicit cancel"), Fixture.Crafting->CancelCraft());
	TestFalse(TEXT("Station lifetime unbound after the explicit cancel"), Fixture.IsStationEndPlayBound(Bench->GetOwner()));
	Bench->GetOwner()->Destroy();
	TestEqual(TEXT("No StationLost after an earlier cancel"), Fixture.Listener->CancelledEvents, 3);
	TestTrue(TEXT("Last reason is still Explicit"), Fixture.Listener->LastCancelReason == ECraftCancelReason::Explicit);
	Fixture.AdvanceTime(5.f);
	TestEqual(TEXT("No cancelled craft ever completes"), Fixture.Listener->ResultEvents, 0);
	Before.Check(*this, Fixture, TEXT("All cancels"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC8AProductionAssetsTest,
	"NullTide.Crafting.C8A.ProductionStationAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC8AProductionAssetsTest::RunTest(const FString& Parameters)
{
	UClass* Workbench = LoadClass<UCraftingStationDefinition>(nullptr,
		TEXT("/Game/LevelPrototyping/CraftingSystem/Stations/Station_Workbench.Station_Workbench_C"));
	UClass* RecipeShortSword = LoadClass<UCraftingRecipeDefinition>(nullptr,
		TEXT("/Game/LevelPrototyping/CraftingSystem/Recipes/Recipe_ShortSword.Recipe_ShortSword_C"));
	UClass* Wood = LoadClass<UItemDefinition>(nullptr, TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Wood.Item_Wood_C"));
	UClass* Scrap = LoadClass<UItemDefinition>(nullptr, TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Scrap.Item_Scrap_C"));
	UClass* ShortSword = LoadClass<UItemDefinition>(nullptr, TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_ShortSword.Item_ShortSword_C"));
	if (!TestTrue(TEXT("Production assets load"), Workbench && RecipeShortSword && Wood && Scrap && ShortSword)) { return false; }

	const UCraftingStationDefinition* Station = Workbench->GetDefaultObject<UCraftingStationDefinition>();
	FString Diagnostic;
	TestTrue(TEXT("Station_Workbench is valid"), Station->ValidateStation(Diagnostic));
	TestEqual(TEXT("Station name"), Station->StationName.ToString(), FString(TEXT("Workbench")));
	TestEqual(TEXT("Station UseRange"), Station->UseRange, 300.f);

	const UCraftingRecipeDefinition* Recipe = RecipeShortSword->GetDefaultObject<UCraftingRecipeDefinition>();
	TestTrue(TEXT("Recipe_ShortSword is valid"), Recipe->ValidateRecipe(Diagnostic));
	TestTrue(TEXT("Recipe_ShortSword requires the Workbench"), Recipe->RequiredStation.Get() == Workbench);
	TestEqual(TEXT("Recipe_ShortSword takes 4 s"), Recipe->CraftDuration, 4.f);
	TestTrue(TEXT("Ingredients Wood x2, Scrap x4"), Recipe->Ingredients.Num() == 2
		&& Recipe->Ingredients[0].ItemDefinition.Get() == Wood && Recipe->Ingredients[0].Quantity == 2
		&& Recipe->Ingredients[1].ItemDefinition.Get() == Scrap && Recipe->Ingredients[1].Quantity == 4);
	TestTrue(TEXT("Output ShortSword x1"), Recipe->Outputs.Num() == 1
		&& Recipe->Outputs[0].ItemDefinition.Get() == ShortSword && Recipe->Outputs[0].Quantity == 1);
	TestEqual(TEXT("Recipe name"), Recipe->RecipeName.ToString(), FString(TEXT("Short Sword")));

	// Not registered on the player yet (C8C registers it).
	UBlueprint* PlayerBlueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/TopDown/Blueprints/BP_TopDownCharacter.BP_TopDownCharacter"));
	if (!TestNotNull(TEXT("Production player Blueprint loads"), PlayerBlueprint)) { return false; }
	bool bRegistered = false;
	if (const USimpleConstructionScript* ConstructionScript = PlayerBlueprint->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : ConstructionScript->GetAllNodes())
		{
			if (const UCraftingComponent* Template = Node ? Cast<UCraftingComponent>(Node->ComponentTemplate) : nullptr)
			{
				bRegistered |= Template->IsRecipeAvailable(RecipeShortSword);
			}
		}
	}
	TestFalse(TEXT("Recipe_ShortSword is not registered on BP_TopDownCharacter in C8A"), bRegistered);

	// End to end with the production data: Workbench in range, consume at completion.
	FC8AFixture Fixture({ RecipeShortSword });
	if (!Fixture.IsValid(*this) || !C8AAdd(*this, Fixture.Inventory.Get(), Wood, 3) || !C8AAdd(*this, Fixture.Inventory.Get(), Scrap, 4)) { return false; }
	UCraftingStationComponent* Bench = Fixture.SpawnStation(Workbench, C8AInRange);
	if (!TestNotNull(TEXT("Workbench station spawns"), Bench)) { return false; }
	Fixture.StartObserving();
	TestTrue(TEXT("Hand context refuses Recipe_ShortSword"),
		C8AIs(Fixture.Crafting->TryStartCraft(RecipeShortSword), ECraftingTransactionResult::StationRequired));
	TestTrue(TEXT("Recipe_ShortSword starts at the Workbench"),
		C8AIs(Fixture.Crafting->TryStartCraftAtStation(RecipeShortSword, Bench), ECraftingTransactionResult::Started));
	const int32 Revision = Fixture.Inventory->GetRevision();
	Fixture.AdvanceTime(3.9f);
	TestEqual(TEXT("Nothing consumed before 4 s"), Fixture.Inventory->GetTotalQuantity(Scrap), 4);
	Fixture.AdvanceTime(0.2f);
	TestTrue(TEXT("Completion succeeded"), Fixture.Listener->LastResult.IsSuccess());
	TestEqual(TEXT("Wood 3 - 2"), Fixture.Inventory->GetTotalQuantity(Wood), 1);
	TestEqual(TEXT("Scrap 4 - 4"), Fixture.Inventory->GetTotalQuantity(Scrap), 0);
	TestEqual(TEXT("ShortSword created"), Fixture.Inventory->GetTotalQuantity(ShortSword), 1);
	TestEqual(TEXT("One revision on completion"), Fixture.Inventory->GetRevision(), Revision + 1);
	TestEqual(TEXT("One inventory event on completion"), Fixture.Listener->InventoryEvents, 1);
	return true;
}

namespace
{
const TCHAR* C8BWorkbenchPath = TEXT("/Game/LevelPrototyping/CraftingSystem/Stations/BP_Workbench.BP_Workbench_C");
const TCHAR* C8BInteractablePath = TEXT("/Game/LevelPrototyping/interactionSystem/BPI_Interactable.BPI_Interactable_C");

ACraftingStationActor* C8BSpawnStationActor(const FC8AFixture& Fixture, UClass* Class, const FVector& Location,
	TSubclassOf<UCraftingStationDefinition> Definition, bool bSetDefinition)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACraftingStationActor* Actor = Fixture.World->SpawnActor<ACraftingStationActor>(Class, FTransform(Location), Params);
	if (Actor)
	{
		if (bSetDefinition)
		{
			C8ASetStationDefinition(Actor->GetCraftingStationComponent(), Definition);
		}
		Actor->DispatchBeginPlay();
	}
	return Actor;
}

APlayerController* C8BSpawnPlayerController(const FC8AFixture& Fixture, APawn* PawnToPossess)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>(APlayerController::StaticClass(), FTransform::Identity, Params);
	if (Controller && PawnToPossess)
	{
		Controller->Possess(PawnToPossess);
	}
	return Controller;
}

void C8BSetScreenClass(ACraftingStationActor* Actor, UClass* ScreenClass)
{
	FClassProperty* Property = FindFProperty<FClassProperty>(ACraftingStationActor::StaticClass(), TEXT("CraftingScreenClass"));
	check(Property);
	Property->SetObjectPropertyValue_InContainer(Actor, ScreenClass);
}

UClass* C8BGetScreenClass(const ACraftingStationActor* Actor)
{
	const FClassProperty* Property = FindFProperty<FClassProperty>(ACraftingStationActor::StaticClass(), TEXT("CraftingScreenClass"));
	return Property ? Cast<UClass>(Property->GetObjectPropertyValue_InContainer(Actor)) : nullptr;
}

int32 C8BCountLiveObjects(const UClass* Class)
{
	int32 Count = 0;
	for (TObjectIterator<UObject> It; It; ++It)
	{
		Count += ::IsValid(*It) && It->IsA(Class) && !It->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) ? 1 : 0;
	}
	return Count;
}

/** Everything a rejected interaction must leave untouched: inventory, crafting state, UI and input. */
struct FC8BUnchanged
{
	FC8AStacks Stacks;
	int32 Revision = 0;
	bool bCrafting = false;
	TSubclassOf<UCraftingRecipeDefinition> ActiveRecipe;
	UCraftingStationComponent* ActiveStation = nullptr;
	bool bShowMouseCursor = false;
	int32 InventoryScreens = 0;
	int32 CraftingComponents = 0;

	FC8BUnchanged(const FC8AFixture& Fixture, const APlayerController* Controller)
		: Stacks(C8ACapture(Fixture.Inventory.Get()))
		, Revision(Fixture.Inventory->GetRevision())
		, bCrafting(Fixture.Crafting->IsCrafting())
		, ActiveRecipe(Fixture.Crafting->GetActiveCraftRecipe())
		, ActiveStation(Fixture.Crafting->GetActiveCraftStation())
		, bShowMouseCursor(Controller->bShowMouseCursor)
		, InventoryScreens(C8BCountLiveObjects(UInventoryScreenWidget::StaticClass()))
		, CraftingComponents(C8BCountLiveObjects(UCraftingComponent::StaticClass()))
	{
	}

	void Check(FAutomationTestBase& Test, const FC8AFixture& Fixture, const APlayerController* Controller, const FString& What) const
	{
		Test.TestTrue(What + TEXT(": inventory ItemIds and quantities unchanged"), C8ACapture(Fixture.Inventory.Get()) == Stacks);
		Test.TestEqual(What + TEXT(": no inventory revision"), Fixture.Inventory->GetRevision(), Revision);
		Test.TestTrue(What + TEXT(": crafting state unchanged"), Fixture.Crafting->IsCrafting() == bCrafting
			&& Fixture.Crafting->GetActiveCraftRecipe() == ActiveRecipe && Fixture.Crafting->GetActiveCraftStation() == ActiveStation);
		Test.TestEqual(What + TEXT(": cursor visibility unchanged"), Controller->bShowMouseCursor, bShowMouseCursor);
		Test.TestEqual(What + TEXT(": no inventory/crafting screen created"), C8BCountLiveObjects(UInventoryScreenWidget::StaticClass()), InventoryScreens);
		Test.TestEqual(What + TEXT(": no crafting component created"), C8BCountLiveObjects(UCraftingComponent::StaticClass()), CraftingComponents);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC8BActorContractTest,
	"NullTide.Crafting.C8B.ActorContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC8BActorContractTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Native station actor root is abstract"), ACraftingStationActor::StaticClass()->HasAnyClassFlags(CLASS_Abstract));

	FC8AFixture Fixture({});
	if (!Fixture.IsValid(*this)) { return false; }
	ACraftingStationActor* Actor = C8BSpawnStationActor(Fixture, ACraftingTestStationActor::StaticClass(), C8AInRange, C8ABench(), true);
	if (!TestNotNull(TEXT("Station actor spawns"), Actor)) { return false; }

	const USceneComponent* Root = Actor->GetRootComponent();
	TestTrue(TEXT("Root is a plain, unscaled scene component"), Root && !Root->IsA<UPrimitiveComponent>()
		&& Root->GetRelativeScale3D().Equals(FVector::OneVector));

	TInlineComponentArray<UStaticMeshComponent*> Meshes(Actor);
	if (!TestEqual(TEXT("One placeholder mesh"), Meshes.Num(), 1)) { return false; }
	const UStaticMeshComponent* Mesh = Meshes[0];
	TestTrue(TEXT("Mesh has a placeholder static mesh"), Mesh->GetStaticMesh() != nullptr);
	TestTrue(TEXT("Mesh is attached to the root"), Mesh->GetAttachParent() == Root);
	TestTrue(TEXT("Mesh is 160 x 80 x 80 cm standing on the origin"), Mesh->GetRelativeScale3D().Equals(FVector(1.6f, 0.8f, 0.8f))
		&& Mesh->GetRelativeLocation().Equals(FVector(0.f, 0.f, 40.f)));
	TestTrue(TEXT("Mesh blocks (BlockAllDynamic)"), Mesh->GetCollisionProfileName() == FName(TEXT("BlockAllDynamic")));

	TInlineComponentArray<UBoxComponent*> Boxes(Actor);
	if (!TestEqual(TEXT("One interaction bounds box"), Boxes.Num(), 1)) { return false; }
	const UBoxComponent* Bounds = Boxes[0];
	TestTrue(TEXT("Bounds are attached to the root"), Bounds->GetAttachParent() == Root);
	TestTrue(TEXT("Bounds are query-only"), Bounds->GetCollisionEnabled() == ECollisionEnabled::QueryOnly);
	TestTrue(TEXT("Bounds overlap pawns"), Bounds->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Overlap);
	TestTrue(TEXT("Bounds generate overlap events"), Bounds->GetGenerateOverlapEvents());
	TestTrue(TEXT("Bounds extent is inside UseRange"),
		Bounds->GetScaledBoxExtent().Equals(FVector(150.f, 120.f, 110.f)) && Bounds->GetScaledBoxExtent().X < 300.f);
	TestTrue(TEXT("Bounds follow the actor"), Bounds->GetComponentLocation().Equals(Actor->GetActorLocation() + FVector(0.f, 0.f, 90.f)));
	TestFalse(TEXT("Bounds do not affect navigation"), Bounds->CanEverAffectNavigation());

	UCraftingStationComponent* Station = Actor->GetCraftingStationComponent();
	TInlineComponentArray<UCraftingStationComponent*> Stations(Actor);
	TestTrue(TEXT("Exactly one station component, exposed by the getter"), Station && Stations.Num() == 1 && Stations[0] == Station);
	TestTrue(TEXT("Station definition"), Station && Station->GetStationDefinition() == C8ABench());
	TestTrue(TEXT("Station is usable"), Station && Station->IsStationUsable());
	TestNull(TEXT("CraftingScreenClass is unset by default"), C8BGetScreenClass(Actor));

	TInlineComponentArray<UCraftingComponent*> CraftingComponents(Actor);
	TInlineComponentArray<UInventoryComponent*> Inventories(Actor);
	TestEqual(TEXT("No crafting authority on the station"), CraftingComponents.Num(), 0);
	TestEqual(TEXT("No inventory on the station"), Inventories.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC8BOpenRejectsTest,
	"NullTide.Crafting.C8B.OpenRejectsInvalidInteractor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC8BOpenRejectsTest::RunTest(const FString& Parameters)
{
	FC8AFixture Fixture({ C8ABenchTimed() });
	if (!Fixture.IsValid(*this) || !C8AAdd(*this, Fixture.Inventory.Get(), C8ABig(), 5)
		|| !C8AAdd(*this, Fixture.Inventory.Get(), C8ASingle(), 2)) { return false; }
	APlayerController* Controller = C8BSpawnPlayerController(Fixture, Fixture.Character);
	ACraftingStationActor* Bench = C8BSpawnStationActor(Fixture, ACraftingTestStationActor::StaticClass(), C8AInRange, C8ABench(), true);
	ACraftingStationActor* Unset = C8BSpawnStationActor(Fixture, ACraftingTestStationActor::StaticClass(), FVector(0.f, 200.f, 0.f), nullptr, false);
	if (!TestTrue(TEXT("Controller possesses the character; stations spawn"),
		Controller && Fixture.Character->GetController() == Controller && Bench && Unset)) { return false; }

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* PlainActor = Fixture.World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	AActor* DestroyedActor = Fixture.World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	ACharacter* Unpossessed = Fixture.World->SpawnActor<ACharacter>(ACharacter::StaticClass(), FVector(50.f, 0.f, 100.f), FRotator::ZeroRotator, Params);
	APlayerController* PawnlessController = C8BSpawnPlayerController(Fixture, nullptr);
	if (!TestTrue(TEXT("Helper actors spawn"), PlainActor && DestroyedActor && Unpossessed && PawnlessController)) { return false; }
	DestroyedActor->Destroy();

	// A craft is running at the bench: rejected interactions must not touch it either.
	if (!TestTrue(TEXT("A station craft is active"),
		C8AIs(Fixture.Crafting->TryStartCraftAtStation(C8ABenchTimed(), Bench->GetCraftingStationComponent()), ECraftingTransactionResult::Started))) { return false; }
	const FC8BUnchanged Before(Fixture, Controller);

	struct FCase { const TCHAR* Name; ACraftingStationActor* Station; AActor* Interactor; ECraftingStationOpenResult Expected; };
	const FCase Cases[] = {
		{ TEXT("Null interactor"), Bench, nullptr, ECraftingStationOpenResult::InvalidInteractor },
		{ TEXT("Destroyed interactor"), Bench, DestroyedActor, ECraftingStationOpenResult::InvalidInteractor },
		{ TEXT("Station without a definition"), Unset, Fixture.Character, ECraftingStationOpenResult::StationUnavailable },
		{ TEXT("Actor that is not a pawn or controller"), Bench, PlainActor, ECraftingStationOpenResult::NoPlayerController },
		{ TEXT("Pawn without a controller"), Bench, Unpossessed, ECraftingStationOpenResult::NoPlayerController },
		{ TEXT("Player controller without a pawn"), Bench, PawnlessController, ECraftingStationOpenResult::InvalidInteractor },
		{ TEXT("Player pawn, no crafting screen"), Bench, Fixture.Character, ECraftingStationOpenResult::NoCraftingScreen },
		{ TEXT("Player controller, no crafting screen"), Bench, Controller, ECraftingStationOpenResult::NoCraftingScreen },
	};
	for (const FCase& Case : Cases)
	{
		TestTrue(FString(Case.Name) + TEXT(": result"), Case.Station->GetOpenCraftingResult(Case.Interactor) == Case.Expected);
		TestFalse(FString(Case.Name) + TEXT(": nothing opens"), Case.Station->OpenCraftingForInteractor(Case.Interactor));
	}
	Before.Check(*this, Fixture, Controller, TEXT("Rejected interactions"));

	// Out of range (the station moved away; the craft is not polled and keeps running until completion).
	Bench->SetActorLocation(C8AOutOfRange);
	TestTrue(TEXT("Out of range: result"), Bench->GetOpenCraftingResult(Fixture.Character) == ECraftingStationOpenResult::OutOfRange);
	TestFalse(TEXT("Out of range: nothing opens"), Bench->OpenCraftingForInteractor(Fixture.Character));
	Before.Check(*this, Fixture, Controller, TEXT("Out-of-range interaction"));
	Bench->SetActorLocation(C8AInRange);

	// Even a fully valid request opens nothing in C8B: the station screen is wired in C8C.
	C8BSetScreenClass(Bench, UInventoryScreenWidget::StaticClass());
	TestTrue(TEXT("Valid request: Ready"), Bench->GetOpenCraftingResult(Fixture.Character) == ECraftingStationOpenResult::Ready);
	TestFalse(TEXT("Valid request: C8B still opens nothing"), Bench->OpenCraftingForInteractor(Fixture.Character));
	Before.Check(*this, Fixture, Controller, TEXT("Valid interaction in C8B"));

	TInlineComponentArray<UCraftingComponent*> PlayerCrafting(Fixture.Character);
	TestEqual(TEXT("The player still has exactly one crafting authority"), PlayerCrafting.Num(), 1);
	Fixture.AdvanceTime(2.1f);
	TestTrue(TEXT("The station craft completes normally afterwards"),
		!Fixture.Crafting->IsCrafting() && Fixture.Inventory->GetTotalQuantity(C8AOther()) == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC8BProductionWorkbenchTest,
	"NullTide.Crafting.C8B.ProductionWorkbench",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC8BProductionWorkbenchTest::RunTest(const FString& Parameters)
{
	UClass* WorkbenchClass = LoadClass<ACraftingStationActor>(nullptr, C8BWorkbenchPath);
	UClass* StationClass = LoadClass<UCraftingStationDefinition>(nullptr,
		TEXT("/Game/LevelPrototyping/CraftingSystem/Stations/Station_Workbench.Station_Workbench_C"));
	UClass* InteractableClass = LoadClass<UInterface>(nullptr, C8BInteractablePath);
	if (!TestTrue(TEXT("BP_Workbench, Station_Workbench and BPI_Interactable load"), WorkbenchClass && StationClass && InteractableClass)) { return false; }

	const ACraftingStationActor* Defaults = WorkbenchClass->GetDefaultObject<ACraftingStationActor>();
	TestTrue(TEXT("BP_Workbench station definition is Station_Workbench"),
		Defaults->GetCraftingStationComponent() && Defaults->GetCraftingStationComponent()->GetStationDefinition().Get() == StationClass);
	TestNull(TEXT("BP_Workbench has no crafting screen in C8B"), C8BGetScreenClass(Defaults));
	TestTrue(TEXT("BP_Workbench implements BPI_Interactable"), WorkbenchClass->ImplementsInterface(InteractableClass));

	// No extra crafting authority, inventory or recipe list authored on the Blueprint.
	bool bExtraAuthority = false;
	if (const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(WorkbenchClass))
	{
		if (const USimpleConstructionScript* ConstructionScript = Generated->SimpleConstructionScript)
		{
			for (const USCS_Node* Node : ConstructionScript->GetAllNodes())
			{
				bExtraAuthority |= Node && (Cast<UCraftingComponent>(Node->ComponentTemplate) || Cast<UInventoryComponent>(Node->ComponentTemplate)
					|| Cast<UCraftingStationComponent>(Node->ComponentTemplate));
			}
		}
	}
	TestFalse(TEXT("No crafting, inventory or second station component added in the Blueprint"), bExtraAuthority);

	FC8AFixture Fixture({ C8AHandInstant() });
	if (!Fixture.IsValid(*this) || !C8AAdd(*this, Fixture.Inventory.Get(), C8ABig(), 5)
		|| !C8AAdd(*this, Fixture.Inventory.Get(), C8ASingle(), 2)) { return false; }
	APlayerController* Controller = C8BSpawnPlayerController(Fixture, Fixture.Character);
	ACraftingStationActor* Workbench = C8BSpawnStationActor(Fixture, WorkbenchClass, C8AInRange, nullptr, false);
	if (!TestTrue(TEXT("Workbench spawns for a possessed player"), Controller && Workbench)) { return false; }
	TInlineComponentArray<UCraftingComponent*> WorkbenchCrafting(Workbench);
	TestEqual(TEXT("Spawned workbench has no crafting authority"), WorkbenchCrafting.Num(), 0);
	TInlineComponentArray<UBoxComponent*> WorkbenchBoxes(Workbench);
	TInlineComponentArray<UStaticMeshComponent*> WorkbenchMeshes(Workbench);
	TestTrue(TEXT("Production workbench keeps the native layout (one mesh and one bounds box, both on the root)"),
		WorkbenchBoxes.Num() == 1 && WorkbenchMeshes.Num() == 1
		&& WorkbenchBoxes[0]->GetAttachParent() == Workbench->GetRootComponent()
		&& WorkbenchMeshes[0]->GetAttachParent() == Workbench->GetRootComponent()
		&& WorkbenchMeshes[0]->GetRelativeScale3D().Equals(FVector(1.6f, 0.8f, 0.8f))
		&& WorkbenchBoxes[0]->GetScaledBoxExtent().Equals(FVector(150.f, 120.f, 110.f)));
	TestTrue(TEXT("Spawned workbench station is usable"), Workbench->GetCraftingStationComponent()->IsStationUsable());

	// E_InteractionType from the interface: Press.
	UFunction* GetType = Workbench->FindFunction(FName(TEXT("GetInteractionType")));
	if (TestNotNull(TEXT("GetInteractionType is implemented"), GetType))
	{
		TArray<uint8> Buffer;
		Buffer.SetNumZeroed(FMath::Max<int32>(GetType->ParmsSize, 1));
		Workbench->ProcessEvent(GetType, Buffer.GetData());
		FString TypeName;
		// Blueprint functions return values through an out parameter (here "Type"), not a C++ return property.
		const FProperty* Return = GetType->GetReturnProperty();
		for (TFieldIterator<FProperty> It(GetType); It && !Return; ++It)
		{
			Return = It->HasAnyPropertyFlags(CPF_OutParm) ? *It : nullptr;
		}
		if (Return)
		{
			if (const FByteProperty* Byte = CastField<FByteProperty>(Return); Byte && Byte->Enum)
			{
				TypeName = Byte->Enum->GetDisplayNameTextByValue(*Byte->ContainerPtrToValuePtr<uint8>(Buffer.GetData())).ToString();
			}
			else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Return))
			{
				const int64 Value = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(Buffer.GetData()));
				TypeName = EnumProperty->GetEnum()->GetDisplayNameTextByValue(Value).ToString();
			}
		}
		TestEqual(TEXT("Interaction type is Press"), TypeName, FString(TEXT("Press")));
	}

	// Interact(Interactor) through the interface event: rejected safely while no screen is configured.
	TestTrue(TEXT("Production workbench in range: NoCraftingScreen"),
		Workbench->GetOpenCraftingResult(Fixture.Character) == ECraftingStationOpenResult::NoCraftingScreen);
	const FC8BUnchanged Before(Fixture, Controller);
	UFunction* Interact = Workbench->FindFunction(FName(TEXT("Interact")));
	if (TestNotNull(TEXT("Interact is implemented"), Interact))
	{
		const FObjectPropertyBase* InteractorParam = CastField<FObjectPropertyBase>(Interact->FindPropertyByName(FName(TEXT("Interactor"))));
		if (TestNotNull(TEXT("Interact has an Interactor parameter"), InteractorParam))
		{
			TArray<uint8> Buffer;
			Buffer.SetNumZeroed(FMath::Max<int32>(Interact->ParmsSize, 1));
			InteractorParam->SetObjectPropertyValue_InContainer(Buffer.GetData(), Fixture.Character);
			Workbench->ProcessEvent(Interact, Buffer.GetData());
		}
	}
	Before.Check(*this, Fixture, Controller, TEXT("F on the production workbench"));
	TestTrue(TEXT("Hand crafting still works after the interaction"), Fixture.Crafting->TryStartCraft(C8AHandInstant()).IsSuccess());
	return true;
}

#endif
