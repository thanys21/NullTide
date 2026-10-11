#pragma once

#include "CoreMinimal.h"

class UActorComponent;
class UItemDefinition;
class FArrayProperty;
class FClassProperty;

/** Internal reflection adapter; reading is restricted to pre-cutover seeds/rollback mode. */
namespace LegacyInventoryStorage
{
bool FindArray(const UActorComponent* Inventory, FArrayProperty*& ArrayProperty, FClassProperty*& ClassProperty);
TArray<TSubclassOf<UItemDefinition>> ReadArray(UActorComponent* Inventory, FArrayProperty* ArrayProperty, FClassProperty* ClassProperty);
void WriteArray(UActorComponent* Inventory, FArrayProperty* ArrayProperty, FClassProperty* ClassProperty,
	const TArray<TSubclassOf<UItemDefinition>>& Definitions);
}
