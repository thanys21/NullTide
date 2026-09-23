#include "UI/InventoryDragDropOperation.h"

#include "Items/ItemInstance.h"
#include "Inventory/InventoryComponent.h"
#include "UI/InventorySlotWidget.h"
#include "UI/InventoryUICategory.h"

void UInventoryDragDropOperation::InitializePayload(UInventoryComponent* InSourceInventory, UItemInstance* InItemInstance)
{
	SourceInventory = InSourceInventory;
	ItemInstance = InItemInstance;
	ItemId = InItemInstance ? InItemInstance->GetInstanceId() : FGuid();
	Result = EInventoryUIDropResult::Invalid;
	bComplete = false;
}

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
	return !bComplete
		&& SourceInventory.IsValid()
		&& SourceInventory.Get() == Inventory
		&& UInventoryUIFunctionLibrary::IsDragPayloadValid(Inventory, ItemId, ItemInstance);
}

void UInventoryDragDropOperation::Complete(const EInventoryUIDropResult InResult)
{
	if (bComplete)
	{
		return;
	}

	bComplete = true;
	Result = InResult;
	ResetSourceVisual();
}
