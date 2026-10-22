#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseZoneActor.generated.h"

class UBaseZoneComponent;
class UBoxComponent;

/**
 * A base zone placed in a level (Phase 6 S2). Placing one and setting its size is all it takes to allow building
 * there. ZoneBounds shows the exact snapped rectangle in the editor (no collision, hidden in game). The zone follows
 * the world-aligned building grid: actor rotation and scale are ignored. Always loaded in a partitioned world, so
 * building never depends on streaming.
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

	/** Height of the editor outline box. */
	static constexpr double BoundsHalfHeight = 100.0;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base|Zone")
	TObjectPtr<USceneComponent> ZoneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base|Zone")
	TObjectPtr<UBaseZoneComponent> Zone;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base|Zone")
	TObjectPtr<UBoxComponent> ZoneBounds;
};
