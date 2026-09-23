#include "UI/InventoryDragDropOperation.h"

#include "UI/InventorySlotWidget.h"
#include "UI/InventoryUICategory.h"

void UInventoryDragDropOperation::SetSourceSlot(UInventorySlotWidget* InSourceSlot)
{
	SourceSlot = InSourceSlot;
}

void UInventoryDragDropOperation::ResetSourceVisual()
{
	if (SourceSlot.IsValid())
	{
		SourceSlot->SetDragging(false);
	}
}

bool UInventoryDragDropOperation::IsPayloadValid(const UInventoryComponent* Inventory) const
{
	return UInventoryUIFunctionLibrary::IsDragPayloadValid(Inventory, ItemId, ItemInstance);
}
