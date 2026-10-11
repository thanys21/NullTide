// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ItemInstance.generated.h"

class UInventoryComponent;
class UItemDefinition;
class UStorageTransferLibrary;
class FInventoryStacking;

/** Runtime identity for one inventory stack. */
UCLASS(BlueprintType)
class NULLTIDE_API UItemInstance : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	FGuid GetInstanceId() const { return InstanceId; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	TSubclassOf<UItemDefinition> GetDefinitionClass() const { return DefinitionClass; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	int32 GetQuantity() const { return Quantity; }

private:
	friend class UInventoryComponent;
	friend class UStorageTransferLibrary;
	friend class FInventoryStacking;

	bool Initialize(const FGuid& InInstanceId, TSubclassOf<UItemDefinition> InDefinitionClass, int32 InQuantity = 1);
	bool SetQuantity(int32 InQuantity);

	UPROPERTY(VisibleInstanceOnly, Category = "Inventory|Item")
	FGuid InstanceId;

	UPROPERTY(VisibleInstanceOnly, Category = "Inventory|Item")
	TSubclassOf<UItemDefinition> DefinitionClass;

	UPROPERTY(VisibleInstanceOnly, Category = "Inventory|Item", meta = (ClampMin = "1"))
	int32 Quantity = 1;
};
