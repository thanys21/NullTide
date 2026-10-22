#include "Npc/NpcSubsystem.h"

#include "Base/BaseZoneComponent.h"
#include "Base/BaseZoneSubsystem.h"
#include "Building/BuildingSubsystem.h"
#include "Engine/World.h"
#include "Npc/NpcCharacter.h"
#include "Npc/NpcDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogNpcResidents, Log, All);

UNpcSubsystem* UNpcSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UNpcSubsystem>() : nullptr;
}

void UNpcSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UBaseZoneSubsystem* Zones = Collection.InitializeDependency<UBaseZoneSubsystem>())
	{
		RegisteredHandle = Zones->OnZoneRegistered.AddUObject(this, &UNpcSubsystem::HandleZoneRegistered);
		UnregisteredHandle = Zones->OnZoneUnregistered.AddUObject(this, &UNpcSubsystem::HandleZoneUnregistered);
	}
}

void UNpcSubsystem::Deinitialize()
{
	if (UBaseZoneSubsystem* Zones = GetWorld() ? GetWorld()->GetSubsystem<UBaseZoneSubsystem>() : nullptr)
	{
		Zones->OnZoneRegistered.Remove(RegisteredHandle);
		Zones->OnZoneUnregistered.Remove(UnregisteredHandle);
	}
	Residents.Reset();
	Super::Deinitialize();
}

TArray<ANpcCharacter*> UNpcSubsystem::GetResidents(const UBaseZoneComponent* Zone) const
{
	TArray<const FResident*> Matching;
	for (const FResident& Resident : Residents)
	{
		if (Zone && Resident.Zone.Get() == Zone && Resident.Npc.IsValid())
		{
			Matching.Add(&Resident);
		}
	}
	Matching.Sort([](const FResident& A, const FResident& B) { return A.ResidentIndex < B.ResidentIndex; });
	TArray<ANpcCharacter*> Live;
	for (const FResident* Resident : Matching)
	{
		Live.Add(Resident->Npc.Get());
	}
	return Live;
}

void UNpcSubsystem::HandleZoneRegistered(const UBaseZoneComponent* Zone)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client || !Zone || !Zone->GetOwner())
	{
		return;
	}
	Residents.RemoveAll([](const FResident& Resident) { return !Resident.Npc.IsValid(); });
	const FIntRect Interior = Zone->GetBuildableCells();
	for (int32 Index = 0; Index < Zone->Residents.Num(); ++Index)
	{
		const bool bAlreadyLiving = Residents.ContainsByPredicate([Zone, Index](const FResident& Resident)
		{
			return Resident.Zone.Get() == Zone && Resident.ResidentIndex == Index;
		});
		FString Diagnostic;
		const UNpcDefinition* Definition = UNpcDefinition::ResolveValidNpc(Zone->Residents[Index], Diagnostic);
		if (bAlreadyLiving || !Definition)
		{
			if (!Definition)
			{
				UE_LOG(LogNpcResidents, Warning, TEXT("%s: resident %d skipped (%s)."), *Zone->GetPathName(), Index, *Diagnostic);
			}
			continue;
		}
		const FIntPoint Cell(FMath::RandRange(Interior.Min.X, Interior.Max.X - 1), FMath::RandRange(Interior.Min.Y, Interior.Max.Y - 1));
		const FTransform SpawnTransform(UBuildingSubsystem::CellToWorldCenter(Cell, Zone->GetOwner()->GetActorLocation().Z + SpawnLift));
		ANpcCharacter* Npc = World->SpawnActorDeferred<ANpcCharacter>(Definition->NpcActorClass, SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!Npc)
		{
			UE_LOG(LogNpcResidents, Warning, TEXT("%s: resident %d could not be spawned."), *Zone->GetPathName(), Index);
			continue;
		}
		Npc->InitializeResident(Zone->Residents[Index], Zone);
		Npc->FinishSpawning(SpawnTransform);
		FResident& Resident = Residents.AddDefaulted_GetRef();
		Resident.Zone = Zone;
		Resident.ResidentIndex = Index;
		Resident.Npc = Npc;
	}
}

void UNpcSubsystem::HandleZoneUnregistered(const UBaseZoneComponent* Zone)
{
	// Only a destroyed zone takes its residents with it. Level teardown (end of play, travel) ends every actor itself,
	// and destroying actors from inside that pass would race it, so the residents are only forgotten then.
	const UWorld* World = GetWorld();
	const AActor* ZoneOwner = Zone ? Zone->GetOwner() : nullptr;
	const bool bZoneDestroyed = World && !World->bIsTearingDown && ZoneOwner && ZoneOwner->IsActorBeingDestroyed();
	for (int32 Index = Residents.Num() - 1; Index >= 0; --Index)
	{
		if (Residents[Index].Zone.Get() != Zone && Residents[Index].Zone.IsValid())
		{
			continue;
		}
		ANpcCharacter* Npc = Residents[Index].Npc.Get();
		Residents.RemoveAt(Index);
		if (Npc && bZoneDestroyed)
		{
			Npc->Destroy();
		}
	}
}
