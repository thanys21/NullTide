#include "UI/BuildingHudWidget.h"

#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "BuildingHud"

void UBuildingHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// The HUD must never take clicks from the world placement.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (HintText)
	{
		HintText->SetText(LOCTEXT("Hint", "1-5 select  ·  R rotate  ·  C demolish  ·  LMB build  ·  RMB / B exit"));
	}
}

void UBuildingHudWidget::SetHintText(const FText& Hint)
{
	if (HintText)
	{
		HintText->SetText(Hint);
	}
}

FText UBuildingHudWidget::GetHintText() const
{
	return HintText ? HintText->GetText() : FText::GetEmpty();
}

void UBuildingHudWidget::SetBuildingText(const FText& BuildingName, const FText& Cost)
{
	if (BuildingNameText)
	{
		BuildingNameText->SetText(BuildingName);
	}
	if (CostText)
	{
		CostText->SetText(Cost);
	}
}

void UBuildingHudWidget::SetStatusText(const FText& Status, const bool bValid)
{
	bStatusValid = bValid;
	if (StatusText)
	{
		StatusText->SetText(Status);
		StatusText->SetColorAndOpacity(FSlateColor(bValid ? ValidStatusColor : InvalidStatusColor));
	}
}

FText UBuildingHudWidget::GetBuildingNameText() const
{
	return BuildingNameText ? BuildingNameText->GetText() : FText::GetEmpty();
}

FText UBuildingHudWidget::GetCostText() const
{
	return CostText ? CostText->GetText() : FText::GetEmpty();
}

FText UBuildingHudWidget::GetStatusText() const
{
	return StatusText ? StatusText->GetText() : FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
