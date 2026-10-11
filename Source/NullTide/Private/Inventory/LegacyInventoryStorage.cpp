#include "Inventory/LegacyInventoryStorage.h"

#include "Items/ItemDefinition.h"
#include "Components/ActorComponent.h"
#include "UObject/UnrealType.h"

bool LegacyInventoryStorage::FindArray(
	const UActorComponent* Inventory, FArrayProperty*& ArrayProperty, FClassProperty*& ClassProperty)
{
	ArrayProperty = FindFProperty<FArrayProperty>(Inventory->GetClass(), TEXT("InventoryItems"));
	ClassProperty = ArrayProperty ? CastField<FClassProperty>(ArrayProperty->Inner) : nullptr;
	return ClassProperty && ClassProperty->MetaClass
		&& ClassProperty->MetaClass->IsChildOf(UItemDefinition::StaticClass());
}

TArray<TSubclassOf<UItemDefinition>> LegacyInventoryStorage::ReadArray(
	UActorComponent* Inventory, FArrayProperty* ArrayProperty, FClassProperty* ClassProperty)
{
	FScriptArrayHelper Array(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Inventory));
	TArray<TSubclassOf<UItemDefinition>> Definitions;
	Definitions.Reserve(Array.Num());
	for (int32 Index = 0; Index < Array.Num(); ++Index)
	{
		Definitions.Add(Cast<UClass>(ClassProperty->GetObjectPropertyValue(Array.GetRawPtr(Index))));
	}
	return Definitions;
}

void LegacyInventoryStorage::WriteArray(
	UActorComponent* Inventory, FArrayProperty* ArrayProperty, FClassProperty* ClassProperty,
	const TArray<TSubclassOf<UItemDefinition>>& Definitions)
{
	FScriptArrayHelper Array(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Inventory));
	Array.EmptyAndAddValues(Definitions.Num());
	for (int32 Index = 0; Index < Definitions.Num(); ++Index)
	{
		ClassProperty->SetObjectPropertyValue(Array.GetRawPtr(Index), Definitions[Index].Get());
	}
}
