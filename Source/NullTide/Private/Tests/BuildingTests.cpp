#include "BuildingTestTypes.h"

#include "BaseTestTypes.h"
#include "InventoryComponentTestTypes.h"
#include "Building/BuildingDefinition.h"
#include "Building/BuildingPieceActor.h"
#include "Building/BuildingPieceComponent.h"
#include "Building/BuildingSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Items/ItemDefinition.h"
#include "Misc/AutomationTest.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
FInventoryDefinitionQuantity B1Cost(TSubclassOf<UItemDefinition> Definition, int32 Quantity)
{
	FInventoryDefinitionQuantity Cost;
	Cost.DefinitionClass = Definition;
	Cost.Quantity = Quantity;
	return Cost;
}
}

ABuildingTestPieceActor::ABuildingTestPieceActor()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PieceMesh->SetStaticMesh(CubeMesh.Object);
		PieceMesh->SetRelativeScale3D(FVector(2.0, 2.0, 0.1));
		PieceMesh->SetRelativeLocation(FVector(0.0, 0.0, 5.0));
	}
}

UBuildingTestFloorDefinition::UBuildingTestFloorDefinition()
{
	BuildingName = FText::FromString(TEXT("Test Floor"));
	Category = EBuildingCategory::Foundation;
	PlacementMode = EBuildingPlacementMode::GridCell;
	PieceActorClass = ABuildingTestPieceActor::StaticClass();
	Cost.Add(B1Cost(UInventoryStackableTestItemDefinition::StaticClass(), 4));
}

UBuildingTestWideFloorDefinition::UBuildingTestWideFloorDefinition()
{
	BuildingName = FText::FromString(TEXT("Test Wide Floor"));
	Category = EBuildingCategory::Foundation;
	PlacementMode = EBuildingPlacementMode::GridCell;
	Footprint = FIntPoint(2, 1);
	PieceActorClass = ABuildingTestPieceActor::StaticClass();
	Cost.Add(B1Cost(UInventoryStackableTestItemDefinition::StaticClass(), 8));
}

UBuildingTestWallDefinition::UBuildingTestWallDefinition()
{
	BuildingName = FText::FromString(TEXT("Test Wall"));
	Category = EBuildingCategory::Wall;
	PlacementMode = EBuildingPlacementMode::GridEdge;
	PieceActorClass = ABuildingTestPieceActor::StaticClass();
	Cost.Add(B1Cost(UInventoryStackableTestItemDefinition::StaticClass(), 6));
}

UBuildingTestDoorDefinition::UBuildingTestDoorDefinition()
{
	BuildingName = FText::FromString(TEXT("Test Door"));
	Category = EBuildingCategory::Door;
	PlacementMode = EBuildingPlacementMode::GridEdge;
	PieceActorClass = ABuildingTestPieceActor::StaticClass();
	Cost.Add(B1Cost(UInventoryStackableTestItemDefinition::StaticClass(), 8));
	Cost.Add(B1Cost(UInventoryOtherTestItemDefinition::StaticClass(), 2));
}

UBuildingTestFurnitureDefinition::UBuildingTestFurnitureDefinition()
{
	BuildingName = FText::FromString(TEXT("Test Furniture"));
	Category = EBuildingCategory::Furniture;
	PlacementMode = EBuildingPlacementMode::GridCell;
	PieceActorClass = ABuildingTestPieceActor::StaticClass();
	Cost.Add(B1Cost(UInventoryStackableTestItemDefinition::StaticClass(), 2));
}

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using FB1Keys = TArray<FBuildingOccupancyKey>;

const UBuildingDefinition& B1Floor() { return *GetDefault<UBuildingTestFloorDefinition>(); }
const UBuildingDefinition& B1Wall() { return *GetDefault<UBuildingTestWallDefinition>(); }

FBuildingOccupancyKey FloorKey(int32 X, int32 Y, int32 Layer = 0)
{
	return FBuildingOccupancyKey(EBuildingOccupancyCategory::Floor, EBuildingEdgeAxis::None, X, Y, Layer);
}

FBuildingOccupancyKey EdgeKey(EBuildingEdgeAxis Axis, int32 X, int32 Y, int32 Layer = 0)
{
	return FBuildingOccupancyKey(EBuildingOccupancyCategory::Edge, Axis, X, Y, Layer);
}

/** Transient world with a world context and initialized actors (the CQTest spawner setup, as in C8A). */
struct FB1World
{
	UWorld* World = nullptr;
	UBuildingSubsystem* Subsystem = nullptr;
	UStaticMesh* Cube = nullptr;

	FB1World()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!World)
		{
			return;
		}
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		Subsystem = World->GetSubsystem<UBuildingSubsystem>();
		Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		NullTideBaseTests::SpawnBaseZone(World, FVector::ZeroVector, NullTideBaseTests::FixtureZoneSize);
	}

	~FB1World()
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
	}

	bool IsValid(FAutomationTestBase& Test) const
	{
		return Test.TestTrue(TEXT("Building test world with subsystem and cube mesh"), World && Subsystem && Cube);
	}

	/** A box of the engine cube mesh; HalfExtent in cm. Profiles: BlockAll (WorldStatic), PhysicsActor, BlockAllDynamic. */
	AActor* SpawnBox(const FVector& Center, const FVector& HalfExtent, const FRotator& Rotation = FRotator::ZeroRotator,
		const FName Profile = TEXT("BlockAll")) const
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor, TEXT("Box"));
		Mesh->SetStaticMesh(Cube);
		Mesh->SetCollisionProfileName(Profile);
		Actor->SetRootComponent(Mesh);
		Actor->AddInstanceComponent(Mesh);
		Mesh->SetWorldTransform(FTransform(Rotation, Center, HalfExtent / 50.0));
		Mesh->RegisterComponent();
		return Actor;
	}

	ACharacter* SpawnCharacter(const FVector& Location) const
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<ACharacter>(ACharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
	}

	/** A test piece initialized before FinishSpawning (the B2 construction order); optionally begins play. */
	ABuildingTestPieceActor* SpawnPiece(TSubclassOf<UBuildingDefinition> Definition, const FBuildingPlacement& Placement,
		const FB1Keys& Keys, EBuildingPieceState State, bool bBeginPlay) const
	{
		ABuildingTestPieceActor* Piece = World->SpawnActorDeferred<ABuildingTestPieceActor>(ABuildingTestPieceActor::StaticClass(),
			Placement.WorldTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Piece)
		{
			return nullptr;
		}
		Piece->GetBuildingPieceComponent()->InitializePiece(Definition, Placement, Keys, State, false);
		Piece->FinishSpawning(Placement.WorldTransform);
		if (bBeginPlay)
		{
			Piece->DispatchBeginPlay();
		}
		return Piece;
	}

	bool Audit(FAutomationTestBase& Test, const FString& What) const
	{
		const TArray<FString> Issues = Subsystem->AuditRegistry();
		for (const FString& Issue : Issues)
		{
			Test.AddError(What + TEXT(": ") + Issue);
		}
		return Issues.IsEmpty();
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB1DefinitionValidationTest,
	"NullTide.Building.B1.DefinitionValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB1DefinitionValidationTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Native definition root is abstract"), UBuildingDefinition::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	TestTrue(TEXT("Native definition root is const static data"), UBuildingDefinition::StaticClass()->HasAnyClassFlags(CLASS_Const));
	const UBuildingDefinition* Native = GetDefault<UBuildingDefinition>();
	TestTrue(TEXT("Defaults: 1 x 1, 90 degrees, full refund (B6), default range"), Native->Footprint == FIntPoint(1, 1)
		&& Native->RotationStep == 90 && FMath::IsNearlyEqual(Native->RefundFraction, 1.f) && Native->BuildRangeOverride == 0.f);

	FString Diagnostic;
	for (const UBuildingDefinition* Valid : { static_cast<const UBuildingDefinition*>(GetDefault<UBuildingTestFloorDefinition>()),
		static_cast<const UBuildingDefinition*>(GetDefault<UBuildingTestWideFloorDefinition>()),
		static_cast<const UBuildingDefinition*>(GetDefault<UBuildingTestWallDefinition>()),
		static_cast<const UBuildingDefinition*>(GetDefault<UBuildingTestDoorDefinition>()),
		static_cast<const UBuildingDefinition*>(GetDefault<UBuildingTestFurnitureDefinition>()) })
	{
		TestTrue(FString::Printf(TEXT("%s is valid"), *Valid->GetClass()->GetName()), Valid->ValidateBuilding(Diagnostic) && Diagnostic.IsEmpty());
	}
	TestTrue(TEXT("Occupancy categories"), B1Floor().GetOccupancyCategory() == EBuildingOccupancyCategory::Floor
		&& B1Wall().GetOccupancyCategory() == EBuildingOccupancyCategory::Edge
		&& GetDefault<UBuildingTestDoorDefinition>()->GetOccupancyCategory() == EBuildingOccupancyCategory::Edge
		&& GetDefault<UBuildingTestFurnitureDefinition>()->GetOccupancyCategory() == EBuildingOccupancyCategory::Furniture);

	TestTrue(TEXT("Native piece actor carries the piece component"), UBuildingDefinition::ClassHasPieceComponent(ABuildingTestPieceActor::StaticClass()));
	TestFalse(TEXT("A plain actor has no piece component"), UBuildingDefinition::ClassHasPieceComponent(AActor::StaticClass()));
	TestFalse(TEXT("A non-actor class is rejected"), UBuildingDefinition::ClassHasPieceComponent(UObject::StaticClass()));

	struct FCase { const TCHAR* Name; TFunction<void(UBuildingDefinition&)> Break; const TCHAR* Expect; };
	const FCase Cases[] = {
		{ TEXT("Null actor class"), [](UBuildingDefinition& D) { D.PieceActorClass = nullptr; }, TEXT("PieceActorClass") },
		{ TEXT("Abstract actor class"), [](UBuildingDefinition& D) { D.PieceActorClass = ABuildingPieceActor::StaticClass(); }, TEXT("abstract") },
		{ TEXT("Actor class without a piece component"), [](UBuildingDefinition& D) { D.PieceActorClass = AActor::StaticClass(); }, TEXT("UBuildingPieceComponent") },
		{ TEXT("Zero footprint"), [](UBuildingDefinition& D) { D.Footprint = FIntPoint(0, 1); }, TEXT("Footprint") },
		{ TEXT("Edge piece wider than one edge"), [](UBuildingDefinition& D) { D.Category = EBuildingCategory::Wall;
			D.PlacementMode = EBuildingPlacementMode::GridEdge; D.Footprint = FIntPoint(2, 1); }, TEXT("GridEdge") },
		{ TEXT("Wall on cells"), [](UBuildingDefinition& D) { D.Category = EBuildingCategory::Wall; }, TEXT("GridEdge") },
		{ TEXT("Floor on edges"), [](UBuildingDefinition& D) { D.PlacementMode = EBuildingPlacementMode::GridEdge; }, TEXT("GridEdge") },
		{ TEXT("45 degree rotation"), [](UBuildingDefinition& D) { D.RotationStep = 45; }, TEXT("RotationStep") },
		{ TEXT("Empty cost"), [](UBuildingDefinition& D) { D.Cost.Reset(); }, TEXT("Cost is empty") },
		{ TEXT("Null cost item"), [](UBuildingDefinition& D) { D.Cost[0].DefinitionClass = nullptr; }, TEXT("Cost[0]") },
		{ TEXT("Abstract cost item"), [](UBuildingDefinition& D) { D.Cost[0].DefinitionClass = UItemDefinition::StaticClass(); }, TEXT("Cost[0]") },
		{ TEXT("Invalid cost item data"), [](UBuildingDefinition& D) { D.Cost[0].DefinitionClass = UInventoryInvalidDataTestDefinition::StaticClass(); }, TEXT("invalid item data") },
		{ TEXT("Cost quantity 0"), [](UBuildingDefinition& D) { D.Cost[0].Quantity = 0; }, TEXT("below 1") },
		{ TEXT("Duplicate cost item"), [](UBuildingDefinition& D) { const FInventoryDefinitionQuantity Copy = D.Cost[0]; D.Cost.Add(Copy); }, TEXT("duplicate") },
		{ TEXT("Negative refund"), [](UBuildingDefinition& D) { D.RefundFraction = -0.1f; }, TEXT("RefundFraction") },
		{ TEXT("Refund above 1"), [](UBuildingDefinition& D) { D.RefundFraction = 1.1f; }, TEXT("RefundFraction") },
		{ TEXT("Negative build range"), [](UBuildingDefinition& D) { D.BuildRangeOverride = -1.f; }, TEXT("BuildRangeOverride") },
	};
	for (const FCase& Case : Cases)
	{
		UBuildingTestFloorDefinition* Definition = NewObject<UBuildingTestFloorDefinition>(GetTransientPackage());
		Case.Break(*Definition);
		const bool bValid = Definition->ValidateBuilding(Diagnostic);
		TestFalse(FString(Case.Name) + TEXT(" is rejected"), bValid);
		TestTrue(FString(Case.Name) + TEXT(" diagnostic mentions ") + Case.Expect, Diagnostic.Contains(Case.Expect));
	}

	TestNull(TEXT("Null class does not resolve"), UBuildingDefinition::ResolveValidBuilding(nullptr, Diagnostic));
	TestNull(TEXT("Abstract class does not resolve"), UBuildingDefinition::ResolveValidBuilding(UBuildingDefinition::StaticClass(), Diagnostic));
	TestTrue(TEXT("Valid class resolves to its default object"),
		UBuildingDefinition::ResolveValidBuilding(UBuildingTestFloorDefinition::StaticClass(), Diagnostic) == GetDefault<UBuildingTestFloorDefinition>());
#if WITH_EDITOR
	FDataValidationContext Context;
	TestTrue(TEXT("IsDataValid accepts a valid building"),
		GetMutableDefault<UBuildingTestFloorDefinition>()->IsDataValid(Context) == EDataValidationResult::Valid);
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB1GridCellMathTest,
	"NullTide.Building.B1.GridCellMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB1GridCellMathTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Cell size"), UBuildingSubsystem::CellSize, 200.0);
	struct FCase { FVector World; FIntPoint Cell; };
	const FCase Cases[] = {
		{ FVector(0.0, 0.0, 0.0), FIntPoint(0, 0) },
		{ FVector(199.999, 0.0, 0.0), FIntPoint(0, 0) },
		{ FVector(200.0, 0.0, 0.0), FIntPoint(1, 0) },    // exact boundary belongs to the positive side
		{ FVector(-0.001, 0.0, 0.0), FIntPoint(-1, 0) },  // floor division, not truncation
		{ FVector(-200.0, -200.0, 0.0), FIntPoint(-1, -1) },
		{ FVector(-200.001, 400.0, 0.0), FIntPoint(-2, 2) },
		{ FVector(-1.0, -1.0, 5000.0), FIntPoint(-1, -1) }, // height does not affect cells
		{ FVector(1999.0, -1999.0, 0.0), FIntPoint(9, -10) },
	};
	for (const FCase& Case : Cases)
	{
		TestTrue(FString::Printf(TEXT("WorldToCell(%s)"), *Case.World.ToCompactString()), UBuildingSubsystem::WorldToCell(Case.World) == Case.Cell);
	}
	TestTrue(TEXT("Cell (0,0) center"), UBuildingSubsystem::CellToWorldCenter(FIntPoint(0, 0)).Equals(FVector(100.0, 100.0, 0.0)));
	TestTrue(TEXT("Cell (-1,-1) center"), UBuildingSubsystem::CellToWorldCenter(FIntPoint(-1, -1), 30.0).Equals(FVector(-100.0, -100.0, 30.0)));
	for (int32 X = -3; X <= 3; ++X)
	{
		for (int32 Y = -3; Y <= 3; ++Y)
		{
			const FIntPoint Cell(X, Y);
			if (UBuildingSubsystem::WorldToCell(UBuildingSubsystem::CellToWorldCenter(Cell)) != Cell)
			{
				AddError(FString::Printf(TEXT("Round trip failed for cell (%d,%d)"), X, Y));
			}
		}
	}
	const FBuildingPlacement Placement = UBuildingSubsystem::MakePlacementFromWorld(B1Floor(), FVector(-150.0, 250.0, 12.0), 0);
	TestTrue(TEXT("Cursor snaps to its cell"), Placement.PlacementMode == EBuildingPlacementMode::GridCell
		&& Placement.Anchor == FIntPoint(-1, 1) && Placement.EdgeAxis == EBuildingEdgeAxis::None);
	TestTrue(TEXT("Floor transform at the cell center and height hint"),
		Placement.WorldTransform.GetLocation().Equals(FVector(-100.0, 300.0, 12.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB1GridEdgeSnappingTest,
	"NullTide.Building.B1.GridEdgeSnapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB1GridEdgeSnappingTest::RunTest(const FString& Parameters)
{
	auto Snap = [](const FVector& World)
	{
		EBuildingEdgeAxis Axis = EBuildingEdgeAxis::None;
		FIntPoint Edge;
		UBuildingSubsystem::SnapToEdge(World, Axis, Edge);
		return TPair<EBuildingEdgeAxis, FIntPoint>(Axis, Edge);
	};
	auto Is = [](const TPair<EBuildingEdgeAxis, FIntPoint>& Got, EBuildingEdgeAxis Axis, int32 X, int32 Y)
	{
		return Got.Key == Axis && Got.Value == FIntPoint(X, Y);
	};
	TestTrue(TEXT("South edge"), Is(Snap(FVector(100.0, 10.0, 0.0)), EBuildingEdgeAxis::X, 0, 0));
	TestTrue(TEXT("North edge is the next cell's south edge"), Is(Snap(FVector(100.0, 190.0, 0.0)), EBuildingEdgeAxis::X, 0, 1));
	TestTrue(TEXT("West edge"), Is(Snap(FVector(10.0, 100.0, 0.0)), EBuildingEdgeAxis::Y, 0, 0));
	TestTrue(TEXT("East edge is the next cell's west edge"), Is(Snap(FVector(190.0, 100.0, 0.0)), EBuildingEdgeAxis::Y, 1, 0));
	TestTrue(TEXT("Tie at the center resolves South"), Is(Snap(FVector(100.0, 100.0, 0.0)), EBuildingEdgeAxis::X, 0, 0));
	TestTrue(TEXT("Tie at a corner resolves South before West"), Is(Snap(FVector(5.0, 5.0, 0.0)), EBuildingEdgeAxis::X, 0, 0));
	TestTrue(TEXT("Tie North/East resolves North before East"), Is(Snap(FVector(195.0, 195.0, 0.0)), EBuildingEdgeAxis::X, 0, 1));

	// Shared edges have one canonical key from either side.
	TestTrue(TEXT("Shared X edge from below"), Is(Snap(FVector(100.0, 199.0, 0.0)), EBuildingEdgeAxis::X, 0, 1));
	TestTrue(TEXT("Shared X edge from above"), Is(Snap(FVector(100.0, 201.0, 0.0)), EBuildingEdgeAxis::X, 0, 1));
	TestTrue(TEXT("Shared Y edge from the left"), Is(Snap(FVector(199.0, 100.0, 0.0)), EBuildingEdgeAxis::Y, 1, 0));
	TestTrue(TEXT("Shared Y edge from the right"), Is(Snap(FVector(201.0, 100.0, 0.0)), EBuildingEdgeAxis::Y, 1, 0));
	TestTrue(TEXT("Exactly on an X edge line"), Is(Snap(FVector(100.0, 200.0, 0.0)), EBuildingEdgeAxis::X, 0, 1));

	// Negative coordinates.
	TestTrue(TEXT("Negative cell south edge"), Is(Snap(FVector(-100.0, -190.0, 0.0)), EBuildingEdgeAxis::X, -1, -1));
	TestTrue(TEXT("Negative cell north edge"), Is(Snap(FVector(-100.0, -10.0, 0.0)), EBuildingEdgeAxis::X, -1, 0));
	TestTrue(TEXT("Negative cell west edge"), Is(Snap(FVector(-195.0, -100.0, 0.0)), EBuildingEdgeAxis::Y, -1, -1));
	TestTrue(TEXT("Negative cell east edge"), Is(Snap(FVector(-5.0, -100.0, 0.0)), EBuildingEdgeAxis::Y, 0, -1));

	TestTrue(TEXT("X edge center"), UBuildingSubsystem::EdgeToWorldCenter(EBuildingEdgeAxis::X, FIntPoint(0, 1)).Equals(FVector(100.0, 200.0, 0.0)));
	TestTrue(TEXT("Y edge center"), UBuildingSubsystem::EdgeToWorldCenter(EBuildingEdgeAxis::Y, FIntPoint(-1, -1), 10.0).Equals(FVector(-200.0, -100.0, 10.0)));
	const FBuildingPlacement Wall = UBuildingSubsystem::MakePlacementFromWorld(B1Wall(), FVector(205.0, 300.0, 0.0), 0);
	TestTrue(TEXT("Wall placement snaps to the canonical edge"), Wall.PlacementMode == EBuildingPlacementMode::GridEdge
		&& Wall.EdgeAxis == EBuildingEdgeAxis::Y && Wall.Anchor == FIntPoint(1, 1)
		&& Wall.WorldTransform.GetLocation().Equals(FVector(200.0, 300.0, 0.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB1RotationMappingTest,
	"NullTide.Building.B1.RotationMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB1RotationMappingTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Normalize -1"), UBuildingSubsystem::NormalizeRotation(-1), 3);
	TestEqual(TEXT("Normalize 5"), UBuildingSubsystem::NormalizeRotation(5), 1);
	TestEqual(TEXT("Normalize -4"), UBuildingSubsystem::NormalizeRotation(-4), 0);
	TestTrue(TEXT("2 x 1 unrotated"), UBuildingSubsystem::GetRotatedFootprint(FIntPoint(2, 1), 0) == FIntPoint(2, 1));
	TestTrue(TEXT("2 x 1 quarter turn"), UBuildingSubsystem::GetRotatedFootprint(FIntPoint(2, 1), 1) == FIntPoint(1, 2));
	TestTrue(TEXT("2 x 1 half turn"), UBuildingSubsystem::GetRotatedFootprint(FIntPoint(2, 1), 2) == FIntPoint(2, 1));

	const UBuildingDefinition& Wide = *GetDefault<UBuildingTestWideFloorDefinition>();
	for (int32 Rotation = 0; Rotation < 4; ++Rotation)
	{
		const FBuildingPlacement Placement = UBuildingSubsystem::MakePlacementFromWorld(Wide, FVector(50.0, 50.0, 0.0), Rotation);
		const double Yaw = UBuildingSubsystem::GetPlacementYaw(Placement);
		TestEqual(FString::Printf(TEXT("Cell yaw for rotation %d"), Rotation), Yaw, Rotation * 90.0);
		TestTrue(FString::Printf(TEXT("Transform yaw is an exact multiple of 90 (rotation %d)"), Rotation),
			FMath::IsNearlyEqual(FRotator::NormalizeAxis(Placement.WorldTransform.Rotator().Yaw - Yaw), 0.0, 0.01));
		const FB1Keys Keys = UBuildingSubsystem::ComputeOccupancyKeys(Wide, Placement);
		const FB1Keys Expected = Rotation % 2 == 0 ? FB1Keys({ FloorKey(0, 0), FloorKey(1, 0) }) : FB1Keys({ FloorKey(0, 0), FloorKey(0, 1) });
		TestTrue(FString::Printf(TEXT("Footprint cells for rotation %d"), Rotation), Keys == Expected);
		const FVector Center = Rotation % 2 == 0 ? FVector(200.0, 100.0, 0.0) : FVector(100.0, 200.0, 0.0);
		TestTrue(FString::Printf(TEXT("Footprint center for rotation %d"), Rotation), Placement.WorldTransform.GetLocation().Equals(Center));
	}

	const FBuildingPlacement XEdge = UBuildingSubsystem::MakePlacementFromWorld(B1Wall(), FVector(100.0, 10.0, 0.0), 0);
	const FBuildingPlacement XEdgeFlipped = UBuildingSubsystem::MakePlacementFromWorld(B1Wall(), FVector(100.0, 10.0, 0.0), 1);
	const FBuildingPlacement YEdge = UBuildingSubsystem::MakePlacementFromWorld(B1Wall(), FVector(10.0, 100.0, 0.0), 0);
	const FBuildingPlacement YEdgeFlipped = UBuildingSubsystem::MakePlacementFromWorld(B1Wall(), FVector(10.0, 100.0, 0.0), 3);
	TestEqual(TEXT("X edge yaw"), UBuildingSubsystem::GetPlacementYaw(XEdge), 0.0);
	TestEqual(TEXT("X edge flipped yaw"), UBuildingSubsystem::GetPlacementYaw(XEdgeFlipped), 180.0);
	TestEqual(TEXT("Y edge yaw"), UBuildingSubsystem::GetPlacementYaw(YEdge), 90.0);
	TestEqual(TEXT("Y edge flipped yaw"), UBuildingSubsystem::GetPlacementYaw(YEdgeFlipped), 270.0);
	TestTrue(TEXT("Facing does not change the edge key"),
		UBuildingSubsystem::ComputeOccupancyKeys(B1Wall(), XEdge) == UBuildingSubsystem::ComputeOccupancyKeys(B1Wall(), XEdgeFlipped)
		&& UBuildingSubsystem::ComputeOccupancyKeys(B1Wall(), XEdge) == FB1Keys({ EdgeKey(EBuildingEdgeAxis::X, 0, 0) }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB1LayerMappingTest,
	"NullTide.Building.B1.LayerMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB1LayerMappingTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Ground"), UBuildingSubsystem::HeightToLayer(0.0), 0);
	TestEqual(TEXT("Floor top (10 cm) stays on the ground layer"), UBuildingSubsystem::HeightToLayer(UBuildingSubsystem::FloorThickness), 0);
	TestEqual(TEXT("24.9 cm"), UBuildingSubsystem::HeightToLayer(24.9), 0);
	TestEqual(TEXT("25 cm rounds up"), UBuildingSubsystem::HeightToLayer(25.0), 1);
	TestEqual(TEXT("200 cm plateau"), UBuildingSubsystem::HeightToLayer(200.0), 4);
	TestEqual(TEXT("Plateau floor top 210 cm"), UBuildingSubsystem::HeightToLayer(210.0), 4);
	TestEqual(TEXT("Below zero"), UBuildingSubsystem::HeightToLayer(-30.0), -1);
	const FBuildingPlacement Ground = UBuildingSubsystem::MakePlacementFromWorld(B1Floor(), FVector(100.0, 100.0, 0.0), 0);
	const FBuildingPlacement Plateau = UBuildingSubsystem::MakePlacementFromWorld(B1Floor(), FVector(100.0, 100.0, 200.0), 0);
	TestTrue(TEXT("Same column, different layers, different keys"),
		UBuildingSubsystem::ComputeOccupancyKeys(B1Floor(), Ground) == FB1Keys({ FloorKey(0, 0, 0) })
		&& UBuildingSubsystem::ComputeOccupancyKeys(B1Floor(), Plateau) == FB1Keys({ FloorKey(0, 0, 4) }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB1OccupancyLifecycleTest,
	"NullTide.Building.B1.OccupancyLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB1OccupancyLifecycleTest::RunTest(const FString& Parameters)
{
	FB1World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	UBuildingSubsystem* Registry = Fixture.Subsystem;
	TStrongObjectPtr<UBuildingTestReservationOwner> OwnerA(NewObject<UBuildingTestReservationOwner>(GetTransientPackage()));
	TStrongObjectPtr<UBuildingTestReservationOwner> OwnerB(NewObject<UBuildingTestReservationOwner>(GetTransientPackage()));
	const FBuildingOccupancyKey K1 = FloorKey(1, 1), K2 = FloorKey(2, 1), K3 = FloorKey(3, 1);

	// Invalid reservations change nothing.
	TestFalse(TEXT("Null owner"), Registry->ReserveKeys(nullptr, { K1 }).IsValid());
	TestFalse(TEXT("Empty keys"), Registry->ReserveKeys(OwnerA.Get(), {}).IsValid());
	TestFalse(TEXT("Duplicate keys in one request"), Registry->ReserveKeys(OwnerA.Get(), { K1, K1 }).IsValid());
	TestEqual(TEXT("Nothing registered"), Registry->GetOccupiedKeyCount() + Registry->GetReservedKeyCount(), 0);

	// All-or-nothing.
	const FBuildingReservationHandle H1 = Registry->ReserveKeys(OwnerA.Get(), { K1, K2 });
	TestTrue(TEXT("Reserve K1, K2"), H1.IsValid() && Registry->GetReservedKeyCount() == 2);
	TestFalse(TEXT("Overlapping reservation refused"), Registry->ReserveKeys(OwnerB.Get(), { K2, K3 }).IsValid());
	TestTrue(TEXT("K3 untouched by the refused request"), Registry->IsKeyFree(K3));
	TestFalse(TEXT("Reserved key is not free"), Registry->IsKeyFree(K1));
	TestTrue(TEXT("Reserved key is free for its own handle"), Registry->IsKeyFree(K1, H1));
	Fixture.Audit(*this, TEXT("After reservation"));

	// Owner validation and stale handles.
	TestFalse(TEXT("A foreign owner cannot release"), Registry->ReleaseReservation(H1, OwnerB.Get()));
	TestTrue(TEXT("The owner releases"), Registry->ReleaseReservation(H1, OwnerA.Get()));
	TestTrue(TEXT("Released keys are free"), Registry->IsKeyFree(K1) && Registry->IsKeyFree(K2));
	TestFalse(TEXT("Releasing twice fails"), Registry->ReleaseReservation(H1, OwnerA.Get()));
	const FBuildingReservationHandle H2 = Registry->ReserveKeys(OwnerB.Get(), { K1 });
	TestTrue(TEXT("Handles are unique and never reused"), H2.IsValid() && H2.Id != H1.Id);
	TestFalse(TEXT("A stale handle cannot release the newer reservation of the same key"), Registry->ReleaseReservation(H1, OwnerA.Get()));
	TestTrue(TEXT("The newer reservation is intact"), Registry->IsReservationActive(H2) && !Registry->IsKeyFree(K1));
	Fixture.Audit(*this, TEXT("After stale release"));

	// Commit: exactly the reserved keys, once.
	const FBuildingPlacement Placement = UBuildingSubsystem::MakePlacementFromWorld(B1Floor(), FVector(300.0, 300.0, 0.0), 0);
	ABuildingTestPieceActor* PieceActor = Fixture.SpawnPiece(UBuildingTestFloorDefinition::StaticClass(), Placement, { K1 },
		EBuildingPieceState::Provisional, false);
	ABuildingTestPieceActor* Mismatch = Fixture.SpawnPiece(UBuildingTestFloorDefinition::StaticClass(), Placement, { K3 },
		EBuildingPieceState::Provisional, false);
	if (!TestTrue(TEXT("Pieces spawn"), PieceActor && Mismatch)) { return false; }
	UBuildingPieceComponent* Piece = PieceActor->GetBuildingPieceComponent();
	TestFalse(TEXT("A provisional piece is not registered"), Registry->IsPieceRegistered(Piece));
	TestFalse(TEXT("A foreign owner cannot commit"), Registry->CommitReservation(H2, OwnerA.Get(), Piece));
	TestFalse(TEXT("Mismatched piece keys cannot commit"), Registry->CommitReservation(H2, OwnerB.Get(), Mismatch->GetBuildingPieceComponent()));
	TestTrue(TEXT("The reservation survives refused commits"), Registry->IsReservationActive(H2));
	TestTrue(TEXT("Commit"), Registry->CommitReservation(H2, OwnerB.Get(), Piece));
	TestTrue(TEXT("Committed: occupied by the piece, handle consumed"), Registry->GetOccupyingPiece(K1) == Piece
		&& Registry->IsPieceRegistered(Piece) && !Registry->IsReservationActive(H2));
	TestFalse(TEXT("The same handle cannot commit twice"), Registry->CommitReservation(H2, OwnerB.Get(), Piece));
	TestTrue(TEXT("Registering the same footprint again is a no-op"), Registry->RegisterActivePiece(Piece));
	TestEqual(TEXT("No duplicate occupancy"), Registry->GetOccupiedKeyCount(), 1);
	TestFalse(TEXT("A registered piece cannot be re-initialized"), Piece->InitializePiece(UBuildingTestFloorDefinition::StaticClass(),
		Placement, { K2 }, EBuildingPieceState::Active, false));
	Fixture.Audit(*this, TEXT("After commit"));

	// Destroying an actor that never began play: component destruction unregisters.
	PieceActor->Destroy();
	Mismatch->Destroy();
	TestTrue(TEXT("Destroyed piece freed its key"), Registry->IsKeyFree(K1) && Registry->GetOccupiedKeyCount() == 0);
	Fixture.Audit(*this, TEXT("After destroying a committed piece"));

	// BeginPlay registers Active pieces; EndPlay and component destruction unregister them.
	const FBuildingPlacement P4 = UBuildingSubsystem::MakePlacementFromWorld(B1Floor(), FVector(900.0, 900.0, 0.0), 0);
	ABuildingTestPieceActor* Active = Fixture.SpawnPiece(UBuildingTestFloorDefinition::StaticClass(), P4, { FloorKey(4, 4) },
		EBuildingPieceState::Active, true);
	ABuildingTestPieceActor* Provisional = Fixture.SpawnPiece(UBuildingTestFloorDefinition::StaticClass(), P4, { FloorKey(5, 5) },
		EBuildingPieceState::Provisional, true);
	TestTrue(TEXT("Active piece registered on BeginPlay"), Registry->GetOccupyingPiece(FloorKey(4, 4)) == Active->GetBuildingPieceComponent());
	TestTrue(TEXT("Provisional piece not registered on BeginPlay"), Registry->IsKeyFree(FloorKey(5, 5))
		&& !Registry->IsPieceRegistered(Provisional->GetBuildingPieceComponent()));
	Active->GetBuildingPieceComponent()->DestroyComponent();
	TestTrue(TEXT("Component destruction unregisters"), Registry->IsKeyFree(FloorKey(4, 4)));
	ABuildingTestPieceActor* Ended = Fixture.SpawnPiece(UBuildingTestFloorDefinition::StaticClass(), P4, { FloorKey(6, 6) },
		EBuildingPieceState::Active, true);
	TestFalse(TEXT("Second active piece registered"), Registry->IsKeyFree(FloorKey(6, 6)));
	Ended->Destroy();
	TestTrue(TEXT("Actor destruction (EndPlay) unregisters"), Registry->IsKeyFree(FloorKey(6, 6)) && Registry->GetOccupiedKeyCount() == 0);
	Fixture.Audit(*this, TEXT("After EndPlay and component destruction"));

	// Dead weak references (no callbacks at all) are treated as free and purged.
	AActor* Host = Fixture.SpawnBox(FVector(0.0, 0.0, -5000.0), FVector(10.0));
	UBuildingPieceComponent* Orphan = NewObject<UBuildingPieceComponent>(Host);
	Orphan->InitializePiece(UBuildingTestFloorDefinition::StaticClass(), P4, { FloorKey(7, 7) }, EBuildingPieceState::Active, false);
	TestTrue(TEXT("Orphan piece registers"), Registry->RegisterActivePiece(Orphan));
	TStrongObjectPtr<UBuildingTestReservationOwner> Doomed(NewObject<UBuildingTestReservationOwner>(GetTransientPackage()));
	TestTrue(TEXT("Reserve for a doomed owner"), Registry->ReserveKeys(Doomed.Get(), { FloorKey(8, 8) }).IsValid());
	Fixture.Audit(*this, TEXT("Before the holders die"));
	// Both holders die without any callback (no EndPlay, no component destruction, no release).
	Orphan->MarkAsGarbage();
	UBuildingTestReservationOwner* DoomedRaw = Doomed.Get();
	Doomed.Reset();
	DoomedRaw->MarkAsGarbage();
	TestTrue(TEXT("Dead piece's key counts as free"), Registry->IsKeyFree(FloorKey(7, 7)) && !Registry->GetOccupyingPiece(FloorKey(7, 7)));
	TestTrue(TEXT("Dead owner's reservation counts as free"), Registry->IsKeyFree(FloorKey(8, 8)));
	TestEqual(TEXT("Audit reports both stale entries before a purge"), Registry->AuditRegistry().Num(), 3);
	TestEqual(TEXT("Purge frees both stale keys"), Registry->PurgeStaleEntries(), 2);
	TestEqual(TEXT("Registry empty after purge"), Registry->GetOccupiedKeyCount() + Registry->GetReservedKeyCount(), 0);
	Fixture.Audit(*this, TEXT("After purge"));
	TestTrue(TEXT("The freed keys can be reserved again"), Registry->ReserveKeys(OwnerA.Get(), { FloorKey(7, 7), FloorKey(8, 8) }).IsValid());
	Fixture.Audit(*this, TEXT("After reusing purged keys"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB1OccupancyCategoriesTest,
	"NullTide.Building.B1.OccupancyCategories",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB1OccupancyCategoriesTest::RunTest(const FString& Parameters)
{
	FB1World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	UBuildingSubsystem* Registry = Fixture.Subsystem;
	TStrongObjectPtr<UBuildingTestReservationOwner> Owner(NewObject<UBuildingTestReservationOwner>(GetTransientPackage()));

	const FBuildingPlacement Floor = UBuildingSubsystem::MakePlacementFromWorld(B1Floor(), FVector(300.0, 300.0, 0.0), 0);
	const FBuildingPlacement Wall = UBuildingSubsystem::MakePlacementFromWorld(B1Wall(), FVector(300.0, 205.0, 0.0), 0);
	const FBuildingPlacement Door = UBuildingSubsystem::MakePlacementFromWorld(*GetDefault<UBuildingTestDoorDefinition>(), FVector(300.0, 195.0, 0.0), 1);
	const FBuildingPlacement Furniture = UBuildingSubsystem::MakePlacementFromWorld(*GetDefault<UBuildingTestFurnitureDefinition>(), FVector(300.0, 300.0, 0.0), 0);
	const FB1Keys FloorKeys = UBuildingSubsystem::ComputeOccupancyKeys(B1Floor(), Floor);
	const FB1Keys WallKeys = UBuildingSubsystem::ComputeOccupancyKeys(B1Wall(), Wall);
	const FB1Keys DoorKeys = UBuildingSubsystem::ComputeOccupancyKeys(*GetDefault<UBuildingTestDoorDefinition>(), Door);
	const FB1Keys FurnitureKeys = UBuildingSubsystem::ComputeOccupancyKeys(*GetDefault<UBuildingTestFurnitureDefinition>(), Furniture);
	TestTrue(TEXT("Wall and door from both sides of one edge share its canonical key"), WallKeys == DoorKeys
		&& WallKeys == FB1Keys({ EdgeKey(EBuildingEdgeAxis::X, 1, 1) }));

	TestTrue(TEXT("Floor reserved"), Registry->ReserveKeys(Owner.Get(), FloorKeys).IsValid());
	TestTrue(TEXT("A wall on the floor's edge does not conflict with the floor"), Registry->ReserveKeys(Owner.Get(), WallKeys).IsValid());
	TestFalse(TEXT("A door on the walled edge conflicts"), Registry->ReserveKeys(Owner.Get(), DoorKeys).IsValid());
	TestTrue(TEXT("Furniture uses its own key space"), Registry->ReserveKeys(Owner.Get(), FurnitureKeys).IsValid());
	const FBuildingPlacement Plateau = UBuildingSubsystem::MakePlacementFromWorld(B1Floor(), FVector(300.0, 300.0, 200.0), 0);
	TestTrue(TEXT("A second floor on the 200 cm layer of the same column coexists"),
		Registry->ReserveKeys(Owner.Get(), UBuildingSubsystem::ComputeOccupancyKeys(B1Floor(), Plateau)).IsValid());
	const FBuildingPlacement PlateauWall = UBuildingSubsystem::MakePlacementFromWorld(B1Wall(), FVector(300.0, 205.0, 200.0), 0);
	TestTrue(TEXT("The same edge on another layer coexists"),
		Registry->ReserveKeys(Owner.Get(), UBuildingSubsystem::ComputeOccupancyKeys(B1Wall(), PlateauWall)).IsValid());
	TestEqual(TEXT("Five keys reserved"), Registry->GetReservedKeyCount(), 5);
	Fixture.Audit(*this, TEXT("Categories"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB1PlacementValidationTest,
	"NullTide.Building.B1.PlacementValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB1PlacementValidationTest::RunTest(const FString& Parameters)
{
	FB1World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	UBuildingSubsystem* Subsystem = Fixture.Subsystem;
	// Ground: x [-400, 1000], y [-700, 700], top at 0. Platform: x [300, 700], y [-400, 0], top at 200 (layer 4).
	AActor* Ground = Fixture.SpawnBox(FVector(300.0, 0.0, -50.0), FVector(700.0, 700.0, 50.0));
	Fixture.SpawnBox(FVector(500.0, -200.0, 100.0), FVector(200.0, 200.0, 100.0));
	// A 35 degree slab over cell (0, 2); sized so it never reaches a neighbouring cell's surface samples.
	Fixture.SpawnBox(FVector(100.0, 500.0, 40.0), FVector(120.0, 100.0, 10.0), FRotator(35.0, 0.0, 0.0));
	ACharacter* Builder = Fixture.SpawnCharacter(FVector(0.0, 0.0, 92.0));
	if (!TestTrue(TEXT("World set up"), Ground && Builder)) { return false; }
	const TSubclassOf<UBuildingDefinition> FloorClass = UBuildingTestFloorDefinition::StaticClass();
	const TSubclassOf<UBuildingDefinition> WallClass = UBuildingTestWallDefinition::StaticClass();
	auto Floor = [](const FVector& World) { return UBuildingSubsystem::MakePlacementFromWorld(B1Floor(), World, 0); };
	auto Wall = [](const FVector& World) { return UBuildingSubsystem::MakePlacementFromWorld(B1Wall(), World, 0); };
	auto Expect = [this](const FBuildingPlacementResult& Result, EBuildResult Expected, const TCHAR* What)
	{
		const bool bMatch = Result.Result == Expected;
		TestTrue(FString::Printf(TEXT("%s: %s (got %s, %s)"), What, *StaticEnum<EBuildResult>()->GetNameStringByValue(static_cast<int64>(Expected)),
			*StaticEnum<EBuildResult>()->GetNameStringByValue(static_cast<int64>(Result.Result)), *Result.Diagnostic), bMatch);
		return bMatch;
	};

	// Valid floors on the ground and on the 200 cm platform.
	const FBuildingPlacementResult OnGround = Subsystem->ValidatePlacement(FloorClass, Floor(FVector(300.0, 300.0, 0.0)), Builder);
	if (Expect(OnGround, EBuildResult::Success, TEXT("Floor on the ground")))
	{
		TestTrue(TEXT("Ground floor resolved"), FMath::IsNearlyEqual(OnGround.Placement.SurfaceZ, 0.f, 0.1f) && OnGround.Placement.Layer == 0
			&& OnGround.Placement.WorldTransform.GetLocation().Equals(FVector(300.0, 300.0, 0.0), 0.1) && OnGround.Keys == FB1Keys({ FloorKey(1, 1) }));
	}
	const FBuildingPlacementResult OnPlatform = Subsystem->ValidatePlacement(FloorClass, Floor(FVector(500.0, -100.0, 200.0)), Builder);
	if (Expect(OnPlatform, EBuildResult::Success, TEXT("Floor on the platform")))
	{
		TestTrue(TEXT("Platform floor on layer 4"), FMath::IsNearlyEqual(OnPlatform.Placement.SurfaceZ, 200.f, 0.1f)
			&& OnPlatform.Placement.Layer == 4 && OnPlatform.Keys == FB1Keys({ FloorKey(2, -1, 4) }));
	}
	TestTrue(TEXT("The surface is recomputed, not trusted from the hint"), Subsystem->ValidatePlacement(FloorClass,
		Floor(FVector(300.0, 300.0, 120.0)), Builder).Placement.Layer == 0);

	// Shape, definition and builder.
	Expect(Subsystem->ValidatePlacement(nullptr, Floor(FVector(300.0, 300.0, 0.0)), Builder), EBuildResult::InvalidDefinition, TEXT("Null definition"));
	Expect(Subsystem->ValidatePlacement(UBuildingDefinition::StaticClass(), Floor(FVector(300.0, 300.0, 0.0)), Builder),
		EBuildResult::InvalidDefinition, TEXT("Abstract definition"));
	Expect(Subsystem->ValidatePlacement(WallClass, Floor(FVector(300.0, 300.0, 0.0)), Builder), EBuildResult::InvalidPlacement, TEXT("Wall with a cell placement"));
	FBuildingPlacement BadAxis = Floor(FVector(300.0, 300.0, 0.0));
	BadAxis.EdgeAxis = EBuildingEdgeAxis::X;
	Expect(Subsystem->ValidatePlacement(FloorClass, BadAxis, Builder), EBuildResult::InvalidPlacement, TEXT("Cell placement with an edge axis"));
	FBuildingPlacement BadRotation = Floor(FVector(300.0, 300.0, 0.0));
	BadRotation.Rotation = 4;
	Expect(Subsystem->ValidatePlacement(FloorClass, BadRotation, Builder), EBuildResult::InvalidPlacement, TEXT("Rotation outside 0-3"));
	Expect(Subsystem->ValidatePlacement(FloorClass, Floor(FVector(300.0, 300.0, 0.0)), nullptr), EBuildResult::InvalidPlacement, TEXT("No builder"));

	// Range, ground, slope, unevenness.
	Expect(Subsystem->ValidatePlacement(FloorClass, Floor(FVector(900.0, 900.0, 0.0)), Builder), EBuildResult::OutOfRange, TEXT("Beyond 600 cm"));
	Expect(Subsystem->ValidatePlacement(FloorClass, Floor(FVector(-500.0, 100.0, 0.0)), Builder), EBuildResult::NoGround, TEXT("Past the ground's edge"));
	Expect(Subsystem->ValidatePlacement(FloorClass, Floor(FVector(100.0, 500.0, 0.0)), Builder), EBuildResult::TooSteep, TEXT("35 degree slab"));
	Expect(Subsystem->ValidatePlacement(FloorClass, Floor(FVector(300.0, -100.0, 0.0)), Builder), EBuildResult::Uneven,
		TEXT("Cell straddling the 200 cm platform edge"));

	// Occupancy: a reservation blocks, and is ignored for its own handle.
	TStrongObjectPtr<UBuildingTestReservationOwner> Owner(NewObject<UBuildingTestReservationOwner>(GetTransientPackage()));
	const FBuildingReservationHandle Reserved = Subsystem->ReserveKeys(Owner.Get(), { FloorKey(1, 1) });
	Expect(Subsystem->ValidatePlacement(FloorClass, Floor(FVector(300.0, 300.0, 0.0)), Builder), EBuildResult::Occupied, TEXT("Reserved cell"));
	Expect(Subsystem->ValidatePlacement(FloorClass, Floor(FVector(300.0, 300.0, 0.0)), Builder, Reserved), EBuildResult::Success,
		TEXT("Reserved cell, own reservation ignored"));
	Subsystem->ReleaseReservation(Reserved, Owner.Get());

	// Collision: the ground under the piece never blocks, but an unrelated blocker inside the volume does.
	AActor* Crate = Fixture.SpawnBox(FVector(350.0, 350.0, 30.0), FVector(15.0, 15.0, 30.0));
	const FBuildingPlacementResult CrateResult = Subsystem->ValidatePlacement(FloorClass, Floor(FVector(300.0, 300.0, 0.0)), Builder);
	if (Expect(CrateResult, EBuildResult::Blocked, TEXT("WorldStatic crate inside the cell (between surface samples)")))
	{
		TestTrue(TEXT("The crate is reported, not the ground"), CrateResult.BlockingActor == Crate);
	}
	Crate->Destroy();
	AActor* Neighbour = Fixture.SpawnBox(FVector(450.0, 300.0, 30.0), FVector(15.0, 15.0, 30.0));
	Expect(Subsystem->ValidatePlacement(FloorClass, Floor(FVector(300.0, 300.0, 0.0)), Builder), EBuildResult::Success,
		TEXT("A blocker in the next cell does not block"));
	Neighbour->Destroy();
	AActor* Body = Fixture.SpawnBox(FVector(-150.0, 250.0, 30.0), FVector(15.0, 15.0, 30.0), FRotator::ZeroRotator, TEXT("PhysicsActor"));
	const FBuildingPlacementResult BodyResult = Subsystem->ValidatePlacement(FloorClass, Floor(FVector(-100.0, 300.0, 0.0)), Builder);
	TestTrue(TEXT("A physics body blocks"), Expect(BodyResult, EBuildResult::Blocked, TEXT("Physics body")) && BodyResult.BlockingActor == Body);
	ACharacter* Bystander = Fixture.SpawnCharacter(FVector(300.0, 500.0, 92.0));
	const FBuildingPlacementResult PawnResult = Subsystem->ValidatePlacement(FloorClass, Floor(FVector(300.0, 500.0, 0.0)), Builder);
	TestTrue(TEXT("Another pawn blocks"), Expect(PawnResult, EBuildResult::Blocked, TEXT("Pawn")) && PawnResult.BlockingActor == Bystander);
	Expect(Subsystem->ValidatePlacement(FloorClass, Floor(FVector(100.0, 100.0, 0.0)), Builder), EBuildResult::BlocksBuilder,
		TEXT("Cell under the builder"));

	// Walls: on the ground, and on a floor's boundary (the floor supports, it does not block).
	const FBuildingPlacementResult GroundWall = Subsystem->ValidatePlacement(WallClass, Wall(FVector(300.0, 205.0, 0.0)), Builder);
	if (Expect(GroundWall, EBuildResult::Success, TEXT("Wall on the ground")))
	{
		TestTrue(TEXT("Ground wall key and transform"), GroundWall.Keys == FB1Keys({ EdgeKey(EBuildingEdgeAxis::X, 1, 1) })
			&& GroundWall.Placement.WorldTransform.GetLocation().Equals(FVector(300.0, 200.0, 0.0), 0.1));
	}
	ABuildingTestPieceActor* PlacedFloor = Fixture.SpawnPiece(FloorClass, OnGround.Placement, OnGround.Keys, EBuildingPieceState::Active, true);
	if (!TestTrue(TEXT("Active floor piece registered"), PlacedFloor && Subsystem->GetOccupyingPiece(FloorKey(1, 1)) == PlacedFloor->GetBuildingPieceComponent())) { return false; }
	const FBuildingPlacementResult FloorWall = Subsystem->ValidatePlacement(WallClass, Wall(FVector(300.0, 205.0, 0.0)), Builder);
	if (Expect(FloorWall, EBuildResult::Success, TEXT("Wall on the floor's edge")))
	{
		TestTrue(TEXT("The wall stands on the floor top, same layer"), FMath::IsNearlyEqual(FloorWall.Placement.SurfaceZ, 10.f, 0.1f)
			&& FloorWall.Placement.Layer == 0);
	}
	Expect(Subsystem->ValidatePlacement(FloorClass, Floor(FVector(300.0, 300.0, 0.0)), Builder), EBuildResult::Occupied, TEXT("Floor on a floor"));
	Expect(Subsystem->ValidatePlacement(FloorClass, Floor(FVector(500.0, 300.0, 0.0)), Builder), EBuildResult::Success, TEXT("Floor next to a floor"));
	PlacedFloor->GetBuildingPieceComponent()->SetPieceState(EBuildingPieceState::Removing);
	Expect(Subsystem->ValidatePlacement(WallClass, Wall(FVector(300.0, 395.0, 0.0)), Builder), EBuildResult::Blocked,
		TEXT("A floor that is not Active does not support"));
	PlacedFloor->GetBuildingPieceComponent()->SetPieceState(EBuildingPieceState::Active);

	// Read-only: validation never changes the registry.
	const int32 Occupied = Subsystem->GetOccupiedKeyCount();
	const int32 ReservedCount = Subsystem->GetReservedKeyCount();
	Subsystem->ValidatePlacement(FloorClass, Floor(FVector(500.0, 300.0, 0.0)), Builder);
	TestTrue(TEXT("Validation is read-only"), Subsystem->GetOccupiedKeyCount() == Occupied && Subsystem->GetReservedKeyCount() == ReservedCount);
	TestTrue(TEXT("Validation is deterministic"), Subsystem->ValidatePlacement(FloorClass, Floor(FVector(300.0, -100.0, 0.0)), Builder).Diagnostic
		== Subsystem->ValidatePlacement(FloorClass, Floor(FVector(300.0, -100.0, 0.0)), Builder).Diagnostic);
	Fixture.Audit(*this, TEXT("Placement validation"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBuildingB1OverlapOnlyVolumesTest,
	"NullTide.Building.B1.OverlapOnlyVolumes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuildingB1OverlapOnlyVolumesTest::RunTest(const FString& Parameters)
{
	FB1World Fixture;
	if (!Fixture.IsValid(*this)) { return false; }
	UBuildingSubsystem* Subsystem = Fixture.Subsystem;
	AActor* Ground = Fixture.SpawnBox(FVector(300.0, 0.0, -50.0), FVector(700.0, 700.0, 50.0));
	ACharacter* Builder = Fixture.SpawnCharacter(FVector(0.0, 0.0, 92.0));
	if (!TestTrue(TEXT("World set up"), Ground && Builder)) { return false; }
	const TSubclassOf<UBuildingDefinition> FloorClass = UBuildingTestFloorDefinition::StaticClass();
	const FBuildingPlacement Cell = UBuildingSubsystem::MakePlacementFromWorld(B1Floor(), FVector(300.0, 300.0, 0.0), 0);
	auto SpawnVolume = [](UShapeComponent* Shape, const FVector& Location)
	{
		AActor* Owner = Shape->GetOwner();
		Owner->SetRootComponent(Shape);
		Owner->AddInstanceComponent(Shape);
		Shape->SetWorldLocation(Location);
		Shape->RegisterComponent();
		return Owner;
	};
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Mirrors BP_PickUpItem "Sphere" (read through MCP for B2): WorldDynamic, QueryOnly, overlaps Pawn only, radius 32.
	AActor* PickupActor = Fixture.World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	USphereComponent* Sphere = NewObject<USphereComponent>(PickupActor, TEXT("Sphere"));
	Sphere->SetSphereRadius(32.f);
	Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Sphere->SetGenerateOverlapEvents(true);
	SpawnVolume(Sphere, FVector(300.0, 300.0, 32.0));

	// Mirrors the InteractionBounds of ACraftingStationActor and AStorageContainerActor: QueryOnly, overlaps every channel.
	AActor* BoundsActor = Fixture.World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	UBoxComponent* Bounds = NewObject<UBoxComponent>(BoundsActor, TEXT("InteractionBounds"));
	Bounds->SetBoxExtent(FVector(150.0, 120.0, 110.0));
	Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Bounds->SetCollisionResponseToAllChannels(ECR_Overlap);
	Bounds->SetGenerateOverlapEvents(true);
	SpawnVolume(Bounds, FVector(300.0, 300.0, 90.0));

	const FBuildingPlacementResult WithVolumes = Subsystem->ValidatePlacement(FloorClass, Cell, Builder);
	TestTrue(FString::Printf(TEXT("Overlap-only pickup sphere and interaction bounds never block (%s)"), *WithVolumes.Diagnostic),
		WithVolumes.IsSuccess());

	AActor* Control = Fixture.SpawnBox(FVector(350.0, 350.0, 30.0), FVector(15.0, 15.0, 30.0), FRotator::ZeroRotator, TEXT("BlockAllDynamic"));
	const FBuildingPlacementResult WithControl = Subsystem->ValidatePlacement(FloorClass, Cell, Builder);
	TestTrue(TEXT("A BlockAllDynamic control in the same cell blocks"), WithControl.Result == EBuildResult::Blocked && WithControl.BlockingActor == Control);
	Fixture.Audit(*this, TEXT("Overlap-only volumes"));
	return true;
}

#endif
