#include "Npc/NpcAIController.h"

#include "Base/BaseZoneComponent.h"
#include "Building/BuildingSubsystem.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Npc/NpcCharacter.h"
#include "Npc/NpcDefinition.h"
#include "TimerManager.h"

ANpcAIController::ANpcAIController()
{
	// Timers and the engine's path following drive everything.
	PrimaryActorTick.bCanEverTick = false;
	WanderStream.GenerateNewSeed();
}

void ANpcAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	StartIdle();
}

void ANpcAIController::OnUnPossess()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(IdleTimerHandle);
	}
	State = ENpcAIState::Idle;
	Super::OnUnPossess();
}

const UBaseZoneComponent* ANpcAIController::GetHomeZone() const
{
	const ANpcCharacter* Npc = Cast<ANpcCharacter>(GetPawn());
	return Npc ? Npc->GetHomeZone() : nullptr;
}

bool ANpcAIController::IsIdleTimerActive() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(IdleTimerHandle);
}

void ANpcAIController::StartIdle()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(IdleTimerHandle);
	State = ENpcAIState::Idle;
	// Without a home zone there is nowhere to wander: stand still.
	if (!GetHomeZone())
	{
		return;
	}
	const ANpcCharacter* Npc = Cast<ANpcCharacter>(GetPawn());
	const UNpcDefinition* Definition = Npc ? Npc->GetValidDefinition() : nullptr;
	const float MinTime = Definition ? Definition->IdleTimeMin : 3.f;
	const float MaxTime = Definition ? Definition->IdleTimeMax : 8.f;
	World->GetTimerManager().SetTimer(IdleTimerHandle, this, &ANpcAIController::StartWander,
		FMath::Max(WanderStream.FRandRange(MinTime, MaxTime), 0.01f), false);
}

bool ANpcAIController::ChooseWanderCell(FIntPoint& OutCell)
{
	const UBaseZoneComponent* Zone = GetHomeZone();
	const FIntRect Interior = Zone ? Zone->GetBuildableCells() : FIntRect();
	if (!Zone || Interior.Width() <= 0 || Interior.Height() <= 0)
	{
		return false;
	}
	OutCell = FIntPoint(WanderStream.RandRange(Interior.Min.X, Interior.Max.X - 1), WanderStream.RandRange(Interior.Min.Y, Interior.Max.Y - 1));
	return true;
}

void ANpcAIController::StartWander()
{
	const APawn* NpcPawn = GetPawn();
	if (State == ENpcAIState::Talking || !NpcPawn)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(IdleTimerHandle);
	}
	const UBaseZoneComponent* Zone = GetHomeZone();
	const FIntRect Interior = Zone ? Zone->GetBuildableCells() : FIntRect();
	for (int32 Attempt = 0; Attempt < MaxWanderAttempts; ++Attempt)
	{
		FIntPoint Cell;
		FVector Target;
		// The projection may shift the point; a target that left the interior is not used.
		if (ChooseWanderCell(Cell)
			&& ProjectToNavigation(UBuildingSubsystem::CellToWorldCenter(Cell, NpcPawn->GetActorLocation().Z), Target)
			&& Interior.Contains(UBuildingSubsystem::WorldToCell(Target)))
		{
			State = ENpcAIState::Moving;
			// MoveTo can finish synchronously (already at the goal, no path); that finish belongs to this request and
			// must not start idling while the loop may still start a real move.
			bRequestingMove = true;
			const bool bMoving = RequestWanderMove(Target);
			bRequestingMove = false;
			if (bMoving)
			{
				return;
			}
			State = ENpcAIState::Idle;
		}
	}
	StartIdle();
}

void ANpcAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);
	HandleMoveFinished();
}

void ANpcAIController::HandleMoveFinished()
{
	if (State == ENpcAIState::Moving && !bRequestingMove)
	{
		StartIdle();
	}
}

void ANpcAIController::BeginTalk(const AActor* Partner)
{
	// State first: stopping the move reports an aborted move, which must not restart idling.
	State = ENpcAIState::Talking;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(IdleTimerHandle);
	}
	StopMovement();
	APawn* NpcPawn = GetPawn();
	if (NpcPawn && Partner)
	{
		const FVector ToPartner = Partner->GetActorLocation() - NpcPawn->GetActorLocation();
		if (ToPartner.SizeSquared2D() > UE_KINDA_SMALL_NUMBER)
		{
			NpcPawn->SetActorRotation(FRotator(0.0, ToPartner.Rotation().Yaw, 0.0));
		}
	}
}

void ANpcAIController::ResumeWander()
{
	if (State == ENpcAIState::Talking)
	{
		StartIdle();
	}
}

bool ANpcAIController::ProjectToNavigation(const FVector& Point, FVector& OutLocation) const
{
	const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Projected;
	if (!Navigation || !Navigation->ProjectPointToNavigation(Point, Projected, FVector(60.0, 60.0, 300.0)))
	{
		return false;
	}
	OutLocation = Projected.Location;
	return true;
}

bool ANpcAIController::RequestWanderMove(const FVector& Location)
{
	return MoveToLocation(Location, 50.f, true, true, false, false) == EPathFollowingRequestResult::RequestSuccessful;
}
