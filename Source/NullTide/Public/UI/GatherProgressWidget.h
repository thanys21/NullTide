#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GatherProgressWidget.generated.h"

class AResourceNodeActor;
class UProgressBar;
class UTextBlock;

/** Presentation-only view of an authority-owned resource gather. */
UCLASS(Abstract, Blueprintable)
class NULLTIDE_API UGatherProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void UpdateFromGatherNode(const AResourceNodeActor* ResourceNode);
	void ResetGatherProgress();

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> GatherProgressBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GatherStatusText;
};
