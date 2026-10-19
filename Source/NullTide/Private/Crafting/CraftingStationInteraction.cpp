#include "Crafting/CraftingStationInteraction.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Crafting/CraftingStationComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/InventoryScreenWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogCraftingStation, Log, All);

void FCraftingStationInteraction::ResolveInteractor(AActor* Interactor, APawn*& OutPawn, APlayerController*& OutController)
{
	OutPawn = nullptr;
	OutController = Cast<APlayerController>(Interactor);
	if (OutController)
	{
		OutPawn = OutController->GetPawn();
	}
	else if (APawn* Pawn = Cast<APawn>(Interactor))
	{
		OutPawn = Pawn;
		OutController = Cast<APlayerController>(Pawn->GetController());
	}
}

ECraftingStationOpenResult FCraftingStationInteraction::GetOpenResult(AActor* Interactor, const UCraftingStationComponent* Station,
	TSubclassOf<UInventoryScreenWidget> ScreenClass)
{
	if (!IsValid(Interactor))
	{
		return ECraftingStationOpenResult::InvalidInteractor;
	}
	if (!IsValid(Station) || !Station->IsStationUsable())
	{
		return ECraftingStationOpenResult::StationUnavailable;
	}
	APawn* Pawn = nullptr;
	APlayerController* PlayerController = nullptr;
	ResolveInteractor(Interactor, Pawn, PlayerController);
	if (!PlayerController)
	{
		return ECraftingStationOpenResult::NoPlayerController;
	}
	if (!IsValid(Pawn))
	{
		return ECraftingStationOpenResult::InvalidInteractor;
	}
	if (!Station->IsActorInUseRange(Pawn))
	{
		return ECraftingStationOpenResult::OutOfRange;
	}
	if (!ScreenClass)
	{
		return ECraftingStationOpenResult::NoCraftingScreen;
	}
	return ECraftingStationOpenResult::Ready;
}

bool FCraftingStationInteraction::Open(AActor* Interactor, UCraftingStationComponent* Station, TSubclassOf<UInventoryScreenWidget> ScreenClass,
	TWeakObjectPtr<UInventoryScreenWidget>& CachedScreen, const AActor& StationActor)
{
	const ECraftingStationOpenResult Result = GetOpenResult(Interactor, Station, ScreenClass);
	if (Result != ECraftingStationOpenResult::Ready)
	{
		// Rejected requests change nothing: no widget, no input mode or cursor change.
		UE_LOG(LogCraftingStation, Log, TEXT("%s: crafting station not opened for %s (%s)."), *StationActor.GetName(), *GetNameSafe(Interactor),
			*StaticEnum<ECraftingStationOpenResult>()->GetNameStringByValue(static_cast<int64>(Result)));
		return false;
	}
	APawn* Pawn = nullptr;
	APlayerController* PlayerController = nullptr;
	ResolveInteractor(Interactor, Pawn, PlayerController);

	// One cached screen per station, owned by the interacting player; a different player gets a fresh one.
	UInventoryScreenWidget* Screen = CachedScreen.Get();
	if (!IsValid(Screen) || Screen->GetOwningPlayer() != PlayerController)
	{
		Screen = CreateWidget<UInventoryScreenWidget>(PlayerController, ScreenClass);
		if (!Screen)
		{
			UE_LOG(LogCraftingStation, Warning, TEXT("%s: could not create the crafting screen for %s."), *StationActor.GetName(), *GetNameSafe(Interactor));
			return false;
		}
		CachedScreen = Screen;
	}

	// Initialize the station context first; input mode and cursor change only after it succeeded.
	if (!Screen->OpenForCraftingStation(Station))
	{
		UE_LOG(LogCraftingStation, Warning, TEXT("%s: the crafting screen rejected the station context."), *StationActor.GetName());
		return false;
	}
	if (!Screen->IsInViewport())
	{
		Screen->AddToViewport(100);
	}
	PlayerController->SetShowMouseCursor(true);
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(PlayerController, Screen, EMouseLockMode::DoNotLock, false);
	Screen->SetKeyboardFocus();
	return true;
}
