// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/InventoryComponent.h"

#include "Inventory/LegacyInventoryStorage.h"
#include "Inventory/InventoryStacking.h"
#include "Storage/StorageComponent.h"
#include "Storage/StorageTransferLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"

#include "Items/Fragments/ItemFragment_WorldPresentation.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"

namespace
{
FInventoryOperationResult MakeInventoryResult(
	EInventoryOperationResult Result,
	UItemInstance* Item = nullptr,
	const FGuid& ItemId = FGuid(),
	const int32 Quantity = 0)
{
	FInventoryOperationResult OperationResult;
	OperationResult.Result = Result;
	OperationResult.Item = Item;
	OperationResult.ItemId = ItemId;
	OperationResult.Quantity = Quantity;
	return OperationResult;
}

FInventoryWorldDropResult MakeWorldDropResult(
	const EInventoryWorldDropResult Result,
	const FGuid& ItemId,
	AActor* SpawnedPickup = nullptr,
	const int32 Quantity = 0)
{
	FInventoryWorldDropResult DropResult;
	DropResult.Result = Result;
	DropResult.ItemId = ItemId;
	DropResult.SpawnedPickup = SpawnedPickup;
	DropResult.Quantity = Quantity;
	return DropResult;
}

struct FWorldDropPlacementProfile
{
	float SurfaceOffset = 32.0f;
	FCollisionShape OccupancyShape = FCollisionShape::MakeBox(FVector(32.0f));
};

bool BuildWorldDropPlacementProfile(const UItemDefinition* Definition, FWorldDropPlacementProfile& OutProfile)
{
	const UItemFragment_WorldPresentation* Presentation = Definition
		? Cast<UItemFragment_WorldPresentation>(Definition->FindFragmentByClass(UItemFragment_WorldPresentation::StaticClass()))
		: nullptr;
	if (!Presentation || !IsValid(Presentation->WorldMesh))
	{
		return false;
	}

	const FBox PresentationBounds = Presentation->WorldMesh->GetBoundingBox().TransformBy(
		FTransform(Presentation->WorldRotationOffset, FVector::ZeroVector, Presentation->WorldScale));
	if (!PresentationBounds.IsValid)
	{
		return false;
	}

	const FVector MeshExtent = PresentationBounds.GetExtent();
	const FVector OccupancyExtent(
		FMath::Max(32.0f, static_cast<float>(MeshExtent.X)),
		FMath::Max(32.0f, static_cast<float>(MeshExtent.Y)),
		FMath::Max(32.0f, static_cast<float>(MeshExtent.Z)));
	OutProfile.SurfaceOffset = OccupancyExtent.Z;
	OutProfile.OccupancyShape = FCollisionShape::MakeBox(OccupancyExtent);
	return true;
}

UClass* GetWorldPickupClass()
{
	static const FSoftClassPath PickupClassPath(
		TEXT("/Game/LevelPrototyping/InventorySystem/BP_PickUpItem.BP_PickUpItem_C"));
	return PickupClassPath.TryLoadClass<AActor>();
}
}

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::BeginPlay()
{
	if (bImportLegacyInventoryOnBeginPlay)
	{
		const FInventoryOperationResult Result = InitializeFromLegacyInventory();
		if (!Result.IsSuccess())
		{
			UE_LOG(LogTemp, Warning, TEXT("Inventory seed import failed for %s (result %d); writes remain blocked."),
				*GetPathName(), static_cast<int32>(Result.Result));
		}
	}
	Super::BeginPlay();
}

EInventoryInitializationState UInventoryComponent::GetInitializationState() const
{
	if (InitializationState == EInventoryInitializationState::NotRequired
		&& (bLegacyInventoryMode || bImportLegacyInventoryOnBeginPlay))
	{
		return EInventoryInitializationState::PendingLegacyImport;
	}
	return InitializationState;
}

FInventoryOperationResult UInventoryComponent::InitializeFromLegacyInventory()
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed | RF_FinishDestroyed))
	{
		return MakeInventoryResult(EInventoryOperationResult::NotInitialized);
	}
	if (bMutationInProgress)
	{
		return MakeInventoryResult(EInventoryOperationResult::Busy);
	}
	if (InitializationState == EInventoryInitializationState::NativeReady)
	{
		return MakeInventoryResult(EInventoryOperationResult::Success);
	}
	if (InitializationState == EInventoryInitializationState::Failed)
	{
		return MakeInventoryResult(LastInitializationResult);
	}
	if (!bLegacyInventoryMode)
	{
		return MakeInventoryResult(EInventoryOperationResult::NotInitialized);
	}

	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	auto FailImport = [this](EInventoryOperationResult Result)
	{
		InitializationState = EInventoryInitializationState::Failed;
		LastInitializationResult = Result;
		return MakeInventoryResult(Result);
	};
	FArrayProperty* ArrayProperty = nullptr;
	FClassProperty* ClassProperty = nullptr;
	if (!Items.IsEmpty() || Revision != 0
		|| !LegacyInventoryStorage::FindArray(this, ArrayProperty, ClassProperty))
	{
		return FailImport(EInventoryOperationResult::NotInitialized);
	}
	const TArray<TSubclassOf<UItemDefinition>> Seed =
		LegacyInventoryStorage::ReadArray(this, ArrayProperty, ClassProperty);
	for (const TSubclassOf<UItemDefinition>& DefinitionClass : Seed)
	{
		UClass* Definition = DefinitionClass.Get();
		if (!Definition || Definition->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
			|| !Definition->IsChildOf(ClassProperty->MetaClass))
		{
			return FailImport(EInventoryOperationResult::InvalidDefinition);
		}
		if (!IsDefinitionDataValid(Cast<UItemDefinition>(Definition->GetDefaultObject())))
		{
			return FailImport(EInventoryOperationResult::InvalidDefinitionData);
		}
	}

	TArray<TStrongObjectPtr<UItemInstance>> StagedReferences;
	TArray<TObjectPtr<UItemInstance>> ImportedItems;
	TSet<FGuid> ImportedIds;
	ImportedItems.Reserve(Seed.Num());
	StagedReferences.Reserve(Seed.Num());
	for (const TSubclassOf<UItemDefinition>& DefinitionClass : Seed)
	{
		FGuid ItemId;
		do { ItemId = FGuid::NewGuid(); } while (!ItemId.IsValid() || ImportedIds.Contains(ItemId));
		UItemInstance* Item = NewObject<UItemInstance>(this);
		StagedReferences.Emplace(Item);
		if (!Item || !Item->Initialize(ItemId, DefinitionClass))
		{
			return FailImport(EInventoryOperationResult::InvalidDefinitionData);
		}
		ImportedIds.Add(ItemId);
		ImportedItems.Add(Item);
	}

	Items = MoveTemp(ImportedItems);
	RebuildLegacyProjection(ArrayProperty, ClassProperty);
	bLegacyInventoryMode = false;
	InitializationState = EInventoryInitializationState::NativeReady;
	LastInitializationResult = EInventoryOperationResult::Success;
	++Revision;
	OnInventoryChanged.Broadcast(Revision);
	return MakeInventoryResult(EInventoryOperationResult::Success);
}

FInventoryOperationResult UInventoryComponent::TryAddDefinition(
	TSubclassOf<UItemDefinition> DefinitionClass)
{
	return TryAddDefinitionQuantity(DefinitionClass, 1);
}

FInventoryOperationResult UInventoryComponent::TryAddDefinitionQuantity(
	TSubclassOf<UItemDefinition> DefinitionClass,
	const int32 Quantity)
{
	if (!CanOperate())
	{
		return MakeInventoryResult(EInventoryOperationResult::NotInitialized);
	}

	if (bMutationInProgress)
	{
		return MakeInventoryResult(EInventoryOperationResult::Busy);
	}

	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	FInventoryStackPlan StackPlan;
	FArrayProperty* ProjectionArray = nullptr;
	FClassProperty* ProjectionClass = nullptr;
	const EInventoryOperationResult Preflight = PreflightAdd(DefinitionClass, Quantity, StackPlan, ProjectionArray, ProjectionClass);
	if (Preflight != EInventoryOperationResult::Success)
	{
		return MakeInventoryResult(Preflight);
	}

	TSet<FGuid> ReservedIds;
	for (const UItemInstance* ExistingItem : Items)
	{
		if (IsValid(ExistingItem))
		{
			ReservedIds.Add(ExistingItem->GetInstanceId());
		}
	}
	TArray<TStrongObjectPtr<UItemInstance>> StagedStacks;
	if (!FInventoryStacking::StageNewStacks(this, DefinitionClass, StackPlan.NewStackQuantities, ReservedIds, StagedStacks))
	{
		return MakeInventoryResult(EInventoryOperationResult::InvalidDefinitionData);
	}

	FInventoryStacking::ApplyMerges(StackPlan);
	for (const TStrongObjectPtr<UItemInstance>& StagedStack : StagedStacks)
	{
		Items.Add(StagedStack.Get());
	}
	if (ProjectionArray)
	{
		RebuildLegacyProjection(ProjectionArray, ProjectionClass);
	}
	++Revision;

	UItemInstance* PrimaryItem = StackPlan.Merges.IsEmpty()
		? (StagedStacks.IsEmpty() ? nullptr : StagedStacks[0].Get())
		: StackPlan.Merges[0].Item.Get();
	const FInventoryOperationResult Result = MakeInventoryResult(EInventoryOperationResult::Success,
		PrimaryItem, PrimaryItem ? PrimaryItem->GetInstanceId() : FGuid(), Quantity);
	OnInventoryChanged.Broadcast(Revision);
	return Result;
}

FInventoryOperationResult UInventoryComponent::TryRemoveItem(FGuid ItemId)
{
	if (!CanOperate())
	{
		return MakeInventoryResult(EInventoryOperationResult::NotInitialized, nullptr, ItemId);
	}
	if (bMutationInProgress)
	{
		return MakeInventoryResult(EInventoryOperationResult::Busy, nullptr, ItemId);
	}
	UItemInstance* Item = FindItemById(ItemId);
	return Item
		? TryRemoveItemQuantity(ItemId, Item->GetQuantity())
		: MakeInventoryResult(EInventoryOperationResult::InvalidItemId, nullptr, ItemId);
}

FInventoryOperationResult UInventoryComponent::TryRemoveItemQuantity(FGuid ItemId, const int32 Quantity)
{
	if (!CanOperate())
	{
		return MakeInventoryResult(EInventoryOperationResult::NotInitialized, nullptr, ItemId);
	}

	if (bMutationInProgress)
	{
		return MakeInventoryResult(EInventoryOperationResult::Busy, nullptr, ItemId);
	}

	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	if (!ItemId.IsValid())
	{
		return MakeInventoryResult(EInventoryOperationResult::InvalidItemId, nullptr, ItemId);
	}
	if (Quantity < 1)
	{
		return MakeInventoryResult(EInventoryOperationResult::InvalidQuantity, nullptr, ItemId);
	}

	const int32 ItemIndex = Items.IndexOfByPredicate(
		[&ItemId](const TObjectPtr<UItemInstance>& Item)
		{
			return IsValid(Item) && Item->GetInstanceId() == ItemId;
		});

	if (ItemIndex == INDEX_NONE)
	{
		return MakeInventoryResult(EInventoryOperationResult::InvalidItemId, nullptr, ItemId);
	}
	UItemInstance* Item = Items[ItemIndex];
	if (!IsValid(Item) || Quantity > Item->GetQuantity())
	{
		return MakeInventoryResult(EInventoryOperationResult::InvalidQuantity, nullptr, ItemId);
	}

	FArrayProperty* ProjectionArray = nullptr;
	FClassProperty* ProjectionClass = nullptr;
	if (InitializationState == EInventoryInitializationState::NativeReady
		&& !LegacyInventoryStorage::FindArray(this, ProjectionArray, ProjectionClass))
	{
		return MakeInventoryResult(EInventoryOperationResult::NotInitialized, nullptr, ItemId);
	}

	const bool bRemoveStack = Quantity == Item->GetQuantity();
	if (bRemoveStack)
	{
		Items.RemoveAt(ItemIndex);
	}
	else
	{
		check(Item->SetQuantity(Item->GetQuantity() - Quantity));
	}
	if (ProjectionArray && bRemoveStack)
	{
		RebuildLegacyProjection(ProjectionArray, ProjectionClass);
	}
	++Revision;

	const FInventoryOperationResult Result = MakeInventoryResult(
		EInventoryOperationResult::Success,
		Item,
		ItemId,
		Quantity);
	OnInventoryChanged.Broadcast(Revision);
	return Result;
}

FInventoryWorldDropResult UInventoryComponent::TryDropItemToWorld(APawn* OwningPawn, const FGuid ItemId)
{
	if (!CanOperate())
	{
		return MakeWorldDropResult(EInventoryWorldDropResult::NotInitialized, ItemId);
	}
	if (bMutationInProgress)
	{
		return MakeWorldDropResult(EInventoryWorldDropResult::Busy, ItemId);
	}
	if (!IsValid(OwningPawn))
	{
		return MakeWorldDropResult(EInventoryWorldDropResult::InvalidPawn, ItemId);
	}
	if (!ItemId.IsValid())
	{
		return MakeWorldDropResult(EInventoryWorldDropResult::InvalidItemId, ItemId);
	}

	UItemInstance* Item = FindItemById(ItemId);
	if (!Item)
	{
		return MakeWorldDropResult(EInventoryWorldDropResult::InvalidItemId, ItemId);
	}
	const TSubclassOf<UItemDefinition> DefinitionClass = Item->GetDefinitionClass();
	const UItemDefinition* Definition = DefinitionClass ? DefinitionClass->GetDefaultObject<UItemDefinition>() : nullptr;
	if (!Definition)
	{
		return MakeWorldDropResult(EInventoryWorldDropResult::InvalidDefinition, ItemId);
	}
	if (!IsDefinitionDataValid(Definition))
	{
		return MakeWorldDropResult(EInventoryWorldDropResult::InvalidDefinitionData, ItemId);
	}
	const int32 Quantity = Item->GetQuantity();
	FTransform DropTransform;
	if (!ResolveWorldDropTransform(OwningPawn, DefinitionClass, DropTransform))
	{
		return MakeWorldDropResult(EInventoryWorldDropResult::NoSafeTransform, ItemId);
	}

	AActor* SpawnedPickup = SpawnAndConfigureWorldDrop(OwningPawn, DefinitionClass, Quantity, DropTransform);
	if (!IsValid(SpawnedPickup))
	{
		return MakeWorldDropResult(EInventoryWorldDropResult::SpawnFailed, ItemId);
	}

	const FInventoryOperationResult Removal = CommitWorldDropRemoval(ItemId);
	if (!Removal.IsSuccess())
	{
		RollbackWorldDrop(SpawnedPickup);
		return MakeWorldDropResult(EInventoryWorldDropResult::RemovalFailedRolledBack, ItemId);
	}

	return MakeWorldDropResult(EInventoryWorldDropResult::Success, ItemId, SpawnedPickup, Quantity);
}

FInventoryStorageTransferResult UInventoryComponent::TryTransferItemToStorage(
	UStorageComponent* Storage, const FGuid ItemId)
{
	return UStorageTransferLibrary::TransferInventoryToStorage(this, Storage, ItemId);
}

TArray<UItemInstance*> UInventoryComponent::GetItemsSnapshot() const
{
	TArray<UItemInstance*> Snapshot;
	Snapshot.Reserve(Items.Num());
	for (UItemInstance* Item : Items)
	{
		Snapshot.Add(Item);
	}
	return Snapshot;
}

UItemInstance* UInventoryComponent::FindItemById(FGuid ItemId) const
{
	if (!ItemId.IsValid())
	{
		return nullptr;
	}

	const TObjectPtr<UItemInstance>* FoundItem = Items.FindByPredicate(
		[&ItemId](const TObjectPtr<UItemInstance>& Item)
		{
			return IsValid(Item) && Item->GetInstanceId() == ItemId;
		});
	return FoundItem ? FoundItem->Get() : nullptr;
}

bool UInventoryComponent::ContainsItem(FGuid ItemId) const
{
	return FindItemById(ItemId) != nullptr;
}

int32 UInventoryComponent::GetTotalQuantity(TSubclassOf<UItemDefinition> DefinitionClass) const
{
	if (!DefinitionClass)
	{
		return 0;
	}
	int64 Total = 0;
	for (const UItemInstance* Item : Items)
	{
		if (IsValid(Item) && Item->GetDefinitionClass() == DefinitionClass)
		{
			Total += Item->GetQuantity();
		}
	}
	return static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

FInventoryOperationResult UInventoryComponent::CanRemoveDefinitionQuantity(
	TSubclassOf<UItemDefinition> DefinitionClass,
	const int32 Quantity) const
{
	if (!CanOperate())
	{
		return MakeInventoryResult(EInventoryOperationResult::NotInitialized);
	}
	if (Quantity < 1)
	{
		return MakeInventoryResult(EInventoryOperationResult::InvalidQuantity);
	}
	if (!DefinitionClass)
	{
		return MakeInventoryResult(EInventoryOperationResult::InvalidDefinition);
	}
	FArrayProperty* ProjectionArray = nullptr;
	FClassProperty* ProjectionClass = nullptr;
	if (InitializationState == EInventoryInitializationState::NativeReady
		&& !LegacyInventoryStorage::FindArray(this, ProjectionArray, ProjectionClass))
	{
		return MakeInventoryResult(EInventoryOperationResult::NotInitialized);
	}
	if (GetTotalQuantity(DefinitionClass) < Quantity)
	{
		return MakeInventoryResult(EInventoryOperationResult::InsufficientQuantity);
	}
	return MakeInventoryResult(EInventoryOperationResult::Success, nullptr, FGuid(), Quantity);
}

FInventoryOperationResult UInventoryComponent::CanAddDefinitionQuantity(
	TSubclassOf<UItemDefinition> DefinitionClass,
	const int32 Quantity) const
{
	if (!CanOperate())
	{
		return MakeInventoryResult(EInventoryOperationResult::NotInitialized);
	}
	FInventoryStackPlan StackPlan;
	FArrayProperty* ProjectionArray = nullptr;
	FClassProperty* ProjectionClass = nullptr;
	const EInventoryOperationResult Preflight = PreflightAdd(DefinitionClass, Quantity, StackPlan, ProjectionArray, ProjectionClass);
	return MakeInventoryResult(Preflight, nullptr, FGuid(), Preflight == EInventoryOperationResult::Success ? Quantity : 0);
}

FInventoryOperationResult UInventoryComponent::CanAddDefinitionQuantities(
	const TArray<FInventoryDefinitionQuantity>& Requests) const
{
	if (!CanOperate())
	{
		return MakeInventoryResult(EInventoryOperationResult::NotInitialized);
	}
	if (Requests.IsEmpty())
	{
		return MakeInventoryResult(EInventoryOperationResult::InvalidQuantity);
	}

	// Merging only touches stacks of the same definition, so after summing duplicates the requests
	// compete only for free slots.
	TArray<TPair<TSubclassOf<UItemDefinition>, int64>> Combined;
	for (const FInventoryDefinitionQuantity& Request : Requests)
	{
		if (Request.Quantity < 1)
		{
			return MakeInventoryResult(EInventoryOperationResult::InvalidQuantity);
		}
		TPair<TSubclassOf<UItemDefinition>, int64>* Existing = Combined.FindByPredicate(
			[&Request](const TPair<TSubclassOf<UItemDefinition>, int64>& Entry) { return Entry.Key == Request.DefinitionClass; });
		if (Existing)
		{
			Existing->Value += Request.Quantity;
		}
		else
		{
			Combined.Emplace(Request.DefinitionClass, Request.Quantity);
		}
	}

	int64 TotalQuantity = 0;
	int32 NewStacks = 0;
	for (const TPair<TSubclassOf<UItemDefinition>, int64>& Entry : Combined)
	{
		if (Entry.Value > MAX_int32)
		{
			return MakeInventoryResult(EInventoryOperationResult::InvalidQuantity);
		}
		FInventoryStackPlan StackPlan;
		FArrayProperty* ProjectionArray = nullptr;
		FClassProperty* ProjectionClass = nullptr;
		const int32 Quantity = static_cast<int32>(Entry.Value);
		const EInventoryOperationResult Preflight = PreflightAdd(Entry.Key, Quantity, StackPlan, ProjectionArray, ProjectionClass);
		if (Preflight != EInventoryOperationResult::Success)
		{
			return MakeInventoryResult(Preflight);
		}
		NewStacks += StackPlan.NewStackQuantities.Num();
		TotalQuantity += Quantity;
	}
	if (Items.Num() + NewStacks > Capacity)
	{
		return MakeInventoryResult(EInventoryOperationResult::CapacityFull);
	}
	return MakeInventoryResult(EInventoryOperationResult::Success, nullptr, FGuid(),
		static_cast<int32>(FMath::Min<int64>(TotalQuantity, MAX_int32)));
}

bool UInventoryComponent::CanOperate() const
{
	return !bLegacyInventoryMode
		&& (!bImportLegacyInventoryOnBeginPlay || InitializationState == EInventoryInitializationState::NativeReady)
		&& InitializationState != EInventoryInitializationState::Failed
		&& !HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed | RF_FinishDestroyed);
}

bool UInventoryComponent::ResolveWorldDropTransform(
	const APawn* OwningPawn,
	const TSubclassOf<UItemDefinition> DefinitionClass,
	FTransform& OutTransform) const
{
	UWorld* World = OwningPawn ? OwningPawn->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	const UItemDefinition* Definition = DefinitionClass ? DefinitionClass->GetDefaultObject<UItemDefinition>() : nullptr;
	FWorldDropPlacementProfile PlacementProfile;
	if (!BuildWorldDropPlacementProfile(Definition, PlacementProfile))
	{
		return false;
	}

	FVector Forward = OwningPawn->GetActorForwardVector();
	Forward.Z = 0.0f;
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	const FVector PawnLocation = OwningPawn->GetActorLocation();
	const TArray<FVector> CandidateOffsets = {
		Forward * 160.0f,
		Forward * 160.0f + Right * 80.0f,
		Forward * 160.0f - Right * 80.0f,
		Forward * 240.0f
	};
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InventoryWorldDrop), false, OwningPawn);
	UClass* PickupClass = GetWorldPickupClass();
	if (PickupClass)
	{
		const TSubclassOf<AActor> PickupActorClass(PickupClass);
		for (TActorIterator<AActor> It(World, PickupActorClass); It; ++It)
		{
			QueryParams.AddIgnoredActor(*It);
		}
	}
	FCollisionObjectQueryParams OccupiedObjectTypes(ECC_WorldStatic);
	OccupiedObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	for (const FVector& Offset : CandidateOffsets)
	{
		const FVector TraceStart = PawnLocation + Offset + FVector(0.0f, 0.0f, 150.0f);
		const FVector TraceEnd = TraceStart - FVector(0.0f, 0.0f, 1000.0f);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
		{
			continue;
		}
		const FVector Location = Hit.ImpactPoint + Hit.ImpactNormal.GetSafeNormal() * PlacementProfile.SurfaceOffset;
		// The supporting component is expected to touch the placement volume.
		FCollisionQueryParams ClearanceQueryParams(QueryParams);
		ClearanceQueryParams.AddIgnoredComponent(Hit.GetComponent());
		const bool bBlocked = World->OverlapAnyTestByObjectType(
			Location, FQuat::Identity, OccupiedObjectTypes, PlacementProfile.OccupancyShape, ClearanceQueryParams);
		if (bBlocked)
		{
			continue;
		}
		OutTransform = FTransform(Forward.Rotation(), Location);
		return true;
	}
	return false;
}

AActor* UInventoryComponent::SpawnAndConfigureWorldDrop(
	APawn* OwningPawn,
	const TSubclassOf<UItemDefinition> DefinitionClass,
	const int32 Quantity,
	const FTransform& DropTransform)
{
	UWorld* World = OwningPawn ? OwningPawn->GetWorld() : nullptr;
	if (!World || !DefinitionClass || Quantity < 1)
	{
		return nullptr;
	}

	UClass* PickupClass = GetWorldPickupClass();
	if (!PickupClass)
	{
		return nullptr;
	}

	AActor* Pickup = World->SpawnActorDeferred<AActor>(PickupClass, DropTransform, OwningPawn, OwningPawn,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!Pickup)
	{
		return nullptr;
	}

	FClassProperty* DefinitionProperty = FindFProperty<FClassProperty>(Pickup->GetClass(), TEXT("ItemDefinition"));
	if (!DefinitionProperty || !DefinitionClass->IsChildOf(DefinitionProperty->MetaClass))
	{
		Pickup->Destroy();
		return nullptr;
	}
	DefinitionProperty->SetPropertyValue_InContainer(Pickup, DefinitionClass.Get());
	FIntProperty* QuantityProperty = FindFProperty<FIntProperty>(Pickup->GetClass(), TEXT("Quantity"));
	if (!QuantityProperty)
	{
		Pickup->Destroy();
		return nullptr;
	}
	QuantityProperty->SetPropertyValue_InContainer(Pickup, Quantity);
	UGameplayStatics::FinishSpawningActor(Pickup, DropTransform);
	if (UFunction* RefreshPresentation = Pickup->FindFunction(TEXT("RefreshWorldPresentation")))
	{
		Pickup->ProcessEvent(RefreshPresentation, nullptr);
	}
	return Pickup;
}

void UInventoryComponent::RollbackWorldDrop(AActor* SpawnedPickup)
{
	if (IsValid(SpawnedPickup))
	{
		SpawnedPickup->Destroy();
	}
}

FInventoryOperationResult UInventoryComponent::CommitWorldDropRemoval(const FGuid ItemId)
{
	return TryRemoveItem(ItemId);
}

void UInventoryComponent::RebuildLegacyProjection(FArrayProperty* ArrayProperty, FClassProperty* ClassProperty)
{
	TArray<TSubclassOf<UItemDefinition>> Definitions;
	Definitions.Reserve(Items.Num());
	for (const UItemInstance* Item : Items)
	{
		Definitions.Add(Item->GetDefinitionClass());
	}
	LegacyInventoryStorage::WriteArray(this, ArrayProperty, ClassProperty, Definitions);
}

EInventoryOperationResult UInventoryComponent::PreflightAdd(
	TSubclassOf<UItemDefinition> DefinitionClass,
	const int32 Quantity,
	FInventoryStackPlan& OutPlan,
	FArrayProperty*& OutProjectionArray,
	FClassProperty*& OutProjectionClass) const
{
	OutPlan = FInventoryStackPlan();
	OutProjectionArray = nullptr;
	OutProjectionClass = nullptr;
	if (Quantity < 1)
	{
		return EInventoryOperationResult::InvalidQuantity;
	}

	UClass* DefinitionUClass = DefinitionClass.Get();
	if (!DefinitionUClass || DefinitionUClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return EInventoryOperationResult::InvalidDefinition;
	}

	const UItemDefinition* Definition = Cast<UItemDefinition>(DefinitionUClass->GetDefaultObject());
	if (!Definition)
	{
		return EInventoryOperationResult::InvalidDefinition;
	}

	if (!IsDefinitionDataValid(Definition))
	{
		return EInventoryOperationResult::InvalidDefinitionData;
	}

	if (InitializationState == EInventoryInitializationState::NativeReady)
	{
		if (!LegacyInventoryStorage::FindArray(this, OutProjectionArray, OutProjectionClass))
		{
			return EInventoryOperationResult::NotInitialized;
		}
		if (!DefinitionUClass->IsChildOf(OutProjectionClass->MetaClass))
		{
			return EInventoryOperationResult::InvalidDefinition;
		}
	}

	if (!FInventoryStacking::BuildAddPlan(Items, Capacity, DefinitionClass, Quantity, OutPlan))
	{
		return EInventoryOperationResult::CapacityFull;
	}
	return EInventoryOperationResult::Success;
}

bool UInventoryComponent::IsDefinitionDataValid(const UItemDefinition* Definition) const
{
	if (!IsValid(Definition))
	{
		return false;
	}

	FString Diagnostic;
	return Definition->ValidateFragments(Diagnostic);
}

bool UInventoryComponent::HasItemId(const FGuid& ItemId) const
{
	return Items.ContainsByPredicate(
		[&ItemId](const TObjectPtr<UItemInstance>& Item)
		{
			return IsValid(Item) && Item->GetInstanceId() == ItemId;
		});
}
