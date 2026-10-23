#include "Player/PlayerStatusHudComponent.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Player/WalletComponent.h"
#include "UI/PlayerStatusWidget.h"
#include "Widgets/Layout/Anchors.h"
#include "World/GameClockSubsystem.h"

void UPlayerStatusHudComponent::BeginPlay()
{
	Super::BeginPlay();
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		Pawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &UPlayerStatusHudComponent::HandleControllerChanged);
	}
	if (UWalletComponent* Wallet = GetWallet())
	{
		Wallet->OnCoinsChanged.AddUniqueDynamic(this, &UPlayerStatusHudComponent::HandleCoinsChanged);
	}
	if (UGameClockSubsystem* Clock = UGameClockSubsystem::Get(this))
	{
		Clock->OnPhaseChanged.AddUniqueDynamic(this, &UPlayerStatusHudComponent::HandlePhaseChanged);
	}
	RefreshWidget();
}

void UPlayerStatusHudComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		Pawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &UPlayerStatusHudComponent::HandleControllerChanged);
	}
	if (UWalletComponent* Wallet = GetWallet())
	{
		Wallet->OnCoinsChanged.RemoveDynamic(this, &UPlayerStatusHudComponent::HandleCoinsChanged);
	}
	if (UGameClockSubsystem* Clock = UGameClockSubsystem::Get(this))
	{
		Clock->OnPhaseChanged.RemoveDynamic(this, &UPlayerStatusHudComponent::HandlePhaseChanged);
	}
	if (StatusWidget)
	{
		StatusWidget->RemoveFromParent();
		StatusWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UPlayerStatusHudComponent::HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	RefreshWidget();
}

void UPlayerStatusHudComponent::HandleCoinsChanged(int32 NewCoins)
{
	Refresh();
}

void UPlayerStatusHudComponent::HandlePhaseChanged(int32 Day, bool bNight)
{
	Refresh();
}

void UPlayerStatusHudComponent::RefreshWidget()
{
	APlayerController* PlayerController = GetLocalPlayerController();
	// A widget made for another player's screen (or none now) goes away first.
	if (StatusWidget && (!PlayerController || (StatusWidget->GetOwningPlayer() && StatusWidget->GetOwningPlayer() != PlayerController)))
	{
		StatusWidget->RemoveFromParent();
		StatusWidget = nullptr;
	}
	if (PlayerController && !StatusWidget)
	{
		StatusWidget = CreateStatusWidget(*PlayerController);
		if (StatusWidget)
		{
			// The position goes first: setting it resets the anchors to the top left.
			StatusWidget->SetPositionInViewport(FVector2D(-InsetX, InsetY), false);
			StatusWidget->SetAnchorsInViewport(FAnchors(1.f, 0.f));
			StatusWidget->SetAlignmentInViewport(FVector2D(1.f, 0.f));
		}
	}
	Refresh();
}

void UPlayerStatusHudComponent::Refresh()
{
	if (!StatusWidget)
	{
		return;
	}
	const UWalletComponent* Wallet = GetWallet();
	StatusWidget->SetCoins(Wallet ? Wallet->GetCoins() : 0);
	const UGameClockSubsystem* Clock = UGameClockSubsystem::Get(this);
	StatusWidget->SetDay(Clock ? Clock->GetDay() : 1, Clock && Clock->IsNight());
}

APlayerController* UPlayerStatusHudComponent::GetLocalPlayerController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	return PlayerController && IsLocalPlayerController(*PlayerController) ? PlayerController : nullptr;
}

UWalletComponent* UPlayerStatusHudComponent::GetWallet() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UWalletComponent>() : nullptr;
}

bool UPlayerStatusHudComponent::IsLocalPlayerController(const APlayerController& PlayerController) const
{
	return PlayerController.GetLocalPlayer() != nullptr;
}

UPlayerStatusWidget* UPlayerStatusHudComponent::CreateStatusWidget(APlayerController& PlayerController)
{
	if (!StatusWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: no StatusWidgetClass; the coins and day are not shown."), *GetPathName());
		return nullptr;
	}
	UPlayerStatusWidget* Widget = CreateWidget<UPlayerStatusWidget>(&PlayerController, StatusWidgetClass);
	if (!Widget || !Widget->AddToPlayerScreen())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: the status widget could not be added to the player's screen."), *GetPathName());
		return nullptr;
	}
	return Widget;
}
