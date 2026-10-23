#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NpcDefinition.generated.h"

class ANpcCharacter;
class UItemDefinition;

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

	// ---- Store (S5). An empty catalog means the NPC does not trade. ----

	/** Items the NPC sells, unlimited. Each must have a trade value. The NPC buys any item with a trade value. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC|Store")
	TArray<TSubclassOf<UItemDefinition>> StoreItems;

	/** Share of an item's value the NPC pays when the player sells. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC|Store", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SellRate = 0.75f;

	/** Share of an item's value the player pays when buying. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC|Store", meta = (ClampMin = "0.0"))
	float BuyRate = 1.f;

	/** Item types allowed on each side of one trade. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC|Store", meta = (ClampMin = "1"))
	int32 MaxLinesPerSide = 16;

	/** Haggle attempts per game day: a wrong or timed-out answer uses one, and so does a trade that applies a discount. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC|Haggle", meta = (ClampMin = "0"))
	int32 HaggleAttemptsPerDay = 3;

	/** A correct answer rolls a discount between these whole percentages. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC|Haggle", meta = (ClampMin = "0", ClampMax = "100"))
	int32 HaggleDiscountMinPercent = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC|Haggle", meta = (ClampMin = "0", ClampMax = "100"))
	int32 HaggleDiscountMaxPercent = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC|Haggle", meta = (ClampMin = "1.0", Units = "s"))
	float HaggleTimeLimit = 10.f;

	/** Whether this NPC has a store (a non-empty catalog). */
	bool HasStore() const { return !StoreItems.IsEmpty(); }

	/** Reports the first invalid field as a diagnostic. */
	UFUNCTION(BlueprintPure, Category = "NPC")
	bool ValidateNpc(FString& OutDiagnostic) const;

	/** The store part of ValidateNpc (always valid without a catalog). */
	bool ValidateStore(FString& OutDiagnostic) const;

	/** Default object of a concrete, valid NPC class; null (with a diagnostic) otherwise. */
	static const UNpcDefinition* ResolveValidNpc(TSubclassOf<UNpcDefinition> NpcClass, FString& OutDiagnostic);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
