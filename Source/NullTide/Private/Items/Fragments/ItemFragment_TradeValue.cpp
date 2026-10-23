#include "Items/Fragments/ItemFragment_TradeValue.h"

#include "Items/ItemDefinition.h"

bool UItemFragment_TradeValue::ValidateTemplate(FString& OutDiagnostic) const
{
	OutDiagnostic.Reset();
	if (Value < 1)
	{
		OutDiagnostic = TEXT("Value must be at least 1 coin.");
		return false;
	}
	return true;
}

int32 UItemFragment_TradeValue::GetTradeValue(const TSubclassOf<UItemDefinition> DefinitionClass)
{
	const UClass* Class = DefinitionClass.Get();
	if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return 0;
	}
	const UItemFragment_TradeValue* Fragment = Cast<UItemFragment_TradeValue>(
		Class->GetDefaultObject<UItemDefinition>()->FindFragmentByClass(UItemFragment_TradeValue::StaticClass()));
	FString Diagnostic;
	return Fragment && Fragment->ValidateTemplate(Diagnostic) ? Fragment->Value : 0;
}
