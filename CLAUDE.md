# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Null Tide is an Unreal Engine 5.8 top-down game project (single runtime module `NullTide`, Windows). The C++ work so far covers inventory, items, storage, resource gathering, crafting (Phase 4 complete, including workstations) and building (Phase 5 complete; summary in `BUILDING_REPORT.md`). Blueprints and UMG widgets are built on top of that code.

## Project roadmap

Before starting or continuing milestone work, read:

NULLTIDE_MASTER_PLAN.md

Treat it as the current high-level roadmap and architecture source of truth.
Read the latest milestone-specific *_PROGRESS.md and *_REPORT.md files for the most recent implementation details.

Do not start the next milestone until the current milestone is validated and committed.

## Rules (from AGENTS.md)

- Look at the existing architecture before you change it.
- Never edit `.uasset` files directly. Use the Unreal MCP server (the `ModelContextProtocol` and `AllToolsets` plugins, running inside the editor) for assets, Blueprints and widgets.
- Check references before you delete or rename any C++ API that Blueprints can see (`UFUNCTION`, `UPROPERTY`, `UCLASS`, enums or structs).
- Compile after C++ changes. After Blueprint changes, compile the affected Blueprints. Past milestones compiled them with `warnings_as_errors=true`.
- Item data split: static data goes in `UItemDefinition` and its fragments, runtime state goes in `UItemInstance`, and inventory logic goes in `UInventoryComponent`. Use composition (fragments) instead of item subclasses.

## Development workflow (Claude plans and implements, user approves)

The workflow is in the `AGENTS.md` section "Development Workflow — Claude Plans and Implements, User Approves". Read it at the start of every checkpoint.
- Claude plans, implements and self-reviews, and is the only agent that writes.
- The user approves.
- No other agent reviews unless the user brings one in.

- **Plan gate:**
  - After writing a plan, critique it adversarially: verify every source claim at file:line, and check invariants, failure, reentrancy and lifetime paths, tests and scope.
  - Record the open risks, then stop.
  - Implement only after the user approves, and record every approved decision in the `*_PLAN.md` first.
- **Implementation:** implement only the approved checkpoint. Run its validation and record the evidence in the `*_PROGRESS.md`.
- **Self-review:**
  - Review the full diff against the plan and invariants.
  - For native code changes, also run a fresh-context review pass (for example `/code-review`, or a review subagent).
  - Classify each finding as CONFIRMED, REJECTED WITH EVIDENCE or NEEDS USER DECISION.
  - Fix confirmed findings and rerun the relevant tests.
  - A fix that changes an approved architecture decision needs the user.
- **Handoff report:** before any commit, stop and give a handoff report that includes:
  1. the checkpoint and the baseline commit SHA;
  2. `git status` and the exact changed files, including untracked files (which `git diff <baseline>` does not show);
  3. an implementation summary;
  4. the relevant invariants and architecture decisions, with references to the `*_PLAN.md`;
  5. build, test and cook evidence: commands, counts, warnings, and what was not run;
  6. a Content and World Partition audit: changed assets, plus `__ExternalActors__` and `__ExternalObjects__` counts;
  7. the self-review findings and their resolutions, known risks and untested cases;
  8. what needs the user: decisions, manual PIE steps, and the commit message to approve.
- **While the user reviews:** do not change the working tree unless the user asks.
- **Commits and pushes:**
  - Commit only on explicit user approval.
  - Stage only the approved files.
  - Push only on a separate explicit instruction.
  - Never advance to the next checkpoint automatically.

## Build

The engine is installed at `D:/game/epic/UE_5.8`.

```bash
# Editor build (the usual target)
"D:/game/epic/UE_5.8/Engine/Build/BatchFiles/Build.bat" NullTideEditor Win64 Development -Project="C:/Users/loithienly/Documents/Unreal Projects/NullTide/NullTide.uproject" -WaitMutex

# Regenerate VS Code project files / compile databases
"D:/game/epic/UE_5.8/Engine/Build/BatchFiles/Build.bat" -projectfiles -project="<path>/NullTide.uproject" -game -engine -vscode
```

- If the editor is running, linking fails with `LNK1104` because the editor has the DLL locked. Ask the user to close the editor normally. Never force-kill an editor that has unsaved work.
- The wrapper script can return a nonzero exit code even when the build succeeded. Check `%LOCALAPPDATA%\UnrealBuildTool\Log.txt` and look for `Result: Succeeded`.
- Cook check: `UnrealEditor-Cmd.exe NullTide.uproject -run=Cook -TargetPlatform=Windows -Map=/Game/TopDown/Lvl_TopDown -unattended -nop4 -NullRHI -stdout`. Run it with the editor closed (the editor holds the MCP port). From Git Bash, prefix `MSYS_NO_PATHCONV=1`, otherwise MSYS rewrites `-Map=/Game/...` into a Windows path.

## Tests

The automation tests are in `Source/NullTide/Private/Tests/`, wrapped in `#if WITH_DEV_AUTOMATION_TESTS`. Shared fixtures are in the `*TestTypes.h` headers there. Test names are grouped by milestone:

- `NullTide.Inventory.{M1..M6,G7,G9,Stacking}` (G9 = storage UI/UX)
- `NullTide.Gameplay.{G1,G2,G4,G5,G6,G8}` (G8 = interaction targeting)
- `NullTide.ResourceGathering.{RG1,RG3,RG4,RG5}`
- `NullTide.Crafting.{C1,C2,C3C4,C5,C6A,C6B,C7,C8A,C8B,C8C}`
- `NullTide.Building.{B1,B2,B3,B4,B5,B6,B7,B8}`

Past milestones ran tests through the Unreal MCP automation toolset in a live editor. Some UI and drag-drop tests start and own their own PIE session, and starting a test run ends any PIE session that is already running. To run tests from the command line instead:

```bash
UnrealEditor-Cmd.exe "<path>/NullTide.uproject" -ExecCmds="Automation RunTests NullTide.Inventory.Stacking; Quit" -unattended -nop4 -NullRHI -stdout
```

Pass a full test name, such as `NullTide.Gameplay.G1.Catalog`, to run a single test. Pass `NullTide` to run all of them.

## Architecture

All code is in `Source/NullTide/{Public,Private}/<Area>/`.

**Items (`Items/`)**
- `UItemDefinition` is an abstract, const UObject. Each item asset is a data-only Blueprint subclass, for example `Content/LevelPrototyping/InventorySystem/Items/Item_*`. Code refers to items by `TSubclassOf<UItemDefinition>` and reads data from the class default object.
- Capabilities come from instanced `UItemFragment` templates in the `ItemFragments` array (Stackable, Tool, Weapon, Food, Durability, WorldPresentation and others). Look them up with `FindFragmentByClass`, which returns null when the data is invalid or ambiguous.
- The `Fragments` array of `UInventoryItemFragment` is a deprecated legacy authoring path. It is kept only for serialization compatibility.
- `UItemInstance` is one runtime stack: a `FGuid` instance ID, its definition class and a `Quantity`. Its mutators are private. Only `UInventoryComponent`, `UStorageTransferLibrary` and `FInventoryStacking` are friends and can change it.

**Inventory (`Inventory/`)**
- `UInventoryComponent` is the only authority for changing inventory contents.
- Every operation is a `Try*` call (add, remove, quantity, drop to world, transfer to storage). Each one returns a result struct with a reason code from `InventoryTypes.h` and is atomic. Failures roll back, for example a world drop spawns the pickup first and only removes the item if the spawn succeeded.
- Each successful change increments `Revision` and broadcasts `OnInventoryChanged`.
- UI and other callers read data through `GetItemsSnapshot`. They never mutate items locally.
- Read-only queries (`GetTotalQuantity`, `CanRemoveDefinitionQuantity`, `CanAddDefinitionQuantity`, `CanAddDefinitionQuantities`) return the same reason codes as the `Try*` calls and never change `Revision` or broadcast. They are advisory; commits always re-run their own preflight inside the mutation guard.
- `TryApplyDefinitionTransaction(Removals, Additions)` (native only) is the atomic multi-definition commit: removals consume the earliest stacks of the exact class, additions are planned against the post-removal projection, and success bumps `Revision` once and broadcasts once. `CanApplyDefinitionTransaction` is its exact read-only preflight (same planning, no commit). Crafting uses both; do not build a craft out of several `Try*` calls.
- The inventory stays generic: it must not include or depend on crafting types.
- Stack planning lives in the private `InventoryStacking.*` files. `BuildAddPlanForEntries` over `FInventoryStackEntry` (live or projected stacks) is the only stacking rule set; `BuildAddPlan` wraps it for live stacks. Do not add a second capacity or stacking algorithm.

**Legacy compatibility (`Compatibility/`, `Inventory/LegacyInventoryStorage.*`)**
- `InventoryManagerComponent.uasset` is an older Blueprint inventory component. It now inherits from `UInventoryComponent` but still has its own Blueprint array of definition classes.
- Flags on the component choose between legacy mode and importing that array into the native inventory. `LegacyInventoryStorage` reads and writes the Blueprint array through reflection.
- Only one store is authoritative at a time. Do not switch modes during play.
- Keep the `/Script/NullTide2 -> /Script/NullTide` CoreRedirect in `Config/DefaultEngine.ini`.

**Storage (`Storage/`)**
- `UStorageComponent` sits on `AStorageContainerActor` (Blueprint: `BP_StorageContainer`).
- Items move between inventory and storage through `UStorageTransferLibrary`, which is a friend of both components. A transfer moves the same `UItemInstance` and keeps its ItemId.
- **Moves (G9):**
  - `MoveInventoryStackToStorage` / `MoveStorageStackToInventory` move one stack as much as fits. A whole fit is the exact transfer (identity moves). A partial fit leaves the source id with the remainder; the moved part merges or gets new ids.
  - `MoveAll*(…, OnlyDefinition)` moves every stack, or every stack of one item, under one guard, with one revision and one broadcast per side. Leftovers stay.
  - "As much as fits" is a search over `BuildAddPlan`, never a second capacity rule.
- **Split (G9):** `UInventoryComponent::TrySplitStack` / `UStorageComponent::TrySplitStack` (shared `FInventoryStacking::SplitStack`).
  - A new stack of `floor(Q/2)` with a new id goes right after the source. It needs Q of at least 2 (`CannotSplit`) and a free slot (`CapacityFull`); otherwise nothing changes.
- **Storage screen (G9):** `UStorageScreenWidget` / `W_StorageScreen` has three columns: inventory, the selected item's info, the storage. It is generic over any `UStorageComponent`, so future loot containers reuse it.
  - Slot input is resolved by `UStorageSlotWidget::ResolveSlotAction` in `NativeOnPreviewMouseButtonDown`:
    - LMB selects and can start a drag;
    - Shift+LMB moves the stack;
    - Ctrl+LMB moves every stack of the item;
    - Shift+RMB splits.
  - Dragging onto the other panel (`UStorageDragDropOperation`) moves the stack; a drop on the same panel does nothing.
  - `ALL →` / `← ALL` move everything that fits.
  - Both sides are sorted for display only (category, name, quantity), and there is no manual arrangement.
  - Selecting only restyles slots; it does not rebuild them, because a rebuild would kill the drag source.
  - Test seam: `SetPlayerInventoryOverride`.

**Resource gathering (`ResourceGathering/`, `ToolLoadout/`)**
- `AResourceNodeActor` runs a timed gather. Gathering requires a tool, checked against the interactor's `UToolLoadoutComponent` (tools are equipped by ItemId per `EToolType`).
- Each node has a limited yield and is depleted when the yield runs out. The yield is added through the interactor's `UInventoryComponent`.
- `UGatherProgressPresenterComponent` and `UGatherProgressWidget` display progress.

**Crafting (`Crafting/`)**
- `UCraftingRecipeDefinition` is an abstract, const UObject, the same pattern as `UItemDefinition`. Each recipe is a data-only Blueprint subclass in `Content/LevelPrototyping/CraftingSystem/Recipes/Recipe_*`, referenced by `TSubclassOf` and read from its class default object.
- A recipe has `Ingredients` and `Outputs` (`FCraftingItemQuantity`: item definition class + quantity) and `CraftDuration`. `ValidateRecipe` returns an indexed diagnostic, and `IsDataValid` reports the same error in the editor.
- Recipes hold static data only. Recipe-specific inventory checks live in `UCraftingQueryLibrary` (advisory read-only ingredient and output-capacity checks; the output check runs before consumption, so it is conservative).
- `FCraftingTransaction::TryCraftRecipeAtomic` (native only) is the authoritative craft path: it validates the recipe and runs one `TryApplyDefinitionTransaction`.
- `UCraftingComponent` on `BP_TopDownCharacter` is the player's crafting authority. It holds the registered recipes (`AvailableRecipes`, currently `Recipe_Axe`, `Recipe_WoodArrow`, `Recipe_Pickaxe`, `Recipe_ShortSword`), resolves the owner's inventory, and exposes `GetAvailableRecipes`, `CanCraftRecipe` (exact read-only preflight; `CraftInProgress` while a timed craft runs), `TryStartCraft` (UI entry: instant recipes craft now, timed recipes start), `TryCraftRecipe` (instant only; timed recipes return `TimedCraftRequired`), `CancelCraft`, progress queries and `OnCraftStarted`/`OnCraftCancelled`/`OnCraftingResult`. UI and Blueprints call this component, never the inventory transaction.
- Timed crafting (C7) consumes ingredients only at completion: start runs the exact preflight, a world timer runs (no tick), completion re-validates and runs one atomic transaction. One active craft per player. Movement (more than 10 cm horizontally) and jump/fall cancel through native `ACharacter` movement delegates bound only while crafting; do not add crafting cancel calls to Blueprint input graphs. Closing the UI does not cancel; `EndPlay` clears state silently.
- Workstations (C8) are context, never crafting authorities.
  - `UCraftingStationDefinition` is the station kind: abstract, const, with data-only Blueprint subclasses in `Content/LevelPrototyping/CraftingSystem/Stations/Station_*`. It holds `StationName`, `StationIcon` and `UseRange` (default 300 cm).
  - `UCraftingStationComponent` marks an actor as a station of one kind and answers `IsStationUsable` / `IsActorInUseRange`. It never crafts or touches an inventory.
  - `UCraftingRecipeDefinition::RequiredStation` names the kind by exact class; null means a hand recipe.
  - All recipes stay registered on the player's `AvailableRecipes`.
  - Station-aware calls take the station explicitly: `GetRecipesForStation` (that station's recipes first, then hand recipes), `CanCraftRecipeAtStation`, `TryStartCraftAtStation`, `GetActiveCraftStation`. The calls without a station are the hand context; station recipes there return `StationRequired`, and out of range returns `StationOutOfRange`.
  - Station access is re-validated at completion.
  - A timed station recipe binds the station actor's `OnEndPlay`: a destroyed station cancels it (`StationLost`), and world teardown clears it silently. Hand recipes never depend on a station.
- `ACraftingStationActor` (abstract; Blueprint `Content/LevelPrototyping/CraftingSystem/Stations/BP_Workbench`, one placed in `Lvl_TopDown`) is the world station.
  - Components: an unscaled `StationRoot`, a placeholder `StationMesh`, `InteractionBounds` and a `UCraftingStationComponent`.
  - `BPI_Interactable` `Interact` calls `OpenCraftingForInteractor`, which validates the request with `GetOpenCraftingResult` (interactor, player controller, station, `UseRange`, `CraftingScreenClass`). A rejected request changes nothing.
  - A valid request puts the station's cached `CraftingScreenClass` instance (`W_Inventory`) in station context (`UInventoryScreenWidget::OpenForCraftingStation`). Only then does it show the screen with the cursor, UI-only input and keyboard focus.
  - The station actor never creates a crafting component or inventory.
- Crafting progress is tracked in `CRAFTING_PLAN.md` / `CRAFTING_PROGRESS.md`.
- MCP cannot add a Blueprint interface to a new Blueprint. `BP_Workbench` was made by duplicating `BP_Sword` and reparenting it. Duplicating marks the source Blueprint dirty in the editor; never save that source.

**Building (`Building/`)** — Phase 5, complete (design in `BUILDING_PLAN.md`, evidence in `BUILDING_PROGRESS.md`, summary in `BUILDING_REPORT.md`)
- `UBuildingDefinition` is an abstract, const UObject with data-only Blueprint subclasses read from the CDO (the items/recipes/stations pattern). It holds the category, `PieceActorClass` (must carry a `UBuildingPieceComponent`), placement mode (`GridCell` for floors and furniture, `GridEdge` for walls and doors), footprint, cost (`FInventoryDefinitionQuantity`), `RefundFraction` and range override. `ValidateBuilding` and `IsDataValid` report the first problem.
- `UBuildingSubsystem` (`UWorldSubsystem`) is the grid, occupancy and placement-validation authority.
  - Grid: 200 cm cells from the world origin (floor division); canonical edges (X edge `(ix,iy)` runs along X from `(ix*200, iy*200)`; Y edge likewise along Y); edge snapping tie order South, West, North, East.
  - Layers: `RoundToInt(SurfaceZ / 50)`.
  - Rotation: 90° steps.
- **The subsystem is the only writer of the occupancy registry.**
  - Keys (`Floor`, `Edge`, `Furniture` categories plus layer) are reserved all-or-nothing by an owner (`ReserveKeys` → unique, never-reused handle), then committed once to a piece or released by that owner.
  - Finalized pieces register with `RegisterActivePiece`, idempotently.
  - Pieces unregister on `EndPlay` and component destruction; dead holders are purged.
  - `AuditRegistry` must stay empty in tests.
- `ValidatePlacement` is read-only. It recomputes surface, layer and transform (never trusts the caller) and checks, in order: definition, shape, range (600 cm), support surface (`WorldStatic` or `Active` floor; slope ≤ 30°, unevenness ≤ 25 cm), occupancy, blocking collision (Pawn channel, ground excluded geometrically, not by identity), builder safety.
  - Edge pieces keep their end samples and collision box `EdgeEndInset` (12 cm) from the grid corners, so walls meet at corners (B5). Cells sample 5 cm inside (`CellSampleInset`) with a 2 cm box, and ignore only the `Active` edge pieces registered on their own perimeter edges (`CollectOwnEdgeActors`, B7), so floors build next to walls while thin world blockers on a border still block. An edge on an occupied edge reports `Blocked` (the existing piece is under the samples, and support is checked before occupancy).
  - A definition with `bRequiresFloor` needs an `Active` floor under it, else `NeedsFloor` ("Needs a floor"): every sample on at least one side of an edge (walls, doors), or every sample of a cell (chest, workbench; B7).
- `UBuildingPieceComponent` holds per-piece metadata and state (`Provisional`/`Active`/`Removing`). `ABuildingPieceActor` is a thin host whose `SetProvisional` hides the piece and removes collision and navigation.
- Building never changes inventory, crafting, storage or interaction authority. The construction transaction (B2) uses only `UInventoryComponent::TryApplyDefinitionTransaction`.
- `UConstructionComponent` (B2) is a builder's construction authority (the `UCraftingComponent` pattern). Since B3, `BP_TopDownCharacter` has one, named `ConstructionComponent`; since B7 its `AvailableBuildings = [Building_WoodFloor, Building_WoodWall, Building_WoodDoor, Building_WoodChest, Building_Workbench]`.
  - It holds `AvailableBuildings` and exposes `GetAvailableBuildings`, `CanBuild` (exact read-only preflight) and `TryBuild`.
  - `OnBuildResult` fires exactly once per non-reentrant request; a nested request returns `BuildInProgress` without notifying.
  - **`TryBuild` activates first and pays last** (`BUILDING_PLAN.md` 5.2.3):
    1. guard, then validate (definition, registration, an `ABuildingPieceActor` piece class, an active inventory, B1 `ValidatePlacement`, the cost preflight);
    2. reserve the keys;
    3. spawn the piece deferred with `AlwaysSpawn` as provisional;
    4. revalidate;
    5. activate (`Active`, commit the reservation, `SetProvisional(false)`);
    6. integrity check;
    7. pay with one `TryApplyDefinitionTransaction`.

    That transaction is the commit point. Every earlier failure is a fail-closed rollback that never touches the inventory, and nothing fallible runs after payment.
  - `FBuildingConstructionResult` is built once after the payment returns. Its `Piece` is weak. A piece destroyed during payment still reports `Success`, with a Warning and a diagnostic and no refund (D3).
  - `SpawnProvisionalPiece` and `ActivatePiece` are protected virtual test seams; there are no debug switches.
  - **Demolition (B6): `CanDemolish` / `TryDemolish(Piece)`, refund first, then destroy.**
    - Only player-built `Active` pieces with authority, within range.
    - A floor takes the walls and doors it alone carries and the furniture on it (`UBuildingSubsystem::FindDependentPieces`; one level deep, layer-tolerant).
    - A storage that holds items is refused (`StorageNotEmpty`, "Empty it first"), also inside a cascade; an empty one is demolished (B7). A station is allowed; a craft at it ends with `StationLost`.
    - The summed refund (`ComputeRefund`: `floor(Quantity × RefundFraction)`, full by default) must fit, or nothing happens (`RefundDoesNotFit`).
    - Order: every piece `Removing` → one addition-only `TryApplyDefinitionTransaction` (the commit point; on failure all `Active` again) → `Destroy` dependents, then the target.
    - `OnDemolishResult` fires once per non-reentrant request, and the guard is shared with `TryBuild`.
- `UBuildingPlacementComponent` (B4) on `BP_TopDownCharacter` is the player's building UX. It never builds by itself: `CanBuild` feeds the preview, and confirm calls `TryBuild` once per press.
  - **`B` toggles building mode.** The mode shows the cursor, spawns one `ABuildingPreviewActor` ghost (green or red, no collision, never registered) and the `W_BuildingHud` (`UBuildingHudWidget`).
  - **Input:**
    - `IMC_Building` (B, priority 1) is active while possessed.
    - `IMC_BuildingMode` (priority 2) is active only in the mode: LMB confirms (`Started`); RMB, `I`, `Tab` and `F` exit (`Completed`).
    - It consumes those keys, so no screen opens while the mode owns the cursor; a second press opens it.
  - **Every exit restores the remembered cursor state:** toggle, cancel, controller change, `EndPlay`. When the HUD has a build bar, entering also switches the input mode to GameAndUI (so UMG gets clicks) and every exit restores GameOnly.
  - **Build bar (B8):** `W_BuildingHud` holds an optional `BuildBar` (`UBuildingBarWidget`, `W_BuildingBar` with `W_BuildingBarSlot` slots) at the bottom.
    - One slot per `AvailableBuildings` entry (key, icon or initial, name, cost, description tooltip) plus Demolish (`C`).
    - The highlight follows the selection (pushed from `RefreshHud`). Slots grey out when `CanApplyDefinitionTransaction(Cost, {})` fails, refreshed on `OnInventoryChanged`.
    - A click calls `RequestSelectBuilding` (the same path as keys 1-5: leave demolish, then select) or `ToggleDemolishMode`. It never builds.
    - LMB confirm is ignored while the cursor is over the bar (`IsCursorOverBuildBar` seam).
    - Slot buttons are non-focusable, so keys keep reaching Enhanced Input.
  - **Selection and rotation (B5, B7):** `1`–`5` (`IA_BuildSelect1`–`5`) call `SelectBuildingIndex` in `AvailableBuildings` order (missing indices ignored; the selection persists); `R` (`IA_BuildRotate`) calls `RotatePlacement` (quarter turns, wrapping; odd turns flip an edge piece's facing). Both are in `IMC_BuildingMode`.
  - **Demolish sub-mode (B6):** `C` (`IA_BuildDemolish`, in `IMC_BuildingMode`) toggles it inside building mode only.
    - The hovered piece and everything its demolition removes get a pooled highlight ghost: orange when allowed, red on the target when refused.
    - LMB calls `TryDemolish` once.
    - A registered 1–3 key returns to building. Every exit resets to building.
  - The ghost composes each template's transform up its attach chain (the door leaf under its hinge). It does not apply child-Blueprint overrides of inherited SCS templates.
  - The ghost carries a facing arrow (not a piece mesh, no collision): local +Y for edge pieces, which is the side a door opens to, and local +X for cells. It turns with `R`.
  - Test seams: `GetCursorWorldLocation`, `GetCursorHitActor`, `IsLocalBuilderController`, `CreateHudWidget`, `IsCursorOverBuildBar` (B8).
  - Building assets are in `Content/LevelPrototyping/BuildingSystem/{Input,Materials,UI}`.
- `ABuildingDoorActor` (B5; abstract, Blueprint `Pieces/BP_BuildDoor`) is an edge piece with a blocking frame, a `DoorHinge` with the `DoorLeaf`, and an overlap-only `InteractionBounds`.
  - `ToggleDoor`/`SetDoorOpen`/`CanInteract` return `EBuildingDoorResult`: any pawn within `UseRange` (300 cm) of an `Active` door that is not being destroyed.
  - The leaf swings 90° in `SwingDuration` (0.3 s) on a world timer, with no tick, and reverses mid-swing. Leaf collision is off from the start of opening until fully closed.
  - `BP_BuildDoor` implements `BPI_Interactable` (`Interact` → `ToggleDoor`, Press). It holds no inventory or construction authority.
- `ABuildingStorageActor` and `ABuildingCraftingStationActor` (B7; abstract) are the buildable chest and workbench.
  - They are `ABuildingPieceActor` hosts of a `UStorageComponent` / `UCraftingStationComponent` with an overlap-only `InteractionBounds`.
  - They open their screens through the same shared helpers as the authored `AStorageContainerActor` / `ACraftingStationActor`: `FStorageInteraction::OpenStorageScreen` and `FCraftingStationInteraction`.
  - They never open while the piece is not `Active` (`CanOpenStorage`; `StationUnavailable`).
- The shared interaction prompt `WB_interactionWidget` reads its key with `UInputPromptLibrary::GetKeyMappedToAction(IMC_Default, IA_Interact)` (B5). The live query returned no key while `IMC_BuildingMode` consumed F, which showed "None".

**Interaction targeting (`Interaction/`)** — B7.1
- The Blueprint `PlayerInteractionComponent` (`Content/LevelPrototyping/interactionSystem/`) runs interactions through `BPI_Interactable`. Its `GetActiveInteractable` returns `UInteractionTargetingLibrary::GetFacingInteractableFor(GetOwner)`: the player interacts with what they face, not with the last object that overlapped the capsule.
- `UInteractionProbeComponent` (a sphere named `InteractionProbe` on `BP_TopDownCharacter`, attached to the capsule) has radius 75 cm and sits 75 cm ahead (B9; reach 150 cm).
  - It has no collision of its own and no overlap events: it only places and sizes a fresh Pawn-channel overlap query for `BPI_Interactable` candidates, ignoring the owner and actors attached to it. It never affects navigation.
  - A 0.1 s timer (no tick) broadcasts `OnFacingInteractableChanged` only when the target changes, including to null when the previous target was destroyed. `PlayerInteractionComponent` binds it to re-render the prompt.
- `SelectFacingInteractable` considers candidates within ±70° of the actor's forward vector (2D). It picks the nearest of those within 1° of the smallest angle, which is independent of input order. It skips depleted resource nodes (`ShouldShowInteractionPromptFor`). With nothing in front there is no target and no prompt.
- Targeting is read-only and never interacts. Interactable actors need no change; their interaction volume must overlap the Pawn channel.

**UI (`UI/`)**
- The C++ widget base classes are inventory and storage screens and slots, plus the drag-drop operation and drag visual. The UMG Blueprints in `Content/LevelPrototyping/InventorySystem/UI/Widgets/` are built on them.
- Widgets only show inventory data and send requests to the components. They must not depend on specific item assets.
- Crafting UI: `UCraftingPanelWidget` (with `UCraftingRecipeEntryWidget` and `UCraftingRequirementRowWidget`) is a self-contained panel that observes the owning pawn's `UCraftingComponent` and inventory.
  - Display data comes from `UCraftingViewLibrary::BuildRecipeView` / `BuildRecipeViewAtStation` (read-only, exact `CanCraftRecipe` / `CanCraftRecipeAtStation`).
  - CRAFT calls `RequestCraftSelectedRecipe`, which calls `TryStartCraft` (or `TryStartCraftAtStation`) once. The result message comes only from `OnCraftingResult` (`GetCraftFeedbackText`), and reentrant requests are ignored.
  - `SetCraftingSource` binds the panel to a specific crafting component (tests).
  - `SetCraftingStation` sets a weak station UI context. Hand context lists hand recipes only; station context lists that station's recipes first, then hand recipes, and sets the `PanelTitle` text to the station name ("WORKBENCH"). A lost station lists nothing.
  - Progress and CANCEL always follow the player's active craft, even when its recipe is not listed.
  - UMG assets are in `Content/LevelPrototyping/CraftingSystem/UI/`.
- `W_Inventory` embeds `W_CraftingPanel` as its right column (inventory left, character middle, crafting right), bound natively as `UInventoryScreenWidget::CraftingPanel`.
  - `OpenForCraftingStation` gives a station screen its context. Close and destruction clear it, and the screen closes itself when its station becomes unusable.
  - The controller-owned hand screen never has a station context.
  - `I`, `Tab` and `Esc` close the screen in `NativeOnPreviewKeyDown` (and `NativeOnKeyDown`) through `CloseInventory`. The preview pass is required because a focused child (slot, recipe entry, button) otherwise consumes `Tab` as Slate focus navigation.
- Returning to game input (closing the inventory or storage screen, leaving building mode) goes through `NullTideInput::RestoreGameOnlyInput` (`UI/GameInputMode.h`, B9).
  - It applies GameOnly with `SetConsumeCaptureMouseDown(false)` to match the project's `CapturePermanently_IncludingInitialMouseDown`, so the first click afterwards reaches the game.
  - Do not call `SetInputMode_GameOnly` directly; its default swallows that click.
  - Keep every widget name that `UInventoryScreenWidget` binds when editing `W_Inventory`.

**Content**
- The main map is `/Game/TopDown/Lvl_TopDown`.
- `BP_TopDownCharacter` holds the components and `BP_TopDownController` opens the inventory UI on `IA_OpeningInventory` (`I` or `Tab` in `IMC_Default`). The open screen closes on `I`, `Tab` or `Esc`.
- `BP_PickUpItem` is the generic world pickup.
- Building assets live in `Content/LevelPrototyping/BuildingSystem/`:
  - `Pieces/BP_BuildFloor`: a child of `ABuildingPieceActor`. It only sets the inherited `PieceMesh`; it adds no components and keeps an unscaled root.
  - `Pieces/BP_BuildWall` (B5): a child of `ABuildingPieceActor`; `PieceMesh` is a 200 × 20 × 250 cm cube; no added components.
  - `Pieces/BP_BuildDoor` (B5): a child of `ABuildingDoorActor`; no added components.
  - `Pieces/BP_BuildChest` and `Pieces/BP_BuildWorkbench` (B7): children of `ABuildingStorageActor` (title "Wooden Chest", `W_StorageScreen`) and `ABuildingCraftingStationActor` (`Station_Workbench`, `W_Inventory`). Both implement `BPI_Interactable` (`Interact` → `OpenStorageForInteractor` / `OpenCraftingForInteractor`, Press).
  - `Definitions/Building_WoodFloor`: Wood × 4. `Building_WoodWall`: Wood × 6. `Building_WoodDoor`: Wood × 8 + Scrap × 2. `Building_WoodChest`: Wood × 12. `Building_Workbench`: Wood × 20 + Stone × 10. All refund 100% (`RefundFraction` 1.0, the default since B6), and all but the floor have `bRequiresFloor`.
- The `Lvl_TopDown` ground `Floor_0` uses `BlockAll` (`WorldStatic`) since B4, so it supports building like every other map block. It is a World Partition external actor; MCP cannot save actor packages, so map actor edits are saved in the editor (Ctrl+S) and then audited with `git status`.
- `BP_PickUpItem`'s `Quantity` is Instance Editable (B4 test aid). Test pickups near `PlayerStart` (Wood × 100, Stone × 10, Scrap × 10) were placed at the user's request; the user may remove them.

## Milestone docs

The `*_PLAN.md`, `*_REPORT.md` and `*_PROGRESS.md` files in the repo root record each milestone's design decisions and validation. Read the matching report before you change that area. `INVENTORY_V2_PLAN.md` explains the core item architecture.

Keep these docs current as work happens, not only at the end:

- Before you start a milestone or checkpoint, write or update its `*_PLAN.md` with the agreed scope and the user's design decisions.
- After each checkpoint, update the matching `*_PROGRESS.md` (or `*_REPORT.md`) with what was implemented, the validation results that actually ran, and anything deferred.
- Update the status, roadmap and test baseline in `NULLTIDE_MASTER_PLAN.md` when a checkpoint completes or a design decision changes it.
- When a new area or test group is added, update the Architecture and Tests sections of this file.
- Commit the doc updates together with the code they describe.
