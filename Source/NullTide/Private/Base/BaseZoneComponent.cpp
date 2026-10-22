#include "Base/BaseZoneComponent.h"

#include "Base/BaseZoneSubsystem.h"
#include "Building/BuildingSubsystem.h"
#include "GameFramework/Actor.h"

FIntRect UBaseZoneComponent::ComputeZoneCells(const FIntPoint& CenterCell, const FIntPoint& Size)
{
	const FIntPoint Clamped(FMath::Clamp(Size.X, MinSizeInCells, MaxSizeInCells), FMath::Clamp(Size.Y, MinSizeInCells, MaxSizeInCells));
	const FIntPoint Min(CenterCell.X - (Clamped.X - 1) / 2, CenterCell.Y - (Clamped.Y - 1) / 2);
	return FIntRect(Min, Min + Clamped);
}

FIntRect UBaseZoneComponent::ComputeBuildableCells(const FIntRect& ZoneCells)
{
	return FIntRect(ZoneCells.Min + FIntPoint(1, 1), ZoneCells.Max - FIntPoint(1, 1));
}

FIntRect UBaseZoneComponent::GetZoneCells() const
{
	return bRegistered ? RegisteredZoneCells : ComputeCurrentZoneCells();
}

FIntRect UBaseZoneComponent::GetBuildableCells() const
{
	return ComputeBuildableCells(GetZoneCells());
}

FIntRect UBaseZoneComponent::ComputeCurrentZoneCells() const
{
	const AActor* Owner = GetOwner();
	const FVector Location = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	return ComputeZoneCells(UBuildingSubsystem::WorldToCell(Location), SizeInCells);
}

void UBaseZoneComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisteredZoneCells = ComputeCurrentZoneCells();
	if (UBaseZoneSubsystem* Zones = UBaseZoneSubsystem::Get(this))
	{
		bRegistered = Zones->RegisterZone(this);
	}
}

void UBaseZoneComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBaseZoneSubsystem* Zones = UBaseZoneSubsystem::Get(this))
	{
		Zones->UnregisterZone(this);
	}
	bRegistered = false;
	Super::EndPlay(EndPlayReason);
}
