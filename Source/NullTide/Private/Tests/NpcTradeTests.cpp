#include "NpcTestTypes.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/InventoryComponent.h"
#include "Items/Fragments/ItemFragment_Stackable.h"
#include "Items/Fragments/ItemFragment_TradeValue.h"
#include "Misc/AutomationTest.h"
#include "Npc/NpcTraderComponent.h"
#include "UI/NpcTradeItemRowWidget.h"
#include "UI/TradeQuantityPickerWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "World/GameClockSubsystem.h"

namespace
{
UItemFragment_TradeValue* S5AddValue(UItemDefinition& Item, const int32 Value)
{
	UItemFragment_TradeValue* Fragment = Item.CreateDefaultSubobject<UItemFragment_TradeValue>(TEXT("TradeValue"));
	Fragment->Value = Value;
	Item.ItemFragments.Add(Fragment);
	return Fragment;
}

void S5AddStack(UItemDefinition& Item)
{
	UItemFragment_Stackable* Stackable = Item.CreateDefaultSubobject<UItemFragment_Stackable>(TEXT("Stackable"));
	Stackable->MaxStackSize = 50;
	Item.ItemFragments.Add(Stackable);
}
}

UTradeTestItemA::UTradeTestItemA()
{
	ItemName = FText::FromString(TEXT("Item A"));
	S5AddStack(*this);
	S5AddValue(*this, 4);
}

UTradeTestItemB::UTradeTestItemB()
{
	ItemName = FText::FromString(TEXT("Item B"));
	S5AddValue(*this, 10);
}

UTradeTestItemC::UTradeTestItemC()
{
	ItemName = FText::FromString(TEXT("Item C"));
	S5AddStack(*this);
	S5AddValue(*this, 3);
}

UTradeTestItemNoValue::UTradeTestItemNoValue()
{
	ItemName = FText::FromString(TEXT("No Value"));
	S5AddStack(*this);
}

UNpcStoreTestDefinition::UNpcStoreTestDefinition()
{
	StoreItems = { UTradeTestItemA::StaticClass(), UTradeTestItemB::StaticClass() };
}

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
const TCHAR* S5TalkClassPath = TEXT("/Game/LevelPrototyping/NpcSystem/UI/W_NpcTalk.W_NpcTalk_C");
const TCHAR* S5TradeClassPath = TEXT("/Game/LevelPrototyping/NpcSystem/UI/W_NpcTrade.W_NpcTrade_C");
const TCHAR* S5RowClassPath = TEXT("/Game/LevelPrototyping/NpcSystem/UI/W_NpcTradeItemRow.W_NpcTradeItemRow_C");
const TCHAR* S5PickerClassPath = TEXT("/Game/LevelPrototyping/NpcSystem/UI/W_TradeQuantityPicker.W_TradeQuantityPicker_C");
const TCHAR* S5StatusClassPath = TEXT("/Game/LevelPrototyping/BaseSystem/UI/W_PlayerStatus.W_PlayerStatus_C");
const TCHAR* S5TraderClassPath = TEXT("/Game/LevelPrototyping/NpcSystem/BP_Npc_Trader.BP_Npc_Trader_C");
const TCHAR* S5TraderDefinitionPath = TEXT("/Game/LevelPrototyping/NpcSystem/Definitions/NPC_Trader.NPC_Trader_C");
const TCHAR* S5PlayerPath = TEXT("/Game/TopDown/Blueprints/BP_TopDownCharacter.BP_TopDownCharacter");

FInventoryDefinitionQuantity S5Line(const TSubclassOf<UItemDefinition> Item, const int32 Quantity)
{
	FInventoryDefinitionQuantity Line;
	Line.DefinitionClass = Item;
	Line.Quantity = Quantity;
	return Line;
}

FNpcTradeRequest S5Request(const TArray<FInventoryDefinitionQuantity>& Sell, const TArray<FInventoryDefinitionQuantity>& Buy)
{
	FNpcTradeRequest Request;
	Request.Sell = Sell;
	Request.Buy = Buy;
	return Request;
}

FString S5Name(const ENpcTradeResult Result)
{
	return StaticEnum<ENpcTradeResult>()->GetNameStringByValue(static_cast<int64>(Result));
}

FString S5HaggleName(const ENpcHaggleResult Result)
{
	return StaticEnum<ENpcHaggleResult>()->GetNameStringByValue(static_cast<int64>(Result));
}

const TSubclassOf<UItemDefinition> S5A = UTradeTestItemA::StaticClass();
const TSubclassOf<UItemDefinition> S5B = UTradeTestItemB::StaticClass();
const TSubclassOf<UItemDefinition> S5C = UTradeTestItemC::StaticClass();
const TSubclassOf<UItemDefinition> S5NoValue = UTradeTestItemNoValue::StaticClass();

struct FS5Player
{
	ACharacter* Pawn = nullptr;
	APlayerController* Controller = nullptr;
	UInventoryComponent* Inventory = nullptr;
	UWalletTestComponent* Wallet = nullptr;
};

/** A transient world that never begins play: timers never fire, the clock stays on day 1 until advanced. */
struct FS5World
{
	UWorld* World = nullptr;
	TStrongObjectPtr<UNpcTradeTestListener> Listener;

	FS5World()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (World)
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
		Listener.Reset(NewObject<UNpcTradeTestListener>());
	}

	~FS5World()
	{
		if (World)
		{
			for (AActor* Actor : FActorRange(World))
			{
				if (Actor)
				{
					Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
				}
			}
			GEngine->ShutdownWorldNetDriver(World);
			World->DestroyWorld(true);
			GEngine->DestroyWorldContext(World);
		}
	}

	FS5Player SpawnPlayer(const FVector& Location, const int32 Coins, const bool bPossess = true, const int32 StartingCoins = 0) const
	{
		FS5Player Player;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Player.Pawn = World->SpawnActor<ACharacter>(ACharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
		if (!Player.Pawn)
		{
			return Player;
		}
		Player.Inventory = NewObject<UInventoryComponent>(Player.Pawn);
		Player.Pawn->AddInstanceComponent(Player.Inventory);
		Player.Inventory->RegisterComponent();
		Player.Wallet = NewObject<UWalletTestComponent>(Player.Pawn);
		Player.Wallet->StartingCoins = StartingCoins;
		Player.Pawn->AddInstanceComponent(Player.Wallet);
		Player.Wallet->RegisterComponent();
		Player.Pawn->DispatchBeginPlay();
		if (Coins > 0)
		{
			Player.Wallet->TestSetCoins(Coins);
		}
		if (bPossess)
		{
			Player.Controller = World->SpawnActor<APlayerController>(APlayerController::StaticClass(), FTransform::Identity, Params);
			Player.Controller->Possess(Player.Pawn);
		}
		return Player;
	}

	ANpcTestCharacter* SpawnNpc(const FVector& Location, const TSubclassOf<UNpcDefinition> Definition) const
	{
		const FTransform Transform(Location);
		ANpcTestCharacter* Npc = World->SpawnActorDeferred<ANpcTestCharacter>(ANpcTestCharacter::StaticClass(), Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Npc)
		{
			Npc->InitializeResident(Definition, nullptr);
			Npc->FinishSpawning(Transform);
			Npc->SetTalkScreenClass(LoadClass<UNpcTalkWidget>(nullptr, S5TalkClassPath));
			Npc->DispatchBeginPlay();
		}
		return Npc;
	}

	void Listen(const FS5Player& Player, UNpcTraderComponent& Trader) const
	{
		Trader.OnTradeResult.AddDynamic(Listener.Get(), &UNpcTradeTestListener::HandleTradeResult);
		Trader.OnHaggleChanged.AddDynamic(Listener.Get(), &UNpcTradeTestListener::HandleHaggleChanged);
		Player.Inventory->OnInventoryChanged.AddDynamic(Listener.Get(), &UNpcTradeTestListener::HandleInventoryChanged);
		Player.Wallet->OnCoinsChanged.AddDynamic(Listener.Get(), &UNpcTradeTestListener::HandleCoinsChanged);
	}
};

void S5SetCapacity(UInventoryComponent& Inventory, const int32 Capacity)
{
	FIntProperty* Property = FindFProperty<FIntProperty>(UInventoryComponent::StaticClass(), TEXT("Capacity"));
	check(Property);
	Property->SetPropertyValue_InContainer(&Inventory, Capacity);
}

/** The first problem and discount a trader seeded with Seed will make. */
void S5Predict(const int32 Seed, FText& OutText, int32& OutAnswer, int32& OutDiscount)
{
	FRandomStream Stream(Seed);
	UNpcTraderComponent::MakeProblem(Stream, OutText, OutAnswer);
	OutDiscount = Stream.RandRange(5, 20);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWorldS5GameClockTest,
	"NullTide.World.S5.GameClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldS5GameClockTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("A 20-minute day and a 10-minute night"), FMath::IsNearlyEqual(UGameClockSubsystem::DayLengthSeconds, 1200.f)
		&& FMath::IsNearlyEqual(UGameClockSubsystem::NightLengthSeconds, 600.f));
	FS5World Fixture;
	UGameClockSubsystem* Clock = Fixture.World ? Fixture.World->GetSubsystem<UGameClockSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("Clock"), Clock))
	{
		return false;
	}
	TStrongObjectPtr<UGameClockTestListener> Listener(NewObject<UGameClockTestListener>());
	Clock->OnNewDay.AddDynamic(Listener.Get(), &UGameClockTestListener::HandleNewDay);
	Clock->OnPhaseChanged.AddDynamic(Listener.Get(), &UGameClockTestListener::HandlePhaseChanged);
	TestTrue(TEXT("Day 1, daytime"), Clock->GetDay() == 1 && !Clock->IsNight());
	Clock->StartClock();
	Clock->StartClock();
	TestTrue(TEXT("Starting twice is harmless; progress starts at 0"), FMath::IsNearlyZero(Clock->GetPhaseProgress01()));

	Clock->AdvancePhase();
	TestTrue(TEXT("Day 1 ends: night of day 1, no new day"), Clock->GetDay() == 1 && Clock->IsNight() && Listener->NewDays.IsEmpty()
		&& Listener->Phases.Num() == 1 && Listener->Phases[0] == FIntPoint(1, 1));
	Clock->AdvancePhase();
	TestTrue(TEXT("The night ends: day 2 begins once"), Clock->GetDay() == 2 && !Clock->IsNight() && Listener->NewDays.Num() == 1
		&& Listener->NewDays[0] == 2 && Listener->Phases.Num() == 2 && Listener->Phases[1] == FIntPoint(2, 0));
	Clock->AdvanceToNextDay();
	TestTrue(TEXT("From daytime, the next day passes through the night"), Clock->GetDay() == 3 && !Clock->IsNight()
		&& Listener->NewDays.Num() == 2 && Listener->Phases.Num() == 4);
	Clock->AdvancePhase();
	Clock->AdvanceToNextDay();
	TestTrue(TEXT("From the night, only the morning remains"), Clock->GetDay() == 4 && !Clock->IsNight() && Listener->NewDays.Num() == 3
		&& Listener->Phases.Num() == 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNpcS5WalletTest,
	"NullTide.Npc.S5.Wallet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNpcS5WalletTest::RunTest(const FString& Parameters)
{
	FS5World Fixture;
	const FS5Player Player = Fixture.SpawnPlayer(FVector(0.0, 0.0, 100.0), 0, false, 50);
	if (!TestTrue(TEXT("Player with a wallet"), Player.Pawn && Player.Wallet))
	{
		return false;
	}
	TestEqual(TEXT("Starting coins apply on BeginPlay"), Player.Wallet->GetCoins(), 50);
	Player.Wallet->OnCoinsChanged.AddDynamic(Fixture.Listener.Get(), &UNpcTradeTestListener::HandleCoinsChanged);
	TestTrue(TEXT("Add 10"), Player.Wallet->TestAdd(10) && Player.Wallet->GetCoins() == 60 && Fixture.Listener->CoinEvents == 1);
	TestTrue(TEXT("Adding 0 succeeds without an event"), Player.Wallet->TestAdd(0) && Fixture.Listener->CoinEvents == 1);
	TestTrue(TEXT("A negative amount is refused"), !Player.Wallet->TestAdd(-1) && !Player.Wallet->TestSpend(-1) && Player.Wallet->GetCoins() == 60);
	TestTrue(TEXT("Spending more than held is refused, unchanged"), !Player.Wallet->TestSpend(61) && Player.Wallet->GetCoins() == 60
		&& Fixture.Listener->CoinEvents == 1 && !Player.Wallet->CanSpendCoins(61) && Player.Wallet->CanSpendCoins(60));
	TestTrue(TEXT("Spend all: never negative"), Player.Wallet->TestSpend(60) && Player.Wallet->GetCoins() == 0 && Fixture.Listener->CoinEvents == 2);
	Player.Wallet->TestSetCoins(MAX_int32 - 5);
	TestTrue(TEXT("Adding clamps at the int32 maximum"), Player.Wallet->TestAdd(10) && Player.Wallet->GetCoins() == MAX_int32);

	// The status HUD follows the wallet and the clock for the local player.
	UClass* StatusClass = LoadClass<UPlayerStatusWidget>(nullptr, S5StatusClassPath);
	if (!TestNotNull(TEXT("W_PlayerStatus loads"), StatusClass))
	{
		return false;
	}
	const FS5Player Hud = Fixture.SpawnPlayer(FVector(500.0, 0.0, 100.0), 0, false, 0);
	UPlayerStatusHudTestComponent* Status = NewObject<UPlayerStatusHudTestComponent>(Hud.Pawn);
	Status->TestWidgetClass = StatusClass;
	Hud.Pawn->AddInstanceComponent(Status);
	// The pawn has begun play, so registering begins the component's play too.
	Status->RegisterComponent();
	TestNull(TEXT("No widget without a controller"), Status->GetStatusWidget());
	FActorSpawnParameters Params;
	APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>(APlayerController::StaticClass(), FTransform::Identity, Params);
	Hud.Wallet->TestSetCoins(120);
	Controller->Possess(Hud.Pawn);
	UPlayerStatusWidget* Widget = Status->GetStatusWidget();
	TestTrue(TEXT("Possessed by a local player: coins and day shown"), Widget && Widget->GetCoinsText().ToString() == TEXT("120 coins")
		&& Widget->GetDayText().ToString() == UPlayerStatusWidget::FormatDay(1, false).ToString());
	Hud.Wallet->TestAdd(5);
	TestTrue(TEXT("Coins refresh on change"), Widget && Widget->GetCoinsText().ToString() == TEXT("125 coins"));
	Fixture.World->GetSubsystem<UGameClockSubsystem>()->AdvancePhase();
	TestTrue(TEXT("The night shows"), Widget && Widget->GetDayText().ToString() == UPlayerStatusWidget::FormatDay(1, true).ToString());
	Controller->UnPossess();
	TestNull(TEXT("Unpossessed: the widget goes away"), Status->GetStatusWidget());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNpcS5TradeDataTest,
	"NullTide.Npc.S5.TradeData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNpcS5TradeDataTest::RunTest(const FString& Parameters)
{
	FString Diagnostic;
	UItemFragment_TradeValue* Fragment = NewObject<UItemFragment_TradeValue>();
	Fragment->Value = 0;
	TestFalse(TEXT("A zero value is refused"), Fragment->ValidateTemplate(Diagnostic));
	TestTrue(TEXT("Trade values: A 4, B 10, C 3, none 0, null 0, abstract 0"),
		UItemFragment_TradeValue::GetTradeValue(S5A) == 4 && UItemFragment_TradeValue::GetTradeValue(S5B) == 10
		&& UItemFragment_TradeValue::GetTradeValue(S5C) == 3 && UItemFragment_TradeValue::GetTradeValue(S5NoValue) == 0
		&& UItemFragment_TradeValue::GetTradeValue(nullptr) == 0 && UItemFragment_TradeValue::GetTradeValue(UItemDefinition::StaticClass()) == 0);

	TestTrue(TEXT("The test store is valid"), GetDefault<UNpcStoreTestDefinition>()->ValidateNpc(Diagnostic));
	TestTrue(TEXT("An NPC without a catalog has no store and is valid"), !GetDefault<UNpcTestDefinition>()->HasStore()
		&& GetDefault<UNpcTestDefinition>()->ValidateNpc(Diagnostic));
	struct FCase { const TCHAR* Name; TFunction<void(UNpcDefinition&)> Break; };
	const FCase Cases[] = {
		{ TEXT("A catalog item without a value"), [](UNpcDefinition& D) { D.StoreItems.Add(S5NoValue); } },
		{ TEXT("A repeated catalog item"), [](UNpcDefinition& D) { D.StoreItems.Add(S5A); } },
		{ TEXT("Sell rate 0"), [](UNpcDefinition& D) { D.SellRate = 0.f; } },
		{ TEXT("Sell rate above 1"), [](UNpcDefinition& D) { D.SellRate = 1.5f; } },
		{ TEXT("Buy rate 0"), [](UNpcDefinition& D) { D.BuyRate = 0.f; } },
		{ TEXT("No lines allowed"), [](UNpcDefinition& D) { D.MaxLinesPerSide = 0; } },
		{ TEXT("Negative attempts"), [](UNpcDefinition& D) { D.HaggleAttemptsPerDay = -1; } },
		{ TEXT("No time limit"), [](UNpcDefinition& D) { D.HaggleTimeLimit = 0.f; } },
		{ TEXT("Discount range reversed"), [](UNpcDefinition& D) { D.HaggleDiscountMinPercent = 30; } },
		{ TEXT("A 0% minimum discount (endless re-haggles)"), [](UNpcDefinition& D) { D.HaggleDiscountMinPercent = 0; } },
		{ TEXT("Discount above 100%"), [](UNpcDefinition& D) { D.HaggleDiscountMaxPercent = 120; } },
		{ TEXT("A resale profit (buy 90%, 20% off, sell 75%)"), [](UNpcDefinition& D) { D.BuyRate = 0.9f; } },
	};
	for (const FCase& Case : Cases)
	{
		UNpcStoreTestDefinition* Definition = NewObject<UNpcStoreTestDefinition>();
		Case.Break(*Definition);
		const bool bValid = Definition->ValidateNpc(Diagnostic);
		TestTrue(FString::Printf(TEXT("%s is refused (%s)"), Case.Name, *Diagnostic), !bValid && !Diagnostic.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNpcS5PricingTest,
	"NullTide.Npc.S5.Pricing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNpcS5PricingTest::RunTest(const FString& Parameters)
{
	const UNpcDefinition& Store = *GetDefault<UNpcStoreTestDefinition>();
	auto Expect = [this, &Store](const TCHAR* What, const FNpcTradeRequest& Request, const int32 Discount, const ENpcTradeResult Result,
		const int32 Sell, const int32 Buy, const int32 Off, const int32 Net)
	{
		const FNpcTradeQuote Quote = UNpcTraderComponent::ComputePrices(Store, Request, Discount);
		TestTrue(FString::Printf(TEXT("%s: %s sell %d buy %d off %d net %d (got %s %d %d %d %d)"), What, *S5Name(Result), Sell, Buy, Off, Net,
				*S5Name(Quote.Result), Quote.SellTotal, Quote.BuyTotal, Quote.Discount, Quote.Net),
			Quote.Result == Result && (Result != ENpcTradeResult::Success
				|| (Quote.SellTotal == Sell && Quote.BuyTotal == Buy && Quote.Discount == Off && Quote.Net == Net)));
	};
	Expect(TEXT("Sell 10 A (40 x 0.75)"), S5Request({ S5Line(S5A, 10) }, {}), 0, ENpcTradeResult::Success, 30, 0, 0, 30);
	Expect(TEXT("Buy 1 B"), S5Request({}, { S5Line(S5B, 1) }), 0, ENpcTradeResult::Success, 0, 10, 0, -10);
	Expect(TEXT("Mixed with 15% off: sell 3 C (9 -> 6), buy 5 A + 2 B (40, -6)"),
		S5Request({ S5Line(S5C, 3) }, { S5Line(S5A, 5), S5Line(S5B, 2) }), 15, ENpcTradeResult::Success, 6, 40, 6, -28);
	Expect(TEXT("A cheap sale rounds down (3 x 0.75)"), S5Request({ S5Line(S5C, 1) }, {}), 0, ENpcTradeResult::Success, 2, 0, 0, 2);
	Expect(TEXT("Lines of one item are summed (2 + 3 A)"), S5Request({ S5Line(S5A, 2), S5Line(S5A, 3) }, {}), 0, ENpcTradeResult::Success,
		15, 0, 0, 15);
	Expect(TEXT("The discount rounds down (10 x 19%)"), S5Request({}, { S5Line(S5B, 1) }), 19, ENpcTradeResult::Success, 0, 10, 1, -9);
	Expect(TEXT("Empty"), S5Request({}, {}), 0, ENpcTradeResult::EmptyTrade, 0, 0, 0, 0);
	Expect(TEXT("Selling an item without a value"), S5Request({ S5Line(S5NoValue, 1) }, {}), 0, ENpcTradeResult::NotTradable, 0, 0, 0, 0);
	Expect(TEXT("Buying an item not in the store"), S5Request({}, { S5Line(S5C, 1) }), 0, ENpcTradeResult::NotInStore, 0, 0, 0, 0);
	Expect(TEXT("A zero quantity"), S5Request({ S5Line(S5A, 0) }, {}), 0, ENpcTradeResult::InvalidQuantity, 0, 0, 0, 0);
	Expect(TEXT("A negative line cannot cancel another (-5 + 10 A)"), S5Request({ S5Line(S5A, -5), S5Line(S5A, 10) }, {}), 0,
		ENpcTradeResult::InvalidQuantity, 0, 0, 0, 0);
	Expect(TEXT("A trade worth more coins than a wallet holds is refused, not clamped"), S5Request({}, { S5Line(S5A, MAX_int32) }), 0,
		ENpcTradeResult::InvalidQuantity, 0, 0, 0, 0);
	UNpcStoreTestDefinition* OneLine = NewObject<UNpcStoreTestDefinition>();
	OneLine->MaxLinesPerSide = 1;
	const FNpcTradeQuote TooMany = UNpcTraderComponent::ComputePrices(*OneLine, S5Request({ S5Line(S5A, 1), S5Line(S5C, 1) }, {}), 0);
	TestEqual(TEXT("More item types than allowed on one side"), TooMany.Result, ENpcTradeResult::TooManyLines);
	TestEqual(TEXT("The default limit is 16 types per side"), Store.MaxLinesPerSide, 16);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNpcS5TradeTransactionTest,
	"NullTide.Npc.S5.TradeTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNpcS5TradeTransactionTest::RunTest(const FString& Parameters)
{
	FS5World Fixture;
	ANpcTestCharacter* Npc = Fixture.SpawnNpc(FVector(0.0, 0.0, 100.0), UNpcStoreTestDefinition::StaticClass());
	const FS5Player Player = Fixture.SpawnPlayer(FVector(-200.0, 0.0, 100.0), 100);
	const FS5Player Other = Fixture.SpawnPlayer(FVector(200.0, 0.0, 100.0), 100);
	UNpcTraderComponent* Trader = Npc ? Npc->GetTrader() : nullptr;
	if (!TestTrue(TEXT("NPC with a trader, two players"), Trader && Player.Inventory && Other.Inventory && Trader->HasStore()))
	{
		return false;
	}
	Player.Inventory->TryAddDefinitionQuantity(S5A, 20);
	Player.Inventory->TryAddDefinitionQuantity(S5C, 5);
	Player.Inventory->TryAddDefinitionQuantity(S5NoValue, 1);
	Fixture.Listen(Player, *Trader);
	UNpcTradeTestListener* Listener = Fixture.Listener.Get();

	TestEqual(TEXT("Before the talk: NotTalking"), Trader->TryTrade(Player.Pawn, S5Request({ S5Line(S5A, 1) }, {})).Result, ENpcTradeResult::NotTalking);
	TestTrue(TEXT("Talk opens"), Npc->OpenTalkForInteractor(Player.Pawn));
	TestEqual(TEXT("Another pawn is not the partner"), Trader->TryTrade(Other.Pawn, S5Request({ S5Line(S5A, 1) }, {})).Result,
		ENpcTradeResult::NotTalking);

	auto Snapshot = [&Player, Listener]() { return FIntVector4(Player.Inventory->GetRevision(), Player.Wallet->GetCoins(), Listener->InventoryEvents, Listener->CoinEvents); };
	auto Expect = [this, Trader, &Player, Listener, &Snapshot](const TCHAR* What, const FNpcTradeRequest& Request, const ENpcTradeResult Expected,
		const int32 CoinDelta)
	{
		const FIntVector4 Before = Snapshot();
		const int32 ResultsBefore = Listener->Results.Num();
		const FNpcTradeQuote Quote = Trader->QuoteTrade(Player.Pawn, Request);
		const bool bQuoteChangedNothing = Snapshot() == Before && Listener->Results.Num() == ResultsBefore;
		const FNpcTradeQuote Done = Trader->TryTrade(Player.Pawn, Request);
		const FIntVector4 After = Snapshot();
		const bool bSuccess = Expected == ENpcTradeResult::Success;
		TestTrue(FString::Printf(TEXT("%s: %s (quote %s, trade %s), coins %+d"), What, *S5Name(Expected), *S5Name(Quote.Result), *S5Name(Done.Result),
				After.Y - Before.Y),
			Quote.Result == Expected && Done.Result == Expected && bQuoteChangedNothing && After.Y - Before.Y == CoinDelta
			&& After.X - Before.X == (bSuccess ? 1 : 0) && After.Z - Before.Z == (bSuccess ? 1 : 0) && After.W - Before.W == (bSuccess && CoinDelta != 0 ? 1 : 0)
			&& Listener->Results.Num() == ResultsBefore + 1 && Listener->Results.Last().Result == Expected);
	};
	Expect(TEXT("Sell 10 A"), S5Request({ S5Line(S5A, 10) }, {}), ENpcTradeResult::Success, 30);
	TestEqual(TEXT("10 A left"), Player.Inventory->GetTotalQuantity(S5A), 10);
	Expect(TEXT("Buy 2 B"), S5Request({}, { S5Line(S5B, 2) }), ENpcTradeResult::Success, -20);
	TestEqual(TEXT("2 B bought"), Player.Inventory->GetTotalQuantity(S5B), 2);
	Expect(TEXT("Sell 5 C (11) and buy 5 A (20)"), S5Request({ S5Line(S5C, 5) }, { S5Line(S5A, 5) }), ENpcTradeResult::Success, -9);
	TestTrue(TEXT("C gone, 15 A"), Player.Inventory->GetTotalQuantity(S5C) == 0 && Player.Inventory->GetTotalQuantity(S5A) == 15);
	Expect(TEXT("Selling more than held"), S5Request({ S5Line(S5A, 99) }, {}), ENpcTradeResult::InsufficientItems, 0);
	Expect(TEXT("Selling an item without a value"), S5Request({ S5Line(S5NoValue, 1) }, {}), ENpcTradeResult::NotTradable, 0);
	Expect(TEXT("Buying outside the store"), S5Request({}, { S5Line(S5C, 1) }), ENpcTradeResult::NotInStore, 0);
	Expect(TEXT("An empty trade"), S5Request({}, {}), ENpcTradeResult::EmptyTrade, 0);
	const int32 Coins = Player.Wallet->GetCoins();
	Player.Wallet->TestSetCoins(5);
	Expect(TEXT("Too few coins"), S5Request({}, { S5Line(S5B, 1) }), ENpcTradeResult::NotEnoughCoins, 0);
	Player.Wallet->TestSetCoins(MAX_int32 - 1);
	Expect(TEXT("A sale the wallet cannot hold"), S5Request({ S5Line(S5A, 1) }, {}), ENpcTradeResult::InvalidQuantity, 0);
	Player.Wallet->TestSetCoins(Coins);

	// A full inventory: a new stack does not fit, unless the trade frees a slot.
	S5SetCapacity(*Player.Inventory, Player.Inventory->GetItemsSnapshot().Num());
	Expect(TEXT("Full: buying B needs a slot"), S5Request({}, { S5Line(S5B, 1) }), ENpcTradeResult::InventoryFull, 0);
	Expect(TEXT("Full, but selling one B frees its slot for the B bought"), S5Request({ S5Line(S5B, 1) }, { S5Line(S5B, 1) }), ENpcTradeResult::Success, -3);
	S5SetCapacity(*Player.Inventory, 24);

	// A trade requested from inside the inventory broadcast is refused without a notification.
	FNpcTradeQuote Nested;
	Listener->InventoryHook = [&Nested, Trader, &Player]() { Nested = Trader->TryTrade(Player.Pawn, S5Request({ S5Line(S5A, 1) }, {})); };
	const int32 ResultsBefore = Listener->Results.Num();
	const FNpcTradeQuote Outer = Trader->TryTrade(Player.Pawn, S5Request({ S5Line(S5A, 1) }, {}));
	Listener->InventoryHook = nullptr;
	TestTrue(FString::Printf(TEXT("Nested trade: %s, outer %s, one result"), *S5Name(Nested.Result), *S5Name(Outer.Result)),
		Nested.Result == ENpcTradeResult::TradeInProgress && Outer.IsSuccess() && Listener->Results.Num() == ResultsBefore + 1);

	Npc->GetTalkWidget()->CloseTalk();
	TestEqual(TEXT("After the talk: NotTalking"), Trader->TryTrade(Player.Pawn, S5Request({ S5Line(S5A, 1) }, {})).Result, ENpcTradeResult::NotTalking);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNpcS5HaggleTest,
	"NullTide.Npc.S5.Haggle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNpcS5HaggleTest::RunTest(const FString& Parameters)
{
	// Problems: integers, in range, reproducible.
	FRandomStream Stream(11);
	int32 Bad = 0;
	TArray<FString> First;
	for (int32 Index = 0; Index < 400; ++Index)
	{
		FText Text;
		int32 Answer = 0;
		UNpcTraderComponent::MakeProblem(Stream, Text, Answer);
		int32 A = 0;
		int32 B = 0;
		TCHAR Operator = 0;
		const FString Problem = Text.ToString();
		TArray<FString> Parts;
		Problem.ParseIntoArrayWS(Parts);
		if (Parts.Num() != 5 || Parts[3] != TEXT("=") || Parts[4] != TEXT("?"))
		{
			++Bad;
			continue;
		}
		A = FCString::Atoi(*Parts[0]);
		B = FCString::Atoi(*Parts[2]);
		Operator = Parts[1][0];
		const bool bOk = (Operator == TEXT('+') && A >= 10 && A <= 99 && B >= 10 && B <= 99 && Answer == A + B)
			|| (Operator == TEXT('-') && A >= 10 && A <= 99 && B >= 10 && B <= A && Answer == A - B && Answer >= 0)
			|| (Operator == TEXT('x') && A >= 2 && A <= 12 && B >= 2 && B <= 9 && Answer == A * B)
			|| (Operator == TEXT('/') && B >= 2 && B <= 9 && Answer >= 2 && Answer <= 12 && A == Answer * B);
		Bad += bOk ? 0 : 1;
		if (Index < 10)
		{
			First.Add(Problem);
		}
	}
	TestEqual(TEXT("400 problems: integer answers within the ranges"), Bad, 0);
	FRandomStream Again(11);
	bool bSame = true;
	for (const FString& Expected : First)
	{
		FText Text;
		int32 Answer = 0;
		UNpcTraderComponent::MakeProblem(Again, Text, Answer);
		bSame &= Text.ToString() == Expected;
	}
	TestTrue(TEXT("The same seed gives the same problems"), bSame);

	FS5World Fixture;
	ANpcTestCharacter* Npc = Fixture.SpawnNpc(FVector(0.0, 0.0, 100.0), UNpcStoreTestDefinition::StaticClass());
	ANpcTestCharacter* Second = Fixture.SpawnNpc(FVector(0.0, 600.0, 100.0), UNpcStoreTestDefinition::StaticClass());
	const FS5Player Player = Fixture.SpawnPlayer(FVector(-200.0, 0.0, 100.0), 100);
	UNpcTraderComponent* Trader = Npc ? Npc->GetTrader() : nullptr;
	if (!TestTrue(TEXT("NPCs and a player"), Trader && Second && Player.Inventory))
	{
		return false;
	}
	Fixture.Listen(Player, *Trader);
	TestEqual(TEXT("Not talking"), Trader->StartHaggle(Player.Pawn), ENpcHaggleResult::NotTalking);
	Npc->OpenTalkForInteractor(Player.Pawn);
	TestEqual(TEXT("3 attempts today"), Trader->GetHaggleState().AttemptsLeft, 3);

	FText Problem;
	int32 Answer = 0;
	int32 Discount = 0;
	Trader->SetHaggleSeed(5);
	S5Predict(5, Problem, Answer, Discount);
	TestEqual(TEXT("Start"), Trader->StartHaggle(Player.Pawn), ENpcHaggleResult::Started);
	const FNpcHaggleState Active = Trader->GetHaggleState();
	TestTrue(FString::Printf(TEXT("The problem is on the clock (%s, %.1f s)"), *Active.ProblemText.ToString(), Active.TimeLeft),
		Active.bProblemActive && Active.ProblemText.ToString() == Problem.ToString() && FMath::IsNearlyEqual(Active.TimeLeft, 10.f, 0.01f));
	TestEqual(TEXT("A second start while active"), Trader->StartHaggle(Player.Pawn), ENpcHaggleResult::ProblemActive);
	TestEqual(TEXT("Wrong"), Trader->SubmitHaggleAnswer(Player.Pawn, Answer + 1), ENpcHaggleResult::Wrong);
	TestTrue(TEXT("Wrong: one attempt used, no discount"), Trader->GetHaggleState().AttemptsLeft == 2
		&& Trader->GetHaggleState().StoredDiscountPercent == 0 && !Trader->GetHaggleState().bProblemActive);
	TestEqual(TEXT("No problem to answer"), Trader->SubmitHaggleAnswer(Player.Pawn, 1), ENpcHaggleResult::NoProblem);

	TestEqual(TEXT("Start again"), Trader->StartHaggle(Player.Pawn), ENpcHaggleResult::Started);
	Trader->ExpireHaggleProblem();
	TestTrue(TEXT("Timed out: one attempt used"), Trader->GetHaggleState().AttemptsLeft == 1 && Fixture.Listener->Haggles.Last() == ENpcHaggleResult::TimedOut);

	Trader->SetHaggleSeed(9);
	S5Predict(9, Problem, Answer, Discount);
	Trader->StartHaggle(Player.Pawn);
	TestEqual(TEXT("Correct"), Trader->SubmitHaggleAnswer(Player.Pawn, Answer), ENpcHaggleResult::Correct);
	const FNpcHaggleState Stored = Trader->GetHaggleState();
	TestTrue(FString::Printf(TEXT("Correct: a %d%% discount stored (5-20), no attempt used yet"), Stored.StoredDiscountPercent),
		Stored.StoredDiscountPercent == Discount && Discount >= 5 && Discount <= 20 && Stored.AttemptsLeft == 1);
	TestEqual(TEXT("A stored discount blocks another haggle"), Trader->StartHaggle(Player.Pawn), ENpcHaggleResult::DiscountStored);

	// The discount survives the end of the talk.
	Npc->GetTalkWidget()->CloseTalk();
	Npc->OpenTalkForInteractor(Player.Pawn);
	TestEqual(TEXT("Kept after the talk"), Trader->GetHaggleState().StoredDiscountPercent, Discount);
	Player.Inventory->TryAddDefinitionQuantity(S5A, 10);
	const FNpcTradeQuote SellOnly = Trader->TryTrade(Player.Pawn, S5Request({ S5Line(S5A, 1) }, {}));
	TestTrue(TEXT("A sale without purchases does not use the discount"), SellOnly.IsSuccess() && SellOnly.Discount == 0
		&& Trader->GetHaggleState().StoredDiscountPercent == Discount);
	const int32 CoinsBefore = Player.Wallet->GetCoins();
	const FNpcTradeQuote Bought = Trader->TryTrade(Player.Pawn, S5Request({}, { S5Line(S5A, 10) }));
	const int32 ExpectedOff = 40 * Discount / 100;
	TestTrue(FString::Printf(TEXT("Buying 10 A (40) at -%d%%: pays %d"), Discount, 40 - ExpectedOff), Bought.IsSuccess()
		&& Bought.DiscountPercent == Discount && Bought.Discount == ExpectedOff && CoinsBefore - Player.Wallet->GetCoins() == 40 - ExpectedOff);
	TestTrue(TEXT("The discount is used once and spends the last attempt"), Trader->GetHaggleState().StoredDiscountPercent == 0
		&& Trader->GetHaggleState().AttemptsLeft == 0);
	TestEqual(TEXT("No attempts left today"), Trader->StartHaggle(Player.Pawn), ENpcHaggleResult::NoAttemptsLeft);
	TestEqual(TEXT("Another NPC keeps its own attempts"), Second->GetTrader()->GetHaggleState().AttemptsLeft, 3);

	Fixture.World->GetSubsystem<UGameClockSubsystem>()->AdvanceToNextDay();
	TestEqual(TEXT("A new day restores 3 attempts"), Trader->GetHaggleState().AttemptsLeft, 3);
	TestEqual(TEXT("Start on the new day"), Trader->StartHaggle(Player.Pawn), ENpcHaggleResult::Started);
	Npc->GetTalkWidget()->CloseTalk();
	TestTrue(TEXT("Leaving the talk with a problem open uses the attempt"), Trader->GetHaggleState().AttemptsLeft == 2
		&& !Trader->GetHaggleState().bProblemActive);

	// A problem given yesterday and answered today neither earns a discount nor spends today's attempt.
	Npc->OpenTalkForInteractor(Player.Pawn);
	Trader->SetHaggleSeed(17);
	S5Predict(17, Problem, Answer, Discount);
	Trader->StartHaggle(Player.Pawn);
	Fixture.World->GetSubsystem<UGameClockSubsystem>()->AdvanceToNextDay();
	TestEqual(TEXT("Answered on the next day"), Trader->SubmitHaggleAnswer(Player.Pawn, Answer), ENpcHaggleResult::TimedOut);
	TestTrue(TEXT("No discount, all of today's attempts left"), Trader->GetHaggleState().StoredDiscountPercent == 0
		&& Trader->GetHaggleState().AttemptsLeft == 3);
	Npc->GetTalkWidget()->CloseTalk();

	// A discount stored yesterday does not survive the night.
	Npc->OpenTalkForInteractor(Player.Pawn);
	Trader->SetHaggleSeed(21);
	S5Predict(21, Problem, Answer, Discount);
	Trader->StartHaggle(Player.Pawn);
	Trader->SubmitHaggleAnswer(Player.Pawn, Answer);
	Fixture.World->GetSubsystem<UGameClockSubsystem>()->AdvanceToNextDay();
	TestTrue(TEXT("A new day clears a stored discount"), Trader->GetHaggleState().StoredDiscountPercent == 0
		&& Trader->GetHaggleState().AttemptsLeft == 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNpcS5TradeScreenTest,
	"NullTide.Npc.S5.TradeScreen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNpcS5TradeScreenTest::RunTest(const FString& Parameters)
{
	UClass* TradeClass = LoadClass<UNpcTradeScreenWidget>(nullptr, S5TradeClassPath);
	FS5World Fixture;
	if (!TestNotNull(TEXT("W_NpcTrade loads"), TradeClass))
	{
		return false;
	}
	ANpcTestCharacter* Npc = Fixture.SpawnNpc(FVector(0.0, 0.0, 100.0), UNpcStoreTestDefinition::StaticClass());
	ANpcTestCharacter* Plain = Fixture.SpawnNpc(FVector(0.0, 600.0, 100.0), UNpcTestDefinition::StaticClass());
	const FS5Player Player = Fixture.SpawnPlayer(FVector(-200.0, 0.0, 100.0), 100);
	if (!TestTrue(TEXT("NPCs and a player"), Npc && Plain && Player.Inventory))
	{
		return false;
	}
	Npc->SetTradeScreenClass(TradeClass);
	Plain->SetTradeScreenClass(TradeClass);
	Player.Inventory->TryAddDefinitionQuantity(S5A, 20);
	Player.Inventory->TryAddDefinitionQuantity(S5NoValue, 1);

	// An NPC without a store hides Trade.
	Player.Pawn->SetActorLocation(FVector(-200.0, 600.0, 100.0));
	Plain->OpenTalkForInteractor(Player.Pawn);
	UButton* PlainTrade = Plain->GetTalkWidget() ? Plain->GetTalkWidget()->GetTradeButton() : nullptr;
	TestTrue(TEXT("No store: Trade is hidden and does not open"), PlainTrade && PlainTrade->GetVisibility() == ESlateVisibility::Collapsed
		&& !Plain->OpenTrade());
	Plain->GetTalkWidget()->CloseTalk();
	Player.Pawn->SetActorLocation(FVector(-200.0, 0.0, 100.0));

	Npc->OpenTalkForInteractor(Player.Pawn);
	UNpcTalkWidget* Talk = Npc->GetTalkWidget();
	UButton* TradeButton = Talk ? Talk->GetTradeButton() : nullptr;
	TestTrue(TEXT("A store: Trade is shown"), TradeButton && TradeButton->GetVisibility() == ESlateVisibility::Visible);
	if (TradeButton)
	{
		TradeButton->OnClicked.Broadcast();
	}
	UNpcTradeScreenWidget* Screen = Npc->GetTradeWidget();
	if (!TestTrue(TEXT("Trade opens the screen and hides the talk panel"), Screen && Npc->IsTrading()
		&& Talk->GetVisibility() == ESlateVisibility::Collapsed && Screen->HasRequiredWidgets()))
	{
		return false;
	}
	auto Names = [](const TArray<UNpcTradeItemRowWidget*>& Rows)
	{
		TArray<FString> Out;
		for (const UNpcTradeItemRowWidget* Row : Rows)
		{
			Out.Add(FString::Printf(TEXT("%s %s"), *Row->GetNameText().ToString(), *Row->GetQuantityText().ToString()));
		}
		return FString::Join(Out, TEXT(", "));
	};
	TestEqual(TEXT("Left: tradable items only"), Names(Screen->GetRows(ENpcTradeRowSource::Inventory)), FString(TEXT("Item A ×20")));
	TestEqual(TEXT("Right: the catalog"), Names(Screen->GetRows(ENpcTradeRowSource::Store)), FString(TEXT("Item A , Item B ")));
	TestFalse(TEXT("Nothing picked: TRADE disabled"), Screen->IsTradeEnabled());

	// A stackable item asks for a quantity.
	Screen->GetRows(ENpcTradeRowSource::Inventory)[0]->ClickRow();
	UTradeQuantityPickerWidget* Picker = Screen->GetQuantityPicker();
	TestTrue(TEXT("A stackable item opens the picker (1-20)"), Picker && Picker->IsAsking() && Picker->GetMax() == 20);
	Picker->SetQuantity(5);
	Picker->Confirm();
	TestTrue(TEXT("5 A in You sell, 15 A left"), Names(Screen->GetRows(ENpcTradeRowSource::SellBasket)) == TEXT("Item A ×5")
		&& Names(Screen->GetRows(ENpcTradeRowSource::Inventory)) == TEXT("Item A ×15") && !Picker->IsAsking());
	Screen->GetRows(ENpcTradeRowSource::Inventory)[0]->ClickRow();
	Picker->Cancel();
	TestEqual(TEXT("Cancel changes nothing"), Names(Screen->GetRows(ENpcTradeRowSource::SellBasket)), FString(TEXT("Item A ×5")));
	// A non-stackable item goes straight in.
	Screen->GetRows(ENpcTradeRowSource::Store)[1]->ClickRow();
	TestTrue(TEXT("B goes straight into You buy"), Names(Screen->GetRows(ENpcTradeRowSource::BuyBasket)) == TEXT("Item B ×1") && !Picker->IsAsking());
	const FNpcTradeQuote Expected = Npc->GetTrader()->QuoteTrade(Player.Pawn, Screen->GetRequest());
	TestTrue(FString::Printf(TEXT("Totals match the NPC's quote (net %d)"), Screen->GetQuote().Net), Screen->GetQuote().Net == Expected.Net
		&& Screen->GetQuote().Net == 5 && Screen->IsTradeEnabled());

	// Haggling through the screen.
	FText Problem;
	int32 Answer = 0;
	int32 Discount = 0;
	Npc->GetTrader()->SetHaggleSeed(3);
	S5Predict(3, Problem, Answer, Discount);
	Screen->RequestHaggle();
	TestTrue(TEXT("Haggle shows the problem and disables Haggle"), Screen->GetHaggleProblemText().ToString() == Problem.ToString()
		&& !Screen->IsHaggleEnabled());
	Screen->GetHaggleAnswerBox()->SetText(FText::AsNumber(Answer));
	Screen->SubmitHaggle();
	TestTrue(FString::Printf(TEXT("Correct: the %d%% discount is in the totals"), Discount), Screen->GetQuote().DiscountPercent == Discount
		&& Screen->GetQuote().Discount == 10 * Discount / 100);
	TestTrue(TEXT("Answers must be whole numbers"), UNpcTradeScreenWidget::IsWholeNumber(TEXT("12")) && UNpcTradeScreenWidget::IsWholeNumber(TEXT("-3"))
		&& !UNpcTradeScreenWidget::IsWholeNumber(TEXT("12.9")) && !UNpcTradeScreenWidget::IsWholeNumber(TEXT("1e3"))
		&& !UNpcTradeScreenWidget::IsWholeNumber(TEXT("")) && !UNpcTradeScreenWidget::IsWholeNumber(TEXT("-")));

	const int32 Coins = Player.Wallet->GetCoins();
	Screen->RequestTrade();
	TestTrue(FString::Printf(TEXT("TRADE trades once: coins %d -> %d, %s"), Coins, Player.Wallet->GetCoins(), *Screen->GetFeedbackText().ToString()),
		Player.Wallet->GetCoins() == Coins + 15 - (10 - 10 * Discount / 100) && Player.Inventory->GetTotalQuantity(S5A) == 15
		&& Player.Inventory->GetTotalQuantity(S5B) == 1 && Screen->GetRows(ENpcTradeRowSource::SellBasket).IsEmpty()
		&& Screen->GetRows(ENpcTradeRowSource::BuyBasket).IsEmpty() && Screen->GetFeedbackText().ToString().StartsWith(TEXT("Traded")));

	// Not enough coins: TRADE disabled with the reason.
	Player.Wallet->TestSetCoins(0);
	Screen->GetRows(ENpcTradeRowSource::Store)[1]->ClickRow();
	TestTrue(FString::Printf(TEXT("Too few coins: disabled (%s)"), *Screen->GetFeedbackText().ToString()), !Screen->IsTradeEnabled()
		&& Screen->GetFeedbackText().ToString() == UNpcTradeScreenWidget::DescribeTradeResult(ENpcTradeResult::NotEnoughCoins).ToString());
	Screen->GetRows(ENpcTradeRowSource::BuyBasket)[0]->ClickRow();
	TestTrue(TEXT("Clicking a basket line removes it"), Screen->GetRows(ENpcTradeRowSource::BuyBasket).IsEmpty());

	// A new day while the screen is open: the totals and haggling follow at once.
	Player.Wallet->TestSetCoins(100);
	Npc->GetTrader()->SetHaggleSeed(4);
	S5Predict(4, Problem, Answer, Discount);
	Screen->RequestHaggle();
	Screen->GetHaggleAnswerBox()->SetText(FText::AsNumber(Answer));
	Screen->SubmitHaggle();
	Screen->GetRows(ENpcTradeRowSource::Store)[1]->ClickRow();
	TestTrue(TEXT("A discount in the totals"), Screen->GetQuote().DiscountPercent == Discount && !Screen->IsHaggleEnabled());
	Fixture.World->GetSubsystem<UGameClockSubsystem>()->AdvanceToNextDay();
	TestTrue(TEXT("The new day drops it from the totals and allows haggling again"), Screen->GetQuote().DiscountPercent == 0
		&& Screen->GetQuote().Net == -10 && Screen->IsHaggleEnabled());
	Screen->ClearBaskets();

	// Back returns to the talk; Esc-style close ends everything.
	Screen->RequestBack();
	TestTrue(TEXT("Back: trade closed, talk visible"), !Npc->IsTrading() && Npc->IsTalking() && Talk->GetVisibility() == ESlateVisibility::Visible
		&& !Screen->GetNpc());
	Npc->OpenTrade();
	Npc->GetTradeWidget()->RequestClose();
	TestTrue(TEXT("Close: the trade and the talk end"), !Npc->IsTrading() && !Npc->IsTalking());
	TestTrue(TEXT("I, Tab and Esc close; E does not"), UNpcTradeScreenWidget::IsCloseKey(EKeys::Escape) && UNpcTradeScreenWidget::IsCloseKey(EKeys::Tab)
		&& UNpcTradeScreenWidget::IsCloseKey(EKeys::I) && !UNpcTradeScreenWidget::IsCloseKey(EKeys::E));

	Npc->OpenTalkForInteractor(Player.Pawn);
	Npc->OpenTrade();
	UNpcTradeScreenWidget* Last = Npc->GetTradeWidget();
	Npc->Destroy();
	TestTrue(TEXT("The NPC ending play releases its trade screen"), Last && !Last->GetNpc());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNpcS5ProductionWiringTest,
	"NullTide.Npc.S5.ProductionWiring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNpcS5ProductionWiringTest::RunTest(const FString& Parameters)
{
	struct FValue { const TCHAR* Item; int32 Value; };
	const FValue Values[] = {
		{ TEXT("Item_Wood"), 2 }, { TEXT("Item_Stone"), 3 }, { TEXT("Item_Scrap"), 5 }, { TEXT("Item_Meat"), 6 },
		{ TEXT("Item_WaterBottle"), 4 }, { TEXT("Item_Sandwich"), 12 }, { TEXT("Item_HealthPotion"), 25 }, { TEXT("Item_WoodArrow"), 1 },
		{ TEXT("Item_WoodBow"), 40 }, { TEXT("Item_Axe"), 30 }, { TEXT("Item_Pickaxe"), 30 }, { TEXT("Item_ShortSword"), 45 },
		{ TEXT("Item_LongSword"), 80 },
	};
	TMap<FString, UClass*> Items;
	for (const FValue& Entry : Values)
	{
		UClass* Item = LoadClass<UItemDefinition>(nullptr,
			*FString::Printf(TEXT("/Game/LevelPrototyping/InventorySystem/Items/%s.%s_C"), Entry.Item, Entry.Item));
		Items.Add(Entry.Item, Item);
		TestEqual(FString::Printf(TEXT("%s is worth %d"), Entry.Item, Entry.Value), UItemFragment_TradeValue::GetTradeValue(Item), Entry.Value);
	}
	UClass* TraderDefinition = LoadClass<UNpcDefinition>(nullptr, S5TraderDefinitionPath);
	const UNpcDefinition* Trader = TraderDefinition ? TraderDefinition->GetDefaultObject<UNpcDefinition>() : nullptr;
	FString Diagnostic;
	if (TestTrue(TEXT("NPC_Trader loads and is valid"), Trader && Trader->ValidateNpc(Diagnostic)))
	{
		const TArray<TSubclassOf<UItemDefinition>> Catalog = { Items[TEXT("Item_Stone")], Items[TEXT("Item_Scrap")], Items[TEXT("Item_HealthPotion")],
			Items[TEXT("Item_Sandwich")], Items[TEXT("Item_WoodArrow")], Items[TEXT("Item_WoodBow")], Items[TEXT("Item_Axe")], Items[TEXT("Item_Pickaxe")] };
		TestTrue(TEXT("The catalog: Stone, Scrap, Health Potion, Sandwich, Wood Arrow, Wood Bow, Axe, Pickaxe"), Trader->StoreItems == Catalog);
		TestTrue(TEXT("Sell 75%, buy 100%, 3 haggles, 5-20%, 10 s, 16 lines"), FMath::IsNearlyEqual(Trader->SellRate, 0.75f)
			&& FMath::IsNearlyEqual(Trader->BuyRate, 1.f) && Trader->HaggleAttemptsPerDay == 3 && Trader->HaggleDiscountMinPercent == 5
			&& Trader->HaggleDiscountMaxPercent == 20 && FMath::IsNearlyEqual(Trader->HaggleTimeLimit, 10.f) && Trader->MaxLinesPerSide == 16);
	}

	UClass* TraderClass = LoadClass<ANpcCharacter>(nullptr, S5TraderClassPath);
	UClass* TradeClass = LoadClass<UNpcTradeScreenWidget>(nullptr, S5TradeClassPath);
	UClass* RowClass = LoadClass<UNpcTradeItemRowWidget>(nullptr, S5RowClassPath);
	UClass* PickerClass = LoadClass<UTradeQuantityPickerWidget>(nullptr, S5PickerClassPath);
	UClass* StatusClass = LoadClass<UPlayerStatusWidget>(nullptr, S5StatusClassPath);
	if (!TestTrue(TEXT("Widgets and BP_Npc_Trader load"), TraderClass && TradeClass && RowClass && PickerClass && StatusClass))
	{
		return false;
	}
	FClassProperty* ScreenProperty = FindFProperty<FClassProperty>(ANpcCharacter::StaticClass(), TEXT("TradeScreenClass"));
	TestTrue(TEXT("BP_Npc_Trader opens W_NpcTrade"), ScreenProperty && ScreenProperty->GetObjectPropertyValue_InContainer(TraderClass->GetDefaultObject()) == TradeClass);
	FClassProperty* RowProperty = FindFProperty<FClassProperty>(UNpcTradeScreenWidget::StaticClass(), TEXT("RowClass"));
	TestTrue(TEXT("W_NpcTrade rows are W_NpcTradeItemRow"), RowProperty && RowProperty->GetObjectPropertyValue_InContainer(TradeClass->GetDefaultObject()) == RowClass);

	FS5World Fixture;
	const UNpcTradeScreenWidget* Screen = CreateWidget<UNpcTradeScreenWidget>(Fixture.World, TradeClass);
	const UNpcTradeItemRowWidget* Row = CreateWidget<UNpcTradeItemRowWidget>(Fixture.World, RowClass);
	const UTradeQuantityPickerWidget* Picker = CreateWidget<UTradeQuantityPickerWidget>(Fixture.World, PickerClass);
	const UPlayerStatusWidget* Status = CreateWidget<UPlayerStatusWidget>(Fixture.World, StatusClass);
	const UNpcTalkWidget* Talk = CreateWidget<UNpcTalkWidget>(Fixture.World, LoadClass<UNpcTalkWidget>(nullptr, S5TalkClassPath));
	TestTrue(TEXT("Every widget binds what it needs"), Screen && Screen->HasRequiredWidgets() && Row && Row->HasRequiredWidgets() && Picker
		&& Picker->HasRequiredWidgets() && Status && Status->HasRequiredWidgets());
	TestTrue(TEXT("W_NpcTalk has the Trade button"), Talk && Talk->GetTradeButton());

	const UBlueprint* Player = LoadObject<UBlueprint>(nullptr, S5PlayerPath);
	TArray<const UWalletComponent*> Wallets;
	TArray<const UPlayerStatusHudComponent*> Huds;
	if (Player && Player->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : Player->SimpleConstructionScript->GetAllNodes())
		{
			if (const UWalletComponent* Wallet = Node ? Cast<UWalletComponent>(Node->ComponentTemplate) : nullptr)
			{
				Wallets.Add(Wallet);
			}
			if (const UPlayerStatusHudComponent* Hud = Node ? Cast<UPlayerStatusHudComponent>(Node->ComponentTemplate) : nullptr)
			{
				Huds.Add(Hud);
			}
		}
	}
	TestTrue(TEXT("The player has one wallet starting at 200 coins"), Wallets.Num() == 1 && Wallets[0]->StartingCoins == 200);
	FClassProperty* StatusProperty = FindFProperty<FClassProperty>(UPlayerStatusHudComponent::StaticClass(), TEXT("StatusWidgetClass"));
	TestTrue(TEXT("The player has one status HUD using W_PlayerStatus"), Huds.Num() == 1 && StatusProperty
		&& StatusProperty->GetObjectPropertyValue_InContainer(Huds[0]) == StatusClass);
	return true;
}

#endif
