#pragma once

#include "Items/Fragments/ItemFragment.h"
#include "ItemFragment_WorldPresentation.generated.h"

class UStaticMesh;

/** Static world-pickup presentation configuration; never per-instance runtime state. */
UCLASS(BlueprintType, Blueprintable, Const, DefaultToInstanced, EditInlineNew)
class NULLTIDE_API UItemFragment_WorldPresentation : public UItemFragment
{
	GENERATED_BODY()

public:
	/** Hard reference is intentional so every production pickup mesh is gathered by cook. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Presentation")
	TObjectPtr<UStaticMesh> WorldMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Presentation")
	FVector WorldScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Presentation")
	FRotator WorldRotationOffset = FRotator::ZeroRotator;

	virtual bool ValidateTemplate(FString& OutDiagnostic) const override;
};
