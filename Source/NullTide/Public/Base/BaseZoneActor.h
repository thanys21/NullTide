#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseZoneActor.generated.h"

class UBaseZoneComponent;
class UBoxComponent;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * A base zone placed in a level (Phase 6 S2). Placing one and setting its size is all it takes to allow building
 * there. ZoneBounds shows the exact snapped rectangle in the editor (no collision, hidden in game). The ring strips
 * (S3) show the zone's outer ring, where nothing can be built, in game and in the editor; they never collide, block
 * traces or affect navigation. The zone follows the world-aligned building grid: actor rotation and scale are ignored.
 * Always loaded in a partitioned world, so building never depends on streaming.
 */
UCLASS()
class NULLTIDE_API ABaseZoneActor : public AActor
{
	GENERATED_BODY()

public:
	ABaseZoneActor();

	virtual void OnConstruction(const FTransform& Transform) override;

	UBaseZoneComponent* GetZone() const { return Zone; }
	UBoxComponent* GetZoneBounds() const { return ZoneBounds; }

	/** South, north, west, east strips. */
	TArray<UStaticMeshComponent*> GetRingStrips() const;

	/** World-space XY boxes of the ring strips over a zone: south and north span the full width, west and east lie between them. */
	static TArray<FBox2D> ComputeRingStrips(const FIntRect& ZoneCells);

	/** Height of the editor outline box. */
	static constexpr double BoundsHalfHeight = 100.0;
	/** Ring strips are this thick and their underside this far above the zone actor's height. */
	static constexpr double RingThickness = 2.0;
	static constexpr double RingLift = 1.0;

	/** Material of the ring strips (default MI_BaseZoneRing, translucent red). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base|Zone")
	TObjectPtr<UMaterialInterface> RingMaterial;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base|Zone")
	TObjectPtr<USceneComponent> ZoneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base|Zone")
	TObjectPtr<UBaseZoneComponent> Zone;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base|Zone")
	TObjectPtr<UBoxComponent> ZoneBounds;

	UPROPERTY(VisibleAnywhere, Category = "Base|Zone")
	TObjectPtr<UStaticMeshComponent> RingSouth;

	UPROPERTY(VisibleAnywhere, Category = "Base|Zone")
	TObjectPtr<UStaticMeshComponent> RingNorth;

	UPROPERTY(VisibleAnywhere, Category = "Base|Zone")
	TObjectPtr<UStaticMeshComponent> RingWest;

	UPROPERTY(VisibleAnywhere, Category = "Base|Zone")
	TObjectPtr<UStaticMeshComponent> RingEast;

private:
	UStaticMeshComponent* CreateRingStrip(const FName Name, UStaticMesh* Mesh);
};
