#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BuildingHudWidget.generated.h"

class UBuildingBarWidget;
class UBuildingPlacementComponent;
class UTextBlock;

/**
 * Building mode HUD: the selected building, its cost against the builder's inventory, and the placement status.
 * Display only. UBuildingPlacementComponent pushes the texts and owns the widget's lifetime. The texts never take
 * the cursor; the optional build bar (B8) is the only hit-testable part.
 */
UCLASS(Abstract)
class NULLTIDE_API UBuildingHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Building|UI")
	void SetBuildingText(const FText& BuildingName, const FText& Cost);

	/** Status line; valid placements and successful builds use the valid color. */
	UFUNCTION(BlueprintCallable, Category = "Building|UI")
	void SetStatusText(const FText& Status, bool bValid);

	/** Controls hint for the current sub-mode (build or demolish). */
	UFUNCTION(BlueprintCallable, Category = "Building|UI")
	void SetHintText(const FText& Hint);

	FText GetBuildingNameText() const;
	FText GetCostText() const;
	FText GetStatusText() const;
	FText GetHintText() const;
	bool IsStatusValid() const { return bStatusValid; }

	/** Builds the optional build bar for this placement component (B8). Returns whether a bar with slots exists. */
	bool InitializeBuildBar(UBuildingPlacementComponent* Placement);

	/** Highlights the selected building or demolish on the build bar; no-op when unchanged or without a bar. */
	void SetBuildBarSelection(int32 SelectedIndex, bool bDemolish);

	/** Unbinds the build bar from the builder (building mode exit). */
	void ReleaseBuildBar();

	bool HasBuildBar() const;
	bool IsBuildBarHovered() const;
	UBuildingBarWidget* GetBuildBar() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Building|UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> BuildingNameText;

	UPROPERTY(BlueprintReadOnly, Category = "Building|UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> CostText;

	UPROPERTY(BlueprintReadOnly, Category = "Building|UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	/** Optional controls hint, set per sub-mode by the placement component. */
	UPROPERTY(BlueprintReadOnly, Category = "Building|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HintText;

	/** Optional build bar (W_BuildingBar): one slot per building plus demolish (B8). */
	UPROPERTY(BlueprintReadOnly, Category = "Building|UI", meta = (BindWidgetOptional))
	TObjectPtr<UBuildingBarWidget> BuildBar;

	UPROPERTY(EditAnywhere, Category = "Building|UI")
	FLinearColor ValidStatusColor = FLinearColor(0.35f, 1.f, 0.4f);

	UPROPERTY(EditAnywhere, Category = "Building|UI")
	FLinearColor InvalidStatusColor = FLinearColor(1.f, 0.35f, 0.3f);

private:
	bool bStatusValid = false;
};
