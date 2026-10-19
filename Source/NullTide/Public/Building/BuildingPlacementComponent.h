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
struct FInputActionInstance;

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

	/**
	 * Selects the registered building at this index (authored order of GetAvailableBuildings). Missing indices are
	 * ignored and return false. In building mode the ghost is rebuilt for the new piece. The selection persists.
	 */
	UFUNCTION(BlueprintCallable, Category = "Building|Placement")
	bool SelectBuildingIndex(int32 Index);

	/**
	 * The player's select request (keys 1-5 and build bar clicks, B8): a missing index does nothing; otherwise it
	 * leaves demolish first, then selects. Returns whether a building was selected.
	 */
	UFUNCTION(BlueprintCallable, Category = "Building|Placement")
	bool RequestSelectBuilding(int32 Index);

	/** The builder's construction component (on the same actor), or null. */
	UConstructionComponent* GetConstruction() const;

	/** Rotates the placement by one quarter turn (wrapping 3 -> 0). For edge pieces this flips the facing. */
	UFUNCTION(BlueprintCallable, Category = "Building|Placement")
	void RotatePlacement();

	/** The building placed by building mode. */
	UFUNCTION(BlueprintPure, Category = "Building|Placement")
	TSubclassOf<UBuildingDefinition> GetSelectedBuilding() const { return SelectedBuilding; }

	UFUNCTION(BlueprintPure, Category = "Building|Placement")
	int32 GetSelectedBuildingIndex() const { return SelectedIndex; }

	/** Quarter turns 0-3 applied to the next placement. */
	UFUNCTION(BlueprintPure, Category = "Building|Placement")
	int32 GetPlacementRotation() const { return PlacementRotation; }

	/**
	 * In building mode, switches between building and demolishing (C). Demolish highlights the piece under the
	 * cursor and everything a demolition would take with it; LMB demolishes. Outside building mode it does nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Building|Placement")
	void ToggleDemolishMode();

	UFUNCTION(BlueprintPure, Category = "Building|Placement")
	bool IsDemolishModeActive() const { return bDemolishModeActive; }

	/** Demolishes the piece under the cursor once (re-resolved now). Outside demolish mode it fails and does nothing. */
	UFUNCTION(BlueprintCallable, Category = "Building|Placement")
	FBuildingDemolishResult ConfirmDemolish();

	/** Last CanDemolish result for the hovered piece (read-only feedback). */
	UFUNCTION(BlueprintPure, Category = "Building|Placement")
	FBuildingDemolishResult GetDemolishPreview() const { return DemolishPreview; }

	/** The demolish highlight ghosts currently shown (one per affected piece). */
	TArray<ABuildingPreviewActor*> GetVisibleDemolishHighlights() const;

	/** "Wood 3, Scrap 1"-style refund lines. */
	static FText DescribeRefund(const TArray<FInventoryDefinitionQuantity>& Refund);

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

	/** Actor under the cursor (Visibility trace), for demolish mode. Seam for tests. */
	virtual AActor* GetCursorHitActor() const;

	/** Whether this controller may run building mode (its player is on this machine). Seam for tests (no local player). */
	virtual bool IsLocalBuilderController(const APlayerController& PlayerController) const;

	/** Creates and shows the HUD for a local player. Seam for tests (no viewport in a transient world). */
	virtual UBuildingHudWidget* CreateHudWidget(APlayerController& PlayerController);

	/** Whether the cursor is over the HUD build bar, where a confirm click must not build. Seam for tests. */
	virtual bool IsCursorOverBuildBar() const;

	/** LMB confirm input: builds or demolishes once, unless the cursor is over the build bar. */
	void HandleConfirmInput();

	/** Building mode HUD widget class (W_BuildingHud). */
	UPROPERTY(EditDefaultsOnly, Category = "Building|Placement")
	TSubclassOf<UBuildingHudWidget> HudWidgetClass;

private:
	APlayerController* GetOwningPlayerController() const;
	void BindToController(AController* Controller);
	void UnbindFromController();
	void RefreshHud();
	void ShowResultMessage(const FText& Message, bool bValid);

	void HandleToggleInput();
	void HandleCancelInput();
	void HandleRotateInput();
	void HandleSelectInput(const FInputActionInstance& Instance);
	/** Rebuilds the ghost for the selected piece and revalidates at the cursor now. */
	void RefreshPreviewForSelection();

	void HandleDemolishInput();
	/** Demolish mode: resolves the hovered piece, revalidates (throttled unless forced) and updates the highlights. */
	void UpdateDemolishPreview(bool bForceValidation);
	void UpdateDemolishHighlights();
	void HideDemolishHighlights();
	void DestroyDemolishHighlights();
	void RefreshDemolishHud();

	UFUNCTION()
	void HandleDemolishResult(TSubclassOf<UBuildingDefinition> DefinitionClass, const FBuildingDemolishResult& Result);

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

	/** Started: rotate the placement by a quarter turn. */
	UPROPERTY(EditDefaultsOnly, Category = "Building|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> RotateAction;

	/** Started: select the registered building at the action's index (1-5 -> index 0-4). */
	UPROPERTY(EditDefaultsOnly, Category = "Building|Input", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UInputAction>> SelectBuildingActions;

	/** Started (building mode only): switch between building and demolishing. */
	UPROPERTY(EditDefaultsOnly, Category = "Building|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> DemolishAction;

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

	/** Highlight of the pieces a demolition will remove. */
	UPROPERTY(EditDefaultsOnly, Category = "Building|Placement", meta = (AllowPrivateAccess = "true"))
	FLinearColor DemolishPreviewColor = FLinearColor(1.f, 0.55f, 0.05f);

	/** Demolish highlights are this much larger than the piece, so they never z-fight with it. */
	UPROPERTY(EditDefaultsOnly, Category = "Building|Placement", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float DemolishHighlightScale = 1.02f;

	UPROPERTY(Transient)
	TObjectPtr<ABuildingPreviewActor> PreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<UBuildingHudWidget> HudWidget;

	UPROPERTY(Transient)
	TSubclassOf<UBuildingDefinition> SelectedBuilding;

	int32 SelectedIndex = 0;
	int32 PlacementRotation = 0;

	/** One ghost per affected piece; pooled while the mode is active. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABuildingPreviewActor>> DemolishHighlights;

	/** The piece each highlight was built from (rebuilt only when it changes). */
	TArray<TWeakObjectPtr<AActor>> HighlightedPieces;

	bool bDemolishModeActive = false;
	TWeakObjectPtr<AActor> DemolishTarget;
	FBuildingDemolishResult DemolishPreview;
	double LastDemolishValidationTime = -1.0e9;

	FBuildingConstructionResult PreviewResult;
	FBuildingPlacement PreviewPlacement;
	bool bHasPreviewPlacement = false;
	double LastValidationTime = -1.0e9;
	double ResultMessageUntil = -1.0e9;
	FText ResultMessage;
	bool bResultMessageValid = false;

	bool bBuildingModeActive = false;
	bool bCursorWasShown = false;
	/** Whether entering switched the input mode to GameAndUI (for the build bar); exit restores GameOnly. */
	bool bInputModeChanged = false;
	TWeakObjectPtr<APlayerController> ModeController;

	TWeakObjectPtr<APlayerController> InputController;
	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;
	TArray<uint32> InputBindingHandles;
};
