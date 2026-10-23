#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryTypes.h"
#include "NpcTradeTypes.generated.h"

/** Outcome of a trade quote or commit (Phase 6 S5), in check order. */
UENUM(BlueprintType)
enum class ENpcTradeResult : uint8
{
	Success,
	/** The trader is not the pawn this NPC is talking to (or no talk is open). */
	NotTalking,
	NoInventory,
	NoWallet,
	/** The NPC has no store (no catalog or invalid store data). */
	NoStore,
	/** Nothing to sell and nothing to buy. */
	EmptyTrade,
	/** More item types on one side than the NPC allows (16). */
	TooManyLines,
	/** A sold item has no trade value, or is not a valid item. */
	NotTradable,
	/** A bought item is not in the NPC's catalog. */
	NotInStore,
	InvalidQuantity,
	NotEnoughCoins,
	/** The inventory holds less of a sold item than requested. */
	InsufficientItems,
	/** The bought items do not fit, even after the sold ones leave. */
	InventoryFull,
	InventoryBusy,
	/** A trade is already running on this NPC (nested request; nothing changed, no notification). */
	TradeInProgress
};

/** One trade request: what the player sells and what they buy. Lines of the same item are summed. */
USTRUCT(BlueprintType)
struct NULLTIDE_API FNpcTradeRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Trade")
	TArray<FInventoryDefinitionQuantity> Sell;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Trade")
	TArray<FInventoryDefinitionQuantity> Buy;
};

/** Prices of a trade request and whether it can (or did) happen. */
USTRUCT(BlueprintType)
struct NULLTIDE_API FNpcTradeQuote
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade")
	ENpcTradeResult Result = ENpcTradeResult::EmptyTrade;

	/** Coins the NPC pays for the sold items: floor(sum of values x SellRate). */
	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade")
	int32 SellTotal = 0;

	/** Coins the bought items cost before the discount: ceil(sum of values x BuyRate). */
	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade")
	int32 BuyTotal = 0;

	/** The haggle discount applied (whole percent) and its coins: floor(BuyTotal x percent / 100). */
	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade")
	int32 DiscountPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade")
	int32 Discount = 0;

	/** SellTotal - (BuyTotal - Discount): positive, the player receives coins; negative, the player pays. */
	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade")
	int32 Net = 0;

	/** Index of the failing line on its side, or INDEX_NONE. */
	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade")
	int32 FailedIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade")
	FString Diagnostic;

	bool IsSuccess() const { return Result == ENpcTradeResult::Success; }
};

/** Outcome of a haggle request. */
UENUM(BlueprintType)
enum class ENpcHaggleResult : uint8
{
	/** A problem is on the clock. */
	Started,
	/** Correct in time: the discount is stored for this NPC's next trade. */
	Correct,
	/** Wrong: one attempt used, no discount. */
	Wrong,
	/** The time ran out (or the talk ended): one attempt used, no discount. */
	TimedOut,
	NotTalking,
	NoStore,
	/** All of today's attempts are used. */
	NoAttemptsLeft,
	/** A discount is already stored; the next trade uses it first. */
	DiscountStored,
	/** A problem is already on the clock. */
	ProblemActive,
	/** No problem to answer. */
	NoProblem
};

/** What the trade screen shows about haggling with one NPC. */
USTRUCT(BlueprintType)
struct NULLTIDE_API FNpcHaggleState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Haggle")
	int32 AttemptsLeft = 0;

	/** The stored discount in whole percent, or 0. */
	UPROPERTY(BlueprintReadOnly, Category = "NPC|Haggle")
	int32 StoredDiscountPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Haggle")
	bool bProblemActive = false;

	/** "47 + 28 = ?" while a problem is active. The answer never leaves the NPC. */
	UPROPERTY(BlueprintReadOnly, Category = "NPC|Haggle")
	FText ProblemText;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Haggle")
	float TimeLeft = 0.f;
};
