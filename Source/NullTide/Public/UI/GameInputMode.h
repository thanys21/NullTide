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

/**
 * Restores the gameplay cursor after a screen closes: the controller class's default visibility (BP_TopDownController
 * shows it), so cursor facing (Phase 6 S1) keeps working. Screens used to hide it.
 */
NULLTIDE_API void RestoreGameplayCursor(APlayerController& PlayerController);
}
