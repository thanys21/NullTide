#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuildingPieceActor.generated.h"

class UBuildingPieceComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * Thin native host for a placed building piece: an unscaled root at the support surface, a mesh and a
 * UBuildingPieceComponent. Floors, walls and doors are Blueprint children that only set meshes and materials.
 * A provisional piece is hidden, has no collision, does not affect navigation and cannot be interacted with.
 */
UCLASS(Abstract, Blueprintable)
class NULLTIDE_API ABuildingPieceActor : public AActor
{
	GENERATED_BODY()

public:
	ABuildingPieceActor();

	UFUNCTION(BlueprintPure, Category = "Building|Piece")
	UBuildingPieceComponent* GetBuildingPieceComponent() const { return BuildingPiece; }

	/** Hides the piece and removes its collision and navigation influence (true), or restores them (false). */
	UFUNCTION(BlueprintCallable, Category = "Building|Piece")
	void SetProvisional(bool bInProvisional);

	UFUNCTION(BlueprintPure, Category = "Building|Piece")
	bool IsProvisional() const { return bProvisional; }

protected:
	/** Unscaled root at the support surface. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building|Piece")
	TObjectPtr<USceneComponent> PieceRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building|Piece")
	TObjectPtr<UStaticMeshComponent> PieceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building|Piece")
	TObjectPtr<UBuildingPieceComponent> BuildingPiece;

private:
	bool bProvisional = false;
	/** Primitives that affected navigation before the piece became provisional. */
	TArray<TWeakObjectPtr<UPrimitiveComponent>> NavigationRelevantPrimitives;
};
