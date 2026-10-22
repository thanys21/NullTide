#pragma once

#include "Base/BaseZoneActor.h"
#include "Base/BaseZoneComponent.h"
#include "Engine/World.h"

namespace NullTideBaseTests
{
/** Cells -50..50 around the origin (buildable -49..49): every building fixture's area, since every building needs a zone (S2). */
inline const FIntPoint FixtureZoneSize(101, 101);

/**
 * Spawns a base zone of Size cells centred on Location's cell (sized before construction, like a placed actor) and
 * begins its play, so it registers like in a level.
 */
inline ABaseZoneActor* SpawnBaseZone(UWorld* World, const FVector& Location, const FIntPoint& Size)
{
	ABaseZoneActor* Zone = World ? World->SpawnActorDeferred<ABaseZoneActor>(ABaseZoneActor::StaticClass(), FTransform(Location),
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn) : nullptr;
	if (Zone)
	{
		Zone->GetZone()->SizeInCells = Size;
		Zone->FinishSpawning(FTransform(Location));
		Zone->DispatchBeginPlay();
	}
	return Zone;
}
}
