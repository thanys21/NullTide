#include "UI/BuildingHudWidget.h"

#include "Components/TextBlock.h"
#include "UI/BuildingBarWidget.h"

#define LOCTEXT_NAMESPACE "BuildingHud"

void UBuildingHudWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// The HUD itself never takes the cursor; only the build bar inside it is hit-testable (B8).
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UBuildingHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (HintText)
	{
		HintText->SetText(LOCTEXT("Hint", "1-5 / click select  ·  R rotate  ·  C demolish  ·  LMB build  ·  RMB / B exit"));
	}
}

bool UBuildingHudWidget::InitializeBuildBar(UBuildingPlacementComponent* Placement)
{
	return BuildBar && BuildBar->InitializeBar(Placement);
}

void UBuildingHudWidget::SetBuildBarSelection(const int32 SelectedIndex, const bool bDemolish)
{
	if (BuildBar)
	{
		BuildBar->SetSelection(SelectedIndex, bDemolish);
	}
}

void UBuildingHudWidget::ReleaseBuildBar()
{
	if (BuildBar)
	{
		BuildBar->ReleaseBar();
	}
}

bool UBuildingHudWidget::HasBuildBar() const
{
	return BuildBar != nullptr;
}

bool UBuildingHudWidget::IsBuildBarHovered() const
{
	return BuildBar && BuildBar->IsBarHovered();
}

UBuildingBarWidget* UBuildingHudWidget::GetBuildBar() const
{
	return BuildBar;
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
