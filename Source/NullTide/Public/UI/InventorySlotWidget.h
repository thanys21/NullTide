#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/InventoryUICategory.h"
#include "InventorySlotWidget.generated.h"

class UBorder;
class UImage;
class UInventoryComponent;
class UInventoryDragDropOperation;
class UInventoryDragVisualWidget;
class UInventoryScreenWidget;
class UItemInstance;
class UProgressBar;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventorySlotSelected, FGuid, ItemId);

/** One reusable, identity-safe inventory slot for every item category. */
UCLASS(Abstract, Blueprintable)
class NULLTIDE_API UInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeSlot(UItemInstance* Item, UInventoryComponent* Inventory, UInventoryScreenWidget* InOwnerScreen);
	void SetSelected(bool bInSelected);
	void SetDragging(bool bInDragging);
	void SetDropTargetState(bool bInDropTarget, bool bInInvalidDropTarget = false);
	bool IsItemCurrent() const;
	FGuid GetItemId() const;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|UI")
	FOnInventorySlotSelected OnSlotSelected;

protected:
	virtual void NativeDestruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SlotBorder;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|UI", meta = (BindWidgetOptional))
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CategoryIndicator;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> DurabilityBar;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI")
	TSubclassOf<UInventoryDragVisualWidget> DragVisualClass;

private:
	void RefreshContent();
	void RefreshVisualState();
	FLinearColor GetCategoryColor() const;

	UPROPERTY(Transient)
	TObjectPtr<UItemInstance> BoundItem;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> SourceInventory;

	TWeakObjectPtr<UInventoryScreenWidget> OwnerScreen;
	EInventoryCategory Category = EInventoryCategory::Misc;
	bool bSelected = false;
	bool bHovered = false;
	bool bDragging = false;
	bool bDropTarget = false;
	bool bInvalidDropTarget = false;
};
