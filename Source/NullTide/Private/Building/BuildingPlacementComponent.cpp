#include "Building/BuildingPlacementComponent.h"

#include "Building/BuildingDefinition.h"
#include "Building/BuildingPreviewActor.h"
#include "Building/BuildingSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"
#include "UI/BuildingHudWidget.h"

#define LOCTEXT_NAMESPACE "BuildingPlacement"

namespace
{
UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem(const APlayerController* PlayerController)
{
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
}

bool SameSnappedPlacement(const FBuildingPlacement& A, const FBuildingPlacement& B)
{
	return A.Anchor == B.Anchor && A.EdgeAxis == B.EdgeAxis && A.Rotation == B.Rotation && A.Layer == B.Layer;
}
}

UBuildingPlacementComponent::UBuildingPlacementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

UConstructionComponent* UBuildingPlacementComponent::GetConstruction() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) ? Owner->FindComponentByClass<UConstructionComponent>() : nullptr;
}

APlayerController* UBuildingPlacementComponent::GetOwningPlayerController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
}

void UBuildingPlacementComponent::BeginPlay()
{
	Super::BeginPlay();
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		Pawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &UBuildingPlacementComponent::HandleControllerChanged);
		BindToController(Pawn->GetController());
	}
}

void UBuildingPlacementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ExitBuildingMode();
	UnbindFromController();
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		Pawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &UBuildingPlacementComponent::HandleControllerChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void UBuildingPlacementComponent::HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	// Possession changes always leave building mode; the new local controller gets the toggle input.
	ExitBuildingMode();
	UnbindFromController();
	BindToController(NewController);
}

void UBuildingPlacementComponent::BindToController(AController* Controller)
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (!PlayerController || !IsLocalBuilderController(*PlayerController))
	{
		return;
	}
	InputController = PlayerController;
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerController->InputComponent))
	{
		BoundInputComponent = Input;
		if (ToggleAction)
		{
			InputBindingHandles.Add(Input->BindAction(ToggleAction, ETriggerEvent::Started, this,
				&UBuildingPlacementComponent::HandleToggleInput).GetHandle());
		}
		if (ConfirmAction)
		{
			InputBindingHandles.Add(Input->BindAction(ConfirmAction, ETriggerEvent::Started, this,
				&UBuildingPlacementComponent::HandleConfirmInput).GetHandle());
		}
		if (CancelAction)
		{
			InputBindingHandles.Add(Input->BindAction(CancelAction, ETriggerEvent::Completed, this,
				&UBuildingPlacementComponent::HandleCancelInput).GetHandle());
		}
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem(PlayerController))
	{
		if (BuildingContext)
		{
			Subsystem->AddMappingContext(BuildingContext, BuildingContextPriority);
		}
	}
}

void UBuildingPlacementComponent::UnbindFromController()
{
	if (UEnhancedInputComponent* Input = BoundInputComponent.Get())
	{
		for (const uint32 Handle : InputBindingHandles)
		{
			Input->RemoveBindingByHandle(Handle);
		}
	}
	InputBindingHandles.Reset();
	BoundInputComponent.Reset();
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem(InputController.Get()))
	{
		if (BuildingContext)
		{
			Subsystem->RemoveMappingContext(BuildingContext);
		}
	}
	InputController.Reset();
}

void UBuildingPlacementComponent::HandleToggleInput()
{
	ToggleBuildingMode();
}

void UBuildingPlacementComponent::HandleConfirmInput()
{
	ConfirmPlacement();
}

void UBuildingPlacementComponent::HandleCancelInput()
{
	ExitBuildingMode();
}

bool UBuildingPlacementComponent::EnterBuildingMode()
{
	if (bBuildingModeActive)
	{
		return true;
	}
	UConstructionComponent* Construction = GetConstruction();
	const TArray<TSubclassOf<UBuildingDefinition>> Buildings = Construction ? Construction->GetAvailableBuildings() : TArray<TSubclassOf<UBuildingDefinition>>();
	APlayerController* PlayerController = GetOwningPlayerController();
	UWorld* World = GetWorld();
	if (Buildings.IsEmpty() || !PlayerController || !IsLocalBuilderController(*PlayerController) || !World)
	{
		return false;
	}

	bBuildingModeActive = true;
	SelectedBuilding = Buildings[0];
	ModeController = PlayerController;
	bCursorWasShown = PlayerController->bShowMouseCursor;
	PlayerController->SetShowMouseCursor(true);
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem(PlayerController))
	{
		if (BuildingModeContext)
		{
			Subsystem->AddMappingContext(BuildingModeContext, BuildingModeContextPriority);
		}
	}

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	PreviewActor = World->SpawnActor<ABuildingPreviewActor>(ABuildingPreviewActor::StaticClass(), FTransform::Identity, Params);
	if (PreviewActor)
	{
		PreviewActor->InitializeFromPieceClass(SelectedBuilding->GetDefaultObject<UBuildingDefinition>()->PieceActorClass, PreviewMaterial);
		PreviewActor->SetActorHiddenInGame(true);
	}
	HudWidget = CreateHudWidget(*PlayerController);

	Construction->OnBuildResult.AddUniqueDynamic(this, &UBuildingPlacementComponent::HandleBuildResult);
	PreviewResult = FBuildingConstructionResult();
	bHasPreviewPlacement = false;
	LastValidationTime = -1.0e9;
	ResultMessageUntil = -1.0e9;
	SetComponentTickEnabled(true);
	RefreshHud();
	FVector CursorLocation;
	if (GetCursorWorldLocation(CursorLocation))
	{
		UpdatePreviewAt(CursorLocation, true);
	}
	// Last: a listener may leave the mode again; the result reflects that.
	OnBuildingModeChanged.Broadcast(true);
	return bBuildingModeActive;
}

void UBuildingPlacementComponent::ExitBuildingMode()
{
	if (!bBuildingModeActive)
	{
		return;
	}
	bBuildingModeActive = false;
	SetComponentTickEnabled(false);
	if (UConstructionComponent* Construction = GetConstruction())
	{
		Construction->OnBuildResult.RemoveDynamic(this, &UBuildingPlacementComponent::HandleBuildResult);
	}
	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
	if (HudWidget)
	{
		HudWidget->RemoveFromParent();
		HudWidget = nullptr;
	}
	if (APlayerController* PlayerController = ModeController.Get())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem(PlayerController))
		{
			if (BuildingModeContext)
			{
				Subsystem->RemoveMappingContext(BuildingModeContext);
			}
		}
		// Only the state this mode changed is restored.
		PlayerController->SetShowMouseCursor(bCursorWasShown);
	}
	ModeController.Reset();
	bHasPreviewPlacement = false;
	OnBuildingModeChanged.Broadcast(false);
}

void UBuildingPlacementComponent::ToggleBuildingMode()
{
	if (bBuildingModeActive)
	{
		ExitBuildingMode();
	}
	else
	{
		EnterBuildingMode();
	}
}

FBuildingConstructionResult UBuildingPlacementComponent::ConfirmPlacement()
{
	UConstructionComponent* Construction = GetConstruction();
	if (!bBuildingModeActive || !Construction)
	{
		FBuildingConstructionResult Inactive;
		Inactive.Result = EBuildResult::InvalidPlacement;
		Inactive.Diagnostic = TEXT("Building mode is not active.");
		return Inactive;
	}
	// The placement is re-snapped at press time; the preview result itself is never trusted.
	FVector CursorLocation;
	const bool bHasCursorTarget = GetCursorWorldLocation(CursorLocation);
	if (bHasCursorTarget)
	{
		UpdatePreviewAt(CursorLocation, true);
	}
	if (!bHasCursorTarget || !bHasPreviewPlacement)
	{
		FBuildingConstructionResult NoTarget;
		NoTarget.Result = EBuildResult::InvalidPlacement;
		NoTarget.Diagnostic = TEXT("Nothing under the cursor to build on.");
		ShowResultMessage(LOCTEXT("NoTarget", "Point at the ground"), false);
		return NoTarget;
	}
	return Construction->TryBuild(SelectedBuilding, PreviewPlacement);
}

void UBuildingPlacementComponent::HandleBuildResult(TSubclassOf<UBuildingDefinition> DefinitionClass, const FBuildingConstructionResult& Result)
{
	const UBuildingDefinition* Definition = DefinitionClass ? DefinitionClass->GetDefaultObject<UBuildingDefinition>() : nullptr;
	if (Result.IsSuccess())
	{
		ShowResultMessage(FText::Format(LOCTEXT("Built", "Built {0}"), Definition ? Definition->BuildingName : FText::GetEmpty()), true);
	}
	else
	{
		ShowResultMessage(DescribeResult(Result.Result), false);
	}
	// The world or inventory changed: refresh the ghost on the next tick.
	LastValidationTime = -1.0e9;
}

void UBuildingPlacementComponent::ShowResultMessage(const FText& Message, const bool bValid)
{
	const UWorld* World = GetWorld();
	ResultMessage = Message;
	bResultMessageValid = bValid;
	ResultMessageUntil = (World ? World->GetTimeSeconds() : 0.0) + ResultMessageDuration;
	RefreshHud();
}

void UBuildingPlacementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bBuildingModeActive)
	{
		return;
	}
	FVector CursorLocation;
	if (GetCursorWorldLocation(CursorLocation))
	{
		UpdatePreviewAt(CursorLocation, false);
	}
	else
	{
		// Nothing under the cursor: no target, so a confirm cannot build at a stale, hidden placement.
		bHasPreviewPlacement = false;
		if (PreviewActor)
		{
			PreviewActor->SetActorHiddenInGame(true);
		}
		RefreshHud();
	}
}

void UBuildingPlacementComponent::UpdatePreviewAt(const FVector& WorldLocation, const bool bForceValidation)
{
	UConstructionComponent* Construction = GetConstruction();
	const UBuildingDefinition* Definition = SelectedBuilding ? SelectedBuilding->GetDefaultObject<UBuildingDefinition>() : nullptr;
	const UWorld* World = GetWorld();
	if (!bBuildingModeActive || !Construction || !Definition || !World)
	{
		return;
	}
	const FBuildingPlacement Placement = UBuildingSubsystem::MakePlacementFromWorld(*Definition, WorldLocation, 0);
	const bool bMoved = !bHasPreviewPlacement || !SameSnappedPlacement(Placement, PreviewPlacement);
	const double Now = World->GetTimeSeconds();
	PreviewPlacement = Placement;
	bHasPreviewPlacement = true;
	if (bForceValidation || bMoved || Now - LastValidationTime >= PreviewRefreshInterval)
	{
		PreviewResult = Construction->CanBuild(SelectedBuilding, Placement);
		LastValidationTime = Now;
	}
	if (PreviewActor)
	{
		PreviewActor->SetActorTransform(PreviewResult.Placement.WorldTransform);
		PreviewActor->SetPlacementValid(PreviewResult.IsSuccess(), ValidPreviewColor, InvalidPreviewColor);
		PreviewActor->SetActorHiddenInGame(false);
	}
	RefreshHud();
}

void UBuildingPlacementComponent::RefreshHud()
{
	if (!HudWidget)
	{
		return;
	}
	const UBuildingDefinition* Definition = SelectedBuilding ? SelectedBuilding->GetDefaultObject<UBuildingDefinition>() : nullptr;
	const UConstructionComponent* Construction = GetConstruction();
	if (Definition)
	{
		HudWidget->SetBuildingText(Definition->BuildingName,
			DescribeCost(*Definition, Construction ? Construction->GetInventory() : nullptr));
	}
	const UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() < ResultMessageUntil)
	{
		HudWidget->SetStatusText(ResultMessage, bResultMessageValid);
	}
	else if (bHasPreviewPlacement)
	{
		HudWidget->SetStatusText(DescribeResult(PreviewResult.Result), PreviewResult.IsSuccess());
	}
	else
	{
		HudWidget->SetStatusText(LOCTEXT("PointAtGround", "Point at the ground"), false);
	}
}

bool UBuildingPlacementComponent::GetCursorWorldLocation(FVector& OutLocation) const
{
	const APlayerController* PlayerController = ModeController.Get();
	FHitResult Hit;
	if (!PlayerController || !PlayerController->GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit))
	{
		return false;
	}
	OutLocation = Hit.ImpactPoint;
	return true;
}

bool UBuildingPlacementComponent::IsLocalBuilderController(const APlayerController& PlayerController) const
{
	return PlayerController.IsLocalController();
}

UBuildingHudWidget* UBuildingPlacementComponent::CreateHudWidget(APlayerController& PlayerController)
{
	if (!HudWidgetClass || !PlayerController.IsLocalPlayerController() || !PlayerController.GetLocalPlayer())
	{
		return nullptr;
	}
	UBuildingHudWidget* Widget = CreateWidget<UBuildingHudWidget>(&PlayerController, HudWidgetClass);
	if (Widget)
	{
		Widget->AddToPlayerScreen();
	}
	return Widget;
}

FText UBuildingPlacementComponent::DescribeResult(const EBuildResult Result)
{
	switch (Result)
	{
	case EBuildResult::Success:
		return LOCTEXT("Ready", "Ready to build");
	case EBuildResult::OutOfRange:
		return LOCTEXT("TooFar", "Too far");
	case EBuildResult::NoGround:
		return LOCTEXT("NoGround", "No ground");
	case EBuildResult::TooSteep:
		return LOCTEXT("TooSteep", "Too steep");
	case EBuildResult::Uneven:
		return LOCTEXT("Uneven", "Uneven ground");
	case EBuildResult::Occupied:
		return LOCTEXT("Occupied", "Occupied");
	case EBuildResult::Blocked:
		return LOCTEXT("Blocked", "Blocked");
	case EBuildResult::BlocksBuilder:
		return LOCTEXT("BlocksBuilder", "Too close to you");
	case EBuildResult::InsufficientResources:
		return LOCTEXT("Insufficient", "Not enough resources");
	case EBuildResult::InvalidInventory:
		return LOCTEXT("NoInventory", "No inventory");
	case EBuildResult::BuildInProgress:
		return LOCTEXT("InProgress", "Building...");
	case EBuildResult::InvalidDefinition:
	case EBuildResult::NotAvailable:
		return LOCTEXT("CannotBuild", "Cannot build this");
	case EBuildResult::InvalidPlacement:
		return LOCTEXT("InvalidPlacement", "Invalid placement");
	case EBuildResult::SpawnFailed:
	case EBuildResult::TransformMismatch:
	case EBuildResult::BuildInterrupted:
	case EBuildResult::ResourcesRejectedRolledBack:
	default:
		return LOCTEXT("CouldNotBuild", "Could not build");
	}
}

FText UBuildingPlacementComponent::DescribeCost(const UBuildingDefinition& Definition, const UInventoryComponent* Inventory)
{
	TArray<FText> Lines;
	for (const FInventoryDefinitionQuantity& Line : Definition.Cost)
	{
		const UItemDefinition* Item = Line.DefinitionClass ? Line.DefinitionClass->GetDefaultObject<UItemDefinition>() : nullptr;
		const FText Name = Item && !Item->ItemName.IsEmpty() ? Item->ItemName : FText::FromString(GetNameSafe(Line.DefinitionClass.Get()));
		const int32 Have = Inventory ? Inventory->GetTotalQuantity(Line.DefinitionClass) : 0;
		Lines.Add(FText::Format(LOCTEXT("CostLine", "{0} {1}/{2}"), Name, FText::AsNumber(Have), FText::AsNumber(Line.Quantity)));
	}
	return FText::Join(LOCTEXT("CostSeparator", ", "), Lines);
}

#undef LOCTEXT_NAMESPACE
