#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/Object.h"
#include "InteractionTestTypes.generated.h"

/** Records every OnFacingInteractableChanged broadcast (G8). */
UCLASS(Transient, NotBlueprintable)
class UInteractionTestListener : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleFacingInteractableChanged(AActor* NewTarget) { Targets.Add(NewTarget); }

	/** The broadcast payloads in order (weak: a payload may be destroyed later). */
	TArray<TWeakObjectPtr<AActor>> Targets;
};
