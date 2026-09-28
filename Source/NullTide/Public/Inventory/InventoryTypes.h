// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InventoryTypes.generated.h"

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
	CapacityFull,
	NotInitialized,
	Busy
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
	TransferFailed
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
