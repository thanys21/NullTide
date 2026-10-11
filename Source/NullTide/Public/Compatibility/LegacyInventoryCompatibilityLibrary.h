// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LegacyInventoryCompatibilityLibrary.generated.h"

class UActorComponent;
class UItemDefinition;

/** Compatibility boundary: legacy rollback writes before cutover, native transactions/queries afterward. */
UCLASS()
class NULLTIDE_API ULegacyInventoryCompatibilityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Retained production adapter. New native callers should use UInventoryComponent::TryAddDefinition. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Compatibility",
		meta = (ToolTip = "Compatibility transaction adapter for existing callers and rollback. New code should use UInventoryComponent::TryAddDefinition."))
	static FInventoryOperationResult TryAddDefinitionToLegacyInventory(
		UActorComponent* LegacyInventory,
		TSubclassOf<UItemDefinition> DefinitionClass);

	/** Quantity-aware adapter for generic world pickups. Native inventories commit the whole quantity atomically. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Compatibility",
		meta = (ToolTip = "Compatibility transaction adapter for quantity-bearing pickups. Native inventories add atomically."))
	static FInventoryOperationResult TryAddDefinitionQuantityToLegacyInventory(
		UActorComponent* LegacyInventory,
		TSubclassOf<UItemDefinition> DefinitionClass,
		int32 Quantity);

	UFUNCTION(BlueprintPure, Category = "Inventory|Compatibility",
		meta = (DeprecatedFunction, DeprecationMessage = "Legacy class-array query: use UInventoryComponent::GetItemsSnapshot for new code. Retained for rollback compatibility."))
	static TArray<TSubclassOf<UItemDefinition>> GetLegacyItemsSnapshot(
		UActorComponent* LegacyInventory,
		EInventoryOperationResult& Result);
};
