#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameClockSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameNewDay, int32, Day);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGamePhaseChanged, int32, Day, bool, bNight);

/**
 * Minimal game clock (Phase 6 S5): each cycle is a day of DayLengthSeconds followed by a night of NightLengthSeconds.
 * The day number starts at 1 and advances when a new day begins (after the night). Driven by a world timer (no tick)
 * once play begins. It only keeps time: no lighting, no save (Phase 7 will persist the day).
 */
UCLASS()
class NULLTIDE_API UGameClockSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** C-1: a 20-minute day and a 10-minute night (real time). */
	static constexpr float DayLengthSeconds = 1200.f;
	static constexpr float NightLengthSeconds = 600.f;

	static UGameClockSubsystem* Get(const UObject* WorldContextObject);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Starts the phase timer (idempotent). Play does this; tests call it in worlds that never begin play. */
	void StartClock();

	UFUNCTION(BlueprintPure, Category = "World|Clock")
	int32 GetDay() const { return Day; }

	UFUNCTION(BlueprintPure, Category = "World|Clock")
	bool IsNight() const { return bNight; }

	/** Progress through the current phase (day or night), 0..1. */
	UFUNCTION(BlueprintPure, Category = "World|Clock")
	float GetPhaseProgress01() const;

	/** Ends the current phase now: day → night, or night → the next day. Also the timer's work. */
	void AdvancePhase();

	/** Jumps to the start of the next day (test and debug seam). */
	void AdvanceToNextDay();

	UPROPERTY(BlueprintAssignable, Category = "World|Clock")
	FOnGameNewDay OnNewDay;

	UPROPERTY(BlueprintAssignable, Category = "World|Clock")
	FOnGamePhaseChanged OnPhaseChanged;

private:
	void StartPhaseTimer();
	float GetCurrentPhaseLength() const { return bNight ? NightLengthSeconds : DayLengthSeconds; }

	int32 Day = 1;
	bool bNight = false;
	double PhaseStartSeconds = 0.0;
	FTimerHandle PhaseTimerHandle;
};
