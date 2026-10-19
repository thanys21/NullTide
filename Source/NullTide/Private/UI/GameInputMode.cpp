#include "UI/GameInputMode.h"

#include "GameFramework/PlayerController.h"

namespace NullTideInput
{
void RestoreGameOnlyInput(APlayerController& PlayerController)
{
	FInputModeGameOnly GameOnly;
	GameOnly.SetConsumeCaptureMouseDown(false);
	PlayerController.SetInputMode(GameOnly);
}
}
