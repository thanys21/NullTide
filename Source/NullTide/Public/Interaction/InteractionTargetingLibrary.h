#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InteractionTargetingLibrary.generated.h"

/**
 * Facing-based interaction targeting (B7.1): which interactable the player means. Read-only; it never interacts.
 */
UCLASS()
class NULLTIDE_API UInteractionTargetingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Default half-angle of the facing cone, in degrees. */
	static constexpr float DefaultFacingConeDegrees = 70.f;

	/**
	 * The candidate the viewer faces: among valid candidates that advertise a prompt (depleted resource nodes do not),
	 * the one with the smallest 2D angle between the viewer's forward vector and the direction to it, within
	 * +/- MaxAngleDegrees. Angles within 1 degree are a tie, won by the nearer one. Null when nothing is in front.
	 */
	UFUNCTION(BlueprintPure, Category = "Interaction", meta = (DefaultToSelf = "Viewer"))
	static AActor* SelectFacingInteractable(const AActor* Viewer, const TArray<AActor*>& Candidates, float MaxAngleDegrees = 70.f);

	/** The interactable the actor's UInteractionProbeComponent currently targets, or null when it has no probe. */
	UFUNCTION(BlueprintPure, Category = "Interaction", meta = (DefaultToSelf = "Owner"))
	static AActor* GetFacingInteractableFor(const AActor* Owner);
};
