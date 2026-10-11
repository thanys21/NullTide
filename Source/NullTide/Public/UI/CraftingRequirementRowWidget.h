#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/CraftingViewLibrary.h"
#include "CraftingRequirementRowWidget.generated.h"

class UImage;
class UTextBlock;

/** One ingredient (owned / required) or output (xN) line in the crafting panel. */
UCLASS(Abstract, Blueprintable)
class NULLTIDE_API UCraftingRequirementRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Crafting|UI")
	void SetRequirement(const FCraftingRequirementView& InView, bool bInIsOutput);

	UFUNCTION(BlueprintPure, Category = "Crafting|UI")
	FCraftingRequirementView GetRequirement() const { return View; }

	UFUNCTION(BlueprintPure, Category = "Crafting|UI")
	bool IsOutput() const { return bIsOutput; }

	/** "Owned / Required" for ingredients, "xN" for outputs. */
	UFUNCTION(BlueprintPure, Category = "Crafting|UI")
	static FText FormatQuantity(const FCraftingRequirementView& InView, bool bInIsOutput);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

private:
	FCraftingRequirementView View;
	bool bIsOutput = false;
};
