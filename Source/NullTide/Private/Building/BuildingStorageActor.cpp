#include "Building/BuildingStorageActor.h"

#include "Building/BuildingPieceComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Storage/StorageComponent.h"
#include "Storage/StorageInteraction.h"
#include "UI/StorageScreenWidget.h"
#include "UObject/ConstructorHelpers.h"

ABuildingStorageActor::ABuildingStorageActor()
{
	// The inherited PieceMesh is a placeholder chest: 110 x 70 x 70 cm standing on the piece origin.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PieceMesh->SetStaticMesh(CubeMesh.Object);
	}
	PieceMesh->SetRelativeLocation(FVector(0.0, 0.0, 35.0));
	PieceMesh->SetRelativeScale3D(FVector(1.1, 0.7, 0.7));

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(PieceRoot);
	InteractionBounds->SetRelativeLocation(FVector(0.0, 0.0, 90.0));
	InteractionBounds->SetBoxExtent(FVector(130.0, 130.0, 110.0));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractionBounds->SetGenerateOverlapEvents(true);
	InteractionBounds->SetCanEverAffectNavigation(false);

	StorageComponent = CreateDefaultSubobject<UStorageComponent>(TEXT("StorageComponent"));
	ContainerTitle = FText::FromString(TEXT("Storage"));
}

bool ABuildingStorageActor::CanOpenStorage() const
{
	const UBuildingPieceComponent* Piece = GetBuildingPieceComponent();
	return Piece && Piece->IsActivePiece() && !IsProvisional() && !IsActorBeingDestroyed() && IsValid(StorageComponent);
}

bool ABuildingStorageActor::OpenStorageForInteractor(AActor* Interactor)
{
	if (!CanOpenStorage())
	{
		return false;
	}
	return FStorageInteraction::OpenStorageScreen(Interactor, StorageComponent, ContainerTitle, StorageScreenClass, ActiveStorageScreen);
}
