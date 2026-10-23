#include "World/GameClockSubsystem.h"

#include "Engine/World.h"
#include "TimerManager.h"

UGameClockSubsystem* UGameClockSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UGameClockSubsystem>() : nullptr;
}

void UGameClockSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.IsGameWorld())
	{
		StartClock();
	}
}

void UGameClockSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimerHandle);
	}
	Super::Deinitialize();
}

void UGameClockSubsystem::StartClock()
{
	const UWorld* World = GetWorld();
	if (World && !World->GetTimerManager().IsTimerActive(PhaseTimerHandle))
	{
		StartPhaseTimer();
	}
}

void UGameClockSubsystem::StartPhaseTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	PhaseStartSeconds = World->GetTimeSeconds();
	World->GetTimerManager().SetTimer(PhaseTimerHandle, this, &UGameClockSubsystem::AdvancePhase, GetCurrentPhaseLength(), false);
}

float UGameClockSubsystem::GetPhaseProgress01() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}
	return FMath::Clamp(static_cast<float>((World->GetTimeSeconds() - PhaseStartSeconds) / GetCurrentPhaseLength()), 0.f, 1.f);
}

void UGameClockSubsystem::AdvancePhase()
{
	bNight = !bNight;
	if (!bNight)
	{
		++Day;
	}
	StartPhaseTimer();
	// Captured first: a listener that advances the clock again must not change what this call reports.
	const int32 NewDay = Day;
	const bool bNewNight = bNight;
	OnPhaseChanged.Broadcast(NewDay, bNewNight);
	if (!bNewNight)
	{
		OnNewDay.Broadcast(NewDay);
	}
}

void UGameClockSubsystem::AdvanceToNextDay()
{
	if (!bNight)
	{
		AdvancePhase();
	}
	AdvancePhase();
}
