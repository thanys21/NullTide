#include "UI/CraftingRequirementRowWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "NullTideCraftingUI"

namespace
{
const FLinearColor SatisfiedTextColor(0.82f, 0.86f, 0.78f, 1.f);
const FLinearColor MissingTextColor(0.95f, 0.42f, 0.36f, 1.f);
}

FText UCraftingRequirementRowWidget::FormatQuantity(const FCraftingRequirementView& InView, const bool bInIsOutput)
{
	return bInIsOutput
		? FText::Format(LOCTEXT("OutputQuantity", "x{0}"), FText::AsNumber(InView.RequiredQuantity))
		: FText::Format(LOCTEXT("IngredientQuantity", "{0} / {1}"),
			FText::AsNumber(InView.OwnedQuantity), FText::AsNumber(InView.RequiredQuantity));
}

void UCraftingRequirementRowWidget::SetRequirement(const FCraftingRequirementView& InView, const bool bInIsOutput)
{
	View = InView;
	bIsOutput = bInIsOutput;
	const FSlateColor TextColor(View.bSatisfied ? SatisfiedTextColor : MissingTextColor);
	if (ItemIcon)
	{
		ItemIcon->SetBrushFromTexture(View.ItemIcon);
		ItemIcon->SetVisibility(View.ItemIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (ItemName)
	{
		ItemName->SetText(View.ItemName);
		ItemName->SetColorAndOpacity(TextColor);
	}
	if (QuantityText)
	{
		QuantityText->SetText(FormatQuantity(View, bIsOutput));
		QuantityText->SetColorAndOpacity(TextColor);
	}
}

#undef LOCTEXT_NAMESPACE
