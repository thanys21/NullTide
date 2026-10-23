#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerStatusWidget.generated.h"

class UTextBlock;

/**
 * HUD readout of the player's coins and the game day (Phase 6 S5), under the base tag at the top right. Display
 * only and never hit-testable; UPlayerStatusHudComponent owns it and pushes the values.
 */
UCLASS(Abstract)
class NULLTIDE_API UPlayerStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetCoins(int32 Coins);
	void SetDay(int32 Day, bool bNight);

	FText GetCoinsText() const;
	FText GetDayText() const;
	bool HasRequiredWidgets() const { return CoinsText && DayText; }

	static FText FormatCoins(int32 Coins);
	static FText FormatDay(int32 Day, bool bNight);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, Category = "Player|UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> CoinsText;

	UPROPERTY(BlueprintReadOnly, Category = "Player|UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> DayText;
};
