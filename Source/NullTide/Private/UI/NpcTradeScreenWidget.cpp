#include "UI/NpcTradeScreenWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "Inventory/InventoryComponent.h"
#include "Items/Fragments/ItemFragment_Stackable.h"
#include "Items/Fragments/ItemFragment_TradeValue.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "Npc/NpcCharacter.h"
#include "Npc/NpcDefinition.h"
#include "Npc/NpcTraderComponent.h"
#include "Player/WalletComponent.h"
#include "TimerManager.h"
#include "UI/NpcTradeItemRowWidget.h"
#include "UI/TradeQuantityPickerWidget.h"
#include "World/GameClockSubsystem.h"

#define LOCTEXT_NAMESPACE "NpcTradeScreen"

namespace
{
/** Stack size of a stackable item, or 0 for a non-stackable one. */
int32 GetMaxStack(const TSubclassOf<UItemDefinition> Item)
{
	const UItemDefinition* Definition = Item ? Item->GetDefaultObject<UItemDefinition>() : nullptr;
	const UItemFragment_Stackable* Stackable = Definition
		? Cast<UItemFragment_Stackable>(Definition->FindFragmentByClass(UItemFragment_Stackable::StaticClass())) : nullptr;
	return Stackable ? Stackable->MaxStackSize : 0;
}

FText ItemName(const TSubclassOf<UItemDefinition> Item)
{
	const UItemDefinition* Definition = Item ? Item->GetDefaultObject<UItemDefinition>() : nullptr;
	return Definition ? Definition->ItemName : FText::GetEmpty();
}

FText UnitPrice(const int32 Value, const float Rate)
{
	FNumberFormattingOptions Options;
	Options.MaximumFractionalDigits = 2;
	return FText::Format(LOCTEXT("UnitPrice", "{0} ea"), FText::AsNumber(Value * Rate, &Options));
}

FText LinePrice(const int32 Value, const float Rate)
{
	FNumberFormattingOptions Options;
	Options.MaximumFractionalDigits = 2;
	return FText::Format(LOCTEXT("LinePrice", "{0}"), FText::AsNumber(Value * Rate, &Options));
}

int32 BasketQuantity(const TArray<FInventoryDefinitionQuantity>& Basket, const TSubclassOf<UItemDefinition> Item)
{
	const FInventoryDefinitionQuantity* Line = Basket.FindByPredicate(
		[Item](const FInventoryDefinitionQuantity& Entry) { return Entry.DefinitionClass == Item; });
	return Line ? Line->Quantity : 0;
}
}

void UNpcTradeScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (TradeButton)
	{
		TradeButton->OnClicked.AddUniqueDynamic(this, &UNpcTradeScreenWidget::HandleTradeClicked);
	}
	if (ClearButton)
	{
		ClearButton->OnClicked.AddUniqueDynamic(this, &UNpcTradeScreenWidget::HandleClearClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked.AddUniqueDynamic(this, &UNpcTradeScreenWidget::HandleBackClicked);
	}
	if (HaggleButton)
	{
		HaggleButton->OnClicked.AddUniqueDynamic(this, &UNpcTradeScreenWidget::HandleHaggleClicked);
	}
	if (HaggleSubmitButton)
	{
		HaggleSubmitButton->OnClicked.AddUniqueDynamic(this, &UNpcTradeScreenWidget::HandleHaggleSubmitClicked);
	}
}

void UNpcTradeScreenWidget::NativeDestruct()
{
	ReleaseNpc();
	Super::NativeDestruct();
}

bool UNpcTradeScreenWidget::HasRequiredWidgets() const
{
	return InventoryList && StoreList && SellList && BuyList && SellTotalText && BuyTotalText && DiscountText && NetText && CoinsText
		&& FeedbackText && TradeButton && ClearButton && BackButton && HaggleButton && HaggleProblemText && HaggleTimeText && HaggleAnswerBox
		&& HaggleSubmitButton && HaggleStatusText && QuantityPicker;
}

void UNpcTradeScreenWidget::OpenForNpc(ANpcCharacter* InNpc, APawn* InTrader)
{
	ReleaseNpc();
	// ReleaseNpc forgets the NPC; bind the new one.
	Npc = InNpc;
	TraderPawn = InTrader;
	Request = FNpcTradeRequest();
	LastActionText = FText::GetEmpty();
	if (UInventoryComponent* Inventory = GetInventory())
	{
		Inventory->OnInventoryChanged.AddUniqueDynamic(this, &UNpcTradeScreenWidget::HandleInventoryChanged);
		BoundInventory = Inventory;
	}
	if (UWalletComponent* Wallet = GetWallet())
	{
		Wallet->OnCoinsChanged.AddUniqueDynamic(this, &UNpcTradeScreenWidget::HandleCoinsChanged);
		BoundWallet = Wallet;
	}
	if (UNpcTraderComponent* Trader = GetTrader())
	{
		Trader->OnTradeResult.AddUniqueDynamic(this, &UNpcTradeScreenWidget::HandleTradeResult);
		Trader->OnHaggleChanged.AddUniqueDynamic(this, &UNpcTradeScreenWidget::HandleHaggleChanged);
		BoundTrader = Trader;
	}
	// A new day resets haggling (and may drop a stored discount): the totals must follow.
	if (UGameClockSubsystem* Clock = UGameClockSubsystem::Get(this))
	{
		Clock->OnPhaseChanged.AddUniqueDynamic(this, &UNpcTradeScreenWidget::HandlePhaseChanged);
		BoundClock = Clock;
	}
	if (TitleText && InNpc)
	{
		TitleText->SetText(FText::Format(LOCTEXT("Title", "Trading with {0}"), InNpc->GetNpcName()));
	}
	if (QuantityPicker)
	{
		QuantityPicker->Cancel();
	}
	Rebuild();
}

void UNpcTradeScreenWidget::ReleaseNpc()
{
	if (UInventoryComponent* Inventory = BoundInventory.Get())
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UNpcTradeScreenWidget::HandleInventoryChanged);
	}
	if (UWalletComponent* Wallet = BoundWallet.Get())
	{
		Wallet->OnCoinsChanged.RemoveDynamic(this, &UNpcTradeScreenWidget::HandleCoinsChanged);
	}
	if (UNpcTraderComponent* Trader = BoundTrader.Get())
	{
		Trader->OnTradeResult.RemoveDynamic(this, &UNpcTradeScreenWidget::HandleTradeResult);
		Trader->OnHaggleChanged.RemoveDynamic(this, &UNpcTradeScreenWidget::HandleHaggleChanged);
	}
	if (UGameClockSubsystem* Clock = BoundClock.Get())
	{
		Clock->OnPhaseChanged.RemoveDynamic(this, &UNpcTradeScreenWidget::HandlePhaseChanged);
	}
	BoundInventory.Reset();
	BoundWallet.Reset();
	BoundTrader.Reset();
	BoundClock.Reset();
	Npc.Reset();
	TraderPawn.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CountdownTimer);
	}
}

UNpcTraderComponent* UNpcTradeScreenWidget::GetTrader() const
{
	const ANpcCharacter* TalkingNpc = Npc.Get();
	return TalkingNpc ? TalkingNpc->GetTrader() : nullptr;
}

UInventoryComponent* UNpcTradeScreenWidget::GetInventory() const
{
	const APawn* Pawn = TraderPawn.Get();
	return Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
}

UWalletComponent* UNpcTradeScreenWidget::GetWallet() const
{
	const APawn* Pawn = TraderPawn.Get();
	return Pawn ? Pawn->FindComponentByClass<UWalletComponent>() : nullptr;
}

void UNpcTradeScreenWidget::HandleRowClicked(UNpcTradeItemRowWidget& Row)
{
	// Copy first: adding or removing rebuilds the lists, this row included.
	const TSubclassOf<UItemDefinition> Item = Row.GetItem();
	const ENpcTradeRowSource Source = Row.GetSource();
	const int32 Available = Row.GetQuantity();
	switch (Source)
	{
	case ENpcTradeRowSource::SellBasket:
		Request.Sell.RemoveAll([Item](const FInventoryDefinitionQuantity& Line) { return Line.DefinitionClass == Item; });
		LastActionText = FText::GetEmpty();
		Rebuild();
		return;
	case ENpcTradeRowSource::BuyBasket:
		Request.Buy.RemoveAll([Item](const FInventoryDefinitionQuantity& Line) { return Line.DefinitionClass == Item; });
		LastActionText = FText::GetEmpty();
		Rebuild();
		return;
	default:
		break;
	}
	const bool bSell = Source == ENpcTradeRowSource::Inventory;
	const int32 Max = bSell ? Available : GetMaxStack(Item);
	// A stackable item asks how many; a single item (or a non-stackable one) goes straight in.
	if (GetMaxStack(Item) > 0 && Max > 1 && QuantityPicker)
	{
		TWeakObjectPtr<UNpcTradeScreenWidget> WeakThis(this);
		QuantityPicker->Ask(ItemName(Item), Max, FOnTradeQuantityConfirmed::CreateLambda([WeakThis, bSell, Item](const int32 Quantity)
		{
			if (UNpcTradeScreenWidget* Screen = WeakThis.Get())
			{
				Screen->AddToBasket(bSell, Item, Quantity);
			}
		}));
		return;
	}
	AddToBasket(bSell, Item, 1);
}

bool UNpcTradeScreenWidget::AddToBasket(const bool bSell, const TSubclassOf<UItemDefinition> Item, const int32 Quantity)
{
	UNpcTraderComponent* Trader = GetTrader();
	const UNpcDefinition* Store = Trader ? Trader->GetStore() : nullptr;
	if (!Store || !Item || Quantity < 1)
	{
		return false;
	}
	TArray<FInventoryDefinitionQuantity>& Basket = bSell ? Request.Sell : Request.Buy;
	FInventoryDefinitionQuantity* Line = Basket.FindByPredicate([Item](const FInventoryDefinitionQuantity& Entry) { return Entry.DefinitionClass == Item; });
	if (!Line && Basket.Num() >= Store->MaxLinesPerSide)
	{
		SetFeedback(FText::Format(LOCTEXT("TooManyLines", "At most {0} item types per side."), FText::AsNumber(Store->MaxLinesPerSide)));
		return false;
	}
	if (Line)
	{
		Line->Quantity = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(Line->Quantity) + Quantity, MAX_int32));
	}
	else
	{
		FInventoryDefinitionQuantity& Added = Basket.AddDefaulted_GetRef();
		Added.DefinitionClass = Item;
		Added.Quantity = Quantity;
	}
	LastActionText = FText::GetEmpty();
	Rebuild();
	return true;
}

void UNpcTradeScreenWidget::ClampSellBasket()
{
	const UInventoryComponent* Inventory = GetInventory();
	for (int32 Index = Request.Sell.Num() - 1; Index >= 0; --Index)
	{
		const int32 Owned = Inventory ? Inventory->GetTotalQuantity(Request.Sell[Index].DefinitionClass) : 0;
		Request.Sell[Index].Quantity = FMath::Min(Request.Sell[Index].Quantity, Owned);
		if (Request.Sell[Index].Quantity < 1)
		{
			Request.Sell.RemoveAt(Index);
		}
	}
}

void UNpcTradeScreenWidget::AddRow(UPanelWidget* List, const TSubclassOf<UItemDefinition> Item, const ENpcTradeRowSource Source,
	const int32 Quantity, const FText& Price)
{
	if (!List || !RowClass)
	{
		return;
	}
	UNpcTradeItemRowWidget* Row = CreateWidget<UNpcTradeItemRowWidget>(this, RowClass);
	if (Row)
	{
		Row->Setup(this, Item, Source, Quantity, Price);
		List->AddChild(Row);
	}
}

void UNpcTradeScreenWidget::Rebuild()
{
	ClampSellBasket();
	const UNpcTraderComponent* Trader = GetTrader();
	const UNpcDefinition* Store = Trader ? Trader->GetStore() : nullptr;
	for (UPanelWidget* List : { InventoryList.Get(), StoreList.Get(), SellList.Get(), BuyList.Get() })
	{
		if (List)
		{
			List->ClearChildren();
		}
	}
	if (Store)
	{
		// Left: tradable stacks summed per item, in inventory order, less what is already in "You sell".
		TArray<TSubclassOf<UItemDefinition>> Seen;
		const UInventoryComponent* Inventory = GetInventory();
		for (const UItemInstance* Instance : Inventory ? Inventory->GetItemsSnapshot() : TArray<UItemInstance*>())
		{
			const TSubclassOf<UItemDefinition> Item = Instance ? Instance->GetDefinitionClass() : nullptr;
			const int32 Value = UItemFragment_TradeValue::GetTradeValue(Item);
			if (Value <= 0 || Seen.Contains(Item))
			{
				continue;
			}
			Seen.Add(Item);
			const int32 Available = Inventory->GetTotalQuantity(Item) - BasketQuantity(Request.Sell, Item);
			if (Available > 0)
			{
				AddRow(InventoryList, Item, ENpcTradeRowSource::Inventory, Available, UnitPrice(Value, Store->SellRate));
			}
		}
		for (const TSubclassOf<UItemDefinition>& Item : Store->StoreItems)
		{
			AddRow(StoreList, Item, ENpcTradeRowSource::Store, 0, UnitPrice(UItemFragment_TradeValue::GetTradeValue(Item), Store->BuyRate));
		}
		for (const FInventoryDefinitionQuantity& Line : Request.Sell)
		{
			AddRow(SellList, Line.DefinitionClass, ENpcTradeRowSource::SellBasket, Line.Quantity,
				LinePrice(UItemFragment_TradeValue::GetTradeValue(Line.DefinitionClass) * Line.Quantity, Store->SellRate));
		}
		for (const FInventoryDefinitionQuantity& Line : Request.Buy)
		{
			AddRow(BuyList, Line.DefinitionClass, ENpcTradeRowSource::BuyBasket, Line.Quantity,
				LinePrice(UItemFragment_TradeValue::GetTradeValue(Line.DefinitionClass) * Line.Quantity, Store->BuyRate));
		}
	}
	RefreshQuote();
	RefreshHaggle();
}

void UNpcTradeScreenWidget::RefreshQuote()
{
	const UNpcTraderComponent* Trader = GetTrader();
	Quote = Trader ? Trader->QuoteTrade(TraderPawn.Get(), Request) : FNpcTradeQuote();
	const UWalletComponent* Wallet = GetWallet();
	if (SellTotalText)
	{
		SellTotalText->SetText(FText::Format(LOCTEXT("SellTotal", "You receive: {0}"), FormatCoins(Quote.SellTotal)));
	}
	if (BuyTotalText)
	{
		BuyTotalText->SetText(FText::Format(LOCTEXT("BuyTotal", "You pay: {0}"), FormatCoins(Quote.BuyTotal)));
	}
	if (DiscountText)
	{
		DiscountText->SetText(Quote.DiscountPercent > 0
			? FText::Format(LOCTEXT("Discount", "Haggled −{0}%: −{1}"), FText::AsNumber(Quote.DiscountPercent), FormatCoins(Quote.Discount))
			: FText::GetEmpty());
	}
	if (NetText)
	{
		NetText->SetText(Quote.Net >= 0 ? FText::Format(LOCTEXT("NetReceive", "Net: +{0}"), FormatCoins(Quote.Net))
			: FText::Format(LOCTEXT("NetPay", "Net: −{0}"), FormatCoins(-Quote.Net)));
	}
	if (CoinsText)
	{
		CoinsText->SetText(FText::Format(LOCTEXT("Wallet", "Wallet: {0}"), FormatCoins(Wallet ? Wallet->GetCoins() : 0)));
	}
	if (TradeButton)
	{
		TradeButton->SetIsEnabled(Quote.IsSuccess() && !bTradeRequestPending);
	}
	// The last action's message stays until the baskets change; otherwise show why TRADE is disabled.
	if (FeedbackText)
	{
		FeedbackText->SetText(!LastActionText.IsEmpty() ? LastActionText
			: (Quote.IsSuccess() ? FText::GetEmpty() : DescribeTradeResult(Quote.Result)));
	}
}

void UNpcTradeScreenWidget::RefreshHaggle()
{
	const UNpcTraderComponent* Trader = GetTrader();
	const FNpcHaggleState State = Trader ? Trader->GetHaggleState() : FNpcHaggleState();
	if (HaggleButton)
	{
		HaggleButton->SetIsEnabled(Trader && !State.bProblemActive && State.StoredDiscountPercent == 0 && State.AttemptsLeft > 0);
	}
	const ESlateVisibility ProblemVisibility = State.bProblemActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	for (UWidget* Widget : { static_cast<UWidget*>(HaggleProblemText), static_cast<UWidget*>(HaggleTimeText),
		static_cast<UWidget*>(HaggleAnswerBox), static_cast<UWidget*>(HaggleSubmitButton) })
	{
		if (Widget)
		{
			Widget->SetVisibility(ProblemVisibility);
		}
	}
	if (HaggleProblemText)
	{
		HaggleProblemText->SetText(State.ProblemText);
	}
	if (HaggleStatusText)
	{
		HaggleStatusText->SetText(DescribeHaggleState());
	}
	UpdateCountdown();
	UWorld* World = GetWorld();
	if (World && State.bProblemActive && !World->GetTimerManager().IsTimerActive(CountdownTimer))
	{
		World->GetTimerManager().SetTimer(CountdownTimer, this, &UNpcTradeScreenWidget::UpdateCountdown, 0.1f, true);
	}
	else if (World && !State.bProblemActive)
	{
		World->GetTimerManager().ClearTimer(CountdownTimer);
	}
	// Slate keeps focus on a collapsed widget: hand it back to the screen so I and Tab close it again.
	if (!State.bProblemActive && HaggleAnswerBox && HaggleAnswerBox->HasKeyboardFocus())
	{
		SetKeyboardFocus();
	}
}

void UNpcTradeScreenWidget::UpdateCountdown()
{
	const UNpcTraderComponent* Trader = GetTrader();
	const FNpcHaggleState State = Trader ? Trader->GetHaggleState() : FNpcHaggleState();
	if (HaggleTimeText)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 1;
		Options.MaximumFractionalDigits = 1;
		HaggleTimeText->SetText(FText::Format(LOCTEXT("TimeLeft", "{0} s"), FText::AsNumber(State.TimeLeft, &Options)));
	}
}

FText UNpcTradeScreenWidget::DescribeHaggleState() const
{
	const UNpcTraderComponent* Trader = GetTrader();
	const FNpcHaggleState State = Trader ? Trader->GetHaggleState() : FNpcHaggleState();
	if (State.StoredDiscountPercent > 0)
	{
		return FText::Format(LOCTEXT("HaggleStored", "Discount ready: −{0}% on your next purchase"), FText::AsNumber(State.StoredDiscountPercent));
	}
	if (State.bProblemActive)
	{
		return LOCTEXT("HaggleSolve", "Solve it to get a discount!");
	}
	if (State.AttemptsLeft <= 0)
	{
		return LOCTEXT("HaggleNone", "No haggles left today");
	}
	return FText::Format(LOCTEXT("HaggleLeft", "{0} haggle attempts left today"), FText::AsNumber(State.AttemptsLeft));
}

void UNpcTradeScreenWidget::SetFeedback(const FText& Text)
{
	LastActionText = Text;
	if (FeedbackText)
	{
		FeedbackText->SetText(Text);
	}
}

void UNpcTradeScreenWidget::RequestTrade()
{
	UNpcTraderComponent* Trader = GetTrader();
	if (!Trader || bTradeRequestPending)
	{
		return;
	}
	{
		TGuardValue<bool> Pending(bTradeRequestPending, true);
		// A copy: the inventory and result handlers rewrite Request while the trade runs. The message comes from OnTradeResult.
		const FNpcTradeRequest Sent = Request;
		Trader->TryTrade(TraderPawn.Get(), Sent);
	}
	// The result arrived while the request was pending; refresh TRADE now that it is not.
	RefreshQuote();
}

void UNpcTradeScreenWidget::HandleTradeResult(AActor* InTrader, const FNpcTradeQuote& Result)
{
	if (InTrader != TraderPawn.Get())
	{
		return;
	}
	if (Result.IsSuccess())
	{
		Request = FNpcTradeRequest();
		Rebuild();
		SetFeedback(Result.Net >= 0 ? FText::Format(LOCTEXT("TradedReceive", "Traded! +{0}"), FormatCoins(Result.Net))
			: FText::Format(LOCTEXT("TradedPay", "Traded! −{0}"), FormatCoins(-Result.Net)));
		return;
	}
	SetFeedback(DescribeTradeResult(Result.Result));
}

void UNpcTradeScreenWidget::ClearBaskets()
{
	Request = FNpcTradeRequest();
	LastActionText = FText::GetEmpty();
	Rebuild();
}

void UNpcTradeScreenWidget::RequestBack()
{
	if (ANpcCharacter* TalkingNpc = Npc.Get())
	{
		TalkingNpc->CloseTrade(true);
	}
}

void UNpcTradeScreenWidget::RequestClose()
{
	if (ANpcCharacter* TalkingNpc = Npc.Get())
	{
		TalkingNpc->CloseTrade(false);
	}
}

void UNpcTradeScreenWidget::RequestHaggle()
{
	UNpcTraderComponent* Trader = GetTrader();
	if (!Trader)
	{
		return;
	}
	const ENpcHaggleResult Result = Trader->StartHaggle(TraderPawn.Get());
	if (Result == ENpcHaggleResult::Started && HaggleAnswerBox)
	{
		HaggleAnswerBox->SetText(FText::GetEmpty());
		HaggleAnswerBox->SetKeyboardFocus();
	}
	RefreshHaggle();
}

void UNpcTradeScreenWidget::SubmitHaggle()
{
	UNpcTraderComponent* Trader = GetTrader();
	const FString Answer = HaggleAnswerBox ? HaggleAnswerBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (!Trader || !IsWholeNumber(Answer))
	{
		return;
	}
	Trader->SubmitHaggleAnswer(TraderPawn.Get(), FCString::Atoi(*Answer));
}

void UNpcTradeScreenWidget::HandleHaggleChanged(const ENpcHaggleResult Result)
{
	switch (Result)
	{
	case ENpcHaggleResult::Correct:
		SetFeedback(LOCTEXT("HaggleCorrect", "Correct! The Trader lowers the price."));
		break;
	case ENpcHaggleResult::Wrong:
		SetFeedback(LOCTEXT("HaggleWrong", "Wrong answer: no discount."));
		break;
	case ENpcHaggleResult::TimedOut:
		SetFeedback(LOCTEXT("HaggleTimedOut", "Time's up: no discount."));
		break;
	default:
		break;
	}
	// A stored discount changes the price.
	RefreshQuote();
	RefreshHaggle();
}

void UNpcTradeScreenWidget::HandleInventoryChanged(int32 NewRevision)
{
	Rebuild();
}

void UNpcTradeScreenWidget::HandleCoinsChanged(int32 NewCoins)
{
	RefreshQuote();
}

void UNpcTradeScreenWidget::HandlePhaseChanged(int32 Day, bool bNight)
{
	RefreshQuote();
	RefreshHaggle();
}

void UNpcTradeScreenWidget::HandleTradeClicked()
{
	RequestTrade();
}

void UNpcTradeScreenWidget::HandleClearClicked()
{
	ClearBaskets();
}

void UNpcTradeScreenWidget::HandleBackClicked()
{
	RequestBack();
}

void UNpcTradeScreenWidget::HandleHaggleClicked()
{
	RequestHaggle();
}

void UNpcTradeScreenWidget::HandleHaggleSubmitClicked()
{
	SubmitHaggle();
}

bool UNpcTradeScreenWidget::IsWholeNumber(const FString& Text)
{
	// Digits with an optional leading minus, at most 9 digits: "12.9" or "1e3" never count as 12 or 1.
	const int32 Start = Text.StartsWith(TEXT("-")) ? 1 : 0;
	const int32 Digits = Text.Len() - Start;
	if (Digits < 1 || Digits > 9)
	{
		return false;
	}
	for (int32 Index = Start; Index < Text.Len(); ++Index)
	{
		if (!FChar::IsDigit(Text[Index]))
		{
			return false;
		}
	}
	return true;
}

bool UNpcTradeScreenWidget::IsCloseKey(const FKey& Key)
{
	return Key == EKeys::I || Key == EKeys::Tab || Key == EKeys::Escape;
}

FReply UNpcTradeScreenWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	// While the answer box has focus only Esc closes, so typing never closes the screen; Enter submits.
	const bool bTyping = HaggleAnswerBox && HaggleAnswerBox->HasKeyboardFocus();
	if (bTyping && Key == EKeys::Enter)
	{
		SubmitHaggle();
		return FReply::Handled();
	}
	if (IsCloseKey(Key) && (!bTyping || Key == EKeys::Escape))
	{
		RequestClose();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

TArray<UNpcTradeItemRowWidget*> UNpcTradeScreenWidget::GetRows(const ENpcTradeRowSource Source) const
{
	const UPanelWidget* List = Source == ENpcTradeRowSource::Inventory ? InventoryList.Get()
		: Source == ENpcTradeRowSource::Store ? StoreList.Get()
		: Source == ENpcTradeRowSource::SellBasket ? SellList.Get() : BuyList.Get();
	TArray<UNpcTradeItemRowWidget*> Rows;
	if (List)
	{
		for (UWidget* Child : List->GetAllChildren())
		{
			if (UNpcTradeItemRowWidget* Row = Cast<UNpcTradeItemRowWidget>(Child))
			{
				Rows.Add(Row);
			}
		}
	}
	return Rows;
}

FText UNpcTradeScreenWidget::GetFeedbackText() const
{
	return FeedbackText ? FeedbackText->GetText() : FText::GetEmpty();
}

FText UNpcTradeScreenWidget::GetHaggleStatusText() const
{
	return HaggleStatusText ? HaggleStatusText->GetText() : FText::GetEmpty();
}

FText UNpcTradeScreenWidget::GetHaggleProblemText() const
{
	return HaggleProblemText ? HaggleProblemText->GetText() : FText::GetEmpty();
}

FText UNpcTradeScreenWidget::GetNetText() const
{
	return NetText ? NetText->GetText() : FText::GetEmpty();
}

bool UNpcTradeScreenWidget::IsTradeEnabled() const
{
	return TradeButton && TradeButton->GetIsEnabled();
}

bool UNpcTradeScreenWidget::IsHaggleEnabled() const
{
	return HaggleButton && HaggleButton->GetIsEnabled();
}

FText UNpcTradeScreenWidget::FormatCoins(const int32 Coins)
{
	return FText::Format(LOCTEXT("Coins", "{0} coins"), FText::AsNumber(Coins));
}

FText UNpcTradeScreenWidget::DescribeTradeResult(const ENpcTradeResult Result)
{
	switch (Result)
	{
	case ENpcTradeResult::Success:
		return LOCTEXT("ResultSuccess", "Traded!");
	case ENpcTradeResult::EmptyTrade:
		return LOCTEXT("ResultEmpty", "Pick items to sell or buy.");
	case ENpcTradeResult::TooManyLines:
		return LOCTEXT("ResultTooMany", "Too many item types on one side.");
	case ENpcTradeResult::NotTradable:
		return LOCTEXT("ResultNotTradable", "The Trader does not want that item.");
	case ENpcTradeResult::NotInStore:
		return LOCTEXT("ResultNotInStore", "Not sold here.");
	case ENpcTradeResult::NotEnoughCoins:
		return LOCTEXT("ResultCoins", "Not enough coins.");
	case ENpcTradeResult::InsufficientItems:
		return LOCTEXT("ResultItems", "You don't have those items any more.");
	case ENpcTradeResult::InventoryFull:
		return LOCTEXT("ResultFull", "Inventory full.");
	case ENpcTradeResult::InventoryBusy:
	case ENpcTradeResult::TradeInProgress:
		return LOCTEXT("ResultBusy", "Busy, try again.");
	case ENpcTradeResult::InvalidQuantity:
		return LOCTEXT("ResultQuantity", "Invalid amount.");
	default:
		return LOCTEXT("ResultUnavailable", "Trading is not available.");
	}
}

#undef LOCTEXT_NAMESPACE
