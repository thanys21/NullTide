#include "Building/BuildingCraftingStationActor.h"

#include "Building/BuildingPieceComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Crafting/CraftingStationComponent.h"
#include "Crafting/CraftingStationInteraction.h"
#include "Engine/StaticMesh.h"
#include "UI/InventoryScreenWidget.h"
#include "UObject/ConstructorHelpers.h"

ABuildingCraftingStationActor::ABuildingCraftingStationActor()
{
	// The inherited PieceMesh is the authored bench's placeholder: 160 x 80 cm top, 80 cm high, on the piece origin.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PieceMesh->SetStaticMesh(CubeMesh.Object);
	}
	PieceMesh->SetRelativeLocation(FVector(0.0, 0.0, 40.0));
	PieceMesh->SetRelativeScale3D(FVector(1.6, 0.8, 0.8));

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(PieceRoot);
	InteractionBounds->SetRelativeLocation(FVector(0.0, 0.0, 90.0));
	InteractionBounds->SetBoxExtent(FVector(150.0, 120.0, 110.0));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractionBounds->SetGenerateOverlapEvents(true);
	InteractionBounds->SetCanEverAffectNavigation(false);

	CraftingStation = CreateDefaultSubobject<UCraftingStationComponent>(TEXT("CraftingStation"));
}

bool ABuildingCraftingStationActor::IsUsablePiece() const
{
	const UBuildingPieceComponent* Piece = GetBuildingPieceComponent();
	return Piece && Piece->IsActivePiece() && !IsProvisional() && !IsActorBeingDestroyed();
}

ECraftingStationOpenResult ABuildingCraftingStationActor::GetOpenCraftingResult(AActor* Interactor) const
{
	if (IsValid(Interactor) && !IsUsablePiece())
	{
		return ECraftingStationOpenResult::StationUnavailable;
	}
	return FCraftingStationInteraction::GetOpenResult(Interactor, CraftingStation, CraftingScreenClass);
}

bool ABuildingCraftingStationActor::OpenCraftingForInteractor(AActor* Interactor)
{
	if (!IsUsablePiece())
	{
		return false;
	}
	return FCraftingStationInteraction::Open(Interactor, CraftingStation, CraftingScreenClass, ActiveCraftingScreen, *this);
}
