#include "UI/BuildingBarSlotWidget.h"

#include "Building/BuildingDefinition.h"
#include "Building/BuildingPlacementComponent.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/BuildingBarWidget.h"

#define LOCTEXT_NAMESPACE "BuildingBar"

void UBuildingBarSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (SlotButton)
	{
		SlotButton->OnClicked.AddUniqueDynamic(this, &UBuildingBarSlotWidget::HandleSlotClicked);
	}
	ApplyStyle();
}

void UBuildingBarSlotWidget::SetupSlot(UBuildingBarWidget* InBar, const int32 InSlotIndex, const FText& InKey,
	const TSubclassOf<UBuildingDefinition> InBuilding)
{
	Bar = InBar;
	SlotIndex = InSlotIndex;
	Building = InBuilding;
	const UBuildingDefinition* Definition = Building ? Building->GetDefaultObject<UBuildingDefinition>() : nullptr;

	const FText Name = Definition ? Definition->BuildingName : LOCTEXT("DemolishName", "Demolish");
	const FText Cost = !Definition ? LOCTEXT("DemolishCost", "Refund")
		: Definition->Cost.IsEmpty() ? LOCTEXT("Free", "Free")
		: UBuildingPlacementComponent::DescribeRefund(Definition->Cost);
	const FText Tooltip = Definition ? Definition->BuildingDescription
		: LOCTEXT("DemolishTooltip", "Remove a building you built and get its resources back.");
	const FString NameString = Name.ToString();
	const FText Initial = NameString.IsEmpty() ? FText::GetEmpty() : FText::FromString(NameString.Left(1).ToUpper());

	if (KeyText)
	{
		KeyText->SetText(InKey);
	}
	if (NameText)
	{
		NameText->SetText(Name);
	}
	if (CostText)
	{
		CostText->SetText(Cost);
	}
	UTexture2D* Icon = Definition ? Definition->BuildingIcon.Get() : nullptr;
	if (IconImage)
	{
		if (Icon)
		{
			IconImage->SetBrushFromTexture(Icon);
			IconImage->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			// No art yet: a coloured square with the name's initial.
			IconImage->SetColorAndOpacity(Definition ? PlaceholderIconColor : DemolishIconColor);
		}
	}
	if (InitialText)
	{
		InitialText->SetText(Initial);
		InitialText->SetVisibility(Icon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	SetToolTipText(Tooltip);
	ApplyStyle();
}

void UBuildingBarSlotWidget::SetSelected(const bool bInSelected)
{
	if (bSelected != bInSelected)
	{
		bSelected = bInSelected;
		ApplyStyle();
	}
}

void UBuildingBarSlotWidget::SetAffordable(const bool bInAffordable)
{
	if (bAffordable != bInAffordable)
	{
		bAffordable = bInAffordable;
		ApplyStyle();
	}
}

void UBuildingBarSlotWidget::HandleSlotClicked()
{
	if (UBuildingBarWidget* OwningBar = Bar.Get())
	{
		OwningBar->HandleSlotClicked(SlotIndex);
	}
}

void UBuildingBarSlotWidget::ApplyStyle()
{
	if (SlotButton)
	{
		SlotButton->SetBackgroundColor(bSelected ? SelectedColor : NormalColor);
	}
	SetColorAndOpacity(bAffordable ? FLinearColor::White : UnaffordableTint);
}

FText UBuildingBarSlotWidget::GetKeyText() const
{
	return KeyText ? KeyText->GetText() : FText::GetEmpty();
}

FText UBuildingBarSlotWidget::GetNameText() const
{
	return NameText ? NameText->GetText() : FText::GetEmpty();
}

FText UBuildingBarSlotWidget::GetCostText() const
{
	return CostText ? CostText->GetText() : FText::GetEmpty();
}

FText UBuildingBarSlotWidget::GetInitialText() const
{
	return InitialText ? InitialText->GetText() : FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
