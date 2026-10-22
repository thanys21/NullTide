#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BaseZoneSubsystem.generated.h"

class UBaseZoneComponent;
class UBuildingDefinition;
struct FBuildingPlacement;

/**
 * The only registry of base zones in a world (Phase 6 S2). Zones register themselves (UBaseZoneComponent); building
 * asks whether a placement is buildable. Zones combine by union of their buildable rectangles and cover every height
 * layer. Queries are read-only, exact on whole grid cells and independent of registration order. The subsystem never
 * builds and never touches pieces or inventories.
 */
UCLASS()
class NULLTIDE_API UBaseZoneSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBaseZoneSubsystem* Get(const UObject* WorldContextObject);

	/** Adds a zone (idempotent). False for a null zone. */
	bool RegisterZone(UBaseZoneComponent* Zone);

	/** Removes a zone; unknown or destroyed zones are ignored. */
	void UnregisterZone(const UBaseZoneComponent* Zone);

	/** Live registered zones, ordered by their zone rectangle (then path name), not by registration order. */
	TArray<UBaseZoneComponent*> GetZones() const;

	/** Whether any zone covers the cell, its outer ring included. */
	bool IsCellInZone(const FIntPoint& Cell) const;

	/** Whether any zone's buildable rectangle (the zone without its outer ring) covers the cell. */
	bool IsCellBuildable(const FIntPoint& Cell) const;

	/** The first zone (in GetZones order) covering the location's cell, or null. */
	UBaseZoneComponent* FindZoneAt(const FVector& WorldLocation) const;

	/**
	 * Whether a placement may be built: every footprint cell of a cell piece is buildable; an edge piece borders at
	 * least one buildable cell (so every edge of a buildable cell takes a wall).
	 */
	bool IsPlacementBuildable(const UBuildingDefinition& Definition, const FBuildingPlacement& Placement) const;

	/** Drops destroyed zones. Returns how many were dropped. */
	int32 PurgeStaleZones();

	int32 GetRegisteredZoneCount() const { return Zones.Num(); }

	virtual void Deinitialize() override;

private:
	TArray<TWeakObjectPtr<UBaseZoneComponent>> Zones;
};
