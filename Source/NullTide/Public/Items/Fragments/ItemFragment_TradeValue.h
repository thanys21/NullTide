#pragma once

#include "Items/Fragments/ItemFragment.h"
#include "ItemFragment_TradeValue.generated.h"

/**
 * Static trade value in coins (Phase 6 S5). An item with this fragment can be sold to and bought from NPC stores;
 * an item without it cannot be traded. Traders apply their own sell and buy rates to this value.
 */
UCLASS(BlueprintType, Blueprintable, Const, DefaultToInstanced, EditInlineNew)
class NULLTIDE_API UItemFragment_TradeValue : public UItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trade", meta = (ClampMin = "1"))
	int32 Value = 1;

	virtual bool ValidateTemplate(FString& OutDiagnostic) const override;

	/** The item's trade value, or 0 when it has no valid trade value (not tradable). */
	static int32 GetTradeValue(TSubclassOf<class UItemDefinition> DefinitionClass);
};
