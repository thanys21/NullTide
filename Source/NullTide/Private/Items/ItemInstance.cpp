// Fill out your copyright notice in the Description page of Project Settings.

#include "Items/ItemInstance.h"

#include "Items/ItemDefinition.h"

bool UItemInstance::Initialize(
	const FGuid& InInstanceId,
	TSubclassOf<UItemDefinition> InDefinitionClass,
	const int32 InQuantity)
{
	if (InstanceId.IsValid() || DefinitionClass || !InInstanceId.IsValid() || !InDefinitionClass || InQuantity < 1)
	{
		return false;
	}

	InstanceId = InInstanceId;
	DefinitionClass = InDefinitionClass;
	Quantity = InQuantity;
	return true;
}

bool UItemInstance::SetQuantity(const int32 InQuantity)
{
	if (!InstanceId.IsValid() || !DefinitionClass || InQuantity < 1)
	{
		return false;
	}

	Quantity = InQuantity;
	return true;
}
