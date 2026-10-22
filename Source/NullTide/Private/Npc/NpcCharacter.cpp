#include "Npc/NpcCharacter.h"

#include "Base/BaseZoneComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Npc/NpcAIController.h"
#include "Npc/NpcDefinition.h"
#include "UI/NpcTalkWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogNpcTalk, Log, All);

const FRotator ANpcCharacter::NameLabelRotation(50.0, 225.0, 0.0);

ANpcCharacter::ANpcCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	AIControllerClass = ANpcAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0, 540.0, 0.0);
		Movement->MaxWalkSpeed = 150.f;
	}

	NameLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameLabel"));
	NameLabel->SetupAttachment(GetCapsuleComponent());
	NameLabel->SetRelativeLocation(FVector(0.0, 0.0, 120.0));
	// A fixed world rotation toward the top-down camera, whichever way the NPC turns.
	NameLabel->SetUsingAbsoluteRotation(true);
	NameLabel->SetRelativeRotation(NameLabelRotation);
	NameLabel->SetHorizontalAlignment(EHTA_Center);
	NameLabel->SetVerticalAlignment(EVRTA_TextCenter);
	NameLabel->SetWorldSize(28.f);
	NameLabel->SetTextRenderColor(FColor::White);
	NameLabel->SetCastShadow(false);
}

void ANpcCharacter::InitializeResident(const TSubclassOf<UNpcDefinition> InDefinition, const UBaseZoneComponent* InHomeZone)
{
	NpcDefinition = InDefinition;
	HomeZone = InHomeZone;
	ApplyDefinition();
}

const UNpcDefinition* ANpcCharacter::GetValidDefinition() const
{
	FString Diagnostic;
	return UNpcDefinition::ResolveValidNpc(NpcDefinition, Diagnostic);
}

FText ANpcCharacter::GetNpcName() const
{
	const UNpcDefinition* Definition = GetValidDefinition();
	return Definition ? Definition->NpcName : FText::GetEmpty();
}

void ANpcCharacter::ApplyDefinition()
{
	const UNpcDefinition* Definition = GetValidDefinition();
	NameLabel->SetText(Definition ? Definition->NpcName : FText::GetEmpty());
	if (Definition && GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = Definition->WalkSpeed;
	}
}

void ANpcCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyDefinition();
}

void ANpcCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The panel never outlives its NPC.
	if (UNpcTalkWidget* Widget = TalkWidget)
	{
		Widget->CloseTalk();
	}
	TalkWidget = nullptr;
	Super::EndPlay(EndPlayReason);
}

ENpcTalkResult ANpcCharacter::GetTalkResult(AActor* Interactor) const
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	if (!IsValid(Pawn))
	{
		return ENpcTalkResult::InvalidInteractor;
	}
	if (!Cast<APlayerController>(Pawn->GetController()))
	{
		return ENpcTalkResult::NoPlayerController;
	}
	if (FVector::DistSquared2D(Pawn->GetActorLocation(), GetActorLocation()) > FMath::Square(TalkRange))
	{
		return ENpcTalkResult::OutOfRange;
	}
	if (!TalkScreenClass)
	{
		return ENpcTalkResult::NoTalkScreen;
	}
	if (TalkWidget)
	{
		return ENpcTalkResult::Busy;
	}
	return ENpcTalkResult::Ready;
}

bool ANpcCharacter::OpenTalkForInteractor(AActor* Interactor)
{
	const ENpcTalkResult Result = GetTalkResult(Interactor);
	if (Result != ENpcTalkResult::Ready)
	{
		// Rejected requests change nothing: no widget, no input mode or cursor change.
		UE_LOG(LogNpcTalk, Log, TEXT("%s: talk not opened for %s (%s)."), *GetName(), *GetNameSafe(Interactor),
			*StaticEnum<ENpcTalkResult>()->GetNameStringByValue(static_cast<int64>(Result)));
		return false;
	}
	APawn* Pawn = CastChecked<APawn>(Interactor);
	APlayerController* PlayerController = CastChecked<APlayerController>(Pawn->GetController());
	UNpcTalkWidget* Widget = CreateTalkWidget(*PlayerController);
	if (!Widget)
	{
		UE_LOG(LogNpcTalk, Warning, TEXT("%s: could not create the talk panel for %s."), *GetName(), *GetNameSafe(Interactor));
		return false;
	}
	const UNpcDefinition* Definition = GetValidDefinition();
	const FText Greeting = Definition && !Definition->GreetingLines.IsEmpty()
		? Definition->GreetingLines[FMath::RandRange(0, Definition->GreetingLines.Num() - 1)] : FText::GetEmpty();
	TalkWidget = Widget;
	Widget->OpenForNpc(this, GetNpcName(), Greeting);
	if (ANpcAIController* AI = Cast<ANpcAIController>(GetController()))
	{
		AI->BeginTalk(Pawn);
	}
	// Input mode and cursor change only after the panel is ready.
	ShowTalkWidget(*PlayerController, *Widget);
	return true;
}

void ANpcCharacter::EndTalk()
{
	TalkWidget = nullptr;
	if (ANpcAIController* AI = Cast<ANpcAIController>(GetController()))
	{
		AI->ResumeWander();
	}
}

UNpcTalkWidget* ANpcCharacter::CreateTalkWidget(APlayerController& PlayerController)
{
	return CreateWidget<UNpcTalkWidget>(&PlayerController, TalkScreenClass);
}

void ANpcCharacter::ShowTalkWidget(APlayerController& PlayerController, UNpcTalkWidget& Widget)
{
	if (!Widget.IsInViewport())
	{
		Widget.AddToViewport(100);
	}
	PlayerController.SetShowMouseCursor(true);
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(&PlayerController, &Widget, EMouseLockMode::DoNotLock, false);
	Widget.SetKeyboardFocus();
}
