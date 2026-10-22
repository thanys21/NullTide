#include "Npc/NpcDefinition.h"

#include "Npc/NpcCharacter.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UNpcDefinition::ValidateNpc(FString& OutDiagnostic) const
{
	OutDiagnostic.Reset();
	if (NpcName.IsEmptyOrWhitespace())
	{
		OutDiagnostic = TEXT("NpcName is empty.");
		return false;
	}
	if (GreetingLines.IsEmpty())
	{
		OutDiagnostic = TEXT("GreetingLines is empty.");
		return false;
	}
	for (int32 Index = 0; Index < GreetingLines.Num(); ++Index)
	{
		if (GreetingLines[Index].IsEmptyOrWhitespace())
		{
			OutDiagnostic = FString::Printf(TEXT("GreetingLines[%d] is empty."), Index);
			return false;
		}
	}
	const UClass* ActorClass = NpcActorClass.Get();
	if (!ActorClass || ActorClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		OutDiagnostic = TEXT("NpcActorClass is null or abstract.");
		return false;
	}
	if (WalkSpeed <= 0.f)
	{
		OutDiagnostic = FString::Printf(TEXT("WalkSpeed %.3f must be greater than zero."), WalkSpeed);
		return false;
	}
	if (IdleTimeMin < 0.f || IdleTimeMax < IdleTimeMin)
	{
		OutDiagnostic = FString::Printf(TEXT("Idle time %.3f-%.3f must be non-negative and ordered."), IdleTimeMin, IdleTimeMax);
		return false;
	}
	return true;
}

const UNpcDefinition* UNpcDefinition::ResolveValidNpc(const TSubclassOf<UNpcDefinition> NpcClass, FString& OutDiagnostic)
{
	const UClass* Class = NpcClass.Get();
	if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		OutDiagnostic = TEXT("NPC definition class is null or abstract.");
		return nullptr;
	}
	const UNpcDefinition* Npc = Class->GetDefaultObject<UNpcDefinition>();
	if (!IsValid(Npc) || !Npc->ValidateNpc(OutDiagnostic))
	{
		return nullptr;
	}
	return Npc;
}

#if WITH_EDITOR
EDataValidationResult UNpcDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	// The abstract native root is never authored; only concrete NPC assets are validated.
	if (GetClass()->HasAnyClassFlags(CLASS_Abstract))
	{
		return Result;
	}
	FString Diagnostic;
	if (!ValidateNpc(Diagnostic))
	{
		Context.AddError(FText::FromString(Diagnostic));
		return EDataValidationResult::Invalid;
	}
	return CombineDataValidationResults(Result, EDataValidationResult::Valid);
}
#endif
