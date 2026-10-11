// Fill out your copyright notice in the Description page of Project Settings.


#include "Crafting/CraftingRecipeDefinition.h"

#include "Crafting/CraftingStationDefinition.h"
#include "Items/ItemDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
bool ValidateEntries(const TArray<FCraftingItemQuantity>& Entries, const TCHAR* Source, FString& OutDiagnostic)
{
	if (Entries.IsEmpty())
	{
		OutDiagnostic = FString::Printf(TEXT("%s is empty."), Source);
		return false;
	}
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const UClass* Class = Entries[Index].ItemDefinition.Get();
		if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			OutDiagnostic = FString::Printf(TEXT("%s[%d] has a null or abstract item definition."), Source, Index);
			return false;
		}
		FString ItemDiagnostic;
		if (!Class->GetDefaultObject<UItemDefinition>()->ValidateFragments(ItemDiagnostic))
		{
			OutDiagnostic = FString::Printf(TEXT("%s[%d] (%s) has invalid item data: %s"), Source, Index,
				*Class->GetPathName(), *ItemDiagnostic);
			return false;
		}
		if (Entries[Index].Quantity < 1)
		{
			OutDiagnostic = FString::Printf(TEXT("%s[%d] quantity %d is below 1."), Source, Index, Entries[Index].Quantity);
			return false;
		}
		for (int32 Earlier = 0; Earlier < Index; ++Earlier)
		{
			if (Entries[Earlier].ItemDefinition.Get() == Class)
			{
				OutDiagnostic = FString::Printf(TEXT("%s[%d] and %s[%d] duplicate item definition %s."),
					Source, Earlier, Source, Index, *Class->GetPathName());
				return false;
			}
		}
	}
	return true;
}
}

bool UCraftingRecipeDefinition::ValidateRecipe(FString& OutDiagnostic) const
{
	OutDiagnostic.Reset();
	if (!ValidateEntries(Ingredients, TEXT("Ingredients"), OutDiagnostic)
		|| !ValidateEntries(Outputs, TEXT("Outputs"), OutDiagnostic))
	{
		return false;
	}
	if (CraftDuration < 0.f)
	{
		OutDiagnostic = FString::Printf(TEXT("CraftDuration %.3f is negative."), CraftDuration);
		return false;
	}
	if (RequiredStation)
	{
		FString StationDiagnostic;
		if (!UCraftingStationDefinition::ResolveValidStation(RequiredStation, StationDiagnostic))
		{
			OutDiagnostic = FString::Printf(TEXT("RequiredStation (%s) is invalid: %s"),
				*GetNameSafe(RequiredStation.Get()), *StationDiagnostic);
			return false;
		}
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UCraftingRecipeDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	// The abstract native root is never authored; only concrete recipe assets are validated.
	if (GetClass()->HasAnyClassFlags(CLASS_Abstract))
	{
		return Result;
	}
	FString Diagnostic;
	if (!ValidateRecipe(Diagnostic))
	{
		Context.AddError(FText::FromString(Diagnostic));
		return EDataValidationResult::Invalid;
	}
	return CombineDataValidationResults(Result, EDataValidationResult::Valid);
}
#endif
