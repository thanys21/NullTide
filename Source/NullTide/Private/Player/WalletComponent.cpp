#include "Player/WalletComponent.h"

void UWalletComponent::BeginPlay()
{
	Super::BeginPlay();
	if (StartingCoins > 0)
	{
		Coins = StartingCoins;
		OnCoinsChanged.Broadcast(Coins);
	}
}

bool UWalletComponent::AddCoins(const int32 Amount)
{
	if (Amount < 0)
	{
		return false;
	}
	if (Amount == 0)
	{
		return true;
	}
	Coins = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(Coins) + Amount, MAX_int32));
	OnCoinsChanged.Broadcast(Coins);
	return true;
}

bool UWalletComponent::SpendCoins(const int32 Amount)
{
	if (!CanSpendCoins(Amount))
	{
		return false;
	}
	if (Amount == 0)
	{
		return true;
	}
	Coins -= Amount;
	OnCoinsChanged.Broadcast(Coins);
	return true;
}
