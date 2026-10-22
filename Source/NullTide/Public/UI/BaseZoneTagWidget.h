#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BaseZoneTagWidget.generated.h"

class UTextBlock;

/**
 * Small HUD tag naming the base zone the player stands in (Phase 6 S3). Display only and never hit-testable;
 * UBaseZonePresenceComponent owns its lifetime, places it at the top right and pushes the name.
 */
UCLASS(Abstract)
class NULLTIDE_API UBaseZoneTagWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Base|UI")
	void SetZoneName(const FText& ZoneName);

	FText GetZoneNameText() const;
	bool HasZoneNameText() const { return ZoneNameText != nullptr; }

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, Category = "Base|UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> ZoneNameText;
};
