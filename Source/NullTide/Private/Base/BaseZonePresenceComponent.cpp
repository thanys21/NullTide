#include "Base/BaseZonePresenceComponent.h"

#include "Base/BaseZoneComponent.h"
#include "Base/BaseZoneSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UI/BaseZoneTagWidget.h"
#include "Widgets/Layout/Anchors.h"

void UBaseZonePresenceComponent::BeginPlay()
{
	Super::BeginPlay();
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		Pawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &UBaseZonePresenceComponent::HandleControllerChanged);
	}
	RefreshTimer();
}

void UBaseZonePresenceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		Pawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &UBaseZonePresenceComponent::HandleControllerChanged);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	if (TagWidget)
	{
		TagWidget->RemoveFromParent();
		TagWidget = nullptr;
	}
	CurrentZone.Reset();
	bInZone = false;
	Super::EndPlay(EndPlayReason);
}

void UBaseZonePresenceComponent::HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	RefreshTimer();
}

void UBaseZonePresenceComponent::RefreshTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FTimerManager& Timers = World->GetTimerManager();
	if (APlayerController* PlayerController = GetLocalPlayerController())
	{
		// A tag made for another player's screen is rebuilt for this one on the next zone shown.
		if (TagWidget && TagWidget->GetOwningPlayer() && TagWidget->GetOwningPlayer() != PlayerController)
		{
			TagWidget->RemoveFromParent();
			TagWidget = nullptr;
			if (bInZone)
			{
				ShowTag(CurrentZone.Get());
			}
		}
		if (!Timers.IsTimerActive(RefreshTimerHandle))
		{
			Timers.SetTimer(RefreshTimerHandle, this, &UBaseZonePresenceComponent::RefreshZone, RefreshInterval, true);
			RefreshZone();
		}
		return;
	}
	Timers.ClearTimer(RefreshTimerHandle);
	// No local player any more: forget the zone like leaving it.
	if (bInZone)
	{
		bInZone = false;
		CurrentZone.Reset();
		ShowTag(nullptr);
		OnBaseZoneChanged.Broadcast(nullptr);
	}
}

bool UBaseZonePresenceComponent::IsRefreshTimerActive() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(RefreshTimerHandle);
}

void UBaseZonePresenceComponent::RefreshZone()
{
	const AActor* Owner = GetOwner();
	const UBaseZoneSubsystem* Zones = UBaseZoneSubsystem::Get(this);
	if (!Owner || !GetLocalPlayerController())
	{
		return;
	}
	UBaseZoneComponent* Zone = Zones ? Zones->FindZoneAt(Owner->GetActorLocation()) : nullptr;
	// bInZone also catches a zone destroyed under the player (its weak pointer then reads null, like "no zone").
	if (Zone == CurrentZone.Get() && (Zone != nullptr) == bInZone)
	{
		return;
	}
	CurrentZone = Zone;
	bInZone = Zone != nullptr;
	ShowTag(Zone);
	OnBaseZoneChanged.Broadcast(Zone);
}

void UBaseZonePresenceComponent::ShowTag(const UBaseZoneComponent* Zone)
{
	if (!Zone)
	{
		if (TagWidget)
		{
			TagWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	if (!TagWidget)
	{
		APlayerController* PlayerController = GetLocalPlayerController();
		TagWidget = PlayerController ? CreateTagWidget(*PlayerController) : nullptr;
		if (!TagWidget)
		{
			return;
		}
		// Top right. The position goes first: setting it resets the anchors to the top left.
		TagWidget->SetPositionInViewport(FVector2D(-TagInset, TagInset), false);
		TagWidget->SetAnchorsInViewport(FAnchors(1.f, 0.f));
		TagWidget->SetAlignmentInViewport(FVector2D(1.f, 0.f));
	}
	TagWidget->SetZoneName(Zone->ZoneName);
	TagWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
}

APlayerController* UBaseZonePresenceComponent::GetLocalPlayerController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	return PlayerController && IsLocalPlayerController(*PlayerController) ? PlayerController : nullptr;
}

bool UBaseZonePresenceComponent::IsLocalPlayerController(const APlayerController& PlayerController) const
{
	return PlayerController.GetLocalPlayer() != nullptr;
}

UBaseZoneTagWidget* UBaseZonePresenceComponent::CreateTagWidget(APlayerController& PlayerController)
{
	if (!TagWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: no TagWidgetClass; the base tag is not shown."), *GetPathName());
		return nullptr;
	}
	UBaseZoneTagWidget* Widget = CreateWidget<UBaseZoneTagWidget>(&PlayerController, TagWidgetClass);
	if (!Widget || !Widget->AddToPlayerScreen())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: the base tag could not be added to the player's screen."), *GetPathName());
		return nullptr;
	}
	return Widget;
}
