#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StorageSlotWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class UItemInstance;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStorageSlotSelected, FGuid, ItemId, bool, bPlayerSide);

/** Minimal selectable slot used by both sides of the G7 storage screen. */
UCLASS(Abstract, Blueprintable)
class NULLTIDE_API UStorageSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeSlot(UItemInstance* Item, bool bInPlayerSide);
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "Storage|UI")
	FGuid GetItemId() const;

	UPROPERTY(BlueprintAssignable, Category = "Storage|UI")
	FOnStorageSlotSelected OnSlotSelected;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ItemButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SelectionBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemName;

private:
	UFUNCTION()
	void HandleClicked();

	void RefreshVisuals();

	UPROPERTY(Transient)
	TObjectPtr<UItemInstance> BoundItem;

	bool bPlayerSide = true;
	bool bSelected = false;
};
