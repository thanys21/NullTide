#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CraftingStationDefinition.generated.h"

class UTexture2D;

/**
 * Static data for one kind of crafting station (Workbench, Campfire, Forge). Each station kind is a
 * data-only Blueprint subclass, referenced by class and read from its default object. Recipes name
 * the station class they require; stations hold no recipes and no runtime state.
 */
UCLASS(Blueprintable, BlueprintType, Abstract, Const)
class NULLTIDE_API UCraftingStationDefinition : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText StationName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText StationDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TObjectPtr<UTexture2D> StationIcon;

	/** Horizontal distance from the station actor within which a crafter can use it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Station", meta = (ClampMin = "1.0", Units = "cm"))
	float UseRange = 300.f;

	UFUNCTION(BlueprintPure, Category = "Crafting")
	bool ValidateStation(FString& OutDiagnostic) const;

	/** Default object of a concrete, valid station class; null (with a diagnostic) otherwise. */
	static const UCraftingStationDefinition* ResolveValidStation(TSubclassOf<UCraftingStationDefinition> StationClass,
		FString& OutDiagnostic);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
