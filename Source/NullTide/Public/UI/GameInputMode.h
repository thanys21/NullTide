#pragma once

#include "CoreMinimal.h"

class APlayerController;

namespace NullTideInput
{
/**
 * Returns a player to the resting game input after a screen or building mode (B9): GameOnly with the project's
 * capture mode (CapturePermanently_IncludingInitialMouseDown), so the first click afterwards reaches the game.
 * SetInputMode_GameOnly's default (bConsumeCaptureMouseDown) would swallow that click. Cursor visibility is the
 * caller's.
 */
NULLTIDE_API void RestoreGameOnlyInput(APlayerController& PlayerController);
}
