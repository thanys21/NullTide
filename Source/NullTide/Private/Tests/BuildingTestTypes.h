#pragma once

#include "CoreMinimal.h"
#include "Building/BuildingDefinition.h"
#include "Building/BuildingCraftingStationActor.h"
#include "Building/BuildingDoorActor.h"
#include "Building/BuildingPieceActor.h"
#include "Building/BuildingPlacementComponent.h"
#include "Building/BuildingStorageActor.h"
#include "Building/ConstructionComponent.h"
#include "BuildingTestTypes.generated.h"

class UBuildingPieceComponent;

/** Concrete piece for native building tests: a 200 x 200 x 10 cm floor slab (WorldDynamic, blocks pawns). */
UCLASS(Transient, NotBlueprintable)
class ABuildingTestPieceActor : public ABuildingPieceActor
{
	GENERATED_BODY()

public:
	ABuildingTestPieceActor();
};

/** Valid 1 x 1 test floor (Foundation, GridCell). */
UCLASS(Transient, NotBlueprintable)
class UBuildingTestFloorDefinition : public UBuildingDefinition
{
	GENERATED_BODY()

public:
	UBuildingTestFloorDefinition();
};

/** Valid 2 x 1 test floor, for footprint rotation. */
UCLASS(Transient, NotBlueprintable)
class UBuildingTestWideFloorDefinition : public UBuildingDefinition
{
	GENERATED_BODY()

public:
	UBuildingTestWideFloorDefinition();
};

/** Valid test wall (Wall, GridEdge). */
UCLASS(Transient, NotBlueprintable)
class UBuildingTestWallDefinition : public UBuildingDefinition
{
	GENERATED_BODY()

public:
	UBuildingTestWallDefinition();
};

/** Valid test door (Door, GridEdge); shares the Edge key space with walls. */
UCLASS(Transient, NotBlueprintable)
class UBuildingTestDoorDefinition : public UBuildingDefinition
{
	GENERATED_BODY()

public:
	UBuildingTestDoorDefinition();
};

/** Valid test furniture (GridCell, Furniture key space). */
UCLASS(Transient, NotBlueprintable)
class UBuildingTestFurnitureDefinition : public UBuildingDefinition
{
	GENERATED_BODY()

public:
	UBuildingTestFurnitureDefinition();
};

/** A plain reservation owner for registry tests. */
UCLASS(Transient, NotBlueprintable)
class UBuildingTestReservationOwner : public UObject
{
	GENERATED_BODY()
};

// ---- B2 construction fixtures ----

/** A plain actor (not an ABuildingPieceActor) carrying a piece component: valid for B1, unsupported for construction. */
UCLASS(Transient, NotBlueprintable)
class ABuildingPlainPieceTestActor : public AActor
{
	GENERATED_BODY()

public:
	ABuildingPlainPieceTestActor();

	UPROPERTY()
	TObjectPtr<UBuildingPieceComponent> BuildingPiece;
};

/** Destroys itself while spawning (PostInitializeComponents runs inside FinishSpawning). */
UCLASS(Transient, NotBlueprintable)
class ABuildingSelfDestroyTestPiece : public ABuildingTestPieceActor
{
	GENERATED_BODY()

public:
	virtual void PostInitializeComponents() override;
};

/** Moves itself off its spawn transform in OnConstruction. */
UCLASS(Transient, NotBlueprintable)
class ABuildingDriftTestPiece : public ABuildingTestPieceActor
{
	GENERATED_BODY()

public:
	virtual void OnConstruction(const FTransform& Transform) override;
};

/** Spawns a blocking box inside its own cell (off the surface samples) while spawning. */
UCLASS(Transient, NotBlueprintable)
class ABuildingBlockerSpawnTestPiece : public ABuildingTestPieceActor
{
	GENERATED_BODY()

public:
	virtual void PostInitializeComponents() override;
};

/** Test floor built from a plain actor with a piece component. */
UCLASS(Transient, NotBlueprintable)
class UBuildingTestPlainActorDefinition : public UBuildingDefinition
{
	GENERATED_BODY()

public:
	UBuildingTestPlainActorDefinition();
};

UCLASS(Transient, NotBlueprintable)
class UBuildingTestSelfDestroyDefinition : public UBuildingDefinition
{
	GENERATED_BODY()

public:
	UBuildingTestSelfDestroyDefinition();
};

UCLASS(Transient, NotBlueprintable)
class UBuildingTestDriftDefinition : public UBuildingDefinition
{
	GENERATED_BODY()

public:
	UBuildingTestDriftDefinition();
};

UCLASS(Transient, NotBlueprintable)
class UBuildingTestBlockerSpawnDefinition : public UBuildingDefinition
{
	GENERATED_BODY()

public:
	UBuildingTestBlockerSpawnDefinition();
};

/** Construction authority with the two protected seams opened for deterministic fault injection. */
UCLASS(Transient, NotBlueprintable)
class UConstructionTestComponent : public UConstructionComponent
{
	GENERATED_BODY()

public:
	bool bSpawnReturnsNull = false;
	/** Runs inside ActivatePiece before activation (the piece is still provisional). */
	TFunction<void(ABuildingPieceActor&)> BeforeActivate;
	/** Runs inside ActivatePiece after a successful activation (the piece is Active and unpaid). */
	TFunction<void(ABuildingPieceActor&)> AfterActivate;
	int32 SpawnCount = 0;

protected:
	virtual ABuildingPieceActor* SpawnProvisionalPiece(TSubclassOf<UBuildingDefinition> DefinitionClass,
		const FBuildingPlacementResult& Resolved) override;
	virtual bool ActivatePiece(ABuildingPieceActor& Piece, const FBuildingReservationHandle& Handle) override;
};

/** Placement component with a scripted cursor and a viewport-free HUD (a transient world has no local player). */
UCLASS(Transient, NotBlueprintable)
class UBuildingPlacementTestComponent : public UBuildingPlacementComponent
{
	GENERATED_BODY()

public:
	bool bHasCursor = true;
	FVector CursorLocation = FVector::ZeroVector;
	int32 HudCreateCount = 0;
	/** A transient world's player controller has no local player; tests decide whether it counts as local. */
	bool bTreatControllerAsLocal = true;

	/** Demolish mode: the actor "under the cursor". */
	TWeakObjectPtr<AActor> CursorHitActor;

	void SetCursor(const FVector& Location) { CursorLocation = Location; bHasCursor = true; }
	void SetHudClass(TSubclassOf<UBuildingHudWidget> InClass) { HudWidgetClass = InClass; }

	/** B8: whether the cursor counts as over the build bar (the confirm guard). */
	bool bCursorOverBuildBar = false;

	/** The LMB confirm input path, with its build-bar guard (B8). */
	void PressConfirm() { HandleConfirmInput(); }

protected:
	virtual bool IsCursorOverBuildBar() const override { return bCursorOverBuildBar; }
	virtual bool GetCursorWorldLocation(FVector& OutLocation) const override;
	virtual AActor* GetCursorHitActor() const override { return CursorHitActor.Get(); }
	virtual bool IsLocalBuilderController(const APlayerController& PlayerController) const override { return bTreatControllerAsLocal; }
	virtual UBuildingHudWidget* CreateHudWidget(APlayerController& PlayerController) override;
};

/** Records inventory and build-result notifications and runs optional hooks inside them. */
UCLASS(Transient, NotBlueprintable)
class UConstructionTestListener : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleInventoryChanged(int32 NewRevision);

	UFUNCTION()
	void HandleBuildResult(TSubclassOf<UBuildingDefinition> DefinitionClass, const FBuildingConstructionResult& Result);

	UFUNCTION()
	void HandleBuildingModeChanged(bool bBuildingModeActive);

	UFUNCTION()
	void HandleDemolishResult(TSubclassOf<UBuildingDefinition> DefinitionClass, const FBuildingDemolishResult& Result);

	TArray<FBuildingDemolishResult> DemolishResults;
	TArray<bool> ModeChanges;
	TFunction<void()> InventoryHook;
	TFunction<void(const FBuildingConstructionResult&)> BuildResultHook;
	int32 InventoryEvents = 0;
	TArray<FBuildingConstructionResult> BuildResults;
	/** Whether the result's Piece was valid when this listener received it. */
	TArray<bool> PieceValidOnReceipt;
};

// ---- B5 door fixtures ----

/** Concrete native door (the production layout) with a settable swing duration. */
UCLASS(Transient, NotBlueprintable)
class ABuildingTestDoorActor : public ABuildingDoorActor
{
	GENERATED_BODY()

public:
	void SetSwingDuration(float InSwingDuration) { SwingDuration = InSwingDuration; }
};

/** Test door (Door, GridEdge) built from the native test door actor; costs Wood x 4. */
UCLASS(Transient, NotBlueprintable)
class UBuildingTestDoorPieceDefinition : public UBuildingDefinition
{
	GENERATED_BODY()

public:
	UBuildingTestDoorPieceDefinition();
};

/** Test door whose hinge is turned and scaled, so the ghost's attach-chain composition order matters. */
UCLASS(Transient, NotBlueprintable)
class ABuildingTestTurnedHingeDoorActor : public ABuildingTestDoorActor
{
	GENERATED_BODY()

public:
	ABuildingTestTurnedHingeDoorActor();
};

// ---- B6 demolition fixtures ----

class UStorageComponent;
class UCraftingStationComponent;

/** A thin 200 x 20 x 250 cm wall piece (the test piece's slab turned into a wall). */
UCLASS(Transient, NotBlueprintable)
class ABuildingTestWallPieceActor : public ABuildingTestPieceActor
{
	GENERATED_BODY()

public:
	ABuildingTestWallPieceActor();
};

/** A wall piece that is a storage: demolition must always refuse it. */
UCLASS(Transient, NotBlueprintable)
class ABuildingStorageTestPiece : public ABuildingTestWallPieceActor
{
	GENERATED_BODY()

public:
	ABuildingStorageTestPiece();

	UPROPERTY()
	TObjectPtr<UStorageComponent> Storage;
};

/** A floor-sized piece that is a crafting station (its station kind is set by the test). */
UCLASS(Transient, NotBlueprintable)
class ABuildingStationTestPiece : public ABuildingTestPieceActor
{
	GENERATED_BODY()

public:
	ABuildingStationTestPiece();

	UPROPERTY()
	TObjectPtr<UCraftingStationComponent> Station;
};

/** Floor costing Wood x 1: its refund rounds to nothing. */
UCLASS(Transient, NotBlueprintable)
class UBuildingTestTinyFloorDefinition : public UBuildingDefinition
{
	GENERATED_BODY()

public:
	UBuildingTestTinyFloorDefinition();
};

/** Wall that needs a floor (the production rule), on the thin test wall piece; Wood x 6. */
UCLASS(Transient, NotBlueprintable)
class UBuildingTestSupportedWallDefinition : public UBuildingDefinition
{
	GENERATED_BODY()

public:
	UBuildingTestSupportedWallDefinition();
};

/** Storage wall that needs a floor; Wood x 2. */
UCLASS(Transient, NotBlueprintable)
class UBuildingTestStorageWallDefinition : public UBuildingDefinition
{
	GENERATED_BODY()

public:
	UBuildingTestStorageWallDefinition();
};

/** Station on a cell; Wood x 2. */
UCLASS(Transient, NotBlueprintable)
class UBuildingTestStationDefinition : public UBuildingDefinition
{
	GENERATED_BODY()

public:
	UBuildingTestStationDefinition();
};

// ---- B7 chest and workbench fixtures ----

/** Concrete native chest (the production layout). */
UCLASS(Transient, NotBlueprintable)
class ABuildingTestChestActor : public ABuildingStorageActor
{
	GENERATED_BODY()
};

/** Concrete native workbench (the production layout); the test sets its station kind. */
UCLASS(Transient, NotBlueprintable)
class ABuildingTestBenchActor : public ABuildingCraftingStationActor
{
	GENERATED_BODY()
};

/** Chest on a floor (Furniture, GridCell, requires a floor); Wood x 2. */
UCLASS(Transient, NotBlueprintable)
class UBuildingTestChestDefinition : public UBuildingDefinition
{
	GENERATED_BODY()

public:
	UBuildingTestChestDefinition();
};

/** Workbench (Station, GridCell, ground or floor); Wood x 2. */
UCLASS(Transient, NotBlueprintable)
class UBuildingTestBenchDefinition : public UBuildingDefinition
{
	GENERATED_BODY()

public:
	UBuildingTestBenchDefinition();
};
