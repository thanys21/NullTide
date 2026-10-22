#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NpcDefinition.generated.h"

class ANpcCharacter;

/**
 * Static data for one kind of NPC (Trader). Each NPC kind is a data-only Blueprint subclass, referenced by class and
 * read from its default object, the same pattern as items, recipes, stations and buildings. Runtime state lives on
 * the spawned ANpcCharacter.
 */
UCLASS(Blueprintable, BlueprintType, Abstract, Const)
class NULLTIDE_API UNpcDefinition : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText NpcName;

	/** One is picked at random each time the player starts talking. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TArray<FText> GreetingLines;

	/** Character spawned for this NPC. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC")
	TSubclassOf<ANpcCharacter> NpcActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC", meta = (ClampMin = "1.0", Units = "cm/s"))
	float WalkSpeed = 150.f;

	/** Pause between walks, picked between the two values. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC", meta = (ClampMin = "0.0", Units = "s"))
	float IdleTimeMin = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC", meta = (ClampMin = "0.0", Units = "s"))
	float IdleTimeMax = 8.f;

	/** Reports the first invalid field as a diagnostic. */
	UFUNCTION(BlueprintPure, Category = "NPC")
	bool ValidateNpc(FString& OutDiagnostic) const;

	/** Default object of a concrete, valid NPC class; null (with a diagnostic) otherwise. */
	static const UNpcDefinition* ResolveValidNpc(TSubclassOf<UNpcDefinition> NpcClass, FString& OutDiagnostic);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
