#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CraftingStationComponent.generated.h"

class UCraftingStationDefinition;

/**
 * Marks its owner as a crafting station of one kind. Context only: it answers which station kind it is and
 * whether an actor is close enough to use it. It never crafts, stores items or touches an inventory; the
 * crafter's UCraftingComponent validates station access and runs every craft.
 */
UCLASS(ClassGroup = (Crafting), meta = (BlueprintSpawnableComponent))
class NULLTIDE_API UCraftingStationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCraftingStationComponent();

	UFUNCTION(BlueprintPure, Category = "Crafting|Station")
	TSubclassOf<UCraftingStationDefinition> GetStationDefinition() const { return StationDefinition; }

	/** Default object of the station definition when it is concrete and valid; null otherwise. */
	const UCraftingStationDefinition* GetValidStationDefinition() const;

	/** True while the owner is alive (not being destroyed) and the station definition is valid. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Station")
	bool IsStationUsable() const;

	/** Whether the actor is within the definition's UseRange (horizontal distance between actor locations). */
	UFUNCTION(BlueprintPure, Category = "Crafting|Station")
	bool IsActorInUseRange(const AActor* Actor) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting|Station", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UCraftingStationDefinition> StationDefinition;
};
