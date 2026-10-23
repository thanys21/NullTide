#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerStatusHudComponent.generated.h"

class APlayerController;
class UPlayerStatusWidget;
class UWalletComponent;

/**
 * Shows the local player's coins and the game day on the HUD (Phase 6 S5), at the top right under the base tag.
 * Event-driven: it refreshes on the wallet's OnCoinsChanged and the clock's OnPhaseChanged; no tick, no timer. The
 * widget exists only while the owner is controlled by a local player. Read-only.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class NULLTIDE_API UPlayerStatusHudComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Below the base tag (S3): inset from the top right corner, in viewport units. */
	static constexpr float InsetX = 24.f;
	static constexpr float InsetY = 72.f;

	UPlayerStatusWidget* GetStatusWidget() const { return StatusWidget; }

	/** Pushes the current coins and day to the widget. */
	void Refresh();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Whether this player controller belongs to a local player on this machine. Seam for tests. */
	virtual bool IsLocalPlayerController(const APlayerController& PlayerController) const;

	/** Creates the widget for the player. Seam for tests (a transient world has no local player). */
	virtual UPlayerStatusWidget* CreateStatusWidget(APlayerController& PlayerController);

	UPROPERTY(EditDefaultsOnly, Category = "Player|UI")
	TSubclassOf<UPlayerStatusWidget> StatusWidgetClass;

private:
	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	UFUNCTION()
	void HandleCoinsChanged(int32 NewCoins);

	UFUNCTION()
	void HandlePhaseChanged(int32 Day, bool bNight);

	/** Shows the widget for a local player controller, removes it otherwise. */
	void RefreshWidget();

	APlayerController* GetLocalPlayerController() const;
	UWalletComponent* GetWallet() const;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerStatusWidget> StatusWidget;
};
