#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BaseZonePresenceComponent.generated.h"

class APlayerController;
class UBaseZoneComponent;
class UBaseZoneTagWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBaseZoneChanged, UBaseZoneComponent*, NewZone);

/**
 * Tracks which base zone the local player's pawn stands in (Phase 6 S3) and shows its name in a HUD tag at the top
 * right. A timer (no tick) looks the zone up while the pawn is controlled by a local player; OnBaseZoneChanged fires
 * once per change (null when leaving). Read-only: it never changes zones, building or input.
 */
UCLASS(ClassGroup = (Base), meta = (BlueprintSpawnableComponent))
class NULLTIDE_API UBaseZonePresenceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Seconds between zone lookups. */
	static constexpr float RefreshInterval = 0.25f;
	/** The tag's inset from the top right corner, in viewport units. */
	static constexpr float TagInset = 24.f;

	UFUNCTION(BlueprintPure, Category = "Base|Zone")
	UBaseZoneComponent* GetCurrentZone() const { return CurrentZone.Get(); }

	UPROPERTY(BlueprintAssignable, Category = "Base|Zone")
	FOnBaseZoneChanged OnBaseZoneChanged;

	/** Looks the zone up now and applies a change (the timer's work). */
	void RefreshZone();

	bool IsRefreshTimerActive() const;
	UBaseZoneTagWidget* GetTagWidget() const { return TagWidget; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Whether this player controller belongs to a local player on this machine. Seam for tests. */
	virtual bool IsLocalPlayerController(const APlayerController& PlayerController) const;

	/** Creates the tag widget for the player. Seam for tests (a transient world has no local player). */
	virtual UBaseZoneTagWidget* CreateTagWidget(APlayerController& PlayerController);

	UPROPERTY(EditDefaultsOnly, Category = "Base|UI")
	TSubclassOf<UBaseZoneTagWidget> TagWidgetClass;

private:
	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	/** Runs the timer only for a locally player-controlled pawn; stopping forgets the zone and hides the tag. */
	void RefreshTimer();
	APlayerController* GetLocalPlayerController() const;
	void ShowTag(const UBaseZoneComponent* Zone);

	FTimerHandle RefreshTimerHandle;
	TWeakObjectPtr<UBaseZoneComponent> CurrentZone;
	/** Whether the last lookup found a zone (CurrentZone alone cannot tell a destroyed zone from none). */
	bool bInZone = false;

	UPROPERTY(Transient)
	TObjectPtr<UBaseZoneTagWidget> TagWidget;
};
