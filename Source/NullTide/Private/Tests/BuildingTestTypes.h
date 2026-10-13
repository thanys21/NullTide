#pragma once

#include "CoreMinimal.h"
#include "Building/BuildingDefinition.h"
#include "Building/BuildingPieceActor.h"
#include "BuildingTestTypes.generated.h"

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
