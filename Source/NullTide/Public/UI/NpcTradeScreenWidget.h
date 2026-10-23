#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Npc/NpcTradeTypes.h"
#include "NpcTradeScreenWidget.generated.h"

class ANpcCharacter;
class APawn;
class UButton;
class UEditableTextBox;
class UInventoryComponent;
class UItemDefinition;
class UNpcTradeItemRowWidget;
class UNpcTraderComponent;
class UPanelWidget;
class UTextBlock;
class UTradeQuantityPickerWidget;
class UWalletComponent;
class UGameClockSubsystem;
enum class ENpcTradeRowSource : uint8;

/**
 * The trade screen (Phase 6 S5): the player's tradable inventory on the left, the exchange in the middle ("You sell",
 * "You buy", totals, haggling, TRADE) and the NPC's store on the right. Display only: every price, check and haggle
 * lives on the NPC's UNpcTraderComponent, and TRADE sends one request. A stackable item asks for a quantity; a
 * non-stackable item moves straight in. Back returns to the talk panel; Esc, I and Tab close everything.
 */
UCLASS(Abstract)
class NULLTIDE_API UNpcTradeScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Binds the screen to an NPC that is talking to Trader, and builds the lists. */
	void OpenForNpc(ANpcCharacter* InNpc, APawn* InTrader);

	/** Unbinds from the NPC, the inventory, the wallet and the countdown (the NPC closes the screen). */
	void ReleaseNpc();

	/** A row was clicked: inventory and store rows add (asking a quantity for stackables), basket rows are removed. */
	void HandleRowClicked(UNpcTradeItemRowWidget& Row);

	/** Adds to one side; refused past the line limit. */
	bool AddToBasket(bool bSell, TSubclassOf<UItemDefinition> Item, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "NPC|Trade")
	void RequestTrade();

	UFUNCTION(BlueprintCallable, Category = "NPC|Trade")
	void ClearBaskets();

	UFUNCTION(BlueprintCallable, Category = "NPC|Trade")
	void RequestBack();

	UFUNCTION(BlueprintCallable, Category = "NPC|Trade")
	void RequestClose();

	UFUNCTION(BlueprintCallable, Category = "NPC|Trade")
	void RequestHaggle();

	/** Sends the answer box's number (OK's work). A non-number is not sent. */
	UFUNCTION(BlueprintCallable, Category = "NPC|Trade")
	void SubmitHaggle();

	const FNpcTradeRequest& GetRequest() const { return Request; }
	const FNpcTradeQuote& GetQuote() const { return Quote; }
	TArray<UNpcTradeItemRowWidget*> GetRows(ENpcTradeRowSource Source) const;
	UTradeQuantityPickerWidget* GetQuantityPicker() const { return QuantityPicker; }
	UEditableTextBox* GetHaggleAnswerBox() const { return HaggleAnswerBox; }
	FText GetFeedbackText() const;
	FText GetHaggleStatusText() const;
	FText GetHaggleProblemText() const;
	FText GetNetText() const;
	bool IsTradeEnabled() const;
	bool IsHaggleEnabled() const;
	ANpcCharacter* GetNpc() const { return Npc.Get(); }
	bool HasRequiredWidgets() const;

	static FText DescribeTradeResult(ENpcTradeResult Result);
	static FText FormatCoins(int32 Coins);
	static bool IsCloseKey(const FKey& Key);

	/** A haggle answer: digits with an optional leading minus (no decimals, no exponent). */
	static bool IsWholeNumber(const FString& Text);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(EditDefaultsOnly, Category = "NPC|Trade")
	TSubclassOf<UNpcTradeItemRowWidget> RowClass;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UPanelWidget> InventoryList;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UPanelWidget> StoreList;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UPanelWidget> SellList;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UPanelWidget> BuyList;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UTextBlock> SellTotalText;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UTextBlock> BuyTotalText;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UTextBlock> DiscountText;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UTextBlock> NetText;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UTextBlock> CoinsText;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UTextBlock> FeedbackText;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UButton> TradeButton;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UButton> ClearButton;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UButton> BackButton;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UButton> HaggleButton;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UTextBlock> HaggleProblemText;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UTextBlock> HaggleTimeText;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UEditableTextBox> HaggleAnswerBox;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UButton> HaggleSubmitButton;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UTextBlock> HaggleStatusText;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UTradeQuantityPickerWidget> QuantityPicker;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

private:
	UFUNCTION()
	void HandleInventoryChanged(int32 NewRevision);

	UFUNCTION()
	void HandleCoinsChanged(int32 NewCoins);

	UFUNCTION()
	void HandlePhaseChanged(int32 Day, bool bNight);

	UFUNCTION()
	void HandleTradeResult(AActor* InTrader, const FNpcTradeQuote& Result);

	UFUNCTION()
	void HandleHaggleChanged(ENpcHaggleResult Result);

	UFUNCTION()
	void HandleTradeClicked();

	UFUNCTION()
	void HandleClearClicked();

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleHaggleClicked();

	UFUNCTION()
	void HandleHaggleSubmitClicked();

	/** Rebuilds every list and refreshes the totals and haggling. */
	void Rebuild();
	void RefreshQuote();
	void RefreshHaggle();
	void UpdateCountdown();
	/** Keeps "You sell" within what the inventory still holds. */
	void ClampSellBasket();
	void AddRow(UPanelWidget* List, TSubclassOf<UItemDefinition> Item, ENpcTradeRowSource Source, int32 Quantity, const FText& Price);
	void SetFeedback(const FText& Text);
	FText DescribeHaggleState() const;

	UNpcTraderComponent* GetTrader() const;
	UInventoryComponent* GetInventory() const;
	UWalletComponent* GetWallet() const;

	TWeakObjectPtr<ANpcCharacter> Npc;
	TWeakObjectPtr<APawn> TraderPawn;
	TWeakObjectPtr<UInventoryComponent> BoundInventory;
	TWeakObjectPtr<UWalletComponent> BoundWallet;
	TWeakObjectPtr<UNpcTraderComponent> BoundTrader;
	TWeakObjectPtr<UGameClockSubsystem> BoundClock;
	FNpcTradeRequest Request;
	FNpcTradeQuote Quote;
	FTimerHandle CountdownTimer;
	bool bTradeRequestPending = false;
	/** The last action's message, kept until the baskets change. */
	FText LastActionText;
};
