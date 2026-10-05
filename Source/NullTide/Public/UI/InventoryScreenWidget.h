#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/Fragments/ItemFragmentTypes.h"
#include "UI/InventoryUICategory.h"
#include "InventoryScreenWidget.generated.h"

class AController;
class APawn;
class UButton;
class UImage;
class UInventoryComponent;
class UInventoryDragDropOperation;
class UInventorySlotWidget;
class UToolLoadoutComponent;
class UTextBlock;
class UWrapBox;
enum class EInventoryUIDropResult : uint8;

/** Structured read-only inventory projection with identity-based selection. */
UCLASS(Abstract, Blueprintable)
class NULLTIDE_API UInventoryScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void RebuildInventoryView();

	void RegisterDragOperation(UInventoryDragDropOperation* Operation);
	void CompleteDragOperation(UInventoryDragDropOperation* Operation, EInventoryUIDropResult Result);
	void HandleUnreceivedDragCancellation(UInventoryDragDropOperation* Operation, FVector2D ScreenPosition);

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	EInventoryCategory GetActiveCategory() const { return ActiveCategory; }

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	FGuid GetSelectedItemId() const { return SelectedItemId; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|UI", meta = (BindWidgetOptional))
	TObjectPtr<UWrapBox> InventoryContainerWrapBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TabAllButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TabResourceButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TabConsumableButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TabWeaponButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TabAmmoButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DetailsIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailsItemName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailsItemDescription;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailsCategory;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CapabilitySummary;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FooterCount;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> AxeToolIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AxeToolName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PickaxeToolIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PickaxeToolName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> EquipToolButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ToolActionButtonText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ToolActionText;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI")
	TSubclassOf<UInventorySlotWidget> SlotWidgetClass;

private:
	UFUNCTION()
	void HandleNativeInventoryChanged(int32 NewRevision);

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void HandleSlotSelected(FGuid ItemId);

	UFUNCTION()
	void ExecuteSelectedToolAction();

	UFUNCTION()
	void CloseInventory();

	UFUNCTION()
	void ShowAll();

	UFUNCTION()
	void ShowResources();

	UFUNCTION()
	void ShowConsumables();

	UFUNCTION()
	void ShowWeapons();

	UFUNCTION()
	void ShowAmmo();

	void BindControls();
	void RebindToPawn(APawn* Pawn);
	void ReleaseInventoryBinding();
	void SetActiveCategory(EInventoryCategory NewCategory);
	void RefreshDetails();
	void ClearDetails();
	void RefreshToolLoadout();
	void RefreshToolSlot(EToolType ToolType, UImage* Icon, UTextBlock* Name);
	EToolType GetSelectedToolType() const;
	void RefreshTabVisuals();
	bool IsVisibleInActiveCategory(const UItemInstance* Item) const;
	bool IsScreenPositionInsideInventoryWindow(FVector2D ScreenPosition) const;
	void CancelActiveDragIfInvalidOrHidden();

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> ObservedInventory;

	UPROPERTY(Transient)
	TObjectPtr<UToolLoadoutComponent> ObservedToolLoadout;

	UPROPERTY(Transient)
	TObjectPtr<AController> ObservedController;

	UPROPERTY(Transient)
	TObjectPtr<APawn> ObservedPawn;

	EInventoryCategory ActiveCategory = EInventoryCategory::All;
	FGuid SelectedItemId;
	TWeakObjectPtr<UInventoryDragDropOperation> ActiveDragOperation;
};
