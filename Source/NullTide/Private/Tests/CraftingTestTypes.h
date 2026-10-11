// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Crafting/CraftingRecipeDefinition.h"
#include "CraftingTestTypes.generated.h"

/** Concrete valid recipe used only by native crafting automation tests. */
UCLASS(Transient, NotBlueprintable)
class UCraftingTestRecipeDefinition : public UCraftingRecipeDefinition
{
	GENERATED_BODY()

public:
	UCraftingTestRecipeDefinition();
};
