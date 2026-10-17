#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InputPromptLibrary.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * Key lookups for on-screen prompts. They read a mapping context's authored mappings, not the live applied set, so a
 * prompt still names its key while a higher-priority context (for example building mode) temporarily consumes it.
 */
UCLASS()
class NULLTIDE_API UInputPromptLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * The first key the context's default mappings bind to the action, or an invalid key. Player remaps and mapping
	 * profiles are not applied; with several keys (for example keyboard and gamepad) the first listed one wins.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|Prompt")
	static FKey GetKeyMappedToAction(const UInputMappingContext* Context, const UInputAction* Action);
};
