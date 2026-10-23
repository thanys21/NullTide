#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/ItemDefinition.h"
#include "Npc/NpcAIController.h"
#include "Npc/NpcCharacter.h"
#include "Npc/NpcDefinition.h"
#include "Npc/NpcTradeTypes.h"
#include "Player/PlayerStatusHudComponent.h"
#include "Player/WalletComponent.h"
#include "UI/NpcTalkWidget.h"
#include "UI/NpcTradeScreenWidget.h"
#include "UI/PlayerStatusWidget.h"
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

	int32 FocusCount = 0;

	void SetTalkScreenClass(TSubclassOf<UNpcTalkWidget> InClass) { TalkScreenClass = InClass; }
	void SetTradeScreenClass(TSubclassOf<UNpcTradeScreenWidget> InClass) { TradeScreenClass = InClass; }

protected:
	virtual UNpcTalkWidget* CreateTalkWidget(APlayerController& PlayerController) override
	{
		return TalkScreenClass ? CreateWidget<UNpcTalkWidget>(GetWorld(), TalkScreenClass) : nullptr;
	}

	virtual void ShowTalkWidget(APlayerController& PlayerController, UNpcTalkWidget& Widget) override { ++ShowCount; }

	virtual UNpcTradeScreenWidget* CreateTradeWidget(APlayerController& PlayerController) override
	{
		return TradeScreenClass ? CreateWidget<UNpcTradeScreenWidget>(GetWorld(), TradeScreenClass) : nullptr;
	}

	virtual void FocusScreen(APlayerController& PlayerController, UUserWidget& Widget) override { ++FocusCount; }
};

/** Wallet with a test seam to set the coins directly. */
UCLASS(Transient, NotBlueprintable)
class UWalletTestComponent : public UWalletComponent
{
	GENERATED_BODY()

public:
	void TestSetCoins(int32 NewCoins)
	{
		SpendCoins(GetCoins());
		AddCoins(NewCoins);
	}

	bool TestAdd(int32 Amount) { return AddCoins(Amount); }
	bool TestSpend(int32 Amount) { return SpendCoins(Amount); }
};

/** Status HUD with the local-player gate forced and a viewport-free widget. */
UCLASS(Transient, NotBlueprintable)
class UPlayerStatusHudTestComponent : public UPlayerStatusHudComponent
{
	GENERATED_BODY()

public:
	bool bTreatAsLocal = true;
	TSubclassOf<UPlayerStatusWidget> TestWidgetClass;

protected:
	virtual bool IsLocalPlayerController(const APlayerController& PlayerController) const override { return bTreatAsLocal; }

	virtual UPlayerStatusWidget* CreateStatusWidget(APlayerController& PlayerController) override
	{
		return TestWidgetClass ? CreateWidget<UPlayerStatusWidget>(GetWorld(), TestWidgetClass) : nullptr;
	}
};

/** Trade test item A: stackable (50), value 4, in the test store. */
UCLASS(Transient, NotBlueprintable)
class UTradeTestItemA : public UItemDefinition
{
	GENERATED_BODY()

public:
	UTradeTestItemA();
};

/** Trade test item B: not stackable, value 10, in the test store. */
UCLASS(Transient, NotBlueprintable)
class UTradeTestItemB : public UItemDefinition
{
	GENERATED_BODY()

public:
	UTradeTestItemB();
};

/** Trade test item C: stackable (50), value 3, sellable but not in the store. */
UCLASS(Transient, NotBlueprintable)
class UTradeTestItemC : public UItemDefinition
{
	GENERATED_BODY()

public:
	UTradeTestItemC();
};

/** Trade test item without a value: not tradable. */
UCLASS(Transient, NotBlueprintable)
class UTradeTestItemNoValue : public UItemDefinition
{
	GENERATED_BODY()

public:
	UTradeTestItemNoValue();
};

/** Records the game clock's events. */
UCLASS(Transient, NotBlueprintable)
class UGameClockTestListener : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleNewDay(int32 Day) { NewDays.Add(Day); }

	UFUNCTION()
	void HandlePhaseChanged(int32 Day, bool bNight) { Phases.Add(FIntPoint(Day, bNight ? 1 : 0)); }

	TArray<int32> NewDays;
	/** (day, night ? 1 : 0) per phase change. */
	TArray<FIntPoint> Phases;
};

/** Records OnTradeResult and OnHaggleChanged. */
UCLASS(Transient, NotBlueprintable)
class UNpcTradeTestListener : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleTradeResult(AActor* Trader, const FNpcTradeQuote& Result)
	{
		Results.Add(Result);
		if (Hook)
		{
			Hook();
		}
	}

	UFUNCTION()
	void HandleHaggleChanged(ENpcHaggleResult Result) { Haggles.Add(Result); }

	UFUNCTION()
	void HandleInventoryChanged(int32 NewRevision)
	{
		++InventoryEvents;
		if (InventoryHook)
		{
			InventoryHook();
		}
	}

	UFUNCTION()
	void HandleCoinsChanged(int32 NewCoins) { ++CoinEvents; }

	TArray<FNpcTradeQuote> Results;
	TArray<ENpcHaggleResult> Haggles;
	int32 InventoryEvents = 0;
	int32 CoinEvents = 0;
	TFunction<void()> Hook;
	TFunction<void()> InventoryHook;
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

/** A store NPC kind for tests: sells items A and B; sell 75%, buy 100%, 3 haggles, 5-20%, 10 s, 16 lines. */
UCLASS(Transient, NotBlueprintable)
class UNpcStoreTestDefinition : public UNpcTestDefinition
{
	GENERATED_BODY()

public:
	UNpcStoreTestDefinition();
};
