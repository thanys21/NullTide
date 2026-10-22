#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NpcSubsystem.generated.h"

class ANpcCharacter;
class UBaseZoneComponent;

/**
 * The only authority over outdoor NPC residents (Phase 6 S4). When a base zone registers, it spawns each valid
 * definition in the zone's Residents once, on a random interior cell (the zone without its ring), with world
 * authority only. When a destroyed zone unregisters, its residents are destroyed with it; any other unregistration
 * (level teardown) only forgets them. It never touches inventories, building or zones.
 */
UCLASS()
class NULLTIDE_API UNpcSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UNpcSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Live residents spawned for the zone, in Residents order. */
	TArray<ANpcCharacter*> GetResidents(const UBaseZoneComponent* Zone) const;

	/** Height above the zone actor at which residents spawn (the capsule settles onto the ground). */
	static constexpr double SpawnLift = 100.0;

private:
	void HandleZoneRegistered(const UBaseZoneComponent* Zone);
	void HandleZoneUnregistered(const UBaseZoneComponent* Zone);

	struct FResident
	{
		TWeakObjectPtr<const UBaseZoneComponent> Zone;
		int32 ResidentIndex = INDEX_NONE;
		TWeakObjectPtr<ANpcCharacter> Npc;
	};
	TArray<FResident> Residents;

	FDelegateHandle RegisteredHandle;
	FDelegateHandle UnregisteredHandle;
};
