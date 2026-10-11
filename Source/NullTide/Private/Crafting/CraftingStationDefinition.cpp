#include "Crafting/CraftingStationDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UCraftingStationDefinition::ValidateStation(FString& OutDiagnostic) const
{
	OutDiagnostic.Reset();
	if (UseRange <= 0.f)
	{
		OutDiagnostic = FString::Printf(TEXT("UseRange %.3f must be greater than zero."), UseRange);
		return false;
	}
	return true;
}

const UCraftingStationDefinition* UCraftingStationDefinition::ResolveValidStation(
	const TSubclassOf<UCraftingStationDefinition> StationClass, FString& OutDiagnostic)
{
	const UClass* Class = StationClass.Get();
	if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		OutDiagnostic = TEXT("Station definition class is null or abstract.");
		return nullptr;
	}
	const UCraftingStationDefinition* Station = Class->GetDefaultObject<UCraftingStationDefinition>();
	if (!IsValid(Station) || !Station->ValidateStation(OutDiagnostic))
	{
		return nullptr;
	}
	return Station;
}

#if WITH_EDITOR
EDataValidationResult UCraftingStationDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	// The abstract native root is never authored; only concrete station assets are validated.
	if (GetClass()->HasAnyClassFlags(CLASS_Abstract))
	{
		return Result;
	}
	FString Diagnostic;
	if (!ValidateStation(Diagnostic))
	{
		Context.AddError(FText::FromString(Diagnostic));
		return EDataValidationResult::Invalid;
	}
	return CombineDataValidationResults(Result, EDataValidationResult::Valid);
}
#endif
