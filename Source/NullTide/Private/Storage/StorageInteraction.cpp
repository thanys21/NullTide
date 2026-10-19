#include "Storage/StorageInteraction.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Storage/StorageComponent.h"
#include "UI/StorageScreenWidget.h"

bool FStorageInteraction::OpenStorageScreen(AActor* Interactor, UStorageComponent* Storage, const FText& Title,
	TSubclassOf<UStorageScreenWidget> ScreenClass, TWeakObjectPtr<UStorageScreenWidget>& CachedScreen)
{
	if (!IsValid(Interactor) || !IsValid(Storage) || !ScreenClass)
	{
		return false;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Interactor);
	if (!PlayerController)
	{
		if (const APawn* Pawn = Cast<APawn>(Interactor))
		{
			PlayerController = Cast<APlayerController>(Pawn->GetController());
		}
	}
	if (!PlayerController)
	{
		return false;
	}

	UStorageScreenWidget* StorageScreen = CachedScreen.Get();
	if (!IsValid(StorageScreen))
	{
		StorageScreen = CreateWidget<UStorageScreenWidget>(PlayerController, ScreenClass);
		if (!StorageScreen)
		{
			return false;
		}
		CachedScreen = StorageScreen;
	}

	// CloseStorage removes the widget from the viewport but intentionally retains the
	// reusable UObject. Put that same screen back before rebinding it to this storage.
	if (!StorageScreen->IsInViewport())
	{
		StorageScreen->AddToViewport(100);
	}

	StorageScreen->OpenForStorage(Storage, Title);
	PlayerController->SetShowMouseCursor(true);
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(PlayerController, StorageScreen, EMouseLockMode::DoNotLock, false);
	StorageScreen->SetKeyboardFocus();
	return true;
}
