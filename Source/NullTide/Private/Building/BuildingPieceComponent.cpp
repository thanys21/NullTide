#include "Building/BuildingPieceComponent.h"

#include "Building/BuildingSubsystem.h"
#include "Engine/World.h"

UBuildingPieceComponent::UBuildingPieceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UBuildingPieceComponent::InitializePiece(TSubclassOf<UBuildingDefinition> InDefinition, const FBuildingPlacement& InPlacement,
	const TArray<FBuildingOccupancyKey>& InKeys, const EBuildingPieceState InState, const bool bInPlayerBuilt)
{
	const UWorld* World = GetWorld();
	const UBuildingSubsystem* Subsystem = World ? World->GetSubsystem<UBuildingSubsystem>() : nullptr;
	if (Subsystem && Subsystem->IsPieceRegistered(this))
	{
		return false;
	}
	BuildingDefinition = InDefinition;
	Placement = InPlacement;
	OccupancyKeys = InKeys;
	PieceState = InState;
	bPlayerBuilt = bInPlayerBuilt;
	return true;
}

void UBuildingPieceComponent::BeginPlay()
{
	Super::BeginPlay();
	// Only finalized pieces that already know their footprint register here (authored pieces, later loaded saves).
	// Provisional pieces never register; the construction authority commits their reservation instead.
	if (PieceState == EBuildingPieceState::Active && !OccupancyKeys.IsEmpty())
	{
		if (UBuildingSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UBuildingSubsystem>() : nullptr)
		{
			Subsystem->RegisterActivePiece(this);
		}
	}
}

void UBuildingPieceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromSubsystem();
	Super::EndPlay(EndPlayReason);
}

void UBuildingPieceComponent::OnComponentDestroyed(const bool bDestroyingHierarchy)
{
	UnregisterFromSubsystem();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UBuildingPieceComponent::UnregisterFromSubsystem()
{
	if (UBuildingSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UBuildingSubsystem>() : nullptr)
	{
		Subsystem->UnregisterPiece(this);
	}
}
