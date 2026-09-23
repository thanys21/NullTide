#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryDragVisualWidget.generated.h"

class UImage;
class UItemInstance;
class UTextBlock;

UCLASS(Abstract, Blueprintable)
class NULLTIDE_API UInventoryDragVisualWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeFromItem(const UItemInstance* Item);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DragIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DragName;
};
