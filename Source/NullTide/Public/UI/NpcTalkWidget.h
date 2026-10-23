#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NpcTalkWidget.generated.h"

class ANpcCharacter;
class UButton;
class UTextBlock;

/**
 * Talk panel for an NPC (Phase 6 S4): its name, a greeting and Bye. Display only; trading arrives in S5. Bye, I, Tab
 * and Esc close it, return the player to game input with the gameplay cursor, and let the NPC walk again.
 */
UCLASS(Abstract)
class NULLTIDE_API UNpcTalkWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Fills the panel for an NPC that just started talking. */
	void OpenForNpc(ANpcCharacter* InNpc, const FText& NpcName, const FText& Greeting);

	/** Closes the panel once: removes it, restores game input and ends the NPC's talk. A second call does nothing. */
	UFUNCTION(BlueprintCallable, Category = "NPC|UI")
	void CloseTalk();

	bool IsTalkOpen() const { return bTalkOpen; }
	ANpcCharacter* GetNpc() const { return Npc.Get(); }
	FText GetNpcNameText() const;
	FText GetGreetingText() const;
	bool HasRequiredWidgets() const { return NpcNameText && GreetingText && ByeButton; }
	UButton* GetTradeButton() const { return TradeButton; }

	static bool IsCloseKey(const FKey& Key);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> NpcNameText;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> GreetingText;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|UI", meta = (BindWidget))
	TObjectPtr<UButton> ByeButton;

	/** Opens the trade screen (S5); shown only when the NPC trades. */
	UPROPERTY(BlueprintReadOnly, Category = "NPC|UI", meta = (BindWidgetOptional))
	TObjectPtr<UButton> TradeButton;

private:
	UFUNCTION()
	void HandleByeClicked();

	UFUNCTION()
	void HandleTradeClicked();

	/** Restores game input and ends the NPC's talk (the panel is already closed). */
	void FinishTalk();

	TWeakObjectPtr<ANpcCharacter> Npc;
	bool bTalkOpen = false;
};
