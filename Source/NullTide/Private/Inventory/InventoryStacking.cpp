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

bool FInventoryStacking::BuildAddPlan(
	const TArray<TObjectPtr<UItemInstance>>& DestinationItems,
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
		for (UItemInstance* Existing : DestinationItems)
		{
			if (!IsValid(Existing) || Existing->GetDefinitionClass() != DefinitionClass || Existing->GetQuantity() >= MaxStackSize)
			{
				continue;
			}
			const int32 ToAdd = FMath::Min(Remaining, MaxStackSize - Existing->GetQuantity());
			if (ToAdd > 0)
			{
				OutPlan.Merges.Add({ Existing, ToAdd });
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

	return DestinationItems.Num() + OutPlan.NewStackQuantities.Num() <= Capacity;
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
