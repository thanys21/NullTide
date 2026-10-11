# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Null Tide is an Unreal Engine 5.8 top-down game project (single runtime module `NullTide`, Windows). The C++ work so far is mostly about inventory, items, storage, resource gathering and (starting) crafting. Blueprints and UMG widgets are built on top of that code.

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
- Cook check: `UnrealEditor-Cmd.exe NullTide.uproject -run=Cook -TargetPlatform=Windows -Map=/Game/TopDown/Lvl_TopDown -unattended -nop4 -NullRHI -stdout`

## Tests

The automation tests are in `Source/NullTide/Private/Tests/`, wrapped in `#if WITH_DEV_AUTOMATION_TESTS`. Shared fixtures are in the `*TestTypes.h` headers there. Test names are grouped by milestone:

- `NullTide.Inventory.{M1..M6,G7,Stacking}`
- `NullTide.Gameplay.{G1,G2,G4,G5,G6}`
- `NullTide.ResourceGathering.{RG1,RG3,RG4,RG5}`
- `NullTide.Crafting.{C1,C2}`

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
- Read-only queries (`GetTotalQuantity`, `CanRemoveDefinitionQuantity`, `CanAddDefinitionQuantity`, `CanAddDefinitionQuantities`) return the same reason codes as the `Try*` calls and never change `Revision` or broadcast. Add commits and add queries share one private `PreflightAdd`; do not add a second capacity algorithm.
- The inventory stays generic: it must not include or depend on crafting types.
- Stack planning lives in the private `InventoryStacking.*` files.

**Legacy compatibility (`Compatibility/`, `Inventory/LegacyInventoryStorage.*`)**
- `InventoryManagerComponent.uasset` is an older Blueprint inventory component. It now inherits from `UInventoryComponent` but still has its own Blueprint array of definition classes.
- Flags on the component choose between legacy mode and importing that array into the native inventory. `LegacyInventoryStorage` reads and writes the Blueprint array through reflection.
- Only one store is authoritative at a time. Do not switch modes during play.
- Keep the `/Script/NullTide2 -> /Script/NullTide` CoreRedirect in `Config/DefaultEngine.ini`.

**Storage (`Storage/`)**
- `UStorageComponent` sits on `AStorageContainerActor` (Blueprint: `BP_StorageContainer`).
- Items move between inventory and storage through `UStorageTransferLibrary`, which is a friend of both components. A transfer moves the same `UItemInstance` and keeps its ItemId.

**Resource gathering (`ResourceGathering/`, `ToolLoadout/`)**
- `AResourceNodeActor` runs a timed gather. Gathering requires a tool, checked against the interactor's `UToolLoadoutComponent` (tools are equipped by ItemId per `EToolType`).
- Each node has a limited yield and is depleted when the yield runs out. The yield is added through the interactor's `UInventoryComponent`.
- `UGatherProgressPresenterComponent` and `UGatherProgressWidget` display progress.

**Crafting (`Crafting/`)**
- `UCraftingRecipeDefinition` is an abstract, const UObject, the same pattern as `UItemDefinition`. Each recipe is a data-only Blueprint subclass in `Content/LevelPrototyping/CraftingSystem/Recipes/Recipe_*`, referenced by `TSubclassOf` and read from its class default object.
- A recipe has `Ingredients` and `Outputs` (`FCraftingItemQuantity`: item definition class + quantity) and `CraftDuration`. `ValidateRecipe` returns an indexed diagnostic, and `IsDataValid` reports the same error in the editor.
- Recipes hold static data only. Recipe-specific inventory checks live in `UCraftingQueryLibrary` (read-only ingredient and output-capacity checks built on the generic inventory queries). Its output check runs before consumption, so it is conservative.
- Crafting progress is tracked in `CRAFTING_PLAN.md` / `CRAFTING_PROGRESS.md`. Consumption, a crafting authority and UI come in later checkpoints.

**UI (`UI/`)**
- The C++ widget base classes are inventory and storage screens and slots, plus the drag-drop operation and drag visual. The UMG Blueprints in `Content/LevelPrototyping/InventorySystem/UI/Widgets/` are built on them.
- Widgets only show inventory data and send requests to the components. They must not depend on specific item assets.

**Content**
- The main map is `/Game/TopDown/Lvl_TopDown`.
- `BP_TopDownCharacter` holds the components and `BP_TopDownController` opens the inventory UI.
- `BP_PickUpItem` is the generic world pickup.

## Milestone docs

The `*_PLAN.md`, `*_REPORT.md` and `*_PROGRESS.md` files in the repo root record each milestone's design decisions and validation. Read the matching report before you change that area. `INVENTORY_V2_PLAN.md` explains the core item architecture.

Keep these docs current as work happens, not only at the end:

- Before you start a milestone or checkpoint, write or update its `*_PLAN.md` with the agreed scope and the user's design decisions.
- After each checkpoint, update the matching `*_PROGRESS.md` (or `*_REPORT.md`) with what was implemented, the validation results that actually ran, and anything deferred.
- Update the status, roadmap and test baseline in `NULLTIDE_MASTER_PLAN.md` when a checkpoint completes or a design decision changes it.
- When a new area or test group is added, update the Architecture and Tests sections of this file.
- Commit the doc updates together with the code they describe.
