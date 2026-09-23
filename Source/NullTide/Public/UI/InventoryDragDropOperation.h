#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "InventoryDragDropOperation.generated.h"

class UInventorySlotWidget;
class UInventoryComponent;
class UItemInstance;

/** Identity-only G4 payload. It never performs an inventory mutation. */
UCLASS(BlueprintType)
class NULLTIDE_API UInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Drag")
	FGuid ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Drag")
	TObjectPtr<UItemInstance> ItemInstance;

	void SetSourceSlot(UInventorySlotWidget* InSourceSlot);
	void ResetSourceVisual();
	bool IsPayloadValid(const UInventoryComponent* Inventory) const;

private:
	TWeakObjectPtr<UInventorySlotWidget> SourceSlot;
};
