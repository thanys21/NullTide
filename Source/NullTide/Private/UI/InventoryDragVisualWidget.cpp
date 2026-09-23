#include "UI/InventoryDragVisualWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "UI/InventoryUICategory.h"

void UInventoryDragVisualWidget::InitializeFromItem(const UItemInstance* Item)
{
	const UItemDefinition* Definition = Item
		? UInventoryUIFunctionLibrary::GetDefinition(Item->GetDefinitionClass())
		: nullptr;
	if (DragIcon)
	{
		DragIcon->SetBrushFromTexture(Definition ? Definition->ItemIcon.Get() : nullptr, true);
	}
	if (DragName)
	{
		DragName->SetText(Definition ? Definition->ItemName : FText::GetEmpty());
	}
}
