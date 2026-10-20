#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StorageSlotWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class UInventoryDragVisualWidget;
class UItemInstance;
class UTextBlock;

/** What a click on a storage slot asks for (G9). */
UENUM(BlueprintType)
enum class EStorageSlotAction : uint8
{
	None,
	/** LMB: select and show it in the middle (a drag can start). */
	Select,
	/** Shift + LMB: move this stack to the other side, as much as fits. */
	MoveStack,
	/** Ctrl + LMB: move every stack of this item to the other side, as much as fits. */
	MoveAllOfItem,
	/** Shift + RMB: split this stack 50% in place. */
	Split
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStorageSlotSelected, FGuid, ItemId, bool, bPlayerSide);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnStorageSlotAction, FGuid, ItemId, bool, bPlayerSide, EStorageSlotAction, Action);

/**
 * One slot of the storage screen, on either side. Display only: it reports a selection, a modified click (G9) or a
 * drag; the screen calls the authority.
 */
UCLASS(Abstract, Blueprintable)
class NULLTIDE_API UStorageSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeSlot(UItemInstance* Item, bool bInPlayerSide);
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "Storage|UI")
	FGuid GetItemId() const;

	bool IsPlayerSide() const { return bPlayerSide; }
	bool IsSelected() const { return bSelected; }
	FText GetQuantityText() const;

	/**
	 * The input mapping (G9, pure): LMB selects, Shift + LMB moves the stack, Ctrl + LMB moves every stack of the item,
	 * Shift + RMB splits. Anything else (RMB alone, Ctrl + RMB, other buttons) does nothing.
	 */
	static EStorageSlotAction ResolveSlotAction(const FKey& Button, bool bShiftDown, bool bControlDown);

	/** The click path (also used by tests): broadcasts the action for this slot's item. Returns whether it acted. */
	bool HandleSlotInput(const FKey& Button, bool bShiftDown, bool bControlDown);

	UPROPERTY(BlueprintAssignable, Category = "Storage|UI")
	FOnStorageSlotSelected OnSlotSelected;

	UPROPERTY(BlueprintAssignable, Category = "Storage|UI")
	FOnStorageSlotAction OnSlotAction;

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	/** A fast second click arrives as a double click with no preview pass; it acts like a single click (G9 review). */
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ItemButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SelectionBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ItemIcon;

	/** The item name (also in the tooltip). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemName;

	/** Quantity badge ("37"); empty for a single item. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

	/** Drag visual (W_InventoryDragVisual). */
	UPROPERTY(EditDefaultsOnly, Category = "Storage|UI")
	TSubclassOf<UInventoryDragVisualWidget> DragVisualClass;

private:
	void RefreshVisuals();

	UPROPERTY(Transient)
	TObjectPtr<UItemInstance> BoundItem;

	bool bPlayerSide = true;
	bool bSelected = false;
};
