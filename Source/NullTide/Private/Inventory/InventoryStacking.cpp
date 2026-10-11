#include "Inventory/InventoryStacking.h"

#include "Items/Fragments/ItemFragment_Stackable.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"

bool FInventoryStacking::ResolveMaxStackSize(
	const TSubclassOf<UItemDefinition> DefinitionClass,
	int32& OutMaxStackSize)
{
	OutMaxStackSize = 1;
	const UItemDefinition* Definition = DefinitionClass ? DefinitionClass->GetDefaultObject<UItemDefinition>() : nullptr;
	if (!IsValid(Definition))
	{
		return false;
	}

	const UItemFragment_Stackable* Stackable = Cast<UItemFragment_Stackable>(
		Definition->FindFragmentByClass(UItemFragment_Stackable::StaticClass()));
	if (Stackable)
	{
		if (Stackable->MaxStackSize < 2)
		{
			return false;
		}
		OutMaxStackSize = Stackable->MaxStackSize;
	}
	return true;
}

void FInventoryStacking::MakeEntries(
	const TArray<TObjectPtr<UItemInstance>>& Items,
	TArray<FInventoryStackEntry>& OutEntries)
{
	OutEntries.Reset(Items.Num());
	for (UItemInstance* Item : Items)
	{
		FInventoryStackEntry& Entry = OutEntries.AddDefaulted_GetRef();
		if (IsValid(Item))
		{
			Entry.Item = Item;
			Entry.DefinitionClass = Item->GetDefinitionClass();
			Entry.Quantity = Item->GetQuantity();
		}
	}
}

bool FInventoryStacking::BuildAddPlan(
	const TArray<TObjectPtr<UItemInstance>>& DestinationItems,
	const int32 Capacity,
	const TSubclassOf<UItemDefinition> DefinitionClass,
	const int32 Quantity,
	FInventoryStackPlan& OutPlan)
{
	TArray<FInventoryStackEntry> Entries;
	MakeEntries(DestinationItems, Entries);
	return BuildAddPlanForEntries(Entries, Capacity, DefinitionClass, Quantity, OutPlan);
}

bool FInventoryStacking::BuildAddPlanForEntries(
	const TArray<FInventoryStackEntry>& Entries,
	const int32 Capacity,
	const TSubclassOf<UItemDefinition> DefinitionClass,
	const int32 Quantity,
	FInventoryStackPlan& OutPlan)
{
	OutPlan = FInventoryStackPlan();
	if (Capacity < 1 || !DefinitionClass || Quantity < 1)
	{
		return false;
	}

	int32 MaxStackSize = 1;
	if (!ResolveMaxStackSize(DefinitionClass, MaxStackSize))
	{
		return false;
	}

	int32 Remaining = Quantity;
	if (MaxStackSize > 1)
	{
		for (const FInventoryStackEntry& Existing : Entries)
		{
			// Only live stacks can be merge targets; planned stacks do not exist yet.
			if (!IsValid(Existing.Item) || Existing.DefinitionClass != DefinitionClass || Existing.Quantity >= MaxStackSize)
			{
				continue;
			}
			const int32 ToAdd = FMath::Min(Remaining, MaxStackSize - Existing.Quantity);
			if (ToAdd > 0)
			{
				OutPlan.Merges.Add({ Existing.Item, ToAdd });
				Remaining -= ToAdd;
				if (Remaining == 0)
				{
					break;
				}
			}
		}
	}

	while (Remaining > 0)
	{
		const int32 NewStackQuantity = FMath::Min(Remaining, MaxStackSize);
		OutPlan.NewStackQuantities.Add(NewStackQuantity);
		Remaining -= NewStackQuantity;
	}

	return Entries.Num() + OutPlan.NewStackQuantities.Num() <= Capacity;
}

void FInventoryStacking::ApplyPlanToEntries(
	const FInventoryStackPlan& Plan,
	const TSubclassOf<UItemDefinition> DefinitionClass,
	TArray<FInventoryStackEntry>& InOutEntries)
{
	for (const FInventoryStackMerge& Merge : Plan.Merges)
	{
		FInventoryStackEntry* Target = InOutEntries.FindByPredicate(
			[&Merge](const FInventoryStackEntry& Entry) { return Entry.Item == Merge.Item; });
		check(Target);
		Target->Quantity += Merge.Quantity;
	}
	for (const int32 NewStackQuantity : Plan.NewStackQuantities)
	{
		FInventoryStackEntry& Planned = InOutEntries.AddDefaulted_GetRef();
		Planned.DefinitionClass = DefinitionClass;
		Planned.Quantity = NewStackQuantity;
	}
}

bool FInventoryStacking::PlanRemoval(
	TArray<FInventoryStackEntry>& InOutEntries,
	const TSubclassOf<UItemDefinition> DefinitionClass,
	const int32 Quantity)
{
	if (!DefinitionClass || Quantity < 1)
	{
		return false;
	}
	int64 Available = 0;
	for (const FInventoryStackEntry& Entry : InOutEntries)
	{
		if (Entry.DefinitionClass == DefinitionClass && Entry.Quantity > 0)
		{
			Available += Entry.Quantity;
		}
	}
	if (Available < Quantity)
	{
		return false;
	}

	int32 Remaining = Quantity;
	for (FInventoryStackEntry& Entry : InOutEntries)
	{
		if (Remaining == 0)
		{
			break;
		}
		if (Entry.DefinitionClass != DefinitionClass || Entry.Quantity < 1)
		{
			continue;
		}
		const int32 Taken = FMath::Min(Remaining, Entry.Quantity);
		Entry.Quantity -= Taken;
		Remaining -= Taken;
	}
	// Drop only the stacks this removal emptied. RemoveAll keeps the relative order of the rest.
	InOutEntries.RemoveAll([&DefinitionClass](const FInventoryStackEntry& Entry)
	{
		return Entry.DefinitionClass == DefinitionClass && Entry.Quantity < 1;
	});
	return true;
}

bool FInventoryStacking::StageNewStacks(
	UObject* Owner,
	const TSubclassOf<UItemDefinition> DefinitionClass,
	const TArray<int32>& Quantities,
	const TSet<FGuid>& ReservedIds,
	TArray<TStrongObjectPtr<UItemInstance>>& OutStacks)
{
	OutStacks.Reset();
	if (!IsValid(Owner) || !DefinitionClass)
	{
		return false;
	}

	TSet<FGuid> UsedIds = ReservedIds;
	OutStacks.Reserve(Quantities.Num());
	for (const int32 Quantity : Quantities)
	{
		if (Quantity < 1)
		{
			return false;
		}
		FGuid NewItemId;
		do
		{
			NewItemId = FGuid::NewGuid();
		}
		while (!NewItemId.IsValid() || UsedIds.Contains(NewItemId));

		UItemInstance* NewItem = NewObject<UItemInstance>(Owner);
		if (!NewItem || !NewItem->Initialize(NewItemId, DefinitionClass, Quantity))
		{
			OutStacks.Reset();
			return false;
		}
		UsedIds.Add(NewItemId);
		OutStacks.Emplace(NewItem);
	}
	return true;
}

bool FInventoryStacking::SetStackQuantity(UItemInstance* Item, const int32 Quantity)
{
	return IsValid(Item) && Item->SetQuantity(Quantity);
}

void FInventoryStacking::ApplyMerges(const FInventoryStackPlan& Plan)
{
	for (const FInventoryStackMerge& Merge : Plan.Merges)
	{
		check(IsValid(Merge.Item));
		check(Merge.Quantity > 0);
		check(Merge.Item->SetQuantity(Merge.Item->GetQuantity() + Merge.Quantity));
	}
}
