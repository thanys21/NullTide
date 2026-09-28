#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StorageScreenWidget.generated.h"

class UButton;
class UInventoryComponent;
class UItemInstance;
class UStorageComponent;
class UStorageSlotWidget;
class UTextBlock;
class UWrapBox;

/** Dual-panel, projection-only storage screen with separate exact-ID selections. */
UCLASS(Abstract, Blueprintable)
class NULLTIDE_API UStorageScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenForStorage(UStorageComponent* Storage, const FText& InTitle);

	UFUNCTION(BlueprintCallable, Category = "Storage|UI")
	void CloseStorage();

	UFUNCTION(BlueprintPure, Category = "Storage|UI")
	FGuid GetSelectedPlayerItemId() const { return SelectedPlayerItemId; }

	UFUNCTION(BlueprintPure, Category = "Storage|UI")
	FGuid GetSelectedStorageItemId() const { return SelectedStorageItemId; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWrapBox> PlayerItemsWrapBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWrapBox> StorageItemsWrapBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerCountText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StorageCountText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StorageTitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TransferToStorageButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TransferToInventoryButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

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
	void TransferToStorage();

	UFUNCTION()
	void TransferToInventory();

	void RebindPlayerInventory();
	void ReleaseBindings();
	void RebuildViews();
	void RebuildPanel(UWrapBox* Panel, const TArray<UItemInstance*>& Items, bool bPlayerSide, FGuid SelectedItemId);
	void ValidateSelections();
	void RefreshCounts();

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> PlayerInventory;

	UPROPERTY(Transient)
	TObjectPtr<UStorageComponent> ActiveStorage;

	FGuid SelectedPlayerItemId;
	FGuid SelectedStorageItemId;
	FText StorageTitle;
};
