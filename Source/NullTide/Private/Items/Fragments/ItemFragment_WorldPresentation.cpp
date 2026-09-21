#include "Items/Fragments/ItemFragment_WorldPresentation.h"

#include "Engine/StaticMesh.h"

bool UItemFragment_WorldPresentation::ValidateTemplate(FString& OutDiagnostic) const
{
	OutDiagnostic.Reset();
	if (!IsValid(WorldMesh))
	{
		OutDiagnostic = TEXT("WorldMesh must reference a valid static mesh.");
		return false;
	}
	if (!FMath::IsFinite(WorldScale.X) || !FMath::IsFinite(WorldScale.Y) || !FMath::IsFinite(WorldScale.Z)
		|| WorldScale.X <= 0.0 || WorldScale.Y <= 0.0 || WorldScale.Z <= 0.0)
	{
		OutDiagnostic = TEXT("WorldScale components must be finite and greater than zero.");
		return false;
	}
	if (!FMath::IsFinite(WorldRotationOffset.Pitch) || !FMath::IsFinite(WorldRotationOffset.Yaw)
		|| !FMath::IsFinite(WorldRotationOffset.Roll))
	{
		OutDiagnostic = TEXT("WorldRotationOffset components must be finite.");
		return false;
	}
	return true;
}
