#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NpcTradeItemRowWidget.generated.h"

class UButton;
class UImage;
class UItemDefinition;
class UNpcTradeScreenWidget;
class UTextBlock;

/** Which list a trade row belongs to. */
UENUM(BlueprintType)
enum class ENpcTradeRowSource : uint8
{
	/** The player's inventory: a click puts it into "You sell". */
	Inventory,
	/** The NPC's store: a click puts it into "You buy". */
	Store,
	/** A line of "You sell": a click takes it out. */
	SellBasket,
	/** A line of "You buy": a click takes it out. */
	BuyBasket
};

/** One item row of the trade screen (Phase 6 S5): icon, name, quantity and price. Display only; a click goes to its screen. */
UCLASS(Abstract)
class NULLTIDE_API UNpcTradeItemRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(UNpcTradeScreenWidget* InScreen, TSubclassOf<UItemDefinition> InItem, ENpcTradeRowSource InSource, int32 InQuantity,
		const FText& PriceText);

	TSubclassOf<UItemDefinition> GetItem() const { return Item; }
	ENpcTradeRowSource GetSource() const { return Source; }
	int32 GetQuantity() const { return Quantity; }
	FText GetNameText() const;
	FText GetQuantityText() const;
	FText GetPriceText() const;
	bool HasRequiredWidgets() const { return RowButton && NameText && QuantityText && PriceText; }

	/** Clicks the row (the button's work). */
	UFUNCTION(BlueprintCallable, Category = "NPC|Trade")
	void ClickRow();

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UButton> RowButton;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UTextBlock> QuantityText;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UTextBlock> PriceText;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

private:
	UFUNCTION()
	void HandleClicked();

	TWeakObjectPtr<UNpcTradeScreenWidget> Screen;
	TSubclassOf<UItemDefinition> Item;
	ENpcTradeRowSource Source = ENpcTradeRowSource::Inventory;
	int32 Quantity = 0;
};
