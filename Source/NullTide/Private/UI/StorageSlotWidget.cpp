#include "UI/StorageSlotWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "UI/InventoryDragVisualWidget.h"
#include "UI/InventoryUICategory.h"
#include "UI/StorageDragDropOperation.h"

void UStorageSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
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
	if (bSelected != bInSelected)
	{
		bSelected = bInSelected;
		RefreshVisuals();
	}
}

FGuid UStorageSlotWidget::GetItemId() const
{
	return IsValid(BoundItem) ? BoundItem->GetInstanceId() : FGuid();
}

FText UStorageSlotWidget::GetQuantityText() const
{
	return QuantityText ? QuantityText->GetText() : FText::GetEmpty();
}

EStorageSlotAction UStorageSlotWidget::ResolveSlotAction(const FKey& Button, const bool bShiftDown, const bool bControlDown)
{
	if (Button == EKeys::LeftMouseButton)
	{
		// Ctrl takes precedence when both are held: it is the wider move.
		return bControlDown ? EStorageSlotAction::MoveAllOfItem
			: bShiftDown ? EStorageSlotAction::MoveStack
			: EStorageSlotAction::Select;
	}
	if (Button == EKeys::RightMouseButton && bShiftDown && !bControlDown)
	{
		return EStorageSlotAction::Split;
	}
	return EStorageSlotAction::None;
}

bool UStorageSlotWidget::HandleSlotInput(const FKey& Button, const bool bShiftDown, const bool bControlDown)
{
	const EStorageSlotAction Action = ResolveSlotAction(Button, bShiftDown, bControlDown);
	const FGuid ItemId = GetItemId();
	if (Action == EStorageSlotAction::None || !ItemId.IsValid())
	{
		return false;
	}
	// Last: a move rebuilds the panels and may remove this slot, so nothing touches it afterwards.
	if (Action == EStorageSlotAction::Select)
	{
		OnSlotSelected.Broadcast(ItemId, bPlayerSide);
	}
	else
	{
		OnSlotAction.Broadcast(ItemId, bPlayerSide, Action);
	}
	return true;
}

FReply UStorageSlotWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// The preview pass runs before the button, so modifiers and the right button reach the slot (G9).
	const FKey Button = InMouseEvent.GetEffectingButton();
	const EStorageSlotAction Action = ResolveSlotAction(Button, InMouseEvent.IsShiftDown(), InMouseEvent.IsControlDown());
	if (Action == EStorageSlotAction::Select && GetItemId().IsValid())
	{
		// Selecting only restyles the slots, so this slot is still in its panel for the drag detection.
		HandleSlotInput(Button, false, false);
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}
	if (Action != EStorageSlotAction::None && HandleSlotInput(Button, InMouseEvent.IsShiftDown(), InMouseEvent.IsControlDown()))
	{
		return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UStorageSlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Slate routes a quick second click only here (bubbling, no preview), so without this a fast repeat (a second
	// split, or the next sorted stack moving under the cursor) would be lost. A plain double click only selects.
	if (HandleSlotInput(InMouseEvent.GetEffectingButton(), InMouseEvent.IsShiftDown(), InMouseEvent.IsControlDown()))
	{
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

void UStorageSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	if (!GetItemId().IsValid())
	{
		return;
	}
	UStorageDragDropOperation* Operation = NewObject<UStorageDragDropOperation>(this);
	Operation->ItemId = GetItemId();
	Operation->bFromPlayerSide = bPlayerSide;
	Operation->Pivot = EDragPivot::MouseDown;
	if (DragVisualClass)
	{
		if (UInventoryDragVisualWidget* Visual = CreateWidget<UInventoryDragVisualWidget>(this, DragVisualClass))
		{
			Visual->InitializeFromItem(BoundItem);
			Operation->DefaultDragVisual = Visual;
		}
	}
	OutOperation = Operation;
}

void UStorageSlotWidget::RefreshVisuals()
{
	const UItemDefinition* Definition = IsValid(BoundItem)
		? UInventoryUIFunctionLibrary::GetDefinition(BoundItem->GetDefinitionClass())
		: nullptr;
	const int32 Quantity = Definition ? BoundItem->GetQuantity() : 0;
	if (ItemIcon)
	{
		ItemIcon->SetBrushFromTexture(Definition ? Definition->ItemIcon.Get() : nullptr, true);
	}
	if (ItemName)
	{
		ItemName->SetText(Definition ? Definition->ItemName : FText::GetEmpty());
	}
	if (QuantityText)
	{
		QuantityText->SetText(Quantity > 1 ? FText::AsNumber(Quantity) : FText::GetEmpty());
	}
	if (SelectionBorder)
	{
		SelectionBorder->SetBrushColor(bSelected
			? FLinearColor(0.92f, 0.69f, 0.24f, 1.0f)
			: FLinearColor(0.16f, 0.18f, 0.17f, 1.0f));
	}
	SetToolTipText(Definition ? Definition->ItemName : FText::GetEmpty());
}
