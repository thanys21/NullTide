#pragma once

#include "Items/Fragments/ItemFragment.h"
#include "ItemFragment_Stackable.generated.h"

/** Static stack configuration. Runtime quantities belong to UItemInstance. */
UCLASS(BlueprintType, Blueprintable, Const, DefaultToInstanced, EditInlineNew)
class NULLTIDE_API UItemFragment_Stackable : public UItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stacking", meta = (ClampMin = "2"))
	int32 MaxStackSize = 50;

	virtual bool ValidateTemplate(FString& OutDiagnostic) const override;
};
