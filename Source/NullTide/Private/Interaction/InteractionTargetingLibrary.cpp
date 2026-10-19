#include "Interaction/InteractionTargetingLibrary.h"

#include "GameFramework/Actor.h"
#include "Interaction/InteractionProbeComponent.h"
#include "ResourceGathering/ResourceNodeActor.h"

AActor* UInteractionTargetingLibrary::SelectFacingInteractable(const AActor* Viewer, const TArray<AActor*>& Candidates, const float MaxAngleDegrees)
{
	if (!IsValid(Viewer))
	{
		return nullptr;
	}
	const FVector2D Forward = FVector2D(Viewer->GetActorForwardVector()).GetSafeNormal();
	const FVector2D Origin(Viewer->GetActorLocation());
	const double MaxAngle = FMath::Clamp(static_cast<double>(MaxAngleDegrees), 0.0, 180.0);
	constexpr double TieDegrees = 1.0;

	struct FScored
	{
		AActor* Actor;
		double Angle;
		double DistanceSquared;
	};
	TArray<FScored, TInlineAllocator<8>> InCone;
	double MinAngle = TNumericLimits<double>::Max();
	for (AActor* Candidate : Candidates)
	{
		if (!IsValid(Candidate) || Candidate == Viewer || Candidate->IsActorBeingDestroyed()
			|| !AResourceNodeActor::ShouldShowInteractionPromptFor(Candidate))
		{
			continue;
		}
		const FVector2D ToCandidate = FVector2D(Candidate->GetActorLocation()) - Origin;
		const double DistanceSquared = ToCandidate.SizeSquared();
		// A candidate right at the viewer's position counts as straight ahead.
		const double Angle = DistanceSquared < KINDA_SMALL_NUMBER ? 0.0
			: FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector2D::DotProduct(Forward, ToCandidate.GetSafeNormal()), -1.0, 1.0)));
		if (Angle <= MaxAngle)
		{
			InCone.Add({ Candidate, Angle, DistanceSquared });
			MinAngle = FMath::Min(MinAngle, Angle);
		}
	}

	// Two passes so the result does not depend on candidate order: the nearest among those within TieDegrees of the
	// smallest angle.
	const FScored* Best = nullptr;
	for (const FScored& Scored : InCone)
	{
		if (Scored.Angle <= MinAngle + TieDegrees && (!Best || Scored.DistanceSquared < Best->DistanceSquared))
		{
			Best = &Scored;
		}
	}
	return Best ? Best->Actor : nullptr;
}

AActor* UInteractionTargetingLibrary::GetFacingInteractableFor(const AActor* Owner)
{
	const UInteractionProbeComponent* Probe = IsValid(Owner) ? Owner->FindComponentByClass<UInteractionProbeComponent>() : nullptr;
	return Probe ? Probe->GetFacingInteractable() : nullptr;
}
