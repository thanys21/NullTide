// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InventoryTypes.generated.h"

class UItemDefinition;
class UItemInstance;
class AActor;

UENUM(BlueprintType)
enum class EInventoryInitializationState : uint8
{
	NotRequired,
	PendingLegacyImport,
	NativeReady,
	Failed
};

UENUM(BlueprintType, meta = (ScriptName = "InventoryOperationResultCode"))
enum class EInventoryOperationResult : uint8
{
	Success,
	InvalidDefinition,
	InvalidDefinitionData,
	InvalidItemId,
	InvalidQuantity,
	CapacityFull,
	NotInitialized,
	Busy,
	/** The inventory holds less than the requested total quantity. */
	InsufficientQuantity,
	/** A split needs a stack of at least 2 (G9). */
	CannotSplit
};

/** A definition and quantity requested from an inventory query. */
USTRUCT(BlueprintType)
struct NULLTIDE_API FInventoryDefinitionQuantity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TSubclassOf<UItemDefinition> DefinitionClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "1"))
	int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct NULLTIDE_API FInventoryOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EInventoryOperationResult Result = EInventoryOperationResult::NotInitialized;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UItemInstance> Item = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FGuid ItemId;

	/** Quantity added or removed by this operation. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Quantity = 0;

	bool IsSuccess() const
	{
		return Result == EInventoryOperationResult::Success;
	}
};

/** Which request list a failed definition transaction stopped on. */
UENUM(BlueprintType)
enum class EInventoryTransactionSide : uint8
{
	None,
	Removal,
	Addition
};

/** Result of one atomic remove-and-add definition transaction. */
USTRUCT(BlueprintType)
struct NULLTIDE_API FInventoryTransactionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EInventoryOperationResult Result = EInventoryOperationResult::NotInitialized;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EInventoryTransactionSide FailedSide = EInventoryTransactionSide::None;

	/** First caller index (in Removals or Additions) of the definition that failed, or INDEX_NONE. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 FailedIndex = INDEX_NONE;

	bool IsSuccess() const
	{
		return Result == EInventoryOperationResult::Success;
	}
};

UENUM(BlueprintType, meta = (ScriptName = "InventoryWorldDropResultCode"))
enum class EInventoryWorldDropResult : uint8
{
	Success,
	InvalidPawn,
	InvalidItemId,
	InvalidDefinition,
	InvalidDefinitionData,
	NotInitialized,
	Busy,
	NoSafeTransform,
	SpawnFailed,
	RemovalFailedRolledBack
};

/** Result for the authoritative inventory-to-world transaction. */
USTRUCT(BlueprintType)
struct NULLTIDE_API FInventoryWorldDropResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EInventoryWorldDropResult Result = EInventoryWorldDropResult::NotInitialized;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FGuid ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<AActor> SpawnedPickup = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Quantity = 0;

	bool IsSuccess() const
	{
		return Result == EInventoryWorldDropResult::Success;
	}
};

UENUM(BlueprintType, meta = (ScriptName = "InventoryStorageTransferResultCode"))
enum class EInventoryStorageTransferResult : uint8
{
	Success,
	InvalidSource,
	InvalidDestination,
	InvalidItemId,
	DestinationFull,
	Busy,
	TransferFailed,
	/** A batch found no stack to move on the source side (G9). */
	NothingToMove
};

/** Result for an exact runtime-item transfer between player inventory and storage. */
USTRUCT(BlueprintType)
struct NULLTIDE_API FInventoryStorageTransferResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Storage")
	EInventoryStorageTransferResult Result = EInventoryStorageTransferResult::InvalidSource;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Storage")
	FGuid ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Storage")
	TObjectPtr<UItemInstance> Item = nullptr;

	/** Units moved by this transfer (the whole stack for an exact transfer, or the part that fit, G9). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Storage")
	int32 MovedQuantity = 0;

	bool IsSuccess() const
	{
		return Result == EInventoryStorageTransferResult::Success;
	}
};

/** Result of a batch storage move (G9: every stack of one item, or everything), as much as fits. */
USTRUCT(BlueprintType)
struct NULLTIDE_API FStorageTransferBatchResult
{
	GENERATED_BODY()

	/** Success when anything moved; NothingToMove or DestinationFull when nothing did (then nothing changed). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Storage")
	EInventoryStorageTransferResult Result = EInventoryStorageTransferResult::InvalidSource;

	/** Units moved in total. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Storage")
	int32 MovedQuantity = 0;

	/** Source stacks that moved fully or in part. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Storage")
	int32 MovedStacks = 0;

	/** Matching source stacks still on the source side afterwards (whole or remainder). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Storage")
	int32 StacksLeft = 0;

	bool IsSuccess() const
	{
		return Result == EInventoryStorageTransferResult::Success;
	}
};

/** Blueprint helpers for inventory operation results. */
UCLASS()
class NULLTIDE_API UInventoryOperationResultLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Inventory", meta = (DisplayName = "Is Success", ScriptMethod))
	static bool IsSuccess(const FInventoryOperationResult& OperationResult)
	{
		return OperationResult.IsSuccess();
	}
};
