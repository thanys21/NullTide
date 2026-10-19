#pragma once

#include "CoreMinimal.h"
#include "Building/BuildingPieceActor.h"
#include "Crafting/CraftingStationActor.h"
#include "BuildingCraftingStationActor.generated.h"

class UBoxComponent;
class UCraftingStationComponent;
class UInventoryScreenWidget;

/**
 * A buildable crafting station (the workbench): a building piece that hosts one UCraftingStationComponent and opens the
 * station crafting screen through the shared FCraftingStationInteraction route, exactly like the authored
 * ACraftingStationActor. Context only: it never crafts or touches an inventory. Not usable while the piece is not
 * Active; demolishing it cancels a craft at it through the existing StationLost path.
 */
UCLASS(Abstract, Blueprintable)
class NULLTIDE_API ABuildingCraftingStationActor : public ABuildingPieceActor
{
	GENERATED_BODY()

public:
	ABuildingCraftingStationActor();

	UFUNCTION(BlueprintPure, Category = "Building|Station")
	UCraftingStationComponent* GetCraftingStationComponent() const { return CraftingStation; }

	/** Read-only check of an interaction request; StationUnavailable while the piece is not Active. */
	UFUNCTION(BlueprintPure, Category = "Building|Station")
	ECraftingStationOpenResult GetOpenCraftingResult(AActor* Interactor) const;

	/** Call from the BPI_Interactable Interact event (see ACraftingStationActor::OpenCraftingForInteractor). */
	UFUNCTION(BlueprintCallable, Category = "Building|Station|Interaction")
	bool OpenCraftingForInteractor(AActor* Interactor);

	UBoxComponent* GetInteractionBounds() const { return InteractionBounds; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building|Station")
	TObjectPtr<UCraftingStationComponent> CraftingStation;

	/** Overlap-only volume for the F prompt. Never blocks. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building|Station")
	TObjectPtr<UBoxComponent> InteractionBounds;

	/** Screen opened for the interacting player (W_Inventory in station context). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building|Station|UI")
	TSubclassOf<UInventoryScreenWidget> CraftingScreenClass;

private:
	bool IsUsablePiece() const;

	TWeakObjectPtr<UInventoryScreenWidget> ActiveCraftingScreen;
};
