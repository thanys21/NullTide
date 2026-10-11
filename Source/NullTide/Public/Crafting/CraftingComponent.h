// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Crafting/CraftingTransaction.h"
#include "Engine/EngineTypes.h"
#include "CraftingComponent.generated.h"

class ACharacter;
class UCraftingRecipeDefinition;
class UInventoryComponent;

/** Why an active timed craft stopped before completing. */
UENUM(BlueprintType)
enum class ECraftCancelReason : uint8
{
	Explicit,
	Movement,
	Jump
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
	 * Exact read-only preflight: whether the recipe could be crafted from the current inventory, without
	 * changing anything. Returns CraftInProgress while a timed craft is active.
	 */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	FCraftingTransactionResult CanCraftRecipe(TSubclassOf<UCraftingRecipeDefinition> RecipeClass) const;

	/**
	 * Crafts one registered instant recipe atomically. Timed recipes return TimedCraftRequired without
	 * changing anything. Broadcasts OnCraftingResult once unless the call is reentrant.
	 */
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	FCraftingTransactionResult TryCraftRecipe(TSubclassOf<UCraftingRecipeDefinition> RecipeClass);

	/**
	 * The UI entry point. Instant recipes craft immediately (as TryCraftRecipe). Timed recipes run the exact
	 * preflight, start the timer, broadcast OnCraftStarted and return Started; ingredients are consumed only
	 * at completion, which re-validates and broadcasts OnCraftingResult. Refused starts notify OnCraftingResult.
	 */
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	FCraftingTransactionResult TryStartCraft(TSubclassOf<UCraftingRecipeDefinition> RecipeClass);

	/** Cancels the active timed craft without changing the inventory. Returns false when nothing was active. */
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	bool CancelCraft();

	UFUNCTION(BlueprintPure, Category = "Crafting")
	bool IsCrafting() const { return ActiveRecipe != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Crafting")
	TSubclassOf<UCraftingRecipeDefinition> GetActiveCraftRecipe() const { return ActiveRecipe; }

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

	/** Explicit, movement and jump cancellation. Not broadcast on EndPlay. */
	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FOnCraftCancelled OnCraftCancelled;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	/** Validates the request up to the inventory; returns false with OutFailure set when it cannot proceed. */
	bool ValidateRequest(TSubclassOf<UCraftingRecipeDefinition> RecipeClass, const UCraftingRecipeDefinition*& OutRecipe,
		UInventoryComponent*& OutInventory, FCraftingTransactionResult& OutFailure) const;

	void HandleCraftTimerCompleted();
	void CancelActiveCraft(ECraftCancelReason Reason);
	/** Clears the timer, the movement bindings and the active state. Never notifies. */
	void ClearActiveCraft();
	void BindOwnerMovement();
	void UnbindOwnerMovement();

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
	FTimerHandle CraftTimerHandle;
	FVector ActiveCraftStartLocation = FVector::ZeroVector;
	bool bCraftInProgress = false;
};
