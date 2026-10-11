// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Crafting/CraftingRecipeDefinition.h"
#include "Crafting/CraftingComponent.h"
#include "Crafting/CraftingStationDefinition.h"
#include "Crafting/CraftingTransaction.h"
#include "Inventory/InventoryTypes.h"
#include "Items/ItemDefinition.h"
#include "CraftingTestTypes.generated.h"

class UCraftingComponent;
class UCraftingPanelWidget;
class UInventoryComponent;

/** Concrete valid recipe used only by native crafting automation tests. */
UCLASS(Transient, NotBlueprintable)
class UCraftingTestRecipeDefinition : public UCraftingRecipeDefinition
{
	GENERATED_BODY()

public:
	UCraftingTestRecipeDefinition();
};

/** Second stackable definition (MaxStackSize 20) used only by crafting transaction tests. */
UCLASS(Transient, NotBlueprintable)
class UCraftingSmallStackTestItemDefinition : public UItemDefinition
{
	GENERATED_BODY()

public:
	UCraftingSmallStackTestItemDefinition();
};

/** Change listener that can attempt a reentrant transaction or craft while the inventory broadcasts. */
UCLASS(Transient, NotBlueprintable)
class UCraftingTransactionTestListener : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleInventoryChanged(int32 NewRevision);

	UPROPERTY()
	TObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY()
	TObjectPtr<UCraftingRecipeDefinition> ReentrantRecipe;

	TArray<FInventoryDefinitionQuantity> ReentrantRemovals;
	TArray<FInventoryDefinitionQuantity> ReentrantAdditions;
	bool bAttemptReentrantTransaction = false;
	FInventoryTransactionResult ReentrantTransactionResult;
	FCraftingTransactionResult ReentrantCraftResult;
	int32 EventCount = 0;
	int32 LastRevision = 0;
	int32 ObservedItemCount = 0;
};

/** Valid recipe whose ingredient and output share a definition: Stackable x10 -> Stackable x5. */
UCLASS(Transient, NotBlueprintable)
class UCraftingOverlapTestRecipe : public UCraftingRecipeDefinition
{
	GENERATED_BODY()

public:
	UCraftingOverlapTestRecipe();
};

/** Valid recipe that needs two new slots: Stackable x1 -> Other x2. */
UCLASS(Transient, NotBlueprintable)
class UCraftingOutputHeavyTestRecipe : public UCraftingRecipeDefinition
{
	GENERATED_BODY()

public:
	UCraftingOutputHeavyTestRecipe();
};

/** Invalid recipe (no outputs) used to test registration filtering. */
UCLASS(Transient, NotBlueprintable)
class UCraftingInvalidTestRecipe : public UCraftingRecipeDefinition
{
	GENERATED_BODY()

public:
	UCraftingInvalidTestRecipe();
};

/** Records inventory and crafting notifications and can attempt reentrant crafts from either. */
UCLASS(Transient, NotBlueprintable)
class UCraftingComponentTestListener : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleInventoryChanged(int32 NewRevision);

	UFUNCTION()
	void HandleCraftingResult(TSubclassOf<UCraftingRecipeDefinition> RecipeClass, const FCraftingTransactionResult& Result);

	UPROPERTY()
	TObjectPtr<UCraftingComponent> Crafting;

	TSubclassOf<UCraftingRecipeDefinition> ReentrantRecipe;
	bool bCraftOnInventoryChanged = false;
	bool bCraftOnCraftingResult = false;
	TArray<FCraftingTransactionResult> ReentrantResults;
	int32 InventoryEvents = 0;
	int32 CraftingEvents = 0;
	TSubclassOf<UCraftingRecipeDefinition> LastRecipe;
	FCraftingTransactionResult LastResult;
};

/** Inventory listener that asks a crafting panel to craft again while a craft is being committed. */
UCLASS(Transient, NotBlueprintable)
class UCraftingPanelTestListener : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleInventoryChanged(int32 NewRevision);

	UFUNCTION()
	void HandleCraftingResult(TSubclassOf<UCraftingRecipeDefinition> RecipeClass, const FCraftingTransactionResult& Result);

	UPROPERTY()
	TObjectPtr<UCraftingPanelWidget> Panel;

	bool bRequestCraftOnInventoryChanged = false;
	int32 InventoryEvents = 0;
	int32 CraftingEvents = 0;
	FCraftingTransactionResult LastResult;
};

/** Valid timed recipe (2 s): Stackable x3 + Single x1 -> Other x1. */
UCLASS(Transient, NotBlueprintable)
class UCraftingTimedTestRecipe : public UCraftingRecipeDefinition
{
	GENERATED_BODY()

public:
	UCraftingTimedTestRecipe();
};

/** Records timed-craft notifications and can attempt nested starts from inside them. */
UCLASS(Transient, NotBlueprintable)
class UCraftingTimedTestListener : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleStarted(TSubclassOf<UCraftingRecipeDefinition> RecipeClass, float Duration);

	UFUNCTION()
	void HandleCancelled(TSubclassOf<UCraftingRecipeDefinition> RecipeClass, ECraftCancelReason Reason);

	UFUNCTION()
	void HandleResult(TSubclassOf<UCraftingRecipeDefinition> RecipeClass, const FCraftingTransactionResult& Result);

	UFUNCTION()
	void HandleInventoryChanged(int32 NewRevision);

	UPROPERTY()
	TObjectPtr<UCraftingComponent> Crafting;

	TSubclassOf<UCraftingRecipeDefinition> NestedRecipe;
	bool bStartFromResult = false;
	bool bStartFromStarted = false;
	TArray<FCraftingTransactionResult> NestedResults;
	int32 StartedEvents = 0;
	int32 CancelledEvents = 0;
	int32 ResultEvents = 0;
	int32 InventoryEvents = 0;
	float LastDuration = 0.f;
	ECraftCancelReason LastCancelReason = ECraftCancelReason::Explicit;
	FCraftingTransactionResult LastResult;
	bool bWasCraftingDuringResult = false;
};

/** Valid timed recipe (1 s) whose outputs need two new slots: Stackable x1 -> Other x2. */
UCLASS(Transient, NotBlueprintable)
class UCraftingTimedHeavyTestRecipe : public UCraftingRecipeDefinition
{
	GENERATED_BODY()

public:
	UCraftingTimedHeavyTestRecipe();
};

/** Valid station kind used only by C8 tests ("Test Bench", default UseRange 300). */
UCLASS(Transient, NotBlueprintable)
class UCraftingTestStationDefinition : public UCraftingStationDefinition
{
	GENERATED_BODY()

public:
	UCraftingTestStationDefinition();
};

/** A second valid station kind, for mismatch tests ("Test Forge"). */
UCLASS(Transient, NotBlueprintable)
class UCraftingOtherTestStationDefinition : public UCraftingStationDefinition
{
	GENERATED_BODY()

public:
	UCraftingOtherTestStationDefinition();
};

/** Invalid station kind: UseRange 0. */
UCLASS(Transient, NotBlueprintable)
class UCraftingInvalidTestStationDefinition : public UCraftingStationDefinition
{
	GENERATED_BODY()

public:
	UCraftingInvalidTestStationDefinition();
};

/** Timed (2 s) Test Bench recipe: Stackable x3 + Single x1 -> Other x1. */
UCLASS(Transient, NotBlueprintable)
class UCraftingStationTimedTestRecipe : public UCraftingRecipeDefinition
{
	GENERATED_BODY()

public:
	UCraftingStationTimedTestRecipe();
};

/** Instant Test Bench recipe: Stackable x2 -> Other x1. */
UCLASS(Transient, NotBlueprintable)
class UCraftingStationInstantTestRecipe : public UCraftingRecipeDefinition
{
	GENERATED_BODY()

public:
	UCraftingStationInstantTestRecipe();
};

/** Instant Test Forge recipe: Stackable x1 -> Other x1. */
UCLASS(Transient, NotBlueprintable)
class UCraftingOtherStationTestRecipe : public UCraftingRecipeDefinition
{
	GENERATED_BODY()

public:
	UCraftingOtherStationTestRecipe();
};

/** Invalid recipe: RequiredStation is the abstract native station root. */
UCLASS(Transient, NotBlueprintable)
class UCraftingAbstractStationTestRecipe : public UCraftingRecipeDefinition
{
	GENERATED_BODY()

public:
	UCraftingAbstractStationTestRecipe();
};

/** Invalid recipe: RequiredStation is an invalid station kind (UseRange 0). */
UCLASS(Transient, NotBlueprintable)
class UCraftingInvalidStationTestRecipe : public UCraftingRecipeDefinition
{
	GENERATED_BODY()

public:
	UCraftingInvalidStationTestRecipe();
};
