#pragma once

#include "CoreMinimal.h"
#include "BuildingTypes.generated.h"

class AActor;

/** UI grouping and placement defaults of a building. */
UENUM(BlueprintType)
enum class EBuildingCategory : uint8
{
	Foundation,
	Wall,
	Door,
	Furniture,
	Station
};

/** How a building snaps to the grid: onto cells (floors) or onto cell edges (walls and doors). */
UENUM(BlueprintType)
enum class EBuildingPlacementMode : uint8
{
	GridCell,
	GridEdge
};

/** Independent occupancy key spaces. Floors never conflict with the walls on their edges. */
UENUM(BlueprintType)
enum class EBuildingOccupancyCategory : uint8
{
	Floor,
	Edge,
	Furniture
};

/** Direction a grid edge runs along. None for cell keys. */
UENUM(BlueprintType)
enum class EBuildingEdgeAxis : uint8
{
	None,
	X,
	Y
};

/** Lifecycle of a placed piece. Only Active pieces are visible, collidable and counted as occupancy. */
UENUM(BlueprintType)
enum class EBuildingPieceState : uint8
{
	Provisional,
	Active,
	Removing
};

/**
 * Deterministic placement and construction result codes. Values are only ever appended.
 * B1 produces the validation codes; B2 appends the construction transaction codes.
 */
UENUM(BlueprintType, meta = (ScriptName = "BuildResultCode"))
enum class EBuildResult : uint8
{
	Success,
	/** Null, abstract or invalid building definition. */
	InvalidDefinition,
	/** The placement does not match the definition (mode, axis, rotation) or no builder was given. */
	InvalidPlacement,
	/** The piece center is farther than the build range from the builder. */
	OutOfRange,
	/** A surface sample found no ground or supporting floor. */
	NoGround,
	/** A surface sample is steeper than the maximum slope. */
	TooSteep,
	/** The surface samples differ by more than the maximum unevenness. */
	Uneven,
	/** An occupancy key is already reserved or occupied. */
	Occupied,
	/** A blocking actor (world, pawn or physics body) overlaps the piece. */
	Blocked,
	/** The piece would overlap the builder. */
	BlocksBuilder
};

/** One occupancy slot: a floor cell, a canonical edge or a furniture cell, on one height layer. */
USTRUCT(BlueprintType)
struct NULLTIDE_API FBuildingOccupancyKey
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Building")
	EBuildingOccupancyCategory Category = EBuildingOccupancyCategory::Floor;

	UPROPERTY(BlueprintReadOnly, Category = "Building")
	EBuildingEdgeAxis Axis = EBuildingEdgeAxis::None;

	UPROPERTY(BlueprintReadOnly, Category = "Building")
	int32 X = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Building")
	int32 Y = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Building")
	int32 Layer = 0;

	FBuildingOccupancyKey() = default;
	FBuildingOccupancyKey(EBuildingOccupancyCategory InCategory, EBuildingEdgeAxis InAxis, int32 InX, int32 InY, int32 InLayer)
		: Category(InCategory), Axis(InAxis), X(InX), Y(InY), Layer(InLayer)
	{
	}

	bool operator==(const FBuildingOccupancyKey& Other) const
	{
		return Category == Other.Category && Axis == Other.Axis && X == Other.X && Y == Other.Y && Layer == Other.Layer;
	}

	bool operator!=(const FBuildingOccupancyKey& Other) const { return !(*this == Other); }

	FString ToString() const;

	friend uint32 GetTypeHash(const FBuildingOccupancyKey& Key)
	{
		uint32 Hash = GetTypeHash(static_cast<uint8>(Key.Category));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Key.Axis)));
		Hash = HashCombine(Hash, GetTypeHash(Key.X));
		Hash = HashCombine(Hash, GetTypeHash(Key.Y));
		return HashCombine(Hash, GetTypeHash(Key.Layer));
	}
};

/** Identity of one reservation. Ids are unique per subsystem and never reused; 0 is invalid. */
USTRUCT(BlueprintType)
struct NULLTIDE_API FBuildingReservationHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Building")
	int64 Id = 0;

	bool IsValid() const { return Id != 0; }
	bool operator==(const FBuildingReservationHandle& Other) const { return Id == Other.Id; }
};

/**
 * A grid placement request and, after validation, its resolved result.
 * Inputs: PlacementMode, Anchor, EdgeAxis, Rotation and SurfaceZ (a height hint, for example the cursor hit).
 * Outputs (filled by validation, never trusted from callers): SurfaceZ, Layer, WorldTransform.
 */
USTRUCT(BlueprintType)
struct NULLTIDE_API FBuildingPlacement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Building")
	EBuildingPlacementMode PlacementMode = EBuildingPlacementMode::GridCell;

	/** GridCell: the anchor cell. GridEdge: the canonical edge coordinate. */
	UPROPERTY(BlueprintReadWrite, Category = "Building")
	FIntPoint Anchor = FIntPoint::ZeroValue;

	/** None for GridCell; X or Y for GridEdge. */
	UPROPERTY(BlueprintReadWrite, Category = "Building")
	EBuildingEdgeAxis EdgeAxis = EBuildingEdgeAxis::None;

	/** Quarter turns 0-3. GridCell: yaw = Rotation * 90. GridEdge: odd values flip the facing by 180. */
	UPROPERTY(BlueprintReadWrite, Category = "Building")
	int32 Rotation = 0;

	/** Input: height hint for the surface traces. Output: the resolved support surface height. */
	UPROPERTY(BlueprintReadWrite, Category = "Building")
	float SurfaceZ = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Building")
	int32 Layer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Building")
	FTransform WorldTransform = FTransform::Identity;
};

/** Read-only placement validation result. */
USTRUCT(BlueprintType)
struct NULLTIDE_API FBuildingPlacementResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Building")
	EBuildResult Result = EBuildResult::InvalidDefinition;

	/** The resolved placement (surface, layer and transform recomputed by the subsystem). */
	UPROPERTY(BlueprintReadOnly, Category = "Building")
	FBuildingPlacement Placement;

	/** Occupancy keys the placement would occupy (empty when it could not be resolved). */
	UPROPERTY(BlueprintReadOnly, Category = "Building")
	TArray<FBuildingOccupancyKey> Keys;

	/** The actor that blocks the placement (Blocked, BlocksBuilder), if any. */
	UPROPERTY(BlueprintReadOnly, Category = "Building")
	TObjectPtr<AActor> BlockingActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Building")
	FString Diagnostic;

	bool IsSuccess() const { return Result == EBuildResult::Success; }
};
