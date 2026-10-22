#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CursorFacingComponent.generated.h"

class APlayerController;

/**
 * Turns the player's character toward the mouse cursor (Phase 6 S1). Each frame it projects the cursor onto the
 * horizontal plane at the character's feet and sets the controller's yaw toward that point; the character turns
 * smoothly through its movement component (controller desired rotation). It ticks only while its pawn is controlled
 * by a player controller with a local player (never on NPCs or test pawns), pauses while a UI-only screen is open,
 * and keeps the facing when the cursor is over the character (DeadZonePixels on screen, DeadZone on the ground). It
 * runs before the movement component in the same frame. It changes neither the camera nor the
 * movement directions (both are camera-relative). Facing drives interaction targeting (B7.1) for free.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class NULLTIDE_API UCursorFacingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCursorFacingComponent();

	/**
	 * The yaw (degrees) from PawnFeet toward where the cursor ray meets the horizontal plane through PawnFeet. False when
	 * the ray is parallel to the plane or points away from it, or the point is within DeadZone (2D) of PawnFeet.
	 */
	static bool ComputeFacingYaw(const FVector& PawnFeet, const FVector& RayOrigin, const FVector& RayDirection, float DeadZone,
		float& OutYaw);

	/** True when the cursor is within RadiusPixels of the character's on-screen position (its body; keep the facing). */
	static bool IsWithinScreenDeadZone(const FVector2D& CharacterScreen, const FVector2D& Mouse, float RadiusPixels);

	/** Ground distance (2D, cm) from the character's feet within which the facing is kept. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Facing", meta = (ClampMin = "0.0", Units = "cm"))
	float DeadZone = 40.f;

	/**
	 * Screen distance (pixels) from the character's projected centre within which the facing is kept, so moving the
	 * cursor across the character's body does not flip it around (S1 review).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Facing", meta = (ClampMin = "0.0"))
	float DeadZonePixels = 48.f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Whether game input is active: not while a UI-only screen is open (its input mode makes the viewport ignore input;
	 * game-only and building mode's game-and-UI do not). Seam for tests.
	 */
	virtual bool IsGameInputActive(const APlayerController& PlayerController) const;

	/** Whether this player controller belongs to a local player on this machine. Seam for tests (no local player there). */
	virtual bool IsLocalPlayerController(const APlayerController& PlayerController) const;

	/** The local player's controller driving the owner, or null (no controller, an AI, or no local player). */
	APlayerController* GetLocalPlayerController() const;

private:
	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	/** Ticks only for a locally player-controlled pawn. */
	void RefreshTickEnabled();
};
