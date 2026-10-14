#pragma once

#include "CoreMinimal.h"
#include "Building/BuildingTypes.h"
#include "Components/ActorComponent.h"
#include "ConstructionComponent.generated.h"

class ABuildingPieceActor;
class UBuildingDefinition;
class UBuildingSubsystem;
class UInventoryComponent;

/**
 * Result of one construction request (CanBuild or TryBuild). TryBuild builds it once, after the cost transaction
 * returns, and never changes it afterwards. Piece is weak: an OnBuildResult listener may destroy the piece, so
 * later listeners and the caller must check it before use.
 */
USTRUCT(BlueprintType)
struct NULLTIDE_API FBuildingConstructionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Building")
	EBuildResult Result = EBuildResult::InvalidDefinition;

	/** The placed piece on Success; null for failures and for CanBuild. Reads invalid once the piece is gone. */
	UPROPERTY(BlueprintReadOnly, Category = "Building")
	TWeakObjectPtr<AActor> Piece;

	/** The resolved placement (surface, layer and transform recomputed by the subsystem). */
	UPROPERTY(BlueprintReadOnly, Category = "Building")
	FBuildingPlacement Placement;

	UPROPERTY(BlueprintReadOnly, Category = "Building")
	FString Diagnostic;

	bool IsSuccess() const { return Result == EBuildResult::Success; }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBuildResult,
	TSubclassOf<UBuildingDefinition>, DefinitionClass, const FBuildingConstructionResult&, Result);

/**
 * A builder's construction authority (the UCraftingComponent pattern). Holds the registered buildings and builds
 * them against the owner's UInventoryComponent and the world's UBuildingSubsystem. Stores no inventory contents and
 * never writes the occupancy registry except through the subsystem.
 *
 * TryBuild activates the piece first and pays last: guard, resolve and validate, reserve, spawn provisional,
 * revalidate, activate, integrity check, then one atomic cost transaction as the commit point. Every failure before
 * payment is a fail-closed rollback that never touches the inventory; nothing fallible runs after payment.
 */
UCLASS(ClassGroup = (Building), meta = (BlueprintSpawnableComponent))
class NULLTIDE_API UConstructionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UConstructionComponent();

	/**
	 * Registered buildings in authored order, without null, abstract, invalid or duplicate entries, and without
	 * buildings whose piece class is not an ABuildingPieceActor (they can never be constructed).
	 */
	UFUNCTION(BlueprintPure, Category = "Building")
	TArray<TSubclassOf<UBuildingDefinition>> GetAvailableBuildings() const;

	UFUNCTION(BlueprintPure, Category = "Building")
	bool IsBuildingAvailable(TSubclassOf<UBuildingDefinition> DefinitionClass) const;

	/**
	 * Exact read-only preflight: definition, registration, piece class, inventory, placement validation and the
	 * cost preflight, as TryBuild runs them. Returns BuildInProgress while a build runs. Changes nothing.
	 */
	UFUNCTION(BlueprintPure, Category = "Building")
	FBuildingConstructionResult CanBuild(TSubclassOf<UBuildingDefinition> DefinitionClass, const FBuildingPlacement& Placement) const;

	/**
	 * Builds one piece and pays its cost once, or changes nothing. Broadcasts OnBuildResult exactly once unless the
	 * call is reentrant (BuildInProgress, no notification).
	 */
	UFUNCTION(BlueprintCallable, Category = "Building")
	FBuildingConstructionResult TryBuild(TSubclassOf<UBuildingDefinition> DefinitionClass, const FBuildingPlacement& Placement);

	UFUNCTION(BlueprintPure, Category = "Building")
	bool IsBuildInProgress() const { return bBuildInProgress; }

	/** The owner's inventory authority, or null when the owner has none. */
	UFUNCTION(BlueprintPure, Category = "Building")
	UInventoryComponent* GetInventory() const;

	/** Outcome of every non-reentrant TryBuild. */
	UPROPERTY(BlueprintAssignable, Category = "Building")
	FOnBuildResult OnBuildResult;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	/**
	 * Step 4: spawns the piece deferred with AlwaysSpawn at the resolved transform, initializes it as Provisional
	 * and player-built, hides it and removes its collision and navigation, then finishes spawning. May return null.
	 */
	virtual ABuildingPieceActor* SpawnProvisionalPiece(TSubclassOf<UBuildingDefinition> DefinitionClass,
		const FBuildingPlacementResult& Resolved);

	/**
	 * Step 6: activates the unpaid piece: Active state, reservation committed to the piece, provisional state
	 * removed. Returns false when the registry refuses the commit.
	 */
	virtual bool ActivatePiece(ABuildingPieceActor& Piece, const FBuildingReservationHandle& Handle);

private:
	/** Steps 2-9 inside the guard; the caller broadcasts the returned result once. */
	FBuildingConstructionResult RunBuild(TSubclassOf<UBuildingDefinition> DefinitionClass, const FBuildingPlacement& Placement);

	/** Step 2 (shared with CanBuild). Returns false with OutFailure set when the request cannot proceed. */
	bool ValidateRequest(TSubclassOf<UBuildingDefinition> DefinitionClass, const FBuildingPlacement& Placement,
		const UBuildingDefinition*& OutDefinition, UInventoryComponent*& OutInventory, UBuildingSubsystem*& OutSubsystem,
		FBuildingPlacementResult& OutResolved, FBuildingConstructionResult& OutFailure) const;

	/** Step 7: whether the activated piece is still exactly the approved, usable piece. */
	bool IsActivatedPieceIntact(const ABuildingPieceActor* Piece, const FBuildingPlacementResult& Resolved,
		const UBuildingSubsystem& Subsystem, const UInventoryComponent* Inventory, FString& OutDiagnostic) const;

	/** Fail-closed rollback (steps 4-8). Safe to repeat; never touches the inventory. */
	void RollbackPiece(ABuildingPieceActor* Piece, const FBuildingReservationHandle& Handle, UBuildingSubsystem& Subsystem) const;

	/** Authored building list. Order is the listing order; only these buildings can be built. */
	UPROPERTY(EditDefaultsOnly, Category = "Building", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<UBuildingDefinition>> AvailableBuildings;

	bool bBuildInProgress = false;
	bool bEndPlayed = false;
};
