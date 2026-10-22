#include "BaseTestTypes.h"
#include "BaseZoneTestTypes.h"
#include "BuildingTestTypes.h"
#include "InventoryComponentTestTypes.h"

#include "Base/BaseZoneActor.h"
#include "Base/BaseZoneComponent.h"
#include "Base/BaseZoneSubsystem.h"
#include "Building/BuildingPlacementComponent.h"
#include "Building/BuildingSubsystem.h"
#include "Building/ConstructionComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Base/BaseZonePresenceComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/InventoryComponent.h"
#include "Materials/MaterialInstance.h"
#include "Misc/AutomationTest.h"
#include "UI/BaseZoneTagWidget.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
/** Zone A of the S2 worlds: 5 x 5 cells around the origin (cells -2..2, buildable -1..1). */
const FIntPoint S2ZoneSize(5, 5);

const TCHAR* S3PlayerPath = TEXT("/Game/TopDown/Blueprints/BP_TopDownCharacter.BP_TopDownCharacter");
const TCHAR* S3TagClassPath = TEXT("/Game/LevelPrototyping/BaseSystem/UI/W_BaseZoneTag.W_BaseZoneTag_C");
const TCHAR* S3RingMaterialPath = TEXT("/Game/LevelPrototyping/BaseSystem/Materials/MI_BaseZoneRing.MI_BaseZoneRing");

FString S2Name(const EBuildResult Result)
{
	return StaticEnum<EBuildResult>()->GetNameStringByValue(static_cast<int64>(Result));
}

FBuildingPlacement S2Cell(const FIntPoint& Cell, TSubclassOf<UBuildingDefinition> Definition = UBuildingTestFloorDefinition::StaticClass(),
	int32 Rotation = 0)
{
	return UBuildingSubsystem::MakePlacementFromWorld(*Definition->GetDefaultObject<UBuildingDefinition>(),
		UBuildingSubsystem::CellToWorldCenter(Cell), Rotation);
}

FBuildingPlacement S2Edge(const EBuildingEdgeAxis Axis, const FIntPoint& Edge)
{
	FBuildingPlacement Placement;
	Placement.PlacementMode = EBuildingPlacementMode::GridEdge;
	Placement.EdgeAxis = Axis;
	Placement.Anchor = Edge;
	return Placement;
}

FVector S2CellLocation(const FIntPoint& Cell)
{
	return UBuildingSubsystem::CellToWorldCenter(Cell);
}

/**
 * Transient world with no zone of its own (unlike the building fixtures) and ground x [-400, 600], y [-700, 700],
 * top at 0: cell -3 (x -600..-400) has no ground.
 */
struct FS2World
{
	UWorld* World = nullptr;
	UBuildingSubsystem* Building = nullptr;
	UBaseZoneSubsystem* Zones = nullptr;
	TArray<TStrongObjectPtr<UConstructionTestListener>> Listeners;

	explicit FS2World(bool bWithGround = true)
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!World)
		{
			return;
		}
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		Building = World->GetSubsystem<UBuildingSubsystem>();
		Zones = World->GetSubsystem<UBaseZoneSubsystem>();
		if (bWithGround)
		{
			SpawnGround(FVector(100.0, 0.0, -50.0), FVector(500.0, 700.0, 50.0));
		}
	}

	~FS2World()
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
		return Test.TestTrue(TEXT("S2 test world with building and zone subsystems"), World && Building && Zones);
	}

	void SpawnGround(const FVector& Center, const FVector& HalfExtent) const
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor, TEXT("Ground"));
		Mesh->SetStaticMesh(Cube);
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		Actor->SetRootComponent(Mesh);
		Actor->AddInstanceComponent(Mesh);
		Mesh->SetWorldTransform(FTransform(FRotator::ZeroRotator, Center, HalfExtent / 50.0));
		Mesh->RegisterComponent();
	}

	ABaseZoneActor* SpawnZone(const FIntPoint& CenterCell, const FIntPoint& Size) const
	{
		return NullTideBaseTests::SpawnBaseZone(World, S2CellLocation(CenterCell), Size);
	}

	ACharacter* SpawnPawn() const
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<ACharacter>(ACharacter::StaticClass(), FVector(0.0, 0.0, 92.0), FRotator::ZeroRotator, Params);
	}

	int32 CountPieces() const
	{
		int32 Count = 0;
		for (TActorIterator<ABuildingPieceActor> It(World); It; ++It)
		{
			++Count;
		}
		return Count;
	}
};

bool S2Expect(FAutomationTestBase& Test, const TCHAR* What, const FBuildingPlacementResult& Result, const EBuildResult Expected)
{
	const bool bMatch = Result.Result == Expected;
	Test.TestTrue(FString::Printf(TEXT("%s: %s (got %s, %s)"), What, *S2Name(Expected), *S2Name(Result.Result), *Result.Diagnostic), bMatch);
	return bMatch;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseS2ZoneMathTest,
	"NullTide.Base.S2.ZoneMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBaseS2ZoneMathTest::RunTest(const FString& Parameters)
{
	struct FCase { const TCHAR* Name; FIntPoint Center; FIntPoint Size; FIntRect Zone; FIntRect Buildable; };
	const FCase Cases[] = {
		{ TEXT("15 x 15 at the origin"), FIntPoint(0, 0), FIntPoint(15, 15), FIntRect(-7, -7, 8, 8), FIntRect(-6, -6, 7, 7) },
		{ TEXT("4 x 6 (even sizes add +X / +Y)"), FIntPoint(2, -3), FIntPoint(4, 6), FIntRect(1, -5, 5, 1), FIntRect(2, -4, 4, 0) },
		{ TEXT("3 x 3: one buildable cell"), FIntPoint(-5, -5), FIntPoint(3, 3), FIntRect(-6, -6, -3, -3), FIntRect(-5, -5, -4, -4) },
		{ TEXT("Too small clamps to 3 x 3"), FIntPoint(0, 0), FIntPoint(1, 0), FIntRect(-1, -1, 2, 2), FIntRect(0, 0, 1, 1) },
		{ TEXT("Too large clamps to 1000"), FIntPoint(0, 0), FIntPoint(MAX_int32, 1000), FIntRect(-499, -499, 501, 501), FIntRect(-498, -498, 500, 500) },
	};
	for (const FCase& Case : Cases)
	{
		const FIntRect Zone = UBaseZoneComponent::ComputeZoneCells(Case.Center, Case.Size);
		const FIntRect Buildable = UBaseZoneComponent::ComputeBuildableCells(Zone);
		TestTrue(FString::Printf(TEXT("%s: zone %s, buildable %s"), Case.Name, *Zone.ToString(), *Buildable.ToString()),
			Zone == Case.Zone && Buildable == Case.Buildable);
	}

	const FIntRect Zone = UBaseZoneComponent::ComputeZoneCells(FIntPoint(0, 0), FIntPoint(15, 15));
	const FIntRect Buildable = UBaseZoneComponent::ComputeBuildableCells(Zone);
	TestTrue(TEXT("The ring cells are in the zone but not buildable"), Zone.Contains(FIntPoint(-7, 0)) && Zone.Contains(FIntPoint(7, 7))
		&& !Buildable.Contains(FIntPoint(-7, 0)) && !Buildable.Contains(FIntPoint(7, 7)) && !Buildable.Contains(FIntPoint(0, -7)));
	TestTrue(TEXT("The cells next to the ring are buildable"), Buildable.Contains(FIntPoint(-6, 0)) && Buildable.Contains(FIntPoint(6, 6)));
	TestTrue(TEXT("Cells beyond the zone are outside"), !Zone.Contains(FIntPoint(8, 0)) && !Zone.Contains(FIntPoint(0, -8)));
	TestTrue(TEXT("13 x 13 buildable cells"), Buildable.Width() == 13 && Buildable.Height() == 13);
	TestTrue(TEXT("The default size is 15 x 15"), GetDefault<UBaseZoneComponent>()->SizeInCells == FIntPoint(15, 15));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseS2RegistryTest,
	"NullTide.Base.S2.Registry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBaseS2RegistryTest::RunTest(const FString& Parameters)
{
	FS2World Fixture(false);
	if (!Fixture.IsValid(*this))
	{
		return false;
	}
	UBaseZoneSubsystem* Zones = Fixture.Zones;
	TestTrue(TEXT("No zone, no buildable cell"), Zones->GetRegisteredZoneCount() == 0 && !Zones->IsCellBuildable(FIntPoint(0, 0)));

	// B first: the order of the queries must not depend on registration order.
	ABaseZoneActor* B = Fixture.SpawnZone(FIntPoint(5, 0), S2ZoneSize);
	ABaseZoneActor* A = Fixture.SpawnZone(FIntPoint(0, 0), S2ZoneSize);
	if (!TestTrue(TEXT("Two zones spawned"), A && B))
	{
		return false;
	}
	TestTrue(TEXT("Zones register on BeginPlay"), Zones->GetRegisteredZoneCount() == 2 && A->GetZone()->IsZoneRegistered()
		&& B->GetZone()->IsZoneRegistered());
	TestTrue(TEXT("Registering again is a no-op"), Zones->RegisterZone(A->GetZone()) && Zones->GetRegisteredZoneCount() == 2);
	const TArray<UBaseZoneComponent*> Ordered = Zones->GetZones();
	TestTrue(TEXT("GetZones is ordered by rectangle, not registration"), Ordered.Num() == 2 && Ordered[0] == A->GetZone()
		&& Ordered[1] == B->GetZone());

	TestTrue(TEXT("Zone A cells: ring in zone, inner cells buildable"), Zones->IsCellInZone(FIntPoint(2, 2)) && !Zones->IsCellBuildable(FIntPoint(2, 2))
		&& Zones->IsCellBuildable(FIntPoint(1, 1)) && Zones->IsCellBuildable(FIntPoint(-1, -1)) && !Zones->IsCellBuildable(FIntPoint(-2, 0)));
	TestTrue(TEXT("Zone B's buildable cells are buildable too"), Zones->IsCellBuildable(FIntPoint(4, 0)) && Zones->IsCellBuildable(FIntPoint(6, 1)));
	TestTrue(TEXT("Touching zones keep both rings: a 2-cell strip between them"), Zones->IsCellInZone(FIntPoint(2, 0))
		&& Zones->IsCellInZone(FIntPoint(3, 0)) && !Zones->IsCellBuildable(FIntPoint(2, 0)) && !Zones->IsCellBuildable(FIntPoint(3, 0)));
	TestTrue(TEXT("Outside every zone"), !Zones->IsCellInZone(FIntPoint(0, 3)) && !Zones->IsCellInZone(FIntPoint(-3, 0)));
	TestTrue(TEXT("FindZoneAt"), Zones->FindZoneAt(S2CellLocation(FIntPoint(2, 0))) == A->GetZone()
		&& Zones->FindZoneAt(S2CellLocation(FIntPoint(3, 0))) == B->GetZone() && !Zones->FindZoneAt(S2CellLocation(FIntPoint(0, 5))));

	// Edge rule: an edge needs at least one buildable cell beside it.
	const UBuildingDefinition& Wall = *GetDefault<UBuildingTestWallDefinition>();
	struct FEdgeCase { const TCHAR* Name; EBuildingEdgeAxis Axis; FIntPoint Edge; bool bAllowed; };
	const FEdgeCase Edges[] = {
		{ TEXT("North side of buildable (1, 1)"), EBuildingEdgeAxis::X, FIntPoint(1, 2), true },
		{ TEXT("South side of buildable (1, -1)"), EBuildingEdgeAxis::X, FIntPoint(1, -1), true },
		{ TEXT("East side of buildable (1, 0)"), EBuildingEdgeAxis::Y, FIntPoint(2, 0), true },
		{ TEXT("West side of buildable (-1, 0)"), EBuildingEdgeAxis::Y, FIntPoint(-1, 0), true },
		{ TEXT("Between two buildable cells"), EBuildingEdgeAxis::X, FIntPoint(0, 0), true },
		{ TEXT("Inside the ring (between ring cells)"), EBuildingEdgeAxis::Y, FIntPoint(-1, 2), false },
		{ TEXT("The zone's outer line (south)"), EBuildingEdgeAxis::X, FIntPoint(0, -2), false },
		{ TEXT("The zone's outer line (west)"), EBuildingEdgeAxis::Y, FIntPoint(-2, 0), false },
		{ TEXT("Between the two rings"), EBuildingEdgeAxis::Y, FIntPoint(3, 0), false },
	};
	for (const FEdgeCase& Case : Edges)
	{
		TestEqual(Case.Name, Zones->IsPlacementBuildable(Wall, S2Edge(Case.Axis, Case.Edge)), Case.bAllowed);
	}

	// A registered zone keeps its rectangle when its size or actor changes later (R-S2-3).
	A->GetZone()->SizeInCells = FIntPoint(41, 41);
	A->SetActorLocation(S2CellLocation(FIntPoint(-20, -20)));
	TestTrue(TEXT("The registered rectangle is fixed"), A->GetZone()->GetZoneCells() == FIntRect(-2, -2, 3, 3)
		&& Zones->IsCellBuildable(FIntPoint(1, 1)) && !Zones->IsCellBuildable(FIntPoint(-20, -20)));

	// Re-registering A after B does not change any query order.
	Zones->UnregisterZone(A->GetZone());
	TestTrue(TEXT("Unregistered zone A no longer counts"), Zones->GetRegisteredZoneCount() == 1 && !Zones->IsCellBuildable(FIntPoint(0, 0)));
	Zones->UnregisterZone(A->GetZone());
	TestTrue(TEXT("Unregistering again is a no-op"), Zones->GetRegisteredZoneCount() == 1);
	Zones->RegisterZone(A->GetZone());
	TestTrue(TEXT("Same order after re-registering"), Zones->GetZones().Num() == 2 && Zones->GetZones()[0] == A->GetZone()
		&& Zones->FindZoneAt(S2CellLocation(FIntPoint(2, 0))) == A->GetZone());

	B->Destroy();
	TestTrue(TEXT("A destroyed zone unregisters (EndPlay)"), Zones->GetRegisteredZoneCount() == 1 && !Zones->IsCellBuildable(FIntPoint(4, 0)));

	UBaseZoneComponent* Loose = NewObject<UBaseZoneComponent>(GetTransientPackage());
	TestTrue(TEXT("A loose zone registers"), Zones->RegisterZone(Loose) && Zones->GetRegisteredZoneCount() == 2);
	Loose->MarkAsGarbage();
	TestEqual(TEXT("A stale zone is purged"), Zones->PurgeStaleZones(), 1);
	TestTrue(TEXT("Only zone A is left"), Zones->GetZones().Num() == 1 && Zones->GetZones()[0] == A->GetZone());
	TestFalse(TEXT("A null zone is refused"), Zones->RegisterZone(nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseS2PlacementRuleTest,
	"NullTide.Base.S2.PlacementRule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBaseS2PlacementRuleTest::RunTest(const FString& Parameters)
{
	FS2World Fixture;
	if (!Fixture.IsValid(*this))
	{
		return false;
	}
	ACharacter* Builder = Fixture.SpawnPawn();
	if (!TestNotNull(TEXT("Builder"), Builder))
	{
		return false;
	}
	UBuildingSubsystem* Building = Fixture.Building;
	const TSubclassOf<UBuildingDefinition> Floor = UBuildingTestFloorDefinition::StaticClass();
	const TSubclassOf<UBuildingDefinition> Wide = UBuildingTestWideFloorDefinition::StaticClass();
	const TSubclassOf<UBuildingDefinition> Wall = UBuildingTestWallDefinition::StaticClass();

	S2Expect(*this, TEXT("No zone at all"), Building->ValidatePlacement(Floor, S2Cell(FIntPoint(1, 0)), Builder), EBuildResult::OutsideBuildZone);

	Fixture.SpawnZone(FIntPoint(0, 0), S2ZoneSize);
	S2Expect(*this, TEXT("A buildable cell"), Building->ValidatePlacement(Floor, S2Cell(FIntPoint(1, 0)), Builder), EBuildResult::Success);
	S2Expect(*this, TEXT("A ring cell"), Building->ValidatePlacement(Floor, S2Cell(FIntPoint(2, 0)), Builder), EBuildResult::OutsideBuildZone);
	S2Expect(*this, TEXT("A ring corner"), Building->ValidatePlacement(Floor, S2Cell(FIntPoint(-2, -2)), Builder), EBuildResult::OutsideBuildZone);
	S2Expect(*this, TEXT("A 2 x 1 floor half on the ring"), Building->ValidatePlacement(Wide, S2Cell(FIntPoint(1, 0), Wide), Builder),
		EBuildResult::OutsideBuildZone);
	S2Expect(*this, TEXT("The same floor turned inward"), Building->ValidatePlacement(Wide, S2Cell(FIntPoint(1, 0), Wide, 1), Builder),
		EBuildResult::Success);

	S2Expect(*this, TEXT("Wall on the north side of the buildable area"), Building->ValidatePlacement(Wall, S2Edge(EBuildingEdgeAxis::X, FIntPoint(1, 2)), Builder),
		EBuildResult::Success);
	S2Expect(*this, TEXT("Wall on the south side"), Building->ValidatePlacement(Wall, S2Edge(EBuildingEdgeAxis::X, FIntPoint(1, -1)), Builder),
		EBuildResult::Success);
	S2Expect(*this, TEXT("Wall on the east side"), Building->ValidatePlacement(Wall, S2Edge(EBuildingEdgeAxis::Y, FIntPoint(2, 0)), Builder),
		EBuildResult::Success);
	S2Expect(*this, TEXT("Wall on the west side"), Building->ValidatePlacement(Wall, S2Edge(EBuildingEdgeAxis::Y, FIntPoint(-1, 0)), Builder),
		EBuildResult::Success);
	S2Expect(*this, TEXT("Wall inside the ring"), Building->ValidatePlacement(Wall, S2Edge(EBuildingEdgeAxis::Y, FIntPoint(2, -2)), Builder),
		EBuildResult::OutsideBuildZone);
	S2Expect(*this, TEXT("Wall on the zone's outer line"), Building->ValidatePlacement(Wall, S2Edge(EBuildingEdgeAxis::X, FIntPoint(0, -2)), Builder),
		EBuildResult::OutsideBuildZone);

	S2Expect(*this, TEXT("Out of range wins over the zone"), Building->ValidatePlacement(Floor, S2Cell(FIntPoint(3, 0)), Builder),
		EBuildResult::OutOfRange);
	S2Expect(*this, TEXT("Outside the zone wins over no ground"), Building->ValidatePlacement(Floor, S2Cell(FIntPoint(-3, 0)), Builder),
		EBuildResult::OutsideBuildZone);
	Fixture.SpawnZone(FIntPoint(-3, 0), FIntPoint(3, 3));
	S2Expect(*this, TEXT("The same cell in a second zone reaches the ground check"), Building->ValidatePlacement(Floor, S2Cell(FIntPoint(-3, 0)), Builder),
		EBuildResult::NoGround);

	TestTrue(TEXT("Validation changed nothing"), Building->GetOccupiedKeyCount() == 0 && Building->GetReservedKeyCount() == 0
		&& Building->AuditRegistry().IsEmpty() && Fixture.Zones->GetRegisteredZoneCount() == 2 && Fixture.CountPieces() == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseS2BuildOutsideZoneTest,
	"NullTide.Base.S2.BuildOutsideZone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBaseS2BuildOutsideZoneTest::RunTest(const FString& Parameters)
{
	FS2World Fixture;
	if (!Fixture.IsValid(*this))
	{
		return false;
	}
	Fixture.SpawnZone(FIntPoint(0, 0), S2ZoneSize);
	ACharacter* Pawn = Fixture.SpawnPawn();
	if (!TestNotNull(TEXT("Builder"), Pawn))
	{
		return false;
	}
	UInventoryComponent* Inventory = NewObject<UInventoryComponent>(Pawn);
	Pawn->AddInstanceComponent(Inventory);
	Inventory->RegisterComponent();
	UConstructionTestComponent* Construction = NewObject<UConstructionTestComponent>(Pawn);
	Pawn->AddInstanceComponent(Construction);
	Construction->RegisterComponent();
	FArrayProperty* Buildings = FindFProperty<FArrayProperty>(UConstructionComponent::StaticClass(), TEXT("AvailableBuildings"));
	if (!TestNotNull(TEXT("AvailableBuildings property"), Buildings))
	{
		return false;
	}
	*Buildings->ContainerPtrToValuePtr<TArray<TSubclassOf<UBuildingDefinition>>>(Construction) = { UBuildingTestFloorDefinition::StaticClass() };
	Pawn->DispatchBeginPlay();
	const TSubclassOf<UItemDefinition> Wood = UInventoryStackableTestItemDefinition::StaticClass();
	Inventory->TryAddDefinitionQuantity(Wood, 20);
	UConstructionTestListener* Listener = NewObject<UConstructionTestListener>(GetTransientPackage());
	Fixture.Listeners.Emplace(Listener);
	Inventory->OnInventoryChanged.AddDynamic(Listener, &UConstructionTestListener::HandleInventoryChanged);
	Construction->OnBuildResult.AddDynamic(Listener, &UConstructionTestListener::HandleBuildResult);

	const TSubclassOf<UBuildingDefinition> Floor = UBuildingTestFloorDefinition::StaticClass();
	const int32 Revision = Inventory->GetRevision();
	const FBuildingConstructionResult Preview = Construction->CanBuild(Floor, S2Cell(FIntPoint(2, 0)));
	TestTrue(FString::Printf(TEXT("CanBuild on a ring cell: OutsideBuildZone (got %s)"), *S2Name(Preview.Result)),
		Preview.Result == EBuildResult::OutsideBuildZone);
	const FBuildingConstructionResult Refused = Construction->TryBuild(Floor, S2Cell(FIntPoint(2, 0)));
	TestTrue(FString::Printf(TEXT("TryBuild on a ring cell: OutsideBuildZone (got %s)"), *S2Name(Refused.Result)),
		Refused.Result == EBuildResult::OutsideBuildZone);
	TestTrue(TEXT("Nothing spawned, paid or registered"), Construction->SpawnCount == 0 && Fixture.CountPieces() == 0
		&& Inventory->GetRevision() == Revision && Inventory->GetTotalQuantity(Wood) == 20 && Listener->InventoryEvents == 0
		&& Fixture.Building->GetOccupiedKeyCount() == 0 && Fixture.Building->GetReservedKeyCount() == 0);
	TestTrue(TEXT("OnBuildResult fired once with the refusal"), Listener->BuildResults.Num() == 1
		&& Listener->BuildResults[0].Result == EBuildResult::OutsideBuildZone);

	const FBuildingConstructionResult Built = Construction->TryBuild(Floor, S2Cell(FIntPoint(1, 0)));
	TestTrue(FString::Printf(TEXT("One cell inward builds (got %s)"), *S2Name(Built.Result)), Built.Result == EBuildResult::Success);
	TestTrue(TEXT("Paid once, one piece, one more result"), Inventory->GetTotalQuantity(Wood) == 16 && Fixture.CountPieces() == 1
		&& Listener->BuildResults.Num() == 2 && Fixture.Building->AuditRegistry().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseS2ProductionRulesTest,
	"NullTide.Base.S2.ProductionRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBaseS2ProductionRulesTest::RunTest(const FString& Parameters)
{
	const ABaseZoneActor* Defaults = GetDefault<ABaseZoneActor>();
#if WITH_EDITOR
	TestFalse(TEXT("Zone actors are always loaded (not spatially loaded)"), Defaults->GetIsSpatiallyLoaded());
#endif
	TestTrue(TEXT("Default zone size 15 x 15"), Defaults->GetZone() && Defaults->GetZone()->SizeInCells == FIntPoint(15, 15));
	const UBoxComponent* Bounds = Defaults->GetZoneBounds();
	TestTrue(TEXT("The outline has no collision, no overlaps, no navigation and is hidden in game"), Bounds
		&& Bounds->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Bounds->GetGenerateOverlapEvents()
		&& !Bounds->CanEverAffectNavigation() && Bounds->bHiddenInGame);
	TestFalse(TEXT("Zone actors never tick"), Defaults->PrimaryActorTick.bCanEverTick);
	TestEqual(TEXT("HUD text"), UBuildingPlacementComponent::DescribeResult(EBuildResult::OutsideBuildZone).ToString(),
		FString(TEXT("Outside the build zone")));

	// The outline matches the snapped rectangle of a placed zone.
	FS2World Fixture(false);
	if (!Fixture.IsValid(*this))
	{
		return false;
	}
	ABaseZoneActor* Zone = Fixture.SpawnZone(FIntPoint(1, -2), FIntPoint(4, 3));
	if (!TestNotNull(TEXT("Zone"), Zone))
	{
		return false;
	}
	// Cells x 0..3, y -3..-1: 800 x 600 cm centred on (400, -300), whatever the actor's rotation.
	auto OutlineMatches = [this, Zone](const TCHAR* What)
	{
		const FVector Center = Zone->GetZoneBounds()->GetComponentLocation();
		const FVector Extent = Zone->GetZoneBounds()->GetUnscaledBoxExtent();
		TestTrue(FString::Printf(TEXT("%s: outline centre %s, extent %s"), What, *Center.ToString(), *Extent.ToString()),
			Center.Equals(FVector(400.0, -300.0, Center.Z), 0.01) && Extent.Equals(FVector(400.0, 300.0, ABaseZoneActor::BoundsHalfHeight), 0.01)
			&& Zone->GetZoneBounds()->GetComponentRotation().IsNearlyZero(0.01));
	};
	OutlineMatches(TEXT("After spawning"));
	Zone->SetActorRotation(FRotator(0.0, 30.0, 0.0));
	Zone->OnConstruction(Zone->GetActorTransform());
	OutlineMatches(TEXT("A rotated actor"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseS3RingGeometryTest,
	"NullTide.Base.S3.RingGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBaseS3RingGeometryTest::RunTest(const FString& Parameters)
{
	// The strips cover exactly the ring cells, each once.
	struct FCase { FIntPoint Center; FIntPoint Size; };
	const FCase Cases[] = { { FIntPoint(0, 0), FIntPoint(5, 5) }, { FIntPoint(-5, -5), FIntPoint(3, 3) }, { FIntPoint(2, -3), FIntPoint(4, 6) } };
	for (const FCase& Case : Cases)
	{
		const FIntRect Zone = UBaseZoneComponent::ComputeZoneCells(Case.Center, Case.Size);
		const FIntRect Buildable = UBaseZoneComponent::ComputeBuildableCells(Zone);
		const TArray<FBox2D> Strips = ABaseZoneActor::ComputeRingStrips(Zone);
		int32 Wrong = 0;
		for (int32 Y = Zone.Min.Y - 1; Y <= Zone.Max.Y; ++Y)
		{
			for (int32 X = Zone.Min.X - 1; X <= Zone.Max.X; ++X)
			{
				const FVector Center = S2CellLocation(FIntPoint(X, Y));
				int32 Covered = 0;
				for (const FBox2D& Strip : Strips)
				{
					Covered += Strip.IsInside(FVector2D(Center.X, Center.Y)) ? 1 : 0;
				}
				const bool bRing = Zone.Contains(FIntPoint(X, Y)) && !Buildable.Contains(FIntPoint(X, Y));
				Wrong += Covered == (bRing ? 1 : 0) ? 0 : 1;
			}
		}
		TestTrue(FString::Printf(TEXT("%d x %d: 4 strips cover each ring cell once and nothing else (%d wrong cells)"),
			Case.Size.X, Case.Size.Y, Wrong), Strips.Num() == 4 && Wrong == 0);
	}

	FS2World Fixture;
	if (!Fixture.IsValid(*this))
	{
		return false;
	}
	ABaseZoneActor* Zone = Fixture.SpawnZone(FIntPoint(0, 0), S2ZoneSize);
	if (!TestNotNull(TEXT("Zone"), Zone))
	{
		return false;
	}
	const UMaterialInterface* RingMaterial = LoadObject<UMaterialInterface>(nullptr, S3RingMaterialPath);
	TestTrue(TEXT("The ring uses MI_BaseZoneRing"), RingMaterial && Zone->RingMaterial == RingMaterial);
	auto StripsMatch = [this, Zone, RingMaterial](const TCHAR* What)
	{
		const TArray<FBox2D> Expected = ABaseZoneActor::ComputeRingStrips(Zone->GetZone()->GetZoneCells());
		const TArray<UStaticMeshComponent*> Strips = Zone->GetRingStrips();
		bool bMatch = Strips.Num() == 4;
		for (int32 Index = 0; bMatch && Index < Strips.Num(); ++Index)
		{
			const FBox Bounds = Strips[Index]->Bounds.GetBox();
			bMatch = FVector2D(Bounds.Min.X, Bounds.Min.Y).Equals(Expected[Index].Min, 0.1)
				&& FVector2D(Bounds.Max.X, Bounds.Max.Y).Equals(Expected[Index].Max, 0.1)
				&& FMath::IsNearlyEqual(Bounds.Min.Z, ABaseZoneActor::RingLift, 0.1)
				&& FMath::IsNearlyEqual(Bounds.Max.Z, ABaseZoneActor::RingLift + ABaseZoneActor::RingThickness, 0.1)
				&& Strips[Index]->GetMaterial(0) == RingMaterial && !Strips[Index]->bHiddenInGame;
		}
		TestTrue(FString::Printf(TEXT("%s: the strips lie on the ring at the zone height, visible, with the ring material"), What), bMatch);
	};
	StripsMatch(TEXT("Spawned"));
	Zone->SetActorRotation(FRotator(0.0, 45.0, 0.0));
	Zone->OnConstruction(Zone->GetActorTransform());
	StripsMatch(TEXT("A rotated actor"));

	bool bInert = true;
	for (const UStaticMeshComponent* Strip : Zone->GetRingStrips())
	{
		bInert &= Strip->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Strip->GetGenerateOverlapEvents()
			&& !Strip->CanEverAffectNavigation() && !Strip->CastShadow;
	}
	TestTrue(TEXT("No collision, overlaps, navigation or shadow"), bInert);
	// A trace from above into the south strip (cell (0, -2) at 1..3 cm) reaches nothing but the ground below it.
	FHitResult Hit;
	const FVector Above = S2CellLocation(FIntPoint(0, -2)) + FVector(0.0, 0.0, 100.0);
	const FVector InStrip = S2CellLocation(FIntPoint(0, -2)) + FVector(0.0, 0.0, 2.0);
	TestFalse(TEXT("Traces pass through the ring (visibility)"), Fixture.World->LineTraceSingleByChannel(Hit, Above, InStrip, ECC_Visibility));
	TestFalse(TEXT("Traces pass through the ring (pawn)"), Fixture.World->LineTraceSingleByChannel(Hit, Above, InStrip, ECC_Pawn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseS3PresenceAndTagTest,
	"NullTide.Base.S3.PresenceAndTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBaseS3PresenceAndTagTest::RunTest(const FString& Parameters)
{
	UClass* TagClass = LoadClass<UBaseZoneTagWidget>(nullptr, S3TagClassPath);
	FS2World Fixture(false);
	if (!TestNotNull(TEXT("W_BaseZoneTag loads"), TagClass) || !Fixture.IsValid(*this))
	{
		return false;
	}
	ABaseZoneActor* Home = Fixture.SpawnZone(FIntPoint(0, 0), S2ZoneSize);
	ABaseZoneActor* Outpost = Fixture.SpawnZone(FIntPoint(5, 0), S2ZoneSize);
	if (!TestTrue(TEXT("Two touching zones"), Home && Outpost))
	{
		return false;
	}
	Outpost->GetZone()->ZoneName = FText::FromString(TEXT("Outpost"));

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Pawn = Fixture.World->SpawnActor<ACharacter>(ACharacter::StaticClass(), S2CellLocation(FIntPoint(20, 20)),
		FRotator::ZeroRotator, Params);
	APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>(APlayerController::StaticClass(), FTransform::Identity, Params);
	if (!TestTrue(TEXT("Pawn and controller"), Pawn && Controller))
	{
		return false;
	}
	UBaseZonePresenceTestComponent* Presence = NewObject<UBaseZonePresenceTestComponent>(Pawn);
	Presence->TestTagClass = TagClass;
	Presence->bTreatAsLocal = false;
	Pawn->AddInstanceComponent(Presence);
	Presence->RegisterComponent();
	Pawn->DispatchBeginPlay();
	TStrongObjectPtr<UBaseZoneTestListener> Listener(NewObject<UBaseZoneTestListener>());
	Presence->OnBaseZoneChanged.AddDynamic(Listener.Get(), &UBaseZoneTestListener::HandleZoneChanged);
	TestFalse(TEXT("No timer without a controller"), Presence->IsRefreshTimerActive());

	Controller->Possess(Pawn);
	TestFalse(TEXT("No timer for a controller without a local player"), Presence->IsRefreshTimerActive());
	Controller->UnPossess();
	Presence->bTreatAsLocal = true;
	Controller->Possess(Pawn);
	TestTrue(TEXT("Timer runs for a local player; outside every zone nothing is shown"), Presence->IsRefreshTimerActive()
		&& Listener->Changes.IsEmpty() && !Presence->GetTagWidget() && Presence->TagCreateCount == 0);

	auto MoveTo = [Pawn, Presence](const FIntPoint& Cell)
	{
		Pawn->SetActorLocation(S2CellLocation(Cell) + FVector(0.0, 0.0, 92.0));
		Presence->RefreshZone();
	};
	MoveTo(FIntPoint(1, 1));
	UBaseZoneTagWidget* Tag = Presence->GetTagWidget();
	TestTrue(TEXT("Entering Home: one change, the tag shows its name"), Listener->Changes.Num() == 1
		&& Listener->Changes[0].Get() == Home->GetZone() && Presence->GetCurrentZone() == Home->GetZone() && Tag
		&& Tag->GetZoneNameText().ToString() == TEXT("Base") && Tag->GetVisibility() == ESlateVisibility::HitTestInvisible);
	if (Tag)
	{
		const FAnchors Anchors = Tag->GetAnchorsInViewport();
		TestTrue(TEXT("The tag sits at the top right"), Anchors.Minimum.Equals(FVector2D(1.0, 0.0)) && Anchors.Maximum.Equals(FVector2D(1.0, 0.0))
			&& Tag->GetAlignmentInViewport().Equals(FVector2D(1.0, 0.0)));
	}
	MoveTo(FIntPoint(-2, 2));
	TestEqual(TEXT("Moving within Home (its ring included) changes nothing"), Listener->Changes.Num(), 1);
	MoveTo(FIntPoint(4, 0));
	TestTrue(TEXT("Into the touching Outpost: one change, its name"), Listener->Changes.Num() == 2
		&& Listener->Changes[1].Get() == Outpost->GetZone() && Tag && Tag->GetZoneNameText().ToString() == TEXT("Outpost"));
	MoveTo(FIntPoint(20, 0));
	TestTrue(TEXT("Leaving: one change to none, the tag collapses"), Listener->Changes.Num() == 3 && !Listener->Changes[2].IsValid()
		&& !Presence->GetCurrentZone() && Tag && Tag->GetVisibility() == ESlateVisibility::Collapsed);
	MoveTo(FIntPoint(0, 0));
	TestTrue(TEXT("Back in Home: the same tag is reused"), Listener->Changes.Num() == 4 && Presence->GetTagWidget() == Tag
		&& Presence->TagCreateCount == 1 && Tag->GetVisibility() == ESlateVisibility::HitTestInvisible);

	Controller->UnPossess();
	TestTrue(TEXT("Unpossessed: the timer stops and the zone is forgotten"), !Presence->IsRefreshTimerActive()
		&& Listener->Changes.Num() == 5 && !Listener->Changes[4].IsValid() && Tag->GetVisibility() == ESlateVisibility::Collapsed);
	MoveTo(FIntPoint(4, 0));
	TestEqual(TEXT("Without a local player, refreshing does nothing"), Listener->Changes.Num(), 5);

	// A zone destroyed under the player counts as leaving it (its weak pointer reads null like "no zone").
	MoveTo(FIntPoint(0, 0));
	Controller->Possess(Pawn);
	TestTrue(TEXT("Possessed again inside Home: the first lookup runs at once"), Listener->Changes.Num() == 6 && Listener->Changes[5].Get() == Home->GetZone()
		&& Tag->GetVisibility() == ESlateVisibility::HitTestInvisible);
	Home->Destroy();
	Presence->RefreshZone();
	TestTrue(TEXT("Home destroyed: one change to none, the tag collapses"), Listener->Changes.Num() == 7 && !Listener->Changes[6].IsValid()
		&& !Presence->GetCurrentZone() && Tag->GetVisibility() == ESlateVisibility::Collapsed);
	Presence->RefreshZone();
	TestEqual(TEXT("Still nothing afterwards"), Listener->Changes.Num(), 7);

	Pawn->Destroy();
	TestTrue(TEXT("EndPlay releases the tag"), !Presence->GetTagWidget() && !Presence->IsRefreshTimerActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBaseS3ProductionWiringTest,
	"NullTide.Base.S3.ProductionWiring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBaseS3ProductionWiringTest::RunTest(const FString& Parameters)
{
	const UBlueprint* PlayerBlueprint = LoadObject<UBlueprint>(nullptr, S3PlayerPath);
	UClass* TagClass = LoadClass<UBaseZoneTagWidget>(nullptr, S3TagClassPath);
	if (!TestTrue(TEXT("BP_TopDownCharacter and W_BaseZoneTag load"), PlayerBlueprint && TagClass))
	{
		return false;
	}
	TArray<const UBaseZonePresenceComponent*> Presences;
	if (const USimpleConstructionScript* ConstructionScript = PlayerBlueprint->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : ConstructionScript->GetAllNodes())
		{
			if (const UBaseZonePresenceComponent* Component = Node ? Cast<UBaseZonePresenceComponent>(Node->ComponentTemplate) : nullptr)
			{
				Presences.Add(Component);
			}
		}
	}
	FClassProperty* TagProperty = FindFProperty<FClassProperty>(UBaseZonePresenceComponent::StaticClass(), TEXT("TagWidgetClass"));
	if (TestEqual(TEXT("The player has exactly one BaseZonePresence"), Presences.Num(), 1) && TestNotNull(TEXT("TagWidgetClass"), TagProperty))
	{
		TestTrue(TEXT("Its tag is W_BaseZoneTag"), TagProperty->GetObjectPropertyValue_InContainer(Presences[0]) == TagClass);
	}

	FS2World Fixture(false);
	if (!Fixture.IsValid(*this))
	{
		return false;
	}
	const UBaseZoneTagWidget* Tag = CreateWidget<UBaseZoneTagWidget>(Fixture.World, TagClass);
	TestTrue(TEXT("W_BaseZoneTag binds ZoneNameText"), Tag && Tag->HasZoneNameText());
	TestEqual(TEXT("Default zone name"), GetDefault<UBaseZoneComponent>()->ZoneName.ToString(), FString(TEXT("Base")));

	const UMaterialInstance* Ring = Cast<UMaterialInstance>(GetDefault<ABaseZoneActor>()->RingMaterial);
	FLinearColor Color = FLinearColor::Black;
	const bool bHasColor = Ring && Ring->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Color")), Color);
	TestTrue(TEXT("The default ring material is MI_BaseZoneRing"), Ring && Ring->GetPathName() == S3RingMaterialPath);
	TestTrue(FString::Printf(TEXT("The ring is red (%s)"), *Color.ToString()), bHasColor && Color.R > 0.8f && Color.G < 0.3f && Color.B < 0.3f);
	return true;
}

#endif
