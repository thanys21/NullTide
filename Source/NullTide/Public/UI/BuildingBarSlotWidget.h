#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BuildingBarSlotWidget.generated.h"

class UBuildingBarWidget;
class UBuildingDefinition;
class UButton;
class UImage;
class UTextBlock;

/**
 * One slot of the building mode build bar (B8): a registered building (key, icon or initial, name, cost) or the
 * demolish slot. Display only: a click forwards to the owning bar, which asks the placement component to select.
 * The button must not be focusable (authored on the Blueprint) so keys keep reaching the game.
 */
UCLASS(Abstract)
class NULLTIDE_API UBuildingBarSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Fills the slot. A null building makes it the demolish slot. */
	void SetupSlot(UBuildingBarWidget* InBar, int32 InSlotIndex, const FText& InKey, TSubclassOf<UBuildingDefinition> InBuilding);

	void SetSelected(bool bInSelected);
	void SetAffordable(bool bInAffordable);

	/** The click path (also used by tests): forwards to the bar. */
	UFUNCTION(BlueprintCallable, Category = "Building|UI")
	void HandleSlotClicked();

	int32 GetSlotIndex() const { return SlotIndex; }
	TSubclassOf<UBuildingDefinition> GetBuilding() const { return Building; }
	bool IsDemolishSlot() const { return !Building; }
	bool IsSelected() const { return bSelected; }
	bool IsAffordable() const { return bAffordable; }
	UButton* GetSlotButton() const { return SlotButton; }
	FText GetKeyText() const;
	FText GetNameText() const;
	FText GetCostText() const;
	FText GetInitialText() const;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, Category = "Building|UI", meta = (BindWidget))
	TObjectPtr<UButton> SlotButton;

	UPROPERTY(BlueprintReadOnly, Category = "Building|UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, Category = "Building|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> KeyText;

	UPROPERTY(BlueprintReadOnly, Category = "Building|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CostText;

	/** The building's icon; without one it shows a coloured square under InitialText. */
	UPROPERTY(BlueprintReadOnly, Category = "Building|UI", meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	/** The name's first letter, shown when the building has no icon. */
	UPROPERTY(BlueprintReadOnly, Category = "Building|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InitialText;

	UPROPERTY(EditAnywhere, Category = "Building|UI")
	FLinearColor NormalColor = FLinearColor(0.08f, 0.08f, 0.08f, 0.85f);

	UPROPERTY(EditAnywhere, Category = "Building|UI")
	FLinearColor SelectedColor = FLinearColor(0.95f, 0.65f, 0.15f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Building|UI")
	FLinearColor PlaceholderIconColor = FLinearColor(0.25f, 0.4f, 0.55f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Building|UI")
	FLinearColor DemolishIconColor = FLinearColor(0.6f, 0.2f, 0.15f, 1.f);

	/** Tint of the whole slot when the builder cannot afford the building. */
	UPROPERTY(EditAnywhere, Category = "Building|UI")
	FLinearColor UnaffordableTint = FLinearColor(0.5f, 0.5f, 0.5f, 0.6f);

private:
	void ApplyStyle();

	TWeakObjectPtr<UBuildingBarWidget> Bar;
	int32 SlotIndex = INDEX_NONE;
	TSubclassOf<UBuildingDefinition> Building;
	bool bSelected = false;
	bool bAffordable = true;
};
