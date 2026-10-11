// Fill out your copyright notice in the Description page of Project Settings.

#include "Compatibility/LegacyInventoryCompatibilityLibrary.h"

#include "Inventory/InventoryComponent.h"
#include "Inventory/LegacyInventoryStorage.h"
#include "Items/ItemInstance.h"

#include "Items/ItemDefinition.h"
#include "Components/ActorComponent.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

namespace
{
TSet<TWeakObjectPtr<UActorComponent>> ActiveLegacyMutations;

FInventoryOperationResult MakeLegacyResult(EInventoryOperationResult Result)
{
	FInventoryOperationResult OperationResult;
	OperationResult.Result = Result;
	return OperationResult;
}

bool IsLegacyReceiverUsable(const UActorComponent* LegacyInventory)
{
	return IsValid(LegacyInventory)
		&& !LegacyInventory->HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed | RF_FinishDestroyed);
}

bool IsDefinitionUsable(TSubclassOf<UItemDefinition> DefinitionClass)
{
	UClass* DefinitionUClass = DefinitionClass.Get();
	return DefinitionUClass
		&& !DefinitionUClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
}

bool IsDefinitionDataValid(TSubclassOf<UItemDefinition> DefinitionClass)
{
	const UClass* DefinitionUClass = DefinitionClass.Get();
	const UItemDefinition* Definition = DefinitionUClass
		? Cast<UItemDefinition>(DefinitionUClass->GetDefaultObject())
		: nullptr;
	if (!IsValid(Definition))
	{
		return false;
	}

	FString Diagnostic;
	return Definition->ValidateFragments(Diagnostic);
}

bool FindLegacyArray(
	UActorComponent* LegacyInventory,
	FArrayProperty*& OutArrayProperty,
	FClassProperty*& OutClassProperty)
{
	return LegacyInventoryStorage::FindArray(LegacyInventory, OutArrayProperty, OutClassProperty);
}
}

FInventoryOperationResult ULegacyInventoryCompatibilityLibrary::TryAddDefinitionToLegacyInventory(
	UActorComponent* LegacyInventory,
	TSubclassOf<UItemDefinition> DefinitionClass)
{
	return TryAddDefinitionQuantityToLegacyInventory(LegacyInventory, DefinitionClass, 1);
}

FInventoryOperationResult ULegacyInventoryCompatibilityLibrary::TryAddDefinitionQuantityToLegacyInventory(
	UActorComponent* LegacyInventory,
	TSubclassOf<UItemDefinition> DefinitionClass,
	const int32 Quantity)
{
	if (!IsLegacyReceiverUsable(LegacyInventory))
	{
		return MakeLegacyResult(EInventoryOperationResult::NotInitialized);
	}
	if (Quantity < 1)
	{
		return MakeLegacyResult(EInventoryOperationResult::InvalidQuantity);
	}

	if (UInventoryComponent* NativeInventory = Cast<UInventoryComponent>(LegacyInventory))
	{
		if (!NativeInventory->IsLegacyInventoryMode())
		{
			if (NativeInventory->GetInitializationState() != EInventoryInitializationState::NativeReady)
			{
				return MakeLegacyResult(EInventoryOperationResult::NotInitialized);
			}
			return NativeInventory->TryAddDefinitionQuantity(DefinitionClass, Quantity);
		}
		if (NativeInventory->ShouldBlockLegacyWrites())
		{
			return MakeLegacyResult(EInventoryOperationResult::NotInitialized);
		}
	}

	if (ActiveLegacyMutations.Contains(LegacyInventory))
	{
		return MakeLegacyResult(EInventoryOperationResult::Busy);
	}
	// The legacy class-array protocol has neither quantities nor a preflight capacity contract.
	// Refuse multi-unit writes instead of partially emulating a stacked transaction.
	if (Quantity != 1)
	{
		return MakeLegacyResult(EInventoryOperationResult::NotInitialized);
	}

	if (!IsDefinitionUsable(DefinitionClass))
	{
		return MakeLegacyResult(EInventoryOperationResult::InvalidDefinition);
	}

	if (!IsDefinitionDataValid(DefinitionClass))
	{
		return MakeLegacyResult(EInventoryOperationResult::InvalidDefinitionData);
	}

	FArrayProperty* ArrayProperty = nullptr;
	FClassProperty* ClassProperty = nullptr;
	if (!FindLegacyArray(LegacyInventory, ArrayProperty, ClassProperty)
		|| !DefinitionClass->IsChildOf(ClassProperty->MetaClass))
	{
		return MakeLegacyResult(EInventoryOperationResult::NotInitialized);
	}

	ActiveLegacyMutations.Add(LegacyInventory);
	ON_SCOPE_EXIT
	{
		ActiveLegacyMutations.Remove(LegacyInventory);
	};

	FScriptArrayHelper Items(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(LegacyInventory));
	const int32 NewIndex = Items.AddValue();
	ClassProperty->SetObjectPropertyValue(Items.GetRawPtr(NewIndex), DefinitionClass.Get());
	return MakeLegacyResult(EInventoryOperationResult::Success);
}

TArray<TSubclassOf<UItemDefinition>> ULegacyInventoryCompatibilityLibrary::GetLegacyItemsSnapshot(
	UActorComponent* LegacyInventory,
	EInventoryOperationResult& Result)
{
	TArray<TSubclassOf<UItemDefinition>> Snapshot;
	if (!IsLegacyReceiverUsable(LegacyInventory))
	{
		Result = EInventoryOperationResult::NotInitialized;
		return Snapshot;
	}

	if (UInventoryComponent* NativeInventory = Cast<UInventoryComponent>(LegacyInventory))
	{
		if (!NativeInventory->IsLegacyInventoryMode())
		{
			if (NativeInventory->GetInitializationState() != EInventoryInitializationState::NativeReady
				|| !NativeInventory->IsNativeAuthorityActive())
			{
				Result = EInventoryOperationResult::NotInitialized;
				return Snapshot;
			}
			for (const UItemInstance* Item : NativeInventory->GetItemsSnapshot())
			{
				Snapshot.Add(Item->GetDefinitionClass());
			}
			Result = EInventoryOperationResult::Success;
			return Snapshot;
		}
		if (NativeInventory->ShouldBlockLegacyWrites())
		{
			Result = EInventoryOperationResult::NotInitialized;
			return Snapshot;
		}
	}

	FArrayProperty* ArrayProperty = nullptr;
	FClassProperty* ClassProperty = nullptr;
	if (!FindLegacyArray(LegacyInventory, ArrayProperty, ClassProperty))
	{
		Result = EInventoryOperationResult::NotInitialized;
		return Snapshot;
	}

	FScriptArrayHelper Items(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(LegacyInventory));
	Snapshot.Reserve(Items.Num());
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		Snapshot.Add(Cast<UClass>(ClassProperty->GetObjectPropertyValue(Items.GetRawPtr(Index))));
	}

	Result = EInventoryOperationResult::Success;
	return Snapshot;
}
