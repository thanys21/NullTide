#pragma once

#include "Player/CursorFacingComponent.h"
#include "PlayerTestTypes.generated.h"

/** Cursor facing with the local-player and input-mode gates forced (G10): a test world has no local player or viewport. */
UCLASS(Transient, NotBlueprintable)
class UCursorFacingTestComponent : public UCursorFacingComponent
{
	GENERATED_BODY()

public:
	bool bTreatAsLocal = true;

protected:
	virtual bool IsLocalPlayerController(const APlayerController& PlayerController) const override { return bTreatAsLocal; }
	virtual bool IsGameInputActive(const APlayerController& PlayerController) const override { return true; }
};
