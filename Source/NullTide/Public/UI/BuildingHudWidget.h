#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BuildingHudWidget.generated.h"

class UTextBlock;

/**
 * Building mode HUD: the selected building, its cost against the builder's inventory, and the placement status.
 * Display only. UBuildingPlacementComponent pushes the texts and owns the widget's lifetime; the widget never
 * blocks the cursor.
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

	FText GetBuildingNameText() const;
	FText GetCostText() const;
	FText GetStatusText() const;
	bool IsStatusValid() const { return bStatusValid; }

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Building|UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> BuildingNameText;

	UPROPERTY(BlueprintReadOnly, Category = "Building|UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> CostText;

	UPROPERTY(BlueprintReadOnly, Category = "Building|UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	/** Optional controls hint ("LMB build · RMB / B exit"). */
	UPROPERTY(BlueprintReadOnly, Category = "Building|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HintText;

	UPROPERTY(EditAnywhere, Category = "Building|UI")
	FLinearColor ValidStatusColor = FLinearColor(0.35f, 1.f, 0.4f);

	UPROPERTY(EditAnywhere, Category = "Building|UI")
	FLinearColor InvalidStatusColor = FLinearColor(1.f, 0.35f, 0.3f);

private:
	bool bStatusValid = false;
};
