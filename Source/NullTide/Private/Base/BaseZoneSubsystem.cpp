#include "Base/BaseZoneSubsystem.h"

#include "Base/BaseZoneComponent.h"
#include "Building/BuildingDefinition.h"
#include "Building/BuildingSubsystem.h"
#include "Engine/World.h"

UBaseZoneSubsystem* UBaseZoneSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBaseZoneSubsystem>() : nullptr;
}

bool UBaseZoneSubsystem::RegisterZone(UBaseZoneComponent* Zone)
{
	if (!IsValid(Zone))
	{
		return false;
	}
	PurgeStaleZones();
	Zones.AddUnique(Zone);
	return true;
}

void UBaseZoneSubsystem::UnregisterZone(const UBaseZoneComponent* Zone)
{
	Zones.RemoveAll([Zone](const TWeakObjectPtr<UBaseZoneComponent>& Entry) { return !Entry.IsValid() || Entry.Get() == Zone; });
}

TArray<UBaseZoneComponent*> UBaseZoneSubsystem::GetZones() const
{
	TArray<UBaseZoneComponent*> Live;
	for (const TWeakObjectPtr<UBaseZoneComponent>& Entry : Zones)
	{
		if (UBaseZoneComponent* Zone = Entry.Get())
		{
			Live.Add(Zone);
		}
	}
	Live.Sort([](const UBaseZoneComponent& A, const UBaseZoneComponent& B)
	{
		const FIntRect RectA = A.GetZoneCells();
		const FIntRect RectB = B.GetZoneCells();
		if (RectA.Min != RectB.Min)
		{
			return RectA.Min.X != RectB.Min.X ? RectA.Min.X < RectB.Min.X : RectA.Min.Y < RectB.Min.Y;
		}
		if (RectA.Max != RectB.Max)
		{
			return RectA.Max.X != RectB.Max.X ? RectA.Max.X < RectB.Max.X : RectA.Max.Y < RectB.Max.Y;
		}
		return A.GetPathName() < B.GetPathName();
	});
	return Live;
}

bool UBaseZoneSubsystem::IsCellInZone(const FIntPoint& Cell) const
{
	for (const TWeakObjectPtr<UBaseZoneComponent>& Entry : Zones)
	{
		if (const UBaseZoneComponent* Zone = Entry.Get(); Zone && Zone->GetZoneCells().Contains(Cell))
		{
			return true;
		}
	}
	return false;
}

bool UBaseZoneSubsystem::IsCellBuildable(const FIntPoint& Cell) const
{
	for (const TWeakObjectPtr<UBaseZoneComponent>& Entry : Zones)
	{
		if (const UBaseZoneComponent* Zone = Entry.Get(); Zone && Zone->GetBuildableCells().Contains(Cell))
		{
			return true;
		}
	}
	return false;
}

UBaseZoneComponent* UBaseZoneSubsystem::FindZoneAt(const FVector& WorldLocation) const
{
	const FIntPoint Cell = UBuildingSubsystem::WorldToCell(WorldLocation);
	for (UBaseZoneComponent* Zone : GetZones())
	{
		if (Zone->GetZoneCells().Contains(Cell))
		{
			return Zone;
		}
	}
	return nullptr;
}

bool UBaseZoneSubsystem::IsPlacementBuildable(const UBuildingDefinition& Definition, const FBuildingPlacement& Placement) const
{
	if (Placement.PlacementMode == EBuildingPlacementMode::GridEdge)
	{
		// An X edge (x, y) lies between cells (x, y - 1) and (x, y); a Y edge (x, y) between (x - 1, y) and (x, y).
		const FIntPoint Across = Placement.EdgeAxis == EBuildingEdgeAxis::X ? FIntPoint(0, -1) : FIntPoint(-1, 0);
		return IsCellBuildable(Placement.Anchor) || IsCellBuildable(Placement.Anchor + Across);
	}
	const FIntPoint Size = UBuildingSubsystem::GetRotatedFootprint(Definition.Footprint, Placement.Rotation);
	for (int32 DY = 0; DY < Size.Y; ++DY)
	{
		for (int32 DX = 0; DX < Size.X; ++DX)
		{
			if (!IsCellBuildable(Placement.Anchor + FIntPoint(DX, DY)))
			{
				return false;
			}
		}
	}
	return true;
}

int32 UBaseZoneSubsystem::PurgeStaleZones()
{
	return Zones.RemoveAll([](const TWeakObjectPtr<UBaseZoneComponent>& Entry) { return !Entry.IsValid(); });
}

void UBaseZoneSubsystem::Deinitialize()
{
	Zones.Reset();
	Super::Deinitialize();
}
