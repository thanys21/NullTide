#include "Crafting/CraftingStationActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Crafting/CraftingStationComponent.h"
#include "Crafting/CraftingStationInteraction.h"
#include "UObject/ConstructorHelpers.h"

ACraftingStationActor::ACraftingStationActor()
{
	PrimaryActorTick.bCanEverTick = false;
	// An unscaled root at floor level: placing or scaling the actor never distorts the mesh or the bounds.
	StationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StationRoot"));
	SetRootComponent(StationRoot);

	StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationMesh"));
	StationMesh->SetupAttachment(StationRoot);
	StationMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		// Placeholder bench: 160 x 80 cm top, 80 cm high, standing on the actor origin.
		StationMesh->SetStaticMesh(CubeMesh.Object);
		StationMesh->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
		StationMesh->SetRelativeScale3D(FVector(1.6f, 0.8f, 0.8f));
	}

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(StationRoot);
	// Overlap area for the F prompt; well inside the 300 cm UseRange.
	InteractionBounds->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
	InteractionBounds->SetBoxExtent(FVector(150.0f, 120.0f, 110.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractionBounds->SetGenerateOverlapEvents(true);
	InteractionBounds->SetCanEverAffectNavigation(false);

	CraftingStation = CreateDefaultSubobject<UCraftingStationComponent>(TEXT("CraftingStation"));
}

ECraftingStationOpenResult ACraftingStationActor::GetOpenCraftingResult(AActor* Interactor) const
{
	return FCraftingStationInteraction::GetOpenResult(Interactor, CraftingStation, CraftingScreenClass);
}

bool ACraftingStationActor::OpenCraftingForInteractor(AActor* Interactor)
{
	return FCraftingStationInteraction::Open(Interactor, CraftingStation, CraftingScreenClass, ActiveCraftingScreen, *this);
}
