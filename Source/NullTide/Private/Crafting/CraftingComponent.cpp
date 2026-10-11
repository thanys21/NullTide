// Fill out your copyright notice in the Description page of Project Settings.


#include "Crafting/CraftingComponent.h"

#include "Crafting/CraftingRecipeDefinition.h"
#include "Crafting/CraftingStationComponent.h"
#include "Crafting/CraftingStationDefinition.h"
#include "Inventory/InventoryComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
FCraftingTransactionResult MakeCraftingRequestResult(const ECraftingTransactionResult Result, const FString& Diagnostic)
{
	FCraftingTransactionResult RequestResult;
	RequestResult.Result = Result;
	RequestResult.Diagnostic = Diagnostic;
	return RequestResult;
}

/** Returns the recipe default object when the class is concrete and its data is valid. */
const UCraftingRecipeDefinition* ResolveValidRecipe(const TSubclassOf<UCraftingRecipeDefinition> RecipeClass, FString& OutDiagnostic)
{
	const UClass* Class = RecipeClass.Get();
	if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		OutDiagnostic = TEXT("Recipe class is null or abstract.");
		return nullptr;
	}
	const UCraftingRecipeDefinition* Recipe = Class->GetDefaultObject<UCraftingRecipeDefinition>();
	if (!IsValid(Recipe) || !Recipe->ValidateRecipe(OutDiagnostic))
	{
		return nullptr;
	}
	return Recipe;
}
}

UCraftingComponent::UCraftingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

TArray<TSubclassOf<UCraftingRecipeDefinition>> UCraftingComponent::GetAvailableRecipes() const
{
	TArray<TSubclassOf<UCraftingRecipeDefinition>> Recipes;
	for (const TSubclassOf<UCraftingRecipeDefinition>& RecipeClass : AvailableRecipes)
	{
		FString Diagnostic;
		if (!Recipes.Contains(RecipeClass) && ResolveValidRecipe(RecipeClass, Diagnostic))
		{
			Recipes.Add(RecipeClass);
		}
	}
	return Recipes;
}

bool UCraftingComponent::IsRecipeAvailable(TSubclassOf<UCraftingRecipeDefinition> RecipeClass) const
{
	FString Diagnostic;
	return RecipeClass && AvailableRecipes.Contains(RecipeClass) && ResolveValidRecipe(RecipeClass, Diagnostic);
}

UInventoryComponent* UCraftingComponent::GetInventory() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
}

TArray<TSubclassOf<UCraftingRecipeDefinition>> UCraftingComponent::GetRecipesForStation(UCraftingStationComponent* Station) const
{
	const TArray<TSubclassOf<UCraftingRecipeDefinition>> Registered = GetAvailableRecipes();
	const UClass* StationClass = IsValid(Station) && Station->IsStationUsable() ? Station->GetStationDefinition().Get() : nullptr;
	TArray<TSubclassOf<UCraftingRecipeDefinition>> Recipes;
	if (StationClass)
	{
		for (const TSubclassOf<UCraftingRecipeDefinition>& RecipeClass : Registered)
		{
			if (RecipeClass->GetDefaultObject<UCraftingRecipeDefinition>()->RequiredStation.Get() == StationClass)
			{
				Recipes.Add(RecipeClass);
			}
		}
	}
	for (const TSubclassOf<UCraftingRecipeDefinition>& RecipeClass : Registered)
	{
		if (!RecipeClass->GetDefaultObject<UCraftingRecipeDefinition>()->RequiredStation)
		{
			Recipes.Add(RecipeClass);
		}
	}
	return Recipes;
}

UCraftingStationComponent* UCraftingComponent::GetActiveCraftStation() const
{
	UCraftingStationComponent* Station = ActiveCraftStation.Get();
	return IsValid(Station) ? Station : nullptr;
}

bool UCraftingComponent::ValidateStationAccess(
	const UCraftingRecipeDefinition& Recipe,
	const UCraftingStationComponent* Station,
	FCraftingTransactionResult& OutFailure) const
{
	const UClass* RequiredClass = Recipe.RequiredStation.Get();
	if (!RequiredClass)
	{
		return true;
	}
	if (!IsValid(Station) || !Station->IsStationUsable())
	{
		OutFailure = MakeCraftingRequestResult(ECraftingTransactionResult::StationRequired,
			FString::Printf(TEXT("%s requires station %s; no usable station was given."),
				*GetNameSafe(Recipe.GetClass()), *GetNameSafe(RequiredClass)));
		return false;
	}
	if (Station->GetStationDefinition().Get() != RequiredClass)
	{
		OutFailure = MakeCraftingRequestResult(ECraftingTransactionResult::StationRequired,
			FString::Printf(TEXT("%s requires station %s, but the given station is %s."),
				*GetNameSafe(Recipe.GetClass()), *GetNameSafe(RequiredClass), *GetNameSafe(Station->GetStationDefinition().Get())));
		return false;
	}
	if (!Station->IsActorInUseRange(GetOwner()))
	{
		OutFailure = MakeCraftingRequestResult(ECraftingTransactionResult::StationOutOfRange,
			FString::Printf(TEXT("The crafter is outside the use range of station %s."), *GetNameSafe(RequiredClass)));
		return false;
	}
	return true;
}

bool UCraftingComponent::ValidateRequest(
	TSubclassOf<UCraftingRecipeDefinition> RecipeClass,
	const UCraftingStationComponent* Station,
	const UCraftingRecipeDefinition*& OutRecipe,
	UInventoryComponent*& OutInventory,
	FCraftingTransactionResult& OutFailure) const
{
	FString Diagnostic;
	OutRecipe = ResolveValidRecipe(RecipeClass, Diagnostic);
	if (!OutRecipe)
	{
		OutFailure = MakeCraftingRequestResult(ECraftingTransactionResult::InvalidRecipe, Diagnostic);
		return false;
	}
	if (!AvailableRecipes.Contains(RecipeClass))
	{
		OutFailure = MakeCraftingRequestResult(ECraftingTransactionResult::RecipeNotAvailable,
			FString::Printf(TEXT("%s is not registered on this crafting component."), *GetNameSafe(RecipeClass.Get())));
		return false;
	}
	if (!ValidateStationAccess(*OutRecipe, Station, OutFailure))
	{
		return false;
	}
	OutInventory = GetInventory();
	if (!IsValid(OutInventory))
	{
		OutFailure = MakeCraftingRequestResult(ECraftingTransactionResult::InvalidInventory,
			TEXT("The crafting component's owner has no inventory component."));
		return false;
	}
	return true;
}

FCraftingTransactionResult UCraftingComponent::CanCraftRecipe(TSubclassOf<UCraftingRecipeDefinition> RecipeClass) const
{
	return CanCraftRecipeAtStation(RecipeClass, nullptr);
}

FCraftingTransactionResult UCraftingComponent::CanCraftRecipeAtStation(TSubclassOf<UCraftingRecipeDefinition> RecipeClass,
	UCraftingStationComponent* Station) const
{
	const UCraftingRecipeDefinition* Recipe = nullptr;
	UInventoryComponent* Inventory = nullptr;
	FCraftingTransactionResult Failure;
	if (!ValidateRequest(RecipeClass, Station, Recipe, Inventory, Failure))
	{
		return Failure;
	}
	if (IsCrafting())
	{
		return MakeCraftingRequestResult(ECraftingTransactionResult::CraftInProgress,
			TEXT("A timed craft is already in progress."));
	}
	return FCraftingTransaction::CanCraftRecipe(Inventory, Recipe);
}

FCraftingTransactionResult UCraftingComponent::TryCraftRecipe(TSubclassOf<UCraftingRecipeDefinition> RecipeClass)
{
	// Reentrant calls (from inventory or crafting notifications of this craft) neither mutate nor notify.
	if (bCraftInProgress)
	{
		return MakeCraftingRequestResult(ECraftingTransactionResult::CraftInProgress,
			TEXT("A craft is already in progress on this crafting component."));
	}
	TGuardValue<bool> CraftGuard(bCraftInProgress, true);

	const UCraftingRecipeDefinition* Recipe = nullptr;
	UInventoryComponent* Inventory = nullptr;
	FCraftingTransactionResult Result;
	if (ValidateRequest(RecipeClass, nullptr, Recipe, Inventory, Result))
	{
		if (IsCrafting())
		{
			Result = MakeCraftingRequestResult(ECraftingTransactionResult::CraftInProgress,
				TEXT("A timed craft is already in progress."));
		}
		else if (Recipe->CraftDuration > 0.f)
		{
			Result = MakeCraftingRequestResult(ECraftingTransactionResult::TimedCraftRequired,
				TEXT("This recipe takes time; start it with TryStartCraft."));
		}
		else
		{
			Result = FCraftingTransaction::TryCraftRecipeAtomic(Inventory, Recipe);
		}
	}
	OnCraftingResult.Broadcast(RecipeClass, Result);
	return Result;
}

FCraftingTransactionResult UCraftingComponent::TryStartCraft(TSubclassOf<UCraftingRecipeDefinition> RecipeClass)
{
	return TryStartCraftAtStation(RecipeClass, nullptr);
}

FCraftingTransactionResult UCraftingComponent::TryStartCraftAtStation(TSubclassOf<UCraftingRecipeDefinition> RecipeClass,
	UCraftingStationComponent* Station)
{
	if (bCraftInProgress)
	{
		return MakeCraftingRequestResult(ECraftingTransactionResult::CraftInProgress,
			TEXT("A craft is already in progress on this crafting component."));
	}
	TGuardValue<bool> CraftGuard(bCraftInProgress, true);

	const UCraftingRecipeDefinition* Recipe = nullptr;
	UInventoryComponent* Inventory = nullptr;
	FCraftingTransactionResult Result;
	if (!ValidateRequest(RecipeClass, Station, Recipe, Inventory, Result))
	{
		OnCraftingResult.Broadcast(RecipeClass, Result);
		return Result;
	}
	if (IsCrafting())
	{
		Result = MakeCraftingRequestResult(ECraftingTransactionResult::CraftInProgress,
			TEXT("A timed craft is already in progress."));
		OnCraftingResult.Broadcast(RecipeClass, Result);
		return Result;
	}
	if (Recipe->CraftDuration <= 0.f)
	{
		Result = FCraftingTransaction::TryCraftRecipeAtomic(Inventory, Recipe);
		OnCraftingResult.Broadcast(RecipeClass, Result);
		return Result;
	}

	// Timed: exact preflight only. Nothing is consumed or reserved until completion.
	Result = FCraftingTransaction::CanCraftRecipe(Inventory, Recipe);
	UWorld* World = GetWorld();
	if (Result.IsSuccess() && !World)
	{
		Result = MakeCraftingRequestResult(ECraftingTransactionResult::InventoryRejected,
			TEXT("No world is available to run the craft timer."));
	}
	if (!Result.IsSuccess())
	{
		OnCraftingResult.Broadcast(RecipeClass, Result);
		return Result;
	}

	ActiveRecipe = RecipeClass;
	const AActor* Owner = GetOwner();
	ActiveCraftStartLocation = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	World->GetTimerManager().SetTimer(CraftTimerHandle, this, &UCraftingComponent::HandleCraftTimerCompleted,
		Recipe->CraftDuration, false);
	BindOwnerMovement();
	if (Recipe->RequiredStation)
	{
		// Only station recipes depend on the station's lifetime; ValidateRequest proved it usable.
		BindActiveStation(Station);
	}
	Result = MakeCraftingRequestResult(ECraftingTransactionResult::Started, TEXT("Timed craft started."));
	OnCraftStarted.Broadcast(RecipeClass, Recipe->CraftDuration);
	return Result;
}

bool UCraftingComponent::CancelCraft()
{
	if (!IsCrafting())
	{
		return false;
	}
	CancelActiveCraft(ECraftCancelReason::Explicit);
	return true;
}

void UCraftingComponent::CancelActiveCraft(const ECraftCancelReason Reason)
{
	const TSubclassOf<UCraftingRecipeDefinition> Recipe = ActiveRecipe;
	ClearActiveCraft();
	// Listeners cannot start another craft from inside the cancellation notification.
	TGuardValue<bool> CraftGuard(bCraftInProgress, true);
	OnCraftCancelled.Broadcast(Recipe, Reason);
}

void UCraftingComponent::HandleCraftTimerCompleted()
{
	const TSubclassOf<UCraftingRecipeDefinition> RecipeClass = ActiveRecipe;
	const TWeakObjectPtr<UCraftingStationComponent> Station = ActiveCraftStation;
	// Clear the active state and bindings before any feedback, so listeners see an idle component.
	ClearActiveCraft();
	TGuardValue<bool> CraftGuard(bCraftInProgress, true);

	// Re-validate: registration, station access and inventory may have changed while the timer ran.
	// A station that is gone resolves to null here and fails as StationRequired.
	const UCraftingRecipeDefinition* Recipe = nullptr;
	UInventoryComponent* Inventory = nullptr;
	FCraftingTransactionResult Result;
	if (ValidateRequest(RecipeClass, Station.Get(), Recipe, Inventory, Result))
	{
		Result = FCraftingTransaction::TryCraftRecipeAtomic(Inventory, Recipe);
	}
	OnCraftingResult.Broadcast(RecipeClass, Result);
}

float UCraftingComponent::GetActiveCraftProgress() const
{
	const UWorld* World = GetWorld();
	if (!IsCrafting() || !World)
	{
		return 0.f;
	}
	const FTimerManager& Timers = World->GetTimerManager();
	const float Rate = Timers.GetTimerRate(CraftTimerHandle);
	return Rate > 0.f ? FMath::Clamp(Timers.GetTimerElapsed(CraftTimerHandle) / Rate, 0.f, 1.f) : 0.f;
}

float UCraftingComponent::GetActiveCraftRemainingSeconds() const
{
	const UWorld* World = GetWorld();
	return IsCrafting() && World ? FMath::Max(0.f, World->GetTimerManager().GetTimerRemaining(CraftTimerHandle)) : 0.f;
}

void UCraftingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearActiveCraft();
	Super::EndPlay(EndPlayReason);
}

void UCraftingComponent::ClearActiveCraft()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CraftTimerHandle);
	}
	CraftTimerHandle.Invalidate();
	UnbindOwnerMovement();
	UnbindActiveStation();
	ActiveRecipe = nullptr;
}

void UCraftingComponent::BindActiveStation(UCraftingStationComponent* Station)
{
	UnbindActiveStation();
	ActiveCraftStation = Station;
	AActor* StationActor = IsValid(Station) ? Station->GetOwner() : nullptr;
	if (IsValid(StationActor))
	{
		BoundStationActor = StationActor;
		StationActor->OnEndPlay.AddUniqueDynamic(this, &UCraftingComponent::HandleActiveStationEndPlay);
	}
}

void UCraftingComponent::UnbindActiveStation()
{
	// The station actor may already be marked for destruction; unbind from it anyway.
	if (AActor* StationActor = BoundStationActor.Get(/*bEvenIfPendingKill*/ true))
	{
		StationActor->OnEndPlay.RemoveDynamic(this, &UCraftingComponent::HandleActiveStationEndPlay);
	}
	BoundStationActor.Reset();
	ActiveCraftStation.Reset();
}

void UCraftingComponent::HandleActiveStationEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	if (!IsCrafting() || Actor != BoundStationActor.Get(/*bEvenIfPendingKill*/ true))
	{
		return;
	}
	if (EndPlayReason == EEndPlayReason::Destroyed || EndPlayReason == EEndPlayReason::RemovedFromWorld)
	{
		CancelActiveCraft(ECraftCancelReason::StationLost);
	}
	else
	{
		// The whole world is ending (PIE end, quit, level transition): clear silently, as the crafter's own EndPlay does.
		ClearActiveCraft();
	}
}

void UCraftingComponent::BindOwnerMovement()
{
	UnbindOwnerMovement();
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	BoundCharacter = Character;
	Character->OnCharacterMovementUpdated.AddUniqueDynamic(this, &UCraftingComponent::HandleOwnerMovementUpdated);
	Character->MovementModeChangedDelegate.AddUniqueDynamic(this, &UCraftingComponent::HandleOwnerMovementModeChanged);
}

void UCraftingComponent::UnbindOwnerMovement()
{
	if (ACharacter* Character = BoundCharacter.Get())
	{
		Character->OnCharacterMovementUpdated.RemoveDynamic(this, &UCraftingComponent::HandleOwnerMovementUpdated);
		Character->MovementModeChangedDelegate.RemoveDynamic(this, &UCraftingComponent::HandleOwnerMovementModeChanged);
	}
	BoundCharacter.Reset();
}

void UCraftingComponent::HandleOwnerMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	const ACharacter* Character = BoundCharacter.Get();
	if (!IsCrafting() || !Character)
	{
		return;
	}
	// Any displacement source counts: WASD, click-to-move path following, or being pushed.
	if (FVector::DistSquared2D(Character->GetActorLocation(), ActiveCraftStartLocation) > FMath::Square(MovementCancelTolerance))
	{
		CancelActiveCraft(ECraftCancelReason::Movement);
	}
}

void UCraftingComponent::HandleOwnerMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode,
	uint8 PreviousCustomMode)
{
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (IsCrafting() && Movement && Movement->MovementMode == MOVE_Falling)
	{
		CancelActiveCraft(ECraftCancelReason::Jump);
	}
}

#if WITH_EDITOR
EDataValidationResult UCraftingComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	for (int32 Index = 0; Index < AvailableRecipes.Num(); ++Index)
	{
		FString Diagnostic;
		if (!ResolveValidRecipe(AvailableRecipes[Index], Diagnostic))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("AvailableRecipes[%d]: %s"), Index, *Diagnostic)));
			Result = EDataValidationResult::Invalid;
		}
		else if (AvailableRecipes.IndexOfByKey(AvailableRecipes[Index]) != Index)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("AvailableRecipes[%d] duplicates %s."), Index,
				*GetNameSafe(AvailableRecipes[Index].Get()))));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif
