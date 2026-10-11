#include "CraftingTestTypes.h"

#include "InventoryComponentTestTypes.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Crafting/CraftingComponent.h"
#include "Crafting/CraftingRecipeDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"
#include "UI/CraftingPanelWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
FCraftingItemQuantity C7Entry(TSubclassOf<UItemDefinition> Definition, int32 Quantity)
{
	FCraftingItemQuantity Entry;
	Entry.ItemDefinition = Definition;
	Entry.Quantity = Quantity;
	return Entry;
}
}

UCraftingTimedTestRecipe::UCraftingTimedTestRecipe()
{
	Ingredients.Add(C7Entry(UInventoryStackableTestItemDefinition::StaticClass(), 3));
	Ingredients.Add(C7Entry(UInventoryTestItemDefinition::StaticClass(), 1));
	Outputs.Add(C7Entry(UInventoryOtherTestItemDefinition::StaticClass(), 1));
	CraftDuration = 2.f;
}

UCraftingTimedHeavyTestRecipe::UCraftingTimedHeavyTestRecipe()
{
	Ingredients.Add(C7Entry(UInventoryStackableTestItemDefinition::StaticClass(), 1));
	Outputs.Add(C7Entry(UInventoryOtherTestItemDefinition::StaticClass(), 2));
	CraftDuration = 1.f;
}

void UCraftingTimedTestListener::HandleStarted(TSubclassOf<UCraftingRecipeDefinition> RecipeClass, float Duration)
{
	++StartedEvents;
	LastDuration = Duration;
	if (bStartFromStarted && Crafting)
	{
		NestedResults.Add(Crafting->TryStartCraft(NestedRecipe));
	}
}

void UCraftingTimedTestListener::HandleCancelled(TSubclassOf<UCraftingRecipeDefinition> RecipeClass, ECraftCancelReason Reason)
{
	++CancelledEvents;
	LastCancelReason = Reason;
}

void UCraftingTimedTestListener::HandleResult(TSubclassOf<UCraftingRecipeDefinition> RecipeClass, const FCraftingTransactionResult& Result)
{
	++ResultEvents;
	LastResult = Result;
	bWasCraftingDuringResult = Crafting && Crafting->IsCrafting();
	if (bStartFromResult && Crafting)
	{
		NestedResults.Add(Crafting->TryStartCraft(NestedRecipe));
	}
}

void UCraftingTimedTestListener::HandleInventoryChanged(int32 NewRevision)
{
	++InventoryEvents;
}

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using FC7RecipeList = TArray<TSubclassOf<UCraftingRecipeDefinition>>;

TSubclassOf<UItemDefinition> C7Big() { return UInventoryStackableTestItemDefinition::StaticClass(); }
TSubclassOf<UItemDefinition> C7Single() { return UInventoryTestItemDefinition::StaticClass(); }
TSubclassOf<UItemDefinition> C7Other() { return UInventoryOtherTestItemDefinition::StaticClass(); }
TSubclassOf<UCraftingRecipeDefinition> C7Timed() { return UCraftingTimedTestRecipe::StaticClass(); }      // 2 s
TSubclassOf<UCraftingRecipeDefinition> C7Instant() { return UCraftingTestRecipeDefinition::StaticClass(); } // instant

void C7SetRecipes(UCraftingComponent* Crafting, const FC7RecipeList& Recipes)
{
	FArrayProperty* Property = FindFProperty<FArrayProperty>(UCraftingComponent::StaticClass(), TEXT("AvailableRecipes"));
	check(Property);
	*Property->ContainerPtrToValuePtr<FC7RecipeList>(Crafting) = Recipes;
}

TArray<TPair<FGuid, int32>> C7Capture(const UInventoryComponent* Inventory)
{
	TArray<TPair<FGuid, int32>> Stacks;
	for (const UItemInstance* Item : Inventory->GetItemsSnapshot())
	{
		Stacks.Emplace(Item ? Item->GetInstanceId() : FGuid(), Item ? Item->GetQuantity() : 0);
	}
	return Stacks;
}

template <typename DelegateType>
int32 C7BindingCount(const DelegateType& Delegate, const UObject* Object)
{
	int32 Count = 0;
	for (const UObject* Bound : Delegate.GetAllObjects())
	{
		Count += Bound == Object ? 1 : 0;
	}
	return Count;
}

FInventoryDefinitionQuantity C7Request(TSubclassOf<UItemDefinition> Definition, int32 Quantity)
{
	FInventoryDefinitionQuantity Request;
	Request.DefinitionClass = Definition;
	Request.Quantity = Quantity;
	return Request;
}

bool C7Is(const FCraftingTransactionResult& Result, ECraftingTransactionResult Expected)
{
	return Result.Result == Expected;
}

/** A spawned character in a transient world with inventory and crafting components; timers advance manually. */
struct FC7Fixture
{
	UWorld* World = nullptr;
	ACharacter* Character = nullptr;
	TStrongObjectPtr<UInventoryComponent> Inventory;
	TStrongObjectPtr<UCraftingComponent> Crafting;
	TStrongObjectPtr<UCraftingTimedTestListener> Listener;

	explicit FC7Fixture(const FC7RecipeList& Recipes)
		: Listener(NewObject<UCraftingTimedTestListener>(GetTransientPackage()))
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!World)
		{
			return;
		}
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
		C7SetRecipes(Crafting.Get(), Recipes);
		Listener->Crafting = Crafting.Get();
	}

	~FC7Fixture()
	{
		Crafting.Reset();
		Inventory.Reset();
		if (World)
		{
			World->DestroyWorld(false);
		}
	}

	bool IsValid(FAutomationTestBase& Test) const
	{
		return Test.TestTrue(TEXT("Timed crafting fixture is ready"), World && Character && Inventory && Crafting);
	}

	void StartObserving()
	{
		Crafting->OnCraftStarted.AddDynamic(Listener.Get(), &UCraftingTimedTestListener::HandleStarted);
		Crafting->OnCraftCancelled.AddDynamic(Listener.Get(), &UCraftingTimedTestListener::HandleCancelled);
		Crafting->OnCraftingResult.AddDynamic(Listener.Get(), &UCraftingTimedTestListener::HandleResult);
		Inventory->OnInventoryChanged.AddDynamic(Listener.Get(), &UCraftingTimedTestListener::HandleInventoryChanged);
	}

	/**
	 * Advances the world timers like real frames would. FTimerManager ticks once per frame and activates
	 * timers set before that frame's tick only after advancing, so a zero-length frame activates them first.
	 */
	void AdvanceTime(float Seconds) const
	{
		FTimerManager& Timers = World->GetTimerManager();
		++GFrameCounter;
		Timers.Tick(0.f);
		++GFrameCounter;
		Timers.Tick(Seconds);
	}

	/** Moves the character (as WASD or path following would) and reports a movement update. */
	void MoveCharacterTo(const FVector& Location) const
	{
		const FVector Old = Character->GetActorLocation();
		Character->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		Character->OnCharacterMovementUpdated.Broadcast(0.016f, Old, FVector::ZeroVector);
	}
};

bool C7Add(FAutomationTestBase& Test, UInventoryComponent* Inventory, TSubclassOf<UItemDefinition> Definition, int32 Quantity)
{
	return Test.TestTrue(FString::Printf(TEXT("Setup adds %d of %s"), Quantity, *GetNameSafe(Definition.Get())),
		Inventory->TryAddDefinitionQuantity(Definition, Quantity).IsSuccess());
}

bool C7AddTimedIngredients(FAutomationTestBase& Test, UInventoryComponent* Inventory)
{
	return C7Add(Test, Inventory, C7Big(), 5) && C7Add(Test, Inventory, C7Single(), 2);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC7StartCompleteTest,
	"NullTide.Crafting.C7.StartAndTimedCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC7StartCompleteTest::RunTest(const FString& Parameters)
{
	FC7Fixture Fixture({ C7Timed(), C7Instant() });
	if (!Fixture.IsValid(*this) || !C7AddTimedIngredients(*this, Fixture.Inventory.Get())) { return false; }
	Fixture.StartObserving();
	const TArray<TPair<FGuid, int32>> Before = C7Capture(Fixture.Inventory.Get());
	const int32 RevisionBefore = Fixture.Inventory->GetRevision();

	const FCraftingTransactionResult Start = Fixture.Crafting->TryStartCraft(C7Timed());
	TestTrue(TEXT("Timed start returns Started"), C7Is(Start, ECraftingTransactionResult::Started));
	TestTrue(TEXT("Component is crafting"), Fixture.Crafting->IsCrafting());
	TestTrue(TEXT("Active recipe"), Fixture.Crafting->GetActiveCraftRecipe() == C7Timed());
	TestEqual(TEXT("OnCraftStarted fired once"), Fixture.Listener->StartedEvents, 1);
	TestEqual(TEXT("OnCraftStarted carries the duration"), Fixture.Listener->LastDuration, 2.f);
	TestEqual(TEXT("No result notification on start"), Fixture.Listener->ResultEvents, 0);
	TestTrue(TEXT("Start consumes nothing (contents and ItemIds)"), C7Capture(Fixture.Inventory.Get()) == Before);
	TestEqual(TEXT("Start changes no revision"), Fixture.Inventory->GetRevision(), RevisionBefore);
	TestEqual(TEXT("Start emits no inventory event"), Fixture.Listener->InventoryEvents, 0);
	TestEqual(TEXT("Remaining time at start"), Fixture.Crafting->GetActiveCraftRemainingSeconds(), 2.f, 0.01f);

	Fixture.AdvanceTime(1.f);
	TestEqual(TEXT("Halfway progress"), Fixture.Crafting->GetActiveCraftProgress(), 0.5f, 0.01f);
	TestEqual(TEXT("Halfway remaining"), Fixture.Crafting->GetActiveCraftRemainingSeconds(), 1.f, 0.01f);
	TestTrue(TEXT("Still nothing consumed halfway"), C7Capture(Fixture.Inventory.Get()) == Before);

	Fixture.AdvanceTime(1.1f);
	TestFalse(TEXT("Idle after completion"), Fixture.Crafting->IsCrafting());
	TestEqual(TEXT("Big 5 - 3"), Fixture.Inventory->GetTotalQuantity(C7Big()), 2);
	TestEqual(TEXT("Single 2 - 1"), Fixture.Inventory->GetTotalQuantity(C7Single()), 1);
	TestEqual(TEXT("Other created"), Fixture.Inventory->GetTotalQuantity(C7Other()), 1);
	TestEqual(TEXT("Completion: exactly one revision"), Fixture.Inventory->GetRevision(), RevisionBefore + 1);
	TestEqual(TEXT("Completion: exactly one inventory event"), Fixture.Listener->InventoryEvents, 1);
	TestEqual(TEXT("Completion: exactly one result notification"), Fixture.Listener->ResultEvents, 1);
	TestTrue(TEXT("Completion result is success"), Fixture.Listener->LastResult.IsSuccess());
	TestFalse(TEXT("Listeners see an idle component during completion feedback"), Fixture.Listener->bWasCraftingDuringResult);
	TestEqual(TEXT("Progress resets"), Fixture.Crafting->GetActiveCraftProgress(), 0.f);
	TestEqual(TEXT("Movement delegate unbound after completion"),
		C7BindingCount(Fixture.Character->OnCharacterMovementUpdated, Fixture.Crafting.Get()), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC7InventoryChangedTest,
	"NullTide.Crafting.C7.InventoryChangedDuringCraft",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC7InventoryChangedTest::RunTest(const FString& Parameters)
{
	// Ingredient removed during the craft: completion fails atomically.
	{
		FC7Fixture Fixture({ C7Timed() });
		if (!Fixture.IsValid(*this) || !C7AddTimedIngredients(*this, Fixture.Inventory.Get())) { return false; }
		Fixture.StartObserving();
		if (!TestTrue(TEXT("Start"), Fixture.Crafting->TryStartCraft(C7Timed()).Result == ECraftingTransactionResult::Started)) { return false; }
		TestTrue(TEXT("Removing an ingredient does not cancel"),
			Fixture.Inventory->TryApplyDefinitionTransaction({ C7Request(C7Single(), 2) }, {}).IsSuccess() && Fixture.Crafting->IsCrafting());
		const TArray<TPair<FGuid, int32>> BeforeCompletion = C7Capture(Fixture.Inventory.Get());
		const int32 Revision = Fixture.Inventory->GetRevision();
		const int32 Events = Fixture.Listener->InventoryEvents;
		Fixture.AdvanceTime(2.1f);
		TestTrue(TEXT("Completion reports the missing ingredient"),
			Fixture.Listener->LastResult.Result == ECraftingTransactionResult::InsufficientIngredients && Fixture.Listener->LastResult.EntryIndex == 1);
		TestTrue(TEXT("Failed completion changes nothing"), C7Capture(Fixture.Inventory.Get()) == BeforeCompletion);
		TestEqual(TEXT("Failed completion: no revision"), Fixture.Inventory->GetRevision(), Revision);
		TestEqual(TEXT("Failed completion: no inventory event"), Fixture.Listener->InventoryEvents, Events);
		TestFalse(TEXT("Idle after failed completion"), Fixture.Crafting->IsCrafting());
		TestEqual(TEXT("One result notification"), Fixture.Listener->ResultEvents, 1);
	}

	// Inventory filled during the craft: two outputs no longer fit (Big 5 -> 4 frees no slot).
	{
		const TSubclassOf<UCraftingRecipeDefinition> Heavy = UCraftingTimedHeavyTestRecipe::StaticClass();
		FC7Fixture Fixture({ Heavy });
		if (!Fixture.IsValid(*this) || !C7Add(*this, Fixture.Inventory.Get(), C7Big(), 5)) { return false; }
		Fixture.StartObserving();
		if (!TestTrue(TEXT("Heavy start passes the preflight"), Fixture.Crafting->TryStartCraft(Heavy).Result == ECraftingTransactionResult::Started)) { return false; }
		TestTrue(TEXT("Filling the inventory does not cancel"),
			C7Add(*this, Fixture.Inventory.Get(), C7Single(), 22) && Fixture.Crafting->IsCrafting());
		TestEqual(TEXT("One free slot left"), Fixture.Inventory->GetItemCount(), 23);
		const TArray<TPair<FGuid, int32>> BeforeCompletion = C7Capture(Fixture.Inventory.Get());
		const int32 Revision = Fixture.Inventory->GetRevision();
		const int32 Events = Fixture.Listener->InventoryEvents;
		Fixture.AdvanceTime(1.1f);
		TestTrue(TEXT("Completion reports output capacity"),
			Fixture.Listener->LastResult.Result == ECraftingTransactionResult::OutputCapacityFull);
		TestTrue(TEXT("Capacity failure changes nothing"), C7Capture(Fixture.Inventory.Get()) == BeforeCompletion);
		TestEqual(TEXT("Capacity failure: no revision"), Fixture.Inventory->GetRevision(), Revision);
		TestEqual(TEXT("Capacity failure: no inventory event"), Fixture.Listener->InventoryEvents, Events);
		TestFalse(TEXT("Idle after the failed completion"), Fixture.Crafting->IsCrafting());
	}

	// Unregistered while crafting: completion re-validates registration.
	{
		FC7Fixture Fixture({ C7Timed() });
		if (!Fixture.IsValid(*this) || !C7AddTimedIngredients(*this, Fixture.Inventory.Get())) { return false; }
		Fixture.StartObserving();
		Fixture.Crafting->TryStartCraft(C7Timed());
		C7SetRecipes(Fixture.Crafting.Get(), {});
		Fixture.AdvanceTime(2.1f);
		TestTrue(TEXT("Completion refuses an unregistered recipe"),
			Fixture.Listener->LastResult.Result == ECraftingTransactionResult::RecipeNotAvailable);
		TestEqual(TEXT("Nothing consumed"), Fixture.Inventory->GetTotalQuantity(C7Big()), 5);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC7ExplicitCancelTest,
	"NullTide.Crafting.C7.ExplicitCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC7ExplicitCancelTest::RunTest(const FString& Parameters)
{
	FC7Fixture Fixture({ C7Timed() });
	if (!Fixture.IsValid(*this) || !C7AddTimedIngredients(*this, Fixture.Inventory.Get())) { return false; }
	Fixture.StartObserving();
	const TArray<TPair<FGuid, int32>> Before = C7Capture(Fixture.Inventory.Get());
	const int32 Revision = Fixture.Inventory->GetRevision();
	Fixture.Crafting->TryStartCraft(C7Timed());
	Fixture.AdvanceTime(1.f);
	TestTrue(TEXT("CancelCraft succeeds while crafting"), Fixture.Crafting->CancelCraft());
	TestFalse(TEXT("Idle after cancel"), Fixture.Crafting->IsCrafting());
	TestEqual(TEXT("One cancel notification"), Fixture.Listener->CancelledEvents, 1);
	TestTrue(TEXT("Cancel reason Explicit"), Fixture.Listener->LastCancelReason == ECraftCancelReason::Explicit);
	TestTrue(TEXT("Cancel changes nothing"), C7Capture(Fixture.Inventory.Get()) == Before);
	TestEqual(TEXT("Cancel: no revision"), Fixture.Inventory->GetRevision(), Revision);
	Fixture.AdvanceTime(5.f);
	TestEqual(TEXT("Cancelled timer never completes"), Fixture.Listener->ResultEvents, 0);
	TestFalse(TEXT("CancelCraft is false when idle"), Fixture.Crafting->CancelCraft());
	TestEqual(TEXT("No extra cancel notification"), Fixture.Listener->CancelledEvents, 1);
	TestEqual(TEXT("Movement delegates unbound after cancel"),
		C7BindingCount(Fixture.Character->MovementModeChangedDelegate, Fixture.Crafting.Get()), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC7MovementCancelTest,
	"NullTide.Crafting.C7.MovementAndJumpCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC7MovementCancelTest::RunTest(const FString& Parameters)
{
	FC7Fixture Fixture({ C7Timed() });
	if (!Fixture.IsValid(*this) || !C7AddTimedIngredients(*this, Fixture.Inventory.Get())) { return false; }
	Fixture.StartObserving();
	const FVector Start = Fixture.Character->GetActorLocation();

	// Displacement beyond 10 cm cancels with Movement.
	Fixture.Crafting->TryStartCraft(C7Timed());
	TestEqual(TEXT("Movement update bound once"), C7BindingCount(Fixture.Character->OnCharacterMovementUpdated, Fixture.Crafting.Get()), 1);
	TestEqual(TEXT("Movement mode bound once"), C7BindingCount(Fixture.Character->MovementModeChangedDelegate, Fixture.Crafting.Get()), 1);
	Fixture.MoveCharacterTo(Start + FVector(5.f, 0.f, 0.f));
	TestTrue(TEXT("5 cm does not cancel"), Fixture.Crafting->IsCrafting());
	Fixture.MoveCharacterTo(Start + FVector(5.f, 0.f, 40.f));
	TestTrue(TEXT("Vertical movement alone does not cancel"), Fixture.Crafting->IsCrafting());
	Fixture.MoveCharacterTo(Start + FVector(15.f, 0.f, 0.f));
	TestFalse(TEXT("15 cm cancels"), Fixture.Crafting->IsCrafting());
	TestTrue(TEXT("Reason Movement"), Fixture.Listener->CancelledEvents == 1 && Fixture.Listener->LastCancelReason == ECraftCancelReason::Movement);
	TestEqual(TEXT("Movement delegates unbound after cancel"),
		C7BindingCount(Fixture.Character->OnCharacterMovementUpdated, Fixture.Crafting.Get()), 0);
	TestEqual(TEXT("Nothing consumed"), Fixture.Inventory->GetTotalQuantity(C7Big()), 5);

	// Click-to-move: path following moves in small steps; cumulative distance from the start location counts.
	Fixture.MoveCharacterTo(Start);
	Fixture.Crafting->TryStartCraft(C7Timed());
	Fixture.MoveCharacterTo(Start + FVector(0.f, 4.f, 0.f));
	Fixture.MoveCharacterTo(Start + FVector(0.f, 8.f, 0.f));
	TestTrue(TEXT("8 cm of path following does not cancel"), Fixture.Crafting->IsCrafting());
	Fixture.MoveCharacterTo(Start + FVector(0.f, 12.f, 0.f));
	TestFalse(TEXT("12 cm of path following cancels"), Fixture.Crafting->IsCrafting());
	TestTrue(TEXT("Path following reason Movement"), Fixture.Listener->CancelledEvents == 2 && Fixture.Listener->LastCancelReason == ECraftCancelReason::Movement);

	// Jumping or falling cancels with Jump.
	Fixture.MoveCharacterTo(Start);
	Fixture.Crafting->TryStartCraft(C7Timed());
	UCharacterMovementComponent* Movement = Fixture.Character->GetCharacterMovement();
	if (!TestNotNull(TEXT("Character movement"), Movement)) { return false; }
	Movement->SetMovementMode(MOVE_Walking);
	TestTrue(TEXT("Walking does not cancel"), Fixture.Crafting->IsCrafting());
	Movement->SetMovementMode(MOVE_Falling);
	TestTrue(TEXT("Movement mode is Falling"), Movement->MovementMode == MOVE_Falling);
	TestFalse(TEXT("Falling cancels"), Fixture.Crafting->IsCrafting());
	TestTrue(TEXT("Reason Jump"), Fixture.Listener->CancelledEvents == 3 && Fixture.Listener->LastCancelReason == ECraftCancelReason::Jump);
	TestEqual(TEXT("Mode delegate unbound after jump cancel"),
		C7BindingCount(Fixture.Character->MovementModeChangedDelegate, Fixture.Crafting.Get()), 0);
	Fixture.AdvanceTime(5.f);
	TestEqual(TEXT("No cancelled craft ever completes"), Fixture.Listener->ResultEvents, 0);
	TestEqual(TEXT("Nothing consumed by any cancelled craft"), Fixture.Inventory->GetTotalQuantity(C7Single()), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC7RejectionTest,
	"NullTide.Crafting.C7.RejectionsAndInstantRegression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC7RejectionTest::RunTest(const FString& Parameters)
{
	FC7Fixture Fixture({ C7Timed(), C7Instant() });
	if (!Fixture.IsValid(*this) || !C7Add(*this, Fixture.Inventory.Get(), C7Big(), 20)
		|| !C7Add(*this, Fixture.Inventory.Get(), C7Single(), 6)) { return false; }
	Fixture.StartObserving();

	// TryCraftRecipe cannot bypass the timer.
	const TArray<TPair<FGuid, int32>> Before = C7Capture(Fixture.Inventory.Get());
	const FCraftingTransactionResult Bypass = Fixture.Crafting->TryCraftRecipe(C7Timed());
	TestTrue(TEXT("TryCraftRecipe refuses timed recipes"), C7Is(Bypass, ECraftingTransactionResult::TimedCraftRequired));
	TestTrue(TEXT("Refusal changes nothing"), C7Capture(Fixture.Inventory.Get()) == Before);
	TestFalse(TEXT("Refusal starts nothing"), Fixture.Crafting->IsCrafting());
	TestEqual(TEXT("Refusal notifies once"), Fixture.Listener->ResultEvents, 1);

	// One active craft: every second request is CraftInProgress.
	TestTrue(TEXT("Timed start"), C7Is(Fixture.Crafting->TryStartCraft(C7Timed()), ECraftingTransactionResult::Started));
	TestTrue(TEXT("Second timed start"), C7Is(Fixture.Crafting->TryStartCraft(C7Timed()), ECraftingTransactionResult::CraftInProgress));
	TestTrue(TEXT("Instant start while crafting"), C7Is(Fixture.Crafting->TryStartCraft(C7Instant()), ECraftingTransactionResult::CraftInProgress));
	TestTrue(TEXT("Instant TryCraftRecipe while crafting"), C7Is(Fixture.Crafting->TryCraftRecipe(C7Instant()), ECraftingTransactionResult::CraftInProgress));
	TestTrue(TEXT("CanCraftRecipe reports CraftInProgress"), C7Is(Fixture.Crafting->CanCraftRecipe(C7Instant()), ECraftingTransactionResult::CraftInProgress));
	TestEqual(TEXT("One started notification"), Fixture.Listener->StartedEvents, 1);
	TestEqual(TEXT("Each refused request notified once"), Fixture.Listener->ResultEvents, 4);
	TestTrue(TEXT("The first craft is still the active one"), Fixture.Crafting->GetActiveCraftRecipe() == C7Timed());
	TestTrue(TEXT("Nothing consumed by refused requests"), C7Capture(Fixture.Inventory.Get()) == Before);
	Fixture.Crafting->CancelCraft();

	// Instant recipes are unchanged.
	TestTrue(TEXT("CanCraftRecipe is a normal preflight again"), Fixture.Crafting->CanCraftRecipe(C7Instant()).IsSuccess());
	const int32 StartedBefore = Fixture.Listener->StartedEvents;
	TestTrue(TEXT("Instant via TryStartCraft crafts immediately"), Fixture.Crafting->TryStartCraft(C7Instant()).IsSuccess());
	TestEqual(TEXT("Instant craft fires no started event"), Fixture.Listener->StartedEvents, StartedBefore);
	TestTrue(TEXT("Instant via TryCraftRecipe crafts immediately"), Fixture.Crafting->TryCraftRecipe(C7Instant()).IsSuccess());
	TestEqual(TEXT("Two instant crafts consumed Big 6"), Fixture.Inventory->GetTotalQuantity(C7Big()), 14);
	TestEqual(TEXT("Two instant crafts created two Others"), Fixture.Inventory->GetTotalQuantity(C7Other()), 2);
	TestFalse(TEXT("Instant crafts leave the component idle"), Fixture.Crafting->IsCrafting());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC7EndPlayTest,
	"NullTide.Crafting.C7.EndPlayCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC7EndPlayTest::RunTest(const FString& Parameters)
{
	FC7Fixture Fixture({ C7Timed() });
	if (!Fixture.IsValid(*this) || !C7AddTimedIngredients(*this, Fixture.Inventory.Get())) { return false; }
	Fixture.StartObserving();
	const TArray<TPair<FGuid, int32>> Before = C7Capture(Fixture.Inventory.Get());
	// Match the real lifecycle: EndPlay only follows BeginPlay.
	Fixture.Crafting->BeginPlay();
	Fixture.Crafting->TryStartCraft(C7Timed());
	Fixture.Crafting->EndPlay(EEndPlayReason::Destroyed);
	TestFalse(TEXT("EndPlay clears the active craft"), Fixture.Crafting->IsCrafting());
	TestEqual(TEXT("EndPlay sends no cancel notification"), Fixture.Listener->CancelledEvents, 0);
	TestEqual(TEXT("Movement delegates unbound"), C7BindingCount(Fixture.Character->OnCharacterMovementUpdated, Fixture.Crafting.Get()), 0);
	TestEqual(TEXT("Mode delegate unbound"), C7BindingCount(Fixture.Character->MovementModeChangedDelegate, Fixture.Crafting.Get()), 0);
	Fixture.AdvanceTime(5.f);
	TestEqual(TEXT("Cleared timer never completes"), Fixture.Listener->ResultEvents, 0);
	TestTrue(TEXT("Inventory untouched"), C7Capture(Fixture.Inventory.Get()) == Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC7ReentrancyTest,
	"NullTide.Crafting.C7.ReentrancyProtection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC7ReentrancyTest::RunTest(const FString& Parameters)
{
	FC7Fixture Fixture({ C7Timed() });
	if (!Fixture.IsValid(*this) || !C7Add(*this, Fixture.Inventory.Get(), C7Big(), 20)
		|| !C7Add(*this, Fixture.Inventory.Get(), C7Single(), 4)) { return false; }
	Fixture.Listener->NestedRecipe = C7Timed();
	Fixture.Listener->bStartFromStarted = true;
	Fixture.Listener->bStartFromResult = true;
	Fixture.StartObserving();

	Fixture.Crafting->TryStartCraft(C7Timed());
	TestTrue(TEXT("Nested start from OnCraftStarted is CraftInProgress"),
		Fixture.Listener->NestedResults.Num() == 1 && C7Is(Fixture.Listener->NestedResults[0], ECraftingTransactionResult::CraftInProgress));
	TestEqual(TEXT("Nested start sent no notification"), Fixture.Listener->ResultEvents, 0);
	TestEqual(TEXT("Still one started event"), Fixture.Listener->StartedEvents, 1);

	Fixture.Listener->bStartFromStarted = false;
	Fixture.AdvanceTime(2.1f);
	TestEqual(TEXT("One completion notification"), Fixture.Listener->ResultEvents, 1);
	TestTrue(TEXT("Completion succeeded"), Fixture.Listener->LastResult.IsSuccess());
	TestFalse(TEXT("Active state cleared before completion feedback"), Fixture.Listener->bWasCraftingDuringResult);
	TestTrue(TEXT("Nested start from OnCraftingResult is CraftInProgress"),
		Fixture.Listener->NestedResults.Num() == 2 && C7Is(Fixture.Listener->NestedResults[1], ECraftingTransactionResult::CraftInProgress));
	TestFalse(TEXT("No recursive craft started"), Fixture.Crafting->IsCrafting());
	TestEqual(TEXT("Only one craft consumed ingredients"), Fixture.Inventory->GetTotalQuantity(C7Big()), 17);

	Fixture.Listener->bStartFromResult = false;
	TestTrue(TEXT("A later independent start works"), C7Is(Fixture.Crafting->TryStartCraft(C7Timed()), ECraftingTransactionResult::Started));
	Fixture.AdvanceTime(2.1f);
	TestEqual(TEXT("Second craft consumed again"), Fixture.Inventory->GetTotalQuantity(C7Big()), 14);
	TestEqual(TEXT("Two crafts, two inventory events"), Fixture.Listener->InventoryEvents, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC7ProductionTest,
	"NullTide.Crafting.C7.ProductionPickaxe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC7ProductionTest::RunTest(const FString& Parameters)
{
	UClass* Wood = LoadClass<UItemDefinition>(nullptr, TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Wood.Item_Wood_C"));
	UClass* Stone = LoadClass<UItemDefinition>(nullptr, TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Stone.Item_Stone_C"));
	UClass* Pickaxe = LoadClass<UItemDefinition>(nullptr, TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Pickaxe.Item_Pickaxe_C"));
	UClass* RecipePickaxe = LoadClass<UCraftingRecipeDefinition>(nullptr,
		TEXT("/Game/LevelPrototyping/CraftingSystem/Recipes/Recipe_Pickaxe.Recipe_Pickaxe_C"));
	if (!TestTrue(TEXT("Production assets load"), Wood && Stone && Pickaxe && RecipePickaxe)) { return false; }
	TestEqual(TEXT("Recipe_Pickaxe takes 3 s"), RecipePickaxe->GetDefaultObject<UCraftingRecipeDefinition>()->CraftDuration, 3.f);

	FC7Fixture Fixture({ RecipePickaxe });
	if (!Fixture.IsValid(*this) || !C7Add(*this, Fixture.Inventory.Get(), Wood, 4) || !C7Add(*this, Fixture.Inventory.Get(), Stone, 3)) { return false; }
	const FGuid WoodStack = C7Capture(Fixture.Inventory.Get())[0].Key;
	Fixture.StartObserving();
	const int32 Revision = Fixture.Inventory->GetRevision();
	TestTrue(TEXT("Pickaxe starts"), C7Is(Fixture.Crafting->TryStartCraft(RecipePickaxe), ECraftingTransactionResult::Started));
	TestTrue(TEXT("TryCraftRecipe cannot bypass Recipe_Pickaxe"), C7Is(Fixture.Crafting->TryCraftRecipe(RecipePickaxe), ECraftingTransactionResult::CraftInProgress));
	Fixture.AdvanceTime(2.9f);
	TestEqual(TEXT("Nothing consumed before 3 s"), Fixture.Inventory->GetTotalQuantity(Stone), 3);
	Fixture.AdvanceTime(0.2f);
	const TArray<TPair<FGuid, int32>> After = C7Capture(Fixture.Inventory.Get());
	TestTrue(TEXT("Wood 2 in the same stack, Stone consumed, Pickaxe created"), After.Num() == 2 && After[0].Key == WoodStack
		&& After[0].Value == 2 && Fixture.Inventory->GetTotalQuantity(Pickaxe) == 1 && Fixture.Inventory->GetTotalQuantity(Stone) == 0);
	TestEqual(TEXT("One revision on completion"), Fixture.Inventory->GetRevision(), Revision + 1);
	TestEqual(TEXT("One inventory event on completion"), Fixture.Listener->InventoryEvents, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCraftingC7PanelTest,
	"NullTide.Crafting.C7.PanelCloseAndReopen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCraftingC7PanelTest::RunTest(const FString& Parameters)
{
	FC7Fixture Fixture({ C7Instant(), C7Timed() });
	if (!Fixture.IsValid(*this) || !C7Add(*this, Fixture.Inventory.Get(), C7Big(), 20)
		|| !C7Add(*this, Fixture.Inventory.Get(), C7Single(), 4)) { return false; }
	UClass* PanelClass = LoadClass<UCraftingPanelWidget>(nullptr, TEXT("/Game/LevelPrototyping/CraftingSystem/UI/W_CraftingPanel.W_CraftingPanel_C"));
	if (!TestNotNull(TEXT("Panel class loads"), PanelClass)) { return false; }
	auto Find = [](const UCraftingPanelWidget* Panel, const TCHAR* Name) { return Panel->WidgetTree->FindWidget(FName(Name)); };
	auto Label = [&Find](const UCraftingPanelWidget* Panel) { return Cast<UTextBlock>(Find(Panel, TEXT("CraftButtonText")))->GetText().ToString(); };
	auto Status = [&Find](const UCraftingPanelWidget* Panel) { return Cast<UTextBlock>(Find(Panel, TEXT("CraftStatusText")))->GetText().ToString(); };
	auto Bar = [&Find](const UCraftingPanelWidget* Panel) { return Cast<UProgressBar>(Find(Panel, TEXT("CraftProgressBar"))); };

	TStrongObjectPtr<UCraftingPanelWidget> Panel(CreateWidget<UCraftingPanelWidget>(Fixture.World, PanelClass));
	if (!TestTrue(TEXT("Panel has the C7 widgets"), Panel && Find(Panel.Get(), TEXT("CraftButtonText")) && Bar(Panel.Get()))) { return false; }
	Panel->SetCraftingSource(Fixture.Crafting.Get());
	TestEqual(TEXT("Idle label"), Label(Panel.Get()), FString(TEXT("CRAFT")));
	TestFalse(TEXT("Progress hidden while idle"), Bar(Panel.Get())->GetVisibility() == ESlateVisibility::HitTestInvisible);

	Panel->SelectRecipe(C7Timed());
	Cast<UButton>(Find(Panel.Get(), TEXT("CraftButton")))->OnClicked.Broadcast();
	TestTrue(TEXT("Click started the timed craft"), Fixture.Crafting->IsCrafting());
	TestEqual(TEXT("Label switches to CANCEL"), Label(Panel.Get()), FString(TEXT("CANCEL")));
	TestTrue(TEXT("Progress visible while crafting"), Bar(Panel.Get())->GetVisibility() == ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("Status shows progress, not success"), Status(Panel.Get()).StartsWith(TEXT("Crafting ")));
	TestEqual(TEXT("Nothing consumed on start"), Fixture.Inventory->GetTotalQuantity(C7Big()), 20);

	// Close the screen: the panel goes away, the craft keeps running.
	Panel.Reset();
	Fixture.AdvanceTime(1.f);
	TestTrue(TEXT("Closing the UI does not cancel"), Fixture.Crafting->IsCrafting());

	// Reopen: a fresh panel shows the active craft even though the first recipe would be the default.
	TStrongObjectPtr<UCraftingPanelWidget> Reopened(CreateWidget<UCraftingPanelWidget>(Fixture.World, PanelClass));
	Reopened->SetCraftingSource(Fixture.Crafting.Get());
	TestTrue(TEXT("Reopened panel selects the active recipe"), Reopened->GetSelectedRecipe() == C7Timed());
	TestEqual(TEXT("Reopened label is CANCEL"), Label(Reopened.Get()), FString(TEXT("CANCEL")));
	TestEqual(TEXT("Reopened progress is authoritative"), Bar(Reopened.Get())->GetPercent(), 0.5f, 0.02f);
	TestTrue(TEXT("Reopened status shows remaining time"), Status(Reopened.Get()).Contains(TEXT("1.0 s")));

	// CANCEL cancels through the component.
	Cast<UButton>(Find(Reopened.Get(), TEXT("CraftButton")))->OnClicked.Broadcast();
	TestFalse(TEXT("CANCEL cancelled the craft"), Fixture.Crafting->IsCrafting());
	TestEqual(TEXT("Cancel feedback"), Status(Reopened.Get()), FString(TEXT("Crafting cancelled.")));
	TestEqual(TEXT("Label back to CRAFT"), Label(Reopened.Get()), FString(TEXT("CRAFT")));
	TestEqual(TEXT("Cancel consumed nothing"), Fixture.Inventory->GetTotalQuantity(C7Big()), 20);

	// Completion feedback after a full timed craft.
	Cast<UButton>(Find(Reopened.Get(), TEXT("CraftButton")))->OnClicked.Broadcast();
	Fixture.AdvanceTime(2.1f);
	TestTrue(TEXT("Completion feedback is the success message"), Status(Reopened.Get()).StartsWith(TEXT("Crafted ")));
	TestEqual(TEXT("Completion consumed once"), Fixture.Inventory->GetTotalQuantity(C7Big()), 17);
	TestTrue(TEXT("Selection kept"), Reopened->GetSelectedRecipe() == C7Timed());
	return true;
}

#endif
