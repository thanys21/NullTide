#include "UI/InventorySlotWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Input/Reply.h"
#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "Items/Fragments/ItemFragment_Durability.h"
#include "UI/InventoryDragDropOperation.h"
#include "UI/InventoryDragVisualWidget.h"
#include "UI/InventoryScreenWidget.h"

void UInventorySlotWidget::InitializeSlot(UItemInstance* Item, UInventoryComponent* Inventory, UInventoryScreenWidget* InOwnerScreen)
{
	BoundItem = Item;
	SourceInventory = Inventory;
	OwnerScreen = InOwnerScreen;
	bSelected = false;
	bHovered = false;
	bDragging = false;
	bDropTarget = false;
	bInvalidDropTarget = false;
	Category = UInventoryUIFunctionLibrary::ResolveItemCategory(Item);
	RefreshContent();
}

void UInventorySlotWidget::SetSelected(const bool bInSelected)
{
	bSelected = bInSelected;
	RefreshVisualState();
}

void UInventorySlotWidget::SetDragging(const bool bInDragging)
{
	bDragging = bInDragging;
	RefreshVisualState();
}

void UInventorySlotWidget::SetDropTargetState(const bool bInDropTarget, const bool bInInvalidDropTarget)
{
	bDropTarget = bInDropTarget;
	bInvalidDropTarget = bInDropTarget && bInInvalidDropTarget;
	RefreshVisualState();
}

bool UInventorySlotWidget::IsItemCurrent() const
{
	return UInventoryUIFunctionLibrary::IsCurrentItem(SourceInventory, BoundItem);
}

FGuid UInventorySlotWidget::GetItemId() const
{
	return BoundItem ? BoundItem->GetInstanceId() : FGuid();
}

void UInventorySlotWidget::NativeDestruct()
{
	BoundItem = nullptr;
	SourceInventory = nullptr;
	OwnerScreen = nullptr;
	OnSlotSelected.Clear();
	Super::NativeDestruct();
}

void UInventorySlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHovered = true;
	RefreshVisualState();
}

void UInventorySlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHovered = false;
	RefreshVisualState();
}

FReply UInventorySlotWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && IsItemCurrent())
	{
		OnSlotSelected.Broadcast(GetItemId());
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

void UInventorySlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (!IsItemCurrent())
	{
		RefreshVisualState();
		return;
	}

	auto* Operation = NewObject<UInventoryDragDropOperation>(this);
	Operation->InitializePayload(SourceInventory, BoundItem);
	Operation->Pivot = EDragPivot::MouseDown;
	Operation->SetSourceSlot(this);

	if (DragVisualClass)
	{
		if (auto* Visual = CreateWidget<UInventoryDragVisualWidget>(GetOwningPlayer(), DragVisualClass))
		{
			Visual->InitializeFromItem(BoundItem);
			Operation->DefaultDragVisual = Visual;
		}
	}

	bDragging = true;
	RefreshVisualState();
	if (OwnerScreen.IsValid())
	{
		OwnerScreen->RegisterDragOperation(Operation);
	}
	OutOperation = Operation;
}

void UInventorySlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);
	if (const auto* Operation = Cast<UInventoryDragDropOperation>(InOperation))
	{
		const bool bValidTarget = Operation->IsPayloadValid(SourceInventory)
			&& IsItemCurrent()
			&& Operation->ItemId != GetItemId();
		SetDropTargetState(true, !bValidTarget);
	}
}

void UInventorySlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	SetDropTargetState(false);
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

bool UInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	SetDropTargetState(false);
	if (auto* Operation = Cast<UInventoryDragDropOperation>(InOperation))
	{
		const bool bValidTarget = Operation->IsPayloadValid(SourceInventory)
			&& IsItemCurrent()
			&& Operation->ItemId != GetItemId();
		if (OwnerScreen.IsValid())
		{
			OwnerScreen->CompleteDragOperation(Operation,
				bValidTarget ? EInventoryUIDropResult::AcceptedNoMutation : EInventoryUIDropResult::Invalid);
		}
		else
		{
			Operation->Complete(bValidTarget ? EInventoryUIDropResult::AcceptedNoMutation : EInventoryUIDropResult::Invalid);
		}
		return true;
	}
	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UInventorySlotWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (auto* Operation = Cast<UInventoryDragDropOperation>(InOperation))
	{
		if (OwnerScreen.IsValid())
		{
			OwnerScreen->HandleUnreceivedDragCancellation(Operation, InDragDropEvent.GetScreenSpacePosition());
		}
		else
		{
			Operation->Complete(EInventoryUIDropResult::Cancelled);
		}
	}
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
}

void UInventorySlotWidget::RefreshContent()
{
	const UItemDefinition* Definition = IsItemCurrent()
		? UInventoryUIFunctionLibrary::GetDefinition(BoundItem->GetDefinitionClass())
		: nullptr;

	if (ItemIcon)
	{
		ItemIcon->SetBrushFromTexture(Definition ? Definition->ItemIcon.Get() : nullptr, true);
	}
	if (CategoryIndicator)
	{
		const FText CategoryDisplayName = UInventoryUIFunctionLibrary::GetCategoryDisplayName(Category);
		CategoryIndicator->SetText(FText::FromString(CategoryDisplayName.ToString().Left(1).ToUpper()));
		CategoryIndicator->SetColorAndOpacity(GetCategoryColor());
	}
	if (DurabilityBar)
	{
		const bool bHasDurability = Definition
			&& Definition->HasFragmentByClass(UItemFragment_Durability::StaticClass());
		DurabilityBar->SetVisibility(bHasDurability ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		DurabilityBar->SetPercent(1.0f);
	}
	if (Definition)
	{
		SetToolTipText(FText::FromString(FString::Printf(TEXT("%s\n%s\n%s"),
			*Definition->ItemName.ToString(),
			*Definition->ItemDescription.ToString(),
			*UInventoryUIFunctionLibrary::GetCategoryDisplayName(Category).ToString())));
	}
	else
	{
		SetToolTipText(FText::FromString(TEXT("Item is no longer in this inventory.")));
	}
	RefreshVisualState();
}

void UInventorySlotWidget::RefreshVisualState()
{
	if (!SlotBorder)
	{
		return;
	}
	FLinearColor Color = GetCategoryColor();
	if (!IsItemCurrent())
	{
		Color = FLinearColor(0.45f, 0.12f, 0.10f, 1.0f);
	}
	else if (bDragging)
	{
		Color = FLinearColor(Color.R, Color.G, Color.B, 0.35f);
	}
	else if (bInvalidDropTarget)
	{
		Color = FLinearColor(0.45f, 0.12f, 0.10f, 1.0f);
	}
	else if (bDropTarget)
	{
		Color = FLinearColor(0.20f, 0.66f, 0.72f, 1.0f);
	}
	else if (bSelected)
	{
		Color = FLinearColor(0.92f, 0.69f, 0.24f, 1.0f);
	}
	else if (bHovered)
	{
		Color = Color * 1.35f;
		Color.A = 1.0f;
	}
	SlotBorder->SetBrushColor(Color);
	SetRenderOpacity(bDragging ? 0.65f : 1.0f);
}

FLinearColor UInventorySlotWidget::GetCategoryColor() const
{
	switch (Category)
	{
	case EInventoryCategory::Resource: return FLinearColor(0.30f, 0.49f, 0.29f, 1.0f);
	case EInventoryCategory::Consumable: return FLinearColor(0.64f, 0.27f, 0.23f, 1.0f);
	case EInventoryCategory::Weapon: return FLinearColor(0.38f, 0.48f, 0.58f, 1.0f);
	case EInventoryCategory::Ammo: return FLinearColor(0.67f, 0.52f, 0.22f, 1.0f);
	default: return FLinearColor(0.34f, 0.35f, 0.33f, 1.0f);
	}
}
