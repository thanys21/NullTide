#pragma once

#include "CoreMinimal.h"
#include "Building/BuildingDefinition.h"
#include "Building/BuildingDoorActor.h"
#include "Building/BuildingPieceActor.h"
#include "Building/BuildingPlacementComponent.h"
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

	void SetCursor(const FVector& Location) { CursorLocation = Location; bHasCursor = true; }
	void SetHudClass(TSubclassOf<UBuildingHudWidget> InClass) { HudWidgetClass = InClass; }

protected:
	virtual bool GetCursorWorldLocation(FVector& OutLocation) const override;
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
