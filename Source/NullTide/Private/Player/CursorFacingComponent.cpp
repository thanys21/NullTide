#include "Player/CursorFacingComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UCursorFacingComponent::UCursorFacingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

bool UCursorFacingComponent::ComputeFacingYaw(const FVector& PawnFeet, const FVector& RayOrigin, const FVector& RayDirection,
	const float DeadZone, float& OutYaw)
{
	if (FMath::Abs(RayDirection.Z) < UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const double Distance = (PawnFeet.Z - RayOrigin.Z) / RayDirection.Z;
	if (Distance <= 0.0)
	{
		return false;
	}
	const FVector Point = RayOrigin + RayDirection * Distance;
	const FVector2D Delta(Point.X - PawnFeet.X, Point.Y - PawnFeet.Y);
	if (Delta.Size() < FMath::Max(DeadZone, 0.f))
	{
		return false;
	}
	OutYaw = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
	return true;
}

bool UCursorFacingComponent::IsWithinScreenDeadZone(const FVector2D& CharacterScreen, const FVector2D& Mouse, const float RadiusPixels)
{
	return FVector2D::Distance(CharacterScreen, Mouse) < FMath::Max(RadiusPixels, 0.f);
}

void UCursorFacingComponent::BeginPlay()
{
	Super::BeginPlay();
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		Pawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &UCursorFacingComponent::HandleControllerChanged);
	}
	// The movement component turns toward the new control rotation in the same frame.
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);
		}
	}
	RefreshTickEnabled();
}

void UCursorFacingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		Pawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &UCursorFacingComponent::HandleControllerChanged);
	}
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->PrimaryComponentTick.RemovePrerequisite(this, PrimaryComponentTick);
		}
	}
	SetComponentTickEnabled(false);
	Super::EndPlay(EndPlayReason);
}

void UCursorFacingComponent::HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	RefreshTickEnabled();
}

void UCursorFacingComponent::RefreshTickEnabled()
{
	SetComponentTickEnabled(GetLocalPlayerController() != nullptr);
}

APlayerController* UCursorFacingComponent::GetLocalPlayerController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	return PlayerController && IsLocalPlayerController(*PlayerController) ? PlayerController : nullptr;
}

bool UCursorFacingComponent::IsLocalPlayerController(const APlayerController& PlayerController) const
{
	return PlayerController.GetLocalPlayer() != nullptr;
}

bool UCursorFacingComponent::IsGameInputActive(const APlayerController& PlayerController) const
{
	// The input mode, not focus: a UI-only screen (inventory, storage, workbench) makes the viewport ignore input, while
	// game-only and building mode's game-and-UI do not, whichever widget a click has focused.
	const UWorld* World = GetWorld();
	UGameViewportClient* ViewportClient = World ? World->GetGameViewport() : nullptr;
	return ViewportClient && !ViewportClient->IgnoreInput();
}

void UCursorFacingComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	APlayerController* PlayerController = GetLocalPlayerController();
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!PlayerController || !Pawn || !IsGameInputActive(*PlayerController))
	{
		return;
	}
	// Over the character's own body (on screen), keep the facing: projecting that onto the ground would flip it.
	FVector2D Mouse;
	FVector2D CharacterScreen;
	if (!PlayerController->GetMousePosition(Mouse.X, Mouse.Y)
		|| (PlayerController->ProjectWorldLocationToScreen(Pawn->GetActorLocation(), CharacterScreen)
			&& IsWithinScreenDeadZone(CharacterScreen, Mouse, DeadZonePixels)))
	{
		return;
	}
	FVector RayOrigin;
	FVector RayDirection;
	if (!PlayerController->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return;
	}
	// The cursor points at the ground, so aim on the plane through the character's feet.
	FVector Feet = Pawn->GetActorLocation();
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		Feet.Z -= Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	float Yaw = 0.f;
	if (ComputeFacingYaw(Feet, RayOrigin, RayDirection, DeadZone, Yaw))
	{
		PlayerController->SetControlRotation(FRotator(0.0, Yaw, 0.0));
	}
}
