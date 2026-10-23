#include "UI/NpcTradeItemRowWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Items/ItemDefinition.h"
#include "UI/NpcTradeScreenWidget.h"

void UNpcTradeItemRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// RowButton is non-focusable in W_NpcTradeItemRow (a UButton's focusability is fixed when it is constructed).
	if (RowButton)
	{
		RowButton->OnClicked.AddUniqueDynamic(this, &UNpcTradeItemRowWidget::HandleClicked);
	}
}

void UNpcTradeItemRowWidget::Setup(UNpcTradeScreenWidget* InScreen, const TSubclassOf<UItemDefinition> InItem, const ENpcTradeRowSource InSource,
	const int32 InQuantity, const FText& InPriceText)
{
	Screen = InScreen;
	Item = InItem;
	Source = InSource;
	Quantity = InQuantity;
	const UItemDefinition* Definition = InItem ? InItem->GetDefaultObject<UItemDefinition>() : nullptr;
	if (NameText)
	{
		NameText->SetText(Definition ? Definition->ItemName : FText::GetEmpty());
	}
	if (QuantityText)
	{
		// The store is unlimited: no count there.
		QuantityText->SetText(InSource == ENpcTradeRowSource::Store ? FText::GetEmpty()
			: FText::Format(NSLOCTEXT("NpcTrade", "RowQuantity", "×{0}"), FText::AsNumber(InQuantity)));
	}
	if (PriceText)
	{
		PriceText->SetText(InPriceText);
	}
	if (IconImage)
	{
		UTexture2D* Icon = Definition ? Definition->ItemIcon.Get() : nullptr;
		IconImage->SetBrushFromTexture(Icon, false);
		IconImage->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UNpcTradeItemRowWidget::ClickRow()
{
	if (UNpcTradeScreenWidget* Owner = Screen.Get())
	{
		Owner->HandleRowClicked(*this);
	}
}

void UNpcTradeItemRowWidget::HandleClicked()
{
	ClickRow();
}

FText UNpcTradeItemRowWidget::GetNameText() const
{
	return NameText ? NameText->GetText() : FText::GetEmpty();
}

FText UNpcTradeItemRowWidget::GetQuantityText() const
{
	return QuantityText ? QuantityText->GetText() : FText::GetEmpty();
}

FText UNpcTradeItemRowWidget::GetPriceText() const
{
	return PriceText ? PriceText->GetText() : FText::GetEmpty();
}
