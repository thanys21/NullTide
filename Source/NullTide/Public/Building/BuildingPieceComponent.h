#pragma once

#include "CoreMinimal.h"
#include "Building/BuildingTypes.h"
#include "Components/ActorComponent.h"
#include "BuildingPieceComponent.generated.h"

class UBuildingDefinition;

/**
 * Marks its owner as a placed building piece: which building it is, where it sits on the grid and its lifecycle
 * state. It never writes the occupancy registry itself; UBuildingSubsystem is the only registry writer. An
 * Active piece that already knows its keys registers them on BeginPlay, and every piece unregisters on EndPlay
 * and on component destruction, so destroyed pieces never leave occupancy behind.
 */
UCLASS(ClassGroup = (Building), meta = (BlueprintSpawnableComponent))
class NULLTIDE_API UBuildingPieceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuildingPieceComponent();

	UFUNCTION(BlueprintPure, Category = "Building|Piece")
	TSubclassOf<UBuildingDefinition> GetBuildingDefinition() const { return BuildingDefinition; }

	UFUNCTION(BlueprintPure, Category = "Building|Piece")
	EBuildingPieceState GetPieceState() const { return PieceState; }

	UFUNCTION(BlueprintPure, Category = "Building|Piece")
	bool IsActivePiece() const { return PieceState == EBuildingPieceState::Active; }

	UFUNCTION(BlueprintPure, Category = "Building|Piece")
	FBuildingPlacement GetPlacement() const { return Placement; }

	/** The occupancy keys this piece claims (registered only while the subsystem says so). */
	UFUNCTION(BlueprintPure, Category = "Building|Piece")
	TArray<FBuildingOccupancyKey> GetOccupancyKeys() const { return OccupancyKeys; }

	UFUNCTION(BlueprintPure, Category = "Building|Piece")
	bool IsPlayerBuilt() const { return bPlayerBuilt; }

	/**
	 * Native setup used before the piece is registered: by the construction authority (B2) before FinishSpawning,
	 * or by authored pieces and tests. Refused (returns false, changes nothing) while the subsystem holds keys
	 * for this piece, so registered occupancy can never be rewritten.
	 */
	bool InitializePiece(TSubclassOf<UBuildingDefinition> InDefinition, const FBuildingPlacement& InPlacement,
		const TArray<FBuildingOccupancyKey>& InKeys, EBuildingPieceState InState, bool bInPlayerBuilt);

	/** Lifecycle state changes (Provisional -> Active -> Removing). Does not touch the registry. */
	void SetPieceState(EBuildingPieceState NewState) { PieceState = NewState; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	void UnregisterFromSubsystem();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Piece", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UBuildingDefinition> BuildingDefinition;

	UPROPERTY(VisibleInstanceOnly, Category = "Building|Piece")
	EBuildingPieceState PieceState = EBuildingPieceState::Active;

	UPROPERTY(VisibleInstanceOnly, Category = "Building|Piece")
	FBuildingPlacement Placement;

	UPROPERTY(VisibleInstanceOnly, Category = "Building|Piece")
	TArray<FBuildingOccupancyKey> OccupancyKeys;

	UPROPERTY(VisibleInstanceOnly, Category = "Building|Piece")
	bool bPlayerBuilt = false;
};
