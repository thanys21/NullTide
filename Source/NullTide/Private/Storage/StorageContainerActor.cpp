#include "Storage/StorageContainerActor.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Storage/StorageComponent.h"
#include "UI/StorageScreenWidget.h"
#include "UObject/ConstructorHelpers.h"

AStorageContainerActor::AStorageContainerActor()
{
	PrimaryActorTick.bCanEverTick = false;
	ContainerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ContainerMesh"));
	SetRootComponent(ContainerMesh);
	ContainerMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		ContainerMesh->SetStaticMesh(CubeMesh.Object);
		ContainerMesh->SetRelativeScale3D(FVector(1.1f, 0.7f, 0.7f));
	}

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(ContainerMesh);
	InteractionBounds->SetBoxExtent(FVector(130.0f, 130.0f, 110.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractionBounds->SetGenerateOverlapEvents(true);

	StorageComponent = CreateDefaultSubobject<UStorageComponent>(TEXT("StorageComponent"));
	ContainerTitle = FText::FromString(TEXT("Storage"));
}

bool AStorageContainerActor::OpenStorageForInteractor(AActor* Interactor)
{
	if (!IsValid(Interactor) || !IsValid(StorageComponent) || !StorageScreenClass)
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

	UStorageScreenWidget* StorageScreen = ActiveStorageScreen.Get();
	if (!IsValid(StorageScreen))
	{
		StorageScreen = CreateWidget<UStorageScreenWidget>(PlayerController, StorageScreenClass);
		if (!StorageScreen)
		{
			return false;
		}
		ActiveStorageScreen = StorageScreen;
	}

	// CloseStorage removes the widget from the viewport but intentionally retains the
	// reusable UObject. Put that same screen back before rebinding it to this storage.
	if (!StorageScreen->IsInViewport())
	{
		StorageScreen->AddToViewport(100);
	}

	StorageScreen->OpenForStorage(StorageComponent, ContainerTitle);
	PlayerController->SetShowMouseCursor(true);
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(PlayerController, StorageScreen, EMouseLockMode::DoNotLock, false);
	StorageScreen->SetKeyboardFocus();
	return true;
}
