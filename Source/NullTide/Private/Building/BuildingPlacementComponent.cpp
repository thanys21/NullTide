#include "Building/BuildingPlacementComponent.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Building/BuildingDefinition.h"
#include "Building/BuildingPieceComponent.h"
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
#include "UI/GameInputMode.h"

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
		if (RotateAction)
		{
			InputBindingHandles.Add(Input->BindAction(RotateAction, ETriggerEvent::Started, this,
				&UBuildingPlacementComponent::HandleRotateInput).GetHandle());
		}
		for (const TObjectPtr<UInputAction>& SelectAction : SelectBuildingActions)
		{
			if (SelectAction)
			{
				InputBindingHandles.Add(Input->BindAction(SelectAction, ETriggerEvent::Started, this,
					&UBuildingPlacementComponent::HandleSelectInput).GetHandle());
			}
		}
		if (DemolishAction)
		{
			InputBindingHandles.Add(Input->BindAction(DemolishAction, ETriggerEvent::Started, this,
				&UBuildingPlacementComponent::HandleDemolishInput).GetHandle());
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
	// A click on the build bar selects there; it must never also build or demolish in the world behind it.
	if (IsCursorOverBuildBar())
	{
		return;
	}
	if (bDemolishModeActive)
	{
		ConfirmDemolish();
	}
	else
	{
		ConfirmPlacement();
	}
}

void UBuildingPlacementComponent::HandleDemolishInput()
{
	ToggleDemolishMode();
}

void UBuildingPlacementComponent::HandleCancelInput()
{
	ExitBuildingMode();
}

void UBuildingPlacementComponent::HandleRotateInput()
{
	if (bBuildingModeActive && !bDemolishModeActive)
	{
		RotatePlacement();
	}
}

void UBuildingPlacementComponent::HandleSelectInput(const FInputActionInstance& Instance)
{
	const int32 Index = SelectBuildingActions.IndexOfByKey(Instance.GetSourceAction());
	if (bBuildingModeActive && Index != INDEX_NONE)
	{
		RequestSelectBuilding(Index);
	}
}

bool UBuildingPlacementComponent::RequestSelectBuilding(const int32 Index)
{
	const UConstructionComponent* Construction = GetConstruction();
	// A key or slot without a registered building does nothing, in either sub-mode.
	if (!Construction || !Construction->GetAvailableBuildings().IsValidIndex(Index))
	{
		return false;
	}
	// Picking a building while demolishing returns to building with it.
	if (bDemolishModeActive)
	{
		ToggleDemolishMode();
	}
	return SelectBuildingIndex(Index);
}

bool UBuildingPlacementComponent::SelectBuildingIndex(const int32 Index)
{
	const UConstructionComponent* Construction = GetConstruction();
	const TArray<TSubclassOf<UBuildingDefinition>> Buildings = Construction ? Construction->GetAvailableBuildings()
		: TArray<TSubclassOf<UBuildingDefinition>>();
	if (!Buildings.IsValidIndex(Index))
	{
		return false;
	}
	const bool bChanged = SelectedBuilding != Buildings[Index];
	SelectedIndex = Index;
	SelectedBuilding = Buildings[Index];
	if (bBuildingModeActive && bChanged)
	{
		RefreshPreviewForSelection();
	}
	return true;
}

void UBuildingPlacementComponent::RotatePlacement()
{
	PlacementRotation = (PlacementRotation + 1) % 4;
	if (bBuildingModeActive)
	{
		FVector CursorLocation;
		if (GetCursorWorldLocation(CursorLocation))
		{
			UpdatePreviewAt(CursorLocation, true);
		}
	}
}

void UBuildingPlacementComponent::RefreshPreviewForSelection()
{
	if (PreviewActor && SelectedBuilding)
	{
		const UBuildingDefinition* Selected = SelectedBuilding->GetDefaultObject<UBuildingDefinition>();
		PreviewActor->InitializeFromPieceClass(Selected->PieceActorClass, PreviewMaterial);
		PreviewActor->SetFacingArrow(Selected->PlacementMode == EBuildingPlacementMode::GridEdge);
	}
	bHasPreviewPlacement = false;
	LastValidationTime = -1.0e9;
	FVector CursorLocation;
	if (GetCursorWorldLocation(CursorLocation))
	{
		UpdatePreviewAt(CursorLocation, true);
	}
	else
	{
		RefreshHud();
	}
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
	// The selection persists across enter and exit; fall back to the first building if the list shrank.
	SelectedIndex = Buildings.IsValidIndex(SelectedIndex) ? SelectedIndex : 0;
	SelectedBuilding = Buildings[SelectedIndex];
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
		const UBuildingDefinition* Selected = SelectedBuilding->GetDefaultObject<UBuildingDefinition>();
		PreviewActor->InitializeFromPieceClass(Selected->PieceActorClass, PreviewMaterial);
		PreviewActor->SetFacingArrow(Selected->PlacementMode == EBuildingPlacementMode::GridEdge);
		PreviewActor->SetActorHiddenInGame(true);
	}
	HudWidget = CreateHudWidget(*PlayerController);
	if (HudWidget && HudWidget->InitializeBuildBar(this))
	{
		// The bar is clickable: GameAndUI captures the mouse only while a button is held, so UMG gets clicks; keys and
		// world clicks still reach Enhanced Input. Exit restores GameOnly, the resting mode screens restore too.
		UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(PlayerController, nullptr, EMouseLockMode::DoNotLock, false);
		bInputModeChanged = true;
	}

	Construction->OnBuildResult.AddUniqueDynamic(this, &UBuildingPlacementComponent::HandleBuildResult);
	Construction->OnDemolishResult.AddUniqueDynamic(this, &UBuildingPlacementComponent::HandleDemolishResult);
	bDemolishModeActive = false;
	DemolishPreview = FBuildingDemolishResult();
	DemolishTarget.Reset();
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
		Construction->OnDemolishResult.RemoveDynamic(this, &UBuildingPlacementComponent::HandleDemolishResult);
	}
	// Every exit returns to building for the next entry.
	bDemolishModeActive = false;
	DemolishPreview = FBuildingDemolishResult();
	DemolishTarget.Reset();
	DestroyDemolishHighlights();
	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
	if (HudWidget)
	{
		HudWidget->ReleaseBuildBar();
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
		if (bInputModeChanged)
		{
			// Back to the resting game mode, first click included. Building mode keeps every screen key, so no other UI
			// mode can be active here.
			NullTideInput::RestoreGameOnlyInput(*PlayerController);
		}
	}
	bInputModeChanged = false;
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

void UBuildingPlacementComponent::ToggleDemolishMode()
{
	if (!bBuildingModeActive)
	{
		return;
	}
	bDemolishModeActive = !bDemolishModeActive;
	DemolishTarget.Reset();
	DemolishPreview = FBuildingDemolishResult();
	LastDemolishValidationTime = -1.0e9;
	ResultMessageUntil = -1.0e9;
	if (bDemolishModeActive)
	{
		bHasPreviewPlacement = false;
		if (PreviewActor)
		{
			PreviewActor->SetActorHiddenInGame(true);
		}
		UpdateDemolishPreview(true);
	}
	else
	{
		HideDemolishHighlights();
		// Back to building with the previous selection and rotation.
		RefreshPreviewForSelection();
	}
}

FBuildingDemolishResult UBuildingPlacementComponent::ConfirmDemolish()
{
	UConstructionComponent* Construction = GetConstruction();
	if (!bBuildingModeActive || !bDemolishModeActive || !Construction)
	{
		FBuildingDemolishResult Inactive;
		Inactive.Diagnostic = TEXT("Demolish mode is not active.");
		return Inactive;
	}
	// The target is re-resolved at press time; the hover preview is never trusted.
	UpdateDemolishPreview(true);
	AActor* Target = DemolishTarget.Get();
	if (!Target)
	{
		FBuildingDemolishResult NoTarget;
		NoTarget.Diagnostic = TEXT("No building piece under the cursor.");
		ShowResultMessage(LOCTEXT("NoDemolishTarget", "Point at a building"), false);
		return NoTarget;
	}
	return Construction->TryDemolish(Target);
}

TArray<ABuildingPreviewActor*> UBuildingPlacementComponent::GetVisibleDemolishHighlights() const
{
	TArray<ABuildingPreviewActor*> Visible;
	for (ABuildingPreviewActor* Highlight : DemolishHighlights)
	{
		if (IsValid(Highlight) && !Highlight->IsHidden())
		{
			Visible.Add(Highlight);
		}
	}
	return Visible;
}

void UBuildingPlacementComponent::UpdateDemolishPreview(const bool bForceValidation)
{
	UConstructionComponent* Construction = GetConstruction();
	const UWorld* World = GetWorld();
	if (!bBuildingModeActive || !bDemolishModeActive || !Construction || !World)
	{
		return;
	}
	// Only building pieces are targets; anything else under the cursor is "nothing to demolish".
	AActor* Hovered = GetCursorHitActor();
	AActor* Target = Hovered && Hovered->FindComponentByClass<UBuildingPieceComponent>() ? Hovered : nullptr;
	const bool bChanged = Target != DemolishTarget.Get();
	const double Now = World->GetTimeSeconds();
	DemolishTarget = Target;
	if (!Target)
	{
		DemolishPreview = FBuildingDemolishResult();
	}
	else if (bForceValidation || bChanged || Now - LastDemolishValidationTime >= PreviewRefreshInterval)
	{
		DemolishPreview = Construction->CanDemolish(Target);
		LastDemolishValidationTime = Now;
	}
	UpdateDemolishHighlights();
	RefreshHud();
}

void UBuildingPlacementComponent::UpdateDemolishHighlights()
{
	UWorld* World = GetWorld();
	TArray<AActor*> Pieces;
	if (AActor* Target = DemolishTarget.Get())
	{
		if (DemolishPreview.IsSuccess())
		{
			for (const TWeakObjectPtr<AActor>& Piece : DemolishPreview.Pieces)
			{
				if (Piece.IsValid())
				{
					Pieces.Add(Piece.Get());
				}
			}
		}
		else
		{
			// A refused request highlights only the hovered piece, in the invalid color.
			Pieces.Add(Target);
		}
	}
	const FLinearColor Color = DemolishPreview.IsSuccess() ? DemolishPreviewColor : InvalidPreviewColor;
	for (int32 Index = 0; Index < Pieces.Num(); ++Index)
	{
		if (!DemolishHighlights.IsValidIndex(Index))
		{
			FActorSpawnParameters Params;
			Params.Owner = GetOwner();
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.ObjectFlags |= RF_Transient;
			DemolishHighlights.Add(World ? World->SpawnActor<ABuildingPreviewActor>(ABuildingPreviewActor::StaticClass(), FTransform::Identity, Params) : nullptr);
			HighlightedPieces.Add(nullptr);
		}
		ABuildingPreviewActor* Highlight = DemolishHighlights[Index];
		if (!Highlight)
		{
			continue;
		}
		if (HighlightedPieces[Index].Get() != Pieces[Index])
		{
			Highlight->InitializeFromPieceClass(Pieces[Index]->GetClass(), PreviewMaterial);
			Highlight->SetFacingArrowVisible(false);
			HighlightedPieces[Index] = Pieces[Index];
		}
		FTransform Transform = Pieces[Index]->GetActorTransform();
		Transform.SetScale3D(Transform.GetScale3D() * DemolishHighlightScale);
		Highlight->SetActorTransform(Transform);
		Highlight->SetPlacementValid(DemolishPreview.IsSuccess(), Color, Color);
		Highlight->SetActorHiddenInGame(false);
	}
	for (int32 Index = Pieces.Num(); Index < DemolishHighlights.Num(); ++Index)
	{
		if (DemolishHighlights[Index])
		{
			DemolishHighlights[Index]->SetActorHiddenInGame(true);
		}
	}
}

void UBuildingPlacementComponent::HideDemolishHighlights()
{
	for (ABuildingPreviewActor* Highlight : DemolishHighlights)
	{
		if (IsValid(Highlight))
		{
			Highlight->SetActorHiddenInGame(true);
		}
	}
}

void UBuildingPlacementComponent::DestroyDemolishHighlights()
{
	for (ABuildingPreviewActor* Highlight : DemolishHighlights)
	{
		if (IsValid(Highlight))
		{
			Highlight->Destroy();
		}
	}
	DemolishHighlights.Reset();
	HighlightedPieces.Reset();
}

void UBuildingPlacementComponent::RefreshDemolishHud()
{
	HudWidget->SetHintText(LOCTEXT("DemolishHint", "C build  ·  LMB demolish  ·  RMB / B exit"));
	const UBuildingDefinition* Definition = DemolishTarget.IsValid() && DemolishPreview.Definition
		? DemolishPreview.Definition->GetDefaultObject<UBuildingDefinition>() : nullptr;
	if (!DemolishTarget.IsValid())
	{
		HudWidget->SetBuildingText(LOCTEXT("DemolishTitle", "Demolish"), FText::GetEmpty());
	}
	else
	{
		const FText Name = Definition ? Definition->BuildingName : FText::FromString(DemolishTarget->GetName());
		const int32 Extra = DemolishPreview.IsSuccess() ? DemolishPreview.Pieces.Num() - 1 : 0;
		const FText Title = Extra > 0
			? FText::Format(LOCTEXT("DemolishMany", "Demolish: {0} + {1} more"), Name, FText::AsNumber(Extra))
			: FText::Format(LOCTEXT("DemolishOne", "Demolish: {0}"), Name);
		const FText Refund = DemolishPreview.Refund.IsEmpty() ? LOCTEXT("NoRefund", "No refund")
			: FText::Format(LOCTEXT("Refund", "Refund: {0}"), DescribeRefund(DemolishPreview.Refund));
		HudWidget->SetBuildingText(Title, Refund);
	}
	const UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() < ResultMessageUntil)
	{
		HudWidget->SetStatusText(ResultMessage, bResultMessageValid);
	}
	else if (!DemolishTarget.IsValid())
	{
		HudWidget->SetStatusText(LOCTEXT("PointAtBuilding", "Point at a building"), false);
	}
	else if (DemolishPreview.IsSuccess())
	{
		HudWidget->SetStatusText(LOCTEXT("ReadyToDemolish", "Ready to demolish"), true);
	}
	else
	{
		HudWidget->SetStatusText(DescribeResult(DemolishPreview.Result), false);
	}
}

void UBuildingPlacementComponent::HandleDemolishResult(TSubclassOf<UBuildingDefinition> DefinitionClass, const FBuildingDemolishResult& Result)
{
	const UBuildingDefinition* Definition = DefinitionClass ? DefinitionClass->GetDefaultObject<UBuildingDefinition>() : nullptr;
	if (Result.IsSuccess())
	{
		ShowResultMessage(FText::Format(LOCTEXT("Demolished", "Demolished {0}"), Definition ? Definition->BuildingName : FText::GetEmpty()), true);
	}
	else
	{
		ShowResultMessage(DescribeResult(Result.Result), false);
	}
	LastDemolishValidationTime = -1.0e9;
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
	if (bDemolishModeActive)
	{
		UpdateDemolishPreview(false);
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
	if (!bBuildingModeActive || bDemolishModeActive || !Construction || !Definition || !World)
	{
		return;
	}
	const FBuildingPlacement Placement = UBuildingSubsystem::MakePlacementFromWorld(*Definition, WorldLocation, PlacementRotation);
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
	HudWidget->SetBuildBarSelection(SelectedIndex, bDemolishModeActive);
	if (bDemolishModeActive)
	{
		RefreshDemolishHud();
		return;
	}
	HudWidget->SetHintText(LOCTEXT("BuildHint", "1-5 / click select  ·  R rotate  ·  C demolish  ·  LMB build  ·  RMB / B exit"));
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

AActor* UBuildingPlacementComponent::GetCursorHitActor() const
{
	const APlayerController* PlayerController = ModeController.Get();
	FHitResult Hit;
	if (!PlayerController || !PlayerController->GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit))
	{
		return nullptr;
	}
	return Hit.GetActor();
}

bool UBuildingPlacementComponent::IsCursorOverBuildBar() const
{
	return HudWidget && HudWidget->IsBuildBarHovered();
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
	case EBuildResult::NeedsFloor:
		return LOCTEXT("NeedsFloor", "Needs a floor");
	case EBuildResult::OutsideBuildZone:
		return LOCTEXT("OutsideBuildZone", "Outside the build zone");
	case EBuildResult::NotDemolishable:
		return LOCTEXT("NotDemolishable", "Can't demolish this");
	case EBuildResult::RefundDoesNotFit:
		return LOCTEXT("RefundDoesNotFit", "Inventory full");
	case EBuildResult::IsStorage:
		return LOCTEXT("IsStorage", "Storage can't be demolished");
	case EBuildResult::StorageNotEmpty:
		return LOCTEXT("StorageNotEmpty", "Empty it first");
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

FText UBuildingPlacementComponent::DescribeRefund(const TArray<FInventoryDefinitionQuantity>& Refund)
{
	TArray<FText> Lines;
	for (const FInventoryDefinitionQuantity& Line : Refund)
	{
		const UItemDefinition* Item = Line.DefinitionClass ? Line.DefinitionClass->GetDefaultObject<UItemDefinition>() : nullptr;
		const FText Name = Item && !Item->ItemName.IsEmpty() ? Item->ItemName : FText::FromString(GetNameSafe(Line.DefinitionClass.Get()));
		Lines.Add(FText::Format(LOCTEXT("RefundLine", "{0} {1}"), Name, FText::AsNumber(Line.Quantity)));
	}
	return FText::Join(LOCTEXT("RefundSeparator", ", "), Lines);
}

#undef LOCTEXT_NAMESPACE
