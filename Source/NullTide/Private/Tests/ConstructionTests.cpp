#include "BuildingTestTypes.h"

#include "BaseTestTypes.h"
#include "InventoryComponentTestTypes.h"
#include "Blueprint/UserWidget.h"
#include "Building/BuildingDoorActor.h"
#include "Building/BuildingPieceComponent.h"
#include "Building/BuildingPlacementComponent.h"
#include "Building/BuildingPreviewActor.h"
#include "Building/BuildingSubsystem.h"
#include "Building/ConstructionComponent.h"
#include "Components/BoxComponent.h"
#include "Components/Button.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CraftingTestTypes.h"
#include "Crafting/CraftingComponent.h"
#include "Crafting/CraftingStationComponent.h"
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
#include "Items/ItemInstance.h"
#include "Storage/StorageTransferLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Storage/StorageComponent.h"
#include "TimerManager.h"
#include "ToolLoadout/ToolLoadoutComponent.h"
#include "UI/BuildingBarSlotWidget.h"
#include "UI/BuildingBarWidget.h"
#include "UI/BuildingHudWidget.h"
#include "UI/InputPromptLibrary.h"
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

ABuildingTestWallPieceActor::ABuildingTestWallPieceActor()
{
	PieceMesh->SetRelativeLocation(FVector(0.0, 0.0, 125.0));
	PieceMesh->SetRelativeScale3D(FVector(2.0, 0.2, 2.5));
}

ABuildingStorageTestPiece::ABuildingStorageTestPiece()
{
	Storage = CreateDefaultSubobject<UStorageComponent>(TEXT("Storage"));
}

ABuildingStationTestPiece::ABuildingStationTestPiece()
{
	Station = CreateDefaultSubobject<UCraftingStationComponent>(TEXT("Station"));
}

UBuildingTestTinyFloorDefinition::UBuildingTestTinyFloorDefinition()
{
	B2MakeFloor(*this, TEXT("Test Tiny Floor"), ABuildingTestPieceActor::StaticClass());
	Cost.Reset();
	Cost.Add(B2Cost(UInventoryStackableTestItemDefinition::StaticClass(), 1));
	// A partial refund (the default is full since B6) that rounds Wood x 1 down to nothing.
	RefundFraction = 0.5f;
}

namespace
{
void B6MakeSupportedWall(UBuildingDefinition& Definition, const TCHAR* Name, TSubclassOf<AActor> PieceClass, int32 Wood)
{
	B2MakeFloor(Definition, Name, PieceClass);
	Definition.Category = EBuildingCategory::Wall;
	Definition.PlacementMode = EBuildingPlacementMode::GridEdge;
	Definition.bRequiresFloor = true;
	Definition.Cost.Reset();
	Definition.Cost.Add(B2Cost(UInventoryStackableTestItemDefinition::StaticClass(), Wood));
}
}

UBuildingTestSupportedWallDefinition::UBuildingTestSupportedWallDefinition()
{
	B6MakeSupportedWall(*this, TEXT("Test Supported Wall"), ABuildingTestWallPieceActor::StaticClass(), 6);
}

UBuildingTestStorageWallDefinition::UBuildingTestStorageWallDefinition()
{
	B6MakeSupportedWall(*this, TEXT("Test Storage Wall"), ABuildingStorageTestPiece::StaticClass(), 2);
}

UBuildingTestStationDefinition::UBuildingTestStationDefinition()
{
	B2MakeFloor(*this, TEXT("Test Station"), ABuildingStationTestPiece::StaticClass());
	Category = EBuildingCategory::Station;
	Cost.Reset();
	Cost.Add(B2Cost(UInventoryStackableTestItemDefinition::StaticClass(), 2));
}

UBuildingTestChestDefinition::UBuildingTestChestDefinition()
{
	B2MakeFloor(*this, TEXT("Test Chest"), ABuildingTestChestActor::StaticClass());
	Category = EBuildingCategory::Furniture;
	bRequiresFloor = true;
	Cost.Reset();
	Cost.Add(B2Cost(UInventoryStackableTestItemDefinition::StaticClass(), 2));
}

UBuildingTestBenchDefinition::UBuildingTestBenchDefinition()
{
	B2MakeFloor(*this, TEXT("Test Bench"), ABuildingTestBenchActor::StaticClass());
	Category = EBuildingCategory::Station;
	Cost.Reset();
	Cost.Add(B2Cost(UInventoryStackableTestItemDefinition::StaticClass(), 2));
}

void UConstructionTestListener::HandleDemolishResult(TSubclassOf<UBuildingDefinition> DefinitionClass, const FBuildingDemolishResult& Result)
{
	DemolishResults.Add(Result);
}

ABuildingTestTurnedHingeDoorActor::ABuildingTestTurnedHingeDoorActor()
{
	DoorHinge->SetRelativeRotation(FRotator(0.0, 30.0, 0.0));
	DoorHinge->SetRelativeScale3D(FVector(1.25));
}

UBuildingTestDoorPieceDefinition::UBuildingTestDoorPieceDefinition()
{
	B2MakeFloor(*this, TEXT("Test Swing Door"), ABuildingTestDoorActor::StaticClass());
	Category = EBuildingCategory::Door;
	PlacementMode = EBuildingPlacementMode::GridEdge;
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
		NullTideBaseTests::SpawnBaseZone(World, FVector::ZeroVector, NullTideBaseTests::FixtureZoneSize);
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
		Builder.Construction->OnDemolishResult.AddDynamic(Builder.Listener, &UConstructionTestListener::HandleDemolishResult);
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
	TestTrue(TEXT("Full refund (B6), default range"), FMath::IsNearlyEqual(Floor->RefundFraction, 1.f) && Floor->BuildRangeOverride == 0.f);
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
	// B5 appends the wall and door (asserted exactly in B5.ProductionInput); the floor stays first.
	const FB2Buildings Registered = Construction[0]->GetAvailableBuildings();
	TestTrue(TEXT("Building_WoodFloor is registered first"), !Registered.IsEmpty() && Registered[0] == Assets.Floor);
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
	// B5 adds R and 1-3 (the exact set is asserted in B5.ProductionInput).
	TestTrue(TEXT("IMC_BuildingMode: LMB confirms; RMB, I, Tab and F exit"), Mode.FindRef(EKeys::LeftMouseButton) == Confirm && Mode.FindRef(EKeys::RightMouseButton) == Cancel
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


// ---- B5: wooden wall and wooden door ----

namespace
{
const TCHAR* B5WallDefinitionPath = TEXT("/Game/LevelPrototyping/BuildingSystem/Definitions/Building_WoodWall.Building_WoodWall_C");
const TCHAR* B5DoorDefinitionPath = TEXT("/Game/LevelPrototyping/BuildingSystem/Definitions/Building_WoodDoor.Building_WoodDoor_C");
const TCHAR* B5WallPiecePath = TEXT("/Game/LevelPrototyping/BuildingSystem/Pieces/BP_BuildWall.BP_BuildWall_C");
const TCHAR* B5DoorPiecePath = TEXT("/Game/LevelPrototyping/BuildingSystem/Pieces/BP_BuildDoor.BP_BuildDoor_C");
const TCHAR* B5ScrapPath = TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Scrap.Item_Scrap_C");
const TCHAR* B5InteractablePath = TEXT("/Game/LevelPrototyping/interactionSystem/BPI_Interactable.BPI_Interactable_C");

/** Edge points on the fixture ground, all within range of a builder at the origin. */
const FVector B5EdgeX11(300.0, 205.0, 0.0);   // X edge (1, 1), center (300, 200)
const FVector B5EdgeY1m2(205.0, -300.0, 0.0); // Y edge (1, -2), center (200, -300)
const FVector B5EdgeX10(300.0, -5.0, 0.0);    // X edge (1, 0), center (300, 0)
const FVector B5EdgeX21(500.0, 195.0, 0.0);   // X edge (2, 1), center (500, 200): the north edge of cell C
const FVector B5EdgeX01(100.0, 205.0, 0.0);   // X edge (0, 1), center (100, 200)
const FVector B5CellD(300.0, 100.0, 0.0);     // Cell (1, 0): south of X edge (1, 1), east of Y edge (1, 0)
const FVector B5EdgeY20(405.0, 100.0, 0.0);   // Y edge (2, 0), center (400, 100): between cell D and cell C
const FVector B5EdgeY2m2(405.0, -300.0, 0.0); // Y edge (2, -2), center (400, -300): east of cell B
const FVector B5EdgeX0m1(100.0, -195.0, 0.0); // X edge (0, -1), center (100, -200): only diagonal to cell B
const TCHAR* B5DefaultContextPath = TEXT("/Game/TopDown/Input/IMC_Default.IMC_Default");
const TCHAR* B5InteractActionPath = TEXT("/Game/TopDown/Input/Actions/IA_Interact.IA_Interact");
const FVector B5EdgeY10(205.0, 100.0, 0.0);   // Y edge (1, 0), center (200, 100): meets X edge (1, 1) at a corner

struct FB5Assets
{
	FB3Assets Base;
	TSubclassOf<UBuildingDefinition> Wall;
	TSubclassOf<UBuildingDefinition> Door;
	TSubclassOf<ABuildingPieceActor> WallPiece;
	TSubclassOf<ABuildingDoorActor> DoorPiece;
	TSubclassOf<UItemDefinition> Scrap;

	static FB5Assets Load()
	{
		FB5Assets Assets;
		Assets.Base = FB3Assets::Load();
		Assets.Wall = LoadClass<UBuildingDefinition>(nullptr, B5WallDefinitionPath);
		Assets.Door = LoadClass<UBuildingDefinition>(nullptr, B5DoorDefinitionPath);
		Assets.WallPiece = LoadClass<ABuildingPieceActor>(nullptr, B5WallPiecePath);
		Assets.DoorPiece = LoadClass<ABuildingDoorActor>(nullptr, B5DoorPiecePath);
		Assets.Scrap = LoadClass<UItemDefinition>(nullptr, B5ScrapPath);
		return Assets;
	}

	bool IsValid(FAutomationTestBase& Test) const
	{
		return Base.IsValid(Test) && Test.TestTrue(TEXT("Building_WoodWall, Building_WoodDoor, BP_BuildWall, BP_BuildDoor and Item_Scrap load"),
			Wall && Door && WallPiece && DoorPiece && Scrap);
	}
};

FBuildingPlacement B5Place(TSubclassOf<UBuildingDefinition> Definition, const FVector& Point, int32 Rotation = 0)
{
	return UBuildingSubsystem::MakePlacementFromWorld(*Definition->GetDefaultObject<UBuildingDefinition>(), Point, Rotation);
}

FBuildingOccupancyKey B5EdgeKey(EBuildingEdgeAxis Axis, int32 X, int32 Y)
{
	return FBuildingOccupancyKey(EBuildingOccupancyCategory::Edge, Axis, X, Y, 0);
}

bool B5YawIs(const AActor* Actor, double Yaw)
{
	return Actor && FMath::IsNearlyZero(FRotator::NormalizeAxis(Actor->GetActorRotation().Yaw - Yaw), 0.01);
}

int32 B5AddedComponents(UClass* Class)
{
	const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Class);
	const USimpleConstructionScript* ConstructionScript = Generated ? Generated->SimpleConstructionScript.Get() : nullptr;
	return ConstructionScript ? ConstructionScript->GetAllNodes().Num() : 0;
}

/** The B3 piece contract: unscaled root, no child actors, no navigable geometry marked EvenIfNotCollidable. */
void B5ExpectPieceContract(FAutomationTestBase& Test, const FString& What, const AActor& Piece)
{
	Test.TestTrue(What + TEXT(": unscaled root"), Piece.GetRootComponent() && Piece.GetRootComponent()->GetRelativeScale3D().Equals(FVector::OneVector));
	TInlineComponentArray<UChildActorComponent*> ChildActors(&Piece);
	Test.TestEqual(What + TEXT(": no child actor components"), ChildActors.Num(), 0);
	TInlineComponentArray<UPrimitiveComponent*> Primitives(&Piece);
	bool bEvenIfNotCollidable = false;
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		bEvenIfNotCollidable |= Primitive && Primitive->HasCustomNavigableGeometry() == EHasCustomNavigableGeometry::EvenIfNotCollidable;
	}
	Test.TestFalse(What + TEXT(": no custom navigable geometry marked EvenIfNotCollidable"), bEvenIfNotCollidable);
}

bool B5BlocksPawns(const UPrimitiveComponent* Primitive)
{
	return Primitive && Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision
		&& Primitive->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block;
}

bool B5OverlapOnly(const UPrimitiveComponent* Primitive)
{
	if (!Primitive || Primitive->GetCollisionEnabled() != ECollisionEnabled::QueryOnly || Primitive->CanEverAffectNavigation())
	{
		return false;
	}
	for (int32 Channel = 0; Channel < ECC_MAX; ++Channel)
	{
		if (Primitive->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(Channel)) == ECR_Block)
		{
			return false;
		}
	}
	return true;
}

FBox B5MeshBounds(const AActor& Actor)
{
	FBox Box(ForceInit);
	TInlineComponentArray<UStaticMeshComponent*> Meshes(&Actor);
	for (const UStaticMeshComponent* Mesh : Meshes)
	{
		Box += Mesh->Bounds.GetBox();
	}
	return Box;
}

/** Advances the world timers like real frames (see CraftingTimedTests: one tick per frame, timers activate on the next). */
void B5AdvanceTime(UWorld* World, float Seconds)
{
	FTimerManager& Timers = World->GetTimerManager();
	++GFrameCounter;
	Timers.Tick(0.f);
	++GFrameCounter;
	Timers.Tick(Seconds);
}

/** Calls a Blueprint interface function with one object parameter (the C8B ProcessEvent pattern). */
void B5CallWithObject(FAutomationTestBase& Test, UObject* Target, const TCHAR* FunctionName, const TCHAR* ParameterName, UObject* Value)
{
	UFunction* Function = Target->FindFunction(FName(FunctionName));
	if (!Test.TestNotNull(FString::Printf(TEXT("%s is implemented"), FunctionName), Function))
	{
		return;
	}
	const FObjectPropertyBase* Parameter = CastField<FObjectPropertyBase>(Function->FindPropertyByName(FName(ParameterName)));
	if (!Test.TestNotNull(FString::Printf(TEXT("%s has a %s parameter"), FunctionName, ParameterName), Parameter))
	{
		return;
	}
	TArray<uint8> Buffer;
	Buffer.SetNumZeroed(FMath::Max<int32>(Function->ParmsSize, 1));
	Parameter->SetObjectPropertyValue_InContainer(Buffer.GetData(), Value);
	Target->ProcessEvent(Function, Buffer.GetData());
}

/** Display name of the enum a Blueprint function returns through its out parameter. */
FString B5CallEnumGetter(UObject* Target, UFunction* Function)
{
	TArray<uint8> Buffer;
	Buffer.SetNumZeroed(FMath::Max<int32>(Function->ParmsSize, 1));
	Target->ProcessEvent(Function, Buffer.GetData());
	const FProperty* Return = Function->GetReturnProperty();
	for (TFieldIterator<FProperty> It(Function); It && !Return; ++It)
	{
		Return = It->HasAnyPropertyFlags(CPF_OutParm) ? *It : nullptr;
	}
	if (const FByteProperty* Byte = CastField<FByteProperty>(Return); Byte && Byte->Enum)
	{
		return Byte->Enum->GetDisplayNameTextByValue(*Byte->ContainerPtrToValuePtr<uint8>(Buffer.GetData())).ToString();
	}
	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Return))
	{
		const int64 Value = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(Buffer.GetData()));
		return EnumProperty->GetEnum()->GetDisplayNameTextByValue(Value).ToString();
	}
	return FString();
}

/** The facing arrow's meshes that have a static mesh. */
TArray<const UStaticMeshComponent*> B5ArrowMeshes(const ABuildingPreviewActor& Preview)
{
	TArray<const UStaticMeshComponent*> Meshes;
	if (const USceneComponent* Arrow = Preview.GetFacingArrow())
	{
		for (const USceneComponent* Child : Arrow->GetAttachChildren())
		{
			if (const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Child); Mesh && Mesh->GetStaticMesh())
			{
				Meshes.Add(Mesh);
			}
		}
	}
	return Meshes;
}

/** Whether the door's leaf lies on its facing side (local +Y, the side the ghost's arrow showed). */
bool B5LeafOnFacingSide(const ABuildingDoorActor& Door)
{
	const FVector Offset = Door.GetDoorLeaf()->Bounds.Origin - Door.GetActorLocation();
	return FVector::DotProduct(Offset, Door.GetActorRightVector()) > 30.0;
}

ACharacter* B5SpawnCharacter(UWorld* World, const FVector& Location)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<ACharacter>(ACharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
}

TSubclassOf<UBuildingDefinition> B5TestDoorClass() { return UBuildingTestDoorPieceDefinition::StaticClass(); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB5ProductionDefinitionsTest,
	"NullTide.Building.B5.ProductionDefinitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB5ProductionDefinitionsTest::RunTest(const FString& Parameters)
{
	const FB5Assets Assets = FB5Assets::Load();
	if (!Assets.IsValid(*this)) { return false; }
	struct FExpected
	{
		TSubclassOf<UBuildingDefinition> Definition;
		const TCHAR* Name;
		EBuildingCategory Category;
		UClass* Piece;
		TArray<FInventoryDefinitionQuantity> Cost;
	};
	const FExpected Expected[] = {
		{ Assets.Wall, TEXT("Wooden Wall"), EBuildingCategory::Wall, Assets.WallPiece.Get(), { B2Cost(Assets.Base.Wood, 6) } },
		{ Assets.Door, TEXT("Wooden Door"), EBuildingCategory::Door, Assets.DoorPiece.Get(), { B2Cost(Assets.Base.Wood, 8), B2Cost(Assets.Scrap, 2) } },
	};
	for (const FExpected& Entry : Expected)
	{
		const UBuildingDefinition* Definition = Entry.Definition->GetDefaultObject<UBuildingDefinition>();
		FString Diagnostic;
		TestTrue(FString::Printf(TEXT("%s is valid (%s)"), Entry.Name, *Diagnostic), Definition->ValidateBuilding(Diagnostic));
		TestEqual(FString::Printf(TEXT("%s: name"), Entry.Name), Definition->BuildingName.ToString(), FString(Entry.Name));
		TestTrue(FString::Printf(TEXT("%s: category, GridEdge, 1 x 1, 90 degree steps"), Entry.Name), Definition->Category == Entry.Category
			&& Definition->PlacementMode == EBuildingPlacementMode::GridEdge && Definition->Footprint == FIntPoint(1, 1) && Definition->RotationStep == 90);
		bool bCost = Definition->Cost.Num() == Entry.Cost.Num();
		for (int32 Index = 0; bCost && Index < Entry.Cost.Num(); ++Index)
		{
			bCost = Definition->Cost[Index].DefinitionClass == Entry.Cost[Index].DefinitionClass && Definition->Cost[Index].Quantity == Entry.Cost[Index].Quantity;
		}
		TestTrue(FString::Printf(TEXT("%s: exact cost"), Entry.Name), bCost);
		TestTrue(FString::Printf(TEXT("%s: full refund (B6), default range"), Entry.Name), FMath::IsNearlyEqual(Definition->RefundFraction, 1.f)
			&& Definition->BuildRangeOverride == 0.f);
		TestTrue(FString::Printf(TEXT("%s: piece class"), Entry.Name), Definition->PieceActorClass == Entry.Piece);
		TestTrue(FString::Printf(TEXT("%s: requires a floor (B5-R1)"), Entry.Name), Definition->bRequiresFloor);
#if WITH_EDITOR
		FDataValidationContext Context;
		TestTrue(FString::Printf(TEXT("%s: editor data validation passes"), Entry.Name), Definition->IsDataValid(Context) != EDataValidationResult::Invalid);
#endif
	}

	TestTrue(TEXT("BP_BuildWall derives from ABuildingPieceActor, BP_BuildDoor from ABuildingDoorActor"),
		Assets.WallPiece->IsChildOf(ABuildingPieceActor::StaticClass()) && Assets.DoorPiece->IsChildOf(ABuildingDoorActor::StaticClass()));
	TestEqual(TEXT("BP_BuildWall adds no components"), B5AddedComponents(Assets.WallPiece.Get()), 0);
	TestEqual(TEXT("BP_BuildDoor adds no components"), B5AddedComponents(Assets.DoorPiece.Get()), 0);

	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABuildingPieceActor* Wall = Fixture.World->SpawnActor<ABuildingPieceActor>(Assets.WallPiece, FVector(300.0, 200.0, 0.0), FRotator::ZeroRotator, Params);
	ABuildingDoorActor* Door = Fixture.World->SpawnActor<ABuildingDoorActor>(Assets.DoorPiece, FVector(300.0, 0.0, 0.0), FRotator::ZeroRotator, Params);
	if (!TestTrue(TEXT("Both pieces spawn"), Wall && Door)) { return false; }
	B5ExpectPieceContract(*this, TEXT("BP_BuildWall"), *Wall);
	B5ExpectPieceContract(*this, TEXT("BP_BuildDoor"), *Door);

	TInlineComponentArray<UStaticMeshComponent*> WallMeshes(Wall);
	if (TestEqual(TEXT("BP_BuildWall: one mesh (the inherited PieceMesh)"), WallMeshes.Num(), 1))
	{
		const FBox Bounds = WallMeshes[0]->Bounds.GetBox();
		TestTrue(TEXT("BP_BuildWall: engine cube that blocks pawns"), WallMeshes[0]->GetStaticMesh()
			&& WallMeshes[0]->GetStaticMesh()->GetPathName() == TEXT("/Engine/BasicShapes/Cube.Cube") && B5BlocksPawns(WallMeshes[0]));
		TestTrue(FString::Printf(TEXT("BP_BuildWall: 200 x 20 x 250 cm along the edge, standing on the surface (got %s)"), *Bounds.ToString()),
			Bounds.Min.Equals(FVector(200.0, 190.0, 0.0), 0.5) && Bounds.Max.Equals(FVector(400.0, 210.0, 250.0), 0.5));
	}

	TInlineComponentArray<UStaticMeshComponent*> DoorMeshes(Door);
	TestEqual(TEXT("BP_BuildDoor: two posts, a lintel and a leaf"), DoorMeshes.Num(), 4);
	bool bAllBlock = DoorMeshes.Num() == 4;
	for (const UStaticMeshComponent* Mesh : DoorMeshes)
	{
		bAllBlock &= B5BlocksPawns(Mesh);
	}
	TestTrue(TEXT("BP_BuildDoor: the closed frame and leaf block pawns"), bAllBlock);
	TestTrue(TEXT("BP_BuildDoor: the leaf hangs on the hinge"), Door->GetDoorLeaf() && Door->GetDoorHinge()
		&& Door->GetDoorLeaf()->GetAttachParent() == Door->GetDoorHinge());
	const FBox DoorBounds = B5MeshBounds(*Door);
	TestTrue(FString::Printf(TEXT("BP_BuildDoor: 200 x 20 x 250 cm on the edge (got %s)"), *DoorBounds.ToString()),
		DoorBounds.Min.Equals(FVector(200.0, -10.0, 0.0), 0.5) && DoorBounds.Max.Equals(FVector(400.0, 10.0, 250.0), 0.5));
	const FBox Leaf = Door->GetDoorLeaf()->Bounds.GetBox();
	TestTrue(FString::Printf(TEXT("BP_BuildDoor: a 100 x 220 cm leaf centred in the edge (got %s)"), *Leaf.ToString()),
		Leaf.Min.Equals(FVector(250.0, -4.0, 0.0), 0.5) && Leaf.Max.Equals(FVector(350.0, 4.0, 220.0), 0.5));
	TestTrue(TEXT("BP_BuildDoor: the interaction volume is overlap-only and never affects navigation"),
		B5OverlapOnly(Door->GetInteractionBounds()));
	TestTrue(TEXT("BP_BuildDoor: closed and idle, no actor tick"), !Door->IsDoorOpen() && !Door->IsDoorMoving()
		&& !Door->PrimaryActorTick.bCanEverTick);
	Fixture.Clean(*this, TEXT("Production definitions"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB5EdgeBuildTest,
	"NullTide.Building.B5.EdgeBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB5EdgeBuildTest::RunTest(const FString& Parameters)
{
	const FB5Assets Assets = FB5Assets::Load();
	if (!Assets.IsValid(*this)) { return false; }
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { Assets.Base.Floor, Assets.Wall, Assets.Door }, 0);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	TestTrue(TEXT("54 production Wood and 2 Scrap"), Builder.Inventory->TryAddDefinitionQuantity(Assets.Base.Wood, 54).IsSuccess()
		&& Builder.Inventory->TryAddDefinitionQuantity(Assets.Scrap, 2).IsSuccess());
	UInventoryComponent* Inventory = Builder.Inventory;
	auto Wood = [&]() { return Inventory->GetTotalQuantity(Assets.Base.Wood); };
	auto Scrap = [&]() { return Inventory->GetTotalQuantity(Assets.Scrap); };
	auto PieceAtEdge = [&](EBuildingEdgeAxis Axis, int32 X, int32 Y)
	{
		const UBuildingPieceComponent* Piece = Fixture.Subsystem->GetOccupyingPiece(B5EdgeKey(Axis, X, Y));
		return Piece ? Cast<ABuildingPieceActor>(Piece->GetOwner()) : nullptr;
	};

	// B5-R1: walls and doors need an Active floor along one side; bare ground refuses without consuming anything.
	int32 WoodBefore = Wood();
	B2Expect(*this, TEXT("Wall on bare ground"), Builder.Construction->TryBuild(Assets.Wall, B5Place(Assets.Wall, B5EdgeX11)), EBuildResult::NeedsFloor);
	B2Expect(*this, TEXT("Door on bare ground"), Builder.Construction->TryBuild(Assets.Door, B5Place(Assets.Door, B5EdgeY10)), EBuildResult::NeedsFloor);
	TestTrue(TEXT("Refused on bare ground: nothing consumed, nothing built"), Wood() == WoodBefore && Scrap() == 2 && Fixture.LivePieces() == 0);
	B2Expect(*this, TEXT("Floor on cell A"), Builder.Construction->TryBuild(Assets.Base.Floor, B5Place(Assets.Base.Floor, B2CellA)), EBuildResult::Success);
	B2Expect(*this, TEXT("Floor on cell B"), Builder.Construction->TryBuild(Assets.Base.Floor, B5Place(Assets.Base.Floor, B2CellB)), EBuildResult::Success);

	// Walls on an X edge and on a Y edge, each with a floor on one side only (a room's outer boundary).
	WoodBefore = Wood();
	int32 RevisionBefore = Inventory->GetRevision();
	FBuildingConstructionResult Result = Builder.Construction->TryBuild(Assets.Wall, B5Place(Assets.Wall, B5EdgeX11));
	if (!B2Expect(*this, TEXT("Wall on X edge (1, 1), floor A on one side"), Result, EBuildResult::Success)) { return false; }
	const ABuildingPieceActor* WallX = Cast<ABuildingPieceActor>(Result.Piece.Get());
	TestTrue(TEXT("X wall: Wood -6 in one revision"), Wood() == WoodBefore - 6 && Inventory->GetRevision() == RevisionBefore + 1);
	TestTrue(TEXT("X wall: a BP_BuildWall on its Edge key, yaw 0, at the edge center on the floor's top, Active"), WallX
		&& WallX->IsA(Assets.WallPiece) && PieceAtEdge(EBuildingEdgeAxis::X, 1, 1) == WallX && B5YawIs(WallX, 0.0)
		&& WallX->GetActorLocation().Equals(FVector(300.0, 200.0, UBuildingSubsystem::FloorThickness), 0.1)
		&& WallX->GetBuildingPieceComponent()->IsActivePiece());

	Result = Builder.Construction->TryBuild(Assets.Wall, B5Place(Assets.Wall, B5EdgeY1m2));
	B2Expect(*this, TEXT("Wall on Y edge (1, -2), floor B on one side"), Result, EBuildResult::Success);
	const ABuildingPieceActor* WallY = Cast<ABuildingPieceActor>(Result.Piece.Get());
	TestTrue(TEXT("Y wall: on its Edge key, yaw 90, at the edge center"), WallY && PieceAtEdge(EBuildingEdgeAxis::Y, 1, -2) == WallY
		&& B5YawIs(WallY, 90.0) && WallY->GetActorLocation().Equals(FVector(200.0, -300.0, UBuildingSubsystem::FloorThickness), 0.1));
	const FBox WallYBounds = WallY ? B5MeshBounds(*WallY) : FBox(ForceInit);
	TestTrue(FString::Printf(TEXT("Y wall: runs along Y (got %s)"), *WallYBounds.ToString()),
		WallYBounds.Min.Equals(FVector(190.0, -400.0, 10.0), 0.5) && WallYBounds.Max.Equals(FVector(210.0, -200.0, 260.0), 0.5));

	// A wall on a production floor's edge stands on the floor's top.
	B2Expect(*this, TEXT("Floor on cell C"), Builder.Construction->TryBuild(Assets.Base.Floor, B5Place(Assets.Base.Floor, B2CellC)), EBuildResult::Success);
	Result = Builder.Construction->TryBuild(Assets.Wall, B5Place(Assets.Wall, B5EdgeX21));
	B2Expect(*this, TEXT("Wall on the floor's edge"), Result, EBuildResult::Success);
	TestTrue(TEXT("The wall on the floor's edge stands at 10 cm on layer 0"), Result.Piece.IsValid()
		&& FMath::IsNearlyEqual(Result.Piece->GetActorLocation().Z, UBuildingSubsystem::FloorThickness, 0.1)
		&& PieceAtEdge(EBuildingEdgeAxis::X, 2, 1) == Result.Piece.Get());

	// D3: a floor builds in a cell whose edge already holds a wall (the wall's half thickness reaches into the cell).
	B2Expect(*this, TEXT("Floor on cell D, next to the X wall"), Builder.Construction->TryBuild(Assets.Base.Floor, B5Place(Assets.Base.Floor, B5CellD)),
		EBuildResult::Success);

	// Floor sides: both sides, the negative side of a Y edge alone, and a floor that only touches the edge's corner.
	B2Expect(*this, TEXT("Wall on Y edge (2, 0), floors on both sides"), Builder.Construction->TryBuild(Assets.Wall, B5Place(Assets.Wall, B5EdgeY20)),
		EBuildResult::Success);
	B2Expect(*this, TEXT("Wall on Y edge (2, -2), floor B on its negative side only"),
		Builder.Construction->TryBuild(Assets.Wall, B5Place(Assets.Wall, B5EdgeY2m2)), EBuildResult::Success);
	B2Expect(*this, TEXT("Wall on X edge (0, -1), floor B only diagonal"), Builder.Construction->TryBuild(Assets.Wall, B5Place(Assets.Wall, B5EdgeX0m1)),
		EBuildResult::NeedsFloor);

	// An edge holds one piece. The existing piece stands on the new piece's surface samples, so the B1 order (support
	// surface before occupancy) refuses with Blocked and names it; the edge key stays with the first piece.
	WoodBefore = Wood();
	int32 ScrapBefore = Scrap();
	B2ExpectBranch(*this, TEXT("Door on a walled edge"), Builder.Construction->TryBuild(Assets.Door, B5Place(Assets.Door, B5EdgeX11)),
		EBuildResult::Blocked, WallX ? *WallX->GetName() : TEXT("<no wall>"));
	TestTrue(TEXT("The walled edge still belongs to the wall"), WallX && PieceAtEdge(EBuildingEdgeAxis::X, 1, 1) == WallX);
	TestTrue(TEXT("Refused door: nothing consumed"), Wood() == WoodBefore && Scrap() == ScrapBefore);

	// A door on a free edge, rotated once: the facing flips by 180 degrees.
	RevisionBefore = Inventory->GetRevision();
	Result = Builder.Construction->TryBuild(Assets.Door, B5Place(Assets.Door, B5EdgeY10, 1));
	B2Expect(*this, TEXT("Door on Y edge (1, 0) at rotation 1, at the X wall's corner"), Result, EBuildResult::Success);
	ABuildingDoorActor* Door = Cast<ABuildingDoorActor>(Result.Piece.Get());
	TestTrue(TEXT("Door: Wood -8 and Scrap -2 in one revision"), Wood() == WoodBefore - 8 && Scrap() == ScrapBefore - 2
		&& Inventory->GetRevision() == RevisionBefore + 1);
	TestTrue(TEXT("Door: a BP_BuildDoor on its Edge key, yaw 270 (90 flipped), Active, closed, usable by the builder"), Door && Door->IsA(Assets.DoorPiece)
		&& PieceAtEdge(EBuildingEdgeAxis::Y, 1, 0) == Door && B5YawIs(Door, 270.0) && Door->GetBuildingPieceComponent()->IsActivePiece()
		&& !Door->IsDoorOpen() && Door->CanInteract(Builder.Pawn) == EBuildingDoorResult::Success);
	TestTrue(TEXT("The finished door is visible and collidable"), Door && !Door->IsHidden() && Door->GetActorEnableCollision()
		&& B5BlocksPawns(Door->GetDoorLeaf()));
	B2ExpectBranch(*this, TEXT("Wall on a door's edge"), Builder.Construction->TryBuild(Assets.Wall, B5Place(Assets.Wall, B5EdgeY10)),
		EBuildResult::Blocked, Door ? *Door->GetName() : TEXT("<no door>"));
	TestTrue(TEXT("The door's edge still belongs to the door"), Door && PieceAtEdge(EBuildingEdgeAxis::Y, 1, 0) == Door);

	TestTrue(FString::Printf(TEXT("All resources spent exactly (Wood %d, Scrap %d)"), Wood(), Scrap()), Wood() == 0 && Scrap() == 0);
	TestEqual(TEXT("Four floors, five walls and a door"), Fixture.LivePieces(), 10);
	Fixture.Clean(*this, TEXT("Edge build"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB5DoorToggleTest,
	"NullTide.Building.B5.DoorToggle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB5DoorToggleTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { B5TestDoorClass() }, 8);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid())) { return false; }

	// A provisional door refuses interaction.
	EBuildingDoorResult Provisional = EBuildingDoorResult::Success;
	bool bProvisionalOpened = true;
	Builder.Construction->BeforeActivate = [&Provisional, &bProvisionalOpened, &Builder](ABuildingPieceActor& Piece)
	{
		if (ABuildingDoorActor* ProvisionalDoor = Cast<ABuildingDoorActor>(&Piece))
		{
			Provisional = ProvisionalDoor->ToggleDoor(Builder.Pawn);
			bProvisionalOpened = ProvisionalDoor->IsDoorOpen();
		}
	};
	const FBuildingConstructionResult Built = Builder.Construction->TryBuild(B5TestDoorClass(), B5Place(B5TestDoorClass(), B5EdgeY10));
	Builder.Construction->BeforeActivate = nullptr;
	if (!B2Expect(*this, TEXT("Build the test door"), Built, EBuildResult::Success)) { return false; }
	TestTrue(TEXT("Provisional door: NotActive, stays closed"), Provisional == EBuildingDoorResult::NotActive && !bProvisionalOpened);
	ABuildingTestDoorActor* Door = Cast<ABuildingTestDoorActor>(Built.Piece.Get());
	if (!TestNotNull(TEXT("Test door"), Door)) { return false; }
	UStaticMeshComponent* Leaf = Door->GetDoorLeaf();
	TestTrue(TEXT("Closed after the build: the leaf blocks"), !Door->IsDoorOpen() && B5BlocksPawns(Leaf));
	auto FrameBlocks = [Door, Leaf]()
	{
		TInlineComponentArray<UStaticMeshComponent*> Meshes(Door);
		bool bBlocks = Meshes.Num() == 4;
		for (const UStaticMeshComponent* Mesh : Meshes)
		{
			bBlocks &= Mesh == Leaf || B5BlocksPawns(Mesh);
		}
		return bBlocks;
	};

	// Instant swing.
	Door->SetSwingDuration(0.f);
	TestEqual(TEXT("Open"), Door->ToggleDoor(Builder.Pawn), EBuildingDoorResult::Success);
	TestTrue(TEXT("Open: fully swung, the leaf has no collision, the frame still blocks, not moving"), Door->IsDoorOpen()
		&& FMath::IsNearlyEqual(Door->GetDoorOpenAlpha(), 1.f) && Leaf->GetCollisionEnabled() == ECollisionEnabled::NoCollision
		&& FrameBlocks() && !Door->IsDoorMoving());
	TestTrue(TEXT("Open: the hinge is turned 90 degrees"), FMath::IsNearlyEqual(Door->GetDoorHinge()->GetRelativeRotation().Yaw, 90.0, 0.01));
	TestTrue(TEXT("Open at rotation 0: the leaf is on the facing (+Y) side"), B5LeafOnFacingSide(*Door));

	// Rotation 1 turns the door 180 degrees, so it opens to the other world side, still its facing side.
	const FBuildingConstructionResult Flipped = Builder.Construction->TryBuild(B5TestDoorClass(), B5Place(B5TestDoorClass(), B5EdgeX01, 1));
	ABuildingTestDoorActor* FlippedDoor = Cast<ABuildingTestDoorActor>(Flipped.Piece.Get());
	if (B2Expect(*this, TEXT("Second door at rotation 1"), Flipped, EBuildResult::Success) && TestNotNull(TEXT("Second door"), FlippedDoor))
	{
		FlippedDoor->SetSwingDuration(0.f);
		TestEqual(TEXT("Open the rotated door"), FlippedDoor->ToggleDoor(Builder.Pawn), EBuildingDoorResult::Success);
		TestTrue(TEXT("Open at rotation 1: yaw 180, leaf on the facing side, which is world -Y here"), B5YawIs(FlippedDoor, 180.0)
			&& B5LeafOnFacingSide(*FlippedDoor) && FlippedDoor->GetDoorLeaf()->Bounds.Origin.Y < 200.0 - 30.0);
	}
	TestEqual(TEXT("Close"), Door->ToggleDoor(Builder.Pawn), EBuildingDoorResult::Success);
	TestTrue(TEXT("Closed: the leaf blocks again"), !Door->IsDoorOpen() && FMath::IsNearlyZero(Door->GetDoorOpenAlpha()) && B5BlocksPawns(Leaf)
		&& FMath::IsNearlyZero(Door->GetDoorHinge()->GetRelativeRotation().Yaw, 0.01));
	TestEqual(TEXT("SetDoorOpen(false) on a closed door is accepted and changes nothing"), Door->SetDoorOpen(false, Builder.Pawn),
		EBuildingDoorResult::Success);

	// Refusals change nothing.
	ACharacter* Far = B5SpawnCharacter(Fixture.World, FVector(300.0, 400.0, 92.0));
	TestEqual(TEXT("Null interactor"), Door->ToggleDoor(nullptr), EBuildingDoorResult::InvalidInteractor);
	TestEqual(TEXT("Interactor beyond 300 cm"), Door->ToggleDoor(Far), EBuildingDoorResult::OutOfRange);
	TestTrue(TEXT("Refused requests leave the door closed and blocking"), !Door->IsDoorOpen() && B5BlocksPawns(Leaf) && !Door->IsDoorMoving());

	// Timed swing: reversal mid-swing; collision only when fully closed.
	Door->SetSwingDuration(0.3f);
	Door->ToggleDoor(Builder.Pawn);
	TestTrue(TEXT("Opening: moving and the leaf stops blocking at once"), Door->IsDoorOpen() && Door->IsDoorMoving()
		&& Leaf->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	B5AdvanceTime(Fixture.World, 0.1f);
	const float Partway = Door->GetDoorOpenAlpha();
	TestTrue(FString::Printf(TEXT("Partway open after 0.1 s (alpha %.3f)"), Partway), Partway > 0.2f && Partway < 0.5f && Door->IsDoorMoving());
	Door->ToggleDoor(Builder.Pawn);
	B5AdvanceTime(Fixture.World, 0.05f);
	TestTrue(FString::Printf(TEXT("Reversed: closing from where it was (alpha %.3f), still not blocking"), Door->GetDoorOpenAlpha()),
		!Door->IsDoorOpen() && Door->GetDoorOpenAlpha() < Partway && Door->GetDoorOpenAlpha() > 0.f && Door->IsDoorMoving()
		&& Leaf->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	B5AdvanceTime(Fixture.World, 0.5f);
	TestTrue(TEXT("Fully closed: stopped, blocking again"), FMath::IsNearlyZero(Door->GetDoorOpenAlpha()) && !Door->IsDoorMoving()
		&& B5BlocksPawns(Leaf));
	Door->ToggleDoor(Builder.Pawn);
	B5AdvanceTime(Fixture.World, 0.5f);
	TestTrue(TEXT("Fully open: stopped, not blocking, no actor tick"), FMath::IsNearlyEqual(Door->GetDoorOpenAlpha(), 1.f) && !Door->IsDoorMoving()
		&& Leaf->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Door->IsActorTickEnabled());

	// Only pawns use doors; a door being destroyed refuses (no swing restarts on a dying actor).
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* NotAPawn = Fixture.World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Door->GetActorLocation()), Params);
	TestEqual(TEXT("A non-pawn interactor is refused"), Door->ToggleDoor(NotAPawn), EBuildingDoorResult::InvalidInteractor);
	TestTrue(TEXT("The refused request left the door open"), Door->IsDoorOpen());
	Door->Destroy();
	TestTrue(TEXT("A destroyed door refuses and has no swing timer"), Door->ToggleDoor(Builder.Pawn) == EBuildingDoorResult::NotActive
		&& !Door->IsDoorMoving());
	Fixture.Clean(*this, TEXT("Door toggle"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB5DoorInterfaceTest,
	"NullTide.Building.B5.DoorInterface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB5DoorInterfaceTest::RunTest(const FString& Parameters)
{
	const FB5Assets Assets = FB5Assets::Load();
	UClass* InteractableClass = LoadClass<UInterface>(nullptr, B5InteractablePath);
	if (!Assets.IsValid(*this) || !TestNotNull(TEXT("BPI_Interactable loads"), InteractableClass)) { return false; }
	TestTrue(TEXT("BP_BuildDoor implements BPI_Interactable"), Assets.DoorPiece->ImplementsInterface(InteractableClass));

	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { Assets.Base.Floor, Assets.Door }, 0);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	Builder.Inventory->TryAddDefinitionQuantity(Assets.Base.Wood, 12);
	Builder.Inventory->TryAddDefinitionQuantity(Assets.Scrap, 2);
	B2Expect(*this, TEXT("Floor beside the door's edge"), Builder.Construction->TryBuild(Assets.Base.Floor, B5Place(Assets.Base.Floor, B5CellD)),
		EBuildResult::Success);
	const FBuildingConstructionResult Built = Builder.Construction->TryBuild(Assets.Door, B5Place(Assets.Door, B5EdgeY10));
	if (!B2Expect(*this, TEXT("Build the production door"), Built, EBuildResult::Success)) { return false; }
	ABuildingDoorActor* Door = Cast<ABuildingDoorActor>(Built.Piece.Get());
	if (!TestNotNull(TEXT("Production door"), Door)) { return false; }

	UFunction* GetType = Door->FindFunction(FName(TEXT("GetInteractionType")));
	if (TestNotNull(TEXT("GetInteractionType is implemented"), GetType))
	{
		TestEqual(TEXT("Interaction type is Press"), B5CallEnumGetter(Door, GetType), FString(TEXT("Press")));
	}

	B5CallWithObject(*this, Door, TEXT("Interact"), TEXT("Interactor"), Builder.Pawn);
	TestTrue(TEXT("Interact in range opens the door"), Door->IsDoorOpen());
	B5AdvanceTime(Fixture.World, 0.5f);
	TestTrue(TEXT("Fully open: the leaf is on the facing side the ghost's arrow showed"), !Door->IsDoorMoving() && B5LeafOnFacingSide(*Door));
	B5CallWithObject(*this, Door, TEXT("Interact"), TEXT("Interactor"), Builder.Pawn);
	TestFalse(TEXT("Interact again closes it"), Door->IsDoorOpen());
	ACharacter* Far = B5SpawnCharacter(Fixture.World, FVector(300.0, 400.0, 92.0));
	B5CallWithObject(*this, Door, TEXT("Interact"), TEXT("Interactor"), Far);
	TestFalse(TEXT("Interact out of range is refused"), Door->IsDoorOpen());
	TestTrue(TEXT("Interaction never touches the inventory or the registry"), Builder.Inventory->GetTotalQuantity(Assets.Base.Wood) == 0
		&& Fixture.LivePieces() == 2);
	Fixture.Clean(*this, TEXT("Door interface"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB5SelectionAndRotationTest,
	"NullTide.Building.B5.SelectionAndRotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB5SelectionAndRotationTest::RunTest(const FString& Parameters)
{
	UClass* HudClass = LoadClass<UBuildingHudWidget>(nullptr, B4HudPath);
	if (!TestNotNull(TEXT("W_BuildingHud loads"), HudClass)) { return false; }
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	const TSubclassOf<UBuildingDefinition> Wall = UBuildingTestWallDefinition::StaticClass();
	FB4Player Player = B4SpawnPlayer(Fixture, { B2FloorClass(), Wall, B5TestDoorClass() }, 30);
	if (!TestTrue(TEXT("Player set up"), Player.IsValid())) { return false; }
	UBuildingPlacementTestComponent* Placement = Player.Placement;
	Placement->SetHudClass(HudClass);
	Placement->SetCursor(B5EdgeX10);
	auto HudName = [Placement]() { return Placement->GetHudWidget() ? Placement->GetHudWidget()->GetBuildingNameText().ToString() : FString(); };

	if (!TestTrue(TEXT("Enter"), Placement->EnterBuildingMode())) { return false; }
	ABuildingPreviewActor* Preview = Placement->GetPreviewActor();
	if (!TestNotNull(TEXT("Preview"), Preview)) { return false; }
	TestTrue(TEXT("Index 0 (Floor) by default: one ghost mesh"), Placement->GetSelectedBuildingIndex() == 0
		&& Placement->GetSelectedBuilding() == B2FloorClass() && Preview->GetPreviewMeshCount() == 1 && HudName() == TEXT("Test Floor"));
	// B5-R3: the facing arrow is part of the ghost (moves and turns with it), never collides, and is not a piece mesh.
	auto ArrowAlong = [Preview](const FVector& Direction)
	{
		return Preview->GetFacingArrow() && Preview->GetFacingArrow()->GetForwardVector().Equals(Direction, 0.01);
	};
	TestTrue(TEXT("Floor ghost: the arrow points along the piece's +X"), ArrowAlong(Preview->GetActorForwardVector()));
	TestEqual(TEXT("The arrow has its shaft and head meshes"), B5ArrowMeshes(*Preview).Num(), 2);
	bool bArrowCollides = false;
	TInlineComponentArray<UPrimitiveComponent*> GhostPrimitives(Preview);
	for (const UPrimitiveComponent* Primitive : GhostPrimitives)
	{
		bArrowCollides |= Primitive && (Primitive->IsCollisionEnabled() || Primitive->IsNavigationRelevant());
	}
	TestFalse(TEXT("No ghost part, arrow included, collides or affects navigation"), bArrowCollides);

	TestTrue(TEXT("Select index 2 (Door)"), Placement->SelectBuildingIndex(2));
	TestTrue(TEXT("Door selected: the same ghost actor now has the door's four meshes, the HUD follows"), Placement->GetSelectedBuildingIndex() == 2
		&& Placement->GetSelectedBuilding() == B5TestDoorClass() && Placement->GetPreviewActor() == Preview && Preview->GetPreviewMeshCount() == 4
		&& HudName() == TEXT("Test Swing Door") && B4CountPreviews(Fixture.World) == 1);
	TestTrue(TEXT("Door ghost snaps to X edge (1, 0) and is valid"), Preview->GetActorLocation().Equals(FVector(300.0, 0.0, 0.0), 0.1)
		&& B5YawIs(Preview, 0.0) && Placement->GetPreviewResult().IsSuccess());
	TestTrue(TEXT("Door ghost: the arrow points to the facing (+Y) side, where the door will open"), ArrowAlong(Preview->GetActorRightVector())
		&& ArrowAlong(FVector(0.0, 1.0, 0.0)));

	// The ghost composes the hinge: every real door mesh has a ghost mesh with the same world bounds.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABuildingTestDoorActor* Reference = Fixture.World->SpawnActor<ABuildingTestDoorActor>(ABuildingTestDoorActor::StaticClass(),
		Preview->GetActorTransform(), Params);
	if (TestNotNull(TEXT("Reference door"), Reference))
	{
		TInlineComponentArray<UStaticMeshComponent*> RealMeshes(Reference);
		TInlineComponentArray<UStaticMeshComponent*> GhostMeshes(Preview);
		int32 Matched = 0;
		for (const UStaticMeshComponent* Real : RealMeshes)
		{
			const FBox RealBox = Real->Bounds.GetBox();
			for (const UStaticMeshComponent* Ghost : GhostMeshes)
			{
				const FBox GhostBox = Ghost->Bounds.GetBox();
				if (GhostBox.Min.Equals(RealBox.Min, 0.5) && GhostBox.Max.Equals(RealBox.Max, 0.5))
				{
					++Matched;
					break;
				}
			}
		}
		const FBox GhostLeaf = Reference->GetDoorLeaf()->Bounds.GetBox();
		TestTrue(FString::Printf(TEXT("Every door mesh, including the hinged leaf (%s), has a matching ghost mesh (%d/%d)"), *GhostLeaf.ToString(),
			Matched, RealMeshes.Num()), RealMeshes.Num() == 4 && Matched == 4);
		Reference->Destroy();
	}

	TestFalse(TEXT("Missing index 3 is ignored"), Placement->SelectBuildingIndex(3));
	TestFalse(TEXT("Negative index is ignored"), Placement->SelectBuildingIndex(-1));
	TestTrue(TEXT("The selection is unchanged"), Placement->GetSelectedBuildingIndex() == 2 && Preview->GetPreviewMeshCount() == 4);

	// R: quarter turns wrap 0 -> 1 -> 2 -> 3 -> 0; on an edge, odd turns flip the facing.
	TArray<int32> Rotations;
	TArray<bool> Flipped;
	TArray<bool> ArrowFlipped;
	for (int32 Step = 0; Step < 4; ++Step)
	{
		Placement->RotatePlacement();
		Rotations.Add(Placement->GetPlacementRotation());
		Flipped.Add(B5YawIs(Preview, 180.0));
		ArrowFlipped.Add(ArrowAlong(FVector(0.0, -1.0, 0.0)) && ArrowAlong(Preview->GetActorRightVector()));
	}
	TestTrue(TEXT("Rotation wraps 1, 2, 3, 0"), Rotations == TArray<int32>({ 1, 2, 3, 0 }));
	TestTrue(TEXT("The ghost follows at once: flipped on odd turns only"), Flipped == TArray<bool>({ true, false, true, false }));
	TestTrue(TEXT("The arrow turns with the ghost: it points to world -Y on odd turns"), ArrowFlipped == TArray<bool>({ true, false, true, false }));
	TestEqual(TEXT("NeedsFloor reason text"), UBuildingPlacementComponent::DescribeResult(EBuildResult::NeedsFloor).ToString(),
		FString(TEXT("Needs a floor")));

	// Confirm builds the selected building at the selected rotation.
	Placement->RotatePlacement();
	const FBuildingConstructionResult Built = Placement->ConfirmPlacement();
	B2Expect(*this, TEXT("Confirm the rotated door"), Built, EBuildResult::Success);
	TestTrue(TEXT("A test door on X edge (1, 0) at yaw 180"), Cast<ABuildingTestDoorActor>(Built.Piece.Get())
		&& Fixture.Subsystem->GetOccupyingPiece(B5EdgeKey(EBuildingEdgeAxis::X, 1, 0)) == Built.Piece->FindComponentByClass<UBuildingPieceComponent>()
		&& B5YawIs(Built.Piece.Get(), 180.0));

	TestTrue(TEXT("Select index 1 (Wall)"), Placement->SelectBuildingIndex(1) && Placement->GetSelectedBuilding() == Wall
		&& Preview->GetPreviewMeshCount() == 1 && HudName() == TEXT("Test Wall"));
	B4Tick(Placement);
	B2ExpectBranch(*this, TEXT("The wall ghost on the door's edge is red"), Placement->GetPreviewResult(), EBuildResult::Blocked,
		Built.Piece.IsValid() ? *Built.Piece->GetName() : TEXT("<no door>"));

	// The selection persists across exit and enter; selecting outside the mode creates nothing.
	Placement->ExitBuildingMode();
	TestTrue(TEXT("Select outside the mode: accepted, no ghost"), Placement->SelectBuildingIndex(2) && B4CountPreviews(Fixture.World) == 0);
	Placement->EnterBuildingMode();
	TestTrue(TEXT("Re-entered with the door still selected"), Placement->GetSelectedBuildingIndex() == 2 && Placement->GetPreviewActor()
		&& Placement->GetPreviewActor()->GetPreviewMeshCount() == 4 && HudName() == TEXT("Test Swing Door"));
	Placement->ExitBuildingMode();
	TestEqual(TEXT("One door built"), Fixture.LivePieces(), 1);

	// Composition order: with a turned and scaled hinge, only child-then-parent composition matches the real leaf.
	const FTransform Turned(FRotator(0.0, 90.0, 0.0), FVector(700.0, -500.0, 0.0));
	ABuildingTestTurnedHingeDoorActor* TurnedDoor = Fixture.World->SpawnActor<ABuildingTestTurnedHingeDoorActor>(
		ABuildingTestTurnedHingeDoorActor::StaticClass(), Turned, Params);
	ABuildingPreviewActor* TurnedGhost = Fixture.World->SpawnActor<ABuildingPreviewActor>(ABuildingPreviewActor::StaticClass(), Turned, Params);
	if (TestTrue(TEXT("Turned-hinge door and ghost spawn"), TurnedDoor && TurnedGhost))
	{
		// With a material the arrow takes the ghost's tint; re-initialising without one clears it.
		UMaterialInterface* GhostMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/LevelPrototyping/BuildingSystem/Materials/M_BuildPreview.M_BuildPreview"));
		TurnedGhost->InitializeFromPieceClass(ABuildingTestTurnedHingeDoorActor::StaticClass(), GhostMaterial);
		const TArray<const UStaticMeshComponent*> ArrowParts = B5ArrowMeshes(*TurnedGhost);
		const UMaterialInstanceDynamic* Tint = TurnedGhost->GetPreviewMaterial();
		TestTrue(TEXT("The arrow is tinted with the ghost's material"), GhostMaterial && Tint && ArrowParts.Num() == 2
			&& ArrowParts[0]->GetMaterial(0) == Tint && ArrowParts[1]->GetMaterial(0) == Tint);
		TurnedGhost->InitializeFromPieceClass(ABuildingTestTurnedHingeDoorActor::StaticClass(), nullptr);
		TestTrue(TEXT("Without a material the arrow drops the old tint"), ArrowParts.Num() == 2 && Tint
			&& ArrowParts[0]->GetMaterial(0) != Tint && ArrowParts[1]->GetMaterial(0) != Tint);
		const FBox RealLeaf = TurnedDoor->GetDoorLeaf()->Bounds.GetBox();
		bool bLeafMatched = false;
		TInlineComponentArray<UStaticMeshComponent*> TurnedGhostMeshes(TurnedGhost);
		for (const UStaticMeshComponent* Ghost : TurnedGhostMeshes)
		{
			const FBox GhostBox = Ghost->Bounds.GetBox();
			bLeafMatched |= GhostBox.Min.Equals(RealLeaf.Min, 0.5) && GhostBox.Max.Equals(RealLeaf.Max, 0.5);
		}
		TestTrue(FString::Printf(TEXT("The ghost leaf matches the real leaf under a turned, scaled hinge (%s)"), *RealLeaf.ToString()),
			TurnedGhost->GetPreviewMeshCount() == 4 && bLeafMatched);
		TurnedDoor->Destroy();
		TurnedGhost->Destroy();
	}
	Fixture.Clean(*this, TEXT("Selection and rotation"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB5ProductionInputTest,
	"NullTide.Building.B5.ProductionInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB5ProductionInputTest::RunTest(const FString& Parameters)
{
	const FB5Assets Assets = FB5Assets::Load();
	if (!Assets.IsValid(*this)) { return false; }
	const TCHAR* InputRoot = TEXT("/Game/LevelPrototyping/BuildingSystem/Input/");
	auto LoadAction = [InputRoot](const TCHAR* Name) { return LoadObject<UInputAction>(nullptr, *FString::Printf(TEXT("%s%s.%s"), InputRoot, Name, Name)); };
	const UInputMappingContext* ModeContext = LoadObject<UInputMappingContext>(nullptr,
		TEXT("/Game/LevelPrototyping/BuildingSystem/Input/IMC_BuildingMode.IMC_BuildingMode"));
	const UInputAction* Confirm = LoadAction(TEXT("IA_BuildConfirm"));
	const UInputAction* Cancel = LoadAction(TEXT("IA_BuildCancel"));
	const UInputAction* Rotate = LoadAction(TEXT("IA_BuildRotate"));
	const UInputAction* Select1 = LoadAction(TEXT("IA_BuildSelect1"));
	const UInputAction* Select2 = LoadAction(TEXT("IA_BuildSelect2"));
	const UInputAction* Select3 = LoadAction(TEXT("IA_BuildSelect3"));
	const UInputAction* Demolish = LoadAction(TEXT("IA_BuildDemolish"));
	if (!TestTrue(TEXT("IMC_BuildingMode and the building actions load"), ModeContext && Confirm && Cancel && Rotate && Select1 && Select2 && Select3
		&& Demolish))
	{
		return false;
	}
	for (const UInputAction* Action : { Rotate, Select1, Select2, Select3, Demolish })
	{
		TestTrue(FString::Printf(TEXT("%s: digital, consumes its keys, no triggers"), *Action->GetName()),
			Action->ValueType == EInputActionValueType::Boolean && Action->bConsumeInput && Action->Triggers.IsEmpty());
	}
	TMap<FKey, const UInputAction*> Mode;
	bool bPlain = true;
	for (const FEnhancedActionKeyMapping& Mapping : ModeContext->GetMappings())
	{
		Mode.Add(Mapping.Key, Mapping.Action);
		bPlain &= Mapping.Triggers.IsEmpty() && Mapping.Modifiers.IsEmpty();
	}
	TestTrue(TEXT("IMC_BuildingMode: no per-mapping triggers or modifiers"), bPlain);
	// B6 adds C -> IA_BuildDemolish (decision B6-1).
	// The exact key set (B7 adds 4 and 5) is asserted in B7.ProductionInput.
	TestTrue(TEXT("IMC_BuildingMode: LMB, RMB, I, Tab, F, R, 1, 2, 3, C to the right actions"), ModeContext->GetMappings().Num() >= 10
		&& Mode.FindRef(EKeys::LeftMouseButton) == Confirm && Mode.FindRef(EKeys::RightMouseButton) == Cancel
		&& Mode.FindRef(EKeys::I) == Cancel && Mode.FindRef(EKeys::Tab) == Cancel && Mode.FindRef(EKeys::F) == Cancel
		&& Mode.FindRef(EKeys::R) == Rotate && Mode.FindRef(EKeys::One) == Select1 && Mode.FindRef(EKeys::Two) == Select2
		&& Mode.FindRef(EKeys::Three) == Select3 && Mode.FindRef(EKeys::C) == Demolish);

	UBlueprint* PlayerBlueprint = LoadObject<UBlueprint>(nullptr, B3PlayerPath);
	if (!TestNotNull(TEXT("Production player Blueprint loads"), PlayerBlueprint)) { return false; }
	const UConstructionComponent* Construction = nullptr;
	const UBuildingPlacementComponent* Placement = nullptr;
	if (const USimpleConstructionScript* ConstructionScript = PlayerBlueprint->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : ConstructionScript->GetAllNodes())
		{
			Construction = Construction ? Construction : (Node ? Cast<UConstructionComponent>(Node->ComponentTemplate) : nullptr);
			Placement = Placement ? Placement : (Node ? Cast<UBuildingPlacementComponent>(Node->ComponentTemplate) : nullptr);
		}
	}
	if (!TestTrue(TEXT("Production player has construction and placement components"), Construction && Placement)) { return false; }
	const FB2Buildings Registered = Construction->GetAvailableBuildings();
	TestTrue(TEXT("Floor, Wall, Door are registered first, in that order (B7 appends more)"),
		Registered.Num() >= 3 && Registered[0] == Assets.Base.Floor && Registered[1] == Assets.Wall && Registered[2] == Assets.Door);
	TestTrue(TEXT("Placement rotates with IA_BuildRotate"), B4ReadObject<UInputAction>(Placement, TEXT("RotateAction")) == Rotate);
	TestTrue(TEXT("Placement demolishes with IA_BuildDemolish"), B4ReadObject<UInputAction>(Placement, TEXT("DemolishAction")) == Demolish);
	const FArrayProperty* SelectProperty = FindFProperty<FArrayProperty>(UBuildingPlacementComponent::StaticClass(), TEXT("SelectBuildingActions"));
	const TArray<TObjectPtr<UInputAction>>* SelectActions = SelectProperty
		? SelectProperty->ContainerPtrToValuePtr<TArray<TObjectPtr<UInputAction>>>(Placement) : nullptr;
	TestTrue(TEXT("Placement selects with IA_BuildSelect1/2/3 first, in order"), SelectActions && SelectActions->Num() >= 3
		&& (*SelectActions)[0] == Select1 && (*SelectActions)[1] == Select2 && (*SelectActions)[2] == Select3);

	// B5-R2: the interaction prompt reads its key from IMC_Default, not from the live (possibly consumed) mappings.
	const UInputMappingContext* DefaultContext = LoadObject<UInputMappingContext>(nullptr, B5DefaultContextPath);
	const UInputAction* Interact = LoadObject<UInputAction>(nullptr, B5InteractActionPath);
	if (TestTrue(TEXT("IMC_Default and IA_Interact load"), DefaultContext && Interact))
	{
		const FKey PromptKey = UInputPromptLibrary::GetKeyMappedToAction(DefaultContext, Interact);
		TestTrue(FString::Printf(TEXT("The prompt key for IA_Interact is F (got %s)"), *PromptKey.ToString()), PromptKey == EKeys::F);
		TestFalse(TEXT("An action the context does not map has no prompt key"),
			UInputPromptLibrary::GetKeyMappedToAction(DefaultContext, Rotate).IsValid());
	}
	return true;
}


// ---- B6: demolition and refund ----

namespace
{
TSubclassOf<UBuildingDefinition> B6Tiny() { return UBuildingTestTinyFloorDefinition::StaticClass(); }
TSubclassOf<UBuildingDefinition> B6Wall() { return UBuildingTestSupportedWallDefinition::StaticClass(); }

const FVector B6CellE(500.0, 300.0, 0.0);      // Cell (2, 1), east of cell A
const FVector B6EdgeY11(205.0, 300.0, 0.0);    // Y edge (1, 1): west of cell A
const FVector B6EdgeX12(300.0, 395.0, 0.0);    // X edge (1, 2): north of cell A
const FVector B6EdgeY21(405.0, 300.0, 0.0);    // Y edge (2, 1): between cell A and cell E
const FVector B6EdgeX1m1(300.0, -205.0, 0.0);  // X edge (1, -1): north of cell B

bool B6Expect(FAutomationTestBase& Test, const FString& What, const FBuildingDemolishResult& Result, const EBuildResult Expected)
{
	const bool bMatch = Result.Result == Expected;
	Test.TestTrue(FString::Printf(TEXT("%s: %s (got %s, %d pieces, %s)"), *What, *B2Name(Expected), *B2Name(Result.Result),
		Result.Pieces.Num(), *Result.Diagnostic), bMatch);
	return bMatch;
}

int32 B6RefundOf(const TArray<FInventoryDefinitionQuantity>& Refund, TSubclassOf<UItemDefinition> Item)
{
	int32 Quantity = 0;
	for (const FInventoryDefinitionQuantity& Line : Refund)
	{
		Quantity += Line.DefinitionClass == Item ? Line.Quantity : 0;
	}
	return Quantity;
}

ABuildingPieceActor* B6Build(FAutomationTestBase& Test, const FB2Builder& Builder, TSubclassOf<UBuildingDefinition> Definition,
	const FVector& Point, int32 Rotation = 0)
{
	const FBuildingConstructionResult Result = Builder.Construction->TryBuild(Definition, B5Place(Definition, Point, Rotation));
	B2Expect(Test, FString::Printf(TEXT("Build %s at %s"), *GetNameSafe(Definition.Get()), *Point.ToCompactString()), Result, EBuildResult::Success);
	return Cast<ABuildingPieceActor>(Result.Piece.Get());
}

bool B6Alive(const AActor* Actor)
{
	return IsValid(Actor) && !Actor->IsActorBeingDestroyed();
}

bool B6ActiveAlive(const ABuildingPieceActor* Piece)
{
	return B6Alive(Piece) && Piece->GetBuildingPieceComponent() && Piece->GetBuildingPieceComponent()->IsActivePiece();
}

bool B6Contains(const FBuildingDemolishResult& Result, const AActor* Actor)
{
	return Result.Pieces.ContainsByPredicate([Actor](const TWeakObjectPtr<AActor>& Piece) { return Piece.Get() == Actor; });
}

/** Fills every free inventory slot with single (non-stackable) items. */
void B6FillInventory(UInventoryComponent* Inventory)
{
	for (int32 Guard = 0; Guard < 64 && !Inventory->IsFull(); ++Guard)
	{
		Inventory->TryAddDefinitionQuantity(UInventoryTestItemDefinition::StaticClass(), 1);
	}
}

/** An authored (not player-built) Active floor piece on a cell. */
ABuildingTestPieceActor* B6SpawnAuthoredFloor(FB2World& Fixture, const FVector& Cell)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FBuildingPlacement Placement = B2Floor(Cell);
	ABuildingTestPieceActor* Piece = Fixture.World->SpawnActor<ABuildingTestPieceActor>(ABuildingTestPieceActor::StaticClass(),
		Placement.WorldTransform, Params);
	if (Piece)
	{
		Piece->GetBuildingPieceComponent()->InitializePiece(B2FloorClass(), Placement, { B2Key(Cell) }, EBuildingPieceState::Active, false);
	}
	return Piece;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB6DemolishRulesTest,
	"NullTide.Building.B6.DemolishRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB6DemolishRulesTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { B2FloorClass() }, 8);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid())) { return false; }
	ABuildingPieceActor* Floor = B6Build(*this, Builder, B2FloorClass(), B2CellA);
	ABuildingTestPieceActor* Authored = B6SpawnAuthoredFloor(Fixture, B2CellB);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* NotAPiece = Fixture.World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(500.0, 100.0, 0.0)), Params);
	if (!TestTrue(TEXT("Pieces set up"), Floor && Authored && NotAPiece)) { return false; }

	const FB2Counts Before = FB2Counts::Capture(Builder);
	const int32 PiecesBefore = Fixture.LivePieces();
	int32 Requests = 0;
	auto Check = [&](const TCHAR* Name, AActor* Target, const EBuildResult Expected, const TCHAR* Fragment)
	{
		const FBuildingDemolishResult Preflight = Builder.Construction->CanDemolish(Target);
		const FBuildingDemolishResult Attempt = Builder.Construction->TryDemolish(Target);
		++Requests;
		B6Expect(*this, FString(Name) + TEXT(" (CanDemolish)"), Preflight, Expected);
		B6Expect(*this, FString(Name) + TEXT(" (TryDemolish)"), Attempt, Expected);
		TestTrue(FString::Printf(TEXT("%s: diagnostic names the rule \"%s\" (got \"%s\")"), Name, Fragment, *Attempt.Diagnostic),
			Attempt.Diagnostic.Contains(Fragment));
	};
	Check(TEXT("Null"), nullptr, EBuildResult::NotDemolishable, TEXT("not a building piece"));
	Check(TEXT("An actor without a piece"), NotAPiece, EBuildResult::NotDemolishable, TEXT("not a building piece"));
	Check(TEXT("An authored piece"), Authored, EBuildResult::NotDemolishable, TEXT("not built by a player"));
	Floor->GetBuildingPieceComponent()->SetPieceState(EBuildingPieceState::Provisional);
	Check(TEXT("A provisional piece"), Floor, EBuildResult::NotDemolishable, TEXT("not an Active piece"));
	Floor->GetBuildingPieceComponent()->SetPieceState(EBuildingPieceState::Active);
	Builder.Pawn->SetActorLocation(FVector(-300.0, 0.0, 92.0));
	Check(TEXT("Out of range"), Floor, EBuildResult::OutOfRange, TEXT("farther than"));
	Builder.Pawn->SetActorLocation(FVector(0.0, 0.0, 92.0));

	B2ExpectDelta(*this, TEXT("Refusals never touch the inventory"), Before, Builder, 0, 0, 0, 0);
	TestEqual(TEXT("One demolish result per refused request"), Builder.Listener->DemolishResults.Num(), Requests);
	TestTrue(TEXT("Every piece is still there and Active"), Fixture.LivePieces() == PiecesBefore && B6ActiveAlive(Floor) && B6ActiveAlive(Authored));

	const FBuildingDemolishResult Ready = Builder.Construction->CanDemolish(Floor);
	B6Expect(*this, TEXT("A player-built floor in range"), Ready, EBuildResult::Success);
	TestTrue(TEXT("CanDemolish lists the piece and its refund and changes nothing"), Ready.Pieces.Num() == 1 && Ready.Pieces[0].Get() == Floor
		&& B6RefundOf(Ready.Refund, B2Wood()) == 4 && Ready.Definition == B2FloorClass() && B6ActiveAlive(Floor)
		&& Builder.Listener->DemolishResults.Num() == Requests);
	B2ExpectDelta(*this, TEXT("CanDemolish never touches the inventory"), Before, Builder, 0, 0, 0, 0);
	Fixture.Clean(*this, TEXT("Demolish rules"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB6RefundRoundingTest,
	"NullTide.Building.B6.RefundRounding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB6RefundRoundingTest::RunTest(const FString& Parameters)
{
	const FB5Assets Assets = FB5Assets::Load();
	if (!Assets.IsValid(*this)) { return false; }
	const TArray<FInventoryDefinitionQuantity> FloorRefund = UConstructionComponent::ComputeRefund(*Assets.Base.Floor->GetDefaultObject<UBuildingDefinition>());
	const TArray<FInventoryDefinitionQuantity> WallRefund = UConstructionComponent::ComputeRefund(*Assets.Wall->GetDefaultObject<UBuildingDefinition>());
	const TArray<FInventoryDefinitionQuantity> DoorRefund = UConstructionComponent::ComputeRefund(*Assets.Door->GetDefaultObject<UBuildingDefinition>());
	TestTrue(TEXT("Wooden Floor refunds its full cost, Wood 4"), FloorRefund.Num() == 1 && B6RefundOf(FloorRefund, Assets.Base.Wood) == 4);
	TestTrue(TEXT("Wooden Wall refunds Wood 6"), WallRefund.Num() == 1 && B6RefundOf(WallRefund, Assets.Base.Wood) == 6);
	TestTrue(TEXT("Wooden Door refunds Wood 8 and Scrap 2"), DoorRefund.Num() == 2 && B6RefundOf(DoorRefund, Assets.Base.Wood) == 8
		&& B6RefundOf(DoorRefund, Assets.Scrap) == 2);
	TestTrue(TEXT("A partial refund of Wood x 1 (50%) rounds to nothing"), UConstructionComponent::ComputeRefund(*GetDefault<UBuildingTestTinyFloorDefinition>()).IsEmpty());

	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { B6Tiny(), Assets.Base.Floor, Assets.Wall }, 1);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	Builder.Inventory->TryAddDefinitionQuantity(Assets.Base.Wood, 10);

	// A refund of nothing: destroyed without a transaction.
	ABuildingPieceActor* Tiny = B6Build(*this, Builder, B6Tiny(), B2CellA);
	FB2Counts Before = FB2Counts::Capture(Builder);
	const FBuildingDemolishResult TinyResult = Builder.Construction->TryDemolish(Tiny);
	B6Expect(*this, TEXT("Demolish the tiny floor"), TinyResult, EBuildResult::Success);
	TestTrue(TEXT("No refund, the piece is gone and its cell is free"), TinyResult.Refund.IsEmpty() && !B6Alive(Tiny)
		&& Fixture.Subsystem->IsKeyFree(B2Key(B2CellA)));
	B2ExpectDelta(*this, TEXT("A refund of nothing runs no transaction"), Before, Builder, 0, 0, 0, 0);

	// A production wall: Wood +6 in one revision; the floor it stood on stays.
	ABuildingPieceActor* Floor = B6Build(*this, Builder, Assets.Base.Floor, B2CellB);
	ABuildingPieceActor* Wall = B6Build(*this, Builder, Assets.Wall, B6EdgeX1m1);
	const int32 WoodBefore = Builder.Inventory->GetTotalQuantity(Assets.Base.Wood);
	const int32 RevisionBefore = Builder.Inventory->GetRevision();
	const FBuildingDemolishResult WallResult = Builder.Construction->TryDemolish(Wall);
	B6Expect(*this, TEXT("Demolish the production wall"), WallResult, EBuildResult::Success);
	TestTrue(FString::Printf(TEXT("Wood +6 in one revision (wood %d -> %d)"), WoodBefore, Builder.Inventory->GetTotalQuantity(Assets.Base.Wood)),
		Builder.Inventory->GetTotalQuantity(Assets.Base.Wood) == WoodBefore + 6 && Builder.Inventory->GetRevision() == RevisionBefore + 1);
	TestTrue(TEXT("Only the wall went; its edge is free"), !B6Alive(Wall) && B6ActiveAlive(Floor) && WallResult.Pieces.Num() == 1
		&& Fixture.Subsystem->IsKeyFree(B5EdgeKey(EBuildingEdgeAxis::X, 1, -1)));
	TestEqual(TEXT("One demolish result per request"), Builder.Listener->DemolishResults.Num(), 2);
	Fixture.Clean(*this, TEXT("Refund rounding"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB6RefundDoesNotFitTest,
	"NullTide.Building.B6.RefundDoesNotFit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB6RefundDoesNotFitTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	// Exactly the cost of a floor and a wall, so no Wood stack is left to take the refund.
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { B2FloorClass(), B6Wall() }, 10);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	ABuildingPieceActor* Floor = B6Build(*this, Builder, B2FloorClass(), B2CellA);
	ABuildingPieceActor* Wall = B6Build(*this, Builder, B6Wall(), B5EdgeX11);
	// One Wood stack of 44 (stacks hold 50) and no free slot: Wood 6 still fits, Wood 10 does not.
	Builder.Inventory->TryAddDefinitionQuantity(B2Wood(), 44);
	B6FillInventory(Builder.Inventory);
	if (!TestTrue(TEXT("Wood 44 in one stack and every slot taken"), Builder.Inventory->GetTotalQuantity(B2Wood()) == 44
		&& Builder.Inventory->IsFull()))
	{
		return false;
	}

	const FB2Counts Before = FB2Counts::Capture(Builder);
	const FBuildingDemolishResult WallPreview = Builder.Construction->CanDemolish(Wall);
	B6Expect(*this, TEXT("The wall alone: its Wood 6 fits the stack"), WallPreview, EBuildResult::Success);
	B6Expect(*this, TEXT("Floor with its wall (CanDemolish)"), Builder.Construction->CanDemolish(Floor), EBuildResult::RefundDoesNotFit);
	const FBuildingDemolishResult FloorResult = Builder.Construction->TryDemolish(Floor);
	B6Expect(*this, TEXT("Floor with its wall: the summed Wood 10 does not fit"), FloorResult, EBuildResult::RefundDoesNotFit);
	TestTrue(TEXT("The refused result names the summed refund, Wood 4 + 6 = 10"), B6RefundOf(FloorResult.Refund, B2Wood()) == 10
		&& FloorResult.Pieces.Num() == 2);
	B2ExpectDelta(*this, TEXT("Refused demolition changes nothing"), Before, Builder, 0, 0, 0, 0);
	TestTrue(TEXT("Both pieces stay Active and registered"), B6ActiveAlive(Floor) && B6ActiveAlive(Wall)
		&& Fixture.Subsystem->GetOccupyingPiece(B5EdgeKey(EBuildingEdgeAxis::X, 1, 1)) == Wall->GetBuildingPieceComponent()
		&& Fixture.PieceAt(B2CellA) == Floor);
	TestEqual(TEXT("One result for the refused request"), Builder.Listener->DemolishResults.Num(), 1);

	// One free slot takes the overflow: one transaction refunds both pieces.
	Builder.Inventory->TryApplyDefinitionTransaction({ B2Cost(UInventoryTestItemDefinition::StaticClass(), 1) }, {});
	const FB2Counts Freed = FB2Counts::Capture(Builder);
	B6Expect(*this, TEXT("Floor with its wall after freeing a slot"), Builder.Construction->TryDemolish(Floor), EBuildResult::Success);
	B2ExpectDelta(*this, TEXT("One transaction refunds both pieces"), Freed, Builder, 1, 1, 0, 10);
	TestTrue(TEXT("Floor and wall are gone"), !B6Alive(Floor) && !B6Alive(Wall));
	Fixture.Clean(*this, TEXT("Refund does not fit"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB6ExactlyOnceAndRegistryTest,
	"NullTide.Building.B6.ExactlyOnceAndRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB6ExactlyOnceAndRegistryTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { B2FloorClass() }, 12);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid())) { return false; }
	TArray<FBuildingDemolishResult>& Results = Builder.Listener->DemolishResults;

	ABuildingPieceActor* Floor = B6Build(*this, Builder, B2FloorClass(), B2CellA);
	FB2Counts Before = FB2Counts::Capture(Builder);
	B6Expect(*this, TEXT("Demolish"), Builder.Construction->TryDemolish(Floor), EBuildResult::Success);
	B2ExpectDelta(*this, TEXT("One revision, one event, Wood +4"), Before, Builder, 1, 1, 0, 4);
	TestTrue(TEXT("Exactly one result; the piece is gone; its cell is free; the registry is clean"), Results.Num() == 1 && Results[0].IsSuccess()
		&& !B6Alive(Floor) && Fixture.Subsystem->IsKeyFree(B2Key(B2CellA)) && Fixture.Clean(*this, TEXT("After demolition")));
	ABuildingPieceActor* Rebuilt = B6Build(*this, Builder, B2FloorClass(), B2CellA);

	// Nested requests from the refund broadcast neither run nor notify.
	FBuildingDemolishResult Nested;
	FBuildingConstructionResult NestedBuild;
	Builder.Listener->InventoryHook = [&Builder, &Nested, &NestedBuild, Rebuilt]()
	{
		Nested = Builder.Construction->TryDemolish(Rebuilt);
		NestedBuild = Builder.Construction->TryBuild(B2FloorClass(), B2Floor(B2CellB));
	};
	Before = FB2Counts::Capture(Builder);
	B6Expect(*this, TEXT("Demolish with nested requests"), Builder.Construction->TryDemolish(Rebuilt), EBuildResult::Success);
	Builder.Listener->InventoryHook = nullptr;
	B6Expect(*this, TEXT("Nested demolish"), Nested, EBuildResult::BuildInProgress);
	B2Expect(*this, TEXT("Nested build"), NestedBuild, EBuildResult::BuildInProgress);
	B2ExpectDelta(*this, TEXT("Still one transaction and no build result"), Before, Builder, 1, 1, 0, 4);
	TestTrue(TEXT("One more result only; nothing was built on cell B"), Results.Num() == 2 && !B6Alive(Rebuilt)
		&& Fixture.Subsystem->IsKeyFree(B2Key(B2CellB)));

	// A listener that destroys the piece during the refund: still Success, refunded once.
	ABuildingPieceActor* Doomed = B6Build(*this, Builder, B2FloorClass(), B2CellA);
	Builder.Listener->InventoryHook = [Doomed]() { if (IsValid(Doomed)) { Doomed->Destroy(); } };
	Before = FB2Counts::Capture(Builder);
	B6Expect(*this, TEXT("Demolish while a listener destroys the piece"), Builder.Construction->TryDemolish(Doomed), EBuildResult::Success);
	Builder.Listener->InventoryHook = nullptr;
	B2ExpectDelta(*this, TEXT("Refunded exactly once"), Before, Builder, 1, 1, 0, 4);
	TestTrue(TEXT("Three results; the piece is gone"), Results.Num() == 3 && !B6Alive(Doomed));
	Fixture.Clean(*this, TEXT("Exactly once and registry"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB6CascadeTest,
	"NullTide.Building.B6.Cascade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB6CascadeTest::RunTest(const FString& Parameters)
{
	const FB5Assets Assets = FB5Assets::Load();
	if (!Assets.IsValid(*this)) { return false; }
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { Assets.Base.Floor, Assets.Wall, Assets.Door }, 0);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	UInventoryComponent* Inventory = Builder.Inventory;
	Inventory->TryAddDefinitionQuantity(Assets.Base.Wood, 60);
	Inventory->TryAddDefinitionQuantity(Assets.Scrap, 2);

	// Floor A carries three edge pieces with no other floor beside them; the fourth wall also stands on floor E.
	ABuildingPieceActor* FloorA = B6Build(*this, Builder, Assets.Base.Floor, B2CellA);
	ABuildingPieceActor* FloorE = B6Build(*this, Builder, Assets.Base.Floor, B6CellE);
	ABuildingPieceActor* South = B6Build(*this, Builder, Assets.Wall, B5EdgeX11);
	ABuildingPieceActor* West = B6Build(*this, Builder, Assets.Wall, B6EdgeY11);
	ABuildingPieceActor* NorthDoor = B6Build(*this, Builder, Assets.Door, B6EdgeX12);
	ABuildingPieceActor* Shared = B6Build(*this, Builder, Assets.Wall, B6EdgeY21);
	if (!TestTrue(TEXT("Room built"), FloorA && FloorE && South && West && NorthDoor && Shared)) { return false; }

	const FBuildingDemolishResult Preview = Builder.Construction->CanDemolish(FloorA);
	B6Expect(*this, TEXT("Floor A (CanDemolish)"), Preview, EBuildResult::Success);
	TestTrue(TEXT("The cascade: floor A first, then the south and west walls and the door, not the shared wall"),
		Preview.Pieces.Num() == 4 && Preview.Pieces[0].Get() == FloorA && B6Contains(Preview, South) && B6Contains(Preview, West)
		&& B6Contains(Preview, NorthDoor) && !B6Contains(Preview, Shared));
	TestTrue(FString::Printf(TEXT("Summed full refund Wood 4+6+6+8 = 24, Scrap 2 (got Wood %d, Scrap %d)"), B6RefundOf(Preview.Refund, Assets.Base.Wood),
		B6RefundOf(Preview.Refund, Assets.Scrap)), B6RefundOf(Preview.Refund, Assets.Base.Wood) == 24 && B6RefundOf(Preview.Refund, Assets.Scrap) == 2
		&& Preview.Refund.Num() == 2);

	const int32 WoodBefore = Inventory->GetTotalQuantity(Assets.Base.Wood);
	const int32 ScrapBefore = Inventory->GetTotalQuantity(Assets.Scrap);
	const int32 RevisionBefore = Inventory->GetRevision();
	B6Expect(*this, TEXT("Demolish floor A"), Builder.Construction->TryDemolish(FloorA), EBuildResult::Success);
	TestTrue(TEXT("One revision: Wood +24, Scrap +2"), Inventory->GetTotalQuantity(Assets.Base.Wood) == WoodBefore + 24
		&& Inventory->GetTotalQuantity(Assets.Scrap) == ScrapBefore + 2 && Inventory->GetRevision() == RevisionBefore + 1);
	TestTrue(TEXT("Floor A, both walls and the door are gone; their keys are free"), !B6Alive(FloorA) && !B6Alive(South) && !B6Alive(West)
		&& !B6Alive(NorthDoor) && Fixture.Subsystem->IsKeyFree(B2Key(B2CellA))
		&& Fixture.Subsystem->IsKeyFree(B5EdgeKey(EBuildingEdgeAxis::X, 1, 1)) && Fixture.Subsystem->IsKeyFree(B5EdgeKey(EBuildingEdgeAxis::Y, 1, 1))
		&& Fixture.Subsystem->IsKeyFree(B5EdgeKey(EBuildingEdgeAxis::X, 1, 2)));
	TestTrue(TEXT("The shared wall and floor E stay Active"), B6ActiveAlive(Shared) && B6ActiveAlive(FloorE));
	TestEqual(TEXT("One demolish result for the whole cascade"), Builder.Listener->DemolishResults.Num(), 1);

	// A wall target alone takes nothing with it; floor E then has no dependents.
	const FBuildingDemolishResult WallResult = Builder.Construction->TryDemolish(Shared);
	B6Expect(*this, TEXT("Demolish the shared wall"), WallResult, EBuildResult::Success);
	TestTrue(TEXT("Only the wall went"), WallResult.Pieces.Num() == 1 && !B6Alive(Shared) && B6ActiveAlive(FloorE));
	const FBuildingDemolishResult FloorEResult = Builder.Construction->CanDemolish(FloorE);
	TestTrue(TEXT("Floor E alone: no dependents"), FloorEResult.IsSuccess() && FloorEResult.Pieces.Num() == 1);

	// A dependent that no player built refuses the whole cascade.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FBuildingPlacement AuthoredPlacement = B5Place(B6Wall(), FVector(605.0, 300.0, 0.0));
	ABuildingTestWallPieceActor* AuthoredWall = Fixture.World->SpawnActor<ABuildingTestWallPieceActor>(ABuildingTestWallPieceActor::StaticClass(),
		AuthoredPlacement.WorldTransform, Params);
	if (TestNotNull(TEXT("Authored wall"), AuthoredWall))
	{
		AuthoredWall->GetBuildingPieceComponent()->InitializePiece(B6Wall(), AuthoredPlacement,
			UBuildingSubsystem::ComputeOccupancyKeys(*GetDefault<UBuildingTestSupportedWallDefinition>(), AuthoredPlacement), EBuildingPieceState::Active, false);
		AuthoredWall->DispatchBeginPlay();
		TestTrue(TEXT("The authored wall holds floor E's east edge"),
			Fixture.Subsystem->GetOccupyingPiece(B5EdgeKey(EBuildingEdgeAxis::Y, 3, 1)) == AuthoredWall->GetBuildingPieceComponent());
		const FBuildingDemolishResult Refused = Builder.Construction->TryDemolish(FloorE);
		B6Expect(*this, TEXT("Floor E carrying an authored wall"), Refused, EBuildResult::NotDemolishable);
		TestTrue(FString::Printf(TEXT("The reason names the dependent (%s); nothing removed"), *Refused.Diagnostic),
			Refused.Diagnostic.Contains(TEXT("cannot go with it")) && B6ActiveAlive(FloorE) && B6ActiveAlive(AuthoredWall));
		AuthoredWall->Destroy();
	}

	// Layers: a floor on 16 cm ground is on layer 0, but its top (26 cm) puts the wall on it on layer 1.
	B2SpawnBlocker(Fixture.World, FVector(300.0, -200.0, 8.0), FVector(100.0, 200.0, 8.0));
	ABuildingPieceActor* Raised = B6Build(*this, Builder, Assets.Base.Floor, B2CellB);
	ABuildingPieceActor* RaisedWall = B6Build(*this, Builder, Assets.Wall, B6EdgeX1m1);
	if (TestTrue(TEXT("Raised floor and its wall built"), Raised && RaisedWall))
	{
		TestTrue(FString::Printf(TEXT("Floor on layer 0, wall on layer 1 (got %d and %d)"), Raised->GetBuildingPieceComponent()->GetPlacement().Layer,
			RaisedWall->GetBuildingPieceComponent()->GetPlacement().Layer), Raised->GetBuildingPieceComponent()->GetPlacement().Layer == 0
			&& RaisedWall->GetBuildingPieceComponent()->GetPlacement().Layer == 1);
		const FBuildingDemolishResult RaisedResult = Builder.Construction->TryDemolish(Raised);
		B6Expect(*this, TEXT("Demolish the raised floor"), RaisedResult, EBuildResult::Success);
		TestTrue(TEXT("The wall on the next layer went with it"), RaisedResult.Pieces.Num() == 2 && !B6Alive(Raised) && !B6Alive(RaisedWall));
	}
	Fixture.Clean(*this, TEXT("Cascade"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB6StorageAndStationTest,
	"NullTide.Building.B6.StorageAndStation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB6StorageAndStationTest::RunTest(const FString& Parameters)
{
	// Storage with items is refused, alone or in a cascade; an empty storage is demolished (B7-2).
	{
		FB2World Fixture;
		if (!Fixture.IsValid(*this)) { return false; }
		FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { B2FloorClass(), UBuildingTestStorageWallDefinition::StaticClass() }, 6);
		if (!TestTrue(TEXT("Builder set up"), Builder.IsValid())) { return false; }
		ABuildingPieceActor* Floor = B6Build(*this, Builder, B2FloorClass(), B2CellA);
		ABuildingPieceActor* Storage = B6Build(*this, Builder, UBuildingTestStorageWallDefinition::StaticClass(), B5EdgeX11);
		if (!TestTrue(TEXT("Floor and storage wall built"), Floor && Storage && Storage->FindComponentByClass<UStorageComponent>())) { return false; }
		UStorageComponent* StorageComponent = Storage->FindComponentByClass<UStorageComponent>();
		Builder.Inventory->TryAddDefinitionQuantity(UInventoryTestItemDefinition::StaticClass(), 1);
		const UItemInstance* Kept = Builder.Inventory->GetItemsSnapshot().IsEmpty() ? nullptr : Builder.Inventory->GetItemsSnapshot()[0];
		if (!TestTrue(TEXT("An item moves into the storage"), Kept
			&& UStorageTransferLibrary::TransferInventoryToStorage(Builder.Inventory, StorageComponent, Kept->GetInstanceId()).IsSuccess()))
		{
			return false;
		}
		const FB2Counts Before = FB2Counts::Capture(Builder);
		B6Expect(*this, TEXT("A storage with items (CanDemolish)"), Builder.Construction->CanDemolish(Storage), EBuildResult::StorageNotEmpty);
		B6Expect(*this, TEXT("A storage with items (TryDemolish)"), Builder.Construction->TryDemolish(Storage), EBuildResult::StorageNotEmpty);
		B6Expect(*this, TEXT("The floor under it (the storage is in the cascade)"), Builder.Construction->TryDemolish(Floor), EBuildResult::StorageNotEmpty);
		B2ExpectDelta(*this, TEXT("Storage refusals change nothing"), Before, Builder, 0, 0, 0, 0);
		TestTrue(TEXT("Floor and storage stay Active; the item is still stored"), B6ActiveAlive(Floor) && B6ActiveAlive(Storage)
			&& StorageComponent->GetItemCount() == 1);
		const FGuid StoredId = Kept->GetInstanceId();
		TestTrue(TEXT("The item comes back"), UStorageTransferLibrary::TransferStorageToInventory(StorageComponent, Builder.Inventory, StoredId).IsSuccess()
			&& StorageComponent->GetItemCount() == 0);
		B6Expect(*this, TEXT("The empty storage"), Builder.Construction->TryDemolish(Storage), EBuildResult::Success);
		TestTrue(TEXT("The storage is gone; the floor stays"), !B6Alive(Storage) && B6ActiveAlive(Floor));
		Fixture.Clean(*this, TEXT("Storage"));
	}

	// A workbench-like station: demolition is allowed and cancels the craft at it (StationLost), consuming nothing.
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(100.0, -150.0, 92.0), { UBuildingTestStationDefinition::StaticClass() }, 2);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	UCraftingComponent* Crafting = NewObject<UCraftingComponent>(Builder.Pawn);
	Builder.Pawn->AddInstanceComponent(Crafting);
	Crafting->RegisterComponent();
	FArrayProperty* Recipes = FindFProperty<FArrayProperty>(UCraftingComponent::StaticClass(), TEXT("AvailableRecipes"));
	if (!TestNotNull(TEXT("AvailableRecipes property"), Recipes)) { return false; }
	*Recipes->ContainerPtrToValuePtr<TArray<TSubclassOf<UCraftingRecipeDefinition>>>(Crafting) = { UCraftingStationTimedTestRecipe::StaticClass() };

	ABuildingPieceActor* StationPiece = B6Build(*this, Builder, UBuildingTestStationDefinition::StaticClass(), FVector(100.0, 100.0, 0.0));
	UCraftingStationComponent* Station = StationPiece ? StationPiece->FindComponentByClass<UCraftingStationComponent>() : nullptr;
	FClassProperty* StationKind = FindFProperty<FClassProperty>(UCraftingStationComponent::StaticClass(), TEXT("StationDefinition"));
	if (!TestTrue(TEXT("Station piece built"), Station && StationKind)) { return false; }
	*StationKind->ContainerPtrToValuePtr<TSubclassOf<UCraftingStationDefinition>>(Station) = UCraftingTestStationDefinition::StaticClass();
	// Begun play, so destroying it routes EndPlay and OnEndPlay as in a running game (the C8A setup).
	StationPiece->DispatchBeginPlay();

	Builder.Inventory->TryAddDefinitionQuantity(B2Wood(), 3);
	Builder.Inventory->TryAddDefinitionQuantity(UInventoryTestItemDefinition::StaticClass(), 1);
	const FCraftingTransactionResult Started = Crafting->TryStartCraftAtStation(UCraftingStationTimedTestRecipe::StaticClass(), Station);
	if (!TestTrue(TEXT("A timed craft runs at the station"), Started.Result == ECraftingTransactionResult::Started && Crafting->IsCrafting()))
	{
		return false;
	}
	const int32 WoodBefore = Builder.Inventory->GetTotalQuantity(B2Wood());
	B6Expect(*this, TEXT("Demolish the station"), Builder.Construction->TryDemolish(StationPiece), EBuildResult::Success);
	TestTrue(TEXT("The craft is cancelled and its station cleared"), !Crafting->IsCrafting() && !Crafting->GetActiveCraftStation());
	TestTrue(TEXT("Nothing was consumed or produced; the refund is Wood +2"), Builder.Inventory->GetTotalQuantity(B2Wood()) == WoodBefore + 2
		&& Builder.Inventory->GetTotalQuantity(UInventoryTestItemDefinition::StaticClass()) == 1
		&& Builder.Inventory->GetTotalQuantity(B2Other()) == 0);
	TestFalse(TEXT("The station piece is gone"), B6Alive(StationPiece));
	Fixture.Clean(*this, TEXT("Station"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB6DemolishModeTest,
	"NullTide.Building.B6.DemolishMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB6DemolishModeTest::RunTest(const FString& Parameters)
{
	UClass* HudClass = LoadClass<UBuildingHudWidget>(nullptr, B4HudPath);
	if (!TestNotNull(TEXT("W_BuildingHud loads"), HudClass)) { return false; }
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB4Player Player = B4SpawnPlayer(Fixture, { B2FloorClass(), B6Wall() }, 30);
	if (!TestTrue(TEXT("Player set up"), Player.IsValid())) { return false; }
	Player.Builder.Listener->DemolishResults.Reset();
	UBuildingPlacementTestComponent* Placement = Player.Placement;
	Placement->SetHudClass(HudClass);
	Placement->SetCursor(B2CellC);
	ABuildingPieceActor* Floor = B6Build(*this, Player.Builder, B2FloorClass(), B2CellA);
	ABuildingPieceActor* Wall = B6Build(*this, Player.Builder, B6Wall(), B5EdgeX11);
	ABuildingTestPieceActor* Authored = B6SpawnAuthoredFloor(Fixture, B2CellB);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Ground = Fixture.World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(700.0, 0.0, 0.0)), Params);
	if (!TestTrue(TEXT("Pieces set up"), Floor && Wall && Authored && Ground)) { return false; }
	const TArray<FBuildingDemolishResult>& Results = Player.Builder.Listener->DemolishResults;
	auto Hud = [Placement]() { return Placement->GetHudWidget(); };
	auto Visible = [Placement]() { return Placement->GetVisibleDemolishHighlights(); };
	auto Hover = [Placement](AActor* Actor) { Placement->CursorHitActor = Actor; B4Tick(Placement); };

	Placement->ToggleDemolishMode();
	TestFalse(TEXT("C does nothing outside building mode"), Placement->IsDemolishModeActive());
	if (!TestTrue(TEXT("Enter building mode"), Placement->EnterBuildingMode())) { return false; }
	TestTrue(TEXT("Build hint mentions C"), Hud() && Hud()->GetHintText().ToString().Contains(TEXT("C demolish")));
	Placement->ToggleDemolishMode();
	TestTrue(TEXT("C switches to demolish; the build ghost hides"), Placement->IsDemolishModeActive() && Placement->GetPreviewActor()
		&& Placement->GetPreviewActor()->IsHidden() && Hud()->GetHintText().ToString().Contains(TEXT("LMB demolish")));

	Hover(Floor);
	TArray<ABuildingPreviewActor*> Highlights = Visible();
	bool bOnPieces = Highlights.Num() == 2;
	for (const ABuildingPreviewActor* Highlight : Highlights)
	{
		bOnPieces &= Highlight->GetActorLocation().Equals(Floor->GetActorLocation(), 0.1) || Highlight->GetActorLocation().Equals(Wall->GetActorLocation(), 0.1);
		bOnPieces &= Highlight->GetActorScale3D().Equals(FVector(1.02), 0.001) && !Highlight->GetFacingArrow()->IsVisible()
			&& Highlight->IsPlacementValid();
	}
	TestTrue(FString::Printf(TEXT("The floor and the wall it carries are highlighted (%d)"), Highlights.Num()), bOnPieces);
	TestTrue(FString::Printf(TEXT("HUD: the floor + 1 more, refund 10, ready (%s / %s / %s)"), *Hud()->GetBuildingNameText().ToString(),
		*Hud()->GetCostText().ToString(), *Hud()->GetStatusText().ToString()),
		Hud()->GetBuildingNameText().ToString() == TEXT("Demolish: Test Floor + 1 more") && Hud()->GetCostText().ToString().StartsWith(TEXT("Refund:"))
		&& Hud()->GetCostText().ToString().Contains(TEXT("10")) && Hud()->GetStatusText().ToString() == TEXT("Ready to demolish") && Hud()->IsStatusValid());

	Hover(Wall);
	TestTrue(TEXT("A wall alone: one highlight"), Visible().Num() == 1 && Hud()->GetBuildingNameText().ToString() == TEXT("Demolish: Test Supported Wall"));
	Hover(Authored);
	TestTrue(TEXT("An authored piece: one red highlight and the reason"), Visible().Num() == 1 && !Visible()[0]->IsPlacementValid()
		&& Hud()->GetStatusText().ToString() == TEXT("Can't demolish this") && !Hud()->IsStatusValid());
	Hover(Ground);
	TestTrue(TEXT("Not a building: nothing highlighted"), Visible().IsEmpty() && Hud()->GetStatusText().ToString() == TEXT("Point at a building"));
	Hover(nullptr);
	TestTrue(TEXT("Nothing under the cursor: nothing highlighted"), Visible().IsEmpty());
	TestFalse(TEXT("Confirm over nothing fails"), Placement->ConfirmDemolish().IsSuccess());
	TestEqual(TEXT("No demolish request without a target"), Results.Num(), 0);

	Hover(Wall);
	B6Expect(*this, TEXT("LMB on the wall"), Placement->ConfirmDemolish(), EBuildResult::Success);
	TestTrue(TEXT("One request; the wall is gone; the HUD confirms"), Results.Num() == 1 && !B6Alive(Wall)
		&& Hud()->GetStatusText().ToString() == TEXT("Demolished Test Supported Wall"));
	Hover(Floor);
	TestEqual(TEXT("The floor now carries nothing"), Visible().Num(), 1);

	Placement->ToggleDemolishMode();
	TestTrue(TEXT("C returns to building: highlights hidden, the ghost back, selection kept"), !Placement->IsDemolishModeActive()
		&& Visible().IsEmpty() && !Placement->GetPreviewActor()->IsHidden() && Placement->GetSelectedBuildingIndex() == 0);
	TestFalse(TEXT("Confirm demolish outside demolish mode fails"), Placement->ConfirmDemolish().IsSuccess());
	Placement->ToggleDemolishMode();
	Placement->ExitBuildingMode();
	TestEqual(TEXT("Exit removes the ghost and every highlight"), B4CountPreviews(Fixture.World), 0);
	Placement->EnterBuildingMode();
	TestFalse(TEXT("The next entry starts in building"), Placement->IsDemolishModeActive());
	Placement->ExitBuildingMode();
	TestEqual(TEXT("Exactly one demolish result"), Results.Num(), 1);
	Fixture.Clean(*this, TEXT("Demolish mode"));
	return true;
}


// ---- B7: buildable chest and workbench ----

namespace
{
const TCHAR* B7ChestDefinitionPath = TEXT("/Game/LevelPrototyping/BuildingSystem/Definitions/Building_WoodChest.Building_WoodChest_C");
const TCHAR* B7BenchDefinitionPath = TEXT("/Game/LevelPrototyping/BuildingSystem/Definitions/Building_Workbench.Building_Workbench_C");
const TCHAR* B7ChestPiecePath = TEXT("/Game/LevelPrototyping/BuildingSystem/Pieces/BP_BuildChest.BP_BuildChest_C");
const TCHAR* B7BenchPiecePath = TEXT("/Game/LevelPrototyping/BuildingSystem/Pieces/BP_BuildWorkbench.BP_BuildWorkbench_C");
const TCHAR* B7StonePath = TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_Stone.Item_Stone_C");
const TCHAR* B7StationPath = TEXT("/Game/LevelPrototyping/CraftingSystem/Stations/Station_Workbench.Station_Workbench_C");
const TCHAR* B7SwordRecipePath = TEXT("/Game/LevelPrototyping/CraftingSystem/Recipes/Recipe_ShortSword.Recipe_ShortSword_C");

TSubclassOf<UBuildingDefinition> B7TestChest() { return UBuildingTestChestDefinition::StaticClass(); }
TSubclassOf<UBuildingDefinition> B7TestBench() { return UBuildingTestBenchDefinition::StaticClass(); }

const FVector B7CellD(300.0, 100.0, 0.0);  // Cell (1, 0), south of cell A

struct FB7Assets
{
	FB5Assets B5;
	TSubclassOf<UBuildingDefinition> Chest;
	TSubclassOf<UBuildingDefinition> Bench;
	TSubclassOf<ABuildingStorageActor> ChestPiece;
	TSubclassOf<ABuildingCraftingStationActor> BenchPiece;
	TSubclassOf<UItemDefinition> Stone;
	TSubclassOf<UCraftingStationDefinition> Station;
	TSubclassOf<UCraftingRecipeDefinition> Sword;

	static FB7Assets Load()
	{
		FB7Assets Assets;
		Assets.B5 = FB5Assets::Load();
		Assets.Chest = LoadClass<UBuildingDefinition>(nullptr, B7ChestDefinitionPath);
		Assets.Bench = LoadClass<UBuildingDefinition>(nullptr, B7BenchDefinitionPath);
		Assets.ChestPiece = LoadClass<ABuildingStorageActor>(nullptr, B7ChestPiecePath);
		Assets.BenchPiece = LoadClass<ABuildingCraftingStationActor>(nullptr, B7BenchPiecePath);
		Assets.Stone = LoadClass<UItemDefinition>(nullptr, B7StonePath);
		Assets.Station = LoadClass<UCraftingStationDefinition>(nullptr, B7StationPath);
		Assets.Sword = LoadClass<UCraftingRecipeDefinition>(nullptr, B7SwordRecipePath);
		return Assets;
	}

	bool IsValid(FAutomationTestBase& Test) const
	{
		return B5.IsValid(Test) && Test.TestTrue(TEXT("Chest and workbench definitions and pieces, Item_Stone, Station_Workbench and Recipe_ShortSword load"),
			Chest && Bench && ChestPiece && BenchPiece && Stone && Station && Sword);
	}
};

void B7SetStationKind(UCraftingStationComponent* Station, TSubclassOf<UCraftingStationDefinition> Kind)
{
	FClassProperty* Property = FindFProperty<FClassProperty>(UCraftingStationComponent::StaticClass(), TEXT("StationDefinition"));
	check(Property);
	*Property->ContainerPtrToValuePtr<TSubclassOf<UCraftingStationDefinition>>(Station) = Kind;
}

UCraftingComponent* B7AddCrafting(APawn* Pawn, const TArray<TSubclassOf<UCraftingRecipeDefinition>>& Recipes)
{
	UCraftingComponent* Crafting = NewObject<UCraftingComponent>(Pawn);
	Pawn->AddInstanceComponent(Crafting);
	Crafting->RegisterComponent();
	FArrayProperty* Property = FindFProperty<FArrayProperty>(UCraftingComponent::StaticClass(), TEXT("AvailableRecipes"));
	check(Property);
	*Property->ContainerPtrToValuePtr<TArray<TSubclassOf<UCraftingRecipeDefinition>>>(Crafting) = Recipes;
	return Crafting;
}

/** Adds one single item and moves it into the storage; returns its id (invalid on failure). */
FGuid B7StoreOne(UInventoryComponent* Inventory, UStorageComponent* Storage)
{
	Inventory->TryAddDefinitionQuantity(UInventoryTestItemDefinition::StaticClass(), 1);
	for (const UItemInstance* Item : Inventory->GetItemsSnapshot())
	{
		if (Item && Item->GetDefinitionClass() == UInventoryTestItemDefinition::StaticClass())
		{
			const FGuid Id = Item->GetInstanceId();
			return UStorageTransferLibrary::TransferInventoryToStorage(Inventory, Storage, Id).IsSuccess() ? Id : FGuid();
		}
	}
	return FGuid();
}

template <typename T>
T* B7ReadClassDefault(UClass* Class, const TCHAR* PropertyName)
{
	const FClassProperty* Property = FindFProperty<FClassProperty>(Class, PropertyName);
	return Property ? Cast<T>(Property->GetPropertyValue_InContainer(Class->GetDefaultObject())) : nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB7ProductionDefinitionsTest,
	"NullTide.Building.B7.ProductionDefinitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB7ProductionDefinitionsTest::RunTest(const FString& Parameters)
{
	const FB7Assets Assets = FB7Assets::Load();
	UClass* InteractableClass = LoadClass<UInterface>(nullptr, B5InteractablePath);
	if (!Assets.IsValid(*this) || !TestNotNull(TEXT("BPI_Interactable loads"), InteractableClass)) { return false; }
	const UBuildingDefinition* Chest = Assets.Chest->GetDefaultObject<UBuildingDefinition>();
	const UBuildingDefinition* Bench = Assets.Bench->GetDefaultObject<UBuildingDefinition>();
	FString Diagnostic;
	TestTrue(FString::Printf(TEXT("Building_WoodChest is valid (%s)"), *Diagnostic), Chest->ValidateBuilding(Diagnostic));
	TestTrue(FString::Printf(TEXT("Building_Workbench is valid (%s)"), *Diagnostic), Bench->ValidateBuilding(Diagnostic));
	TestTrue(TEXT("Chest: Wooden Chest, Furniture, GridCell 1 x 1, Wood x 12, full refund, needs a floor, BP_BuildChest"),
		Chest->BuildingName.ToString() == TEXT("Wooden Chest") && Chest->Category == EBuildingCategory::Furniture
		&& Chest->PlacementMode == EBuildingPlacementMode::GridCell && Chest->Footprint == FIntPoint(1, 1) && Chest->Cost.Num() == 1
		&& Chest->Cost[0].DefinitionClass == Assets.B5.Base.Wood && Chest->Cost[0].Quantity == 12 && FMath::IsNearlyEqual(Chest->RefundFraction, 1.f)
		&& Chest->bRequiresFloor && Chest->PieceActorClass == Assets.ChestPiece);
	TestTrue(TEXT("Workbench: Workbench, Station, GridCell 1 x 1, Wood x 20 + Stone x 10, full refund, needs a floor, BP_BuildWorkbench"),
		Bench->BuildingName.ToString() == TEXT("Workbench") && Bench->Category == EBuildingCategory::Station
		&& Bench->PlacementMode == EBuildingPlacementMode::GridCell && Bench->Footprint == FIntPoint(1, 1) && Bench->Cost.Num() == 2
		&& Bench->Cost[0].DefinitionClass == Assets.B5.Base.Wood && Bench->Cost[0].Quantity == 20 && Bench->Cost[1].DefinitionClass == Assets.Stone
		&& Bench->Cost[1].Quantity == 10 && FMath::IsNearlyEqual(Bench->RefundFraction, 1.f) && Bench->bRequiresFloor
		&& Bench->PieceActorClass == Assets.BenchPiece);
#if WITH_EDITOR
	FDataValidationContext Context;
	TestTrue(TEXT("Editor data validation passes"), Chest->IsDataValid(Context) != EDataValidationResult::Invalid
		&& Bench->IsDataValid(Context) != EDataValidationResult::Invalid);
#endif
	TestTrue(TEXT("Both pieces implement BPI_Interactable and add no components"), Assets.ChestPiece->ImplementsInterface(InteractableClass)
		&& Assets.BenchPiece->ImplementsInterface(InteractableClass) && B5AddedComponents(Assets.ChestPiece.Get()) == 0
		&& B5AddedComponents(Assets.BenchPiece.Get()) == 0);
	TestTrue(TEXT("The chest opens W_StorageScreen; the bench opens a crafting screen"),
		B7ReadClassDefault<UClass>(Assets.ChestPiece.Get(), TEXT("StorageScreenClass")) != nullptr
		&& B7ReadClassDefault<UClass>(Assets.BenchPiece.Get(), TEXT("CraftingScreenClass")) != nullptr);

	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABuildingStorageActor* ChestActor = Fixture.World->SpawnActor<ABuildingStorageActor>(Assets.ChestPiece, FVector(300.0, 300.0, 0.0), FRotator::ZeroRotator, Params);
	ABuildingCraftingStationActor* BenchActor = Fixture.World->SpawnActor<ABuildingCraftingStationActor>(Assets.BenchPiece, FVector(300.0, -300.0, 0.0),
		FRotator::ZeroRotator, Params);
	if (!TestTrue(TEXT("Both pieces spawn"), ChestActor && BenchActor)) { return false; }
	B5ExpectPieceContract(*this, TEXT("BP_BuildChest"), *ChestActor);
	B5ExpectPieceContract(*this, TEXT("BP_BuildWorkbench"), *BenchActor);
	TestTrue(TEXT("The chest has a storage titled Wooden Chest and overlap-only bounds"), ChestActor->GetStorageComponent()
		&& ChestActor->GetContainerTitle().ToString() == TEXT("Wooden Chest") && B5OverlapOnly(ChestActor->GetInteractionBounds()));
	TestTrue(TEXT("The bench is a Station_Workbench with overlap-only bounds"), BenchActor->GetCraftingStationComponent()
		&& BenchActor->GetCraftingStationComponent()->GetStationDefinition() == Assets.Station && B5OverlapOnly(BenchActor->GetInteractionBounds()));
	for (AActor* Piece : { static_cast<AActor*>(ChestActor), static_cast<AActor*>(BenchActor) })
	{
		UFunction* GetType = Piece->FindFunction(FName(TEXT("GetInteractionType")));
		TestTrue(FString::Printf(TEXT("%s: interaction type is Press"), *Piece->GetClass()->GetName()), GetType && B5CallEnumGetter(Piece, GetType) == TEXT("Press"));
	}
	Fixture.Clean(*this, TEXT("Production definitions"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB7ChestStorageTest,
	"NullTide.Building.B7.ChestStorage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB7ChestStorageTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { B2FloorClass(), B7TestChest() }, 6);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }

	// The chest needs a floor under it (user decision for B7).
	const FB2Counts Before = FB2Counts::Capture(Builder);
	B2Expect(*this, TEXT("Chest on bare ground"), Builder.Construction->TryBuild(B7TestChest(), B5Place(B7TestChest(), B2CellA)), EBuildResult::NeedsFloor);
	B2ExpectDelta(*this, TEXT("Refused chest: nothing consumed"), Before, Builder, 0, 0, 1, 0);
	ABuildingPieceActor* Floor = B6Build(*this, Builder, B2FloorClass(), B2CellA);
	ABuildingTestChestActor* Chest = Cast<ABuildingTestChestActor>(B6Build(*this, Builder, B7TestChest(), B2CellA));
	if (!TestTrue(TEXT("Chest built on the floor"), Floor && Chest)) { return false; }
	TestTrue(TEXT("The chest stands on the floor's top and holds the cell's furniture key"),
		FMath::IsNearlyEqual(Chest->GetActorLocation().Z, UBuildingSubsystem::FloorThickness, 0.1)
		&& Fixture.Subsystem->GetOccupyingPiece(FBuildingOccupancyKey(EBuildingOccupancyCategory::Furniture, EBuildingEdgeAxis::None, 1, 1, 0))
			== Chest->GetBuildingPieceComponent());

	// Items move in and out through the storage authority, keeping their ItemId.
	UStorageComponent* Storage = Chest->GetStorageComponent();
	const FGuid Stored = B7StoreOne(Builder.Inventory, Storage);
	TestTrue(TEXT("An item moves into the chest"), Stored.IsValid() && Storage->GetItemCount() == 1
		&& Builder.Inventory->GetTotalQuantity(UInventoryTestItemDefinition::StaticClass()) == 0);
	const FInventoryStorageTransferResult Back = UStorageTransferLibrary::TransferStorageToInventory(Storage, Builder.Inventory, Stored);
	TestTrue(TEXT("It comes back with the same ItemId"), Back.IsSuccess() && Back.ItemId == Stored && Storage->GetItemCount() == 0
		&& Builder.Inventory->GetTotalQuantity(UInventoryTestItemDefinition::StaticClass()) == 1);

	// The chest never opens while it is not an Active piece (the read-only gate; opening itself needs a viewport).
	TestTrue(TEXT("An Active chest can be opened"), Chest->CanOpenStorage());
	Chest->GetBuildingPieceComponent()->SetPieceState(EBuildingPieceState::Removing);
	TestTrue(TEXT("A chest being removed cannot be opened, and does not open"), !Chest->CanOpenStorage()
		&& !Chest->OpenStorageForInteractor(Builder.Pawn));
	Chest->GetBuildingPieceComponent()->SetPieceState(EBuildingPieceState::Active);
	Chest->SetProvisional(true);
	TestFalse(TEXT("A provisional chest cannot be opened"), Chest->CanOpenStorage());
	Chest->SetProvisional(false);
	TestTrue(TEXT("Active again: it can be opened"), Chest->CanOpenStorage());
	Fixture.Clean(*this, TEXT("Chest storage"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB7WorkbenchStationTest,
	"NullTide.Building.B7.WorkbenchStation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB7WorkbenchStationTest::RunTest(const FString& Parameters)
{
	const FB7Assets Assets = FB7Assets::Load();
	if (!Assets.IsValid(*this)) { return false; }
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(100.0, -150.0, 92.0), { Assets.B5.Base.Floor, Assets.Bench }, 0);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid() && Builder.Inventory)) { return false; }
	UInventoryComponent* Inventory = Builder.Inventory;
	Inventory->TryAddDefinitionQuantity(Assets.B5.Base.Wood, 24);
	Inventory->TryAddDefinitionQuantity(Assets.Stone, 10);

	// The workbench needs a floor (user decision after the B7 PIE).
	const FVector BenchCell(100.0, 100.0, 0.0);
	B2Expect(*this, TEXT("Workbench on bare ground"), Builder.Construction->TryBuild(Assets.Bench, B5Place(Assets.Bench, BenchCell)), EBuildResult::NeedsFloor);
	B6Build(*this, Builder, Assets.B5.Base.Floor, BenchCell);
	ABuildingCraftingStationActor* Bench = Cast<ABuildingCraftingStationActor>(B6Build(*this, Builder, Assets.Bench, BenchCell));
	if (!TestNotNull(TEXT("Workbench built on the floor"), Bench)) { return false; }
	UCraftingStationComponent* Station = Bench->GetCraftingStationComponent();
	TestTrue(TEXT("Wood 20 and Stone 10 paid"), Inventory->GetTotalQuantity(Assets.B5.Base.Wood) == 0 && Inventory->GetTotalQuantity(Assets.Stone) == 0);
	TestTrue(TEXT("A usable Station_Workbench"), Station && Station->IsStationUsable() && Station->GetStationDefinition() == Assets.Station);
	TestEqual(TEXT("Open request from a pawn without a player controller"), Bench->GetOpenCraftingResult(Builder.Pawn),
		ECraftingStationOpenResult::NoPlayerController);
	Bench->GetBuildingPieceComponent()->SetPieceState(EBuildingPieceState::Removing);
	TestEqual(TEXT("Not usable while not Active"), Bench->GetOpenCraftingResult(Builder.Pawn), ECraftingStationOpenResult::StationUnavailable);
	Bench->GetBuildingPieceComponent()->SetPieceState(EBuildingPieceState::Active);
	Bench->DispatchBeginPlay();

	// The production station recipe crafts here, following the station's range.
	UCraftingComponent* Crafting = B7AddCrafting(Builder.Pawn, { Assets.Sword });
	TStrongObjectPtr<UCraftingTimedTestListener> CraftListener(NewObject<UCraftingTimedTestListener>(GetTransientPackage()));
	Crafting->OnCraftCancelled.AddDynamic(CraftListener.Get(), &UCraftingTimedTestListener::HandleCancelled);
	Inventory->TryAddDefinitionQuantity(Assets.B5.Base.Wood, 2);
	Inventory->TryAddDefinitionQuantity(Assets.B5.Scrap, 4);
	TestTrue(TEXT("Short Sword can be crafted at the built bench"), Crafting->CanCraftRecipeAtStation(Assets.Sword, Station).IsSuccess());
	Builder.Pawn->SetActorLocation(FVector(100.0, -400.0, 92.0));
	TestTrue(TEXT("Out of the bench's range"), Crafting->CanCraftRecipeAtStation(Assets.Sword, Station).Result == ECraftingTransactionResult::StationOutOfRange);
	Builder.Pawn->SetActorLocation(FVector(100.0, -150.0, 92.0));
	if (!TestTrue(TEXT("The timed craft starts at the bench"),
		Crafting->TryStartCraftAtStation(Assets.Sword, Station).Result == ECraftingTransactionResult::Started))
	{
		return false;
	}

	// Demolishing the bench cancels the craft (StationLost); nothing is consumed and the bench is refunded in full.
	B6Expect(*this, TEXT("Demolish the bench"), Builder.Construction->TryDemolish(Bench), EBuildResult::Success);
	TestTrue(TEXT("The craft is cancelled once, with StationLost"), !Crafting->IsCrafting() && !Crafting->GetActiveCraftStation()
		&& CraftListener->CancelledEvents == 1 && CraftListener->LastCancelReason == ECraftCancelReason::StationLost);
	TestTrue(TEXT("Ingredients untouched; full refund Wood 20 + Stone 10"), Inventory->GetTotalQuantity(Assets.B5.Base.Wood) == 22
		&& Inventory->GetTotalQuantity(Assets.B5.Scrap) == 4 && Inventory->GetTotalQuantity(Assets.Stone) == 10);
	Fixture.Clean(*this, TEXT("Workbench station"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB7StorageDemolishRulesTest,
	"NullTide.Building.B7.StorageDemolishRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB7StorageDemolishRulesTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { B2FloorClass(), B7TestChest() }, 6);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid())) { return false; }
	ABuildingPieceActor* Floor = B6Build(*this, Builder, B2FloorClass(), B2CellA);
	ABuildingTestChestActor* Chest = Cast<ABuildingTestChestActor>(B6Build(*this, Builder, B7TestChest(), B2CellA));
	if (!TestTrue(TEXT("Floor and chest built"), Floor && Chest)) { return false; }

	const FGuid Stored = B7StoreOne(Builder.Inventory, Chest->GetStorageComponent());
	const FB2Counts Before = FB2Counts::Capture(Builder);
	const FBuildingDemolishResult Refused = Builder.Construction->TryDemolish(Chest);
	B6Expect(*this, TEXT("A chest with items"), Refused, EBuildResult::StorageNotEmpty);
	TestEqual(TEXT("Reason text"), UBuildingPlacementComponent::DescribeResult(Refused.Result).ToString(), FString(TEXT("Empty it first")));
	B2ExpectDelta(*this, TEXT("Refused: nothing changed"), Before, Builder, 0, 0, 0, 0);
	TestTrue(TEXT("The chest and its item stay"), B6ActiveAlive(Chest) && Chest->GetStorageComponent()->GetItemCount() == 1);

	UStorageTransferLibrary::TransferStorageToInventory(Chest->GetStorageComponent(), Builder.Inventory, Stored);
	const FB2Counts Emptied = FB2Counts::Capture(Builder);
	B6Expect(*this, TEXT("The emptied chest"), Builder.Construction->TryDemolish(Chest), EBuildResult::Success);
	B2ExpectDelta(*this, TEXT("Full refund Wood 2 in one transaction"), Emptied, Builder, 1, 1, 0, 2);
	TestTrue(TEXT("The chest is gone; the floor stays"), !B6Alive(Chest) && B6ActiveAlive(Floor));
	Fixture.Clean(*this, TEXT("Storage demolish rules"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB7FurnitureCascadeTest,
	"NullTide.Building.B7.FurnitureCascade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB7FurnitureCascadeTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { B2FloorClass(), B7TestChest(), B6Wall(), B7TestBench() }, 30);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid())) { return false; }
	ABuildingPieceActor* FloorA = B6Build(*this, Builder, B2FloorClass(), B2CellA);
	ABuildingTestChestActor* Chest = Cast<ABuildingTestChestActor>(B6Build(*this, Builder, B7TestChest(), B2CellA));
	ABuildingPieceActor* Wall = B6Build(*this, Builder, B6Wall(), B5EdgeX11);
	ABuildingPieceActor* FloorE = B6Build(*this, Builder, B2FloorClass(), B6CellE);
	ABuildingPieceActor* Bench = B6Build(*this, Builder, B7TestBench(), B6CellE);
	if (!TestTrue(TEXT("Room built"), FloorA && Chest && Wall && FloorE && Bench)) { return false; }

	const FBuildingDemolishResult Preview = Builder.Construction->CanDemolish(FloorA);
	B6Expect(*this, TEXT("Floor A (CanDemolish)"), Preview, EBuildResult::Success);
	TestTrue(TEXT("The cascade takes the chest and the wall on floor A"), Preview.Pieces.Num() == 3 && B6Contains(Preview, Chest)
		&& B6Contains(Preview, Wall) && B6RefundOf(Preview.Refund, B2Wood()) == 4 + 2 + 6);

	// A chest with items refuses the whole cascade.
	const FGuid Stored = B7StoreOne(Builder.Inventory, Chest->GetStorageComponent());
	const FB2Counts Before = FB2Counts::Capture(Builder);
	B6Expect(*this, TEXT("Floor A under a chest with items"), Builder.Construction->TryDemolish(FloorA), EBuildResult::StorageNotEmpty);
	B2ExpectDelta(*this, TEXT("Refused cascade changes nothing"), Before, Builder, 0, 0, 0, 0);
	TestTrue(TEXT("Floor, chest and wall stay"), B6ActiveAlive(FloorA) && B6ActiveAlive(Chest) && B6ActiveAlive(Wall));

	UStorageTransferLibrary::TransferStorageToInventory(Chest->GetStorageComponent(), Builder.Inventory, Stored);
	const FB2Counts Emptied = FB2Counts::Capture(Builder);
	B6Expect(*this, TEXT("Floor A with an empty chest"), Builder.Construction->TryDemolish(FloorA), EBuildResult::Success);
	B2ExpectDelta(*this, TEXT("One transaction refunds floor, chest and wall"), Emptied, Builder, 1, 1, 0, 12);
	TestTrue(TEXT("All three are gone"), !B6Alive(FloorA) && !B6Alive(Chest) && !B6Alive(Wall));

	const FBuildingDemolishResult BenchCascade = Builder.Construction->CanDemolish(FloorE);
	TestTrue(TEXT("A floor under a workbench takes it with it"), BenchCascade.IsSuccess() && BenchCascade.Pieces.Num() == 2
		&& B6Contains(BenchCascade, Bench));
	Fixture.Clean(*this, TEXT("Furniture cascade"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB7ThinBlockerRegressionTest,
	"NullTide.Building.B7.ThinBlockerRegression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB7ThinBlockerRegressionTest::RunTest(const FString& Parameters)
{
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB2Builder Builder = Fixture.SpawnBuilder(FVector(0.0, 0.0, 92.0), { B2FloorClass(), B6Wall(), B7TestChest() }, 46);
	if (!TestTrue(TEXT("Builder set up"), Builder.IsValid())) { return false; }

	// A 10 cm world fence on cell A's west border (x 195..205) reaches 5 cm into the cell: it blocks a floor again.
	AActor* Fence = B2SpawnBlocker(Fixture.World, FVector(200.0, 300.0, 25.0), FVector(5.0, 60.0, 25.0));
	const FBuildingConstructionResult Blocked = Builder.Construction->TryBuild(B2FloorClass(), B5Place(B2FloorClass(), B2CellA));
	B2ExpectBranch(*this, TEXT("Floor over a thin border fence"), Blocked, EBuildResult::Blocked, Fence ? *Fence->GetName() : TEXT("<no fence>"));
	if (Fence)
	{
		Fence->Destroy();
	}

	// Walls on the cell's own edges never block it: build them from the neighbouring floors first.
	B6Build(*this, Builder, B2FloorClass(), B7CellD);
	B6Build(*this, Builder, B2FloorClass(), B6CellE);
	ABuildingPieceActor* South = B6Build(*this, Builder, B6Wall(), B5EdgeX11);
	ABuildingPieceActor* East = B6Build(*this, Builder, B6Wall(), B6EdgeY21);
	if (!TestTrue(TEXT("Walls on cell A's south and east edges"), South && East)) { return false; }
	// A wall on a neighbouring cell's edge that only touches cell A's corner (not ignored) never reaches the cell either.
	TestNotNull(TEXT("A wall on cell E's south edge, diagonal to cell A"), B6Build(*this, Builder, B6Wall(), FVector(500.0, 205.0, 0.0)));
	ABuildingPieceActor* FloorA = B6Build(*this, Builder, B2FloorClass(), B2CellA);
	TestNotNull(TEXT("A floor builds between walls on its own edges"), FloorA);
	TestNotNull(TEXT("A chest builds on it, against the walls"), B6Build(*this, Builder, B7TestChest(), B2CellA));
	Fixture.Clean(*this, TEXT("Thin blocker regression"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB7ProductionInputTest,
	"NullTide.Building.B7.ProductionInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB7ProductionInputTest::RunTest(const FString& Parameters)
{
	const FB7Assets Assets = FB7Assets::Load();
	if (!Assets.IsValid(*this)) { return false; }
	const TCHAR* InputRoot = TEXT("/Game/LevelPrototyping/BuildingSystem/Input/");
	auto LoadAction = [InputRoot](const TCHAR* Name) { return LoadObject<UInputAction>(nullptr, *FString::Printf(TEXT("%s%s.%s"), InputRoot, Name, Name)); };
	const UInputMappingContext* ModeContext = LoadObject<UInputMappingContext>(nullptr,
		TEXT("/Game/LevelPrototyping/BuildingSystem/Input/IMC_BuildingMode.IMC_BuildingMode"));
	const UInputAction* Selects[5] = { LoadAction(TEXT("IA_BuildSelect1")), LoadAction(TEXT("IA_BuildSelect2")), LoadAction(TEXT("IA_BuildSelect3")),
		LoadAction(TEXT("IA_BuildSelect4")), LoadAction(TEXT("IA_BuildSelect5")) };
	if (!TestTrue(TEXT("IMC_BuildingMode and the five select actions load"), ModeContext && Selects[0] && Selects[1] && Selects[2] && Selects[3]
		&& Selects[4]))
	{
		return false;
	}
	for (const UInputAction* Action : { Selects[3], Selects[4] })
	{
		TestTrue(FString::Printf(TEXT("%s: digital, consumes its keys, no triggers"), *Action->GetName()),
			Action->ValueType == EInputActionValueType::Boolean && Action->bConsumeInput && Action->Triggers.IsEmpty());
	}
	TMap<FKey, const UInputAction*> Mode;
	for (const FEnhancedActionKeyMapping& Mapping : ModeContext->GetMappings())
	{
		Mode.Add(Mapping.Key, Mapping.Action);
	}
	const FKey Keys[5] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
	bool bSelectKeys = true;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		bSelectKeys &= Mode.FindRef(Keys[Index]) == Selects[Index];
	}
	TestTrue(TEXT("IMC_BuildingMode: exactly 12 mappings, 1-5 select in order"), ModeContext->GetMappings().Num() == 12 && Mode.Num() == 12 && bSelectKeys);

	UBlueprint* PlayerBlueprint = LoadObject<UBlueprint>(nullptr, B3PlayerPath);
	const UConstructionComponent* Construction = nullptr;
	const UBuildingPlacementComponent* Placement = nullptr;
	if (const USimpleConstructionScript* ConstructionScript = PlayerBlueprint ? PlayerBlueprint->SimpleConstructionScript.Get() : nullptr)
	{
		for (const USCS_Node* Node : ConstructionScript->GetAllNodes())
		{
			Construction = Construction ? Construction : (Node ? Cast<UConstructionComponent>(Node->ComponentTemplate) : nullptr);
			Placement = Placement ? Placement : (Node ? Cast<UBuildingPlacementComponent>(Node->ComponentTemplate) : nullptr);
		}
	}
	if (!TestTrue(TEXT("Production player has construction and placement components"), Construction && Placement)) { return false; }
	TestTrue(TEXT("Registered buildings: Floor, Wall, Door, Chest, Workbench"), Construction->GetAvailableBuildings()
		== FB2Buildings({ Assets.B5.Base.Floor, Assets.B5.Wall, Assets.B5.Door, Assets.Chest, Assets.Bench }));
	const FArrayProperty* SelectProperty = FindFProperty<FArrayProperty>(UBuildingPlacementComponent::StaticClass(), TEXT("SelectBuildingActions"));
	const TArray<TObjectPtr<UInputAction>>* SelectActions = SelectProperty
		? SelectProperty->ContainerPtrToValuePtr<TArray<TObjectPtr<UInputAction>>>(Placement) : nullptr;
	bool bSelectActions = SelectActions && SelectActions->Num() == 5;
	for (int32 Index = 0; bSelectActions && Index < 5; ++Index)
	{
		bSelectActions &= (*SelectActions)[Index] == Selects[Index];
	}
	TestTrue(TEXT("Placement selects with IA_BuildSelect1-5 in order"), bSelectActions);
	return true;
}

// ---- B8: building UI (build bar) ----

namespace
{
const TCHAR* B8BarPath = TEXT("/Game/LevelPrototyping/BuildingSystem/UI/W_BuildingBar.W_BuildingBar_C");
const TCHAR* B8SlotPath = TEXT("/Game/LevelPrototyping/BuildingSystem/UI/W_BuildingBarSlot.W_BuildingBarSlot_C");

/** The five production buildings in the player's order. */
FB2Buildings B8Buildings(const FB7Assets& Assets)
{
	return { Assets.B5.Base.Floor, Assets.B5.Wall, Assets.B5.Door, Assets.Chest, Assets.Bench };
}

/** Inventory revision, build results and production Wood, for exact "nothing happened" checks. */
struct FB8Counts
{
	int32 Revision = 0;
	int32 BuildResults = 0;
	int32 Wood = 0;

	static FB8Counts Capture(const FB4Player& Player, const FB7Assets& Assets)
	{
		FB8Counts Counts;
		Counts.Revision = Player.Builder.Inventory->GetRevision();
		Counts.BuildResults = Player.Builder.Listener->BuildResults.Num();
		Counts.Wood = Player.Builder.Inventory->GetTotalQuantity(Assets.B5.Base.Wood);
		return Counts;
	}

	bool Delta(const FB8Counts& After, const int32 InRevision, const int32 InResults, const int32 InWood) const
	{
		return After.Revision - Revision == InRevision && After.BuildResults - BuildResults == InResults && After.Wood - Wood == InWood;
	}
};

/** A builder with the five production buildings and production Wood, in building mode with the production HUD (cursor on cell A). */
FB4Player B8EnterWithHud(FB2World& Fixture, const FB7Assets& Assets, UClass* HudClass, const int32 ProductionWood)
{
	FB4Player Player = B4SpawnPlayer(Fixture, B8Buildings(Assets), 0);
	if (Player.IsValid() && Player.Builder.Inventory)
	{
		if (ProductionWood > 0)
		{
			Player.Builder.Inventory->TryAddDefinitionQuantity(Assets.B5.Base.Wood, ProductionWood);
		}
		Player.Placement->SetHudClass(HudClass);
		Player.Placement->SetCursor(B2CellA);
		Player.Placement->EnterBuildingMode();
	}
	return Player;
}

UBuildingBarWidget* B8Bar(const FB4Player& Player)
{
	const UBuildingHudWidget* Hud = Player.Placement ? Player.Placement->GetHudWidget() : nullptr;
	return Hud ? Hud->GetBuildBar() : nullptr;
}

/** The index of the only selected slot; INDEX_NONE when none or several are selected. */
int32 B8SelectedSlot(const UBuildingBarWidget* Bar)
{
	int32 Selected = INDEX_NONE;
	int32 Count = 0;
	for (const UBuildingBarSlotWidget* BarSlot : Bar ? Bar->GetSlots() : TArray<UBuildingBarSlotWidget*>())
	{
		if (BarSlot && BarSlot->IsSelected())
		{
			Selected = BarSlot->GetSlotIndex();
			++Count;
		}
	}
	return Count == 1 ? Selected : INDEX_NONE;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB8ProductionAssetsTest,
	"NullTide.Building.B8.ProductionAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB8ProductionAssetsTest::RunTest(const FString& Parameters)
{
	UClass* HudClass = LoadClass<UBuildingHudWidget>(nullptr, B4HudPath);
	UClass* BarClass = LoadClass<UBuildingBarWidget>(nullptr, B8BarPath);
	UClass* SlotClass = LoadClass<UBuildingBarSlotWidget>(nullptr, B8SlotPath);
	if (!TestTrue(TEXT("W_BuildingHud, W_BuildingBar and W_BuildingBarSlot load"), HudClass && BarClass && SlotClass)) { return false; }
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }

	UBuildingHudWidget* Hud = CreateWidget<UBuildingHudWidget>(Fixture.World, HudClass);
	UBuildingBarWidget* Bar = Hud ? Hud->GetBuildBar() : nullptr;
	TestTrue(TEXT("W_BuildingHud binds a BuildBar of class W_BuildingBar"), Bar && Bar->IsA(BarClass));
	TestTrue(TEXT("The HUD root does not block hit tests (only the bar is hit-testable)"),
		Hud && Hud->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
	if (Bar)
	{
		const FClassProperty* SlotProperty = FindFProperty<FClassProperty>(UBuildingBarWidget::StaticClass(), TEXT("SlotClass"));
		TestTrue(TEXT("The bar's SlotClass is W_BuildingBarSlot"), SlotProperty && SlotProperty->GetPropertyValue_InContainer(Bar) == SlotClass);
		TestTrue(TEXT("The bar is hit-testable (hover guard) and handles no click itself"), Bar->GetVisibility() == ESlateVisibility::Visible);
	}
	UBuildingBarSlotWidget* BarSlot = CreateWidget<UBuildingBarSlotWidget>(Fixture.World, SlotClass);
	TestTrue(TEXT("The slot button exists and is not focusable (keys keep reaching the game)"),
		BarSlot && BarSlot->GetSlotButton() && !BarSlot->GetSlotButton()->GetIsFocusable());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB8BarSlotsTest,
	"NullTide.Building.B8.BarSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB8BarSlotsTest::RunTest(const FString& Parameters)
{
	const FB7Assets Assets = FB7Assets::Load();
	UClass* HudClass = LoadClass<UBuildingHudWidget>(nullptr, B4HudPath);
	if (!Assets.IsValid(*this) || !TestNotNull(TEXT("W_BuildingHud loads"), HudClass)) { return false; }
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB4Player Player = B8EnterWithHud(Fixture, Assets, HudClass, 0);
	const UBuildingBarWidget* Bar = B8Bar(Player);
	if (!TestNotNull(TEXT("Building mode shows the build bar"), Bar)) { return false; }
	const TArray<UBuildingBarSlotWidget*> Slots = Bar->GetSlots();
	const FB2Buildings Buildings = B8Buildings(Assets);
	if (!TestEqual(TEXT("One slot per building plus demolish"), Slots.Num(), Buildings.Num() + 1)) { return false; }

	for (int32 Index = 0; Index < Buildings.Num(); ++Index)
	{
		const UBuildingBarSlotWidget* BarSlot = Slots[Index];
		const UBuildingDefinition* Definition = Buildings[Index]->GetDefaultObject<UBuildingDefinition>();
		const FString Name = Definition->BuildingName.ToString();
		TestTrue(FString::Printf(TEXT("Slot %d: %s, key %d, cost '%s', initial '%s'"), Index, *Name, Index + 1,
				*BarSlot->GetCostText().ToString(), *BarSlot->GetInitialText().ToString()),
			BarSlot->GetBuilding() == Buildings[Index] && !BarSlot->IsDemolishSlot()
			&& BarSlot->GetKeyText().ToString() == FString::FromInt(Index + 1) && BarSlot->GetNameText().ToString() == Name
			&& BarSlot->GetCostText().ToString() == UBuildingPlacementComponent::DescribeRefund(Definition->Cost).ToString()
			&& BarSlot->GetInitialText().ToString() == Name.Left(1).ToUpper()
			&& !Definition->BuildingDescription.IsEmpty() && BarSlot->GetToolTipText().ToString() == Definition->BuildingDescription.ToString());
	}
	const UBuildingBarSlotWidget* Demolish = Slots.Last();
	TestTrue(TEXT("The last slot is Demolish on C"), Demolish->IsDemolishSlot() && Demolish->GetKeyText().ToString() == TEXT("C")
		&& Demolish->GetNameText().ToString() == TEXT("Demolish") && !Demolish->GetToolTipText().IsEmpty());
	TestTrue(TEXT("The floor costs Wood 4"), Slots[0]->GetCostText().ToString() == TEXT("Wood 4"));
	Player.Placement->ExitBuildingMode();
	Fixture.Clean(*this, TEXT("Bar slots"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB8SelectionSyncTest,
	"NullTide.Building.B8.SelectionSync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB8SelectionSyncTest::RunTest(const FString& Parameters)
{
	const FB7Assets Assets = FB7Assets::Load();
	UClass* HudClass = LoadClass<UBuildingHudWidget>(nullptr, B4HudPath);
	if (!Assets.IsValid(*this) || !TestNotNull(TEXT("W_BuildingHud loads"), HudClass)) { return false; }
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB4Player Player = B8EnterWithHud(Fixture, Assets, HudClass, 100);
	UBuildingPlacementTestComponent* Placement = Player.Placement;
	UBuildingBarWidget* Bar = B8Bar(Player);
	if (!TestNotNull(TEXT("Build bar"), Bar)) { return false; }
	const TArray<UBuildingBarSlotWidget*> Slots = Bar->GetSlots();
	const int32 DemolishSlot = Slots.Num() - 1;
	const FB8Counts Before = FB8Counts::Capture(Player, Assets);

	TestEqual(TEXT("Entering highlights the selected building"), B8SelectedSlot(Bar), 0);
	Placement->SelectBuildingIndex(3);
	TestEqual(TEXT("SelectBuildingIndex moves the highlight"), B8SelectedSlot(Bar), 3);
	Placement->ToggleDemolishMode();
	TestEqual(TEXT("Demolish highlights the demolish slot"), B8SelectedSlot(Bar), DemolishSlot);

	// Through the real button binding.
	Slots[1]->GetSlotButton()->OnClicked.Broadcast();
	TestTrue(TEXT("Clicking a building slot leaves demolish and selects it"), !Placement->IsDemolishModeActive()
		&& Placement->GetSelectedBuildingIndex() == 1 && Placement->GetSelectedBuilding() == Assets.B5.Wall && B8SelectedSlot(Bar) == 1);
	Slots[DemolishSlot]->HandleSlotClicked();
	TestTrue(TEXT("Clicking the demolish slot enters demolish"), Placement->IsDemolishModeActive() && B8SelectedSlot(Bar) == DemolishSlot);
	Slots[DemolishSlot]->HandleSlotClicked();
	TestTrue(TEXT("Clicking it again returns to building with the selection"), !Placement->IsDemolishModeActive() && B8SelectedSlot(Bar) == 1);
	Slots[4]->HandleSlotClicked();
	TestTrue(TEXT("Clicking the workbench selects it"), Placement->GetSelectedBuilding() == Assets.Bench && B8SelectedSlot(Bar) == 4);

	TestTrue(TEXT("Bar clicks never build or touch the inventory"), Before.Delta(FB8Counts::Capture(Player, Assets), 0, 0, 0)
		&& Fixture.LivePieces() == 0);
	TestTrue(TEXT("Still in building mode"), Placement->IsBuildingModeActive());
	Placement->ExitBuildingMode();
	Fixture.Clean(*this, TEXT("Selection sync"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB8AffordabilityTest,
	"NullTide.Building.B8.Affordability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB8AffordabilityTest::RunTest(const FString& Parameters)
{
	const FB7Assets Assets = FB7Assets::Load();
	UClass* HudClass = LoadClass<UBuildingHudWidget>(nullptr, B4HudPath);
	if (!Assets.IsValid(*this) || !TestNotNull(TEXT("W_BuildingHud loads"), HudClass)) { return false; }
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB4Player Player = B8EnterWithHud(Fixture, Assets, HudClass, 0);
	UInventoryComponent* Inventory = Player.Builder.Inventory;
	const UBuildingBarWidget* Bar = B8Bar(Player);
	if (!TestTrue(TEXT("Build bar and inventory"), Bar && Inventory)) { return false; }
	const TArray<UBuildingBarSlotWidget*> Slots = Bar->GetSlots();
	auto Pattern = [&Slots]()
	{
		FString Result;
		for (const UBuildingBarSlotWidget* BarSlot : Slots)
		{
			Result += BarSlot->IsAffordable() ? TEXT("Y") : TEXT("n");
		}
		return Result;
	};
	auto MatchesPreflight = [&Slots, Inventory]()
	{
		for (const UBuildingBarSlotWidget* BarSlot : Slots)
		{
			const UBuildingDefinition* Definition = BarSlot->GetBuilding() ? BarSlot->GetBuilding()->GetDefaultObject<UBuildingDefinition>() : nullptr;
			if (Definition && BarSlot->IsAffordable() != Inventory->CanApplyDefinitionTransaction(Definition->Cost, {}).IsSuccess())
			{
				return false;
			}
		}
		return true;
	};

	TestEqual(TEXT("Empty inventory: every building greyed, demolish not"), Pattern(), FString(TEXT("nnnnnY")));
	Inventory->TryAddDefinitionQuantity(Assets.B5.Base.Wood, 6);
	TestEqual(TEXT("Wood 6 (through OnInventoryChanged alone): floor and wall"), Pattern(), FString(TEXT("YYnnnY")));
	Inventory->TryAddDefinitionQuantity(Assets.B5.Scrap, 2);
	Inventory->TryAddDefinitionQuantity(Assets.B5.Base.Wood, 14);
	TestEqual(TEXT("Wood 20 + Scrap 2: everything but the workbench (needs Stone)"), Pattern(), FString(TEXT("YYYYnY")));
	Inventory->TryAddDefinitionQuantity(Assets.Stone, 10);
	TestEqual(TEXT("Stone 10: the workbench too"), Pattern(), FString(TEXT("YYYYYY")));
	TestTrue(TEXT("Every slot equals CanApplyDefinitionTransaction"), MatchesPreflight());
	FInventoryDefinitionQuantity Stone;
	Stone.DefinitionClass = Assets.Stone;
	Stone.Quantity = 10;
	Inventory->TryApplyDefinitionTransaction({ Stone }, {});
	TestEqual(TEXT("Spending the Stone greys the workbench again"), Pattern(), FString(TEXT("YYYYnY")));

	// After exit the removed bar no longer follows the inventory.
	Player.Placement->ExitBuildingMode();
	Inventory->TryAddDefinitionQuantity(Assets.Stone, 10);
	TestEqual(TEXT("A removed bar does not react to later inventory changes"), Pattern(), FString(TEXT("YYYYnY")));
	Player.Placement->EnterBuildingMode();
	const UBuildingBarWidget* Fresh = B8Bar(Player);
	TestTrue(TEXT("Re-entering builds a fresh bar with the current inventory"), Fresh && Fresh != Bar
		&& Fresh->GetSlots().Num() == Slots.Num() && Fresh->GetSlots()[4]->IsAffordable());
	Player.Placement->ExitBuildingMode();
	Fixture.Clean(*this, TEXT("Affordability"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB8ConfirmGuardTest,
	"NullTide.Building.B8.ConfirmGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB8ConfirmGuardTest::RunTest(const FString& Parameters)
{
	const FB7Assets Assets = FB7Assets::Load();
	UClass* HudClass = LoadClass<UBuildingHudWidget>(nullptr, B4HudPath);
	if (!Assets.IsValid(*this) || !TestNotNull(TEXT("W_BuildingHud loads"), HudClass)) { return false; }
	FB2World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	FB4Player Player = B8EnterWithHud(Fixture, Assets, HudClass, 8);
	UBuildingPlacementTestComponent* Placement = Player.Placement;
	if (!TestTrue(TEXT("Building mode with the floor selected"), Placement->IsBuildingModeActive() && Placement->GetSelectedBuildingIndex() == 0)) { return false; }

	Placement->bCursorOverBuildBar = true;
	FB8Counts Before = FB8Counts::Capture(Player, Assets);
	Placement->PressConfirm();
	TestTrue(TEXT("Confirm over the bar does not build"), Before.Delta(FB8Counts::Capture(Player, Assets), 0, 0, 0) && Fixture.LivePieces() == 0);

	Placement->bCursorOverBuildBar = false;
	Before = FB8Counts::Capture(Player, Assets);
	Placement->PressConfirm();
	TestTrue(TEXT("Confirm in the world builds once"), Before.Delta(FB8Counts::Capture(Player, Assets), 1, 1, -4) && Fixture.LivePieces() == 1);

	AActor* Floor = Fixture.PieceAt(B2CellA);
	Placement->ToggleDemolishMode();
	Placement->CursorHitActor = Floor;
	Placement->bCursorOverBuildBar = true;
	Placement->PressConfirm();
	TestTrue(TEXT("Confirm over the bar does not demolish"), ::IsValid(Floor) && !Floor->IsActorBeingDestroyed() && Fixture.LivePieces() == 1);
	Placement->bCursorOverBuildBar = false;
	Placement->PressConfirm();
	TestEqual(TEXT("Confirm in the world demolishes"), Fixture.LivePieces(), 0);
	Placement->ExitBuildingMode();
	Fixture.Clean(*this, TEXT("Confirm guard"));
	return true;
}

#endif
