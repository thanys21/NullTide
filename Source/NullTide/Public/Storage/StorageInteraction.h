#pragma once

#include "CoreMinimal.h"

class AActor;
class UStorageComponent;
class UStorageScreenWidget;

/**
 * Shared interaction logic for every storage actor, authored (AStorageContainerActor) or built
 * (ABuildingStorageActor): opening the storage screen for the interacting player. Display only: every transfer still
 * runs through UStorageTransferLibrary.
 */
struct NULLTIDE_API FStorageInteraction
{
	/**
	 * Resolves the interactor's player controller, creates or reuses the cached screen, binds it to the storage and
	 * shows it with the cursor, UI-only input and keyboard focus. Returns false and changes nothing when the request
	 * cannot be served.
	 */
	static bool OpenStorageScreen(AActor* Interactor, UStorageComponent* Storage, const FText& Title,
		TSubclassOf<UStorageScreenWidget> ScreenClass, TWeakObjectPtr<UStorageScreenWidget>& CachedScreen);
};
