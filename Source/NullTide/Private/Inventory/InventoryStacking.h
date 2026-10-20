#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryTypes.h"
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

/**
 * Read-only planning view of one stack. Item is the live stack it describes (never written during
 * planning), or null for a stack that a plan would create. Quantity may differ from the live stack
 * when the entry describes a projected state.
 */
struct FInventoryStackEntry
{
	TObjectPtr<UItemInstance> Item = nullptr;
	TSubclassOf<UItemDefinition> DefinitionClass;
	int32 Quantity = 0;
};

/** Full plan for one atomic definition transaction. Built without touching live stacks. */
struct FInventoryTransactionPlan
{
	/** Live stacks that survive the removals, in inventory order, with their post-removal quantities. */
	TArray<FInventoryStackEntry> SurvivingEntries;
	/** Add plans, in addition order, computed against the post-removal state. */
	TArray<TSubclassOf<UItemDefinition>> AdditionDefinitions;
	TArray<FInventoryStackPlan> AdditionPlans;
};

/** Shared stack preflight and commit helpers for inventory and storage authorities. */
class FInventoryStacking
{
public:
	static bool ResolveMaxStackSize(TSubclassOf<UItemDefinition> DefinitionClass, int32& OutMaxStackSize);
	static void MakeEntries(const TArray<TObjectPtr<UItemInstance>>& Items, TArray<FInventoryStackEntry>& OutEntries);
	/** Plans an add against live stacks. Thin wrapper over BuildAddPlanForEntries. */
	static bool BuildAddPlan(const TArray<TObjectPtr<UItemInstance>>& DestinationItems, int32 Capacity,
		TSubclassOf<UItemDefinition> DefinitionClass, int32 Quantity, FInventoryStackPlan& OutPlan);
	/** The single stacking rule set: fill compatible live stacks in order, then plan new stacks, then check slots. */
	static bool BuildAddPlanForEntries(const TArray<FInventoryStackEntry>& Entries, int32 Capacity,
		TSubclassOf<UItemDefinition> DefinitionClass, int32 Quantity, FInventoryStackPlan& OutPlan);
	/** Updates entries as if Plan were committed: merged quantities rise and planned stacks are appended. */
	static void ApplyPlanToEntries(const FInventoryStackPlan& Plan, TSubclassOf<UItemDefinition> DefinitionClass,
		TArray<FInventoryStackEntry>& InOutEntries);
	/**
	 * Removes Quantity of exactly DefinitionClass from the entries, earliest stacks first, and drops entries
	 * that reach zero while keeping order. Changes only the entries. Returns false, leaving them unchanged,
	 * when the entries hold less than Quantity.
	 */
	static bool PlanRemoval(TArray<FInventoryStackEntry>& InOutEntries, TSubclassOf<UItemDefinition> DefinitionClass,
		int32 Quantity);
	static bool StageNewStacks(UObject* Owner, TSubclassOf<UItemDefinition> DefinitionClass,
		const TArray<int32>& Quantities, const TSet<FGuid>& ReservedIds,
		TArray<TStrongObjectPtr<UItemInstance>>& OutStacks);
	static bool SetStackQuantity(UItemInstance* Item, int32 Quantity);
	static void ApplyMerges(const FInventoryStackPlan& Plan);
	/**
	 * Splits a stack in place (G9): a new stack of floor(Q/2) with a new id is inserted right after the source, which
	 * keeps ceil(Q/2) and its id. Needs Q >= 2 (CannotSplit) and a free slot (CapacityFull); otherwise nothing changes.
	 * Commits directly; the caller owns the mutation guard, revision and broadcast.
	 */
	static EInventoryOperationResult SplitStack(TArray<TObjectPtr<UItemInstance>>& Items, int32 Capacity, UObject* Owner,
		FGuid ItemId, UItemInstance*& OutNewStack);
};
