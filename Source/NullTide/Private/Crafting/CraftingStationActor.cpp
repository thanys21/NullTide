#include "Crafting/CraftingStationActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Crafting/CraftingStationComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/InventoryScreenWidget.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCraftingStation, Log, All);

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

void ACraftingStationActor::ResolveInteractor(AActor* Interactor, APawn*& OutPawn, APlayerController*& OutController)
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

ECraftingStationOpenResult ACraftingStationActor::GetOpenCraftingResult(AActor* Interactor) const
{
	if (!IsValid(Interactor))
	{
		return ECraftingStationOpenResult::InvalidInteractor;
	}
	if (!IsValid(CraftingStation) || !CraftingStation->IsStationUsable())
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
	if (!CraftingStation->IsActorInUseRange(Pawn))
	{
		return ECraftingStationOpenResult::OutOfRange;
	}
	if (!CraftingScreenClass)
	{
		return ECraftingStationOpenResult::NoCraftingScreen;
	}
	return ECraftingStationOpenResult::Ready;
}

bool ACraftingStationActor::OpenCraftingForInteractor(AActor* Interactor)
{
	const ECraftingStationOpenResult Result = GetOpenCraftingResult(Interactor);
	if (Result != ECraftingStationOpenResult::Ready)
	{
		// Rejected requests change nothing: no widget, no input mode or cursor change.
		UE_LOG(LogCraftingStation, Log, TEXT("%s: crafting station not opened for %s (%s)."), *GetName(), *GetNameSafe(Interactor),
			*StaticEnum<ECraftingStationOpenResult>()->GetNameStringByValue(static_cast<int64>(Result)));
		return false;
	}
	// The crafting screen in station context is wired in C8C; until then a valid request opens nothing either.
	UE_LOG(LogCraftingStation, Log, TEXT("%s: crafting station request from %s is valid; the station screen arrives in C8C."),
		*GetName(), *GetNameSafe(Interactor));
	return false;
}
