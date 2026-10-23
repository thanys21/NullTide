#include "UI/TradeQuantityPickerWidget.h"

#include "Components/Button.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"

void UTradeQuantityPickerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (OkButton)
	{
		OkButton->OnClicked.AddUniqueDynamic(this, &UTradeQuantityPickerWidget::HandleOk);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddUniqueDynamic(this, &UTradeQuantityPickerWidget::HandleCancel);
	}
	if (QuantityBox)
	{
		QuantityBox->SetMinFractionalDigits(0);
		QuantityBox->SetMaxFractionalDigits(0);
		QuantityBox->SetDelta(1.f);
		QuantityBox->SetAlwaysUsesDeltaSnap(true);
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void UTradeQuantityPickerWidget::Ask(const FText& ItemName, const int32 InMax, FOnTradeQuantityConfirmed InOnConfirmed)
{
	Max = FMath::Max(InMax, 1);
	OnConfirmed = MoveTemp(InOnConfirmed);
	bAsking = true;
	if (TitleText)
	{
		TitleText->SetText(FText::Format(NSLOCTEXT("NpcTrade", "PickerTitle", "How many {0}? (1-{1})"), ItemName, FText::AsNumber(Max)));
	}
	if (QuantityBox)
	{
		QuantityBox->SetMinValue(1.f);
		QuantityBox->SetMaxValue(static_cast<float>(Max));
		QuantityBox->SetMinSliderValue(1.f);
		QuantityBox->SetMaxSliderValue(static_cast<float>(Max));
	}
	SetQuantity(Max);
	SetVisibility(ESlateVisibility::Visible);
}

void UTradeQuantityPickerWidget::SetQuantity(const int32 Quantity)
{
	if (QuantityBox)
	{
		QuantityBox->SetValue(static_cast<float>(FMath::Clamp(Quantity, 1, Max)));
	}
}

int32 UTradeQuantityPickerWidget::GetQuantity() const
{
	return QuantityBox ? FMath::Clamp(FMath::RoundToInt(QuantityBox->GetValue()), 1, Max) : 1;
}

void UTradeQuantityPickerWidget::Confirm()
{
	if (!bAsking)
	{
		return;
	}
	const int32 Quantity = GetQuantity();
	// Close first: the callback may rebuild the screen that holds this picker.
	FOnTradeQuantityConfirmed Callback = MoveTemp(OnConfirmed);
	Cancel();
	Callback.ExecuteIfBound(Quantity);
}

void UTradeQuantityPickerWidget::Cancel()
{
	bAsking = false;
	OnConfirmed.Unbind();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UTradeQuantityPickerWidget::HandleOk()
{
	Confirm();
}

void UTradeQuantityPickerWidget::HandleCancel()
{
	Cancel();
}
