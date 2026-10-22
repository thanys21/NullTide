#include "UI/BaseZoneTagWidget.h"

#include "Components/TextBlock.h"

void UBaseZoneTagWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBaseZoneTagWidget::SetZoneName(const FText& ZoneName)
{
	if (ZoneNameText)
	{
		ZoneNameText->SetText(ZoneName);
	}
}

FText UBaseZoneTagWidget::GetZoneNameText() const
{
	return ZoneNameText ? ZoneNameText->GetText() : FText::GetEmpty();
}
