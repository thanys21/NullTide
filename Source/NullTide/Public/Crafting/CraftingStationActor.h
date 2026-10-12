#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CraftingStationActor.generated.h"

class APawn;
class APlayerController;
class UBoxComponent;
class UCraftingStationComponent;
class UInventoryScreenWidget;
class USceneComponent;
class UStaticMeshComponent;

/** Why a crafting station can or cannot be opened for an interactor. Checked in this order. */
UENUM(BlueprintType)
enum class ECraftingStationOpenResult : uint8
{
	Ready,
	/** Null or destroyed interactor, or a controller without a pawn. */
	InvalidInteractor,
	/** The station component is missing, its owner is being destroyed, or its definition is invalid. */
	StationUnavailable,
	/** The interactor is not, and is not controlled by, a player controller. */
	NoPlayerController,
	/** The interacting pawn is outside the station definition's UseRange. */
	OutOfRange,
	/** No crafting screen class is configured. */
	NoCraftingScreen
};

/**
 * Reusable world actor for a crafting station (Workbench, later Campfire and Forge), exposed through the
 * existing interaction route. It is context only: the station kind comes from its UCraftingStationComponent,
 * every craft still runs on the interacting player's UCraftingComponent, and the actor never touches an
 * inventory or creates crafting state.
 */
UCLASS(Abstract, Blueprintable)
class NULLTIDE_API ACraftingStationActor : public AActor
{
	GENERATED_BODY()

public:
	ACraftingStationActor();

	UFUNCTION(BlueprintPure, Category = "Crafting|Station")
	UCraftingStationComponent* GetCraftingStationComponent() const { return CraftingStation; }

	/** Read-only check of an interaction request; changes nothing. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Station")
	ECraftingStationOpenResult GetOpenCraftingResult(AActor* Interactor) const;

	/**
	 * Call from the BPI_Interactable Interact event. Validates the request and opens the crafting screen for
	 * this station. Any rejected request opens nothing and leaves input mode, cursor and movement unchanged.
	 * The crafting screen is wired in C8C; until then this always returns false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Station|Interaction")
	bool OpenCraftingForInteractor(AActor* Interactor);

protected:
	/** Unscaled root at floor level. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting|Station")
	TObjectPtr<USceneComponent> StationRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting|Station")
	TObjectPtr<UStaticMeshComponent> StationMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting|Station")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting|Station")
	TObjectPtr<UCraftingStationComponent> CraftingStation;

	/** Screen opened for the interacting player (W_Inventory in station context from C8C). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting|Station|UI")
	TSubclassOf<UInventoryScreenWidget> CraftingScreenClass;

private:
	/** The interacting pawn and its player controller; either may be null. */
	static void ResolveInteractor(AActor* Interactor, APawn*& OutPawn, APlayerController*& OutController);
};
