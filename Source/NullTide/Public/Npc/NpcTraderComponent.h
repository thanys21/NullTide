#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Npc/NpcTradeTypes.h"
#include "NpcTraderComponent.generated.h"

class APawn;
class UInventoryComponent;
class UNpcDefinition;
class UWalletComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNpcTradeResult, AActor*, Trader, const FNpcTradeQuote&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNpcHaggleChanged, ENpcHaggleResult, Result);

/**
 * The trade authority of one NPC (Phase 6 S5): its store prices, trades and haggling. Only the pawn the NPC is
 * talking to can trade or haggle. A trade is: checks -> one inventory transaction (the commit point; sold items out,
 * bought items in) -> the wallet change (validated first, so it cannot fail) -> the stored discount used -> one
 * OnTradeResult. Haggling state is kept per NPC until a trade uses the discount or a new game day resets it. The
 * store is unlimited: the NPC holds no items.
 */
UCLASS(ClassGroup = (Npc))
class NULLTIDE_API UNpcTraderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Sums lines of the same item, keeping first-seen order. */
	static TArray<FInventoryDefinitionQuantity> NormalizeLines(const TArray<FInventoryDefinitionQuantity>& Lines);

	/** Pure prices of a request against a store, with a discount (whole percent); checks lines, items and catalog. */
	static FNpcTradeQuote ComputePrices(const UNpcDefinition& Store, const FNpcTradeRequest& Request, int32 DiscountPercent);

	/** A random problem with an integer answer: a + b, a - b (10-99, never negative), a x b (2-12 x 2-9), a / b (exact). */
	static void MakeProblem(FRandomStream& Stream, FText& OutText, int32& OutAnswer);

	/** The NPC's valid definition when it has a store; null otherwise. */
	const UNpcDefinition* GetStore() const;

	UFUNCTION(BlueprintPure, Category = "NPC|Trade")
	bool HasStore() const { return GetStore() != nullptr; }

	/** Exact read-only preflight of TryTrade (including the stored discount, coins and the inventory transaction). */
	FNpcTradeQuote QuoteTrade(AActor* Trader, const FNpcTradeRequest& Request) const;

	/** Runs one trade. OnTradeResult fires once unless the request is nested (TradeInProgress, nothing notified). */
	FNpcTradeQuote TryTrade(AActor* Trader, const FNpcTradeRequest& Request);

	/** Puts one problem on the clock for the talk partner. */
	ENpcHaggleResult StartHaggle(AActor* Trader);

	/** Answers the active problem: correct stores a random discount; wrong uses an attempt. */
	ENpcHaggleResult SubmitHaggleAnswer(AActor* Trader, int32 Answer);

	/** The problem's time ran out (the timer's work), or the talk ended: the attempt is used. */
	void ExpireHaggleProblem();

	FNpcHaggleState GetHaggleState() const;

	void SetHaggleSeed(int32 Seed) { HaggleStream.Initialize(Seed); }
	bool IsTradeInProgress() const { return bTradeInProgress; }

	UPROPERTY(BlueprintAssignable, Category = "NPC|Trade")
	FOnNpcTradeResult OnTradeResult;

	UPROPERTY(BlueprintAssignable, Category = "NPC|Haggle")
	FOnNpcHaggleChanged OnHaggleChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** The trader as a pawn when it is the NPC's talk partner during an open talk; null otherwise. */
	APawn* GetPartner(AActor* Trader) const;

	/** Builds the full quote for a partner (inventory and wallet included). */
	FNpcTradeQuote BuildQuote(AActor* Trader, const FNpcTradeRequest& Request, UInventoryComponent*& OutInventory,
		UWalletComponent*& OutWallet, TArray<FInventoryDefinitionQuantity>& OutSell, TArray<FInventoryDefinitionQuantity>& OutBuy) const;

	int32 GetCurrentDay() const;
	/** Resets attempts and the stored discount when the game day changed since they were counted. */
	void RefreshDay();
	int32 GetEffectiveStoredDiscount() const;
	int32 GetEffectiveAttemptsUsed() const;
	void ClearProblem();

	int32 HaggleDay = 0;
	int32 AttemptsUsed = 0;
	int32 StoredDiscountPercent = 0;
	bool bProblemActive = false;
	/** The game day the active problem was given on. */
	int32 ProblemDay = 0;
	int32 ProblemAnswer = 0;
	FText ProblemText;
	FTimerHandle ProblemTimerHandle;
	FRandomStream HaggleStream;
	bool bTradeInProgress = false;
};
