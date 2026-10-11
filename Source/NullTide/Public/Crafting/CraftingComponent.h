// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Crafting/CraftingTransaction.h"
#include "Engine/EngineTypes.h"
#include "CraftingComponent.generated.h"

class ACharacter;
class UCraftingRecipeDefinition;
class UCraftingStationComponent;
class UInventoryComponent;

/** Why an active timed craft stopped before completing. */
UENUM(BlueprintType)
enum class ECraftCancelReason : uint8
{
	Explicit,
	Movement,
	Jump,
	/** The station a station recipe was started at was destroyed or unloaded. */
	StationLost
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCraftingResult,
	TSubclassOf<UCraftingRecipeDefinition>, RecipeClass, const FCraftingTransactionResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCraftStarted,
	TSubclassOf<UCraftingRecipeDefinition>, RecipeClass, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCraftCancelled,
	TSubclassOf<UCraftingRecipeDefinition>, RecipeClass, ECraftCancelReason, Reason);

/**
 * The player's crafting authority. Holds the registered recipes and crafts them against the owner's
 * UInventoryComponent through one atomic inventory transaction. Timed recipes run on one world timer and
 * consume ingredients only when they complete. Stores no inventory contents.
 *
 * Stations are context: station-aware calls take the UCraftingStationComponent explicitly, and recipes with a
 * RequiredStation are craftable only with a usable station of that exact kind in range. The functions without
 * a station parameter are the hand context.
 */
UCLASS(ClassGroup = (Crafting), meta = (BlueprintSpawnableComponent))
class NULLTIDE_API UCraftingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCraftingComponent();

	/** Registered recipes in authored order, without null, abstract, invalid or duplicate entries. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	TArray<TSubclassOf<UCraftingRecipeDefinition>> GetAvailableRecipes() const;

	UFUNCTION(BlueprintPure, Category = "Crafting")
	bool IsRecipeAvailable(TSubclassOf<UCraftingRecipeDefinition> RecipeClass) const;

	/**
	 * Registered recipes usable in a context, in authored order. With a usable station: recipes that require
	 * that exact station kind first, then hand recipes. Without one: hand recipes only. Range is not
	 * considered here; CanCraftRecipeAtStation reports StationOutOfRange.
	 */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	TArray<TSubclassOf<UCraftingRecipeDefinition>> GetRecipesForStation(UCraftingStationComponent* Station) const;

	/**
	 * Exact read-only preflight in the hand context: whether the recipe could be crafted from the current
	 * inventory, without changing anything. Station recipes return StationRequired. Returns CraftInProgress
	 * while a timed craft is active.
	 */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	FCraftingTransactionResult CanCraftRecipe(TSubclassOf<UCraftingRecipeDefinition> RecipeClass) const;

	/** CanCraftRecipe at a station: also validates the station kind, its lifetime and the crafter's range. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	FCraftingTransactionResult CanCraftRecipeAtStation(TSubclassOf<UCraftingRecipeDefinition> RecipeClass,
		UCraftingStationComponent* Station) const;

	/**
	 * Crafts one registered instant hand recipe atomically. Timed recipes return TimedCraftRequired and station
	 * recipes StationRequired, without changing anything. Broadcasts OnCraftingResult once unless the call is reentrant.
	 */
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	FCraftingTransactionResult TryCraftRecipe(TSubclassOf<UCraftingRecipeDefinition> RecipeClass);

	/**
	 * The UI entry point in the hand context. Instant recipes craft immediately (as TryCraftRecipe). Timed recipes run the exact
	 * preflight, start the timer, broadcast OnCraftStarted and return Started; ingredients are consumed only
	 * at completion, which re-validates and broadcasts OnCraftingResult. Refused starts notify OnCraftingResult.
	 */
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	FCraftingTransactionResult TryStartCraft(TSubclassOf<UCraftingRecipeDefinition> RecipeClass);

	/**
	 * TryStartCraft at a station. Station access is validated at start and again at completion. A timed
	 * station recipe remembers the station and is cancelled (StationLost) if the station actor ends play.
	 * Hand recipes started here do not depend on the station.
	 */
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	FCraftingTransactionResult TryStartCraftAtStation(TSubclassOf<UCraftingRecipeDefinition> RecipeClass,
		UCraftingStationComponent* Station);

	/** Cancels the active timed craft without changing the inventory. Returns false when nothing was active. */
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	bool CancelCraft();

	UFUNCTION(BlueprintPure, Category = "Crafting")
	bool IsCrafting() const { return ActiveRecipe != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Crafting")
	TSubclassOf<UCraftingRecipeDefinition> GetActiveCraftRecipe() const { return ActiveRecipe; }

	/** The station the active timed station recipe depends on; null when idle, for hand recipes or once it is gone. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	UCraftingStationComponent* GetActiveCraftStation() const;

	/** 0..1 progress of the active timed craft, read from the world timer; 0 when idle. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	float GetActiveCraftProgress() const;

	/** Seconds left on the active timed craft; 0 when idle. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	float GetActiveCraftRemainingSeconds() const;

	/** The owner's inventory authority, or null when the owner has none. */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	UInventoryComponent* GetInventory() const;

	/** Outcome of every non-reentrant craft request: instant crafts, refused starts and timed completions. */
	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FOnCraftingResult OnCraftingResult;

	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FOnCraftStarted OnCraftStarted;

	/** Explicit, movement, jump and station-lost cancellation. Not broadcast on EndPlay. */
	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FOnCraftCancelled OnCraftCancelled;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	/**
	 * Validates the request (recipe, registration, station access, inventory); returns false with OutFailure
	 * set when it cannot proceed. Station may be null (hand context).
	 */
	bool ValidateRequest(TSubclassOf<UCraftingRecipeDefinition> RecipeClass, const UCraftingStationComponent* Station,
		const UCraftingRecipeDefinition*& OutRecipe, UInventoryComponent*& OutInventory, FCraftingTransactionResult& OutFailure) const;

	/** Station rules for one valid recipe. Hand recipes always pass. */
	bool ValidateStationAccess(const UCraftingRecipeDefinition& Recipe, const UCraftingStationComponent* Station,
		FCraftingTransactionResult& OutFailure) const;

	void HandleCraftTimerCompleted();
	void CancelActiveCraft(ECraftCancelReason Reason);
	/** Clears the timer, the movement and station bindings and the active state. Never notifies. */
	void ClearActiveCraft();
	void BindOwnerMovement();
	void UnbindOwnerMovement();
	void BindActiveStation(UCraftingStationComponent* Station);
	void UnbindActiveStation();

	UFUNCTION()
	void HandleActiveStationEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

	UFUNCTION()
	void HandleOwnerMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

	UFUNCTION()
	void HandleOwnerMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	/** Authored recipe list. Order is the listing order; only these recipes can be crafted. */
	UPROPERTY(EditDefaultsOnly, Category = "Crafting", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<UCraftingRecipeDefinition>> AvailableRecipes;

	/** Horizontal distance from the start location that cancels a timed craft. */
	UPROPERTY(EditDefaultsOnly, Category = "Crafting", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float MovementCancelTolerance = 10.f;

	UPROPERTY(Transient)
	TSubclassOf<UCraftingRecipeDefinition> ActiveRecipe;

	TWeakObjectPtr<ACharacter> BoundCharacter;
	/** Station the active timed station recipe depends on; unset for hand recipes. */
	TWeakObjectPtr<UCraftingStationComponent> ActiveCraftStation;
	/** Station owner whose OnEndPlay is bound while that recipe runs. */
	TWeakObjectPtr<AActor> BoundStationActor;
	FTimerHandle CraftTimerHandle;
	FVector ActiveCraftStartLocation = FVector::ZeroVector;
	bool bCraftInProgress = false;
};
