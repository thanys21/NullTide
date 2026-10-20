#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/StorageSlotWidget.h"
#include "StorageScreenWidget.generated.h"

class UButton;
class UImage;
class UInventoryComponent;
class UItemInstance;
class UStorageComponent;
class UStorageDragDropOperation;
class UTextBlock;
class UWrapBox;

/**
 * The storage screen (G7, G9): the player's inventory on the left, the selected item's info in the middle, the
 * storage on the right. Projection only: it never mutates items. Every action goes through UStorageTransferLibrary
 * (move a stack, move every stack of an item, move all, each as much as fits) or a container's TrySplitStack.
 * Both sides are sorted for display (category, name, quantity); the containers' own order is unchanged. Generic over
 * any UStorageComponent (chests, later loot containers).
 */
UCLASS(Abstract, Blueprintable)
class NULLTIDE_API UStorageScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenForStorage(UStorageComponent* Storage, const FText& InTitle);

	UFUNCTION(BlueprintCallable, Category = "Storage|UI")
	void CloseStorage();

	/** The selected stack's id when it is on the player side, else invalid. */
	UFUNCTION(BlueprintPure, Category = "Storage|UI")
	FGuid GetSelectedPlayerItemId() const { return bSelectedPlayerSide ? SelectedItemId : FGuid(); }

	/** The selected stack's id when it is on the storage side, else invalid. */
	UFUNCTION(BlueprintPure, Category = "Storage|UI")
	FGuid GetSelectedStorageItemId() const { return bSelectedPlayerSide ? FGuid() : SelectedItemId; }

	/** A slot's modified click (G9): calls the authority once and shows the result. Returns whether it changed items. */
	bool HandleSlotAction(FGuid ItemId, bool bPlayerSide, EStorageSlotAction Action);

	/** A drop on one side's panel (G9): moves the dragged stack there; the same side or a stale payload does nothing. */
	bool HandleDropOnSide(const UStorageDragDropOperation* Operation, bool bTargetPlayerSide);

	/** ALL → : every inventory stack into the storage, as much as fits. */
	UFUNCTION(BlueprintCallable, Category = "Storage|UI")
	void MoveAllToStorage();

	/** ← ALL : every storage stack into the inventory, as much as fits. */
	UFUNCTION(BlueprintCallable, Category = "Storage|UI")
	void MoveAllToInventory();

	/** I, Tab and Esc close the screen (the inventory screen's keys). */
	static bool IsCloseKey(const FKey& Key);

	/** Display order of one side: category, then name, then quantity (largest first), then ItemId. */
	static void SortForDisplay(TArray<UItemInstance*>& Items);

	/** Test seam: the player inventory to use instead of the owning pawn's (no player controller in a test world). */
	void SetPlayerInventoryOverride(UInventoryComponent* Inventory);

	FText GetFeedbackText() const;
	FText GetDetailsNameText() const;
	FText GetDetailsQuantityText() const;
	/** The slots of one side, in display order. */
	TArray<UStorageSlotWidget*> GetSlots(bool bPlayerSide) const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWrapBox> PlayerItemsWrapBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWrapBox> StorageItemsWrapBox;

	/** Drop zone of the player side (its whole panel); falls back to PlayerItemsWrapBox. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PlayerPanel;

	/** Drop zone of the storage side (its whole panel); falls back to StorageItemsWrapBox. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StoragePanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerCountText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StorageCountText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StorageTitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TransferAllToStorageButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TransferAllToInventoryButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	/** Middle panel: the selected stack's info. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DetailsIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailsItemName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailsCategory;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailsQuantity;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailsDescription;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailsCapabilities;

	/** Controls hint (set natively). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HintText;

	/** Result of the last action ("Moved 12 Wood", "Chest is full"). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FeedbackText;

	UPROPERTY(EditDefaultsOnly, Category = "Storage|UI")
	TSubclassOf<UStorageSlotWidget> SlotWidgetClass;

private:
	UFUNCTION()
	void HandlePlayerInventoryChanged(int32 NewRevision);

	UFUNCTION()
	void HandleStorageChanged(int32 NewRevision);

	UFUNCTION()
	void HandleSlotSelected(FGuid ItemId, bool bPlayerSide);

	UFUNCTION()
	void HandleSlotActionRequested(FGuid ItemId, bool bPlayerSide, EStorageSlotAction Action);

	void RebindPlayerInventory();
	UInventoryComponent* ResolvePlayerInventory() const;
	void ReleaseBindings();
	void RebuildViews();
	void RebuildPanel(UWrapBox* Panel, TArray<UItemInstance*> Items, bool bPlayerSide);
	void ValidateSelection();
	void RefreshSelectionVisuals();
	void RefreshDetails();
	void RefreshCounts();
	void SetFeedback(const FText& Message);
	UItemInstance* FindSideItem(bool bPlayerSide, FGuid ItemId) const;
	FText DescribeFull(bool bTargetPlayerSide) const;
	/** The storage's title, or "The chest" when it has none. */
	FText GetStorageDisplayTitle() const;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> PlayerInventory;

	UPROPERTY(Transient)
	TObjectPtr<UStorageComponent> ActiveStorage;

	TWeakObjectPtr<UInventoryComponent> PlayerInventoryOverride;

	/** One selection at a time, on either side; it drives the middle panel. */
	FGuid SelectedItemId;
	bool bSelectedPlayerSide = true;
	FText StorageTitle;
};
