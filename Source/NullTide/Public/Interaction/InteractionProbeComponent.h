#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "InteractionProbeComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFacingInteractableChanged, AActor*, NewTarget);

/**
 * The character's forward interaction area (B7.1): a sphere ahead of the capsule that turns with the character.
 * Interactables are the actors implementing InteractableInterface (BPI_Interactable) whose collision answers a Pawn-channel
 * overlap query of this sphere (queried fresh at every evaluation; the owner and actors attached to it are ignored); the
 * target is the one the owner faces (UInteractionTargetingLibrary::SelectFacingInteractable). A short looping timer (no
 * tick) re-evaluates it and broadcasts OnFacingInteractableChanged only when it changes (including to null when the
 * previous target was destroyed), so turning in place switches the prompt. The component itself has no collision and
 * generates no overlap events: it only places and sizes the query, so it never blocks, never triggers other volumes and
 * never affects navigation. Read-only: it never interacts.
 */
UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class NULLTIDE_API UInteractionProbeComponent : public USphereComponent
{
	GENERATED_BODY()

public:
	UInteractionProbeComponent();

	/** The interactable the owner currently faces, re-evaluated now (null when none is in front). */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetFacingInteractable() const;

	/** The actors implementing InteractableInterface found by a fresh overlap query of this sphere. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	TArray<AActor*> GetInteractableCandidates() const;

	/** Re-evaluates the target and broadcasts when it changed. Called by the refresh timer; callable directly. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RefreshFacingInteractable();

	/** Last target broadcast by RefreshFacingInteractable. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetLastFacingInteractable() const { return LastTarget.Get(); }

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnFacingInteractableChanged OnFacingInteractableChanged;

	/** The interface an actor must implement to be a candidate (BPI_Interactable). Null accepts no actor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	TSubclassOf<UInterface> InteractableInterface;

	/** Half-angle of the facing cone, in degrees. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float FacingConeDegrees = 70.f;

	/** Seconds between target re-evaluations. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0.02", Units = "s"))
	float RefreshInterval = 0.1f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FTimerHandle RefreshTimer;
	TWeakObjectPtr<AActor> LastTarget;
};
