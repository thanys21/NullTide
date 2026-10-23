#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WalletComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCoinsChanged, int32, NewCoins);

/**
 * The player's money (Phase 6 S5), kept apart from the inventory: coins take no slot and show on the HUD. Never
 * negative. Only the trade authority (UNpcTraderComponent) changes it, after the trade's inventory transaction has
 * committed; the amount is validated first, so that change cannot fail.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class NULLTIDE_API UWalletComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class UNpcTraderComponent;

public:
	UFUNCTION(BlueprintPure, Category = "Player|Wallet")
	int32 GetCoins() const { return Coins; }

	UFUNCTION(BlueprintPure, Category = "Player|Wallet")
	bool CanSpendCoins(int32 Amount) const { return Amount >= 0 && Amount <= Coins; }

	UPROPERTY(BlueprintAssignable, Category = "Player|Wallet")
	FOnCoinsChanged OnCoinsChanged;

	/** Coins at the start of play (test aid; 0 by default). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Wallet", meta = (ClampMin = "0"))
	int32 StartingCoins = 0;

protected:
	virtual void BeginPlay() override;

	/** Adds coins, clamped at the int32 maximum. False for a negative amount; a zero amount changes nothing. */
	bool AddCoins(int32 Amount);

	/** Spends coins. False (no change) for a negative amount or when the wallet holds less. */
	bool SpendCoins(int32 Amount);

private:
	int32 Coins = 0;
};
