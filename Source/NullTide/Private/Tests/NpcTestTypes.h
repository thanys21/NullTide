#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Npc/NpcAIController.h"
#include "Npc/NpcCharacter.h"
#include "Npc/NpcDefinition.h"
#include "UI/NpcTalkWidget.h"
#include "NpcTestTypes.generated.h"

/** Wander controller with the navigation seams scripted (S4): a test world has no nav mesh. */
UCLASS(Transient, NotBlueprintable)
class ANpcTestAIController : public ANpcAIController
{
	GENERATED_BODY()

public:
	bool bProjectSucceeds = true;
	bool bMoveSucceeds = true;
	/** Added to every projected point (a projection that drifts out of the interior). */
	FVector ProjectOffset = FVector::ZeroVector;
	/** Like MoveTo when already at the goal or without a path: the request reports its finish synchronously. */
	bool bFinishSynchronously = false;
	/** This many requests fail (after their synchronous finish) before the next ones succeed. */
	int32 FailNextMoves = 0;
	int32 ProjectCalls = 0;
	TArray<FVector> MoveRequests;

	void TestStartWander() { StartWander(); }
	void TestFinishMove() { HandleMoveFinished(); }

protected:
	virtual bool ProjectToNavigation(const FVector& Point, FVector& OutLocation) const override
	{
		++const_cast<ANpcTestAIController*>(this)->ProjectCalls;
		OutLocation = Point + ProjectOffset;
		return bProjectSucceeds;
	}

	virtual bool RequestWanderMove(const FVector& Location) override
	{
		if (bFinishSynchronously)
		{
			HandleMoveFinished();
		}
		if (FailNextMoves > 0)
		{
			--FailNextMoves;
			return false;
		}
		if (bMoveSucceeds)
		{
			MoveRequests.Add(Location);
		}
		return bMoveSucceeds;
	}
};

/** NPC with the test controller and a viewport-free talk panel (S4). */
UCLASS(Transient, NotBlueprintable)
class ANpcTestCharacter : public ANpcCharacter
{
	GENERATED_BODY()

public:
	ANpcTestCharacter() { AIControllerClass = ANpcTestAIController::StaticClass(); }

	int32 ShowCount = 0;

	void SetTalkScreenClass(TSubclassOf<UNpcTalkWidget> InClass) { TalkScreenClass = InClass; }

protected:
	virtual UNpcTalkWidget* CreateTalkWidget(APlayerController& PlayerController) override
	{
		return TalkScreenClass ? CreateWidget<UNpcTalkWidget>(GetWorld(), TalkScreenClass) : nullptr;
	}

	virtual void ShowTalkWidget(APlayerController& PlayerController, UNpcTalkWidget& Widget) override { ++ShowCount; }
};

/** A valid NPC kind for tests: "Tester", two greetings, short idle times. */
UCLASS(Transient, NotBlueprintable)
class UNpcTestDefinition : public UNpcDefinition
{
	GENERATED_BODY()

public:
	UNpcTestDefinition()
	{
		NpcName = FText::FromString(TEXT("Tester"));
		GreetingLines = { FText::FromString(TEXT("Hello")), FText::FromString(TEXT("Hi")) };
		NpcActorClass = ANpcTestCharacter::StaticClass();
		IdleTimeMin = 1.f;
		IdleTimeMax = 2.f;
	}
};

/** An invalid NPC kind (no actor class): residents of this kind are skipped. */
UCLASS(Transient, NotBlueprintable)
class UNpcBrokenTestDefinition : public UNpcDefinition
{
	GENERATED_BODY()

public:
	UNpcBrokenTestDefinition()
	{
		NpcName = FText::FromString(TEXT("Broken"));
		GreetingLines = { FText::FromString(TEXT("...")) };
	}
};
