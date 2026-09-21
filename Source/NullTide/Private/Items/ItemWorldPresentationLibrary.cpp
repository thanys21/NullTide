#include "Items/ItemWorldPresentationLibrary.h"

#include "Components/StaticMeshComponent.h"
#include "Items/Fragments/ItemFragment_WorldPresentation.h"
#include "Items/ItemDefinition.h"

namespace
{
bool PresentationFailure(FString& OutDiagnostic, const FString& Diagnostic)
{
	OutDiagnostic = Diagnostic;
	UE_LOG(LogTemp, Warning, TEXT("World pickup presentation was not applied: %s"), *Diagnostic);
	return false;
}
}

bool UItemWorldPresentationLibrary::ApplyItemWorldPresentation(
	TSubclassOf<UItemDefinition> ItemDefinition,
	UStaticMeshComponent* MeshComponent,
	FString& OutDiagnostic)
{
	OutDiagnostic.Reset();
	if (!IsValid(MeshComponent))
	{
		return PresentationFailure(OutDiagnostic, TEXT("MeshComponent is null or invalid."));
	}
	const UClass* DefinitionClass = ItemDefinition.Get();
	if (!IsValid(DefinitionClass) || DefinitionClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return PresentationFailure(OutDiagnostic, TEXT("ItemDefinition is null, abstract or invalid."));
	}
	const UItemDefinition* Definition = Cast<UItemDefinition>(DefinitionClass->GetDefaultObject());
	if (!IsValid(Definition))
	{
		return PresentationFailure(OutDiagnostic, TEXT("ItemDefinition has no valid class default object."));
	}
	FString ValidationDiagnostic;
	if (!Definition->ValidateFragments(ValidationDiagnostic))
	{
		return PresentationFailure(OutDiagnostic, TEXT("ItemDefinition fragments are invalid: ") + ValidationDiagnostic);
	}
	const UItemFragment_WorldPresentation* Presentation = Cast<UItemFragment_WorldPresentation>(
		Definition->FindFragmentByClass(UItemFragment_WorldPresentation::StaticClass()));
	if (!Presentation)
	{
		return PresentationFailure(OutDiagnostic, TEXT("ItemDefinition has no unambiguous WorldPresentation fragment."));
	}
	if (!Presentation->ValidateTemplate(ValidationDiagnostic))
	{
		return PresentationFailure(OutDiagnostic, TEXT("WorldPresentation is invalid: ") + ValidationDiagnostic);
	}

	MeshComponent->SetStaticMesh(Presentation->WorldMesh);
	MeshComponent->SetRelativeScale3D(Presentation->WorldScale);
	MeshComponent->SetRelativeRotation(Presentation->WorldRotationOffset);
	return true;
}
