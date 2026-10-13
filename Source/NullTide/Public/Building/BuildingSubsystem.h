#pragma once

#include "CoreMinimal.h"
#include "Building/BuildingTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "BuildingSubsystem.generated.h"

class AActor;
class UBuildingDefinition;
class UBuildingPieceComponent;

/**
 * World grid, occupancy and placement-validation authority for building.
 *
 * Grid contract: cells of CellSize centimetres aligned to the world origin (0, 0); cell (ix, iy) covers
 * [ix*S, (ix+1)*S) x [iy*S, (iy+1)*S). Edges have one canonical key: an X edge (ix, iy) runs along X from
 * (ix*S, iy*S); a Y edge (ix, iy) runs along Y from (ix*S, iy*S). Heights map to layers of LayerHeight.
 *
 * The subsystem is the only writer of the occupancy registry. Keys are reserved all-or-nothing by an owner,
 * then committed to a piece or released; finalized pieces can also register directly. Validation is read-only.
 */
UCLASS()
class NULLTIDE_API UBuildingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr double CellSize = 200.0;
	static constexpr double LayerHeight = 50.0;
	static constexpr double MaxUnevenness = 25.0;
	static constexpr double MaxSlopeDegrees = 30.0;
	static constexpr double FloorThickness = 10.0;
	static constexpr double WallHeight = 250.0;
	static constexpr double WallThickness = 20.0;
	static constexpr double DefaultBuildRange = 600.0;
	/** Surface samples are taken this far inside the footprint corners or edge ends. */
	static constexpr double SampleInset = 5.0;
	/** Collision boxes are shrunk by this much so touching neighbours never block each other. */
	static constexpr double CollisionClearance = 2.0;
	/** The builder's capsule must stay this far from a new piece. */
	static constexpr double BuilderClearance = 5.0;
	/** Surface traces run from the height hint plus this distance to the hint minus this distance. */
	static constexpr double TraceHalfHeight = 300.0;

	static UBuildingSubsystem* Get(const UObject* WorldContextObject);

	// ---- Grid math (pure and deterministic) ----

	/** Floor division: negative coordinates map correctly and a boundary belongs to the cell on its positive side. */
	static FIntPoint WorldToCell(const FVector& WorldLocation);
	static FVector CellToWorldCenter(const FIntPoint& Cell, double Z = 0.0);
	/** Nearest edge of the cell containing the point; ties resolve South, West, North, East. Returns the canonical key. */
	static void SnapToEdge(const FVector& WorldLocation, EBuildingEdgeAxis& OutAxis, FIntPoint& OutEdge);
	static FVector EdgeToWorldCenter(EBuildingEdgeAxis Axis, const FIntPoint& Edge, double Z = 0.0);
	static int32 NormalizeRotation(int32 Rotation);
	/** Footprint after Rotation quarter turns (odd turns swap width and height). */
	static FIntPoint GetRotatedFootprint(const FIntPoint& Footprint, int32 Rotation);
	static int32 HeightToLayer(double SurfaceZ);
	/** Exact multiple of 90 degrees: GridCell Rotation * 90; GridEdge 0 (X) or 90 (Y), plus 180 for odd Rotation. */
	static double GetPlacementYaw(const FBuildingPlacement& Placement);
	/** Snaps a world point (for example a cursor hit) to a placement request; its Z becomes the height hint. */
	static FBuildingPlacement MakePlacementFromWorld(const UBuildingDefinition& Definition, const FVector& WorldLocation, int32 Rotation);
	/** Occupancy keys of a placement on its Layer. */
	static TArray<FBuildingOccupancyKey> ComputeOccupancyKeys(const UBuildingDefinition& Definition, const FBuildingPlacement& Placement);
	/** Actor transform: footprint center or edge center at SurfaceZ, with the placement yaw. */
	static FTransform ComputeWorldTransform(const UBuildingDefinition& Definition, const FBuildingPlacement& Placement);

	// ---- Occupancy registry (the subsystem is the only writer) ----

	/**
	 * Reserves every key for Owner, or none: fails (invalid handle, no change) when Owner is invalid, Keys is empty
	 * or contains duplicates, or any key is reserved or occupied. Handle ids are unique and never reused.
	 */
	FBuildingReservationHandle ReserveKeys(const UObject* Owner, const TArray<FBuildingOccupancyKey>& Keys);

	/** Releases a reservation held by Owner. A stale, unknown or foreign handle releases nothing. */
	bool ReleaseReservation(const FBuildingReservationHandle& Handle, const UObject* Owner);

	/**
	 * Converts exactly the reservation's keys into occupancy by Piece and consumes the handle. Fails without change
	 * if the handle is stale or foreign, Piece is invalid or already registered, or Piece's keys differ from the
	 * reserved keys. A handle can be committed only once.
	 */
	bool CommitReservation(const FBuildingReservationHandle& Handle, const UObject* Owner, UBuildingPieceComponent* Piece);

	/** Registers a finalized piece's own keys all-or-nothing. Repeating it with the same keys is a no-op. */
	bool RegisterActivePiece(UBuildingPieceComponent* Piece);

	/** Removes every key held by the piece. Safe for destroyed or never-registered pieces. */
	void UnregisterPiece(const UBuildingPieceComponent* Piece);

	/** Free = no entry, a dead holder, or the reservation being ignored. */
	bool IsKeyFree(const FBuildingOccupancyKey& Key, const FBuildingReservationHandle& IgnoreReservation = FBuildingReservationHandle()) const;
	UBuildingPieceComponent* GetOccupyingPiece(const FBuildingOccupancyKey& Key) const;
	bool IsPieceRegistered(const UBuildingPieceComponent* Piece) const;
	bool IsReservationActive(const FBuildingReservationHandle& Handle) const;
	int32 GetOccupiedKeyCount() const;
	int32 GetReservedKeyCount() const;

	/** Drops reservations whose owner is gone and occupancy whose piece is gone. Returns the number of keys freed. */
	int32 PurgeStaleEntries();

	/** Every internal inconsistency or stale entry as a message; empty when the registry is consistent and clean. */
	TArray<FString> AuditRegistry() const;

	// ---- Placement validation (read-only) ----

	/**
	 * Resolves and validates a placement for Builder. Checks, in order: definition, placement shape (mode, axis,
	 * rotation, builder), build range, support surface (NoGround, Blocked by a non-support surface, TooSteep,
	 * Uneven), occupancy (IgnoreReservation is treated as free), world and pawn collision, builder safety.
	 * The returned placement recomputes SurfaceZ, Layer and WorldTransform; only the requested anchor, axis,
	 * rotation and height hint are used. Changes nothing.
	 */
	FBuildingPlacementResult ValidatePlacement(TSubclassOf<UBuildingDefinition> DefinitionClass, const FBuildingPlacement& Requested,
		const AActor* Builder, const FBuildingReservationHandle& IgnoreReservation = FBuildingReservationHandle(),
		const AActor* IgnoreActor = nullptr) const;

	virtual void Deinitialize() override;

private:
	struct FOccupant
	{
		bool bReserved = false;
		int64 ReservationId = 0;
		FObjectKey HolderKey;
		TWeakObjectPtr<const UObject> Holder;
	};

	struct FReservation
	{
		FObjectKey OwnerKey;
		TWeakObjectPtr<const UObject> Owner;
		TArray<FBuildingOccupancyKey> Keys;
	};

	struct FPieceRecord
	{
		TWeakObjectPtr<const UBuildingPieceComponent> Piece;
		TArray<FBuildingOccupancyKey> Keys;
	};

	bool IsOccupantAlive(const FOccupant& Occupant) const;
	static bool HasDuplicateKeys(const TArray<FBuildingOccupancyKey>& Keys);
	static bool SameKeySet(const TArray<FBuildingOccupancyKey>& A, const TArray<FBuildingOccupancyKey>& B);
	static TArray<FVector2D> GetSurfaceSamplePoints(const UBuildingDefinition& Definition, const FBuildingPlacement& Placement);
	static FBox GetCollisionBox(const UBuildingDefinition& Definition, const FBuildingPlacement& Placement);

	TMap<FBuildingOccupancyKey, FOccupant> Occupancy;
	TMap<int64, FReservation> Reservations;
	TMap<FObjectKey, FPieceRecord> Pieces;
	int64 NextReservationId = 1;
};
