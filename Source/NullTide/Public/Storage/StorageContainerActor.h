#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StorageContainerActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UStorageComponent;
class UStorageScreenWidget;

/** Reusable world actor that exposes one storage authority through the existing interaction route. */
UCLASS(Abstract, Blueprintable)
class NULLTIDE_API AStorageContainerActor : public AActor
{
	GENERATED_BODY()

public:
	AStorageContainerActor();

	UFUNCTION(BlueprintPure, Category = "Storage")
	UStorageComponent* GetStorageComponent() const { return StorageComponent; }

	/** Call from the existing BPI_Interactable Interact event. */
	UFUNCTION(BlueprintCallable, Category = "Storage|Interaction")
	bool OpenStorageForInteractor(AActor* Interactor);

	UFUNCTION(BlueprintPure, Category = "Storage")
	FText GetContainerTitle() const { return ContainerTitle; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storage")
	TObjectPtr<UStorageComponent> StorageComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storage")
	TObjectPtr<UStaticMeshComponent> ContainerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storage")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Storage")
	FText ContainerTitle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Storage|UI")
	TSubclassOf<UStorageScreenWidget> StorageScreenClass;

private:
	TWeakObjectPtr<UStorageScreenWidget> ActiveStorageScreen;
};
