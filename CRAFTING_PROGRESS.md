# Crafting Progress

Plan and design decisions: `CRAFTING_PLAN.md`.

## Status

| Checkpoint | Status |
| --- | --- |
| C1 Recipe Data Model | COMPLETE |
| C2 Inventory Quantity Queries | COMPLETE |
| C3/C4 Atomic Craft Transaction (one checkpoint) | COMPLETE |
| C5 Crafting Authority | COMPLETE |
| C6A Crafting UI Foundation | COMPLETE (approved; manual rendered PIE 12/12 PASS, user-confirmed) |
| C6B Craft Execution from UI | COMPLETE (manual rendered PIE 12/12 PASS, user-confirmed) |
| C7 Timed Crafting | COMPLETE / APPROVED (manual rendered PIE: 12 PASS, 1 NOT TESTED; user-confirmed) |
| C8 Workstations | IN PROGRESS: design approved; C8A COMPLETE / APPROVED (180/180); C8B COMPLETE / APPROVED (183/183, cook PASS, manual rendered PIE 8 PASS + 1 N/A, user-confirmed); C8C NEXT, not started |

## C1 - Recipe Data Model

### Baseline and Scope

- Baseline: `0af68d6`.
- No inventory consumption, crafting authority or UI was added.
- Content: two new packages, no modified tracked package, no map or World Partition external-actor change. Tracked `.uasset`/`.umap` packages 306 -> 308 on disk.

### Implemented Contract

- `FCraftingItemQuantity` (`Public/Crafting/CraftingRecipeDefinition.h`): `TSubclassOf<UItemDefinition> ItemDefinition`, `int32 Quantity` (default 1, min 1).
- `UCraftingRecipeDefinition`: abstract, const, Blueprintable UObject. Recipe assets are data-only Blueprint subclasses read from the CDO.
  - `RecipeName`, `RecipeDescription`, `RecipeIcon`, `Ingredients`, `Outputs`, `CraftDuration` (seconds, `0` = instant).
  - `ValidateRecipe(FString& OutDiagnostic)` rejects, with an indexed diagnostic for the first failure: empty `Ingredients` or `Outputs`; null, abstract or deprecated item class; item definition failing `ValidateFragments`; quantity below 1; the same item twice in one list; negative `CraftDuration`.
  - The same item may appear as both an ingredient and an output.
  - `IsDataValid` reports the `ValidateRecipe` diagnostic as an editor data-validation error on concrete recipes.
- Production recipes in `/Game/LevelPrototyping/CraftingSystem/Recipes/` (created through Unreal MCP):
  - `Recipe_Axe`: Wood x3 + Stone x5 -> Axe x1, icon `T_Icon_Axe`, duration 0.
  - `Recipe_WoodArrow`: Wood x1 -> WoodArrow x5, icon `T_Icon_WoodArrow`, duration 0.
- Recipes are not yet referenced by any map or gameplay object, so the map cook does not include them.

### Validation

- `NullTideEditor Win64 Development` build: succeeded.
- Recipe Blueprints compiled with warnings as errors: `Recipe_Axe`, `Recipe_WoodArrow` passed. Save-time data validation reported no errors.
- Focused `NullTide.Crafting.C1`: 4/4 passed (`RecipeContract`, `ValidationRejectsInvalid`, `ProductionRecipes.Axe`, `ProductionRecipes.WoodArrow`).
- Regression `NullTide.Inventory.Stacking` + `NullTide.Gameplay.G1`: 20/20 passed.
- Full permanent automation `NullTide.`: 125/125 passed (121 previous + 4 new).
- Windows cook (editor closed): succeeded, 0 errors. 613 cooked, 7 platform-skipped, 620 total.
- Rendered PIE: not run. C1 adds no gameplay-visible behaviour.

### Notes

- Cook warnings are environment-only: NTFS journal inactive on `D:`, and the Model Context Protocol plugin licence notice.
- A first cook attempt with the editor open failed only because the cook process's MCP plugin could not bind `127.0.0.1:8000` while the editor held it. Run the cook with the editor closed.
- During development a test case appended an array element to its own array, which tripped an engine container assertion; the test was fixed before the validation above.

### Deferred

- Quantity queries (C2), ingredient consumption (C3), outputs (C4), `UCraftingComponent` (C5), UI (C6), timed crafting (C7), workstations (C8).

## C2 - Inventory Quantity Queries

### Baseline and Scope

- Baseline: `08ed028` (C1 approved).
- Read-only checkpoint: no consumption, no output commit, no crafting authority, no UI.
- Content: no change. Tracked `.uasset`/`.umap` packages 308 -> 308; nothing under `Content/` modified.

### Audit of Existing Stacking Authority

- `UItemInstance` owns `Quantity` (>= 1); only `UInventoryComponent`, `UStorageTransferLibrary` and `FInventoryStacking` can change it.
- `UItemFragment_Stackable::MaxStackSize` is resolved by `FInventoryStacking::ResolveMaxStackSize` (no fragment = max 1).
- `FInventoryStacking::BuildAddPlan` is the single stack simulation: it fills compatible partial stacks in inventory order, then plans new stacks, and checks occupied slots against capacity. It is used by `TryAddDefinitionQuantity` and by `UStorageTransferLibrary` (storage destination).
- Removal exists only per exact ItemId (`TryRemoveItem`, `TryRemoveItemQuantity`). There was no total-per-definition query and no remove-by-definition API.
- Storage has no quantity queries; none were needed for C2.

### APIs

Generic, on `UInventoryComponent` (no crafting includes or types):

| API | Contract |
| --- | --- |
| `GetTotalQuantity(DefinitionClass)` | Sum of `Quantity` over stacks of exactly that class. Non-stackable instances count 1 each. Null, abstract or absent classes return 0. |
| `CanRemoveDefinitionQuantity(DefinitionClass, Quantity)` | `NotInitialized` when native authority is inactive; `InvalidQuantity` for `< 1`; `InvalidDefinition` for null; `InsufficientQuantity` when the total is short; otherwise `Success` with `Quantity`. |
| `CanAddDefinitionQuantity(DefinitionClass, Quantity)` | Same result `TryAddDefinitionQuantity` would return for the current contents (except `Busy`). |
| `CanAddDefinitionQuantities(Requests)` | All requests must fit together. Duplicate definitions are summed; each definition is planned with `BuildAddPlan`; the sum of new stacks must fit the free slots. Empty request list is `InvalidQuantity`. |

- New `EInventoryOperationResult::InsufficientQuantity` (appended) and `FInventoryDefinitionQuantity` (definition class + quantity) in `InventoryTypes.h`.
- Refactor: `TryAddDefinitionQuantity` and the add queries now share private `PreflightAdd` (quantity, definition, definition data, legacy projection, `BuildAddPlan`). No second capacity algorithm exists.
- `LegacyInventoryStorage::FindArray` now takes a `const UActorComponent*` so const queries can use it. It only reads.

Crafting side, `UCraftingQueryLibrary` (`Public/Crafting/CraftingQueryLibrary.h`):

| API | Contract |
| --- | --- |
| `CheckRecipeIngredients(Inventory, RecipeClass)` / `CheckIngredients(Inventory, Recipe)` | Validates inputs, then `CanRemoveDefinitionQuantity` per ingredient. Reports the first short ingredient index. |
| `CheckRecipeOutputCapacity(Inventory, RecipeClass)` / `CheckOutputCapacity(Inventory, Recipe)` | Validates inputs, then one `CanAddDefinitionQuantities` call for all outputs. |

Both return `FCraftingQueryResult`: `Success`, `InvalidInventory`, `InvalidRecipe`, `InsufficientIngredients` or `OutputCapacityFull`, plus `EntryIndex` and the underlying `InventoryResult`.

### Ingredient/Output Overlap Caveat

C2 checks output capacity against the inventory as it is now, before consumption. Consumption can only free space, so a C2 "fits" stays true after consumption and can never cause ingredient loss. The reverse is not true: a full inventory may be rejected even though consuming the ingredients first would make room. Tests pin both cases (`Stackable 50/50 -> consume 10, output 5` and `consume one non-stackable, output one non-stackable` in a full inventory). The exact remove-then-add check belongs to the C3/C4 transaction preflight.

### Validation

- `NullTideEditor Win64 Development` build: succeeded, no compiler warnings.
- Focused `NullTide.Crafting.C2`: 5/5 passed (`InventoryTotalQuantity`, `InventoryRemovePreflight`, `InventoryAddPreflight`, `RecipeIngredientQuery`, `RecipeOutputQuery`). Every query block proves stack ids/quantities, revision and change events are unchanged.
- Regression `NullTide.Crafting.C1` + `NullTide.Inventory.Stacking` + `NullTide.Inventory.M1`: 20/20 passed (4 + 7 + 9).
- Full permanent automation `NullTide.`: 130/130 passed (125 previous + 5 new).
- Windows cook: succeeded, 0 errors (613 cooked, 7 platform-skipped, 620 total). Environment warnings only.
- Tests ran headless through `UnrealEditor-Cmd` with `-NullRHI`; the editor was closed.
- Rendered PIE: not run. C2 adds no gameplay-visible behaviour.

### Transaction-Design Issues for C3/C4

1. **Exact capacity needs a post-removal simulation.** `BuildAddPlan` reads real `UItemInstance` quantities. C3/C4 need to plan removals first, then plan outputs over the post-removal quantities and slot count, without mutating anything.
2. **One revision and one event per craft.** `TryRemoveItemQuantity` and `TryAddDefinitionQuantity` each bump `Revision` and broadcast. A craft built from them would emit several events and could fail halfway. C3/C4 need an internal batch commit (remove by definition + add several definitions) inside one mutation guard, with one legacy-projection rebuild, one revision and one broadcast.
3. **Remove-by-definition does not exist yet.** Only exact-ItemId removal exists. C3 must add deterministic consumption in inventory order inside the inventory authority.
4. **Queries are advisory.** Queries do not report `Busy`, and inventory can change between a query and a commit. The C3 commit must re-run its own preflight inside the mutation guard and never rely on an earlier query result.
5. **Exact-class matching.** Totals and stacking match the exact definition class. A recipe that names a parent class will not match child item classes. This matches current stacking behaviour; change it only deliberately.

## C3/C4 - Atomic Craft Transaction

C3 (ingredient consumption) and C4 (outputs) are one atomic inventory transaction, delivered as one checkpoint.

### Baseline and Scope

- Baseline: `b7a2b86` (C2 approved).
- No `UCraftingComponent`, no UI, no timed crafting, no workstations.
- Content: no change. Tracked `.uasset`/`.umap` packages 308 -> 308; nothing under `Content/` modified.
- Reviewed and approved by the user; committed as the C3/C4 milestone.

### Generic Inventory Transaction

`UInventoryComponent::TryApplyDefinitionTransaction(Removals, Additions)` takes `FInventoryDefinitionQuantity` arrays and returns `FInventoryTransactionResult` (`Result`, `FailedSide` = None/Removal/Addition, `FailedIndex` = first caller index of the failing definition). It is native C++ only; not a `UFUNCTION`, so UI cannot call it before the C5 authority exists. The inventory still includes no crafting types.

Preflight order (all read-only, inside the mutation guard):

1. `NotInitialized` when native authority is inactive; `Busy` during another mutation.
2. Both lists empty: `InvalidQuantity`.
3. Legacy projection lookup when native-ready.
4. Combine each list separately: per-request `Quantity < 1` -> `InvalidQuantity`, null class -> `InvalidDefinition`; duplicates summed (overflow -> `InvalidQuantity`).
5. Validate each addition definition with the same checks as single adds (class flags, fragment data, legacy projection class).
6. Plan removals on a projected copy -> `InsufficientQuantity`.
7. Plan additions in order on the projection -> `CapacityFull`.

### Removal Planning

`FInventoryStacking::PlanRemoval` works on a copy of the entries: exact definition class only, earliest stacks first, partial removal allowed, emptied entries of that definition dropped with the order of the rest kept. A short total returns false and leaves the copy unchanged. Live stacks are never written during planning.

### Projected Inventory Representation

`FInventoryStackEntry { Item, DefinitionClass, Quantity }` (private `InventoryStacking.h`):

- `Item` is the live stack the entry describes, read but never written during planning; null for a stack a plan would create.
- `Quantity` is the projected quantity.
- `MakeEntries` builds entries from live items in inventory order.
- `BuildAddPlan` (live) is now a thin wrapper over `BuildAddPlanForEntries`, the one stacking rule set used by normal adds, storage transfers, batch add queries and transaction outputs. Only live entries are merge targets; planned entries only take slots.
- `ApplyPlanToEntries` updates the projection after each addition (merged quantities rise, planned stacks are appended), so later additions see slots already used.

### Output Preflight

Outputs are planned against the post-removal projection, so freed slots, partially consumed stacks and ingredient/output overlap are all exact. `CanAddDefinitionQuantities` (C2) now runs the same transaction preflight with no removals, so the batch add query and the commit share one path.

### Atomic Commit and Event Contract

Inside one mutation guard, after preflight passes:

1. Stage every new output stack (new ItemIds, unique against existing and other staged ids). A staging failure returns before any live change.
2. Drop fully consumed stacks; set surviving stacks to their projected quantities.
3. Apply output merges onto the post-removal quantities.
4. Append the staged stacks in addition order.
5. Rebuild the legacy projection once (native-ready only).
6. `++Revision` once; broadcast `OnInventoryChanged` once.

Identity: untouched and partially consumed stacks keep their ItemId; a fully consumed stack's ItemId disappears once; merge destinations keep their ItemId; overflow stacks get new ItemIds; a source stack is never cloned. If an ingredient consumes a whole stack and the same definition is an output, the output becomes a new stack with a new ItemId.

### Crafting-Side Executor

`FCraftingTransaction::TryCraftRecipeAtomic(Inventory, RecipeClass | Recipe)` (native C++, `Public/Crafting/CraftingTransaction.h`):

- validates the inventory pointer and the recipe (`ValidateRecipe`; the diagnostic is returned);
- converts ingredients to removals and outputs to additions (`ValidateRecipe` forbids duplicates, so request indices are recipe indices; the primitive still sums duplicates for generic callers);
- calls `TryApplyDefinitionTransaction` and maps the result to `FCraftingTransactionResult`: `Success`, `InvalidInventory`, `InvalidRecipe`, `InsufficientIngredients` (ingredient index), `OutputCapacityFull` (output index), `InventoryBusy`, `InventoryRejected`.

`CheckRecipeIngredients` / `CheckRecipeOutputCapacity` remain advisory UI helpers. `TryCraftRecipeAtomic` is authoritative and never relies on an earlier query.

### Exact-Class Semantics

Kept: removals, totals and stacking match the exact definition class. A recipe naming a parent class does not consume child-class items.

### Ingredient/Output Overlap

Resolved exactly. `Wood 50/50`, recipe `Wood x10 -> Wood x5`, otherwise full inventory: the C2 advisory check still says full; the transaction succeeds and the same stack ends at 45. A full inventory of non-stackables crafting `Single x1 -> Other x1` uses the slot its own ingredient frees.

### Validation

- `NullTideEditor Win64 Development` build: succeeded, no compiler warnings.
- Focused `NullTide.Crafting.C3C4`: 8/8 passed (`RemovalPlan`, `ProjectedCapacity`, `AtomicSuccess`, `AtomicFailure`, `IngredientOutputOverlap`, `RevisionAndEventContract`, `IdentityPreservation`, `ProductionRecipes`). Every failure case proves contents, ItemIds, revision and event count unchanged; every success proves exactly one revision and one event.
- Regression `NullTide.Crafting.C2` + `C1` + `Inventory.Stacking` + `Inventory.M1`: 25/25 passed (5 + 4 + 7 + 9).
- Full permanent automation `NullTide.`: 138/138 passed (130 previous + 8 new).
- Windows cook: succeeded, 0 errors (613 cooked, 7 platform-skipped, 620 total). Environment warnings only.
- Tests ran headless through `UnrealEditor-Cmd` with `-NullRHI`; the editor was closed.
- Rendered PIE: not run. No gameplay-visible behaviour was added (nothing in game calls the transaction yet).

### Notes for C5

- The transaction and the craft executor are native only. C5 decides what `UCraftingComponent` exposes to Blueprints.
- An exact read-only craft preflight (the transaction preflight without commit) would let C6 enable the Craft button exactly; today only the conservative C2 output check is exposed.

## C5 - Crafting Authority

### Baseline and Scope

- Baseline: `e042232` (C3/C4 committed). Previous full automation 138/138.
- Instant crafting only. No crafting UI, hotkeys, timed crafting, workstations or recipe unlock progression.
- Reviewed and approved by the user (all five design decisions approved without changes); committed as the C5 milestone.

### Architecture Audit

- Player: `BP_TopDownCharacter` already owns the inventory (`InventoryManagerComponent`, a `UInventoryComponent` subclass) and `UToolLoadoutComponent`. `UToolLoadoutComponent` resolves the inventory with `GetOwner()->FindComponentByClass<UInventoryComponent>()`; the crafting component uses the same pattern.
- Blueprint-facing patterns reused: `Try*` callable functions returning result structs, `BlueprintPure` queries, a `BlueprintAssignable` dynamic multicast delegate for notifications.
- `FCraftingTransaction` and `UInventoryComponent::TryApplyDefinitionTransaction` stay native only; Blueprints use the component.

### Source Files

- New: `Public/Crafting/CraftingComponent.h`, `Private/Crafting/CraftingComponent.cpp`, `Private/Tests/CraftingComponentTests.cpp`.
- Changed: `Public/Inventory/InventoryComponent.h`, `Private/Inventory/InventoryComponent.cpp` (`CanApplyDefinitionTransaction`); `Public/Crafting/CraftingTransaction.h`, `Private/Crafting/CraftingTransaction.cpp` (`CanCraftRecipe`, shared request building and result mapping, two new result codes, enum `ScriptName`); `Public/Crafting/CraftingQueryLibrary.h` (enum `ScriptName`); `Private/Tests/CraftingTestTypes.h` (C5 fixtures).

### `UCraftingComponent`

| API | Contract |
| --- | --- |
| `GetAvailableRecipes()` (pure) | Registered recipes in authored order; null, abstract, invalid and duplicate entries are dropped (first copy wins). |
| `IsRecipeAvailable(RecipeClass)` (pure) | True for a registered, valid recipe. |
| `CanCraftRecipe(RecipeClass)` (pure) | Exact read-only preflight; returns what `TryCraftRecipe` would return now (except busy states). |
| `TryCraftRecipe(RecipeClass)` (callable) | Authoritative craft through `FCraftingTransaction::TryCraftRecipeAtomic`. |
| `GetInventory()` (pure) | The owner's inventory component, or null. |
| `OnCraftingResult(RecipeClass, Result)` (assignable) | One notification per non-reentrant `TryCraftRecipe` call, success or failure. Preflights never notify. |

- Registration: `AvailableRecipes` (`EditDefaultsOnly`). `IsDataValid` reports null, abstract, invalid and duplicate entries.
- Rejection order: null/abstract/invalid recipe -> `InvalidRecipe` (with the `ValidateRecipe` diagnostic); valid but unregistered -> `RecipeNotAvailable`; no inventory or inactive native authority -> `InvalidInventory`; then the transaction result (`InsufficientIngredients`, `OutputCapacityFull` with recipe entry index, `InventoryBusy`, `InventoryRejected`).
- `ECraftingTransactionResult` gained `RecipeNotAvailable` and `CraftInProgress` (appended).
- Reentrancy: a craft-in-progress guard covers the whole `TryCraftRecipe`, including the notification. A reentrant call from `OnInventoryChanged` during the commit or from an `OnCraftingResult` handler returns `CraftInProgress` without mutation or notification. A craft attempted during an unrelated inventory mutation returns `InventoryBusy` and notifies once.

### Exact Preflight

- `UInventoryComponent::CanApplyDefinitionTransaction(Removals, Additions)` (native, const) returns `NotInitialized` when authority is inactive, otherwise runs the same `PreflightTransaction` as `TryApplyDefinitionTransaction`. No live stack, ItemId, revision or event changes.
- `FCraftingTransaction::CanCraftRecipe(Inventory, Recipe)` uses the same request building and result mapping as `TryCraftRecipeAtomic`.
- Overlap is exact: `Stackable 50/50` in a full inventory, recipe `x10 -> x5`: the C2 advisory check says full, `CanCraftRecipe` succeeds, and the craft leaves 45 in the same stack.
- `UCraftingQueryLibrary` (C2) stays as the conservative advisory helper; C6 should use `CanCraftRecipe`.

### Content

- `BP_TopDownCharacter` (`/Game/TopDown/Blueprints/BP_TopDownCharacter`): added one `UCraftingComponent` named `CraftingComponent` through Unreal MCP (`ActorTools.add_component`), set `AvailableRecipes = [Recipe_Axe, Recipe_WoodArrow]` (`ObjectTools.set_properties`), compiled with warnings as errors, saved only this asset. Save-time data validation reported no errors.
- No other Content change. Tracked `.uasset`/`.umap` packages 308 -> 308; no map, `__ExternalActors__` or `__ExternalObjects__` change.
- The two recipe packages are now referenced by the player Blueprint, so the map cook includes them (620 -> 622 packages).

### Validation

- `NullTideEditor Win64 Development` build: succeeded, no compiler warnings.
- Focused `NullTide.Crafting.C5`: 9/9 passed (`InventoryResolution`, `RecipeRegistration`, `RequestRejection`, `ExactPreflight`, `CraftFailures`, `ProductionRecipes`, `RevisionAndEventContract`, `Reentrancy`, `BlueprintApi`). `RecipeRegistration` also checks that `BP_TopDownCharacter` has exactly one crafting component registering `Recipe_Axe`, `Recipe_WoodArrow` in order. `BlueprintApi` checks function/delegate flags, that the inventory transaction functions are not Blueprint-exposed, and crafts through the reflected `TryCraftRecipe` entry point.
- Regression `NullTide.Crafting.C1` + `C2` + `C3C4` + `Inventory.Stacking` + `Inventory.M1`: 33/33 passed.
- Full permanent automation `NullTide.`: 147/147 passed (138 previous + 9 new).
- Windows cook with the editor closed: succeeded, 0 errors (615 cooked, 7 platform-skipped, 622 total). Environment warnings only.
- Tests ran headless through `UnrealEditor-Cmd` with `-NullRHI`.
- Rendered PIE: not run. Nothing in game calls the crafting component yet, so no visible behaviour changed.

### Notes

- Adding the new enums exposed two Python script-name collisions from C2/C3 (`ECraftingQueryResult`/`FCraftingQueryResult`, `ECraftingTransactionResult`/`FCraftingTransactionResult`). Fixed with `ScriptName = "...ResultCode"`, the same pattern as `EInventoryOperationResult`.

### Approved Design Decisions

All five were approved by the user without changes:

1. `OnCraftingResult` fires once per non-reentrant `TryCraftRecipe` call, for success and for failure, so UI can show feedback such as missing ingredients. `CanCraftRecipe` never notifies.
2. A reentrant `TryCraftRecipe` (from `OnInventoryChanged` during the commit, or from an `OnCraftingResult` handler) returns `CraftInProgress` without mutating or notifying. A handler therefore cannot chain another craft inside the same call.
3. `FCraftingTransactionResult` is reused as the component result type; `RecipeNotAvailable` and `CraftInProgress` were appended to `ECraftingTransactionResult`.
4. `CanCraftRecipe` does not report busy states, like the other queries. During an unrelated inventory mutation `TryCraftRecipe` can still return `InventoryBusy` after a successful preflight.
5. `ECraftingQueryResult` (C2) and `ECraftingTransactionResult` (C3/C4) carry `ScriptName = "...ResultCode"` to remove Python name collisions. C++ and Blueprint names are unchanged.

### Deferred

- Crafting UI (C6), hotkeys, timed crafting and craft cancellation (C7), workstations (C8), recipe unlock progression.

## C6A - Crafting UI Foundation

### Baseline and Scope

- Baseline: `0851c96` (C5 committed). Previous full automation 147/147.
- Read-only crafting UI. The CRAFT button reflects the exact `CanCraftRecipe` result but is not wired; crafting from the UI is C6B.
- Reviewed and approved by the user; committed as the C6A milestone.

### Audit Summary

- Inventory UI pattern: C++ base widgets hold the logic through `BindWidgetOptional` members; UMG assets only lay out; slots are separate widget classes created at runtime.
- Open/close: `BP_TopDownController` creates `W_Inventory` on `IA_OpeningInventory` (`IMC_Default`), shows the cursor and sets UI-only input; the widget closes itself on key presses. It binds to the possessed pawn's components and refreshes from `OnInventoryChanged`.
- Unreal MCP can create widget Blueprints with a native parent, add/move/wrap widgets, set properties and compile, so no `.uasset` was edited directly.

### User Decisions

- Crafting shares the inventory screen: inventory (backpack + item details) left, CHARACTER (existing equipment/tool loadout panel) middle, CRAFTING right. No character stats (no stats system exists).
- `Tab` is added to open and close the same screen as `I`.

### Source Files

- New: `Public/UI/CraftingViewLibrary.h` + `Private/UI/CraftingViewLibrary.cpp`, `Public/UI/CraftingPanelWidget.h` + `.cpp`, `Public/UI/CraftingRecipeEntryWidget.h` + `.cpp`, `Public/UI/CraftingRequirementRowWidget.h` + `.cpp`, `Private/Tests/CraftingUITests.cpp`.
- Changed: `Private/UI/InventoryScreenWidget.cpp` (close on `Tab` as well as `I`/`Esc`).

### Implementation

- `UCraftingViewLibrary::BuildRecipeView(Crafting, RecipeClass)` builds `FCraftingRecipeView`: recipe name, description, icon (falls back to the first output icon), ingredient and output lines (`FCraftingRequirementView`: item name/icon, owned total from `GetTotalQuantity`, required quantity, satisfied flag), the exact `UCraftingComponent::CanCraftRecipe` result, `bCanCraft` and a status line (`GetCraftStatusText`, for example "Missing Stone (2 / 5).", "Ready to craft.", "Not enough inventory space for the output."). It never crafts or changes inventory.
- `UCraftingPanelWidget` binds to the owning pawn's `UCraftingComponent` and its inventory (rebinding when the pawn or components change), refreshes on `OnInventoryChanged` and `OnCraftingResult`, lists `GetAvailableRecipes` as entries, keeps only the selected recipe class (defaulting to the first recipe), fills ingredient/output rows, and sets the CRAFT button enabled state from `bCanCraft`. Entries are rebuilt only when the registered recipe set changes, never from inside an entry click.
- `UCraftingRecipeEntryWidget`: button, icon, name and a READY marker; selected entries use the inventory accent colour. `UCraftingRequirementRowWidget`: icon, name and "owned / required" (ingredients) or "xN" (outputs); missing ingredients are tinted red.

### Content (created or changed through Unreal MCP)

- New `/Game/LevelPrototyping/CraftingSystem/UI/W_CraftingRequirementRow` (parent `UCraftingRequirementRowWidget`), `W_CraftingRecipeEntry` (parent `UCraftingRecipeEntryWidget`), `W_CraftingPanel` (parent `UCraftingPanelWidget`; defaults `RecipeEntryClass = W_CraftingRecipeEntry`, `RequirementRowClass = W_CraftingRequirementRow`). Colours and font sizes follow `W_Inventory`.
- `W_Inventory`: window 1180 x 720 -> 1680 x 760; `CenterPanel` and `DetailsOuter` wrapped in a new `InventorySection` (left column); `EquipmentPanel` moved to the middle with a new `CharacterTitle` ("CHARACTER") above the existing "TOOL LOADOUT" heading; `CraftingPanel` (a `W_CraftingPanel` instance) added as the right column. Every widget name bound by `UInventoryScreenWidget` is unchanged.
- `IMC_Default`: one mapping added, `IA_OpeningInventory` on `Tab`; the eight existing mappings (including modifier subobjects) are unchanged.
- All four widget Blueprints compiled with warnings as errors; save-time data validation reported no errors. Only these five assets were saved.
- Content delta: tracked packages 308 modified 2 (`W_Inventory`, `IMC_Default`), 3 added (on disk 311). No map, `__ExternalActors__`, `__ExternalObjects__`, controller, character, item or recipe change.

### Validation

- `NullTideEditor Win64 Development` build: succeeded, no compiler warnings (one missing include was fixed during development).
- Focused `NullTide.Crafting.C6A`: 6/6 passed (`RecipeView`, `ExactStatus`, `ViewIsReadOnly`, `WidgetAssets`, `InventoryLayout`, `InputMapping`).
- Regression `NullTide.Crafting.C1` + `C2` + `C3C4` + `C5` + `Inventory.Stacking` + `Inventory.M1` + `Gameplay.G4` + `G5` + `G6`: 61/61 passed.
- Full permanent automation `NullTide.`: 153/153 passed (147 previous + 6 new).
- Windows cook with the editor closed: succeeded, 0 errors (618 cooked, 7 platform-skipped, 625 total; the three new widgets are now cooked through `W_Inventory`).
- Rendered PIE: manual acceptance performed by the user, **12/12 checklist cases PASS (user-confirmed)**. The agent did not run or observe PIE itself.

### Notes for C6B

- `CraftButton` has no click binding yet; C6B binds it to `UCraftingComponent::TryCraftRecipe` and adds result feedback.
- The panel recreates requirement rows on each refresh; fine for the current recipe sizes.

## C6B - Craft Execution from UI

### Baseline and Scope

- Baseline: `d831079` (C6A committed; manual PIE 12/12 user-confirmed). Previous full automation 153/153.
- The existing CRAFT button now crafts the selected recipe. Layout, input and assets are unchanged from C6A.
- Manual rendered PIE accepted by the user (12/12 PASS); committed as the C6B milestone.

### Audit Summary

- `W_CraftingPanel` already contains `CraftButton` and `CraftStatusText`; `UCraftingPanelWidget` already subscribed to `OnInventoryChanged` and `OnCraftingResult`. The click only needed a native binding, so no asset change was required.

### Source Files

- Changed: `Public/UI/CraftingPanelWidget.h`, `Private/UI/CraftingPanelWidget.cpp`, `Public/UI/CraftingViewLibrary.h`, `Private/UI/CraftingViewLibrary.cpp`, `Private/Tests/CraftingTestTypes.h` (panel test listener).
- New: `Private/Tests/CraftingUIExecutionTests.cpp`.

### Behaviour

- Click path: `CraftButton.OnClicked` (bound once in `NativeOnInitialized`) -> `RequestCraftSelectedRecipe()` -> exactly one `UCraftingComponent::TryCraftRecipe(SelectedRecipe)`. The panel never touches the inventory or the transaction directly.
- Pre-call checks give local feedback only when no call is made: no crafting component ("Crafting unavailable.") or no selected recipe ("Select a recipe.").
- Feedback: only `OnCraftingResult` produces the craft message (`UCraftingViewLibrary::GetCraftFeedbackText`, decided by result code): "Crafted Axe x1." / "Crafted Wood Arrow x5.", "Not enough Wood (2 / 3).", "Not enough inventory space.", "Recipe unavailable." (invalid or unregistered), "Inventory is busy.", "No inventory available.", "Crafting failed.". Success is green, failure red. The return value of `TryCraftRecipe` is not displayed, so there is no duplicate message.
- Feedback lifetime: `FeedbackDisplaySeconds` (default 3 s, real time). The message survives inventory refreshes, clears when another recipe is selected or a new craft starts, and `ClearCraftFeedback()` clears it explicitly.
- Duplicate submissions: a request guard ignores reentrant requests (for example from an inventory or crafting event handler during the craft), and CRAFT is disabled while a request runs. Each separate click is one attempt; a click after the ingredients run out fails cleanly with feedback.
- Refresh: inventory slots refresh from `OnInventoryChanged` as before; the panel refreshes ingredient owned/required values, READY markers, the exact `CanCraftRecipe` status and the CRAFT enabled state. The selected recipe stays selected while it is registered.
- `SetCraftingSource(UCraftingComponent*)` binds the panel to a specific crafting component; null returns to following the owning pawn. Rebinding never leaves duplicate delegate bindings.

### Event Contract

- Successful craft from the UI: exactly one inventory revision increment, one `OnInventoryChanged` and one `OnCraftingResult`.
- Failed craft from the UI: no ingredient consumed, no output, ItemIds unchanged, revision unchanged, no `OnInventoryChanged`; one `OnCraftingResult` with the failure.

### Content

- No Content change. Tracked `.uasset`/`.umap` packages 311 -> 311; no map, `__ExternalActors__` or `__ExternalObjects__` change. No input mapping change.

### Validation

- The first build ran while the user's editor was open and produced a hot-reload module (`UnrealEditor-NullTide-0001.dll`). After the user closed the editor, a clean rebuild produced `UnrealEditor-NullTide.dll` and all validation below ran on that clean build.
- `NullTideEditor Win64 Development` clean build: succeeded, no compiler warnings.
- Focused `NullTide.Crafting.C6B`: 6/6 passed (`ClickRoutesThroughComponent`, `RefreshAfterCraft`, `FailureFeedback`, `DuplicateAndReentrantClicks`, `NoDuplicateBindings`, `SelectionAndFeedbackLifetime`). The tests create a real `W_CraftingPanel` in a transient world and click through the panel's `CraftButton` binding.
- Regression `NullTide.Crafting.C6A` + `C1` + `C2` + `C3C4` + `C5` + `Inventory.Stacking` + `Inventory.M1` + `Gameplay.G4` + `G5` + `G6` + `Inventory.G7`: 70/70 passed.
- Full permanent automation `NullTide.`: 159/159 passed (153 previous + 6 new).
- Blueprint compile with warnings as errors: `W_CraftingPanel`, `W_CraftingRecipeEntry`, `W_CraftingRequirementRow`, `W_Inventory` passed; none became dirty and nothing was saved.
- Windows cook with the editor closed: succeeded, 0 errors (618 cooked, 7 platform-skipped, 625 total).
- Rendered PIE: manual acceptance performed by the user, **12/12 checklist cases PASS (user-confirmed)**. The agent did not run or observe PIE itself.

## C7 - Timed Crafting

### Baseline and Scope

- Baseline: `94dceff` (C6B committed; manual PIE 12/12 user-confirmed). Previous full automation 159/159.
- Design approved by the user, including all four decisions (see `CRAFTING_PLAN.md`, C7).
- Approved by the user and committed as the C7 milestone (manual rendered PIE: 12 PASS, 1 NOT TESTED).

### Source Files

- Changed: `Public/Crafting/CraftingComponent.h` + `Private/Crafting/CraftingComponent.cpp` (timed lifecycle, events, movement detection), `Public/Crafting/CraftingTransaction.h` (result codes `Started`, `TimedCraftRequired`), `Public/UI/CraftingViewLibrary.h` + `.cpp` (started/cancelled/progress text), `Public/UI/CraftingPanelWidget.h` + `.cpp` (CRAFT/CANCEL, progress, new event bindings, reopen state), `Private/Tests/CraftingTestTypes.h` (timed fixtures and listener), `Private/Tests/CraftingRecipeTests.cpp` (instant fixture, Pickaxe production entry, per-recipe duration), `Private/Tests/CraftingComponentTests.cpp` (three production registrations).
- New: `Private/Tests/CraftingTimedTests.cpp`.

### Lifecycle (consume at completion)

- `TryStartCraft(RecipeClass)`: same validation as `TryCraftRecipe`. Duration 0 crafts immediately (`OnCraftingResult`). Duration > 0 runs the exact preflight (`FCraftingTransaction::CanCraftRecipe`); on success it stores the active recipe and start location, sets one world timer, binds movement, broadcasts `OnCraftStarted(Recipe, Duration)` and returns `Started`. Nothing is consumed or reserved. A refused start broadcasts `OnCraftingResult` once.
- Completion: the timer fires; the active state, timer and movement bindings are cleared first; inside the craft guard the request is re-validated (registered, valid, inventory present) and `FCraftingTransaction::TryCraftRecipeAtomic` runs; `OnCraftingResult` is broadcast once. If the inventory changed so the craft no longer fits or ingredients are short, completion fails atomically (no consumption, no output, no revision, no inventory event).
- One active craft per player: while crafting, `TryStartCraft` and `TryCraftRecipe` return `CraftInProgress` (one notification each) and `CanCraftRecipe` returns `CraftInProgress`.
- `TryCraftRecipe` on a timed recipe returns `TimedCraftRequired` without mutation, so the timer cannot be bypassed. Instant recipes behave as before.
- `CancelCraft()` cancels with reason `Explicit` (returns false when idle). `EndPlay` clears the timer, bindings and state without any notification.
- Progress queries `IsCrafting`, `GetActiveCraftRecipe`, `GetActiveCraftProgress` (0..1), `GetActiveCraftRemainingSeconds` read the world timer; no copies are stored and the component does not tick.

### Re-entrancy

- `OnCraftStarted`, `OnCraftCancelled` and `OnCraftingResult` are broadcast inside the component's craft guard. A synchronous `TryStartCraft`/`TryCraftRecipe` from any of them returns `CraftInProgress` with no extra notification. On completion listeners already see `IsCrafting() == false`. A later independent request starts normally; no guard or binding is left behind.

### Movement and Jump Cancellation

- While a timed craft runs and the owner is an `ACharacter`, the component binds `OnCharacterMovementUpdated` and `MovementModeChangedDelegate` (unbound on completion, cancellation and `EndPlay`).
- Horizontal distance from the start location above `MovementCancelTolerance` (10 cm) cancels with `Movement`. Any displacement source counts, including click-to-move path following. Vertical-only movement does not cancel.
- Movement mode `Falling` cancels with `Jump`.
- No change to `BP_TopDownCharacter` movement graphs, `BP_TopDownController`, `IMC_Default` or the gathering cancel code.

### UI

- CRAFT calls `TryStartCraft`. While a timed craft runs the same button reads CANCEL and calls `CancelCraft`; `CraftProgressBar` shows authoritative progress; the status reads "Crafting Pickaxe... 1.4 s".
- Closing the inventory screen does not cancel. Reopening shows the active recipe selected, the current progress, remaining time and CANCEL.
- Feedback: instant crafts as in C6B; a timed start shows progress (no success message); completion shows the C6B success or the correct failure message; cancellation shows "Crafting cancelled.", "Crafting cancelled: you moved." or "Crafting cancelled: you jumped.". Messages are not overwritten by inventory refreshes.

### Content (through Unreal MCP)

- New `/Game/LevelPrototyping/CraftingSystem/Recipes/Recipe_Pickaxe`: Wood x2 + Stone x3 -> Pickaxe x1, `CraftDuration = 3.0`, icon `T_Icon_Pickaxe`.
- `BP_TopDownCharacter`: `CraftingComponent.AvailableRecipes = [Recipe_Axe, Recipe_WoodArrow, Recipe_Pickaxe]`.
- `W_CraftingPanel`: one `ProgressBar` named `CraftProgressBar` (green fill, hidden while idle) between `CraftStatusText` and `CraftButton`; the existing `CraftButtonText` is now bound for the CRAFT/CANCEL label.
- `Recipe_Pickaxe`, `BP_TopDownCharacter`, `W_CraftingPanel` and `W_Inventory` compiled with warnings as errors; only the three changed assets were saved; save-time validation reported no errors. No other asset was dirty.
- Content delta: tracked packages 311, 2 modified, 1 added (on disk 312). No map, `__ExternalActors__` or `__ExternalObjects__` change.

### Test Fixture Correction

- `UCraftingTestRecipeDefinition` is now instant (`CraftDuration = 0`), so the C5/C6 tests still test instant crafting through `TryCraftRecipe`/`TryStartCraft`; the C1 contract asserts 0. New timed fixtures `UCraftingTimedTestRecipe` (2 s) and `UCraftingTimedHeavyTestRecipe` (1 s, two outputs).
- The C1 production test now covers `Recipe_Pickaxe` (including its 3 s duration) and checks each recipe's authored duration; the C5 registration test expects all three production recipes in order.

### Validation

- `NullTideEditor Win64 Development` clean build (editor closed): succeeded, no compiler warnings.
- Focused `NullTide.Crafting.C7`: 9/9 passed (`StartAndTimedCompletion`, `InventoryChangedDuringCraft`, `ExplicitCancel`, `MovementAndJumpCancel`, `RejectionsAndInstantRegression`, `EndPlayCleanup`, `ReentrancyProtection`, `ProductionPickaxe`, `PanelCloseAndReopen`). Tests run on a spawned `ACharacter` in a transient world and advance the world timer manually.
- Regression `NullTide.Crafting.C1`-`C6B` + `Inventory.Stacking` + `Inventory.M1` + `Gameplay.G4`/`G5`/`G6` + `Inventory.G7` + `ResourceGathering`: 95/95 passed.
- Full permanent automation `NullTide.`: 169/169 passed (159 previous + 9 C7 + 1 new C1 `ProductionRecipes.Pickaxe` case).
- Windows cook with the editor closed: succeeded, 0 errors (619 cooked, 7 platform-skipped, 626 total).
- Final pre-commit Windows cook (editor closed, after the icon reimport): `Done!`, 0 errors, 2 environment warnings (NTFS journal on `D:`, MCP licence notice); 619 cooked, 7 platform-skipped, 626 total; `T_Icon_Axe`, `T_Icon_Pickaxe` and `Recipe_Pickaxe` are in the cooked asset registry.
- Rendered PIE (performed and reported by the user; the agent did not run or observe PIE): **12 PASS, 1 NOT TESTED**. Cases 1-7 and 9-13 passed. Case 8 (click-to-move cancel) was NOT TESTED because the mouse cursor is hidden in game mode; it remains a deferred manual verification. The displacement cancel path itself is covered by automation (`MovementAndJumpCancel`).
- User observations to follow up (not blocking):
  - Case 4: occasionally a quantity briefly flickers to another number and back after the result message disappears; no resources are lost. Cosmetic; to investigate in the inventory/crafting UI/UX pass.
  - Case 7: after closing the screen, moving and reopening, the cancellation message is not shown and the first recipe (Axe) is selected, because the message and selection live in the panel instance that was destroyed when the screen closed. To address in the UI/UX pass (for example a component-held "last craft outcome").
  - Instant recipes (Axe, Wood Arrow) craft without a progress bar, as designed.

### Development Notes

- One C7 test first called `EndPlay` on a component that had never begun play and hit the engine's `bHasBegunPlay` assertion; the test now calls `BeginPlay` first, matching the real lifecycle.
- `FTimerManager` ticks once per frame and activates timers set before a frame's tick only after advancing, so the C7 tests advance `GFrameCounter` and tick a zero-length frame before each simulated time step.

### Known Limitations

- No HUD indicator or toast while the inventory screen is closed; completion feedback appears when the screen is open.
- Non-character owners get no automatic movement cancellation (explicit Cancel and `EndPlay` still apply).
- Damage cancellation is deferred until combat exists; an active craft is not saved.

### Tool Icon Refresh (user request after C7 PIE)

- New 1254 x 1254 source images supplied by the user for Axe and Pickaxe, copied to `Content/Items/Icons/T_Icon_Axe.png` and `Content/Items/Icons/T_Icon_Pickaxe.png` (next to the other icon source PNGs).
- The MCP texture import refuses to overwrite existing assets, so the user reimported `T_Icon_Axe` and `T_Icon_Pickaxe` in the editor (Reimport With New File) and saved them. This keeps the asset paths, so `Item_Axe`, `Item_Pickaxe`, `Recipe_Axe`, `Recipe_Pickaxe` and the icon-path tests need no change.
- Verified through Unreal MCP: both textures are still 1254 x 1254 with `TC_EditorIcon`, no mipmaps, sRGB; not dirty; referenced only by their item and recipe; the item `ItemIcon` and recipe `RecipeIcon` still point at them.
- Tests after the reimport: `NullTide.Gameplay.G1` + `G2` + `Crafting.C1` + `C6A` + `C7` + `ResourceGathering.RG3`: 54/54 passed. The final pre-commit cook (editor closed) succeeded with both icons cooked.
- Content delta for this request: `T_Icon_Axe.uasset`, `T_Icon_Pickaxe.uasset` modified; `T_Icon_Axe.png`, `T_Icon_Pickaxe.png` added as source images.

## C8A - Workstation Data Model and Authority

### Baseline and Scope

- Baseline: `0cf655f` (C7 complete, approved and pushed). Previous full automation 169/169.
- The user approved the C8 design with all nine decisions unchanged, plus `Recipe_ShortSword` as the Workbench recipe (see `CRAFTING_PLAN.md`, C8).
- C8A scope: station data model, station-aware validation in `UCraftingComponent`, station lifetime handling and completion re-validation, two data assets, tests. No station actor, no UI change, no map change, no registration on the player.
- COMPLETE / APPROVED by the user: all implementation decisions accepted, full automation 180/180 and the Windows cook (0 errors) accepted, no manual PIE required. Committed as the C8A milestone.

### Source Files

- New:
  - `Public/Crafting/CraftingStationDefinition.h` + `Private/Crafting/CraftingStationDefinition.cpp`
  - `Public/Crafting/CraftingStationComponent.h` + `Private/Crafting/CraftingStationComponent.cpp`
  - `Private/Tests/CraftingStationTests.cpp`
- Changed:
  - `Public/Crafting/CraftingRecipeDefinition.h` + `.cpp` (`RequiredStation` and its validation rule)
  - `Public/Crafting/CraftingTransaction.h` (result codes `StationRequired`, `StationOutOfRange`)
  - `Public/Crafting/CraftingComponent.h` + `.cpp` (cancel reason `StationLost`, station-aware validation, new API, station lifetime binding)
  - `Private/Tests/CraftingTestTypes.h` (station fixtures)
  - `Private/Tests/CraftingRecipeTests.cpp` (`RecipeContract` checks that `RequiredStation` defaults to null)

### Data Model

- `UCraftingStationDefinition`: abstract, const, data-only Blueprint subclasses, read from the CDO (the same pattern as items and recipes).
  - Fields: `StationName`, `StationDescription`, `StationIcon`, `UseRange` (cm, default 300).
  - `ValidateStation` requires `UseRange > 0`. `IsDataValid` reports the same.
  - `ResolveValidStation(Class)` returns the CDO of a concrete, valid station class, or null with a diagnostic.
- `UCraftingStationComponent`: context only; no tick, no state besides `StationDefinition` (`EditDefaultsOnly`).
  - `GetStationDefinition`.
  - `IsStationUsable`: the owner is valid, not being destroyed, and the definition is valid.
  - `IsActorInUseRange`: 2D distance between actor locations is at most `UseRange`.
  - `IsDataValid` requires a valid definition.
- `UCraftingRecipeDefinition::RequiredStation`: null means a hand recipe. `ValidateRecipe` rejects an abstract or invalid station class, with a diagnostic that names `RequiredStation`.

### Authority (UCraftingComponent stays the only crafting authority)

- New API:
  - `GetRecipesForStation(Station)`: registered valid recipes. A usable station lists the recipes for that exact station kind first (authored order), then hand recipes. A null, destroyed or invalid station lists hand recipes only. Range is not considered when listing.
  - `CanCraftRecipeAtStation(Recipe, Station)`: exact read-only preflight.
  - `TryStartCraftAtStation(Recipe, Station)`: same instant/timed/notification behaviour as `TryStartCraft`.
  - `GetActiveCraftStation()`: the station the active timed station recipe depends on, or null.
- The existing `CanCraftRecipe` / `TryStartCraft` / `TryCraftRecipe` are the hand context (station = null). A station recipe returns `StationRequired` with no mutation and one notification.
- Validation order in `ValidateRequest`:
  1. recipe valid (`InvalidRecipe`);
  2. registered (`RecipeNotAvailable`);
  3. station access: null, destroyed, unusable or wrong kind -> `StationRequired`; out of range -> `StationOutOfRange`; hand recipes skip this step;
  4. inventory (`InvalidInventory`);
  5. active craft (`CraftInProgress`);
  6. exact transaction preflight.
- Station pointers are always checked with `IsValid`. The active station is a weak pointer read with `Get()`, so a stale station resolves to null and is rejected as `StationRequired`.
- No second crafting authority, inventory or transaction: the craft still runs `FCraftingTransaction::TryCraftRecipeAtomic` once.

### Station Lifetime and Completion

- A timed **station** recipe records the station (weak pointer) and binds the station actor's `OnEndPlay` for the duration of the craft. Hand recipes started from a station context record nothing and bind nothing.
- Station actor `EndPlay`:
  - reason `Destroyed` or `RemovedFromWorld`: the craft is cancelled immediately with `StationLost` (one `OnCraftCancelled`, no `OnCraftingResult`, no inventory change);
  - any other reason (PIE end, quit, level transition, meaning the whole world ends): the craft is cleared silently, like the crafter's own `EndPlay`.
- Completion clears the active state and all bindings first, then re-validates registration, station access (kind, lifetime, range) and inventory inside the craft guard, then runs one atomic transaction. A station that moved out of range, changed kind, lost its definition, or whose component was destroyed (the actor still alive, so no `OnEndPlay`) fails the completion atomically, with no consumption, no revision and no inventory event.
- Cancel (explicit, movement, jump, `StationLost`), completion and `EndPlay` all clear the timer, the movement bindings, the station binding and the active station.
- One active craft per player across all contexts: hand and station crafts block each other, and a second station of the same kind is refused with `CraftInProgress`.

### Content (through Unreal MCP)

- New `/Game/LevelPrototyping/CraftingSystem/Stations/Station_Workbench` (parent `UCraftingStationDefinition`): `StationName` "Workbench", `StationDescription` "A sturdy bench for crafting tools and weapons.", `UseRange` 300, `StationIcon` empty (no art yet).
- New `/Game/LevelPrototyping/CraftingSystem/Recipes/Recipe_ShortSword`:
  - Wood x2 + Scrap x4 -> ShortSword x1, `CraftDuration = 4.0`, `RequiredStation = Station_Workbench`.
  - Name "Short Sword", description "Forge a light one-handed sword at a workbench.", icon `T_Icon_ShortSword`.
- Both compiled with warnings as errors and saved by explicit path. Nothing else was dirty (checked: existing recipes, `BP_TopDownCharacter`, `Lvl_TopDown`, the referenced items, `W_CraftingPanel`, `W_Inventory`).
- `Recipe_ShortSword` is **not** registered on `BP_TopDownCharacter`; `ProductionStationAssets` checks this.
- Content delta: 312 tracked packages, 0 modified, 2 added (314 on disk). No map, `__ExternalActors__` or `__ExternalObjects__` change.

### Validation

- `NullTideEditor Win64 Development` build with the editor closed: succeeded, no compiler warnings. The final build includes a comment-only header change made after the first full run; every result below is from the final binary unless noted.
- Focused `NullTide.Crafting.C8A`: 11/11 passed:
  - `StationDefinitionContract`
  - `RecipeStationValidation`
  - `RecipeFiltering`
  - `HandContextRejectsStationRecipe`
  - `StationMismatchAndRange`
  - `StationInstantAndTimedCraft`
  - `StationLostCancels`
  - `CompletionRevalidatesStation`
  - `OneActiveCraftAcrossContexts`
  - `MovementCancelAtStation`
  - `ProductionStationAssets`
- Crafting regression `NullTide.Crafting` (C1 5, C2 5, C3C4 8, C5 9, C6A 6, C6B 6, C7 9, C8A 11): 59/59 passed.
- Full permanent automation `NullTide`: 180/180 passed (169 previous + 11 C8A), on both the first build and the final build. No test warnings; only environment warnings in the log.
- Windows cook with the editor closed: `Done!`, `Success - 0 error(s), 2 warning(s)` (environment only: NTFS journal on `D:`, MCP licence notice); 619 cooked, 7 platform-skipped, 626 total, the same as C7. `Station_Workbench` and `Recipe_ShortSword` are not cooked yet, because nothing cooked references them until C8C registers the recipe. A first cook attempt from Git Bash had its `-Map=/Game/...` argument rewritten into a Windows path by MSYS; it was rerun with `MSYS_NO_PATHCONV=1`.
- No rendered PIE: C8A has no station actor or UI path.

### Development Notes

- The first `StationLostCancels` run failed: the C7-style transient world has no world context and uninitialized actors, so `RouteEndPlay` never ran `EndPlay` on the station actor. The C8A fixture now uses the engine's CQTest `ActorTestSpawner` setup (`CreateNewWorldContext`, `InitializeActorsForPlay`; teardown routes `EndPlay`, then destroys the world and its context). This also removed the "World has no context" warnings from the earlier run. No production code changed for this.
- A freshly created recipe Blueprint fails a warnings-as-errors compile while it is still empty (an invalid recipe), so the recipe data is set on the CDO before the first compile.

### Known Limitations / Deferred

- Hand-context UI text for the new codes (`StationRequired`, `StationOutOfRange`, `StationLost`) comes in C8C. Until then the generic fallback text would show, but no UI path can produce these codes in C8A because the station recipe is not registered on the player.
- If a station component is destroyed alone (its actor alive), the craft is not cancelled immediately; completion fails atomically with `StationRequired`. The user accepted this for C8A and asked not to redesign component lifetime. Requirement carried to C8C: the station UI must close or become unusable when its station component is invalid.
- `Station_Workbench` and `Recipe_ShortSword` are not cooked yet (nothing cooked references them). The user expects this; the final C8C cook must verify both are cooked.
- C8B (`ACraftingStationActor`, `BP_Workbench`, placed bench) and C8C (UI integration, registering `Recipe_ShortSword`, PIE) are not started.

## C8B - Workbench Actor and Interaction

### Baseline and Scope

- Baseline: `c867e6d` (C8A approved and committed; not pushed). Previous full automation 180/180.
- Scope: the station actor and the interaction contract only. C8B has no crafting screen, so `F` on the bench is handled and rejected safely; C8C wires the screen.
- COMPLETE / APPROVED by the user (manual rendered PIE 8 PASS, 1 N/A; automation 183/183; cook PASS). Committed as the C8B milestone.

### Source Files

- New:
  - `Public/Crafting/CraftingStationActor.h` + `Private/Crafting/CraftingStationActor.cpp`
- Changed:
  - `Private/Tests/CraftingTestTypes.h` (`ACraftingTestStationActor`, a concrete test subclass)
  - `Private/Tests/CraftingStationTests.cpp` (C8B tests next to C8A, reusing its station fixture)
- Not changed: `UCraftingComponent`, `UInventoryScreenWidget`, `UCraftingPanelWidget`, storage, gathering, input.

### ACraftingStationActor

- Abstract, Blueprintable, no tick. It has the same shape as `AStorageContainerActor`, but with an unscaled `StationRoot` scene component as root, so placing or scaling the actor never distorts the mesh or bounds.
  - `StationMesh`: placeholder `/Engine/BasicShapes/Cube`, 160 x 80 x 80 cm (scale 1.6/0.8/0.8), raised 40 cm so it stands on the actor origin, collision `BlockAllDynamic`.
  - `InteractionBounds`: box with extent 150 x 120 x 110 at +90 cm, query-only, overlaps all channels, generates overlap events, does not affect navigation. It is well inside the 300 cm `UseRange`.
  - `CraftingStation`: a `UCraftingStationComponent`.
  - `CraftingScreenClass` (`TSubclassOf<UInventoryScreenWidget>`, `EditDefaultsOnly`): unset in C8B, for C8C.
- `GetOpenCraftingResult(Interactor)` (pure) returns `ECraftingStationOpenResult`. Checks run in this order:
  1. `InvalidInteractor`: null or destroyed interactor, or a player controller without a pawn.
  2. `StationUnavailable`: no usable station component or definition.
  3. `NoPlayerController`: not a player controller, and not a pawn controlled by one.
  4. `OutOfRange`: pawn outside `UseRange`.
  5. `NoCraftingScreen`: no screen class set.
  6. `Ready`.
- `OpenCraftingForInteractor(Interactor)`: logs the result to the `LogCraftingStation` category and returns false. In C8B it returns false even for `Ready`, because the station screen is wired in C8C. A rejected request creates no widget and changes no input mode, cursor, inventory or crafting state.
- The actor never creates a `UCraftingComponent` or an inventory and never crafts.

### BP_Workbench (through Unreal MCP)

- `/Game/LevelPrototyping/CraftingSystem/Stations/BP_Workbench`, parent `ACraftingStationActor`.
  - `CraftingStation.StationDefinition = Station_Workbench`; `CraftingScreenClass` unset.
  - Implements `BPI_Interactable`: `GetInteractionType` returns Press (`NewEnumerator0`), `GetHoldDuration` returns 0, and `Interact(Interactor)` calls `OpenCraftingForInteractor(Interactor)`.
  - No variables, no added components; the native components only.
  - Compiled with warnings as errors and saved by explicit path.
- How it was built: the MCP toolsets cannot add a Blueprint interface to a new Blueprint, so `BP_Sword` (parent `AActor`, already implements `BPI_Interactable`, no native components) was duplicated and reparented to `ACraftingStationActor`. Then:
  - its `Sword`, `Sphere` and `DefaultSceneRoot` components and its two variables were removed;
  - its event graph was replaced;
  - its two interface functions were set to constants.
- A first attempt duplicated `BP_StorageContainer` instead. It left `InteractionBounds` with a stale attach parent (storage's `ContainerMesh`) and exposed that the mesh-as-root layout loses the mesh scale when the actor is placed. That attempt was fully rolled back (the unsaved placed actor removed and the asset deleted), and the actor now uses a separate scene root.
- The duplicate step only marked the source Blueprints (`BP_StorageContainer`, `BP_Sword`) dirty in the editor. They were never saved; both are unchanged on disk.

### World Placement

- Exactly one `BP_Workbench` placed in `/Game/TopDown/Lvl_TopDown`: label `C8B_Workbench`, location (-620, -700, 0), yaw 0. This is the empty south-west corner below the central spawn plateau, mirroring `BP_StorageContainer` at the north-west corner (-560, 560).
- Clearance: actor bounds (-770..-470, -820..-580, z -20..200) overlap no other actor. They stay clear of the plateau (-500..500), the west ramp (x -900..-500, y -200..200), the south ramp (x -200..200, y -900..-500), storage, all resource nodes and all pickups (nearest is `G3_Pickup_HealthPotion` at (-900, -600)).
- World Partition audit: 90 external actor packages before, 91 after. The one new package is `Content/__ExternalActors__/TopDown/Lvl_TopDown/2/PF/Z2WV4MR2XZR5TL1I53GCTS.uasset` (`C8B_Workbench`), saved by the user with Ctrl+S. No existing external actor package was modified; `__ExternalObjects__` is unchanged (3). No existing actor was moved, renamed or modified. 97 actors before, 98 after.
- There is no door (`BP_Door`) placed in `Lvl_TopDown`; the door Blueprint exists but has no placed instance.

### Validation

- `NullTideEditor Win64 Development` build with the editor closed: succeeded, no compiler warnings.
- Focused `NullTide.Crafting.C8B`: 3/3 passed.
  - `ActorContract`: scene root, mesh and bounds layout, collision, station component and definition, no crafting or inventory component, screen class unset.
  - `OpenRejectsInvalidInteractor`: null, destroyed, non-pawn, unpossessed pawn, pawnless controller, station without a definition, out of range, and no screen class are each rejected. A valid request reports `Ready` but still opens nothing in C8B. A station craft running at the same bench is not affected, and inventory, crafting state, cursor, screen count and crafting-component count are unchanged.
  - `ProductionWorkbench`: `BP_Workbench` has `Station_Workbench`, no screen class, implements `BPI_Interactable` with Press, keeps the native layout and has no extra authority. `Interact(Character)` through the interface changes nothing, and hand crafting still works afterwards.
- The first in-editor run of `ProductionWorkbench` failed on two test-side issues: a Blueprint function returns through an out parameter rather than a return property, and the fixture lacked the hand recipe's second ingredient. Both were fixed in the test; no production change.
- Full permanent automation `NullTide`: 183/183 passed (180 previous + 3 C8B). Crafting C1 5, C2 5, C3C4 8, C5 9, C6A 6, C6B 6, C7 9, C8A 11, C8B 3; Inventory 53; Gameplay 50; ResourceGathering 18.
- Windows cook with the editor closed: `Done!`, `Success - 0 error(s), 2 warning(s)` (environment only); 621 cooked, 7 platform-skipped, 628 total (+2 compared with C8A). `BP_Workbench` and `Station_Workbench` are in the cooked asset registry (the placed bench references them); `Recipe_ShortSword` still waits for C8C.
- Content delta: 1 new asset (`BP_Workbench`) and 1 new external actor package; no modified tracked package.

### Manual Rendered PIE Checklist (for the user; not run by the agent)

Expected C8B behaviour: `F` on the bench does nothing visible. The Output Log shows `LogCraftingStation: ... crafting station not opened for ... (NoCraftingScreen).`

1. The workbench is visible near the spawn, at the south-west corner below the plateau.
2. The placeholder mesh is about 160 x 80 x 80 cm on the ground, and the character cannot walk through it.
3. The `F` interaction prompt appears when approaching the bench.
4. `F` is handled safely: no screen opens, the cursor does not appear, and the log line above is printed.
5. The character can still move normally after pressing `F`.
6. Storage interaction still works (`F` on `BP_StorageContainer` opens the storage screen).
7. Door interaction: N/A, because no door is placed in `Lvl_TopDown`.
8. Tree, Rock and Scrap gathering still work.
9. The bench does not obstruct navigation (click-to-move around it and down the west and south ramps).

PIE results (performed and reported by the user; the agent did not run or observe PIE): **8 PASS, 1 N/A, 0 FAIL, 0 NOT TESTED**. Items 1-6, 8 and 9 passed; item 7 (door) is N/A because no door actor exists in `Lvl_TopDown`.

## Next Checkpoint

C8C - Workstation crafting UI: NEXT, not started (needs explicit go-ahead). Crafting queue and the inventory/crafting UI-UX pass remain deferred.
