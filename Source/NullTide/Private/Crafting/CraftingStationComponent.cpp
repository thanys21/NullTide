#include "Crafting/CraftingStationComponent.h"

#include "Crafting/CraftingStationDefinition.h"
#include "GameFramework/Actor.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UCraftingStationComponent::UCraftingStationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

const UCraftingStationDefinition* UCraftingStationComponent::GetValidStationDefinition() const
{
	FString Diagnostic;
	return UCraftingStationDefinition::ResolveValidStation(StationDefinition, Diagnostic);
}

bool UCraftingStationComponent::IsStationUsable() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) && !Owner->IsActorBeingDestroyed() && GetValidStationDefinition() != nullptr;
}

bool UCraftingStationComponent::IsActorInUseRange(const AActor* Actor) const
{
	const AActor* Owner = GetOwner();
	const UCraftingStationDefinition* Definition = GetValidStationDefinition();
	if (!IsValid(Actor) || !IsValid(Owner) || !Definition)
	{
		return false;
	}
	return FVector::DistSquared2D(Actor->GetActorLocation(), Owner->GetActorLocation()) <= FMath::Square(Definition->UseRange);
}

#if WITH_EDITOR
EDataValidationResult UCraftingStationComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	FString Diagnostic;
	if (!UCraftingStationDefinition::ResolveValidStation(StationDefinition, Diagnostic))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("StationDefinition: %s"), *Diagnostic)));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
