#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BaseZoneComponent.generated.h"

/**
 * Turns its owner into a base zone (Phase 6 S2): a rectangle of SizeInCells building-grid cells centred on the owner's
 * cell, where the player may build. The outermost ring of cells is never buildable, so the outside of a base stays
 * walkable however the player builds; walls and doors may stand on every edge of the buildable cells. The rectangle is
 * fixed when the zone registers with UBaseZoneSubsystem on BeginPlay, and it unregisters on EndPlay. Zones are
 * authored for the game and never removed during play. The component never builds or touches pieces or inventories.
 * It registers on BeginPlay whatever its owner is: a future buildable zone source (a Base Flag piece) must register
 * only once its piece is Active, or its own provisional spawn would make its placement buildable.
 */
UCLASS(ClassGroup = (Base), meta = (BlueprintSpawnableComponent))
class NULLTIDE_API UBaseZoneComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Smallest zone: one buildable cell inside its ring. */
	static constexpr int32 MinSizeInCells = 3;
	/** Largest zone side (2 km), far below integer overflow of the cell rectangle. */
	static constexpr int32 MaxSizeInCells = 1000;

	/**
	 * Zone cells (Max exclusive) of Size, clamped to [MinSizeInCells, MaxSizeInCells], centred on CenterCell. Odd sizes
	 * are centred; an even size adds its extra row or column on +X or +Y (the outline shows the result).
	 */
	static FIntRect ComputeZoneCells(const FIntPoint& CenterCell, const FIntPoint& Size);

	/** The zone cells without their outermost ring. */
	static FIntRect ComputeBuildableCells(const FIntRect& ZoneCells);

	/** Zone length (X) and width (Y) in building-grid cells, outer ring included. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base|Zone", meta = (ClampMin = "3", ClampMax = "1000"))
	FIntPoint SizeInCells = FIntPoint(15, 15);

	/** The zone cells for the owner's current location (the registered rectangle while registered). */
	UFUNCTION(BlueprintPure, Category = "Base|Zone")
	FIntRect GetZoneCells() const;

	UFUNCTION(BlueprintPure, Category = "Base|Zone")
	FIntRect GetBuildableCells() const;

	UFUNCTION(BlueprintPure, Category = "Base|Zone")
	bool IsZoneRegistered() const { return bRegistered; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FIntRect ComputeCurrentZoneCells() const;

	/** Fixed while registered. */
	FIntRect RegisteredZoneCells;
	bool bRegistered = false;
};
