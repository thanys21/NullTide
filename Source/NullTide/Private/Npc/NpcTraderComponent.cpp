#include "Npc/NpcTraderComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Inventory/InventoryComponent.h"
#include "Items/Fragments/ItemFragment_TradeValue.h"
#include "Items/ItemDefinition.h"
#include "Npc/NpcCharacter.h"
#include "Npc/NpcDefinition.h"
#include "Player/WalletComponent.h"
#include "TimerManager.h"
#include "World/GameClockSubsystem.h"

#define LOCTEXT_NAMESPACE "NpcTrader"

namespace
{
FNpcTradeQuote MakeTradeFailure(FNpcTradeQuote Quote, const ENpcTradeResult Code, const FString& Diagnostic, const int32 FailedIndex = INDEX_NONE)
{
	Quote.Result = Code;
	Quote.Diagnostic = Diagnostic;
	Quote.FailedIndex = FailedIndex;
	return Quote;
}

ENpcTradeResult MapInventoryResult(const EInventoryOperationResult Result)
{
	switch (Result)
	{
	case EInventoryOperationResult::Success:
		return ENpcTradeResult::Success;
	case EInventoryOperationResult::InsufficientQuantity:
		return ENpcTradeResult::InsufficientItems;
	case EInventoryOperationResult::CapacityFull:
		return ENpcTradeResult::InventoryFull;
	case EInventoryOperationResult::Busy:
		return ENpcTradeResult::InventoryBusy;
	case EInventoryOperationResult::InvalidQuantity:
		return ENpcTradeResult::InvalidQuantity;
	case EInventoryOperationResult::NotInitialized:
		return ENpcTradeResult::NoInventory;
	default:
		return ENpcTradeResult::NotTradable;
	}
}
}

void UNpcTraderComponent::BeginPlay()
{
	Super::BeginPlay();
	HaggleStream.GenerateNewSeed();
}

void UNpcTraderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ProblemTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

TArray<FInventoryDefinitionQuantity> UNpcTraderComponent::NormalizeLines(const TArray<FInventoryDefinitionQuantity>& Lines)
{
	TArray<FInventoryDefinitionQuantity> Merged;
	for (const FInventoryDefinitionQuantity& Line : Lines)
	{
		if (FInventoryDefinitionQuantity* Existing = Merged.FindByPredicate(
			[&Line](const FInventoryDefinitionQuantity& Entry) { return Entry.DefinitionClass == Line.DefinitionClass; }))
		{
			Existing->Quantity = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(Existing->Quantity) + Line.Quantity, MAX_int32));
		}
		else
		{
			Merged.Add(Line);
		}
	}
	return Merged;
}

FNpcTradeQuote UNpcTraderComponent::ComputePrices(const UNpcDefinition& Store, const FNpcTradeRequest& Request, const int32 DiscountPercent)
{
	FNpcTradeQuote Quote;
	// Every requested line counts on its own: a negative line must not cancel another one when they are summed.
	for (const TArray<FInventoryDefinitionQuantity>* Side : { &Request.Sell, &Request.Buy })
	{
		for (int32 Index = 0; Index < Side->Num(); ++Index)
		{
			if ((*Side)[Index].Quantity < 1)
			{
				return MakeTradeFailure(Quote, ENpcTradeResult::InvalidQuantity, FString::Printf(TEXT("Line %d has no quantity."), Index), Index);
			}
		}
	}
	const TArray<FInventoryDefinitionQuantity> Sell = NormalizeLines(Request.Sell);
	const TArray<FInventoryDefinitionQuantity> Buy = NormalizeLines(Request.Buy);
	if (Sell.IsEmpty() && Buy.IsEmpty())
	{
		return MakeTradeFailure(Quote, ENpcTradeResult::EmptyTrade, TEXT("Nothing to sell or buy."));
	}
	if (Sell.Num() > Store.MaxLinesPerSide || Buy.Num() > Store.MaxLinesPerSide)
	{
		return MakeTradeFailure(Quote, ENpcTradeResult::TooManyLines,
			FString::Printf(TEXT("At most %d item types per side (selling %d, buying %d)."), Store.MaxLinesPerSide, Sell.Num(), Buy.Num()));
	}
	int64 SellValue = 0;
	for (int32 Index = 0; Index < Sell.Num(); ++Index)
	{
		const int32 Value = UItemFragment_TradeValue::GetTradeValue(Sell[Index].DefinitionClass);
		if (Sell[Index].Quantity < 1)
		{
			return MakeTradeFailure(Quote, ENpcTradeResult::InvalidQuantity, FString::Printf(TEXT("Sell line %d has no quantity."), Index), Index);
		}
		if (Value <= 0)
		{
			return MakeTradeFailure(Quote, ENpcTradeResult::NotTradable, FString::Printf(TEXT("Sell line %d has no trade value."), Index), Index);
		}
		SellValue += static_cast<int64>(Value) * Sell[Index].Quantity;
	}
	int64 BuyValue = 0;
	for (int32 Index = 0; Index < Buy.Num(); ++Index)
	{
		if (Buy[Index].Quantity < 1)
		{
			return MakeTradeFailure(Quote, ENpcTradeResult::InvalidQuantity, FString::Printf(TEXT("Buy line %d has no quantity."), Index), Index);
		}
		if (!Store.StoreItems.Contains(Buy[Index].DefinitionClass))
		{
			return MakeTradeFailure(Quote, ENpcTradeResult::NotInStore, FString::Printf(TEXT("Buy line %d is not in the store."), Index), Index);
		}
		BuyValue += static_cast<int64>(UItemFragment_TradeValue::GetTradeValue(Buy[Index].DefinitionClass)) * Buy[Index].Quantity;
	}
	// Totals per side, so cheap items still count; the rounding favours the trader by under one coin per side.
	const double SellCoins = FMath::FloorToDouble(static_cast<double>(SellValue) * Store.SellRate);
	const double BuyCoins = FMath::CeilToDouble(static_cast<double>(BuyValue) * Store.BuyRate);
	// Refused rather than clamped: a clamp would lose coins (selling) or undercharge (buying).
	if (SellCoins > MAX_int32 || BuyCoins > MAX_int32)
	{
		return MakeTradeFailure(Quote, ENpcTradeResult::InvalidQuantity, TEXT("The trade is worth more coins than a wallet can hold."));
	}
	Quote.SellTotal = static_cast<int32>(SellCoins);
	Quote.BuyTotal = static_cast<int32>(BuyCoins);
	Quote.DiscountPercent = FMath::Clamp(DiscountPercent, 0, 100);
	Quote.Discount = static_cast<int32>(static_cast<int64>(Quote.BuyTotal) * Quote.DiscountPercent / 100);
	Quote.Net = Quote.SellTotal - (Quote.BuyTotal - Quote.Discount);
	Quote.Result = ENpcTradeResult::Success;
	return Quote;
}

void UNpcTraderComponent::MakeProblem(FRandomStream& Stream, FText& OutText, int32& OutAnswer)
{
	int32 A = 0;
	int32 B = 0;
	const TCHAR* Operator = TEXT("+");
	switch (Stream.RandRange(0, 3))
	{
	case 0:
		A = Stream.RandRange(10, 99);
		B = Stream.RandRange(10, 99);
		OutAnswer = A + B;
		break;
	case 1:
		A = Stream.RandRange(10, 99);
		B = Stream.RandRange(10, A);
		Operator = TEXT("-");
		OutAnswer = A - B;
		break;
	case 2:
		A = Stream.RandRange(2, 12);
		B = Stream.RandRange(2, 9);
		Operator = TEXT("x");
		OutAnswer = A * B;
		break;
	default:
		OutAnswer = Stream.RandRange(2, 12);
		B = Stream.RandRange(2, 9);
		A = OutAnswer * B;
		Operator = TEXT("/");
		break;
	}
	OutText = FText::FromString(FString::Printf(TEXT("%d %s %d = ?"), A, Operator, B));
}

const UNpcDefinition* UNpcTraderComponent::GetStore() const
{
	const ANpcCharacter* Npc = Cast<ANpcCharacter>(GetOwner());
	const UNpcDefinition* Definition = Npc ? Npc->GetValidDefinition() : nullptr;
	return Definition && Definition->HasStore() ? Definition : nullptr;
}

APawn* UNpcTraderComponent::GetPartner(AActor* Trader) const
{
	const ANpcCharacter* Npc = Cast<ANpcCharacter>(GetOwner());
	APawn* Pawn = Cast<APawn>(Trader);
	return Npc && Pawn && Npc->IsTalking() && Npc->GetTalkPartner() == Pawn ? Pawn : nullptr;
}

int32 UNpcTraderComponent::GetCurrentDay() const
{
	const UGameClockSubsystem* Clock = UGameClockSubsystem::Get(this);
	return Clock ? Clock->GetDay() : 1;
}

void UNpcTraderComponent::RefreshDay()
{
	const int32 Day = GetCurrentDay();
	if (Day != HaggleDay)
	{
		HaggleDay = Day;
		AttemptsUsed = 0;
		StoredDiscountPercent = 0;
	}
}

int32 UNpcTraderComponent::GetEffectiveStoredDiscount() const
{
	return GetCurrentDay() == HaggleDay ? StoredDiscountPercent : 0;
}

int32 UNpcTraderComponent::GetEffectiveAttemptsUsed() const
{
	return GetCurrentDay() == HaggleDay ? AttemptsUsed : 0;
}

FNpcTradeQuote UNpcTraderComponent::BuildQuote(AActor* Trader, const FNpcTradeRequest& Request, UInventoryComponent*& OutInventory,
	UWalletComponent*& OutWallet, TArray<FInventoryDefinitionQuantity>& OutSell, TArray<FInventoryDefinitionQuantity>& OutBuy) const
{
	FNpcTradeQuote Quote;
	const APawn* Pawn = GetPartner(Trader);
	if (!Pawn)
	{
		return MakeTradeFailure(Quote, ENpcTradeResult::NotTalking, TEXT("Only the pawn this NPC is talking to can trade."));
	}
	const UNpcDefinition* Store = GetStore();
	if (!Store)
	{
		return MakeTradeFailure(Quote, ENpcTradeResult::NoStore, TEXT("This NPC has no store."));
	}
	OutInventory = Pawn->FindComponentByClass<UInventoryComponent>();
	if (!OutInventory || !OutInventory->IsNativeAuthorityActive())
	{
		return MakeTradeFailure(Quote, ENpcTradeResult::NoInventory, TEXT("The trader has no active inventory."));
	}
	OutWallet = Pawn->FindComponentByClass<UWalletComponent>();
	if (!OutWallet)
	{
		return MakeTradeFailure(Quote, ENpcTradeResult::NoWallet, TEXT("The trader has no wallet."));
	}
	Quote = ComputePrices(*Store, Request, GetEffectiveStoredDiscount());
	if (!Quote.IsSuccess())
	{
		return Quote;
	}
	if (Quote.Net < 0 && !OutWallet->CanSpendCoins(-Quote.Net))
	{
		return MakeTradeFailure(Quote, ENpcTradeResult::NotEnoughCoins,
			FString::Printf(TEXT("The trade costs %d coins; the wallet holds %d."), -Quote.Net, OutWallet->GetCoins()));
	}
	if (Quote.Net > 0 && static_cast<int64>(OutWallet->GetCoins()) + Quote.Net > MAX_int32)
	{
		return MakeTradeFailure(Quote, ENpcTradeResult::InvalidQuantity, TEXT("The wallet cannot hold the coins this sale pays."));
	}
	OutSell = NormalizeLines(Request.Sell);
	OutBuy = NormalizeLines(Request.Buy);
	const FInventoryTransactionResult Preflight = OutInventory->CanApplyDefinitionTransaction(OutSell, OutBuy);
	if (!Preflight.IsSuccess())
	{
		return MakeTradeFailure(Quote, MapInventoryResult(Preflight.Result), TEXT("The inventory cannot apply the trade."), Preflight.FailedIndex);
	}
	return Quote;
}

FNpcTradeQuote UNpcTraderComponent::QuoteTrade(AActor* Trader, const FNpcTradeRequest& Request) const
{
	UInventoryComponent* Inventory = nullptr;
	UWalletComponent* Wallet = nullptr;
	TArray<FInventoryDefinitionQuantity> Sell;
	TArray<FInventoryDefinitionQuantity> Buy;
	FNpcTradeQuote Quote = BuildQuote(Trader, Request, Inventory, Wallet, Sell, Buy);
	if (Quote.IsSuccess() && bTradeInProgress)
	{
		Quote.Result = ENpcTradeResult::TradeInProgress;
	}
	return Quote;
}

FNpcTradeQuote UNpcTraderComponent::TryTrade(AActor* Trader, const FNpcTradeRequest& Request)
{
	if (bTradeInProgress)
	{
		FNpcTradeQuote Nested;
		Nested.Result = ENpcTradeResult::TradeInProgress;
		Nested.Diagnostic = TEXT("A trade is already running on this NPC.");
		return Nested;
	}
	TGuardValue<bool> Guard(bTradeInProgress, true);
	RefreshDay();
	UInventoryComponent* Inventory = nullptr;
	UWalletComponent* Wallet = nullptr;
	TArray<FInventoryDefinitionQuantity> Sell;
	TArray<FInventoryDefinitionQuantity> Buy;
	FNpcTradeQuote Quote = BuildQuote(Trader, Request, Inventory, Wallet, Sell, Buy);
	if (Quote.IsSuccess())
	{
		// The commit point: sold items leave and bought items arrive together, or nothing changes.
		const FInventoryTransactionResult Committed = Inventory->TryApplyDefinitionTransaction(Sell, Buy);
		if (!Committed.IsSuccess())
		{
			Quote = MakeTradeFailure(Quote, MapInventoryResult(Committed.Result), TEXT("The inventory refused the trade."), Committed.FailedIndex);
		}
		else
		{
			// Checked above under the guard and the wallet has no other writer here, so neither call can fail.
			const bool bPaid = Quote.Net >= 0 ? Wallet->AddCoins(Quote.Net) : Wallet->SpendCoins(-Quote.Net);
			ensureMsgf(bPaid, TEXT("Wallet change after a committed trade failed (%d coins)."), Quote.Net);
			if (Quote.Discount > 0)
			{
				// A trade that used the haggled discount spends one of today's attempts.
				StoredDiscountPercent = 0;
				++AttemptsUsed;
			}
		}
	}
	OnTradeResult.Broadcast(Trader, Quote);
	if (Quote.IsSuccess() && Quote.Discount > 0)
	{
		OnHaggleChanged.Broadcast(ENpcHaggleResult::NoProblem);
	}
	return Quote;
}

ENpcHaggleResult UNpcTraderComponent::StartHaggle(AActor* Trader)
{
	if (!GetPartner(Trader))
	{
		return ENpcHaggleResult::NotTalking;
	}
	const UNpcDefinition* Store = GetStore();
	if (!Store)
	{
		return ENpcHaggleResult::NoStore;
	}
	RefreshDay();
	if (bProblemActive)
	{
		return ENpcHaggleResult::ProblemActive;
	}
	if (StoredDiscountPercent > 0)
	{
		return ENpcHaggleResult::DiscountStored;
	}
	if (AttemptsUsed >= Store->HaggleAttemptsPerDay)
	{
		return ENpcHaggleResult::NoAttemptsLeft;
	}
	MakeProblem(HaggleStream, ProblemText, ProblemAnswer);
	bProblemActive = true;
	ProblemDay = HaggleDay;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ProblemTimerHandle, this, &UNpcTraderComponent::ExpireHaggleProblem, Store->HaggleTimeLimit, false);
	}
	OnHaggleChanged.Broadcast(ENpcHaggleResult::Started);
	return ENpcHaggleResult::Started;
}

ENpcHaggleResult UNpcTraderComponent::SubmitHaggleAnswer(AActor* Trader, const int32 Answer)
{
	if (!GetPartner(Trader))
	{
		return ENpcHaggleResult::NotTalking;
	}
	const UNpcDefinition* Store = GetStore();
	if (!bProblemActive || !Store)
	{
		return ENpcHaggleResult::NoProblem;
	}
	const bool bCorrect = Answer == ProblemAnswer;
	const bool bSameDay = ProblemDay == GetCurrentDay();
	ClearProblem();
	RefreshDay();
	// A problem from a previous day neither earns today's discount nor spends today's attempt.
	if (!bSameDay)
	{
		OnHaggleChanged.Broadcast(ENpcHaggleResult::TimedOut);
		return ENpcHaggleResult::TimedOut;
	}
	ENpcHaggleResult Result = ENpcHaggleResult::Wrong;
	if (bCorrect)
	{
		StoredDiscountPercent = HaggleStream.RandRange(Store->HaggleDiscountMinPercent, Store->HaggleDiscountMaxPercent);
		Result = ENpcHaggleResult::Correct;
	}
	else
	{
		++AttemptsUsed;
	}
	OnHaggleChanged.Broadcast(Result);
	return Result;
}

void UNpcTraderComponent::ExpireHaggleProblem()
{
	if (!bProblemActive)
	{
		return;
	}
	const bool bSameDay = ProblemDay == GetCurrentDay();
	ClearProblem();
	RefreshDay();
	if (bSameDay)
	{
		++AttemptsUsed;
	}
	OnHaggleChanged.Broadcast(ENpcHaggleResult::TimedOut);
}

void UNpcTraderComponent::ClearProblem()
{
	bProblemActive = false;
	ProblemAnswer = 0;
	ProblemText = FText::GetEmpty();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ProblemTimerHandle);
	}
}

FNpcHaggleState UNpcTraderComponent::GetHaggleState() const
{
	FNpcHaggleState State;
	const UNpcDefinition* Store = GetStore();
	if (!Store)
	{
		return State;
	}
	State.AttemptsLeft = FMath::Max(Store->HaggleAttemptsPerDay - GetEffectiveAttemptsUsed(), 0);
	State.StoredDiscountPercent = GetEffectiveStoredDiscount();
	State.bProblemActive = bProblemActive;
	State.ProblemText = bProblemActive ? ProblemText : FText::GetEmpty();
	const UWorld* World = GetWorld();
	State.TimeLeft = bProblemActive && World ? FMath::Max(World->GetTimerManager().GetTimerRemaining(ProblemTimerHandle), 0.f) : 0.f;
	return State;
}

#undef LOCTEXT_NAMESPACE
