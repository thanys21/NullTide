#include "UI/PlayerStatusWidget.h"

#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "PlayerStatus"

void UPlayerStatusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

FText UPlayerStatusWidget::FormatCoins(const int32 Coins)
{
	return FText::Format(LOCTEXT("Coins", "{0} coins"), FText::AsNumber(Coins));
}

FText UPlayerStatusWidget::FormatDay(const int32 Day, const bool bNight)
{
	return FText::Format(LOCTEXT("Day", "Day {0} · {1}"), FText::AsNumber(Day),
		bNight ? LOCTEXT("Night", "Night") : LOCTEXT("Daytime", "Day"));
}

void UPlayerStatusWidget::SetCoins(const int32 Coins)
{
	if (CoinsText)
	{
		CoinsText->SetText(FormatCoins(Coins));
	}
}

void UPlayerStatusWidget::SetDay(const int32 Day, const bool bNight)
{
	if (DayText)
	{
		DayText->SetText(FormatDay(Day, bNight));
	}
}

FText UPlayerStatusWidget::GetCoinsText() const
{
	return CoinsText ? CoinsText->GetText() : FText::GetEmpty();
}

FText UPlayerStatusWidget::GetDayText() const
{
	return DayText ? DayText->GetText() : FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
