#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TradeQuantityPickerWidget.generated.h"

class UButton;
class USpinBox;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FOnTradeQuantityConfirmed, int32 /*Quantity*/);

/**
 * Asks how many of a stackable item go into the trade (Phase 6 S5): a whole-number box from 1 to a maximum, OK and
 * Cancel. It lives inside the trade screen, collapsed until asked.
 */
UCLASS(Abstract)
class NULLTIDE_API UTradeQuantityPickerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows the picker for an item name and a maximum (at least 1), starting at the maximum. */
	void Ask(const FText& ItemName, int32 Max, FOnTradeQuantityConfirmed InOnConfirmed);

	/** Confirms the current amount (OK's work). */
	UFUNCTION(BlueprintCallable, Category = "NPC|Trade")
	void Confirm();

	/** Closes without a change (Cancel's work). */
	UFUNCTION(BlueprintCallable, Category = "NPC|Trade")
	void Cancel();

	void SetQuantity(int32 Quantity);
	int32 GetQuantity() const;
	int32 GetMax() const { return Max; }
	bool IsAsking() const { return bAsking; }
	bool HasRequiredWidgets() const { return QuantityBox && OkButton && CancelButton; }

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<USpinBox> QuantityBox;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UButton> OkButton;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidget))
	TObjectPtr<UButton> CancelButton;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Trade", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

private:
	UFUNCTION()
	void HandleOk();

	UFUNCTION()
	void HandleCancel();

	FOnTradeQuantityConfirmed OnConfirmed;
	int32 Max = 1;
	bool bAsking = false;
};
