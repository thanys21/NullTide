#pragma once

#include "CoreMinimal.h"
#include "Crafting/CraftingStationActor.h"

class APawn;
class APlayerController;
class UCraftingStationComponent;
class UInventoryScreenWidget;

/**
 * Shared interaction logic for every crafting-station actor, authored (ACraftingStationActor) or built
 * (ABuildingCraftingStationActor): the read-only open check and opening the station's crafting screen.
 * Context only: it never crafts and never touches an inventory.
 */
struct NULLTIDE_API FCraftingStationInteraction
{
	/** The interacting pawn and its player controller; either may be null. */
	static void ResolveInteractor(AActor* Interactor, APawn*& OutPawn, APlayerController*& OutController);

	/** Read-only check of an interaction request (the ECraftingStationOpenResult order); changes nothing. */
	static ECraftingStationOpenResult GetOpenResult(AActor* Interactor, const UCraftingStationComponent* Station,
		TSubclassOf<UInventoryScreenWidget> ScreenClass);

	/**
	 * Validates the request, puts the cached screen (or a new one for this player) in station context, then shows it
	 * with the cursor, UI-only input and keyboard focus. A rejected request opens nothing and changes no input state.
	 */
	static bool Open(AActor* Interactor, UCraftingStationComponent* Station, TSubclassOf<UInventoryScreenWidget> ScreenClass,
		TWeakObjectPtr<UInventoryScreenWidget>& CachedScreen, const AActor& StationActor);
};
