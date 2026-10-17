#pragma once

#include "CoreMinimal.h"
#include "Building/BuildingTypes.h"
#include "Inventory/InventoryTypes.h"
#include "UObject/Object.h"
#include "BuildingDefinition.generated.h"

class UTexture2D;

/**
 * Static data for one kind of building (Wooden Floor, Wall, Door). Each building is a data-only Blueprint
 * subclass, referenced by class and read from its default object, the same pattern as items, recipes and
 * crafting stations. Runtime state never lives here.
 */
UCLASS(Blueprintable, BlueprintType, Abstract, Const)
class NULLTIDE_API UBuildingDefinition : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText BuildingName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText BuildingDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TObjectPtr<UTexture2D> BuildingIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building")
	EBuildingCategory Category = EBuildingCategory::Foundation;

	/** Actor spawned for a placed piece. Must carry a UBuildingPieceComponent. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building")
	TSubclassOf<AActor> PieceActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building")
	EBuildingPlacementMode PlacementMode = EBuildingPlacementMode::GridCell;

	/** Size in grid cells (GridCell). GridEdge pieces occupy exactly one edge and must keep 1 x 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building")
	FIntPoint Footprint = FIntPoint(1, 1);

	/** Rotation increment in degrees. Only 90 is supported. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building", meta = (Units = "deg"))
	int32 RotationStep = 90;

	/** Resources consumed when the piece is built (canonical item definitions). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building")
	TArray<FInventoryDefinitionQuantity> Cost;

	/** Share of each cost line returned on demolition, rounded down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RefundFraction = 0.5f;

	/** Build range for this building; 0 uses the default range. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building", meta = (ClampMin = "0.0", Units = "cm"))
	float BuildRangeOverride = 0.f;

	/** GridEdge only: every surface sample on at least one side of the edge must be an Active floor (walls and doors). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building")
	bool bRequiresFloor = false;

	/** Reports the first invalid field as a diagnostic. */
	UFUNCTION(BlueprintPure, Category = "Building")
	bool ValidateBuilding(FString& OutDiagnostic) const;

	/** The occupancy key space this building uses. */
	UFUNCTION(BlueprintPure, Category = "Building")
	EBuildingOccupancyCategory GetOccupancyCategory() const;

	/** Whether an actor class carries a UBuildingPieceComponent (native default subobject or Blueprint component). */
	static bool ClassHasPieceComponent(const UClass* ActorClass);

	/** Default object of a concrete, valid building class; null (with a diagnostic) otherwise. */
	static const UBuildingDefinition* ResolveValidBuilding(TSubclassOf<UBuildingDefinition> BuildingClass, FString& OutDiagnostic);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
