#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class UItemDefinition;
class UItemInstance;

struct FInventoryStackMerge
{
	TObjectPtr<UItemInstance> Item = nullptr;
	int32 Quantity = 0;
};

struct FInventoryStackPlan
{
	TArray<FInventoryStackMerge> Merges;
	TArray<int32> NewStackQuantities;
};

/** Shared stack preflight and commit helpers for inventory and storage authorities. */
class FInventoryStacking
{
public:
	static bool ResolveMaxStackSize(TSubclassOf<UItemDefinition> DefinitionClass, int32& OutMaxStackSize);
	static bool BuildAddPlan(const TArray<TObjectPtr<UItemInstance>>& DestinationItems, int32 Capacity,
		TSubclassOf<UItemDefinition> DefinitionClass, int32 Quantity, FInventoryStackPlan& OutPlan);
	static bool StageNewStacks(UObject* Owner, TSubclassOf<UItemDefinition> DefinitionClass,
		const TArray<int32>& Quantities, const TSet<FGuid>& ReservedIds,
		TArray<TStrongObjectPtr<UItemInstance>>& OutStacks);
	static bool SetStackQuantity(UItemInstance* Item, int32 Quantity);
	static void ApplyMerges(const FInventoryStackPlan& Plan);
};
