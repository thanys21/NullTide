#pragma once

#include "CoreMinimal.h"
#include "Building/BuildingPieceActor.h"
#include "BuildingDoorActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** Outcome of a door request. */
UENUM(BlueprintType)
enum class EBuildingDoorResult : uint8
{
	Success,
	/** No valid pawn was given as the interactor. */
	InvalidInteractor,
	/** The interactor is farther than UseRange from the door. */
	OutOfRange,
	/** The door piece is not an Active building piece (for example still provisional, or being destroyed). */
	NotActive
};

/**
 * A buildable door on a cell edge: two frame posts and a lintel that always block, and a hinged leaf that swings
 * 90 degrees on a short world timer (no tick when idle). The leaf stops blocking as soon as it starts opening and
 * blocks again only when it is fully closed, so a pawn in the doorway is never pushed. Interaction (through the
 * Blueprint child's BPI_Interactable) is refused while the piece is not Active.
 *
 * Local layout (the edge runs along X, 200 cm; thickness along Y; height from the support surface): posts 50 cm wide at
 * both ends, lintel from 220 to 250 cm, leaf 100 cm wide and 220 cm high hinged at the left post. The leaf opens
 * towards +Y local, so an odd placement rotation flips the swing side.
 */
UCLASS(Abstract, Blueprintable)
class NULLTIDE_API ABuildingDoorActor : public ABuildingPieceActor
{
	GENERATED_BODY()

public:
	ABuildingDoorActor();

	/** Opens a closed (or closing) door and closes an open (or opening) one. */
	UFUNCTION(BlueprintCallable, Category = "Building|Door")
	EBuildingDoorResult ToggleDoor(AActor* Interactor);

	UFUNCTION(BlueprintCallable, Category = "Building|Door")
	EBuildingDoorResult SetDoorOpen(bool bOpen, AActor* Interactor);

	/** Whether a request from this interactor would be accepted (read-only). */
	UFUNCTION(BlueprintPure, Category = "Building|Door")
	EBuildingDoorResult CanInteract(const AActor* Interactor) const;

	/** The target state: true once opening was requested, false once closing was requested. */
	UFUNCTION(BlueprintPure, Category = "Building|Door")
	bool IsDoorOpen() const { return bIsOpen; }

	UFUNCTION(BlueprintPure, Category = "Building|Door")
	bool IsDoorMoving() const;

	/** 0 closed, 1 fully open. */
	UFUNCTION(BlueprintPure, Category = "Building|Door")
	float GetDoorOpenAlpha() const { return OpenAlpha; }

	UStaticMeshComponent* GetDoorLeaf() const { return DoorLeaf; }
	USceneComponent* GetDoorHinge() const { return DoorHinge; }
	UBoxComponent* GetInteractionBounds() const { return InteractionBounds; }

protected:
	virtual void PostInitializeComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building|Door")
	TObjectPtr<UStaticMeshComponent> FramePostRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building|Door")
	TObjectPtr<UStaticMeshComponent> FrameLintel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building|Door")
	TObjectPtr<USceneComponent> DoorHinge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building|Door")
	TObjectPtr<UStaticMeshComponent> DoorLeaf;

	/** Overlap-only volume that lets the player's interaction component find the door. Never blocks. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building|Door")
	TObjectPtr<UBoxComponent> InteractionBounds;

	/** Interactors farther than this (2D, from the door's origin) are refused. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building|Door", meta = (ClampMin = "0.0", Units = "cm"))
	float UseRange = 300.f;

	/** Seconds for a full swing; 0 opens or closes instantly. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building|Door", meta = (ClampMin = "0.0", Units = "s"))
	float SwingDuration = 0.3f;

	/** Leaf yaw when fully open, relative to the hinge. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building|Door", meta = (Units = "deg"))
	float OpenYaw = 90.f;

private:
	void StartSwing();
	void StepSwing();
	void ApplyOpenAlpha();
	void SetLeafBlocking(bool bBlocking);

	/** Interval of the swing timer; each step advances the leaf by exactly this much time. */
	static constexpr float SwingStepSeconds = 1.f / 60.f;

	/** The leaf's authored collision, restored whenever the leaf is fully closed. */
	TEnumAsByte<ECollisionEnabled::Type> ClosedLeafCollision = ECollisionEnabled::QueryAndPhysics;
	bool bIsOpen = false;
	float OpenAlpha = 0.f;
	FTimerHandle SwingTimer;
};
