#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NpcCharacter.generated.h"

class APlayerController;
class UBaseZoneComponent;
class UNpcDefinition;
class UNpcTalkWidget;
class UNpcTraderComponent;
class UNpcTradeScreenWidget;
class UUserWidget;
class UTextRenderComponent;

/** Read-only outcome of a talk request, in check order. */
UENUM(BlueprintType)
enum class ENpcTalkResult : uint8
{
	Ready,
	/** No interactor, or the interactor is not a pawn. */
	InvalidInteractor,
	/** The interacting pawn is not controlled by a player controller. */
	NoPlayerController,
	/** The interactor is farther than TalkRange. */
	OutOfRange,
	/** No talk screen class is set. */
	NoTalkScreen,
	/** The NPC is already talking. */
	Busy
};

/**
 * An NPC character (Phase 6 S4): one kind of UNpcDefinition, living in a home zone and driven by ANpcAIController.
 * The player talks to it through BPI_Interactable (Blueprint) -> OpenTalkForInteractor, which opens the talk panel
 * with UI-only input and pauses the AI; EndTalk resumes it. It never changes an inventory.
 */
UCLASS(Abstract)
class NULLTIDE_API ANpcCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ANpcCharacter();

	/** Sets the definition and home zone before the NPC finishes spawning (UNpcSubsystem). */
	void InitializeResident(TSubclassOf<UNpcDefinition> InDefinition, const UBaseZoneComponent* InHomeZone);

	UFUNCTION(BlueprintPure, Category = "NPC")
	TSubclassOf<UNpcDefinition> GetNpcDefinition() const { return NpcDefinition; }

	/** Default object of the definition when it is valid; null otherwise. */
	const UNpcDefinition* GetValidDefinition() const;

	const UBaseZoneComponent* GetHomeZone() const { return HomeZone.Get(); }

	UFUNCTION(BlueprintPure, Category = "NPC")
	FText GetNpcName() const;

	/** Read-only check of a talk request; changes nothing. */
	UFUNCTION(BlueprintPure, Category = "NPC")
	ENpcTalkResult GetTalkResult(AActor* Interactor) const;

	/** Opens the talk panel for the interacting player, faces them and pauses the AI. A rejected request changes nothing. */
	UFUNCTION(BlueprintCallable, Category = "NPC")
	bool OpenTalkForInteractor(AActor* Interactor);

	/** The talk ended (the panel closed): the AI walks again. */
	void EndTalk();

	/** From the talk panel: opens the trade screen for the talk partner (the talk stays open, its panel hidden). */
	UFUNCTION(BlueprintCallable, Category = "NPC|Trade")
	bool OpenTrade();

	/** Closes the trade screen: back to the talk panel, or ends the whole talk. */
	void CloseTrade(bool bReturnToTalk);

	bool IsTrading() const { return TradeWidget != nullptr; }

	/** Whether this NPC offers trading (a store and a trade screen class). */
	bool CanOpenTrade() const;
	UNpcTradeScreenWidget* GetTradeWidget() const { return TradeWidget; }

	bool IsTalking() const { return TalkWidget != nullptr; }
	UNpcTalkWidget* GetTalkWidget() const { return TalkWidget; }
	UTextRenderComponent* GetNameLabel() const { return NameLabel; }

	/** The pawn this NPC is talking to, while a talk is open. */
	APawn* GetTalkPartner() const { return IsTalking() ? TalkPartner.Get() : nullptr; }

	/** Store, trading and haggling (S5); inactive for an NPC without a store. */
	UNpcTraderComponent* GetTrader() const { return Trader; }

	/** Horizontal distance within which a player can start talking. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC", meta = (ClampMin = "1.0", Units = "cm"))
	float TalkRange = 300.f;

	/** The label faces the fixed top-down camera (pitch -50, yaw 45). */
	static const FRotator NameLabelRotation;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Creates the talk panel for the player. Seam for tests (a transient world has no local player). */
	virtual UNpcTalkWidget* CreateTalkWidget(APlayerController& PlayerController);

	/** Shows the panel with the cursor, UI-only input and focus. Seam for tests. */
	virtual void ShowTalkWidget(APlayerController& PlayerController, UNpcTalkWidget& Widget);

	/** Creates the trade screen for the player. Seam for tests. */
	virtual UNpcTradeScreenWidget* CreateTradeWidget(APlayerController& PlayerController);

	/** Shows a screen with the cursor, UI-only input and focus (the trade screen, or the talk panel again). Seam for tests. */
	virtual void FocusScreen(APlayerController& PlayerController, UUserWidget& Widget);

	UPROPERTY(EditDefaultsOnly, Category = "NPC|UI")
	TSubclassOf<UNpcTalkWidget> TalkScreenClass;

	UPROPERTY(EditDefaultsOnly, Category = "NPC|UI")
	TSubclassOf<UNpcTradeScreenWidget> TradeScreenClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC")
	TObjectPtr<UTextRenderComponent> NameLabel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC")
	TObjectPtr<UNpcTraderComponent> Trader;

private:
	void ApplyDefinition();

	/** Removes the trade screen without touching the talk panel. */
	void DismissTradeWidget();

	UPROPERTY(VisibleInstanceOnly, Category = "NPC")
	TSubclassOf<UNpcDefinition> NpcDefinition;

	TWeakObjectPtr<const UBaseZoneComponent> HomeZone;
	TWeakObjectPtr<APawn> TalkPartner;

	UPROPERTY(Transient)
	TObjectPtr<UNpcTalkWidget> TalkWidget;

	UPROPERTY(Transient)
	TObjectPtr<UNpcTradeScreenWidget> TradeWidget;
};
