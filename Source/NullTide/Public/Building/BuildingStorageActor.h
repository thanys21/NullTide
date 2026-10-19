#pragma once

#include "CoreMinimal.h"
#include "Building/BuildingPieceActor.h"
#include "BuildingStorageActor.generated.h"

class UBoxComponent;
class UStorageComponent;
class UStorageScreenWidget;

/**
 * A buildable storage (the wooden chest): a building piece that hosts one UStorageComponent and opens the storage
 * screen through the shared FStorageInteraction route, exactly like the authored AStorageContainerActor. The
 * Blueprint child implements BPI_Interactable (Interact -> OpenStorageForInteractor). It never opens while the piece is
 * not Active, and demolition refuses it while it holds items.
 */
UCLASS(Abstract, Blueprintable)
class NULLTIDE_API ABuildingStorageActor : public ABuildingPieceActor
{
	GENERATED_BODY()

public:
	ABuildingStorageActor();

	UFUNCTION(BlueprintPure, Category = "Building|Storage")
	UStorageComponent* GetStorageComponent() const { return StorageComponent; }

	/** Read-only: whether the chest can be opened at all (an Active, non-provisional piece that is not being destroyed). */
	UFUNCTION(BlueprintPure, Category = "Building|Storage")
	bool CanOpenStorage() const;

	/** Call from the BPI_Interactable Interact event. Refused (false, nothing opened) unless CanOpenStorage. */
	UFUNCTION(BlueprintCallable, Category = "Building|Storage|Interaction")
	bool OpenStorageForInteractor(AActor* Interactor);

	UFUNCTION(BlueprintPure, Category = "Building|Storage")
	FText GetContainerTitle() const { return ContainerTitle; }

	UBoxComponent* GetInteractionBounds() const { return InteractionBounds; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building|Storage")
	TObjectPtr<UStorageComponent> StorageComponent;

	/** Overlap-only volume that lets the player's interaction component find the chest. Never blocks. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building|Storage")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building|Storage")
	FText ContainerTitle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building|Storage|UI")
	TSubclassOf<UStorageScreenWidget> StorageScreenClass;

private:
	TWeakObjectPtr<UStorageScreenWidget> ActiveStorageScreen;
};
