#include "Storage/StorageContainerActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Storage/StorageComponent.h"
#include "Storage/StorageInteraction.h"
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
	return FStorageInteraction::OpenStorageScreen(Interactor, StorageComponent, ContainerTitle, StorageScreenClass, ActiveStorageScreen);
}
