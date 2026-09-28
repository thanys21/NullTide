#include "UI/StorageSlotWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "UI/InventoryUICategory.h"

void UStorageSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ItemButton)
	{
		ItemButton->OnClicked.AddUniqueDynamic(this, &UStorageSlotWidget::HandleClicked);
	}
	RefreshVisuals();
}

void UStorageSlotWidget::InitializeSlot(UItemInstance* Item, const bool bInPlayerSide)
{
	BoundItem = Item;
	bPlayerSide = bInPlayerSide;
	RefreshVisuals();
}

void UStorageSlotWidget::SetSelected(const bool bInSelected)
{
	bSelected = bInSelected;
	RefreshVisuals();
}

FGuid UStorageSlotWidget::GetItemId() const
{
	return IsValid(BoundItem) ? BoundItem->GetInstanceId() : FGuid();
}

void UStorageSlotWidget::HandleClicked()
{
	const FGuid ItemId = GetItemId();
	if (ItemId.IsValid())
	{
		OnSlotSelected.Broadcast(ItemId, bPlayerSide);
	}
}

void UStorageSlotWidget::RefreshVisuals()
{
	const UItemDefinition* Definition = IsValid(BoundItem)
		? UInventoryUIFunctionLibrary::GetDefinition(BoundItem->GetDefinitionClass())
		: nullptr;
	if (ItemIcon)
	{
		ItemIcon->SetBrushFromTexture(Definition ? Definition->ItemIcon.Get() : nullptr, true);
	}
	if (ItemName)
	{
		ItemName->SetText(Definition ? Definition->ItemName : FText::GetEmpty());
	}
	if (SelectionBorder)
	{
		SelectionBorder->SetBrushColor(bSelected
			? FLinearColor(0.92f, 0.69f, 0.24f, 1.0f)
			: FLinearColor(0.16f, 0.18f, 0.17f, 1.0f));
	}
	SetToolTipText(Definition ? Definition->ItemDescription : FText::GetEmpty());
}
