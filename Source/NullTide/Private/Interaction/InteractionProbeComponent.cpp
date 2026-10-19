#include "Interaction/InteractionProbeComponent.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractionTargetingLibrary.h"
#include "TimerManager.h"

UInteractionProbeComponent::UInteractionProbeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Ahead of the capsule centre; it turns with the character. The sphere only holds the query's placement and radius:
	// candidates come from a world query, so the component itself has no collision and generates no overlap events.
	InitSphereRadius(90.f);
	SetRelativeLocation(FVector(90.0, 0.0, 0.0));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetHiddenInGame(true);
	SetShouldUpdatePhysicsVolume(false);
}

TArray<AActor*> UInteractionProbeComponent::GetInteractableCandidates() const
{
	TArray<AActor*> Candidates;
	const UWorld* World = GetWorld();
	if (!InteractableInterface || !World || !IsRegistered())
	{
		return Candidates;
	}
	// A fresh query on the Pawn channel (what interaction volumes answer for the capsule today) rather than cached
	// overlap state, so the result is exact at every evaluation. The owner and anything attached to it (an equipped
	// item's actor) are never candidates.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractionProbe), false, GetOwner());
	if (const AActor* Owner = GetOwner())
	{
		TArray<AActor*> Attached;
		Owner->GetAttachedActors(Attached, true, true);
		Params.AddIgnoredActors(Attached);
	}
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, GetComponentLocation(), FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(GetScaledSphereRadius()), Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (IsValid(Actor) && Actor != GetOwner() && Actor->GetClass()->ImplementsInterface(InteractableInterface))
		{
			Candidates.AddUnique(Actor);
		}
	}
	return Candidates;
}

AActor* UInteractionProbeComponent::GetFacingInteractable() const
{
	return UInteractionTargetingLibrary::SelectFacingInteractable(GetOwner(), GetInteractableCandidates(), FacingConeDegrees);
}

void UInteractionProbeComponent::RefreshFacingInteractable()
{
	AActor* Target = GetFacingInteractable();
	// A destroyed previous target reads as null through the weak pointer; that is still a change to report.
	const bool bLastTargetLost = !Target && !LastTarget.IsExplicitlyNull() && !LastTarget.IsValid();
	if (Target != LastTarget.Get() || bLastTargetLost)
	{
		LastTarget = Target;
		OnFacingInteractableChanged.Broadcast(Target);
	}
}

void UInteractionProbeComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimer, this, &UInteractionProbeComponent::RefreshFacingInteractable,
			FMath::Max(RefreshInterval, 0.02f), true);
	}
}

void UInteractionProbeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::EndPlay(EndPlayReason);
}
