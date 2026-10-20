#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "StorageDragDropOperation.generated.h"

/**
 * A storage slot being dragged (G9): which side it comes from and its ItemId. Payload only; a drop on the other
 * panel asks the authority to move the stack. A drop re-resolves the id, so a stale payload does nothing.
 */
UCLASS()
class NULLTIDE_API UStorageDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Storage|UI")
	FGuid ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Storage|UI")
	bool bFromPlayerSide = true;
};
