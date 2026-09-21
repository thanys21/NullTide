#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ItemWorldPresentationLibrary.generated.h"

class UItemDefinition;
class UStaticMeshComponent;

/** Stateless bridge from definition-owned presentation data to a pickup mesh component. */
UCLASS()
class NULLTIDE_API UItemWorldPresentationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Items|World Presentation")
	static bool ApplyItemWorldPresentation(
		TSubclassOf<UItemDefinition> ItemDefinition,
		UStaticMeshComponent* MeshComponent,
		FString& OutDiagnostic);
};
