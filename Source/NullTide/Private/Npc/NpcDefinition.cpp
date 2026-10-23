#include "Npc/NpcDefinition.h"

#include "Items/Fragments/ItemFragment_TradeValue.h"
#include "Items/ItemDefinition.h"
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
	return ValidateStore(OutDiagnostic);
}

bool UNpcDefinition::ValidateStore(FString& OutDiagnostic) const
{
	if (!HasStore())
	{
		return true;
	}
	for (int32 Index = 0; Index < StoreItems.Num(); ++Index)
	{
		if (UItemFragment_TradeValue::GetTradeValue(StoreItems[Index]) <= 0)
		{
			OutDiagnostic = FString::Printf(TEXT("StoreItems[%d] is not a tradable item (no trade value)."), Index);
			return false;
		}
		for (int32 Other = 0; Other < Index; ++Other)
		{
			if (StoreItems[Other] == StoreItems[Index])
			{
				OutDiagnostic = FString::Printf(TEXT("StoreItems[%d] repeats StoreItems[%d]."), Index, Other);
				return false;
			}
		}
	}
	if (SellRate <= 0.f || SellRate > 1.f || BuyRate <= 0.f)
	{
		OutDiagnostic = FString::Printf(TEXT("Rates must be 0 < SellRate <= 1 and BuyRate > 0 (got %.3f / %.3f)."), SellRate, BuyRate);
		return false;
	}
	if (MaxLinesPerSide < 1 || HaggleAttemptsPerDay < 0 || HaggleTimeLimit <= 0.f)
	{
		OutDiagnostic = TEXT("MaxLinesPerSide must be at least 1, HaggleAttemptsPerDay non-negative and HaggleTimeLimit positive.");
		return false;
	}
	// A correct answer must store a real discount: a 0% roll would never use an attempt (unlimited re-haggles).
	if (HaggleDiscountMinPercent < (HaggleAttemptsPerDay > 0 ? 1 : 0) || HaggleDiscountMaxPercent > 100
		|| HaggleDiscountMaxPercent < HaggleDiscountMinPercent)
	{
		OutDiagnostic = FString::Printf(TEXT("Haggle discount %d-%d%% must be ordered within 1-100."), HaggleDiscountMinPercent,
			HaggleDiscountMaxPercent);
		return false;
	}
	// Buying at the best discount must still cost more than selling pays, or buying and reselling would make coins.
	if (BuyRate * (100 - HaggleDiscountMaxPercent) / 100.f < SellRate)
	{
		OutDiagnostic = FString::Printf(TEXT("BuyRate %.3f with a %d%% discount falls below SellRate %.3f (a resale profit)."), BuyRate,
			HaggleDiscountMaxPercent, SellRate);
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
