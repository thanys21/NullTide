#include "ResourceGathering/ResourceNodeActor.h"

#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"
#include "TimerManager.h"
#include "ToolLoadout/ToolLoadoutComponent.h"

namespace
{
TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<AResourceNodeActor>> ActiveGatherNodes;
}

AResourceNodeActor::AResourceNodeActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AResourceNodeActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeYield();
}

bool AResourceNodeActor::BeginGatherFromInteractor(AActor* Interactor)
{
	if (IsGathering() || IsDepleted() || !IsValid(Interactor) || !IsOutputDefinitionUsable())
	{
		return false;
	}

	UInventoryComponent* Inventory = ResolveInventoryForInteractor(Interactor);
	if (!IsValid(Inventory))
	{
		return false;
	}

	if (RequiredToolType != EToolType::None)
	{
		UToolLoadoutComponent* ToolLoadout = ResolveToolLoadoutForInteractor(Interactor);
		if (!IsValid(ToolLoadout) || !ToolLoadout->HasEquippedTool(RequiredToolType))
		{
			return false;
		}
	}

	CancelActiveGatherForInteractor(Interactor);
	ActiveInteractor = Interactor;
	GatherState = EResourceNodeState::Gathering;
	GatherStartTimeSeconds = FPlatformTime::Seconds();
	ActiveGatherNodes.Add(Interactor, this);

	if (!ScheduleGatherCompletion())
	{
		ClearGathering();
		return false;
	}

	return true;
}

void AResourceNodeActor::CancelGather()
{
	if (IsGathering())
	{
		ClearGathering();
	}
}

bool AResourceNodeActor::CancelActiveGatherForInteractor(AActor* Interactor)
{
	if (!IsValid(Interactor))
	{
		return false;
	}

	const TWeakObjectPtr<AResourceNodeActor>* FoundNode = ActiveGatherNodes.Find(Interactor);
	AResourceNodeActor* Node = FoundNode ? FoundNode->Get() : nullptr;
	if (!IsValid(Node))
	{
		ActiveGatherNodes.Remove(Interactor);
		return false;
	}

	Node->CancelGather();
	return true;
}

AResourceNodeActor* AResourceNodeActor::FindActiveGatherNodeForInteractor(const AActor* Interactor)
{
	if (!IsValid(Interactor))
	{
		return nullptr;
	}

	const TWeakObjectPtr<AResourceNodeActor>* FoundNode = ActiveGatherNodes.Find(const_cast<AActor*>(Interactor));
	AResourceNodeActor* Node = FoundNode ? FoundNode->Get() : nullptr;
	return IsValid(Node) ? Node : nullptr;
}

bool AResourceNodeActor::ShouldShowInteractionPromptFor(const AActor* Interactable)
{
	if (!IsValid(Interactable))
	{
		return false;
	}

	const AResourceNodeActor* ResourceNode = Cast<AResourceNodeActor>(Interactable);
	return !ResourceNode || !ResourceNode->IsDepleted();
}

float AResourceNodeActor::GetNormalizedGatherProgress() const
{
	if (!IsGathering() || GatherDuration <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(static_cast<float>((FPlatformTime::Seconds() - GatherStartTimeSeconds) / GatherDuration), 0.0f, 1.0f);
}

UInventoryComponent* AResourceNodeActor::ResolveInventoryForInteractor(AActor* Interactor) const
{
	return IsValid(Interactor) ? Interactor->FindComponentByClass<UInventoryComponent>() : nullptr;
}

UToolLoadoutComponent* AResourceNodeActor::ResolveToolLoadoutForInteractor(AActor* Interactor) const
{
	return IsValid(Interactor) ? Interactor->FindComponentByClass<UToolLoadoutComponent>() : nullptr;
}

bool AResourceNodeActor::ScheduleGatherCompletion()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	World->GetTimerManager().SetTimer(
		GatherTimerHandle,
		this,
		&AResourceNodeActor::CompleteGather,
		FMath::Max(GatherDuration, KINDA_SMALL_NUMBER),
		false);
	return true;
}

void AResourceNodeActor::CompleteGather()
{
	if (!IsGathering())
	{
		return;
	}

	AActor* Interactor = ActiveInteractor.Get();
	UInventoryComponent* Inventory = ResolveInventoryForInteractor(Interactor);
	if (IsValid(Inventory))
	{
		const FInventoryOperationResult Result = Inventory->TryAddDefinition(OutputItemDefinition);
		if (Result.IsSuccess())
		{
			RemainingYield = FMath::Max(0, RemainingYield - 1);
		}
	}

	ClearGathering();
}

bool AResourceNodeActor::IsOutputDefinitionUsable() const
{
	UClass* DefinitionClass = OutputItemDefinition.Get();
	return DefinitionClass
		&& !DefinitionClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
		&& DefinitionClass->IsChildOf(UItemDefinition::StaticClass());
}

void AResourceNodeActor::InitializeYield()
{
	RemainingYield = FMath::Max(0, MaxYield);
	GatherState = RemainingYield > 0 ? EResourceNodeState::Idle : EResourceNodeState::Depleted;
}

void AResourceNodeActor::ClearGathering()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GatherTimerHandle);
	}

	if (ActiveInteractor.IsValid())
	{
		const TWeakObjectPtr<AResourceNodeActor>* FoundNode = ActiveGatherNodes.Find(ActiveInteractor);
		if (FoundNode && FoundNode->Get() == this)
		{
			ActiveGatherNodes.Remove(ActiveInteractor);
		}
	}

	ActiveInteractor.Reset();
	GatherStartTimeSeconds = 0.0;
	GatherState = RemainingYield > 0 ? EResourceNodeState::Idle : EResourceNodeState::Depleted;
}
