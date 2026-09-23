#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "InventoryDragDropOperation.generated.h"

class UInventorySlotWidget;
class UInventoryComponent;
class UItemInstance;

UENUM()
enum class EInventoryUIDropResult : uint8
{
	Invalid,
	Cancelled,
	AcceptedNoMutation,
	OutsideDropRequested
};

/** Identity-only payload for UI-only drag/drop. It never performs an inventory mutation. */
UCLASS(BlueprintType)
class NULLTIDE_API UInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Drag")
	FGuid ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Drag")
	TObjectPtr<UItemInstance> ItemInstance;

	void InitializePayload(UInventoryComponent* InSourceInventory, UItemInstance* InItemInstance);
	void SetSourceSlot(UInventorySlotWidget* InSourceSlot);
	void ResetSourceVisual();
	bool IsPayloadValid(const UInventoryComponent* Inventory) const;
	void Complete(EInventoryUIDropResult InResult);
	bool IsComplete() const { return bComplete; }
	EInventoryUIDropResult GetResult() const { return Result; }

private:
	TWeakObjectPtr<UInventoryComponent> SourceInventory;
	TWeakObjectPtr<UInventorySlotWidget> SourceSlot;
	EInventoryUIDropResult Result = EInventoryUIDropResult::Invalid;
	bool bComplete = false;
};
