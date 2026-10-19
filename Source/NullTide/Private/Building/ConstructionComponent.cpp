#include "Building/ConstructionComponent.h"

#include "Building/BuildingDefinition.h"
#include "Building/BuildingPieceActor.h"
#include "Building/BuildingPieceComponent.h"
#include "Building/BuildingSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"
#include "Storage/StorageComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogConstruction, Log, All);

namespace
{
/** Tolerances for "the piece is exactly at its resolved transform". */
constexpr double LocationTolerance = 0.1;
constexpr double RotationToleranceDegrees = 0.01;

FBuildingConstructionResult MakeConstructionResult(const EBuildResult Code, const FBuildingPlacement& Placement, const FString& Diagnostic)
{
	FBuildingConstructionResult Result;
	Result.Result = Code;
	Result.Placement = Placement;
	Result.Diagnostic = Diagnostic;
	return Result;
}

bool MatchesTransform(const FTransform& Actual, const FTransform& Expected)
{
	return Actual.GetLocation().Equals(Expected.GetLocation(), LocationTolerance)
		&& Actual.GetRotation().AngularDistance(Expected.GetRotation()) <= FMath::DegreesToRadians(RotationToleranceDegrees)
		&& Actual.GetScale3D().Equals(Expected.GetScale3D(), KINDA_SMALL_NUMBER);
}

bool SameKeySet(const TArray<FBuildingOccupancyKey>& A, const TArray<FBuildingOccupancyKey>& B)
{
	if (A.Num() != B.Num())
	{
		return false;
	}
	const TSet<FBuildingOccupancyKey> SetA(A);
	if (SetA.Num() != A.Num())
	{
		return false;
	}
	for (const FBuildingOccupancyKey& Key : B)
	{
		if (!SetA.Contains(Key))
		{
			return false;
		}
	}
	return true;
}

FString InventoryResultName(const EInventoryOperationResult Result)
{
	return StaticEnum<EInventoryOperationResult>()->GetNameStringByValue(static_cast<int64>(Result));
}

/** Valid building data whose piece class construction can spawn (it relies on ABuildingPieceActor::SetProvisional). */
const UBuildingDefinition* ResolveConstructibleBuilding(const TSubclassOf<UBuildingDefinition> DefinitionClass, FString& OutDiagnostic)
{
	const UBuildingDefinition* Definition = UBuildingDefinition::ResolveValidBuilding(DefinitionClass, OutDiagnostic);
	if (Definition && !Definition->PieceActorClass->IsChildOf(ABuildingPieceActor::StaticClass()))
	{
		// B1 accepts any actor with a piece component; construction needs the provisional handling of the host class.
		OutDiagnostic = FString::Printf(TEXT("PieceActorClass %s must derive from ABuildingPieceActor to be constructed."),
			*GetNameSafe(Definition->PieceActorClass.Get()));
		return nullptr;
	}
	return Definition;
}

/** Maps a failed cost preflight to a construction result code. */
EBuildResult CostPreflightResult(const EInventoryOperationResult Result)
{
	switch (Result)
	{
	case EInventoryOperationResult::InsufficientQuantity:
		return EBuildResult::InsufficientResources;
	case EInventoryOperationResult::NotInitialized:
	case EInventoryOperationResult::Busy:
		return EBuildResult::InvalidInventory;
	default:
		// The cost lines themselves were rejected (item data or quantity).
		return EBuildResult::InvalidDefinition;
	}
}
}

UConstructionComponent::UConstructionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

TArray<TSubclassOf<UBuildingDefinition>> UConstructionComponent::GetAvailableBuildings() const
{
	TArray<TSubclassOf<UBuildingDefinition>> Buildings;
	for (const TSubclassOf<UBuildingDefinition>& DefinitionClass : AvailableBuildings)
	{
		FString Diagnostic;
		if (!Buildings.Contains(DefinitionClass) && ResolveConstructibleBuilding(DefinitionClass, Diagnostic))
		{
			Buildings.Add(DefinitionClass);
		}
	}
	return Buildings;
}

bool UConstructionComponent::IsBuildingAvailable(TSubclassOf<UBuildingDefinition> DefinitionClass) const
{
	FString Diagnostic;
	return DefinitionClass && AvailableBuildings.Contains(DefinitionClass) && ResolveConstructibleBuilding(DefinitionClass, Diagnostic);
}

UInventoryComponent* UConstructionComponent::GetInventory() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
}

bool UConstructionComponent::ValidateRequest(
	TSubclassOf<UBuildingDefinition> DefinitionClass,
	const FBuildingPlacement& Placement,
	const UBuildingDefinition*& OutDefinition,
	UInventoryComponent*& OutInventory,
	UBuildingSubsystem*& OutSubsystem,
	FBuildingPlacementResult& OutResolved,
	FBuildingConstructionResult& OutFailure) const
{
	FString Diagnostic;
	OutDefinition = UBuildingDefinition::ResolveValidBuilding(DefinitionClass, Diagnostic);
	if (!OutDefinition)
	{
		OutFailure = MakeConstructionResult(EBuildResult::InvalidDefinition, Placement, Diagnostic);
		return false;
	}
	if (!AvailableBuildings.Contains(DefinitionClass))
	{
		OutFailure = MakeConstructionResult(EBuildResult::NotAvailable, Placement,
			FString::Printf(TEXT("%s is not registered on this construction component."), *GetNameSafe(DefinitionClass.Get())));
		return false;
	}
	if (!ResolveConstructibleBuilding(DefinitionClass, Diagnostic))
	{
		OutFailure = MakeConstructionResult(EBuildResult::InvalidDefinition, Placement, Diagnostic);
		return false;
	}
	if (bEndPlayed)
	{
		OutFailure = MakeConstructionResult(EBuildResult::InvalidPlacement, Placement, TEXT("The construction component has ended play."));
		return false;
	}
	OutInventory = GetInventory();
	if (!IsValid(OutInventory) || !OutInventory->IsNativeAuthorityActive())
	{
		OutFailure = MakeConstructionResult(EBuildResult::InvalidInventory, Placement,
			IsValid(OutInventory) ? TEXT("The builder's native inventory authority is not active.")
				: TEXT("The construction component's owner has no inventory component."));
		return false;
	}
	const UWorld* World = GetWorld();
	OutSubsystem = World ? World->GetSubsystem<UBuildingSubsystem>() : nullptr;
	if (!OutSubsystem)
	{
		OutFailure = MakeConstructionResult(EBuildResult::InvalidPlacement, Placement, TEXT("No building subsystem in this world."));
		return false;
	}
	// B1 validation re-snaps the request; the UI transform is never trusted.
	OutResolved = OutSubsystem->ValidatePlacement(DefinitionClass, Placement, GetOwner());
	if (!OutResolved.IsSuccess())
	{
		OutFailure = MakeConstructionResult(OutResolved.Result, OutResolved.Placement, OutResolved.Diagnostic);
		return false;
	}
	const FInventoryTransactionResult CostPreflight = OutInventory->CanApplyDefinitionTransaction(OutDefinition->Cost, {});
	if (!CostPreflight.IsSuccess())
	{
		OutFailure = MakeConstructionResult(CostPreflightResult(CostPreflight.Result), OutResolved.Placement,
			FString::Printf(TEXT("The cost cannot be paid (%s, cost line %d)."), *InventoryResultName(CostPreflight.Result), CostPreflight.FailedIndex));
		return false;
	}
	return true;
}

FBuildingConstructionResult UConstructionComponent::CanBuild(TSubclassOf<UBuildingDefinition> DefinitionClass,
	const FBuildingPlacement& Placement) const
{
	if (bBuildInProgress)
	{
		return MakeConstructionResult(EBuildResult::BuildInProgress, Placement, TEXT("A build is already in progress on this construction component."));
	}
	const UBuildingDefinition* Definition = nullptr;
	UInventoryComponent* Inventory = nullptr;
	UBuildingSubsystem* Subsystem = nullptr;
	FBuildingPlacementResult Resolved;
	FBuildingConstructionResult Failure;
	if (!ValidateRequest(DefinitionClass, Placement, Definition, Inventory, Subsystem, Resolved, Failure))
	{
		return Failure;
	}
	return MakeConstructionResult(EBuildResult::Success, Resolved.Placement, FString());
}

FBuildingConstructionResult UConstructionComponent::TryBuild(TSubclassOf<UBuildingDefinition> DefinitionClass,
	const FBuildingPlacement& Placement)
{
	// Step 1. Reentrant calls (from inventory, build-result or engine callbacks of this build) neither mutate nor notify.
	if (bBuildInProgress)
	{
		return MakeConstructionResult(EBuildResult::BuildInProgress, Placement, TEXT("A build is already in progress on this construction component."));
	}
	TGuardValue<bool> BuildGuard(bBuildInProgress, true);
	const FBuildingConstructionResult Result = RunBuild(DefinitionClass, Placement);
	// The result is final: listeners may destroy the piece, which only invalidates the weak Piece.
	OnBuildResult.Broadcast(DefinitionClass, Result);
	return Result;
}

FBuildingConstructionResult UConstructionComponent::RunBuild(TSubclassOf<UBuildingDefinition> DefinitionClass,
	const FBuildingPlacement& Placement)
{
	// Step 2: resolve and validate.
	const UBuildingDefinition* Definition = nullptr;
	UInventoryComponent* Inventory = nullptr;
	UBuildingSubsystem* Subsystem = nullptr;
	FBuildingPlacementResult Resolved;
	FBuildingConstructionResult Failure;
	if (!ValidateRequest(DefinitionClass, Placement, Definition, Inventory, Subsystem, Resolved, Failure))
	{
		return Failure;
	}

	// Step 3: reserve (all-or-nothing; held by this component for this call only).
	const FBuildingReservationHandle Handle = Subsystem->ReserveKeys(this, Resolved.Keys);
	if (!Handle.IsValid())
	{
		return MakeConstructionResult(EBuildResult::Occupied, Resolved.Placement, TEXT("The occupancy keys could not be reserved."));
	}

	// Step 4: spawn the provisional piece.
	ABuildingPieceActor* Piece = SpawnProvisionalPiece(DefinitionClass, Resolved);
	const UBuildingPieceComponent* SpawnedComponent = IsValid(Piece) && !Piece->IsActorBeingDestroyed()
		? Piece->GetBuildingPieceComponent() : nullptr;
	if (!IsValid(SpawnedComponent) || !Piece->IsProvisional() || SpawnedComponent->GetPieceState() != EBuildingPieceState::Provisional)
	{
		RollbackPiece(Piece, Handle, *Subsystem);
		return MakeConstructionResult(EBuildResult::SpawnFailed, Resolved.Placement, TEXT("The provisional piece could not be spawned."));
	}

	// Step 5: revalidate at the final state.
	if (!MatchesTransform(Piece->GetActorTransform(), Resolved.Placement.WorldTransform))
	{
		RollbackPiece(Piece, Handle, *Subsystem);
		return MakeConstructionResult(EBuildResult::TransformMismatch, Resolved.Placement,
			TEXT("The spawned piece is not at the resolved transform."));
	}
	const FBuildingPlacementResult Revalidated = Subsystem->ValidatePlacement(DefinitionClass, Placement, GetOwner(), Handle, Piece);
	if (!Revalidated.IsSuccess())
	{
		RollbackPiece(Piece, Handle, *Subsystem);
		return MakeConstructionResult(Revalidated.Result, Revalidated.Placement, Revalidated.Diagnostic);
	}
	if (!SameKeySet(Revalidated.Keys, Resolved.Keys) || Revalidated.Placement.Layer != Resolved.Placement.Layer
		|| !MatchesTransform(Revalidated.Placement.WorldTransform, Resolved.Placement.WorldTransform))
	{
		RollbackPiece(Piece, Handle, *Subsystem);
		return MakeConstructionResult(EBuildResult::TransformMismatch, Resolved.Placement,
			TEXT("The world changed the resolved keys, layer or transform during spawning."));
	}

	// Step 6: activate the unpaid piece.
	if (!ActivatePiece(*Piece, Handle))
	{
		RollbackPiece(Piece, Handle, *Subsystem);
		return MakeConstructionResult(EBuildResult::BuildInterrupted, Resolved.Placement,
			TEXT("The occupancy registry refused to commit the reservation to the piece."));
	}

	// Step 7: engine callbacks fired by activation may have changed the piece, the registry or the builder.
	FString IntegrityDiagnostic;
	if (!IsActivatedPieceIntact(Piece, Resolved, *Subsystem, Inventory, IntegrityDiagnostic))
	{
		RollbackPiece(Piece, Handle, *Subsystem);
		return MakeConstructionResult(EBuildResult::BuildInterrupted, Resolved.Placement, IntegrityDiagnostic);
	}

	// Step 8: pay. The successful return of this one atomic transaction is the commit point.
	const FInventoryTransactionResult Payment = Inventory->TryApplyDefinitionTransaction(Definition->Cost, {});
	if (!Payment.IsSuccess())
	{
		RollbackPiece(Piece, Handle, *Subsystem);
		return MakeConstructionResult(EBuildResult::ResourcesRejectedRolledBack, Resolved.Placement,
			FString::Printf(TEXT("The cost transaction failed (%s); the piece was rolled back."), *InventoryResultName(Payment.Result)));
	}

	// Step 9: nothing fallible runs after payment. Built once, after the transaction (and its broadcast) returned.
	FBuildingConstructionResult Result = MakeConstructionResult(EBuildResult::Success, Resolved.Placement, FString());
	Result.Piece = Piece;
	if (!IsValid(Piece) || Piece->IsActorBeingDestroyed())
	{
		Result.Diagnostic = TEXT("The piece was destroyed during payment.");
		UE_LOG(LogConstruction, Warning, TEXT("%s: %s was paid for and then destroyed by an inventory listener during payment (no refund)."),
			*GetNameSafe(GetOwner()), *GetNameSafe(DefinitionClass.Get()));
	}
	return Result;
}

ABuildingPieceActor* UConstructionComponent::SpawnProvisionalPiece(TSubclassOf<UBuildingDefinition> DefinitionClass,
	const FBuildingPlacementResult& Resolved)
{
	UWorld* World = GetWorld();
	const UBuildingDefinition* Definition = DefinitionClass ? DefinitionClass->GetDefaultObject<UBuildingDefinition>() : nullptr;
	UClass* PieceClass = Definition ? Definition->PieceActorClass.Get() : nullptr;
	if (!World || !PieceClass || !PieceClass->IsChildOf(ABuildingPieceActor::StaticClass()))
	{
		return nullptr;
	}
	AActor* Owner = GetOwner();
	const FTransform& Transform = Resolved.Placement.WorldTransform;
	// AlwaysSpawn: the engine never moves a piece off the grid; step 5 validates collision itself.
	ABuildingPieceActor* Piece = World->SpawnActorDeferred<ABuildingPieceActor>(PieceClass, Transform, Owner,
		Cast<APawn>(Owner), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Piece)
	{
		return nullptr;
	}
	UBuildingPieceComponent* PieceComponent = Piece->GetBuildingPieceComponent();
	const bool bInitialized = PieceComponent && PieceComponent->InitializePiece(DefinitionClass, Resolved.Placement, Resolved.Keys,
		EBuildingPieceState::Provisional, true);
	// Provisional before FinishSpawning: hidden, no collision and no navigation from the first frame of its life.
	Piece->SetProvisional(true);
	Piece->FinishSpawning(Transform);
	if (!bInitialized)
	{
		Piece->Destroy();
		return nullptr;
	}
	return Piece;
}

bool UConstructionComponent::ActivatePiece(ABuildingPieceActor& Piece, const FBuildingReservationHandle& Handle)
{
	UBuildingPieceComponent* PieceComponent = Piece.GetBuildingPieceComponent();
	UBuildingSubsystem* Subsystem = UBuildingSubsystem::Get(this);
	if (!IsValid(PieceComponent) || !Subsystem)
	{
		return false;
	}
	PieceComponent->SetPieceState(EBuildingPieceState::Active);
	if (!Subsystem->CommitReservation(Handle, this, PieceComponent))
	{
		return false;
	}
	Piece.SetProvisional(false);
	return true;
}

bool UConstructionComponent::IsActivatedPieceIntact(const ABuildingPieceActor* Piece, const FBuildingPlacementResult& Resolved,
	const UBuildingSubsystem& Subsystem, const UInventoryComponent* Inventory, FString& OutDiagnostic) const
{
	if (!IsValid(Piece) || Piece->IsActorBeingDestroyed())
	{
		OutDiagnostic = TEXT("The piece was destroyed during activation.");
		return false;
	}
	const UBuildingPieceComponent* PieceComponent = Piece->GetBuildingPieceComponent();
	if (!IsValid(PieceComponent) || PieceComponent->GetPieceState() != EBuildingPieceState::Active)
	{
		OutDiagnostic = TEXT("The piece is no longer Active.");
		return false;
	}
	// IsPieceRegistered alone does not prove who holds each key.
	if (!SameKeySet(PieceComponent->GetOccupancyKeys(), Resolved.Keys))
	{
		OutDiagnostic = TEXT("The piece's occupancy keys changed during activation.");
		return false;
	}
	for (const FBuildingOccupancyKey& Key : Resolved.Keys)
	{
		if (Subsystem.GetOccupyingPiece(Key) != PieceComponent)
		{
			OutDiagnostic = FString::Printf(TEXT("%s is not occupied by the piece."), *Key.ToString());
			return false;
		}
	}
	if (!MatchesTransform(Piece->GetActorTransform(), Resolved.Placement.WorldTransform))
	{
		OutDiagnostic = TEXT("The piece moved during activation.");
		return false;
	}
	if (Piece->IsProvisional() || Piece->IsHidden() || !Piece->GetActorEnableCollision())
	{
		OutDiagnostic = TEXT("The piece is not visible and collidable after activation.");
		return false;
	}
	if (bEndPlayed || !IsValid(this) || !IsValid(GetOwner()))
	{
		OutDiagnostic = TEXT("The builder ended play during activation.");
		return false;
	}
	if (!IsValid(Inventory) || !Inventory->IsNativeAuthorityActive())
	{
		OutDiagnostic = TEXT("The builder's inventory is no longer available.");
		return false;
	}
	return true;
}

void UConstructionComponent::RollbackPiece(ABuildingPieceActor* Piece, const FBuildingReservationHandle& Handle,
	UBuildingSubsystem& Subsystem) const
{
	// Every step is safe to repeat and none touches the inventory.
	UBuildingPieceComponent* PieceComponent = Piece ? Piece->GetBuildingPieceComponent() : nullptr;
	const bool bLive = IsValid(Piece) && !Piece->IsActorBeingDestroyed();
	if (bLive)
	{
		Piece->SetProvisional(true);
		if (PieceComponent)
		{
			PieceComponent->SetPieceState(EBuildingPieceState::Provisional);
		}
	}
	if (PieceComponent)
	{
		Subsystem.UnregisterPiece(PieceComponent);
	}
	if (Handle.IsValid())
	{
		// Works even if this component became garbage: the registry matches the owner by object key.
		Subsystem.ReleaseReservation(Handle, this);
	}
	if (bLive)
	{
		Piece->Destroy();
		if (IsValid(Piece) && !Piece->IsActorBeingDestroyed())
		{
			UE_LOG(LogConstruction, Error, TEXT("%s: rollback could not destroy %s (%s); it is left hidden, non-colliding, unregistered and Provisional."),
				*GetNameSafe(GetOwner()), *GetNameSafe(Piece), *GetNameSafe(PieceComponent ? PieceComponent->GetBuildingDefinition().Get() : nullptr));
		}
	}
}

void UConstructionComponent::BeginPlay()
{
	Super::BeginPlay();
	// A component can begin play again after EndPlay without being destroyed (for example level streaming).
	bEndPlayed = false;
}

// ---- Demolition (B6) ----

namespace
{
/** Adds refund lines to a total, one line per item. */
void AddRefundLines(TArray<FInventoryDefinitionQuantity>& Total, const TArray<FInventoryDefinitionQuantity>& Lines)
{
	for (const FInventoryDefinitionQuantity& Line : Lines)
	{
		FInventoryDefinitionQuantity* Existing = Total.FindByPredicate([&Line](const FInventoryDefinitionQuantity& Entry)
		{
			return Entry.DefinitionClass == Line.DefinitionClass;
		});
		if (Existing)
		{
			Existing->Quantity += Line.Quantity;
		}
		else
		{
			Total.Add(Line);
		}
	}
}

/** A piece demolition may remove: alive, Active, player-built, owned with authority, with valid building data. */
const UBuildingDefinition* ResolveDemolishablePiece(const UBuildingPieceComponent* Piece, FString& OutDiagnostic)
{
	const AActor* Owner = Piece ? Piece->GetOwner() : nullptr;
	if (!IsValid(Owner) || Owner->IsActorBeingDestroyed())
	{
		OutDiagnostic = TEXT("The piece is gone.");
		return nullptr;
	}
	if (!Piece->IsActivePiece())
	{
		OutDiagnostic = FString::Printf(TEXT("%s is not an Active piece."), *Owner->GetName());
		return nullptr;
	}
	if (!Piece->IsPlayerBuilt())
	{
		OutDiagnostic = FString::Printf(TEXT("%s was not built by a player."), *Owner->GetName());
		return nullptr;
	}
	if (!Owner->HasAuthority())
	{
		// Destroy after the refund would fail without authority (UWorld::DestroyActor), so never start.
		OutDiagnostic = FString::Printf(TEXT("%s has no authority here."), *Owner->GetName());
		return nullptr;
	}
	return UBuildingDefinition::ResolveValidBuilding(Piece->GetBuildingDefinition(), OutDiagnostic);
}

EBuildResult RefundFailureResult(const EInventoryOperationResult Result)
{
	return Result == EInventoryOperationResult::NotInitialized || Result == EInventoryOperationResult::Busy
		? EBuildResult::InvalidInventory : EBuildResult::RefundDoesNotFit;
}
}

TArray<FInventoryDefinitionQuantity> UConstructionComponent::ComputeRefund(const UBuildingDefinition& Definition)
{
	TArray<FInventoryDefinitionQuantity> Refund;
	const double Fraction = FMath::Clamp(static_cast<double>(Definition.RefundFraction), 0.0, 1.0);
	for (const FInventoryDefinitionQuantity& Line : Definition.Cost)
	{
		// The epsilon keeps exact products (6 x 0.5) from rounding down through float error.
		const int32 Quantity = FMath::FloorToInt(Line.Quantity * Fraction + 1.0e-6);
		if (Line.DefinitionClass && Quantity > 0)
		{
			FInventoryDefinitionQuantity Entry;
			Entry.DefinitionClass = Line.DefinitionClass;
			Entry.Quantity = Quantity;
			AddRefundLines(Refund, { Entry });
		}
	}
	return Refund;
}

bool UConstructionComponent::ValidateDemolish(AActor* Target, TArray<UBuildingPieceComponent*>& OutPieces,
	UInventoryComponent*& OutInventory, FBuildingDemolishResult& OutResult) const
{
	OutPieces.Reset();
	OutInventory = nullptr;
	OutResult = FBuildingDemolishResult();
	auto Fail = [&OutResult](const EBuildResult Code, const FString& Diagnostic)
	{
		OutResult.Result = Code;
		OutResult.Diagnostic = Diagnostic;
		return false;
	};
	if (bEndPlayed)
	{
		return Fail(EBuildResult::NotDemolishable, TEXT("The construction component has ended play."));
	}
	UBuildingPieceComponent* TargetPiece = IsValid(Target) ? Target->FindComponentByClass<UBuildingPieceComponent>() : nullptr;
	if (!TargetPiece)
	{
		return Fail(EBuildResult::NotDemolishable, FString::Printf(TEXT("%s is not a building piece."), *GetNameSafe(Target)));
	}
	OutResult.Definition = TargetPiece->GetBuildingDefinition();
	OutResult.Pieces.Add(Target);
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return Fail(EBuildResult::NotDemolishable, TEXT("The builder has no authority here."));
	}
	FString Diagnostic;
	const UBuildingDefinition* TargetDefinition = ResolveDemolishablePiece(TargetPiece, Diagnostic);
	if (!TargetDefinition)
	{
		return Fail(EBuildResult::NotDemolishable, Diagnostic);
	}
	// The target's refund is known before any later refusal, so a refused preview can still show it.
	AddRefundLines(OutResult.Refund, ComputeRefund(*TargetDefinition));
	const double Range = TargetDefinition->BuildRangeOverride > 0.f ? TargetDefinition->BuildRangeOverride : UBuildingSubsystem::DefaultBuildRange;
	if (FVector::DistSquared2D(Owner->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(Range))
	{
		return Fail(EBuildResult::OutOfRange, FString::Printf(TEXT("The piece is farther than %.0f cm."), Range));
	}

	// The cascade (B6-2, B7-4): a floor takes the walls, doors and furniture that would be left without a floor.
	OutPieces.Add(TargetPiece);
	if (TargetDefinition->Category == EBuildingCategory::Foundation)
	{
		const UWorld* World = GetWorld();
		const UBuildingSubsystem* Subsystem = World ? World->GetSubsystem<UBuildingSubsystem>() : nullptr;
		if (!Subsystem)
		{
			return Fail(EBuildResult::InvalidPlacement, TEXT("No building subsystem in this world."));
		}
		for (UBuildingPieceComponent* Dependent : Subsystem->FindDependentPieces(*TargetPiece))
		{
			if (Dependent && !OutPieces.Contains(Dependent))
			{
				OutPieces.Add(Dependent);
				OutResult.Pieces.Add(Dependent->GetOwner());
			}
		}
	}

	// A storage that holds items is never demolished, alone or in a cascade; its contents must never be lost (B7-2).
	for (const UBuildingPieceComponent* Piece : OutPieces)
	{
		const UStorageComponent* Storage = Piece->GetOwner() ? Piece->GetOwner()->FindComponentByClass<UStorageComponent>() : nullptr;
		if (Storage && Storage->GetItemCount() > 0)
		{
			return Fail(EBuildResult::StorageNotEmpty, FString::Printf(TEXT("%s still holds %d item stack(s)."), *Piece->GetOwner()->GetName(),
				Storage->GetItemCount()));
		}
	}
	for (int32 Index = 1; Index < OutPieces.Num(); ++Index)
	{
		const UBuildingDefinition* DependentDefinition = ResolveDemolishablePiece(OutPieces[Index], Diagnostic);
		if (!DependentDefinition)
		{
			return Fail(EBuildResult::NotDemolishable, FString::Printf(TEXT("A piece standing on this floor cannot go with it: %s"), *Diagnostic));
		}
		AddRefundLines(OutResult.Refund, ComputeRefund(*DependentDefinition));
	}

	OutInventory = GetInventory();
	if (!IsValid(OutInventory) || !OutInventory->IsNativeAuthorityActive())
	{
		return Fail(EBuildResult::InvalidInventory, IsValid(OutInventory) ? TEXT("The builder's native inventory authority is not active.")
			: TEXT("The construction component's owner has no inventory component."));
	}
	if (!OutResult.Refund.IsEmpty())
	{
		const FInventoryTransactionResult Preflight = OutInventory->CanApplyDefinitionTransaction({}, OutResult.Refund);
		if (!Preflight.IsSuccess())
		{
			return Fail(RefundFailureResult(Preflight.Result), FString::Printf(TEXT("The refund does not fit (%s, refund line %d)."),
				*InventoryResultName(Preflight.Result), Preflight.FailedIndex));
		}
	}
	return true;
}

FBuildingDemolishResult UConstructionComponent::CanDemolish(AActor* Piece) const
{
	FBuildingDemolishResult Result;
	if (bBuildInProgress)
	{
		Result.Result = EBuildResult::BuildInProgress;
		Result.Diagnostic = TEXT("A build or demolition is already in progress on this construction component.");
		return Result;
	}
	TArray<UBuildingPieceComponent*> Pieces;
	UInventoryComponent* Inventory = nullptr;
	if (ValidateDemolish(Piece, Pieces, Inventory, Result))
	{
		Result.Result = EBuildResult::Success;
	}
	return Result;
}

FBuildingDemolishResult UConstructionComponent::TryDemolish(AActor* Piece)
{
	if (bBuildInProgress)
	{
		FBuildingDemolishResult Result;
		Result.Result = EBuildResult::BuildInProgress;
		Result.Diagnostic = TEXT("A build or demolition is already in progress on this construction component.");
		return Result;
	}
	TGuardValue<bool> BuildGuard(bBuildInProgress, true);
	const FBuildingDemolishResult Result = RunDemolish(Piece);
	OnDemolishResult.Broadcast(Result.Definition, Result);
	return Result;
}

FBuildingDemolishResult UConstructionComponent::RunDemolish(AActor* Target)
{
	FBuildingDemolishResult Result;
	TArray<UBuildingPieceComponent*> ValidatedPieces;
	UInventoryComponent* Inventory = nullptr;
	if (!ValidateDemolish(Target, ValidatedPieces, Inventory, Result))
	{
		return Result;
	}
	// Weak from here: listeners of the refund broadcast may destroy any of the pieces (or only their components).
	TArray<TWeakObjectPtr<UBuildingPieceComponent>> Pieces;
	TArray<TWeakObjectPtr<AActor>> PieceActors;
	for (UBuildingPieceComponent* Piece : ValidatedPieces)
	{
		Piece->SetPieceState(EBuildingPieceState::Removing);
		Pieces.Emplace(Piece);
		PieceActors.Emplace(Piece->GetOwner());
	}
	if (!Result.Refund.IsEmpty())
	{
		const FInventoryTransactionResult Refund = Inventory->TryApplyDefinitionTransaction({}, Result.Refund);
		if (!Refund.IsSuccess())
		{
			// Nothing was refunded: every piece stays as it was.
			for (const TWeakObjectPtr<UBuildingPieceComponent>& Piece : Pieces)
			{
				if (Piece.IsValid() && Piece->GetPieceState() == EBuildingPieceState::Removing)
				{
					Piece->SetPieceState(EBuildingPieceState::Active);
				}
			}
			Result.Result = RefundFailureResult(Refund.Result);
			Result.Diagnostic = FString::Printf(TEXT("The refund was rejected (%s); nothing was removed."), *InventoryResultName(Refund.Result));
			return Result;
		}
	}
	// The refund is the commit point. Destroy cannot fail with authority; dependents go first, then the target.
	for (int32 Index = PieceActors.Num() - 1; Index >= 0; --Index)
	{
		AActor* Actor = PieceActors[Index].Get();
		if (IsValid(Actor) && !Actor->IsActorBeingDestroyed())
		{
			Actor->Destroy();
		}
	}
	Result.Result = EBuildResult::Success;
	return Result;
}

void UConstructionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// A reservation never outlives one TryBuild call, so there is nothing to release here.
	bEndPlayed = true;
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
EDataValidationResult UConstructionComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	for (int32 Index = 0; Index < AvailableBuildings.Num(); ++Index)
	{
		FString Diagnostic;
		if (!ResolveConstructibleBuilding(AvailableBuildings[Index], Diagnostic))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("AvailableBuildings[%d]: %s"), Index, *Diagnostic)));
			Result = EDataValidationResult::Invalid;
		}
		else if (AvailableBuildings.IndexOfByKey(AvailableBuildings[Index]) != Index)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("AvailableBuildings[%d] duplicates %s."), Index,
				*GetNameSafe(AvailableBuildings[Index].Get()))));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif
