#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "NpcAIController.generated.h"

class ANpcCharacter;
class UBaseZoneComponent;

UENUM(BlueprintType)
enum class ENpcAIState : uint8
{
	/** Waiting before the next walk (or forever without a home zone). */
	Idle,
	Moving,
	/** Stopped and facing the player while the talk panel is open. */
	Talking
};

/**
 * Simple timer-driven wandering for an NPC (Phase 6 S4): idle for a random time, then walk to a random interior cell
 * (the zone without its ring) of the home zone, then idle again. Talking stops it and turns the NPC toward the player;
 * resuming idles again. The controller never ticks; path following is the engine's.
 */
UCLASS()
class NULLTIDE_API ANpcAIController : public AAIController
{
	GENERATED_BODY()

public:
	ANpcAIController();

	/** Cells tried per wander before giving up and idling again. */
	static constexpr int32 MaxWanderAttempts = 5;

	UFUNCTION(BlueprintPure, Category = "NPC")
	ENpcAIState GetNpcState() const { return State; }

	/** Stops walking and turns the NPC toward the partner. */
	void BeginTalk(const AActor* Partner);

	/** Ends a talk: idle, then wander again. */
	void ResumeWander();

	/** A random interior cell of the home zone (false without a home zone). */
	bool ChooseWanderCell(FIntPoint& OutCell);

	void SetWanderSeed(int32 Seed) { WanderStream.Initialize(Seed); }
	bool IsIdleTimerActive() const;

	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/** Projects a point onto the nav mesh. Seam for tests (no nav mesh there). */
	virtual bool ProjectToNavigation(const FVector& Point, FVector& OutLocation) const;

	/** Starts walking to a location; false when the request failed. Seam for tests. */
	virtual bool RequestWanderMove(const FVector& Location);

	void StartIdle();
	void StartWander();
	/** A started move ended (reached or failed). */
	void HandleMoveFinished();

	const UBaseZoneComponent* GetHomeZone() const;

	ENpcAIState State = ENpcAIState::Idle;

private:
	/** True while a move is being requested: a synchronous finish then belongs to the request, not to a walk. */
	bool bRequestingMove = false;
	FTimerHandle IdleTimerHandle;
	FRandomStream WanderStream;
};
