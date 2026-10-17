#pragma once

#include "CoreMinimal.h"
#include "Building/ConstructionComponent.h"
#include "Components/ActorComponent.h"
#include "BuildingPlacementComponent.generated.h"

class ABuildingPreviewActor;
class AController;
class APawn;
class APlayerController;
class UBuildingDefinition;
class UBuildingHudWidget;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class UInventoryComponent;
class UMaterialInterface;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuildingModeChanged, bool, bBuildingModeActive);

/**
 * Player-side building UX on the pawn: building mode, the ghost preview under the cursor, the HUD and placement
 * input. It never builds by itself; every placement goes through the owner's UConstructionComponent (CanBuild for
 * feedback, TryBuild once per confirm).
 *
 * Input: IMC_Building (toggle) is added while the pawn is possessed by a local player; IMC_BuildingMode (confirm and
 * exit keys, consuming I/Tab/F so no screen opens while the mode owns the cursor) only while the mode is active.
 * The mode shows the cursor on enter and restores the remembered state on every exit.
 */
UCLASS(ClassGroup = (Building), meta = (BlueprintSpawnableComponent))
class NULLTIDE_API UBuildingPlacementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuildingPlacementComponent();

	/**
	 * Enters building mode. Requires a construction component with a registered building and a local player
	 * controller. Returns true when the mode is (already) active.
	 */
	UFUNCTION(BlueprintCallable, Category = "Building|Placement")
	bool EnterBuildingMode();

	/** Leaves building mode: removes the ghost, HUD and mode input, restores the cursor. Safe to repeat. */
	UFUNCTION(BlueprintCallable, Category = "Building|Placement")
	void ExitBuildingMode();

	UFUNCTION(BlueprintCallable, Category = "Building|Placement")
	void ToggleBuildingMode();

	/**
	 * Builds the selected building once at the placement under the cursor (re-snapped now). Outside building mode
	 * it does nothing and returns a failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Building|Placement")
	FBuildingConstructionResult ConfirmPlacement();

	UFUNCTION(BlueprintPure, Category = "Building|Placement")
	bool IsBuildingModeActive() const { return bBuildingModeActive; }

	/** The building placed by building mode (the first registered building until selection arrives in B5). */
	UFUNCTION(BlueprintPure, Category = "Building|Placement")
	TSubclassOf<UBuildingDefinition> GetSelectedBuilding() const { return SelectedBuilding; }

	/** Last CanBuild result for the ghost (read-only feedback; never trusted for building). */
	UFUNCTION(BlueprintPure, Category = "Building|Placement")
	FBuildingConstructionResult GetPreviewResult() const { return PreviewResult; }

	UFUNCTION(BlueprintPure, Category = "Building|Placement")
	ABuildingPreviewActor* GetPreviewActor() const { return PreviewActor; }

	UFUNCTION(BlueprintPure, Category = "Building|Placement")
	UBuildingHudWidget* GetHudWidget() const { return HudWidget; }

	UPROPERTY(BlueprintAssignable, Category = "Building|Placement")
	FOnBuildingModeChanged OnBuildingModeChanged;

	/** Moves the ghost to the placement snapped from a world point and refreshes the feedback (throttled unless forced). */
	void UpdatePreviewAt(const FVector& WorldLocation, bool bForceValidation);

	/** Short player-facing reason for a placement or construction result. */
	static FText DescribeResult(EBuildResult Result);

	/** "Wood 8/4"-style cost lines against the inventory (have/need). */
	static FText DescribeCost(const UBuildingDefinition& Definition, const UInventoryComponent* Inventory);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** World point under the cursor (Visibility trace). Seam for tests. */
	virtual bool GetCursorWorldLocation(FVector& OutLocation) const;

	/** Whether this controller may run building mode (its player is on this machine). Seam for tests (no local player). */
	virtual bool IsLocalBuilderController(const APlayerController& PlayerController) const;

	/** Creates and shows the HUD for a local player. Seam for tests (no viewport in a transient world). */
	virtual UBuildingHudWidget* CreateHudWidget(APlayerController& PlayerController);

	/** Building mode HUD widget class (W_BuildingHud). */
	UPROPERTY(EditDefaultsOnly, Category = "Building|Placement")
	TSubclassOf<UBuildingHudWidget> HudWidgetClass;

private:
	UConstructionComponent* GetConstruction() const;
	APlayerController* GetOwningPlayerController() const;
	void BindToController(AController* Controller);
	void UnbindFromController();
	void RefreshHud();
	void ShowResultMessage(const FText& Message, bool bValid);

	void HandleToggleInput();
	void HandleConfirmInput();
	void HandleCancelInput();

	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	UFUNCTION()
	void HandleBuildResult(TSubclassOf<UBuildingDefinition> DefinitionClass, const FBuildingConstructionResult& Result);

	/** Always active while possessed: the building mode toggle. */
	UPROPERTY(EditDefaultsOnly, Category = "Building|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> BuildingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Building|Input", meta = (AllowPrivateAccess = "true"))
	int32 BuildingContextPriority = 1;

	/** Active only in building mode: confirm, and exit keys (RMB, I, Tab, F) that consume lower-priority mappings. */
	UPROPERTY(EditDefaultsOnly, Category = "Building|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> BuildingModeContext;

	UPROPERTY(EditDefaultsOnly, Category = "Building|Input", meta = (AllowPrivateAccess = "true"))
	int32 BuildingModeContextPriority = 2;

	/** Started: toggle building mode. */
	UPROPERTY(EditDefaultsOnly, Category = "Building|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ToggleAction;

	/** Started: build once. */
	UPROPERTY(EditDefaultsOnly, Category = "Building|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ConfirmAction;

	/** Completed (on release, so the release is consumed too): leave building mode. */
	UPROPERTY(EditDefaultsOnly, Category = "Building|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> CancelAction;

	/** Translucent preview material with a Color vector parameter (M_BuildPreview). */
	UPROPERTY(EditDefaultsOnly, Category = "Building|Placement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> PreviewMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Building|Placement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float PreviewRefreshInterval = 0.1f;

	/** How long a build result stays on the HUD before the live placement status returns. */
	UPROPERTY(EditDefaultsOnly, Category = "Building|Placement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float ResultMessageDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Building|Placement", meta = (AllowPrivateAccess = "true"))
	FLinearColor ValidPreviewColor = FLinearColor(0.1f, 0.9f, 0.2f);

	UPROPERTY(EditDefaultsOnly, Category = "Building|Placement", meta = (AllowPrivateAccess = "true"))
	FLinearColor InvalidPreviewColor = FLinearColor(0.95f, 0.15f, 0.1f);

	UPROPERTY(Transient)
	TObjectPtr<ABuildingPreviewActor> PreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<UBuildingHudWidget> HudWidget;

	UPROPERTY(Transient)
	TSubclassOf<UBuildingDefinition> SelectedBuilding;

	FBuildingConstructionResult PreviewResult;
	FBuildingPlacement PreviewPlacement;
	bool bHasPreviewPlacement = false;
	double LastValidationTime = -1.0e9;
	double ResultMessageUntil = -1.0e9;
	FText ResultMessage;
	bool bResultMessageValid = false;

	bool bBuildingModeActive = false;
	bool bCursorWasShown = false;
	TWeakObjectPtr<APlayerController> ModeController;

	TWeakObjectPtr<APlayerController> InputController;
	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;
	TArray<uint32> InputBindingHandles;
};
