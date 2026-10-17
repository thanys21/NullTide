#include "BuildingTestTypes.h"

#include "InventoryComponentTestTypes.h"
#include "Blueprint/UserWidget.h"
#include "Building/BuildingPieceComponent.h"
#include "Building/BuildingPlacementComponent.h"
#include "Building/BuildingPreviewActor.h"
#include "Building/BuildingSubsystem.h"
#include "Building/ConstructionComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Crafting/CraftingComponent.h"
#include "Crafting/CraftingRecipeDefinition.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "ToolLoadout/ToolLoadoutComponent.h"
#include "UI/BuildingHudWidget.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
FInventoryDefinitionQuantity B2Cost(TSubclassOf<UItemDefinition> Definition, int32 Quantity)
{
	FInventoryDefinitionQuantity Cost;
	Cost.DefinitionClass = Definition;
	Cost.Quantity = Quantity;
	return Cost;
}

void B2MakeFloor(UBuildingDefinition& Definition, const TCHAR* Name, TSubclassOf<AActor> PieceClass)
{
	Definition.BuildingName = FText::FromString(Name);
	Definition.Category = EBuildingCategory::Foundation;
	Definition.PlacementMode = EBuildingPlacementMode::GridCell;
	Definition.PieceActorClass = PieceClass;
	Definition.Cost.Add(B2Cost(UInventoryStackableTestItemDefinition::StaticClass(), 4));
}

/** A WorldStatic box blocker built from the engine cube (the B1 crate pattern). */
AActor* B2SpawnBlocker(UWorld* World, const FVector& Center, const FVector& HalfExtent)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!World || !Cube)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor, TEXT("Box"));
	Mesh->SetStaticMesh(Cube);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Actor->SetRootComponent(Mesh);
	Actor->AddInstanceComponent(Mesh);
	Mesh->SetWorldTransform(FTransform(FRotator::ZeroRotator, Center, HalfExtent / 50.0));
	Mesh->RegisterComponent();
	return Actor;
}
}

ABuildingPlainPieceTestActor::ABuildingPlainPieceTestActor()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	BuildingPiece = CreateDefaultSubobject<UBuildingPieceComponent>(TEXT("BuildingPiece"));
}

void ABuildingSelfDestroyTestPiece::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	Destroy();
}

void ABuildingDriftTestPiece::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetActorLocation(Transform.GetLocation() + FVector(30.0, 0.0, 0.0));
}

void ABuildingBlockerSpawnTestPiece::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Inside the cell, between the surface samples (center and inset corners), so only the collision check sees it.
	B2SpawnBlocker(GetWorld(), GetActorLocation() + FVector(50.0, 50.0, 30.0), FVector(15.0, 15.0, 30.0));
}

UBuildingTestPlainActorDefinition::UBuildingTestPlainActorDefinition()
{
	B2MakeFloor(*this, TEXT("Test Plain Actor Floor"), ABuildingPlainPieceTestActor::StaticClass());
}

UBuildingTestSelfDestroyDefinition::UBuildingTestSelfDestroyDefinition()
{
	B2MakeFloor(*this, TEXT("Test Self-Destroying Floor"), ABuildingSelfDestroyTestPiece::StaticClass());
}

UBuildingTestDriftDefinition::UBuildingTestDriftDefinition()
{
	B2MakeFloor(*this, TEXT("Test Drifting Floor"), ABuildingDriftTestPiece::StaticClass());
}

UBuildingTestBlockerSpawnDefinition::UBuildingTestBlockerSpawnDefinition()
{
	B2MakeFloor(*this, TEXT("Test Blocker-Spawning Floor"), ABuildingBlockerSpawnTestPiece::StaticClass());
}

ABuildingPieceActor* UConstructionTestComponent::SpawnProvisionalPiece(TSubclassOf<UBuildingDefinition> DefinitionClass,
	const FBuildingPlacementResult& Resolved)
{
	++SpawnCount;
	return bSpawnReturnsNull ? nullptr : Super::SpawnProvisionalPiece(DefinitionClass, Resolved);
}

bool UConstructionTestComponent::ActivatePiece(ABuildingPieceActor& Piece, const FBuildingReservationHandle& Handle)
{
	if (const TFunction<void(ABuildingPieceActor&)> Hook = BeforeActivate)
	{
		Hook(Piece);
	}
	const bool bActivated = Super::ActivatePiece(Piece, Handle);
	if (bActivated)
	{
		if (const TFunction<void(ABuildingPieceActor&)> Hook = AfterActivate)
		{
			Hook(Piece);
		}
	}
	return bActivated;
}

void UConstructionTestListener::HandleInventoryChanged(int32 NewRevision)
{
	++InventoryEvents;
	if (const TFunction<void()> Hook = InventoryHook)
	{
		Hook();
	}
}

void UConstructionTestListener::HandleBuildingModeChanged(bool bBuildingModeActive)
{
	ModeChanges.Add(bBuildingModeActive);
}

bool UBuildingPlacementTestComponent::GetCursorWorldLocation(FVector& OutLocation) const
{
	OutLocation = CursorLocation;
	return bHasCursor;
}

UBuildingHudWidget* UBuildingPlacementTestComponent::CreateHudWidget(APlayerController& PlayerController)
{
	++HudCreateCount;
	// No viewport or local player in a transient world: create the widget without adding it to a screen.
	return HudWidgetClass ? CreateWidget<UBuildingHudWidget>(GetWorld(), HudWidgetClass) : nullptr;
}

void UConstructionTestListener::HandleBuildResult(TSubclassOf<UBuildingDefinition> DefinitionClass, const FBuildingConstructionResult& Result)
{
	BuildResults.Add(Result);
	PieceValidOnReceipt.Add(Result.Piece.IsValid());
	if (const TFunction<void(const FBuildingConstructionResult&)> Hook = BuildResultHook)
	{
		Hook(Result);
	}
}

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using FB2Buildings = TArray<TSubclassOf<UBuildingDefinition>>;

TSubclassOf<UItemDefinition> B2Wood() { return UInventoryStackableTestItemDefinition::StaticClass(); }
TSubclassOf<UItemDefinition> B2Other() { return UInventoryOtherTestItemDefinition::StaticClass(); }
TSubclassOf<UBuildingDefinition> B2FloorClass() { return UBuildingTestFloorDefinition::StaticClass(); }

/** Cell (1, 1) and two more cells on the fixture ground, all within range of a builder at the origin. */
const FVector B2CellA(300.0, 300.0, 0.0);
const FVector B2CellB(300.0, -300.0, 0.0);
const FVector B2CellC(500.0, 100.0, 0.0);

FBuildingOccupancyKey B2Key(const FVector& Cell)
{
	const FIntPoint Index = UBuildingSubsystem::WorldToCell(Cell);
	return FBuildingOccupancyKey(EBuildingOccupancyCategory::Floor, EBuildingEdgeAxis::None, Index.X, Index.Y, 0);
}

FBuildingPlacement B2Floor(const FVector& Cell, TSubclassOf<UBuildingDefinition> Definition = UBuildingTestFloorDefinition::StaticClass(),
	int32 Rotation = 0)
{
	return UBuildingSubsystem::MakePlacementFromWorld(*Definition->GetDefaultObject<UBuildingDefinition>(), Cell, Rotation);
}

FString B2Name(const EBuildResult Result)
{
	return StaticEnum<EBuildResult>()->GetNameStringByValue(static_cast<int64>(Result));
}

bool B2Expect(FAutomationTestBase& Test, const FString& What, const FBuildingConstructionResult& Result, const EBuildResult Expected)
{
	const bool bMatch = Result.Result == Expected;
	Test.TestTrue(FString::Printf(TEXT("%s: %s (got %s, %s)"), *What, *B2Name(Expected), *B2Name(Result.Result), *Result.Diagnostic), bMatch);
	return bMatch;
}

/** Asserts the code and that the diagnostic names the branch that produced it. */
bool B2ExpectBranch(FAutomationTestBase& Test, const FString& What, const FBuildingConstructionResult& Result, const EBuildResult Expected,
	const TCHAR* DiagnosticFragment)
{
	const bool bCode = B2Expect(Test, What, Result, Expected);
	const bool bBranch = Result.Diagnostic.Contains(DiagnosticFragment);
	Test.TestTrue(FString::Printf(TEXT("%s: diagnostic names the branch \"%s\" (got \"%s\")"), *What, DiagnosticFragment, *Result.Diagnostic), bBranch);
	return bCode && bBranch;
}

void B2SetBuildings(UConstructionComponent* Construction, const FB2Buildings& Buildings)
{
	FArrayProperty* Property = FindFProperty<FArrayProperty>(UConstructionComponent::StaticClass(), TEXT("AvailableBuildings"));
	check(Property);
	*Property->ContainerPtrToValuePtr<FB2Buildings>(Construction) = Buildings;
}

struct FB2Builder
{
	ACharacter* Pawn = nullptr;
	UInventoryComponent* Inventory = nullptr;
	UConstructionTestComponent* Construction = nullptr;
	UConstructionTestListener* Listener = nullptr;

	bool IsValid() const { return Pawn && Construction && Listener; }
};

/** Inventory revision, inventory events, build results and Wood, captured for exact deltas. */
struct FB2Counts
{
	int32 Revision = 0;
	int32 InventoryEvents = 0;
	int32 BuildResults = 0;
	int32 Wood = 0;

	static FB2Counts Capture(const FB2Builder& Builder)
	{
		FB2Counts Counts;
		Counts.Revision = Builder.Inventory ? Builder.Inventory->GetRevision() : 0;
		Counts.InventoryEvents = Builder.Listener->InventoryEvents;
		Counts.BuildResults = Builder.Listener->BuildResults.Num();
		Counts.Wood = Builder.Inventory ? Builder.Inventory->GetTotalQuantity(B2Wood()) : 0;
		return Counts;
	}
};

bool B2ExpectDelta(FAutomationTestBase& Test, const FString& What, const FB2Counts& Before, const FB2Builder& Builder,
	int32 Revision, int32 Events, int32 Results, int32 Wood)
{
	const FB2Counts After = FB2Counts::Capture(Builder);
	const bool bMatch = After.Revision - Before.Revision == Revision && After.InventoryEvents - Before.InventoryEvents == Events
		&& After.BuildResults - Before.BuildResults == Results && After.Wood - Before.Wood == Wood;
	Test.TestTrue(FString::Printf(TEXT("%s: revision %+d / events %+d / results %+d / wood %+d (got %+d / %+d / %+d / %+d)"), *What,
		Revision, Events, Results, Wood, After.Revision - Before.Revision, After.InventoryEvents - Before.InventoryEvents,
		After.BuildResults - Before.BuildResults, After.Wood - Before.Wood), bMatch);
	return bMatch;
}

/**
 * Transient world (the B1 setup) with ground x [-400, 1000], y [-700, 700], top at 0, and builders that carry a
 * native inventory and a seam-enabled construction component.
 */
struct FB2World
{
	UWorld* World = nullptr;
	UBuildingSubsystem* Subsystem = nullptr;
	TArray<TStrongObjectPtr<UConstructionTestListener>> Listeners;

	FB2World()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!World)
		{
			return;
		}
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		Subsystem = World->GetSubsystem<UBuildingSubsystem>();
		B2SpawnBlocker(World, FVector(300.0, 0.0, -50.0), FVector(700.0, 700.0, 50.0));
	}

	~FB2World()
	{
		if (World)
		{
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
		Listeners.Reset();
	}

	bool IsValid(FAutomationTestBase& Test) const
	{
		return Test.TestTrue(TEXT("Construction test world with subsystem"), World && Subsystem);
	}

	FB2Builder SpawnBuilder(const FVector& Location, const FB2Buildings& Buildings, int32 Wood, bool bWithInventory = true)
	{
		FB2Builder Builder;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Builder.Pawn = World->SpawnActor<ACharacter>(ACharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
		if (!Builder.Pawn)
		{
			return Builder;
		}
		if (bWithInventory)
		{
			Builder.Inventory = NewObject<UInventoryComponent>(Builder.Pawn);
			Builder.Pawn->AddInstanceComponent(Builder.Inventory);
			Builder.Inventory->RegisterComponent();
		}
		Builder.Construction = NewObject<UConstructionTestComponent>(Builder.Pawn);
		Builder.Pawn->AddInstanceComponent(Builder.Construction);
		Builder.Construction->RegisterComponent();
		B2SetBuildings(Builder.Construction, Buildings);
		// Begun play, so destroying the builder routes EndPlay like in a real level.
		Builder.Pawn->DispatchBeginPlay();
		if (Builder.Inventory && Wood > 0)
		{
			Builder.Inventory->TryAddDefinitionQuantity(B2Wood(), Wood);
		}
		Builder.Listener = NewObject<UConstructionTestListener>(GetTransientPackage());
		Listeners.Emplace(Builder.Listener);
		if (Builder.Inventory)
		{
			Builder.Inventory->OnInventoryChanged.AddDynamic(Builder.Listener, &UConstructionTestListener::HandleInventoryChanged);
		}
		Builder.Construction->OnBuildResult.AddDynamic(Builder.Listener, &UConstructionTestListener::HandleBuildResult);
		return Builder;
	}

	FB2Builder SpawnFloorBuilder(int32 Wood = 8)
	{
		return SpawnBuilder(FVector(0.0, 0.0, 92.0), { B2FloorClass() }, Wood);
	}

	UConstructionTestListener* AddListener(UConstructionComponent* Construction)
	{
		UConstructionTestListener* Listener = NewObject<UConstructionTestListener>(GetTransientPackage());
		Listeners.Emplace(Listener);
		Construction->OnBuildResult.AddDynamic(Listener, &UConstructionTestListener::HandleBuildResult);
		return Listener;
	}

	int32 LivePieces() const
	{
		int32 Count = 0;
		for (TActorIterator<ABuildingPieceActor> It(World); It; ++It)
		{
			Count += ::IsValid(*It) && !It->IsActorBeingDestroyed() ? 1 : 0;
		}
		return Count;
	}

	ABuildingPieceActor* PieceAt(const FVector& Cell) const
	{
		const UBuildingPieceComponent* Piece = Subsystem->GetOccupyingPiece(B2Key(Cell));
		return Piece ? Cast<ABuildingPieceActor>(Piece->GetOwner()) : nullptr;
	}

	/** Registry consistent, nothing reserved. */
	bool Clean(FAutomationTestBase& Test, const FString& What) const
	{
		const TArray<FString> Issues = Subsystem->AuditRegistry();
		for (const FString& Issue : Issues)
		{
			Test.AddError(What + TEXT(": ") + Issue);
		}
		const int32 Reserved = Subsystem->GetReservedKeyCount();
		Test.TestEqual(What + TEXT(": no reserved keys"), Reserved, 0);
		return Issues.IsEmpty() && Reserved == 0;
	}

	/** A failure that changed nothing: no piece left, the cell free, the registry clean. */
	void ExpectNoResidue(FAutomationTestBase& Test, const FString& What, int32 PiecesBefore, const FVector& Cell) const
	{
		Test.TestEqual(What + TEXT(": no piece left behind"), LivePieces(), PiecesBefore);
		Test.TestTrue(What + TEXT(": the cell is free"), Subsystem->IsKeyFree(B2Key(Cell)));
		Clean(Test, What);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2SuccessTest,
	"NullTide.Building.B2.Success",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2SuccessTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnFloorBuilder();
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }

	TestTrue(TEXT("Registered buildings"), Builder.Construction->GetAvailableBuildings() == FB2Buildings({ B2FloorClass() }));
	const FB2Counts BeforePreview = FB2Counts::Capture(Builder);
	const FBuildingConstructionResult Preview = Builder.Construction->CanBuild(B2FloorClass(), B2Floor(B2CellA));
	B2Expect(*this, TEXT("CanBuild"), Preview, EBuildResult::Success);
	TestFalse(TEXT("CanBuild has no piece"), Preview.Piece.IsValid());
	B2ExpectDelta(*this, TEXT("CanBuild is read-only"), BeforePreview, Builder, 0, 0, 0, 0);
	TestEqual(TEXT("CanBuild spawns nothing"), Builder.Construction->SpawnCount, 0);
	Fixture.Clean(*this, TEXT("After CanBuild"));

	const FB2Counts Before = FB2Counts::Capture(Builder);
	const FBuildingConstructionResult Result = Builder.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellA));
	if (!B2Expect(*this, TEXT("TryBuild"), Result, EBuildResult::Success)) { return false; }
	B2ExpectDelta(*this, TEXT("Exact cost, one revision, one event, one result"), Before, Builder, 1, 1, 1, -4);
	ABuildingPieceActor* Piece = Cast<ABuildingPieceActor>(Result.Piece.Get());
	if (!TestNotNull(TEXT("The result carries the placed piece"), Piece)) { return false; }
	const UBuildingPieceComponent* PieceComponent = Piece->GetBuildingPieceComponent();
	TestTrue(TEXT("Piece is Active and player-built"), PieceComponent->IsActivePiece() && PieceComponent->IsPlayerBuilt());
	TestTrue(TEXT("Piece occupies exactly its key"), Fixture.Subsystem->GetOccupyingPiece(B2Key(B2CellA)) == PieceComponent
		&& PieceComponent->GetOccupancyKeys() == TArray<FBuildingOccupancyKey>({ B2Key(B2CellA) }));
	TestTrue(TEXT("Piece is visible and collidable"), !Piece->IsProvisional() && !Piece->IsHidden() && Piece->GetActorEnableCollision());
	TestTrue(TEXT("Piece sits at the resolved transform"), Piece->GetActorLocation().Equals(Result.Placement.WorldTransform.GetLocation(), 0.1)
		&& Result.Placement.WorldTransform.GetLocation().Equals(FVector(300.0, 300.0, 0.0), 0.1));
	TestTrue(TEXT("Piece is owned by the builder"), Piece->GetOwner() == Builder.Pawn);
	TestTrue(TEXT("No diagnostic on success"), Result.Diagnostic.IsEmpty());
	TestEqual(TEXT("One piece"), Fixture.LivePieces(), 1);
	TestTrue(TEXT("The listener received the returned result"), Builder.Listener->BuildResults.Num() == 1
		&& Builder.Listener->BuildResults[0].Result == EBuildResult::Success && Builder.Listener->BuildResults[0].Piece == Result.Piece);
	TestFalse(TEXT("Guard released"), Builder.Construction->IsBuildInProgress());
	B2Expect(*this, TEXT("CanBuild on the built cell"), Builder.Construction->CanBuild(B2FloorClass(), B2Floor(B2CellA)), EBuildResult::Occupied);

	// A quarter turn exercises the rotation part of the exact-transform checks (steps 5 and 7).
	const FBuildingConstructionResult Rotated = Builder.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellC, B2FloorClass(), 1));
	if (B2Expect(*this, TEXT("Rotated build"), Rotated, EBuildResult::Success))
	{
		const AActor* RotatedPiece = Rotated.Piece.Get();
		TestTrue(TEXT("Rotated piece: yaw 90 at the resolved transform"), RotatedPiece
			&& FMath::IsNearlyEqual(FRotator::NormalizeAxis(RotatedPiece->GetActorRotation().Yaw), 90.0, 0.01)
			&& RotatedPiece->GetActorQuat().AngularDistance(Rotated.Placement.WorldTransform.GetRotation()) <= FMath::DegreesToRadians(0.01));
	}
	TestEqual(TEXT("Two pieces"), Fixture.LivePieces(), 2);
	Fixture.Clean(*this, TEXT("After success"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2ValidationFailuresTest,
	"NullTide.Building.B2.ValidationFailures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2ValidationFailuresTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnFloorBuilder();
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }

	// An existing Active piece on cell A and another owner's reservation on cell B.
	ABuildingTestPieceActor* Existing = Fixture.World->SpawnActorDeferred<ABuildingTestPieceActor>(ABuildingTestPieceActor::StaticClass(),
		B2Floor(B2CellA).WorldTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("Existing piece"), Existing)) { return false; }
	Existing->GetBuildingPieceComponent()->InitializePiece(B2FloorClass(), B2Floor(B2CellA), { B2Key(B2CellA) }, EBuildingPieceState::Active, false);
	Existing->FinishSpawning(B2Floor(B2CellA).WorldTransform);
	TestTrue(TEXT("Existing piece registered"), Fixture.Subsystem->RegisterActivePiece(Existing->GetBuildingPieceComponent()));
	TStrongObjectPtr<UBuildingTestReservationOwner> Other(NewObject<UBuildingTestReservationOwner>(GetTransientPackage()));
	const FBuildingReservationHandle OtherReservation = Fixture.Subsystem->ReserveKeys(Other.Get(), { B2Key(B2CellB) });
	TestTrue(TEXT("Other owner's reservation"), OtherReservation.IsValid());
	AActor* Crate = B2SpawnBlocker(Fixture.World, B2CellC + FVector(50.0, 50.0, 30.0), FVector(15.0, 15.0, 30.0));

	struct FCase { const TCHAR* Name; TSubclassOf<UBuildingDefinition> Definition; FBuildingPlacement Placement; EBuildResult Expected; };
	const FCase Cases[] = {
		{ TEXT("Null definition"), nullptr, B2Floor(B2CellC), EBuildResult::InvalidDefinition },
		{ TEXT("Abstract definition"), UBuildingDefinition::StaticClass(), B2Floor(B2CellC), EBuildResult::InvalidDefinition },
		{ TEXT("Unregistered building"), UBuildingTestWideFloorDefinition::StaticClass(),
			B2Floor(B2CellC, UBuildingTestWideFloorDefinition::StaticClass()), EBuildResult::NotAvailable },
		{ TEXT("Out of range"), B2FloorClass(), B2Floor(FVector(900.0, 500.0, 0.0)), EBuildResult::OutOfRange },
		{ TEXT("Existing piece"), B2FloorClass(), B2Floor(B2CellA), EBuildResult::Occupied },
		{ TEXT("Another owner's reservation"), B2FloorClass(), B2Floor(B2CellB), EBuildResult::Occupied },
		{ TEXT("Blocked"), B2FloorClass(), B2Floor(B2CellC), EBuildResult::Blocked },
	};
	for (const FCase& Case : Cases)
	{
		const int32 Pieces = Fixture.LivePieces();
		const int32 Reserved = Fixture.Subsystem->GetReservedKeyCount();
		const FB2Counts Before = FB2Counts::Capture(Builder);
		B2Expect(*this, Case.Name, Builder.Construction->CanBuild(Case.Definition, Case.Placement), Case.Expected);
		B2Expect(*this, Case.Name, Builder.Construction->TryBuild(Case.Definition, Case.Placement), Case.Expected);
		B2ExpectDelta(*this, Case.Name, Before, Builder, 0, 0, 1, 0);
		TestTrue(FString::Printf(TEXT("%s: nothing spawned or reserved"), Case.Name), Builder.Construction->SpawnCount == 0
			&& Fixture.LivePieces() == Pieces && Fixture.Subsystem->GetReservedKeyCount() == Reserved);
	}
	TestTrue(TEXT("The existing piece still holds its key"), Fixture.Subsystem->GetOccupyingPiece(B2Key(B2CellA)) == Existing->GetBuildingPieceComponent());
	Fixture.Subsystem->ReleaseReservation(OtherReservation, Other.Get());
	Crate->Destroy();
	Fixture.Clean(*this, TEXT("After validation failures"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2InventoryPreflightTest,
	"NullTide.Building.B2.InventoryPreflight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2InventoryPreflightTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }

	FB2Builder Short = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { B2FloorClass() }, 3);
	if (!TestTrue(TEXT("Short builder"), Short.IsValid() && Short.Inventory)) { return false; }
	FB2Counts Before = FB2Counts::Capture(Short);
	B2Expect(*this, TEXT("CanBuild with 3 of 4 Wood"), Short.Construction->CanBuild(B2FloorClass(), B2Floor(B2CellA)), EBuildResult::InsufficientResources);
	B2Expect(*this, TEXT("TryBuild with 3 of 4 Wood"), Short.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellA)), EBuildResult::InsufficientResources);
	B2ExpectDelta(*this, TEXT("Insufficient resources"), Before, Short, 0, 0, 1, 0);
	TestEqual(TEXT("Insufficient resources: nothing spawned"), Short.Construction->SpawnCount, 0);
	Fixture.ExpectNoResidue(*this, TEXT("Insufficient resources"), 0, B2CellA);

	FB2Builder NoInventory = Fixture.SpawnBuilder(FVector(0.0, -300.0, 92.0), { B2FloorClass() }, 0, false);
	if (!TestTrue(TEXT("Builder without inventory"), NoInventory.IsValid())) { return false; }
	B2Expect(*this, TEXT("No inventory"), NoInventory.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellB)), EBuildResult::InvalidInventory);
	TestTrue(TEXT("No inventory: one result, nothing spawned"), NoInventory.Listener->BuildResults.Num() == 1 && NoInventory.Construction->SpawnCount == 0);
	Fixture.ExpectNoResidue(*this, TEXT("No inventory"), 0, B2CellB);

	FB2Builder Legacy = Fixture.SpawnBuilder(FVector(0.0, 300.0, 92.0), { B2FloorClass() }, 8);
	if (!TestTrue(TEXT("Legacy builder"), Legacy.IsValid() && Legacy.Inventory)) { return false; }
	FBoolProperty* LegacyMode = FindFProperty<FBoolProperty>(UInventoryComponent::StaticClass(), TEXT("bLegacyInventoryMode"));
	if (!TestNotNull(TEXT("Legacy mode flag"), LegacyMode)) { return false; }
	LegacyMode->SetPropertyValue_InContainer(Legacy.Inventory, true);
	Before = FB2Counts::Capture(Legacy);
	B2Expect(*this, TEXT("Inventory not initialized"), Legacy.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellA)), EBuildResult::InvalidInventory);
	B2ExpectDelta(*this, TEXT("Inventory not initialized"), Before, Legacy, 0, 0, 1, 0);
	TestEqual(TEXT("Inventory not initialized: nothing spawned"), Legacy.Construction->SpawnCount, 0);
	Fixture.ExpectNoResidue(*this, TEXT("Inventory not initialized"), 0, B2CellA);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2SpawnFailureTest,
	"NullTide.Building.B2.SpawnFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2SpawnFailureTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	const TSubclassOf<UBuildingDefinition> SelfDestroy = UBuildingTestSelfDestroyDefinition::StaticClass();
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { B2FloorClass(), SelfDestroy }, 8);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }

	FB2Counts Before = FB2Counts::Capture(Builder);
	B2Expect(*this, TEXT("Piece destroys itself while spawning"), Builder.Construction->TryBuild(SelfDestroy, B2Floor(B2CellA, SelfDestroy)),
		EBuildResult::SpawnFailed);
	B2ExpectDelta(*this, TEXT("Self-destroying piece"), Before, Builder, 0, 0, 1, 0);
	TestEqual(TEXT("Self-destroying piece: spawned once"), Builder.Construction->SpawnCount, 1);
	Fixture.ExpectNoResidue(*this, TEXT("Self-destroying piece"), 0, B2CellA);

	Builder.Construction->bSpawnReturnsNull = true;
	Before = FB2Counts::Capture(Builder);
	B2Expect(*this, TEXT("Spawn returns null"), Builder.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellA)), EBuildResult::SpawnFailed);
	B2ExpectDelta(*this, TEXT("Spawn returns null"), Before, Builder, 0, 0, 1, 0);
	Fixture.ExpectNoResidue(*this, TEXT("Spawn returns null"), 0, B2CellA);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2TransformMismatchTest,
	"NullTide.Building.B2.TransformMismatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2TransformMismatchTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	const TSubclassOf<UBuildingDefinition> Drift = UBuildingTestDriftDefinition::StaticClass();
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { Drift }, 8);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	const FB2Counts Before = FB2Counts::Capture(Builder);
	B2Expect(*this, TEXT("Piece drifts in OnConstruction"), Builder.Construction->TryBuild(Drift, B2Floor(B2CellA, Drift)),
		EBuildResult::TransformMismatch);
	B2ExpectDelta(*this, TEXT("Drifting piece"), Before, Builder, 0, 0, 1, 0);
	Fixture.ExpectNoResidue(*this, TEXT("Drifting piece"), 0, B2CellA);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2CollisionChangedDuringSpawnTest,
	"NullTide.Building.B2.CollisionChangedDuringSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2CollisionChangedDuringSpawnTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	const TSubclassOf<UBuildingDefinition> Spawner = UBuildingTestBlockerSpawnDefinition::StaticClass();
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { Spawner }, 8);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	const FB2Counts Before = FB2Counts::Capture(Builder);
	B2Expect(*this, TEXT("A blocker appears while the piece spawns"), Builder.Construction->TryBuild(Spawner, B2Floor(B2CellA, Spawner)),
		EBuildResult::Blocked);
	B2ExpectDelta(*this, TEXT("Blocker during spawn"), Before, Builder, 0, 0, 1, 0);
	Fixture.ExpectNoResidue(*this, TEXT("Blocker during spawn"), 0, B2CellA);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2BlockerAfterPreviewTest,
	"NullTide.Building.B2.BlockerAfterPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2BlockerAfterPreviewTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnFloorBuilder();
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	B2Expect(*this, TEXT("Preview is green"), Builder.Construction->CanBuild(B2FloorClass(), B2Floor(B2CellA)), EBuildResult::Success);
	B2SpawnBlocker(Fixture.World, B2CellA + FVector(50.0, 50.0, 30.0), FVector(15.0, 15.0, 30.0));
	const FB2Counts Before = FB2Counts::Capture(Builder);
	B2Expect(*this, TEXT("The preview is not trusted"), Builder.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellA)), EBuildResult::Blocked);
	B2ExpectDelta(*this, TEXT("Blocker after preview"), Before, Builder, 0, 0, 1, 0);
	TestEqual(TEXT("Blocker after preview: nothing spawned"), Builder.Construction->SpawnCount, 0);
	Fixture.ExpectNoResidue(*this, TEXT("Blocker after preview"), 0, B2CellA);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2InventoryBusyAtCommitTest,
	"NullTide.Building.B2.InventoryBusyAtCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2InventoryBusyAtCommitTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnFloorBuilder();
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }

	// A build started from the builder inventory's own broadcast: the preflight passes, the payment finds it Busy.
	bool bTriggered = false;
	FBuildingConstructionResult Nested;
	UConstructionTestComponent* Construction = Builder.Construction;
	Builder.Listener->InventoryHook = [&bTriggered, &Nested, Construction]()
	{
		if (!bTriggered)
		{
			bTriggered = true;
			Nested = Construction->TryBuild(B2FloorClass(), B2Floor(B2CellA));
		}
	};
	const FB2Counts Before = FB2Counts::Capture(Builder);
	TestTrue(TEXT("Trigger mutation"), Builder.Inventory->TryAddDefinitionQuantity(B2Other(), 1).IsSuccess());
	B2Expect(*this, TEXT("Build inside the inventory broadcast"), Nested, EBuildResult::ResourcesRejectedRolledBack);
	B2ExpectDelta(*this, TEXT("Only the trigger's revision and event"), Before, Builder, 1, 1, 1, 0);
	TestEqual(TEXT("The piece was spawned, then rolled back"), Builder.Construction->SpawnCount, 1);
	Fixture.ExpectNoResidue(*this, TEXT("Busy at commit"), 0, B2CellA);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2ResourcesChangedDuringActivationTest,
	"NullTide.Building.B2.ResourcesChangedDuringActivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2ResourcesChangedDuringActivationTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnFloorBuilder(4);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	UInventoryComponent* Inventory = Builder.Inventory;
	Builder.Construction->AfterActivate = [Inventory](ABuildingPieceActor&)
	{
		Inventory->TryApplyDefinitionTransaction({ B2Cost(B2Wood(), 1) }, {});
	};
	const FB2Counts Before = FB2Counts::Capture(Builder);
	B2Expect(*this, TEXT("Wood removed during activation"), Builder.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellA)),
		EBuildResult::ResourcesRejectedRolledBack);
	B2ExpectDelta(*this, TEXT("Only the seam's own transaction"), Before, Builder, 1, 1, 1, -1);
	Fixture.ExpectNoResidue(*this, TEXT("Resources changed during activation"), 0, B2CellA);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2ActivationInterruptedTest,
	"NullTide.Building.B2.ActivationInterrupted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2ActivationInterruptedTest::RunTest(const FString& Parameters)
{
	enum class ECase { DestroyPieceAfterActivation, EndBuilderAfterActivation, EndBuilderBeforeCommit };
	struct FCase { ECase Kind; const TCHAR* Name; const TCHAR* Branch; };
	const FCase Cases[] = {
		{ ECase::DestroyPieceAfterActivation, TEXT("Piece destroyed after activation"), TEXT("destroyed during activation") },
		{ ECase::EndBuilderAfterActivation, TEXT("Builder ended after activation"), TEXT("ended play during activation") },
		{ ECase::EndBuilderBeforeCommit, TEXT("Builder ended before the commit (registry refuses)"), TEXT("refused to commit") },
	};
	for (const FCase& Case : Cases)
	{
		FB2World Fixture;
		if (!Fixture.IsValid(*this)) { return false; }
		FB2Builder Builder = Fixture.SpawnFloorBuilder();
		if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
		ACharacter* Pawn = Builder.Pawn;
		switch (Case.Kind)
		{
		case ECase::DestroyPieceAfterActivation:
			Builder.Construction->AfterActivate = [](ABuildingPieceActor& Piece) { Piece.Destroy(); };
			break;
		case ECase::EndBuilderAfterActivation:
			Builder.Construction->AfterActivate = [Pawn](ABuildingPieceActor&) { Pawn->Destroy(); };
			break;
		case ECase::EndBuilderBeforeCommit:
			Builder.Construction->BeforeActivate = [Pawn](ABuildingPieceActor&) { Pawn->Destroy(); };
			break;
		}
		const FB2Counts Before = FB2Counts::Capture(Builder);
		const FBuildingConstructionResult Result = Builder.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellA));
		B2ExpectBranch(*this, Case.Name, Result, EBuildResult::BuildInterrupted, Case.Branch);
		B2ExpectDelta(*this, Case.Name, Before, Builder, 0, 0, 1, 0);
		TestFalse(FString(Case.Name) + TEXT(": no piece in the result"), Result.Piece.IsValid());
		Fixture.ExpectNoResidue(*this, Case.Name, 0, B2CellA);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2PieceDestroyedAfterPaymentTest,
	"NullTide.Building.B2.PieceDestroyedAfterPayment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2PieceDestroyedAfterPaymentTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnFloorBuilder();
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	FB2World* World = &Fixture;
	Builder.Listener->InventoryHook = [World]()
	{
		if (ABuildingPieceActor* Piece = World->PieceAt(B2CellA))
		{
			Piece->Destroy();
		}
	};
	AddExpectedMessagePlain(TEXT("destroyed by an inventory listener during payment"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	const FB2Counts Before = FB2Counts::Capture(Builder);
	const FBuildingConstructionResult Result = Builder.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellA));
	B2Expect(*this, TEXT("Paid, then destroyed by a listener"), Result, EBuildResult::Success);
	B2ExpectDelta(*this, TEXT("Paid once, no refund"), Before, Builder, 1, 1, 1, -4);
	TestFalse(TEXT("The weak piece reads invalid"), Result.Piece.IsValid());
	TestTrue(TEXT("The diagnostic reports destruction during payment"), Result.Diagnostic.Contains(TEXT("destroyed during payment")));
	TestTrue(TEXT("The listener saw the same result"), Builder.Listener->BuildResults.Num() == 1
		&& Builder.Listener->BuildResults[0].Result == EBuildResult::Success && !Builder.Listener->PieceValidOnReceipt[0]
		&& Builder.Listener->BuildResults[0].Diagnostic == Result.Diagnostic);
	Fixture.ExpectNoResidue(*this, TEXT("Piece destroyed after payment"), 0, B2CellA);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2PieceDestroyedDuringBuildResultTest,
	"NullTide.Building.B2.PieceDestroyedDuringBuildResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2PieceDestroyedDuringBuildResultTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnFloorBuilder();
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	UConstructionTestListener* Second = Fixture.AddListener(Builder.Construction);
	// Whichever listener runs first destroys the piece; the test does not depend on delegate order.
	bool bDestroyed = false;
	auto DestroyOnce = [&bDestroyed](const FBuildingConstructionResult& Result)
	{
		if (!bDestroyed && Result.Piece.IsValid())
		{
			bDestroyed = true;
			Result.Piece->Destroy();
		}
	};
	Builder.Listener->BuildResultHook = DestroyOnce;
	Second->BuildResultHook = DestroyOnce;

	const FB2Counts Before = FB2Counts::Capture(Builder);
	const FBuildingConstructionResult Result = Builder.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellA));
	B2Expect(*this, TEXT("Destroyed by a build-result listener"), Result, EBuildResult::Success);
	B2ExpectDelta(*this, TEXT("Paid once"), Before, Builder, 1, 1, 1, -4);
	TestTrue(TEXT("One listener destroyed the piece"), bDestroyed);
	const bool bBothReceived = Builder.Listener->BuildResults.Num() == 1 && Second->BuildResults.Num() == 1;
	if (TestTrue(TEXT("Both listeners received one result"), bBothReceived))
	{
		TestTrue(TEXT("The first listener saw a valid piece, the second an invalid one"),
			Builder.Listener->PieceValidOnReceipt[0] != Second->PieceValidOnReceipt[0]);
		TestTrue(TEXT("Both saw Success with no diagnostic"), Builder.Listener->BuildResults[0].Result == EBuildResult::Success
			&& Second->BuildResults[0].Result == EBuildResult::Success && Builder.Listener->BuildResults[0].Diagnostic.IsEmpty()
			&& Second->BuildResults[0].Diagnostic.IsEmpty());
	}
	TestTrue(TEXT("The caller sees Success, an invalid piece and no diagnostic"), !Result.Piece.IsValid() && Result.Diagnostic.IsEmpty());
	Fixture.ExpectNoResidue(*this, TEXT("Piece destroyed during OnBuildResult"), 0, B2CellA);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2BuilderEndedDuringPaymentTest,
	"NullTide.Building.B2.BuilderEndedDuringPayment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2BuilderEndedDuringPaymentTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnFloorBuilder();
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	ACharacter* Pawn = Builder.Pawn;
	Builder.Listener->InventoryHook = [Pawn]()
	{
		if (!Pawn->IsActorBeingDestroyed())
		{
			Pawn->Destroy();
		}
	};
	const FB2Counts Before = FB2Counts::Capture(Builder);
	const FBuildingConstructionResult Result = Builder.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellA));
	B2Expect(*this, TEXT("Builder destroyed during payment"), Result, EBuildResult::Success);
	B2ExpectDelta(*this, TEXT("Paid once"), Before, Builder, 1, 1, 1, -4);
	TestTrue(TEXT("The builder is gone"), Pawn->IsActorBeingDestroyed());
	ABuildingPieceActor* Piece = Cast<ABuildingPieceActor>(Result.Piece.Get());
	TestTrue(TEXT("The paid piece stays Active and registered"), Piece && Piece->GetBuildingPieceComponent()->IsActivePiece()
		&& Fixture.Subsystem->GetOccupyingPiece(B2Key(B2CellA)) == Piece->GetBuildingPieceComponent());
	TestTrue(TEXT("OnBuildResult fired exactly once with the returned result"), Builder.Listener->BuildResults.Num() == 1
		&& Builder.Listener->BuildResults[0].Result == EBuildResult::Success && Builder.Listener->BuildResults[0].Piece == Result.Piece);
	TestFalse(TEXT("The guard was released"), Builder.Construction->IsBuildInProgress());
	TestEqual(TEXT("One piece"), Fixture.LivePieces(), 1);
	Fixture.Clean(*this, TEXT("Builder ended during payment"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2ActivationCallbackAltersPieceTest,
	"NullTide.Building.B2.ActivationCallbackAltersPiece",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2ActivationCallbackAltersPieceTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnFloorBuilder();
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	UBuildingSubsystem* Subsystem = Fixture.Subsystem;
	struct FCase { const TCHAR* Name; const TCHAR* Branch; TFunction<void(ABuildingPieceActor&)> Alter; };
	const FCase Cases[] = {
		{ TEXT("Moved"), TEXT("moved during activation"),
			[](ABuildingPieceActor& Piece) { Piece.SetActorLocation(Piece.GetActorLocation() + FVector(50.0, 0.0, 0.0)); } },
		{ TEXT("Hidden"), TEXT("not visible and collidable"), [](ABuildingPieceActor& Piece) { Piece.SetActorHiddenInGame(true); } },
		{ TEXT("Collision disabled"), TEXT("not visible and collidable"), [](ABuildingPieceActor& Piece) { Piece.SetActorEnableCollision(false); } },
		{ TEXT("Made provisional"), TEXT("not visible and collidable"), [](ABuildingPieceActor& Piece) { Piece.SetProvisional(true); } },
		{ TEXT("Unregistered"), TEXT("is not occupied by the piece"),
			[Subsystem](ABuildingPieceActor& Piece) { Subsystem->UnregisterPiece(Piece.GetBuildingPieceComponent()); } },
	};
	for (const FCase& Case : Cases)
	{
		Builder.Construction->AfterActivate = Case.Alter;
		const FB2Counts Before = FB2Counts::Capture(Builder);
		B2ExpectBranch(*this, Case.Name, Builder.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellA)), EBuildResult::BuildInterrupted, Case.Branch);
		B2ExpectDelta(*this, Case.Name, Before, Builder, 0, 0, 1, 0);
		Fixture.ExpectNoResidue(*this, Case.Name, 0, B2CellA);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2UnsupportedPieceClassTest,
	"NullTide.Building.B2.UnsupportedPieceClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2UnsupportedPieceClassTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	const TSubclassOf<UBuildingDefinition> Plain = UBuildingTestPlainActorDefinition::StaticClass();
	FString Diagnostic;
	TestTrue(TEXT("B1 accepts a plain actor with a piece component"),
		GetDefault<UBuildingTestPlainActorDefinition>()->ValidateBuilding(Diagnostic));
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { Plain }, 8);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	const FBuildingConstructionResult Preview = Builder.Construction->CanBuild(Plain, B2Floor(B2CellA, Plain));
	B2Expect(*this, TEXT("CanBuild"), Preview, EBuildResult::InvalidDefinition);
	const FB2Counts Before = FB2Counts::Capture(Builder);
	const FBuildingConstructionResult Result = Builder.Construction->TryBuild(Plain, B2Floor(B2CellA, Plain));
	B2Expect(*this, TEXT("TryBuild"), Result, EBuildResult::InvalidDefinition);
	TestTrue(TEXT("CanBuild and TryBuild agree, naming the class contract"), Preview.Diagnostic == Result.Diagnostic
		&& Result.Diagnostic.Contains(TEXT("ABuildingPieceActor")));
	B2ExpectDelta(*this, TEXT("Unsupported piece class"), Before, Builder, 0, 0, 1, 0);
	TestEqual(TEXT("Nothing spawned"), Builder.Construction->SpawnCount, 0);
	Fixture.ExpectNoResidue(*this, TEXT("Unsupported piece class"), 0, B2CellA);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2ReentrancyTest,
	"NullTide.Building.B2.Reentrancy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2ReentrancyTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnFloorBuilder();
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	UConstructionTestComponent* Construction = Builder.Construction;
	TArray<FBuildingConstructionResult> Nested;
	Builder.Listener->InventoryHook = [Construction, &Nested]()
	{
		Nested.Add(Construction->CanBuild(B2FloorClass(), B2Floor(B2CellB)));
		Nested.Add(Construction->TryBuild(B2FloorClass(), B2Floor(B2CellB)));
	};
	Builder.Listener->BuildResultHook = [Construction, &Nested](const FBuildingConstructionResult&)
	{
		Nested.Add(Construction->TryBuild(B2FloorClass(), B2Floor(B2CellB)));
	};
	const FB2Counts Before = FB2Counts::Capture(Builder);
	B2Expect(*this, TEXT("Outer build"), Builder.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellA)), EBuildResult::Success);
	B2ExpectDelta(*this, TEXT("Only the outer build changes or notifies"), Before, Builder, 1, 1, 1, -4);
	if (TestEqual(TEXT("Three nested requests"), Nested.Num(), 3))
	{
		for (int32 Index = 0; Index < Nested.Num(); ++Index)
		{
			B2Expect(*this, FString::Printf(TEXT("Nested request %d"), Index), Nested[Index], EBuildResult::BuildInProgress);
		}
	}
	TestEqual(TEXT("Exactly one piece"), Fixture.LivePieces(), 1);
	TestEqual(TEXT("Spawned once"), Builder.Construction->SpawnCount, 1);
	TestTrue(TEXT("The nested cell stays free"), Fixture.Subsystem->IsKeyFree(B2Key(B2CellB)));
	Fixture.Clean(*this, TEXT("Reentrancy"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2TwoBuildersTest,
	"NullTide.Building.B2.TwoBuilders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2TwoBuildersTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder A = Fixture.SpawnFloorBuilder();
	FB2Builder B = Fixture.SpawnBuilder(FVector(-200.0, 0.0, 92.0), { B2FloorClass() }, 8);
	if (!TestTrue(TEXT("Builders set up"), A.IsValid() && A.Inventory && B.IsValid() && B.Inventory)) { return false; }

	// While A holds its cell (reserved before activation, occupied after), B targets it and another cell.
	TArray<FBuildingConstructionResult> BResults;
	UConstructionTestComponent* BConstruction = B.Construction;
	A.Construction->BeforeActivate = [BConstruction, &BResults](ABuildingPieceActor&)
	{
		BResults.Add(BConstruction->TryBuild(B2FloorClass(), B2Floor(B2CellA)));
	};
	A.Construction->AfterActivate = [BConstruction, &BResults](ABuildingPieceActor&)
	{
		BResults.Add(BConstruction->TryBuild(B2FloorClass(), B2Floor(B2CellA)));
		BResults.Add(BConstruction->TryBuild(B2FloorClass(), B2Floor(B2CellB)));
	};
	const FB2Counts BeforeA = FB2Counts::Capture(A);
	const FB2Counts BeforeB = FB2Counts::Capture(B);
	B2Expect(*this, TEXT("A builds"), A.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellA)), EBuildResult::Success);
	if (TestEqual(TEXT("B made three requests"), BResults.Num(), 3))
	{
		B2Expect(*this, TEXT("B on A's reserved cell"), BResults[0], EBuildResult::Occupied);
		B2Expect(*this, TEXT("B on A's activated cell"), BResults[1], EBuildResult::Occupied);
		B2Expect(*this, TEXT("B on another cell"), BResults[2], EBuildResult::Success);
	}
	B2ExpectDelta(*this, TEXT("A paid once"), BeforeA, A, 1, 1, 1, -4);
	B2ExpectDelta(*this, TEXT("B paid once"), BeforeB, B, 1, 1, 3, -4);
	TestTrue(TEXT("Each cell holds its own builder's piece"), Fixture.PieceAt(B2CellA) && Fixture.PieceAt(B2CellA)->GetOwner() == A.Pawn
		&& Fixture.PieceAt(B2CellB) && Fixture.PieceAt(B2CellB)->GetOwner() == B.Pawn);
	TestEqual(TEXT("Two pieces"), Fixture.LivePieces(), 2);
	Fixture.Clean(*this, TEXT("Two builders"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2ProvisionalInvariantsTest,
	"NullTide.Building.B2.ProvisionalInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2ProvisionalInvariantsTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnFloorBuilder();
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	UBuildingSubsystem* Subsystem = Fixture.Subsystem;
	ACharacter* Pawn = Builder.Pawn;
	bool bChecked = false;
	Builder.Construction->BeforeActivate = [this, Subsystem, Pawn, &bChecked](ABuildingPieceActor& Piece)
	{
		bChecked = true;
		const UBuildingPieceComponent* PieceComponent = Piece.GetBuildingPieceComponent();
		TestTrue(TEXT("Provisional: hidden, no actor collision"), Piece.IsProvisional() && Piece.IsHidden() && !Piece.GetActorEnableCollision());
		TInlineComponentArray<UPrimitiveComponent*> Primitives(&Piece);
		bool bAnyNavigationRelevant = false;
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			bAnyNavigationRelevant |= Primitive && Primitive->IsNavigationRelevant();
		}
		TestFalse(TEXT("Provisional: no navigation-relevant primitive"), bAnyNavigationRelevant);
		TestTrue(TEXT("Provisional: state, builder and player-built flag"), PieceComponent->GetPieceState() == EBuildingPieceState::Provisional
			&& PieceComponent->IsPlayerBuilt() && Piece.GetOwner() == Pawn);
		TestTrue(TEXT("Provisional: unregistered, key reserved, no occupant"), !Subsystem->IsPieceRegistered(PieceComponent)
			&& Subsystem->GetReservedKeyCount() == 1 && !Subsystem->IsKeyFree(B2Key(B2CellA))
			&& Subsystem->GetOccupyingPiece(B2Key(B2CellA)) == nullptr);
	};
	const FBuildingConstructionResult Result = Builder.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellA));
	B2Expect(*this, TEXT("Build after the provisional checks"), Result, EBuildResult::Success);
	TestTrue(TEXT("The provisional state was inspected"), bChecked);
	if (const ABuildingPieceActor* Piece = Cast<ABuildingPieceActor>(Result.Piece.Get()))
	{
		TInlineComponentArray<UPrimitiveComponent*> Primitives(Piece);
		bool bAnyNavigationRelevant = false;
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			bAnyNavigationRelevant |= Primitive && Primitive->IsNavigationRelevant();
		}
		TestTrue(TEXT("Active: navigation influence restored"), bAnyNavigationRelevant);
	}
	Fixture.Clean(*this, TEXT("Provisional invariants"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2RetryAfterFailureTest,
	"NullTide.Building.B2.RetryAfterFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2RetryAfterFailureTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	const TSubclassOf<UBuildingDefinition> Drift = UBuildingTestDriftDefinition::StaticClass();
	const TSubclassOf<UBuildingDefinition> Spawner = UBuildingTestBlockerSpawnDefinition::StaticClass();
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { B2FloorClass(), Drift, Spawner }, 3);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	UConstructionTestComponent* Construction = Builder.Construction;
	UInventoryComponent* Inventory = Builder.Inventory;
	auto Attempt = [this, &Fixture, Construction](const TCHAR* What, EBuildResult Expected,
		TSubclassOf<UBuildingDefinition> Definition)
	{
		B2Expect(*this, What, Construction->TryBuild(Definition, B2Floor(B2CellA, Definition)), Expected);
		if (Expected != EBuildResult::Success)
		{
			Fixture.ExpectNoResidue(*this, What, 0, B2CellA);
		}
	};
	const FVector BlockerCenter = B2CellA + FVector(50.0, 50.0, 30.0);

	Attempt(TEXT("Short of Wood"), EBuildResult::InsufficientResources, B2FloorClass());
	Inventory->TryAddDefinitionQuantity(B2Wood(), 1);

	TStrongObjectPtr<UBuildingTestReservationOwner> Other(NewObject<UBuildingTestReservationOwner>(GetTransientPackage()));
	const FBuildingReservationHandle OtherReservation = Fixture.Subsystem->ReserveKeys(Other.Get(), { B2Key(B2CellA) });
	B2Expect(*this, TEXT("Occupied by another owner's reservation"), Construction->TryBuild(B2FloorClass(), B2Floor(B2CellA)), EBuildResult::Occupied);
	Fixture.Subsystem->ReleaseReservation(OtherReservation, Other.Get());
	Fixture.ExpectNoResidue(*this, TEXT("Occupied"), 0, B2CellA);

	AActor* Crate = B2SpawnBlocker(Fixture.World, BlockerCenter, FVector(15.0, 15.0, 30.0));
	Attempt(TEXT("Blocked at validation"), EBuildResult::Blocked, B2FloorClass());
	Crate->Destroy();

	Attempt(TEXT("Transform mismatch"), EBuildResult::TransformMismatch, Drift);

	Attempt(TEXT("Blocked at revalidation"), EBuildResult::Blocked, Spawner);
	for (AActor* Actor : FActorRange(Fixture.World))
	{
		// The blocker the spawning fixture left in the cell.
		if (Actor && !Actor->IsA<ABuildingPieceActor>() && Actor->GetActorLocation().Equals(BlockerCenter, 1.0))
		{
			Actor->Destroy();
		}
	}

	Construction->bSpawnReturnsNull = true;
	Attempt(TEXT("Spawn failed"), EBuildResult::SpawnFailed, B2FloorClass());
	Construction->bSpawnReturnsNull = false;

	Construction->AfterActivate = [](ABuildingPieceActor& Piece) { Piece.Destroy(); };
	Attempt(TEXT("Interrupted"), EBuildResult::BuildInterrupted, B2FloorClass());

	Construction->AfterActivate = [Inventory](ABuildingPieceActor&) { Inventory->TryApplyDefinitionTransaction({ B2Cost(B2Wood(), 1) }, {}); };
	Attempt(TEXT("Rejected at payment"), EBuildResult::ResourcesRejectedRolledBack, B2FloorClass());
	Construction->AfterActivate = nullptr;
	Inventory->TryAddDefinitionQuantity(B2Wood(), 1);

	const FB2Counts Before = FB2Counts::Capture(Builder);
	Attempt(TEXT("Retry succeeds"), EBuildResult::Success, B2FloorClass());
	B2ExpectDelta(*this, TEXT("No residue from earlier failures"), Before, Builder, 1, 1, 1, -4);
	TestEqual(TEXT("One piece"), Fixture.LivePieces(), 1);
	Fixture.Clean(*this, TEXT("Retry after failure"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB2ExactlyOnceTest,
	"NullTide.Building.B2.ExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB2ExactlyOnceTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnFloorBuilder(8);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	UConstructionTestComponent* Construction = Builder.Construction;
	const FB2Counts Before = FB2Counts::Capture(Builder);
	TArray<FBuildingConstructionResult> Returned;
	auto Build = [&Returned, Construction](TSubclassOf<UBuildingDefinition> Definition, const FVector& Cell)
	{
		Returned.Add(Construction->TryBuild(Definition, B2Floor(Cell)));
	};

	Build(B2FloorClass(), B2CellA);
	Build(B2FloorClass(), B2CellA);
	Build(UBuildingTestWideFloorDefinition::StaticClass(), B2CellC);
	FBuildingConstructionResult Nested;
	Builder.Listener->BuildResultHook = [Construction, &Nested](const FBuildingConstructionResult&)
	{
		Nested = Construction->TryBuild(B2FloorClass(), B2Floor(B2CellC));
	};
	Build(B2FloorClass(), B2CellB);
	Builder.Listener->BuildResultHook = nullptr;
	Build(B2FloorClass(), B2CellC);

	const EBuildResult Expected[] = { EBuildResult::Success, EBuildResult::Occupied, EBuildResult::NotAvailable, EBuildResult::Success,
		EBuildResult::InsufficientResources };
	if (!TestEqual(TEXT("Five requests"), Returned.Num(), 5)) { return false; }
	for (int32 Index = 0; Index < Returned.Num(); ++Index)
	{
		B2Expect(*this, FString::Printf(TEXT("Request %d"), Index), Returned[Index], Expected[Index]);
	}
	B2Expect(*this, TEXT("The nested request"), Nested, EBuildResult::BuildInProgress);
	B2ExpectDelta(*this, TEXT("One result per non-nested request; one revision per success"), Before, Builder, 2, 2, 5, -8);
	const TArray<FBuildingConstructionResult>& Notified = Builder.Listener->BuildResults;
	bool bSameOrder = Notified.Num() == Returned.Num();
	for (int32 Index = 0; bSameOrder && Index < Returned.Num(); ++Index)
	{
		bSameOrder = Notified[Notified.Num() - Returned.Num() + Index].Result == Returned[Index].Result;
	}
	TestTrue(TEXT("Each notification matches its returned result, in order"), bSameOrder);
	TestEqual(TEXT("Two pieces"), Fixture.LivePieces(), 2);
	Fixture.Clean(*this, TEXT("Exactly once"));
	return true;
}

// ---- B3: production Wooden Floor ----

namespace
{
const TCHAR* B3FloorDefinitionPath = TEXT("/Game/LevelPrototyping/BuildingSystem/Definitions/Building_WoodFloor.Building_WoodFloor_C");
const TCHAR* B3FloorPiecePath = TEXT("/Game/LevelPrototyping/BuildingSystem/Pieces/BP_BuildFloor.BP_BuildFloor_C");
const TCHAR* B3WoodPath = TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Wood.Item_Wood_C");
const TCHAR* B3PlayerPath = TEXT("/Game/TopDown/Blueprints/BP_TopDownCharacter.BP_TopDownCharacter");

struct FB3Assets
{
	TSubclassOf<UBuildingDefinition> Floor;
	TSubclassOf<ABuildingPieceActor> Piece;
	TSubclassOf<UItemDefinition> Wood;

	static FB3Assets Load()
	{
		FB3Assets Assets;
		Assets.Floor = LoadClass<UBuildingDefinition>(nullptr, B3FloorDefinitionPath);
		Assets.Piece = LoadClass<ABuildingPieceActor>(nullptr, B3FloorPiecePath);
		Assets.Wood = LoadClass<UItemDefinition>(nullptr, B3WoodPath);
		return Assets;
	}

	bool IsValid(FAutomationTestBase& Test) const
	{
		return Test.TestTrue(TEXT("Building_WoodFloor, BP_BuildFloor and Item_Wood load"), Floor && Piece && Wood);
	}
};

bool B3AnyNavigationRelevant(const AActor& Actor)
{
	TInlineComponentArray<UPrimitiveComponent*> Primitives(&Actor);
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive && Primitive->IsNavigationRelevant())
		{
			return true;
		}
	}
	return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB3ProductionDefinitionTest,
	"NullTide.Building.B3.ProductionDefinition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB3ProductionDefinitionTest::RunTest(const FString& Parameters)
{
	const FB3Assets Assets = FB3Assets::Load();
	if (!Assets.IsValid(*this)) { return false; }
	const UBuildingDefinition* Floor = Assets.Floor->GetDefaultObject<UBuildingDefinition>();
	FString Diagnostic;
	TestTrue(FString::Printf(TEXT("Building_WoodFloor is valid (%s)"), *Diagnostic), Floor->ValidateBuilding(Diagnostic));
	TestEqual(TEXT("Name"), Floor->BuildingName.ToString(), FString(TEXT("Wooden Floor")));
	TestTrue(TEXT("Foundation on grid cells, 1 x 1, 90 degree steps"), Floor->Category == EBuildingCategory::Foundation
		&& Floor->PlacementMode == EBuildingPlacementMode::GridCell && Floor->Footprint == FIntPoint(1, 1) && Floor->RotationStep == 90);
	TestTrue(TEXT("Cost is exactly Item_Wood x 4"), Floor->Cost.Num() == 1 && Floor->Cost[0].DefinitionClass == Assets.Wood
		&& Floor->Cost[0].Quantity == 4);
	TestTrue(TEXT("50% refund, default range"), FMath::IsNearlyEqual(Floor->RefundFraction, 0.5f) && Floor->BuildRangeOverride == 0.f);
	TestTrue(TEXT("The piece is BP_BuildFloor"), Floor->PieceActorClass == Assets.Piece);
	TestNull(TEXT("No icon until the B4 HUD (B3-3)"), Floor->BuildingIcon.Get());
#if WITH_EDITOR
	FDataValidationContext Context;
	TestTrue(TEXT("Editor data validation passes"), Floor->IsDataValid(Context) != EDataValidationResult::Invalid);
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB3ProductionPieceContractTest,
	"NullTide.Building.B3.ProductionPieceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB3ProductionPieceContractTest::RunTest(const FString& Parameters)
{
	const FB3Assets Assets = FB3Assets::Load();
	if (!Assets.IsValid(*this)) { return false; }
	TestTrue(TEXT("BP_BuildFloor derives from ABuildingPieceActor"), Assets.Piece->IsChildOf(ABuildingPieceActor::StaticClass()));

	// The Blueprint only sets the inherited native mesh: no added components (so no child actors or extra authorities).
	int32 AddedComponents = 0;
	if (const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Assets.Piece.Get()))
	{
		if (const USimpleConstructionScript* ConstructionScript = Generated->SimpleConstructionScript)
		{
			AddedComponents = ConstructionScript->GetAllNodes().Num();
		}
	}
	TestEqual(TEXT("BP_BuildFloor adds no components"), AddedComponents, 0);

	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABuildingPieceActor* Piece = Fixture.World->SpawnActor<ABuildingPieceActor>(Assets.Piece, FVector(300.0, 300.0, 0.0), FRotator::ZeroRotator, Params);
	if (!TestNotNull(TEXT("BP_BuildFloor spawns"), Piece)) { return false; }
	TestTrue(TEXT("Unscaled root"), Piece->GetRootComponent() && Piece->GetRootComponent()->GetRelativeScale3D().Equals(FVector::OneVector));
	TInlineComponentArray<UChildActorComponent*> ChildActors(Piece);
	TestEqual(TEXT("No child actor components"), ChildActors.Num(), 0);
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Piece);
	bool bCustomNavigationEvenIfNotCollidable = false;
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		bCustomNavigationEvenIfNotCollidable |= Primitive && Primitive->HasCustomNavigableGeometry() == EHasCustomNavigableGeometry::EvenIfNotCollidable;
	}
	TestFalse(TEXT("No custom navigable geometry marked EvenIfNotCollidable"), bCustomNavigationEvenIfNotCollidable);

	TInlineComponentArray<UStaticMeshComponent*> Meshes(Piece);
	if (!TestEqual(TEXT("One mesh (the inherited PieceMesh)"), Meshes.Num(), 1)) { return false; }
	const UStaticMeshComponent* Mesh = Meshes[0];
	TestTrue(TEXT("Engine cube placeholder"), Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetPathName() == TEXT("/Engine/BasicShapes/Cube.Cube"));
	TestTrue(TEXT("The floor blocks pawns"), Mesh->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block
		&& Mesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
	const FBox Bounds = Mesh->Bounds.GetBox();
	TestTrue(FString::Printf(TEXT("200 x 200 x 10 cm with its top at 10 cm (got %s)"), *Bounds.ToString()),
		Bounds.Min.Equals(FVector(200.0, 200.0, 0.0), 0.5) && Bounds.Max.Equals(FVector(400.0, 400.0, UBuildingSubsystem::FloorThickness), 0.5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB3PlayerRegistrationTest,
	"NullTide.Building.B3.PlayerRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB3PlayerRegistrationTest::RunTest(const FString& Parameters)
{
	const FB3Assets Assets = FB3Assets::Load();
	if (!Assets.IsValid(*this)) { return false; }
	UBlueprint* PlayerBlueprint = LoadObject<UBlueprint>(nullptr, B3PlayerPath);
	if (!TestNotNull(TEXT("Production player Blueprint loads"), PlayerBlueprint)) { return false; }
	TArray<const UConstructionComponent*> Construction;
	int32 Crafting = 0;
	int32 ToolLoadout = 0;
	if (const USimpleConstructionScript* ConstructionScript = PlayerBlueprint->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : ConstructionScript->GetAllNodes())
		{
			if (const UConstructionComponent* Template = Node ? Cast<UConstructionComponent>(Node->ComponentTemplate) : nullptr)
			{
				Construction.Add(Template);
			}
			Crafting += Node && Cast<UCraftingComponent>(Node->ComponentTemplate) ? 1 : 0;
			ToolLoadout += Node && Node->ComponentClass && Node->ComponentClass->IsChildOf<UToolLoadoutComponent>() ? 1 : 0;
		}
	}
	if (!TestEqual(TEXT("Production player has exactly one ConstructionComponent"), Construction.Num(), 1)) { return false; }
	TestTrue(TEXT("Registered buildings are [Building_WoodFloor]"), Construction[0]->GetAvailableBuildings() == FB2Buildings({ Assets.Floor }));
	TestTrue(TEXT("The crafting and tool-loadout components are unchanged (one each)"), Crafting == 1 && ToolLoadout == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB3ProductionFloorEndToEndTest,
	"NullTide.Building.B3.ProductionFloorEndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB3ProductionFloorEndToEndTest::RunTest(const FString& Parameters)
{
	const FB3Assets Assets = FB3Assets::Load();
	if (!Assets.IsValid(*this)) { return false; }
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { Assets.Floor }, 0);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	TestTrue(TEXT("8 production Wood"), Builder.Inventory->TryAddDefinitionQuantity(Assets.Wood, 8).IsSuccess());
	auto Floor = [&Assets](const FVector& Cell)
	{
		return UBuildingSubsystem::MakePlacementFromWorld(*Assets.Floor->GetDefaultObject<UBuildingDefinition>(), Cell, 0);
	};

	// The production piece honours the provisional contract at runtime (covers anything its Blueprint adds).
	bool bChecked = false;
	const TSubclassOf<ABuildingPieceActor> PieceClass = Assets.Piece;
	Builder.Construction->BeforeActivate = [this, &bChecked, PieceClass](ABuildingPieceActor& Piece)
	{
		bChecked = true;
		TestTrue(TEXT("Provisional production piece: hidden, no actor collision, no navigation influence"), Piece.IsA(PieceClass)
			&& Piece.IsHidden() && !Piece.GetActorEnableCollision() && !B3AnyNavigationRelevant(Piece));
	};

	// Measured from here: adding the production Wood above already raised one inventory event.
	const int32 WoodBefore = Builder.Inventory->GetTotalQuantity(Assets.Wood);
	const int32 RevisionBefore = Builder.Inventory->GetRevision();
	const int32 EventsBefore = Builder.Listener->InventoryEvents;
	const int32 ResultsBefore = Builder.Listener->BuildResults.Num();
	const FBuildingConstructionResult Built = Builder.Construction->TryBuild(Assets.Floor, Floor(B2CellA));
	if (!B2Expect(*this, TEXT("Build the production floor"), Built, EBuildResult::Success)) { return false; }
	Builder.Construction->BeforeActivate = nullptr;
	TestTrue(TEXT("The provisional state was inspected"), bChecked);
	TestTrue(FString::Printf(TEXT("4 Wood consumed, one revision, one event, one result (wood %d -> %d, revision %+d, events %+d, results %+d)"),
		WoodBefore, Builder.Inventory->GetTotalQuantity(Assets.Wood), Builder.Inventory->GetRevision() - RevisionBefore,
		Builder.Listener->InventoryEvents - EventsBefore, Builder.Listener->BuildResults.Num() - ResultsBefore),
		Builder.Inventory->GetTotalQuantity(Assets.Wood) == WoodBefore - 4 && Builder.Inventory->GetRevision() == RevisionBefore + 1
		&& Builder.Listener->InventoryEvents == EventsBefore + 1 && Builder.Listener->BuildResults.Num() == ResultsBefore + 1);
	const ABuildingPieceActor* Piece = Cast<ABuildingPieceActor>(Built.Piece.Get());
	TestTrue(TEXT("A BP_BuildFloor instance, Active, registered, visible, collidable and navigation-relevant"), Piece && Piece->IsA(Assets.Piece)
		&& Piece->GetBuildingPieceComponent()->IsActivePiece() && Fixture.Subsystem->GetOccupyingPiece(B2Key(B2CellA)) == Piece->GetBuildingPieceComponent()
		&& !Piece->IsHidden() && Piece->GetActorEnableCollision() && B3AnyNavigationRelevant(*Piece));

	// The floor supports pieces on its edge at its top.
	const FBuildingPlacement WallOnEdge = UBuildingSubsystem::MakePlacementFromWorld(*GetDefault<UBuildingTestWallDefinition>(),
		FVector(300.0, 205.0, 0.0), 0);
	const FBuildingPlacementResult Wall = Fixture.Subsystem->ValidatePlacement(UBuildingTestWallDefinition::StaticClass(), WallOnEdge, Builder.Pawn);
	TestTrue(FString::Printf(TEXT("A wall on the floor's edge stands on its top (%s)"), *Wall.Diagnostic), Wall.IsSuccess()
		&& FMath::IsNearlyEqual(Wall.Placement.SurfaceZ, static_cast<float>(UBuildingSubsystem::FloorThickness), 0.1f) && Wall.Placement.Layer == 0);

	B2Expect(*this, TEXT("Second floor on the same cell"), Builder.Construction->TryBuild(Assets.Floor, Floor(B2CellA)), EBuildResult::Occupied);
	B2Expect(*this, TEXT("Second floor elsewhere"), Builder.Construction->TryBuild(Assets.Floor, Floor(B2CellB)), EBuildResult::Success);
	B2Expect(*this, TEXT("Third floor without Wood"), Builder.Construction->TryBuild(Assets.Floor, Floor(B2CellC)), EBuildResult::InsufficientResources);
	TestEqual(TEXT("All Wood spent"), Builder.Inventory->GetTotalQuantity(Assets.Wood), 0);
	TestEqual(TEXT("Two floors"), Fixture.LivePieces(), 2);
	Fixture.Clean(*this, TEXT("Production floor"));
	return true;
}

// ---- B4: building mode, ghost preview and placement input ----

namespace
{
const TCHAR* B4HudPath = TEXT("/Game/LevelPrototyping/BuildingSystem/UI/W_BuildingHud.W_BuildingHud_C");

struct FB4Player
{
	FB2Builder Builder;
	UBuildingPlacementTestComponent* Placement = nullptr;
	APlayerController* Controller = nullptr;

	bool IsValid() const { return Builder.IsValid() && Placement && Controller; }
};

APlayerController* B4Possess(FB2World& Fixture, APawn* Pawn)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>(APlayerController::StaticClass(), FTransform::Identity, Params);
	if (Controller && Pawn)
	{
		Controller->Possess(Pawn);
	}
	return Controller;
}

/** A builder (inventory, construction) with a placement component; registered after BeginPlay, so its BeginPlay runs. */
FB4Player B4SpawnPlayer(FB2World& Fixture, const FB2Buildings& Buildings, int32 Wood, bool bPossess = true)
{
	FB4Player Player;
	Player.Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), Buildings, Wood);
	if (!Player.Builder.Pawn)
	{
		return Player;
	}
	Player.Placement = NewObject<UBuildingPlacementTestComponent>(Player.Builder.Pawn);
	Player.Builder.Pawn->AddInstanceComponent(Player.Placement);
	Player.Placement->RegisterComponent();
	Player.Placement->OnBuildingModeChanged.AddDynamic(Player.Builder.Listener, &UConstructionTestListener::HandleBuildingModeChanged);
	if (bPossess)
	{
		Player.Controller = B4Possess(Fixture, Player.Builder.Pawn);
	}
	return Player;
}

int32 B4CountPreviews(UWorld* World)
{
	int32 Count = 0;
	for (TActorIterator<ABuildingPreviewActor> It(World); It; ++It)
	{
		Count += ::IsValid(*It) && !It->IsActorBeingDestroyed() ? 1 : 0;
	}
	return Count;
}

void B4Tick(UBuildingPlacementComponent* Placement)
{
	Placement->TickComponent(0.f, LEVELTICK_All, nullptr);
}

template <typename T>
T* B4ReadObject(const UObject* Object, const TCHAR* PropertyName)
{
	const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(UBuildingPlacementComponent::StaticClass(), PropertyName);
	return Property ? Cast<T>(Property->GetObjectPropertyValue_InContainer(Object)) : nullptr;
}

int32 B4ReadInt(const UObject* Object, const TCHAR* PropertyName)
{
	const FIntProperty* Property = FindFProperty<FIntProperty>(UBuildingPlacementComponent::StaticClass(), PropertyName);
	return Property ? Property->GetPropertyValue_InContainer(Object) : INDEX_NONE;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB4ModeStateMachineTest,
	"NullTide.Building.B4.ModeStateMachine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB4ModeStateMachineTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB4Player Player = B4SpawnPlayer(Fixture, { B2FloorClass() }, 8);
	if (!TestTrue(TEXT("Player set up"), Player.IsValid())) { return false; }
	UBuildingPlacementTestComponent* Placement = Player.Placement;
	const TArray<bool>& Changes = Player.Builder.Listener->ModeChanges;
	Placement->SetCursor(B2CellA);

	TestFalse(TEXT("Idle: not active, not ticking"), Placement->IsBuildingModeActive() || Placement->IsComponentTickEnabled());
	TestTrue(TEXT("Enter"), Placement->EnterBuildingMode());
	TestTrue(TEXT("Active: ticking, one preview, the first registered building selected"), Placement->IsBuildingModeActive()
		&& Placement->IsComponentTickEnabled() && B4CountPreviews(Fixture.World) == 1 && Placement->GetPreviewActor()
		&& Placement->GetSelectedBuilding() == B2FloorClass());
	TestTrue(TEXT("Enter again is a no-op"), Placement->EnterBuildingMode() && B4CountPreviews(Fixture.World) == 1 && Changes.Num() == 1);
	Placement->ExitBuildingMode();
	TestTrue(TEXT("Exit: inactive, not ticking, no preview"), !Placement->IsBuildingModeActive() && !Placement->IsComponentTickEnabled()
		&& B4CountPreviews(Fixture.World) == 0 && !Placement->GetPreviewActor());
	Placement->ExitBuildingMode();
	Placement->ToggleBuildingMode();
	TestTrue(TEXT("Toggle enters"), Placement->IsBuildingModeActive());
	Placement->ToggleBuildingMode();
	TestTrue(TEXT("Toggle exits"), !Placement->IsBuildingModeActive());
	TestTrue(TEXT("OnBuildingModeChanged fires once per real change"), Changes == TArray<bool>({ true, false, true, false }));

	FB4Player NoBuildings = B4SpawnPlayer(Fixture, {}, 0);
	TestTrue(TEXT("No registered building: refused"), NoBuildings.IsValid() && !NoBuildings.Placement->EnterBuildingMode()
		&& NoBuildings.Builder.Listener->ModeChanges.IsEmpty());
	FB4Player Unpossessed = B4SpawnPlayer(Fixture, { B2FloorClass() }, 8, false);
	TestTrue(TEXT("No controller: refused"), Unpossessed.Placement && !Unpossessed.Placement->EnterBuildingMode());
	FB4Player Remote = B4SpawnPlayer(Fixture, { B2FloorClass() }, 8);
	Remote.Placement->bTreatControllerAsLocal = false;
	TestTrue(TEXT("Non-local controller: refused"), Remote.IsValid() && !Remote.Placement->EnterBuildingMode()
		&& !Remote.Controller->bShowMouseCursor);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Bare = Fixture.World->SpawnActor<ACharacter>(ACharacter::StaticClass(), FVector(0.0, 300.0, 92.0), FRotator::ZeroRotator, Params);
	UBuildingPlacementTestComponent* BarePlacement = NewObject<UBuildingPlacementTestComponent>(Bare);
	Bare->AddInstanceComponent(BarePlacement);
	BarePlacement->RegisterComponent();
	Bare->DispatchBeginPlay();
	B4Possess(Fixture, Bare);
	TestFalse(TEXT("No construction component: refused"), BarePlacement->EnterBuildingMode());
	TestEqual(TEXT("No preview left"), B4CountPreviews(Fixture.World), 0);
	Fixture.Clean(*this, TEXT("Mode state machine"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB4CursorStateTest,
	"NullTide.Building.B4.CursorState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB4CursorStateTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB4Player Player = B4SpawnPlayer(Fixture, { B2FloorClass() }, 8);
	if (!TestTrue(TEXT("Player set up"), Player.IsValid())) { return false; }
	UBuildingPlacementTestComponent* Placement = Player.Placement;
	APlayerController* Controller = Player.Controller;
	Placement->SetCursor(B2CellA);

	Controller->SetShowMouseCursor(false);
	Placement->EnterBuildingMode();
	TestTrue(TEXT("Enter shows the cursor"), Controller->bShowMouseCursor);
	Placement->ExitBuildingMode();
	TestFalse(TEXT("Exit restores a hidden cursor"), Controller->bShowMouseCursor);

	Controller->SetShowMouseCursor(true);
	Placement->EnterBuildingMode();
	Placement->ExitBuildingMode();
	TestTrue(TEXT("Exit restores a shown cursor"), Controller->bShowMouseCursor);

	Controller->SetShowMouseCursor(false);
	Placement->EnterBuildingMode();
	Placement->ToggleBuildingMode();
	TestFalse(TEXT("Toggle exit restores the cursor"), Controller->bShowMouseCursor);

	Placement->EnterBuildingMode();
	Controller->UnPossess();
	TestTrue(TEXT("Controller change exits and restores the cursor"), !Placement->IsBuildingModeActive() && !Controller->bShowMouseCursor
		&& B4CountPreviews(Fixture.World) == 0);

	Controller->Possess(Player.Builder.Pawn);
	TestTrue(TEXT("Re-possessed: building mode works again"), Placement->EnterBuildingMode() && Controller->bShowMouseCursor);
	Placement->DestroyComponent();
	TestTrue(TEXT("Component EndPlay (still possessed) exits and restores the cursor"), !Controller->bShowMouseCursor
		&& B4CountPreviews(Fixture.World) == 0);
	Controller->UnPossess();
	TestFalse(TEXT("A later controller change after the component is gone is harmless"), Controller->bShowMouseCursor);
	Fixture.Clean(*this, TEXT("Cursor state"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB4PreviewFollowsCursorTest,
	"NullTide.Building.B4.PreviewFollowsCursor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB4PreviewFollowsCursorTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB4Player Player = B4SpawnPlayer(Fixture, { B2FloorClass() }, 8);
	if (!TestTrue(TEXT("Player set up"), Player.IsValid())) { return false; }
	UBuildingPlacementTestComponent* Placement = Player.Placement;
	const FB2Counts Before = FB2Counts::Capture(Player.Builder);
	Placement->SetCursor(B2CellA + FVector(40.0, -60.0, 0.0));
	if (!TestTrue(TEXT("Enter"), Placement->EnterBuildingMode())) { return false; }

	ABuildingPreviewActor* Preview = Placement->GetPreviewActor();
	if (!TestNotNull(TEXT("Preview"), Preview)) { return false; }
	TestTrue(TEXT("Snapped to the cell, green"), Preview->GetActorLocation().Equals(FVector(300.0, 300.0, 0.0), 0.1)
		&& Preview->IsPlacementValid() && Placement->GetPreviewResult().IsSuccess() && !Preview->IsHidden());
	TestEqual(TEXT("One preview mesh copied from the piece"), Preview->GetPreviewMeshCount(), 1);
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Preview);
	bool bAnyCollision = false;
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		bAnyCollision |= Primitive && Primitive->IsCollisionEnabled();
	}
	TestTrue(TEXT("The preview never collides, affects navigation or acts as a piece"), !bAnyCollision && !B3AnyNavigationRelevant(*Preview)
		&& !Preview->FindComponentByClass<UBuildingPieceComponent>());

	Placement->SetCursor(FVector(900.0, 500.0, 0.0));
	B4Tick(Placement);
	TestTrue(TEXT("Far: red, OutOfRange"), !Preview->IsPlacementValid() && Placement->GetPreviewResult().Result == EBuildResult::OutOfRange);

	B2Expect(*this, TEXT("A piece on cell B"), Player.Builder.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellB)), EBuildResult::Success);
	Placement->SetCursor(B2CellB);
	B4Tick(Placement);
	TestTrue(TEXT("Occupied: red"), !Preview->IsPlacementValid() && Placement->GetPreviewResult().Result == EBuildResult::Occupied);

	Player.Builder.Inventory->TryApplyDefinitionTransaction({ B2Cost(B2Wood(), 4) }, {});
	Placement->SetCursor(B2CellC);
	B4Tick(Placement);
	TestTrue(TEXT("No Wood: red, InsufficientResources"), !Preview->IsPlacementValid()
		&& Placement->GetPreviewResult().Result == EBuildResult::InsufficientResources);

	Placement->bHasCursor = false;
	B4Tick(Placement);
	TestTrue(TEXT("Nothing under the cursor: the preview hides"), Preview->IsHidden());

	// Only the explicit build and Wood removal above changed anything; the preview itself never does.
	B2ExpectDelta(*this, TEXT("Preview is read-only"), Before, Player.Builder, 2, 2, 1, -8);
	TestEqual(TEXT("Only the explicit piece exists"), Fixture.LivePieces(), 1);
	Placement->ExitBuildingMode();
	Fixture.Clean(*this, TEXT("Preview follows cursor"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB4ConfirmBuildsOnceTest,
	"NullTide.Building.B4.ConfirmBuildsOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB4ConfirmBuildsOnceTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB4Player Player = B4SpawnPlayer(Fixture, { B2FloorClass() }, 8);
	if (!TestTrue(TEXT("Player set up"), Player.IsValid())) { return false; }
	UBuildingPlacementTestComponent* Placement = Player.Placement;
	Placement->SetCursor(B2CellA);
	Placement->EnterBuildingMode();

	FB2Counts Before = FB2Counts::Capture(Player.Builder);
	B2Expect(*this, TEXT("Confirm builds"), Placement->ConfirmPlacement(), EBuildResult::Success);
	B2ExpectDelta(*this, TEXT("One build, exact cost"), Before, Player.Builder, 1, 1, 1, -4);
	TestTrue(TEXT("One piece; the mode stays active"), Fixture.LivePieces() == 1 && Placement->IsBuildingModeActive());
	B4Tick(Placement);
	TestEqual(TEXT("The ghost refreshes to Occupied after the build"), Placement->GetPreviewResult().Result, EBuildResult::Occupied);

	Before = FB2Counts::Capture(Player.Builder);
	B2Expect(*this, TEXT("Confirm on the same cell"), Placement->ConfirmPlacement(), EBuildResult::Occupied);
	B2ExpectDelta(*this, TEXT("Refused confirm"), Before, Player.Builder, 0, 0, 1, 0);
	TestEqual(TEXT("Still one piece"), Fixture.LivePieces(), 1);

	// Cursor over nothing: no target, so no build at the last (now hidden) placement.
	Placement->SetCursor(B2CellB);
	B4Tick(Placement);
	Placement->bHasCursor = false;
	B4Tick(Placement);
	Before = FB2Counts::Capture(Player.Builder);
	TestFalse(TEXT("Confirm with nothing under the cursor fails"), Placement->ConfirmPlacement().IsSuccess());
	B2ExpectDelta(*this, TEXT("No build without a cursor target"), Before, Player.Builder, 0, 0, 0, 0);
	TestEqual(TEXT("Still one piece after the no-target confirm"), Fixture.LivePieces(), 1);

	Placement->ExitBuildingMode();
	Before = FB2Counts::Capture(Player.Builder);
	TestFalse(TEXT("Confirm outside building mode fails"), Placement->ConfirmPlacement().IsSuccess());
	B2ExpectDelta(*this, TEXT("Confirm outside building mode does nothing"), Before, Player.Builder, 0, 0, 0, 0);
	Fixture.Clean(*this, TEXT("Confirm builds once"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB4ExitPathsTest,
	"NullTide.Building.B4.ExitPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB4ExitPathsTest::RunTest(const FString& Parameters)
{
	UClass* HudClass = LoadClass<UBuildingHudWidget>(nullptr, B4HudPath);
	if (!TestNotNull(TEXT("W_BuildingHud loads"), HudClass)) { return false; }
	enum class EExit { Toggle, Exit, ControllerChange, PawnDestroyed, ComponentEndPlay };
	struct FCase { EExit Kind; const TCHAR* Name; };
	const FCase Cases[] = {
		{ EExit::Toggle, TEXT("Toggle (B)") },
		{ EExit::Exit, TEXT("Exit (RMB, I, Tab, F)") },
		{ EExit::ControllerChange, TEXT("Controller change") },
		// The pawn is unpossessed before its EndPlay, so this goes through the controller-change path.
		{ EExit::PawnDestroyed, TEXT("Pawn destroyed") },
		// The component's own EndPlay while the pawn stays possessed.
		{ EExit::ComponentEndPlay, TEXT("Component EndPlay") },
	};
	for (const FCase& Case : Cases)
	{
		FB2World Fixture;
		if (!Fixture.IsValid(*this)) { return false; }
		FB4Player Player = B4SpawnPlayer(Fixture, { B2FloorClass() }, 8);
		if (!TestTrue(TEXT("Player set up"), Player.IsValid())) { return false; }
		UBuildingPlacementTestComponent* Placement = Player.Placement;
		Placement->SetHudClass(HudClass);
		Placement->SetCursor(B2CellA);
		Player.Controller->SetShowMouseCursor(false);
		Placement->EnterBuildingMode();
		TestTrue(FString(Case.Name) + TEXT(": HUD shown in building mode"), Placement->GetHudWidget() != nullptr && Placement->HudCreateCount == 1);
		B2Expect(*this, Case.Name, Placement->ConfirmPlacement(), EBuildResult::Success);
		switch (Case.Kind)
		{
		case EExit::Toggle:
			Placement->ToggleBuildingMode();
			break;
		case EExit::Exit:
			Placement->ExitBuildingMode();
			break;
		case EExit::ControllerChange:
			Player.Controller->UnPossess();
			break;
		case EExit::PawnDestroyed:
			Player.Builder.Pawn->Destroy();
			break;
		case EExit::ComponentEndPlay:
			Placement->DestroyComponent();
			break;
		}
		TestTrue(FString(Case.Name) + TEXT(": inactive, no ghost, no HUD, cursor restored"), !Placement->IsBuildingModeActive()
			&& B4CountPreviews(Fixture.World) == 0 && !Placement->GetPreviewActor() && !Placement->GetHudWidget()
			&& !Player.Controller->bShowMouseCursor);
		if (Case.Kind != EExit::PawnDestroyed && Case.Kind != EExit::ComponentEndPlay)
		{
			TestFalse(FString(Case.Name) + TEXT(": not ticking"), Placement->IsComponentTickEnabled());
		}
		const ABuildingPieceActor* Piece = Fixture.PieceAt(B2CellA);
		TestTrue(FString(Case.Name) + TEXT(": the finished build is unaffected"), Fixture.LivePieces() == 1 && Piece
			&& Piece->GetBuildingPieceComponent()->IsActivePiece());
		Fixture.Clean(*this, Case.Name);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB4HudContentTest,
	"NullTide.Building.B4.HudContent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB4HudContentTest::RunTest(const FString& Parameters)
{
	const FB3Assets Assets = FB3Assets::Load();
	UClass* HudClass = LoadClass<UBuildingHudWidget>(nullptr, B4HudPath);
	if (!Assets.IsValid(*this) || !TestNotNull(TEXT("W_BuildingHud loads"), HudClass)) { return false; }
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB4Player Player = B4SpawnPlayer(Fixture, { Assets.Floor }, 0);
	if (!TestTrue(TEXT("Player set up"), Player.IsValid())) { return false; }
	Player.Builder.Inventory->TryAddDefinitionQuantity(Assets.Wood, 8);
	UBuildingPlacementTestComponent* Placement = Player.Placement;
	Placement->SetHudClass(HudClass);
	Placement->SetCursor(B2CellA);
	Placement->EnterBuildingMode();
	const UBuildingHudWidget* Hud = Placement->GetHudWidget();
	if (!TestNotNull(TEXT("The real W_BuildingHud is created"), Hud)) { return false; }

	TestEqual(TEXT("Building name"), Hud->GetBuildingNameText().ToString(), FString(TEXT("Wooden Floor")));
	TestTrue(FString::Printf(TEXT("Cost shows have/need (%s)"), *Hud->GetCostText().ToString()), Hud->GetCostText().ToString().Contains(TEXT("8/4")));
	TestTrue(TEXT("Ready status in the valid color"), Hud->GetStatusText().ToString() == TEXT("Ready to build") && Hud->IsStatusValid());

	Placement->SetCursor(FVector(900.0, 500.0, 0.0));
	Placement->UpdatePreviewAt(FVector(900.0, 500.0, 0.0), true);
	TestTrue(TEXT("Reason for a red ghost"), Hud->GetStatusText().ToString() == TEXT("Too far") && !Hud->IsStatusValid());

	Placement->SetCursor(B2CellA);
	B2Expect(*this, TEXT("Build through the HUD flow"), Placement->ConfirmPlacement(), EBuildResult::Success);
	TestTrue(FString::Printf(TEXT("Result message and updated cost (%s, %s)"), *Hud->GetStatusText().ToString(), *Hud->GetCostText().ToString()),
		Hud->GetStatusText().ToString() == TEXT("Built Wooden Floor") && Hud->IsStatusValid() && Hud->GetCostText().ToString().Contains(TEXT("4/4")));

	struct FReason { EBuildResult Code; const TCHAR* Text; };
	const FReason Reasons[] = {
		{ EBuildResult::Success, TEXT("Ready to build") }, { EBuildResult::OutOfRange, TEXT("Too far") },
		{ EBuildResult::NoGround, TEXT("No ground") }, { EBuildResult::TooSteep, TEXT("Too steep") },
		{ EBuildResult::Uneven, TEXT("Uneven ground") }, { EBuildResult::Occupied, TEXT("Occupied") },
		{ EBuildResult::Blocked, TEXT("Blocked") }, { EBuildResult::BlocksBuilder, TEXT("Too close to you") },
		{ EBuildResult::InsufficientResources, TEXT("Not enough resources") }, { EBuildResult::BuildInterrupted, TEXT("Could not build") },
	};
	for (const FReason& Reason : Reasons)
	{
		TestEqual(FString::Printf(TEXT("Reason for %s"), *B2Name(Reason.Code)),
			UBuildingPlacementComponent::DescribeResult(Reason.Code).ToString(), FString(Reason.Text));
	}
	Placement->ExitBuildingMode();
	TestNull(TEXT("HUD removed on exit"), Placement->GetHudWidget());
	Fixture.Clean(*this, TEXT("HUD content"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB4ProductionAssetsTest,
	"NullTide.Building.B4.ProductionAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB4ProductionAssetsTest::RunTest(const FString& Parameters)
{
	const TCHAR* InputRoot = TEXT("/Game/LevelPrototyping/BuildingSystem/Input/");
	auto LoadInput = [InputRoot](const TCHAR* Name) { return LoadObject<UObject>(nullptr, *FString::Printf(TEXT("%s%s.%s"), InputRoot, Name, Name)); };
	const UInputMappingContext* BuildingContext = Cast<UInputMappingContext>(LoadInput(TEXT("IMC_Building")));
	const UInputMappingContext* ModeContext = Cast<UInputMappingContext>(LoadInput(TEXT("IMC_BuildingMode")));
	const UInputAction* Toggle = Cast<UInputAction>(LoadInput(TEXT("IA_BuildToggle")));
	const UInputAction* Confirm = Cast<UInputAction>(LoadInput(TEXT("IA_BuildConfirm")));
	const UInputAction* Cancel = Cast<UInputAction>(LoadInput(TEXT("IA_BuildCancel")));
	const UMaterialInterface* PreviewMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/LevelPrototyping/BuildingSystem/Materials/M_BuildPreview.M_BuildPreview"));
	UClass* HudClass = LoadClass<UBuildingHudWidget>(nullptr, B4HudPath);
	if (!TestTrue(TEXT("Building input, material and HUD assets load"), BuildingContext && ModeContext && Toggle && Confirm && Cancel
		&& PreviewMaterial && HudClass)) { return false; }

	for (const UInputAction* Action : { Toggle, Confirm, Cancel })
	{
		TestTrue(FString::Printf(TEXT("%s: digital, consumes its keys, no triggers"), *Action->GetName()),
			Action->ValueType == EInputActionValueType::Boolean && Action->bConsumeInput && Action->Triggers.IsEmpty());
	}
	bool bPlainMappings = true;
	for (const UInputMappingContext* Context : { BuildingContext, ModeContext })
	{
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			bPlainMappings &= Mapping.Triggers.IsEmpty() && Mapping.Modifiers.IsEmpty();
		}
	}
	TestTrue(TEXT("No per-mapping triggers or modifiers (the bound Started/Completed events stay as designed)"), bPlainMappings);
	const TArray<FEnhancedActionKeyMapping>& Always = BuildingContext->GetMappings();
	TestTrue(TEXT("IMC_Building: exactly B -> IA_BuildToggle"), Always.Num() == 1 && Always[0].Key == EKeys::B && Always[0].Action == Toggle);
	TMap<FKey, const UInputAction*> Mode;
	for (const FEnhancedActionKeyMapping& Mapping : ModeContext->GetMappings())
	{
		Mode.Add(Mapping.Key, Mapping.Action);
	}
	TestTrue(TEXT("IMC_BuildingMode: LMB confirms; RMB, I, Tab and F exit; nothing else"), ModeContext->GetMappings().Num() == 5
		&& Mode.FindRef(EKeys::LeftMouseButton) == Confirm && Mode.FindRef(EKeys::RightMouseButton) == Cancel
		&& Mode.FindRef(EKeys::I) == Cancel && Mode.FindRef(EKeys::Tab) == Cancel && Mode.FindRef(EKeys::F) == Cancel);

	FLinearColor Color;
	float Opacity = 0.f;
	TestTrue(TEXT("M_BuildPreview: translucent with Color and Opacity parameters"), PreviewMaterial->GetBlendMode() == BLEND_Translucent
		&& PreviewMaterial->GetVectorParameterValue(FHashedMaterialParameterInfo(ABuildingPreviewActor::ColorParameterName), Color)
		&& PreviewMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Opacity")), Opacity));

	UBlueprint* PlayerBlueprint = LoadObject<UBlueprint>(nullptr, B3PlayerPath);
	if (!TestNotNull(TEXT("Production player Blueprint loads"), PlayerBlueprint)) { return false; }
	TArray<const UBuildingPlacementComponent*> Templates;
	if (const USimpleConstructionScript* ConstructionScript = PlayerBlueprint->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : ConstructionScript->GetAllNodes())
		{
			if (const UBuildingPlacementComponent* Template = Node ? Cast<UBuildingPlacementComponent>(Node->ComponentTemplate) : nullptr)
			{
				Templates.Add(Template);
			}
		}
	}
	if (!TestEqual(TEXT("Production player has exactly one BuildingPlacementComponent"), Templates.Num(), 1)) { return false; }
	const UBuildingPlacementComponent* Template = Templates[0];
	TestTrue(TEXT("Configured with the building input"), B4ReadObject<UInputMappingContext>(Template, TEXT("BuildingContext")) == BuildingContext
		&& B4ReadObject<UInputMappingContext>(Template, TEXT("BuildingModeContext")) == ModeContext
		&& B4ReadObject<UInputAction>(Template, TEXT("ToggleAction")) == Toggle && B4ReadObject<UInputAction>(Template, TEXT("ConfirmAction")) == Confirm
		&& B4ReadObject<UInputAction>(Template, TEXT("CancelAction")) == Cancel);
	TestTrue(TEXT("Mode context outranks the toggle context, which outranks IMC_Default"), B4ReadInt(Template, TEXT("BuildingContextPriority")) == 1
		&& B4ReadInt(Template, TEXT("BuildingModeContextPriority")) == 2);
	TestTrue(TEXT("Configured with M_BuildPreview and W_BuildingHud"), B4ReadObject<UMaterialInterface>(Template, TEXT("PreviewMaterial")) == PreviewMaterial
		&& B4ReadObject<UClass>(Template, TEXT("HudWidgetClass")) == HudClass);
	return true;
}

#endif
