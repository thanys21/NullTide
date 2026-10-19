#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BuildingBarWidget.generated.h"

class UBuildingBarSlotWidget;
class UBuildingPlacementComponent;
class UInventoryComponent;
class UPanelWidget;

/**
 * Building mode build bar (B8): one slot per registered building in AvailableBuildings order (keys 1-5), then a
 * demolish slot (C). The selected slot follows the placement component; slots grey out when the builder's inventory
 * cannot pay (exact read-only CanApplyDefinitionTransaction, refreshed on OnInventoryChanged).
 * Display only: it never builds, demolishes or changes the inventory; a slot click asks the placement component to
 * select (RequestSelectBuilding) or toggles demolish. It lives and dies with the building HUD.
 */
UCLASS(Abstract)
class NULLTIDE_API UBuildingBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Builds the slots for this placement component's builder and binds its inventory. Rebuilds if called again. Returns whether any slot exists. */
	bool InitializeBar(UBuildingPlacementComponent* InPlacement);

	/** Highlights the building slot at SelectedIndex, or the demolish slot. No-op when unchanged. */
	void SetSelection(int32 SelectedIndex, bool bDemolish);

	/** Recomputes which buildings the builder can pay for. */
	void RefreshAffordability();

	/** The slot with this index was clicked (building index; demolish = the building count): select it (leaving demolish) or toggle demolish. */
	void HandleSlotClicked(int32 SlotIndex);

	/** Unbinds the inventory and forgets the placement component (building mode exit; also on destruct). */
	void ReleaseBar();

	/** Whether the cursor is over the bar (a confirm click there must not build). */
	bool IsBarHovered() const { return IsHovered(); }

	TArray<UBuildingBarSlotWidget*> GetSlots() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	/** Panel the slots are added to (a horizontal box). */
	UPROPERTY(BlueprintReadOnly, Category = "Building|UI", meta = (BindWidget))
	TObjectPtr<UPanelWidget> SlotContainer;

	/** Slot widget class (W_BuildingBarSlot). */
	UPROPERTY(EditAnywhere, Category = "Building|UI")
	TSubclassOf<UBuildingBarSlotWidget> SlotClass;

private:
	UFUNCTION()
	void HandleInventoryChanged(int32 NewRevision);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBuildingBarSlotWidget>> Slots;

	TWeakObjectPtr<UBuildingPlacementComponent> Placement;
	TWeakObjectPtr<UInventoryComponent> Inventory;
	int32 ShownIndex = INDEX_NONE;
	bool bShownDemolish = false;
	bool bHasShownSelection = false;
};
