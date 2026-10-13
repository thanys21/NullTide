# Crafting Plan

Phase 4 of `NULLTIDE_MASTER_PLAN.md` (COMPLETE). Crafting was delivered in small checkpoints (C1-C8). Each checkpoint is validated, documented in `CRAFTING_PROGRESS.md` and committed before the next one starts.

## Baseline

- Starting commit: `0af68d6` (Inventory Stacking complete, docs updated).
- Test baseline: full permanent suite 121/121.

## Agreed Design Decisions

| Decision | Choice | Reason |
| --- | --- | --- |
| Recipe asset form | Data-only Blueprint subclasses of an abstract, const `UCraftingRecipeDefinition`, referenced by `TSubclassOf` and read from the CDO | Same pattern as `UItemDefinition`, so recipes and items are authored, referenced and validated the same way. |
| Workstation requirement | Not in C1. Designed in C8. | Avoid a speculative field before workstations exist. |
| Craft duration | `CraftDuration` field exists from C1 (`0` = instant). Timed behaviour waits for C7. | Static data only; no runtime state in recipe assets. |
| First production recipes | `Recipe_Axe` (Wood x3 + Stone x5 -> Axe x1), `Recipe_WoodArrow` (Wood x1 -> WoodArrow x5) | Axe covers a non-stackable output; WoodArrow covers a stackable output that C4 can merge into existing stacks. |

## C1 - Recipe Data Model

### In scope

1. `FCraftingItemQuantity` (USTRUCT): `TSubclassOf<UItemDefinition> ItemDefinition`, `int32 Quantity` (min 1).
2. `UCraftingRecipeDefinition` in `Source/NullTide/{Public,Private}/Crafting/`:
   - `RecipeName`, `RecipeDescription`, `RecipeIcon` (named like `ItemName`/`ItemDescription`/`ItemIcon`; production recipes reuse the output item's icon)
   - `Ingredients` and `Outputs` (`TArray<FCraftingItemQuantity>`)
   - `CraftDuration` (seconds, `>= 0`)
3. `ValidateRecipe(FString& OutDiagnostic)` (BlueprintPure). A recipe is invalid when:
   - it has no ingredients or no outputs;
   - an entry has a null or abstract item class;
   - a referenced item definition fails `ValidateFragments`;
   - an entry has `Quantity < 1`;
   - the same item appears twice in `Ingredients`, or twice in `Outputs`;
   - `CraftDuration < 0`.
   The diagnostic names the list and index of the first failing entry. `IsDataValid` reports the same failure in the editor.
4. Production recipe assets in `Content/LevelPrototyping/CraftingSystem/Recipes/`, created through Unreal MCP: `Recipe_Axe`, `Recipe_WoodArrow`.
5. Automation `NullTide.Crafting.C1.*`, with fixtures in `Private/Tests/CraftingTestTypes.h`:
   - `RecipeContract`: a valid recipe exposes its static data from the CDO.
   - `ValidationRejectsInvalid`: one case per validation rule, including the diagnostic.
   - `ProductionRecipes`: both production assets load, validate and hold the agreed quantities.

### Out of scope

Quantity queries (C2), ingredient consumption (C3), outputs (C4), `UCraftingComponent` (C5), UI (C6), timed crafting (C7), workstations (C8).

### Validation

1. `NullTideEditor Win64 Development` build (editor closed).
2. Focused `NullTide.Crafting.C1`.
3. Regression `NullTide.Inventory.Stacking`, `NullTide.Gameplay.G1`.
4. Full permanent suite (121 + new C1 tests).
5. Windows cook.
6. Content audit: only the two new recipe packages are added; no map or World Partition change.
7. No rendered PIE: C1 has no gameplay-visible behaviour.
8. Update `CRAFTING_PROGRESS.md`, `NULLTIDE_MASTER_PLAN.md`, `CLAUDE.md`; commit; stop for review.

## C2 - Inventory Quantity Queries

Baseline: `08ed028` (C1 approved). Read-only checkpoint: nothing is consumed or added.

### Decisions

| Decision | Choice | Reason |
| --- | --- | --- |
| Dependency direction | `UInventoryComponent` stays generic and never includes crafting types. Recipe queries live in a crafting-side `UCraftingQueryLibrary` that calls the generic inventory queries. | User rule: inventory is generic authority. |
| One capacity algorithm | `TryAddDefinitionQuantity` and the new add queries share one private `PreflightAdd` that wraps the existing `FInventoryStacking::BuildAddPlan`. | No second stack simulation. |
| Query result type | Queries return `FInventoryOperationResult` with the same reason codes as the `Try*` calls. New code `InsufficientQuantity` (appended to the enum) reports a short total. | Same shape as existing APIs; callers can tell "not enough" from other failures. |
| Multiple recipe outputs | A generic batch query `CanAddDefinitionQuantities` checks all outputs together: per-definition merges plus the sum of new stacks against free slots. Duplicate definitions are summed first. | Checking each output alone can say "fits" when two outputs compete for the last free slot. |
| Ingredient/output overlap | C2 output capacity is checked against the current inventory, before any consumption. This is conservative: consumption can only free space, so a "fits" answer stays true after consumption, but a full inventory may be rejected even though consuming the ingredients would make room. The exact remove-then-add check belongs to the C3/C4 transaction preflight. | Avoid locking in a check that could approve a craft that later loses ingredients. |
| Busy state | Queries do not report `Busy`; they describe contents and capacity. `NotInitialized` is reported when native authority is inactive, as the `Try*` calls do. | Queries are side-effect free and may run inside change handlers. |

### In scope

1. `UInventoryComponent` (generic):
   - `GetTotalQuantity(DefinitionClass)`: sum of `Quantity` over stacks of exactly that class; `0` for null or unknown classes.
   - `CanRemoveDefinitionQuantity(DefinitionClass, Quantity)`: `InvalidQuantity` for `< 1`, `InvalidDefinition` for null, `InsufficientQuantity` when the total is short, otherwise `Success`.
   - `CanAddDefinitionQuantity(DefinitionClass, Quantity)`: same validation and capacity result as `TryAddDefinitionQuantity`, without committing.
   - `CanAddDefinitionQuantities(Requests)`: batch form using the new `FInventoryDefinitionQuantity` struct.
2. `UCraftingQueryLibrary` (crafting side): `CheckRecipeIngredients` and `CheckRecipeOutputCapacity`, returning `FCraftingQueryResult` (reason, failing ingredient index, underlying inventory result).
3. Automation `NullTide.Crafting.C2.*` covering every case in the C2 request, each proving contents, revision and change events are unchanged.

### Out of scope

Consuming ingredients, adding outputs, any craft transaction, `UCraftingComponent`, UI, C3.

## C3/C4 - Atomic Craft Transaction (one checkpoint)

Baseline: `b7a2b86` (C2 approved). C3 (ingredient consumption) and C4 (outputs) are delivered as **one** atomic inventory transaction. They are never separate gameplay transactions. Approved and committed after user review.

### Decisions

| Decision | Choice | Reason |
| --- | --- | --- |
| Generic primitive | `UInventoryComponent::TryApplyDefinitionTransaction(Removals, Additions)` taking `FInventoryDefinitionQuantity` arrays and returning `FInventoryTransactionResult` (reason, failed side, failed caller index). Native C++ only for now; not a `UFUNCTION`. | Inventory stays generic. Keeping it out of Blueprints until the C5 crafting authority exists stops UI from calling it directly. |
| Projected state | New planning entry `FInventoryStackEntry { Item, DefinitionClass, Quantity }` in the private `InventoryStacking.h`. `Item` is the live stack (never written during planning) or null for a planned stack that does not exist yet. | Smallest representation that lets one stacking algorithm run on live or projected contents. |
| One stacking algorithm | `BuildAddPlan` becomes a thin wrapper that builds entries from live items and calls `BuildAddPlanForEntries`. Normal adds, storage transfers and craft outputs all use the same function. | No second stacking algorithm. |
| Removal planning | `FInventoryStacking::PlanRemoval` works only on a copy of the entries: exact class match, earliest stacks first, partial removal allowed, zero-quantity entries dropped with order preserved. | Deterministic, no live mutation during preflight. |
| Output planning | Outputs are planned in order against the post-removal entries. Each output plan is applied to the working copy (merged quantities updated, planned new stacks appended) so later outputs see the slots already used. | Exact capacity, including freed slots and ingredient/output overlap. |
| Aggregation | Duplicate definitions are summed separately for removals and for additions inside the primitive. A failure reports the first caller index of that definition. | Generic callers may send duplicates; recipes cannot (`ValidateRecipe`). |
| Commit | Inside one mutation guard: stage all new stacks first, then set surviving stack quantities to their projected values, drop fully consumed stacks, apply output merges, append new stacks, rebuild the legacy projection once, `++Revision` once, broadcast once. | No intermediate state is visible; failures before commit change nothing. |
| Preflight re-run | The commit always plans from the live contents inside the guard. It never trusts an earlier C2 query. | Queries are advisory. |
| Exact-class semantics | Kept: removal, totals and stacking match the exact definition class. | Same as current stacking; changing it is a separate decision. |
| Crafting helper | `FCraftingTransaction::TryCraftRecipeAtomic(Inventory, Recipe)` (native C++, not Blueprint) validates the recipe, converts ingredients/outputs to requests and calls the primitive. Returns `FCraftingTransactionResult` (reason, recipe entry index, inventory result, diagnostic). | Smallest crafting-side executor; the C5 `UCraftingComponent` will own it and expose crafting to Blueprints. |
| Query relationship | `CheckRecipeIngredients` / `CheckRecipeOutputCapacity` stay advisory UI helpers. `TryCraftRecipeAtomic` is authoritative. | C2 output check is conservative; the transaction is exact. |

### Out of scope

`UCraftingComponent` (C5), crafting UI (C6), timed crafting (C7), workstations (C8).

## C5 - Crafting Authority

Baseline: `e042232` (C3/C4 approved and committed). Full automation 138/138. Approved by the user (all five design decisions without changes) and committed.

### Owner and API

- Owner: `BP_TopDownCharacter`, next to its existing inventory (`InventoryManagerComponent`, a `UInventoryComponent` subclass) and `UToolLoadoutComponent`.
- Inventory resolution: `GetOwner()->FindComponentByClass<UInventoryComponent>()`, the same pattern as `UToolLoadoutComponent`. The component stores no inventory contents and no transaction state.
- Blueprint API (the only crafting entry points for UI):
  - `GetAvailableRecipes()` (pure): registered recipes in authored order, without null, abstract, invalid or duplicate entries.
  - `CanCraftRecipe(RecipeClass)` (pure): exact read-only preflight, `FCraftingTransactionResult`.
  - `TryCraftRecipe(RecipeClass)` (callable): authoritative craft, `FCraftingTransactionResult`.
  - `OnCraftingResult(RecipeClass, Result)` (assignable): one notification per non-reentrant `TryCraftRecipe` call, success or failure.

### Decisions

| Decision | Choice | Reason |
| --- | --- | --- |
| Recipe registration | `AvailableRecipes` (`EditDefaultsOnly`) on the component, configured on `BP_TopDownCharacter` through Unreal MCP with `Recipe_Axe`, `Recipe_WoodArrow`. Authored order is the listing order; the first copy of a duplicate wins; invalid entries are skipped and reported by `IsDataValid`. | Data-driven, no hard-coded asset paths in C++. |
| Rejection order | Null/abstract or invalid recipe -> `InvalidRecipe`; valid but not registered -> `RecipeNotAvailable`; missing or inactive inventory -> `InvalidInventory`; then the transaction result. | Matches the C5 request. |
| Result type | Reuse `FCraftingTransactionResult`; append `RecipeNotAvailable` and `CraftInProgress` to `ECraftingTransactionResult`. | One result type for UI from preflight, commit and notifications. |
| Exact preflight | New read-only `UInventoryComponent::CanApplyDefinitionTransaction` (native) runs the same `PreflightTransaction` as the commit; `FCraftingTransaction::CanCraftRecipe` builds requests and maps results with the same code as `TryCraftRecipeAtomic`. Like other queries it does not report `Busy`. | No second stacking or planning algorithm; overlap (Wood 50/50, x10 -> x5) is exact. |
| Reentrancy | The component holds a craft-in-progress guard for the whole `TryCraftRecipe`, including its notification. A reentrant call (from `OnInventoryChanged` during the commit, or from an `OnCraftingResult` handler) returns `CraftInProgress` without mutating or notifying. A craft attempted during an unrelated inventory mutation is a real attempt: it returns `InventoryBusy` and notifies once. | No duplicate or nested crafting notifications. |
| Generic primitives | `TryApplyDefinitionTransaction`, `CanApplyDefinitionTransaction` and `FCraftingTransaction` stay native only. | Blueprints go through the crafting component. |

### Out of scope

Crafting UI (C6), hotkeys, timed crafting (C7), workstations (C8), recipe unlock progression.

## C6A - Crafting UI Foundation

Baseline: `0851c96` (C5 approved and committed). Full automation 147/147. Approved by the user (manual rendered PIE 12/12 PASS, user-confirmed) and committed. C6B is next.

### Agreed Layout (user decisions)

- Crafting is shown together with the inventory in one player screen, opened and closed with the inventory keys.
- `W_Inventory` becomes three columns (window about 1680 x 760):
  - Left: BACKPACK grid and ITEM DETAILS (existing widgets, including the Equip Tool action).
  - Middle: CHARACTER: the existing `EquipmentPanel` (Head/Body/MainHand/OffHand placeholders and the Axe/Pickaxe tool slots). No stats: the game has no stats system yet.
  - Right: CRAFTING: an embedded `W_CraftingPanel`, a self-contained widget that can later be reused (for example by workstations).
- Crafting panel content: registered recipe list with icon and name, recipe selection, selected recipe icon/name/description, ingredients as owned / required (missing ones highlighted), outputs as xN, the exact `CanCraftRecipe` status text, and a CRAFT button that is enabled only when `CanCraftRecipe` succeeds.
- Input: `I` keeps opening and closing the screen; **`Tab` is added** (approved by the user) to open and close the same screen. `Esc` still closes. This replaces the master-plan rule "Tab does not open inventory".

### Decisions

| Decision | Choice | Reason |
| --- | --- | --- |
| Authority | The panel reads only `UCraftingComponent` (`GetAvailableRecipes`, `CanCraftRecipe`) and the inventory (`GetTotalQuantity`, `OnInventoryChanged`) of the owning pawn. It keeps no inventory or crafting state besides the selected recipe class. | Single crafting authority. |
| C6A scope | Read-only binding. The CRAFT button's enabled state follows `CanCraftRecipe`, but clicking it does nothing until C6B wires `TryCraftRecipe`. | User scope for C6A. |
| Code shape | Same pattern as the inventory UI: C++ base widgets (`UCraftingPanelWidget`, `UCraftingRecipeEntryWidget`, `UCraftingRequirementRowWidget`) with `BindWidgetOptional` members; UMG assets only lay out. A pure view builder (`UCraftingViewLibrary::BuildRecipeView`) turns a recipe and the component into display data, so the logic is testable without widgets. | Matches existing architecture; testable. |
| Inventory screen | All widget names bound by `UInventoryScreenWidget` stay unchanged. The only C++ change there is closing on `Tab` as well as `I`/`Esc`. | No regression for inventory, drag/drop and tools. |
| Tab binding | Add a `Tab` key mapping for the existing `IA_OpeningInventory` action in `IMC_Default` through Unreal MCP. The controller logic is not changed. | Smallest change that reuses the existing open path. |

### Asset scope

- New: `/Game/LevelPrototyping/CraftingSystem/UI/W_CraftingPanel`, `W_CraftingRecipeEntry`, `W_CraftingRequirementRow`.
- Modified: `/Game/LevelPrototyping/InventorySystem/UI/Widgets/W_Inventory` (layout, embedded crafting panel), `/Game/TopDown/Input/IMC_Default` (Tab mapping).
- Not modified: `BP_TopDownController`, `BP_TopDownCharacter`, maps, World Partition actors, item and recipe assets.

### Out of scope

Executing crafts from the UI (C6B), crafting feedback/toasts (C6B), timed crafting (C7), workstations (C8), character stats.

## C6B - Craft Execution from UI

Baseline: `d831079` (C6A approved and committed; manual PIE 12/12 user-confirmed). Full automation 153/153. Manual rendered PIE 12/12 PASS (user-confirmed); committed.

### Scope

- Wire the existing CRAFT button to `UCraftingComponent::TryCraftRecipe` for the selected recipe. Layout, input and assets stay as in C6A.
- Show one concise result message in the existing `CraftStatusText`, coloured for success or failure.

### Decisions

| Decision | Choice | Reason |
| --- | --- | --- |
| Asset scope | Source and tests only. `W_CraftingPanel` already has `CraftButton`; the click is bound in C++ (`NativeOnInitialized`, once per widget). | Smallest change; no Content edit expected. |
| Click path | `CraftButton.OnClicked` -> `UCraftingPanelWidget::RequestCraftSelectedRecipe()` -> exactly one `UCraftingComponent::TryCraftRecipe(SelectedRecipe)`. The panel never touches `UInventoryComponent` or the transaction. | Single crafting authority. |
| Feedback path | Craft feedback comes only from `OnCraftingResult`; the return value of `TryCraftRecipe` is not used for feedback. Local feedback is shown only when no call is made (no recipe selected, or no crafting component). | No duplicate messages. |
| Messages | Built from structured result codes by `UCraftingViewLibrary::GetCraftFeedbackText`: success "Crafted Axe x1." (all outputs listed); missing "Not enough Wood (2 / 3)."; capacity "Not enough inventory space."; invalid/unavailable recipe "Recipe unavailable."; busy "Inventory is busy."; no inventory "No inventory available."; other "Crafting failed." | Readable, no diagnostic parsing. |
| Feedback lifetime | The message stays for `FeedbackDisplaySeconds` (default 3 s, real time) even when inventory refreshes underneath; it clears early when another recipe is selected or a new craft is requested. | The synchronous refresh must not overwrite the result with "Ready to craft." |
| Duplicate submissions | A panel-level request guard ignores reentrant requests (for example from an inventory or crafting event handler during the craft), and the button is disabled while a request runs. Each separate click is one craft attempt. | No double crafts from one click. |
| Panel source | New `SetCraftingSource(UCraftingComponent*)`: binds the panel to a specific crafting component (null returns to following the owning pawn). Used by tests now and by workstation panels later. | Testable without a player controller; reusable. |
| Selection | The selected recipe stays selected after a craft while it is still registered. | Requested behaviour. |

### Out of scope

Timed crafting (C7), workstations (C8), new hotkeys, layout changes, split stacks.

## C7 - Timed Crafting

Baseline: `94dceff` (C6B complete; manual PIE 12/12 user-confirmed). Full automation 159/159. Design and implementation approved by the user (manual rendered PIE: 12 PASS, 1 NOT TESTED - click-to-move, deferred) and committed.

### User Decisions

- **Ingredients are consumed at completion**, never reserved at start. Start requires the exact read-only preflight (`CanCraftRecipe`). Completion re-validates and runs the existing C3/C4 atomic transaction (`FCraftingTransaction::TryCraftRecipeAtomic`). If the inventory changed and completion cannot succeed, it fails atomically: no partial consumption, no output.
- **Cancellation**: explicit Cancel, movement and jump cancel. Closing the inventory UI, storage interaction and inventory quantity changes do not cancel. Owner/component `EndPlay` clears the timer and state. Damage is deferred until combat exists.
- **Concurrency**: one active craft per player; a second start returns `CraftInProgress`; no queue.
- **Compatibility**: duration-0 recipes stay instant; timed recipes cannot bypass the timer through `TryCraftRecipe`; `UCraftingComponent` stays the only crafting authority; successful completion keeps one revision and one inventory event.

### Audit

- **Crafting API (C5/C6B)**: `UCraftingComponent::TryCraftRecipe` validates (recipe, registration, inventory) and runs one atomic transaction; a craft guard makes reentrant calls return `CraftInProgress`; `OnCraftingResult` fires once per non-reentrant call. `CanCraftRecipe` is the exact preflight. The C6B panel calls `TryCraftRecipe` from CRAFT and shows feedback from `OnCraftingResult`. `CraftDuration` already exists on recipes; both production recipes are 0.
- **Gathering cancel**: `AResourceNodeActor` keeps a static interactor -> node map. `BP_TopDownCharacter`'s IA_Move and IA_Jump graph paths call `CancelActiveGatherForInteractor` before their normal behaviour. That is input-intent based: it only sees those two actions (not the controller's click-to-move path) and lives in a Blueprint graph.
- **UI input flow**: the inventory screen sets UI-only input while open, so movement/jump input cannot reach the pawn while it is open; closing it restores game input. Crafting must keep running after the screen closes.
- **Test fixtures**: `UCraftingTestRecipeDefinition` has `CraftDuration = 1.5` and is used through `UCraftingComponent::TryCraftRecipe` by the C5, C6A and C6B tests. Under C7 rules those calls would be refused, so the fixture must become instant (see Test impact).

### Movement and Jump Detection (recommended)

Detect movement natively in `UCraftingComponent`, from the owner's character movement, instead of adding more calls to the IA_Move/IA_Jump Blueprint paths:

- While a timed craft is active and the owner is an `ACharacter`, bind `OnCharacterMovementUpdated` and `MovementModeChangedDelegate`; unbind when the craft ends.
- **Movement**: cancel when the owner's horizontal distance from the start location exceeds a small tolerance (`MovementCancelTolerance`, default 10 cm). This catches WASD, click-to-move and being pushed, and ignores tiny jitter.
- **Jump**: cancel when the movement mode becomes `Falling` (jumping or walking off a ledge).
- Owners that are not characters get no automatic movement cancel (explicit Cancel and `EndPlay` still work).

Why this is safest: no change to `BP_TopDownCharacter`'s graph, `AResourceNodeActor`, the gathering static map or input assets, so gathering behaviour cannot regress. Both systems cancel independently on the same physical movement. Trade-off: any displacement cancels (also external pushes), which matches "movement cancels".

### State, API and Events (proposed)

State on `UCraftingComponent` (no tick; one world timer):

- `ActiveRecipe` (class), `ActiveCraftStartLocation`, `CraftTimerHandle`. `IsCrafting()` is true while a timed craft runs.

Blueprint API:

| API | Behaviour |
| --- | --- |
| `TryStartCraft(RecipeClass)` (callable, new; UI entry point) | Validates like `TryCraftRecipe` and runs the exact preflight. Duration 0: crafts immediately (same as `TryCraftRecipe`, result `Success`). Duration > 0: starts the timer and returns `Started`; ingredients untouched. While a craft is active: `CraftInProgress`. |
| `TryCraftRecipe(RecipeClass)` (existing) | Unchanged for duration-0 recipes. Timed recipes return `TimedCraftRequired` without mutation, so the timer cannot be bypassed. While a timed craft is active: `CraftInProgress`. |
| `CancelCraft()` (callable, new) | Cancels the active timed craft (reason `Explicit`); no inventory change. |
| `CanCraftRecipe(RecipeClass)` (existing) | Same exact preflight; additionally returns `CraftInProgress` while a timed craft is active (a persistent state, unlike a transient busy). |
| `IsCrafting()`, `GetActiveCraftRecipe()`, `GetActiveCraftProgress()` (0..1), `GetActiveCraftRemainingSeconds()` (pure, new) | Read-only progress for UI; derived from the world timer, no stored copies. |

Result codes appended to `ECraftingTransactionResult`: `Started`, `TimedCraftRequired`.

Events:

- `OnCraftStarted(RecipeClass, Duration)` (new).
- `OnCraftCancelled(RecipeClass, ECraftCancelReason)` (new; reasons `Explicit`, `Movement`, `Jump`). Not broadcast on `EndPlay`.
- `OnCraftingResult(RecipeClass, Result)` (existing): instant crafts as today; timed crafts **once at completion** with the transaction result (success, or the atomic failure reason such as `InsufficientIngredients`/`OutputCapacityFull` when the inventory changed). A refused start (failed preflight, `CraftInProgress`) also notifies once, as in C5.

Completion order: the timer fires -> active state is cleared and movement unbound -> inside the craft guard, re-validate (recipe still registered, inventory present) and run `FCraftingTransaction::TryCraftRecipeAtomic` -> broadcast `OnCraftingResult`. Listeners therefore see `IsCrafting() == false` and may start the next craft.

`EndPlay`: clear the timer, unbind movement, clear state; no inventory change, no broadcast.

### UI (C6B panel, minimal)

- CRAFT calls `TryStartCraft`. While crafting, the same button reads **CANCEL** and calls `CancelCraft`; a progress bar under the status line shows `GetActiveCraftProgress()`; the status reads "Crafting Axe... 1.4 s".
- Feedback: start refusals and completion results reuse the C6B messages; cancel shows "Crafting cancelled." (or "Crafting cancelled: you moved."). Completion while the screen is closed has no HUD toast in C7 (deferred).
- Panel reacts to `OnCraftStarted`/`OnCraftCancelled`/`OnCraftingResult`; progress is read in the panel's existing `NativeTick`.

### Proposed Scope

- Source: `Crafting/CraftingComponent.h/.cpp` (state, timer, events, movement binding, `TryStartCraft`, `CancelCraft`, `EndPlay`, progress queries), `Crafting/CraftingTransaction.h` (two result codes, cancel-reason enum), `UI/CraftingViewLibrary` (status/feedback text for started/cancelled/progress), `UI/CraftingPanelWidget` (start/cancel button mode, progress, new event bindings), tests.
- Assets (through Unreal MCP):
  - `W_CraftingPanel`: add one `ProgressBar` (`CraftProgressBar`); bind the existing `CraftButtonText` for the CRAFT/CANCEL label.
  - One production timed recipe for PIE (decision needed, see below).
- Not touched: `BP_TopDownCharacter` graph, `BP_TopDownController`, `IMC_Default`, `AResourceNodeActor`, maps.

### Test Impact

- Make `UCraftingTestRecipeDefinition` instant (`CraftDuration = 0`) so the existing C5/C6 tests keep testing instant crafting; update the C1 `RecipeContract` expectation; add separate timed fixtures for C7.
- New `NullTide.Crafting.C7.*`: start/complete, consume-at-completion, inventory changed during craft (atomic failure), explicit/movement/jump cancel, closing UI not cancelling, `CraftInProgress` for a second start, `TimedCraftRequired` from `TryCraftRecipe`, instant recipes unchanged, `EndPlay` cleanup, one revision/event on completion, panel start/cancel/progress. Movement and jump tests use a character in a transient world (`SetActorLocation` + movement update, `SetMovementMode(MOVE_Falling)`).

### Approved Decisions (user, all four approved)

1. **Production timed recipe**: new `Recipe_Pickaxe` (Wood x2 + Stone x3 -> Pickaxe x1, `CraftDuration = 3.0`), registered third on `BP_TopDownCharacter`. `Recipe_Axe` and `Recipe_WoodArrow` stay instant.
2. **API shape**: `TryStartCraft` is the unified UI entry point; `TryCraftRecipe` stays instant-only and returns `TimedCraftRequired` for timed recipes.
3. **`CanCraftRecipe`** returns `CraftInProgress` while a timed craft is active.
4. **Movement detection**: native, through the owner's character movement (`OnCharacterMovementUpdated` displacement beyond 10 cm -> `Movement`; movement mode `Falling` -> `Jump`). No Blueprint movement/jump graph, controller, input or gathering change.

Re-entrancy (user requirement): on completion the active timed state, timer and movement bindings are cleared before feedback; the transaction and `OnCraftingResult` run inside the existing craft guard, so a synchronous callback gets `CraftInProgress` without an extra notification; a later independent action can start a new craft normally.

### Out of scope

Crafting queue, multiple simultaneous crafts, workstations (C8), HUD toast while the screen is closed, damage cancel, save/load of an active craft.

## C8 - Workstations (COMPLETE / APPROVED: C8A, C8B, C8C)

Baseline: `0cf655f` (C7 complete and approved; pushed). Full automation 169/169. The user approved all nine design decisions below without changes, with `Recipe_ShortSword` as the production Workbench recipe. C8 is implemented one checkpoint at a time: C8A first, then C8B and C8C only after review.

### Goal

- Hand crafting keeps working with no station.
- A Workbench unlocks recipes that need it.
- Campfire and Forge can be added later as data and content, without new crafting logic.
- The player uses a station with `F`.
- These rules stay as they are: one active craft per player, consume at completion, timed crafting and cancellation.
- Stations never become inventory or crafting authorities.

### Audit

| Area | What exists | Reuse / conflict for C8 |
| --- | --- | --- |
| `UCraftingComponent` | The player's authority. `AvailableRecipes`, `ValidateRequest` (recipe -> registration -> inventory), `CanCraftRecipe`, `TryCraftRecipe` (instant only), `TryStartCraft`, one world timer, `ActiveRecipe`, a craft guard. Movement and jump cancel through `ACharacter` delegates that are bound only while crafting. Completion clears state, then re-validates, then runs one atomic transaction. | **Reuse.** `ValidateRequest` is the single place to add a station check. The C7 completion re-validation already exists, so the station check joins it. Movement cancel (more than 10 cm) already covers "the player walks away" during a craft. |
| `UCraftingRecipeDefinition` | Abstract, const, data-only BP subclasses. `ValidateRecipe` and `IsDataValid`. C1 deliberately left out a workstation field. | **Extend** with one optional field. A null value means hand crafting, so `Recipe_Axe`, `Recipe_WoodArrow` and `Recipe_Pickaxe` stay unchanged. |
| `FCraftingTransaction` / inventory | `TryCraftRecipeAtomic`, `CanCraftRecipe`, and the generic `TryApplyDefinitionTransaction`. | **Unchanged.** The station check runs before the transaction, in the crafting component. The inventory never learns about stations. |
| `BPI_Interactable` / `PlayerInteractionComponent` | `F` (`IA_Interact`). Capsule overlap finds actors that implement `BPI_Interactable`. `GetActiveInteractable` returns the last actor that was overlapped. `Interact(Interactor)` uses Press/Hold through `E_InteractionType`. | **Reuse unchanged.** The workbench implements the interface the same way `BP_StorageContainer` does. Known limitation: when several interactables overlap, the last one overlapped wins. |
| `AStorageContainerActor` | C++ actor: mesh, `InteractionBounds` and one authority component. `OpenStorageForInteractor` resolves the PlayerController, creates and caches its screen widget, sets UI-only input and shows the cursor. The screen closes itself when its source becomes invalid (`NativeTick`). It has no range check. | **Pattern to copy** for `ACraftingStationActor`. Unlike storage, crafting at a station adds a range check in the authority. |
| `UCraftingPanelWidget` | Follows the owning pawn's crafting component. `SetCraftingSource(UCraftingComponent*)` swaps which component it observes. It lists `GetAvailableRecipes()`. `BuildRecipeView` uses `CanCraftRecipe`. On reopen it selects the active recipe if that recipe is listed. | **Reuse.** C6B planned `SetCraftingSource` for "a workstation's" component, but under this design stations own no crafting component. The panel instead gets a separate *station context*: it keeps observing the player's component and passes the station along with each query and request. `SetCraftingSource` stays as it is, for tests. |
| `W_Inventory` / input | `BP_TopDownController` creates `W_Inventory` on `I`/`Tab`, sets UI-only input and shows the cursor. `UInventoryScreenWidget` closes on `I`/`Tab`/`Esc`. While any screen is open, game input (move, jump, `F`) does not reach the pawn. | **Reuse.** The player cannot move while a station screen is open. After it closes, any movement cancels the craft (C7). |

Conflicts found:

1. The C6B comment and the master plan assumed stations might own a crafting component. That would create a second crafting authority and a second "one active craft" state. **Rejected.** Stations are context only.
2. `GetAvailableRecipes()` is used by the panel and by C5 tests as "everything registered". Changing its meaning would break callers. It stays "all registered". A new context-filtered query is added instead.
3. `CanCraftRecipe` / `TryStartCraft` / `TryCraftRecipe` have no station parameter. They become the **hand context**. Station recipes called through them return a new `StationRequired` code.

### Recommended Workstation Model

**Requirement representation: a station definition class (recommended).**

| Option | Pros | Cons |
| --- | --- | --- |
| C++ enum `ECraftingStationType` | Smallest. Blueprint-friendly. Trivial matching. | A C++ change and recompile for every new station. No place for a display name, icon or range. Enum renames break saved data. |
| Gameplay tag `Crafting.Station.Workbench` | Data-driven. Hierarchical matching is possible later. | The project does not use GameplayTags yet, so it would need a new module dependency and an ini tag table. A display name and icon still need a separate lookup. Tags are easy to mistype. |
| **`UCraftingStationDefinition` class** (abstract, const, data-only BP subclasses, referenced by `TSubclassOf`, read from the CDO) | Same pattern as `UItemDefinition` and `UCraftingRecipeDefinition`. Adding Campfire or Forge is a new asset with no C++. It carries a display name, icon and use range. `IsDataValid` works. No new module. | One more asset type. Matching is by class identity, so renaming an asset needs a redirect, as for items. |

New types (all in `Crafting/`):

- `UCraftingStationDefinition`: `StationName`, `StationDescription`, `StationIcon`, `UseRange` (cm, default 300, must be greater than 0), and `IsDataValid`.
- `UCraftingRecipeDefinition::RequiredStation` (`TSubclassOf<UCraftingStationDefinition>`):
  - null means a hand recipe;
  - `ValidateRecipe` rejects an abstract station class.
- `UCraftingStationComponent` (actor component, no tick, no state other than data):
  - `StationDefinition`;
  - `GetStationDefinition()`;
  - `IsActorInUseRange(const AActor*)`: 2D distance between actor locations is at most `UseRange`.

  Being a component lets a later Building-phase actor (a placed Campfire with light and fuel) become a station by adding the component.
- `ACraftingStationActor` (C8B): the same shape as `AStorageContainerActor`, with a mesh, `InteractionBounds`, a `UCraftingStationComponent`, a `CraftingScreenClass` and `OpenCraftingForInteractor(Interactor)`.

**Matching rule:**
- A recipe can be crafted in a context when `RequiredStation` is null (a hand recipe, allowed anywhere, including at a station).
- Otherwise the context's station definition must be **exactly** `RequiredStation`. Exact class only: no `IsChildOf` tiers in C8.

**Recipe registration (recommended):**
- All recipes, including Workbench recipes, stay registered on the player's `UCraftingComponent::AvailableRecipes`.
- The station decides *where* a recipe can be crafted. The player's registry decides *which* recipes the player knows.
- Stations hold no recipe lists, so later unlock or progression work has one registry to extend.
- Alternative: station-owned recipe lists. Rejected because they duplicate registration and make the player authority accept recipes it does not own.

**Future Campfire / Forge:**
- Each new station is a `Station_Campfire` definition asset, a BP subclass of `ACraftingStationActor` (or any actor with `UCraftingStationComponent`), and recipes that set `RequiredStation`.
- No crafting code changes.
- Later per-station rules (fuel, "fire is lit", a craft-speed multiplier) belong in one `ValidateStationAccess` hook. Deferred.

### Workstation Authority (UCraftingComponent stays the authority)

Recommended API: the station is passed explicitly with each request. The component keeps no "current station" between requests, so closing the UI never leaves stale state.

| API | Behaviour |
| --- | --- |
| `GetRecipesForStation(UCraftingStationComponent* Station)` (pure, new) | Registered valid recipes usable in this context, in authored order. Null station: hand recipes only. Valid station: the recipes for that station, then hand recipes. |
| `CanCraftRecipeAtStation(Recipe, Station)` (pure, new) | `CanCraftRecipe` plus the station check. Exact read-only preflight. |
| `TryStartCraftAtStation(Recipe, Station)` (callable, new) | `TryStartCraft` plus the station check. Same instant / timed / notification behaviour. A timed station craft records `ActiveCraftStation` (a weak pointer). |
| `CanCraftRecipe` / `TryStartCraft` / `TryCraftRecipe` (existing) | Unchanged signatures. Act as the hand context (station = null). A station recipe returns `StationRequired` without mutation. |
| `GetActiveCraftStation()` (pure, new) | The station the active craft was started at, or null. |

Validation order (`ValidateRequest` with a station):
1. Recipe valid -> `InvalidRecipe`.
2. Registered -> `RecipeNotAvailable`.
3. Station: if the recipe needs one and the context has none or the wrong definition -> `StationRequired`; if out of range -> `StationOutOfRange`. Hand recipes skip the station check.
4. Inventory -> `InvalidInventory`.
5. Active craft -> `CraftInProgress`.
6. Exact transaction preflight.

Result codes are **appended** to `ECraftingTransactionResult`: `StationRequired`, `StationOutOfRange`. The cancel reason `StationLost` is appended to `ECraftCancelReason`. Appending keeps existing Blueprint enum values.

| Question | Proposed behaviour |
| --- | --- |
| How the active workstation is tracked | Only while a craft runs: `ActiveCraftStation` (a weak pointer captured at start), cleared by `ClearActiveCraft`. The "station being used" for listing is UI context in the panel, not authority state. |
| How requests validate access | Every station query and request runs the station check inside `ValidateRequest`. Timed completion re-runs the same `ValidateRequest` with `ActiveCraftStation`, so a lost or out-of-range station fails atomically with the same codes. |
| Interaction distance | `UseRange` on the station definition (default 300 cm, 2D, actor origin to actor origin). This covers the 130 cm `InteractionBounds` extent plus the capsule radius with some slack. Checked at query, start and completion. No polling. |
| Player moves away | Already handled by the C7 movement cancel (more than 10 cm -> `Movement`) before any range limit is reached. While the station screen is open, input is UI-only, so the player cannot walk away. No new range-based cancel. |
| Station destroyed | While a *station* recipe is crafting, the component binds the station actor's `OnEndPlay`. On that event the craft is cancelled immediately with `StationLost` (no inventory change; `OnCraftCancelled` fires). Completion re-validation is the backstop. Hand recipes started from a station screen do not depend on the station and are not cancelled. |
| UI closes | Does not cancel (same as C7). The craft keeps running on the player's component and completes against the captured station. |
| Station crafting while another craft is active | Not allowed. One active craft per player across all contexts (`CraftInProgress`). Stations have no occupancy lock in C8: two users could use one bench, but only the player uses stations today. |

### UI Integration

Recommended: **reuse `W_Inventory`.** The station opens the same three-column screen (inventory, character, crafting), with the crafting column in station context.

- **F opens it.** `BP_Workbench` implements `BPI_Interactable`, and `Interact` calls `ACraftingStationActor::OpenCraftingForInteractor(Interactor)`. That function copies `OpenStorageForInteractor`:
  - resolve the PlayerController;
  - reject the request when the player is out of `UseRange`;
  - create and cache its own `CraftingScreenClass` instance (`W_Inventory`);
  - call `OpenForCraftingStation(StationComponent)`;
  - set UI-only input, show the cursor and set keyboard focus.
- **Filtering.**
  - `UInventoryScreenWidget` gains an optional `CraftingPanel` binding (the existing widget name in `W_Inventory`) and `OpenForCraftingStation(Station)`, which forwards to the new `UCraftingPanelWidget::SetCraftingStation(Station)`.
  - The panel lists `GetRecipesForStation(Station)`, builds views with `CanCraftRecipeAtStation`, and CRAFT calls `TryStartCraftAtStation`.
  - An optional `CraftingTitleText` shows "CRAFTING" by hand or the station name ("WORKBENCH").
  - The `I`/`Tab` screen keeps a null station, so it shows hand recipes only.
- **The inventory stays available.**
  - The station screen *is* the inventory screen, so backpack, details, equip and drag/drop all work.
  - `I`/`Tab`/`Esc` close it, as today.
  - `I`/`Tab` from game input still opens the controller's own hand-context screen. No change to `BP_TopDownController`.
- **Progress and cancel.**
  - These read the player's component (`IsCrafting`, `GetActiveCraftProgress`, `CancelCraft`), so they work the same in both screens.
  - If the hand screen is opened while a station craft runs, the active recipe is not in the hand list. The panel still shows the progress bar, status and CANCEL for the active craft. C8C must test and, if needed, fix this, because the C7 code selected the active recipe only when it is listed.
- **Station disappears while its screen is open.** The screen closes, as the storage screen does for invalid storage.
- **On close.** The station context on the panel is cleared, so the reused widget instance never keeps a stale station.
- **Separation.**
  - The panel stores only a weak station pointer as context and passes it to the component.
  - Range, definition matching and inventory are validated only in `UCraftingComponent`.
  - The station actor only opens the screen and answers range and definition queries.

Alternative: a dedicated small `W_CraftingStationScreen` containing only `W_CraftingPanel`. It is more isolated, but adds a new layout, hides the backpack while at the bench, and does not reuse the inventory screen. Not recommended.

### Checkpoints

**C8A - Workstation data model and authority validation** (source, tests and data assets; no map change)
- Source:
  - New `Crafting/CraftingStationDefinition.h/.cpp` and `Crafting/CraftingStationComponent.h/.cpp`.
  - `CraftingRecipeDefinition` (`RequiredStation` and its validation rule).
  - `CraftingTransaction.h` (two result codes).
  - `CraftingComponent` (cancel reason, station-aware `ValidateRequest`, `GetRecipesForStation`, `CanCraftRecipeAtStation`, `TryStartCraftAtStation`, `GetActiveCraftStation`, the station `OnEndPlay` binding, completion re-validation).
  - Tests and fixtures in `CraftingTestTypes.h`: a test station definition, a station recipe, and a helper actor with a station component.
- Assets (through MCP):
  - `/Game/LevelPrototyping/CraftingSystem/Stations/Station_Workbench`.
  - The Workbench production recipe (see decision 8), **not registered yet**, so the C6/C7 screens do not change in C8A.
- Tests `NullTide.Crafting.C8A.*`:
  - `StationDefinitionContract`
  - `RecipeStationValidation` (null = hand; abstract station rejected)
  - `RecipeFiltering` (null -> hand only; station -> station plus hand recipes; mismatched station excluded; `GetAvailableRecipes` unchanged)
  - `HandContextRejectsStationRecipe` (`StationRequired` from all three existing APIs; no mutation; one notification)
  - `StationMismatchAndRange`
  - `StationInstantAndTimedCraft` (consume at completion; one revision and one event)
  - `StationLostCancels` (and a hand recipe started at a station is not cancelled)
  - `CompletionRevalidatesStation` (station moved out of range -> atomic `StationOutOfRange`)
  - `OneActiveCraftAcrossContexts`
  - `MovementCancelAtStation`
  - `ProductionStationAssets`
- Regression: C1-C7. `RecipeContract` gets the `RequiredStation` default.

**C8B - Workbench actor and interaction** (source and assets; one new placed actor)
- Source: new `Crafting/CraftingStationActor.h/.cpp`. `OpenCraftingForInteractor` validates the interactor and range and returns false while `CraftingScreenClass` is unset. The screen is wired in C8C.
- Assets:
  - `/Game/LevelPrototyping/CraftingSystem/Stations/BP_Workbench` (parent `ACraftingStationActor`, `StationDefinition = Station_Workbench`, implements `BPI_Interactable` with Press, `Interact` -> `OpenCraftingForInteractor`, placeholder mesh like the storage cube).
  - One `BP_Workbench` placed in `Lvl_TopDown` near the spawn. This is one new World Partition external actor package, saved by the user in the editor.
- Tests `NullTide.Crafting.C8B.*`:
  - `ActorContract`
  - `OpenRejectsInvalidInteractor` (null, no controller, out of range, no screen class)
  - `ProductionWorkbench` (asset loads, definition set, implements `BPI_Interactable`)
- PIE (user):
  - the `F` prompt appears at the bench;
  - storage, door and gathering still work;
  - the bench does not block click-to-move pathing badly.
- Cook.

**C8C - Workstation crafting UI and PIE**
- Source:
  - `UCraftingPanelWidget` (`SetCraftingStation`, filtered list, station-aware view and request, optional title, active-craft display when the active recipe is not listed).
  - `UCraftingViewLibrary` (`BuildRecipeView` station overload; texts "Requires Workbench.", "Too far from the Workbench.", "Crafting cancelled: the Workbench is gone.").
  - `UInventoryScreenWidget` (optional `CraftingPanel` binding, `OpenForCraftingStation`, context cleared on close, close when the station is lost).
  - `ACraftingStationActor` (opens the screen).
- Assets:
  - `BP_Workbench` (`CraftingScreenClass = W_Inventory`).
  - `BP_TopDownCharacter` (register the Workbench recipe fourth).
  - `W_CraftingPanel` (optional `CraftingTitleText`, only if no suitable named text exists).
- Tests `NullTide.Crafting.C8C.*`:
  - station panel lists station plus hand recipes;
  - hand panel hides the station recipe;
  - CRAFT routes through `TryStartCraftAtStation`;
  - new feedback texts;
  - the screen closes on station loss;
  - the hand screen shows active station-craft progress and CANCEL works;
  - the context is cleared on close.
- C5 registration and the C6A list expectations are updated for the fourth recipe.
- Manual rendered PIE checklist (user-run), about 12 items:
  - `F` opens the workbench screen;
  - only the correct recipes are listed;
  - a timed station craft completes;
  - cancel by button;
  - moving cancels;
  - the craft keeps running after closing the screen;
  - Tab during a station craft;
  - the bench recipe is absent from Tab;
  - hand recipes at the bench;
  - an out-of-range attempt;
  - storage still works;
  - gathering still works.
- Full suite and cook.

Each checkpoint follows the usual rules:
- build with the editor closed;
- focused tests, then regression, then the full suite;
- Blueprint compile with warnings as errors;
- save only intentionally changed assets;
- update `CRAFTING_PROGRESS.md`, `NULLTIDE_MASTER_PLAN.md` and `CLAUDE.md`;
- commit only after user review; no push.

### Approved Decisions (user, all nine approved)

1. **Requirement representation:** a `UCraftingStationDefinition` class (recommended), versus an enum or a gameplay tag.
2. **Registration:** all recipes on the player's `AvailableRecipes`, with stations as context only (recommended), versus station-owned recipe lists.
3. **Hand recipes at a station:** the station screen lists station recipes and then hand recipes (recommended), versus station recipes only.
4. **Matching:** exact station class only in C8 (recommended). Tiers (a Forge also doing Workbench recipes) are deferred.
5. **Range:** `UseRange` on the definition, default 300 cm, 2D, checked at query, start and completion. No range polling, because movement cancel covers walking away.
6. **Station destroyed:** immediate cancel with the new reason `StationLost` (station recipes only), and an open station screen closes.
7. **UI:** reuse `W_Inventory` opened by the station with a station context on the crafting panel (recommended), versus a new dedicated station screen.
8. **Production content:**
   - `Station_Workbench`, plus `BP_Workbench` with a placeholder cube mesh, placed near the spawn.
   - Proposed Workbench recipe: `Recipe_ShortSword` (Wood x2 + Scrap x4 -> ShortSword x1, `CraftDuration = 4.0`). It gives Scrap a use and exercises timed station crafting.
   - Approved: `Recipe_ShortSword`. The `Recipe_WoodBow` alternative was not chosen.
9. **API shape:** an explicit station parameter on new `...AtStation` functions; the existing functions become the hand context (recommended), versus persistent "active station" state set on the component when the screen opens.

### C8A Implementation Notes (additions to the approved design; for review)

C8A is complete and approved (details in `CRAFTING_PROGRESS.md`, C8A). The user accepted all of these implementation choices, which fill gaps the design did not spell out:

1. **Station `EndPlay` reason.** `Destroyed` and `RemovedFromWorld` cancel a dependent craft with `StationLost`. Every other reason (PIE end, quit, level transition, meaning the whole world ends) clears it silently, matching the C7 rule that the crafter's own `EndPlay` sends no notification.
2. **Station component destroyed alone** (its actor still alive) sends no `OnEndPlay`. The craft is not cancelled immediately; completion re-validation fails it atomically with `StationRequired`. The same applies to a station actor that never began play.
3. **Listing ignores range.** `GetRecipesForStation` lists station recipes for a usable station even when the crafter is out of range. `CanCraftRecipeAtStation` reports `StationOutOfRange`, so C8C can show "Too far" instead of hiding the recipe.
4. **Validation order** puts station access after registration and before inventory, so "out of range" is reported before missing ingredients.
5. **`Station_Workbench` has no icon** (no art yet). `StationIcon` stays empty until art exists.

Carried into C8C from the C8A review:

- The station UI must close or become unusable when its station component is invalid, not only when the station actor is destroyed (see note 2).
- The final C8C cook must verify that `Station_Workbench` and `Recipe_ShortSword` are cooked once the recipe is registered.

### C8B Scope (COMPLETE / APPROVED; manual rendered PIE 8 PASS, 1 N/A, user-confirmed)

Baseline: `c867e6d` (C8A approved and committed; not pushed). Full automation 180/180.

- `ACraftingStationActor`, following `AStorageContainerActor`:
  - components: a static mesh, `InteractionBounds` and a `UCraftingStationComponent`;
  - `CraftingScreenClass` for C8C;
  - `OpenCraftingForInteractor(Interactor)`, which validates the interactor, its player controller, the station and `UseRange`, and rejects the request safely while `CraftingScreenClass` is unset.
  - It never touches inventory or crafting state and never creates a `UCraftingComponent`.
- `BP_Workbench` (through MCP): parent `ACraftingStationActor`, `StationDefinition = Station_Workbench`, implements `BPI_Interactable` (Press; `Interact` forwards to `OpenCraftingForInteractor`), placeholder mesh. No crafting component or recipe list.
- Exactly one `BP_Workbench` placed in `Lvl_TopDown` near the spawn, clear of storage, resource nodes, the door and the main path. One new external actor package, saved by the user with Ctrl+S. No existing actor changes.
- A failed `F` interaction opens nothing and does not change input mode, cursor or movement.
- No change to `BP_TopDownController`, `IMC_Default`, gathering, storage code, `W_Inventory`, `W_CraftingPanel`, `UCraftingComponent` or recipe registration.
- Tests `NullTide.Crafting.C8B.*`: `ActorContract`, `OpenRejectsInvalidInteractor`, `ProductionWorkbench`.
- A user-run rendered PIE checklist. C8B has no crafting screen, so `F` on the bench is expected to do nothing visible.

C8B implementation notes (accepted with the C8B approval; details in `CRAFTING_PROGRESS.md`, C8B):

1. **Scene root instead of a mesh root.** Unlike `AStorageContainerActor`, the station actor's root is an unscaled `StationRoot`, with the mesh and interaction bounds attached to it. With a scaled mesh as root, placing the actor resets the mesh scale.
2. **`ECraftingStationOpenResult` and `GetOpenCraftingResult`** expose why a request is rejected. C8C can reuse this, for example to show "Too far".
3. **`OpenCraftingForInteractor` returns false even for a valid request** until C8C wires the station screen.
4. **`BP_Workbench` was built by duplicating `BP_Sword` and reparenting it**, because the MCP toolsets cannot add a Blueprint interface. Everything inherited from `BP_Sword` was removed.
5. **No door is placed in `Lvl_TopDown`**, so the door PIE item is N/A.

### C8C Scope (COMPLETE / APPROVED; manual rendered PIE 13 PASS + 1 N/A, Tab focus retest 6/6 PASS)

Baseline: `49f38ca` (C8B approved and committed; C8A and C8B not pushed). Full automation 183/183.

- **Station opening.** `ACraftingStationActor::OpenCraftingForInteractor` opens its own cached `CraftingScreenClass` instance (`W_Inventory`):
  - it calls `UInventoryScreenWidget::OpenForCraftingStation(Station)` before adding the screen to the viewport;
  - it shows the cursor, sets UI-only input and sets keyboard focus only after that succeeds;
  - a failure removes a screen it just added and leaves input unchanged.
- **Inventory screen.** `UInventoryScreenWidget` binds the existing `CraftingPanel` widget (`BindWidgetOptional`) and forwards the station context to it.
  - The context is cleared on close and destruction.
  - The screen closes itself when its station actor or station component is no longer usable.
  - The controller-owned hand screen never has a station context.
  - Backpack, details, equipment and drag/drop are unchanged.
- **Crafting panel.** `UCraftingPanelWidget::SetCraftingStation(Station)` stores a weak UI context only.
  - Hand context: `GetRecipesForStation(nullptr)` (hand recipes only), `CanCraftRecipe`, `TryStartCraft`.
  - Station context: `GetRecipesForStation(Station)`, `CanCraftRecipeAtStation`, `TryStartCraftAtStation`, title "WORKBENCH" (the station name in upper case).
  - A station context whose station is gone makes the panel unusable: no recipes, CRAFT disabled, CANCEL of an active craft still allowed.
  - Progress and CANCEL always follow the player's active craft, even when its recipe is not listed (a Workbench craft seen from the hand screen). The hand list never includes station recipes.
- **Text.** "Requires Workbench.", "Too far from the Workbench." and "Crafting cancelled: the Workbench is gone." come from the recipe's `RequiredStation` name.
- **Assets (MCP):**
  - `BP_Workbench.CraftingScreenClass = W_Inventory`;
  - `BP_TopDownCharacter` registers `Recipe_ShortSword` fourth;
  - `W_CraftingPanel` unchanged: the panel binds its existing `PanelTitle` heading (text "CRAFTING") instead of adding a `CraftingTitleText`.
- **Not changed:** `IMC_Default`, `BP_TopDownController`, storage, gathering, actor placement.
- **Tests:** `NullTide.Crafting.C8C.*`, with the C5 and C8A production registration expectations updated to four recipes and the C8B "valid request opens nothing" check replaced.

C8C implementation notes (accepted with the C8C approval; details in `CRAFTING_PROGRESS.md`, C8C):

1. **Selection on a context change.** Changing the station context resets the panel selection to the active craft when it is listed, otherwise the first listed recipe. Opening a Workbench therefore selects ShortSword; returning to hand selects Axe.
2. **A lost station context lists nothing.** A station context whose station is gone lists no recipes, shows "Workstation unavailable" and refuses CRAFT. CANCEL of an active craft still works. The screen closes on its next tick anyway.
3. **The actor's real open path is covered by PIE.** Creating, adding to the viewport and switching input mode need a local player and a game viewport, which the transient automation world does not have. Automation covers the gating (`GetOpenCraftingResult`) and the screen and panel behaviour directly.
4. **Title binding.** The title binds the existing `PanelTitle` text block, so no UMG asset change was needed.
5. **Close keys in the preview pass (user PIE finding).** The user's C8C PIE (13 PASS, 1 N/A) found that `Tab` did not close the screen once a child widget had keyboard focus, because Slate consumed it as focus navigation. `UInventoryScreenWidget` now handles `I`/`Tab`/`Esc` in `NativeOnPreviewKeyDown` through the existing `CloseInventory`, scoped to this screen only. The user accepted C8C gameplay on condition that this is fixed before commit.

### Risks and Edge Cases

- **Interaction target selection:** `PlayerInteractionComponent` picks the last overlapped interactable. A bench placed next to storage or a resource node may target the wrong actor. Mitigation: place the bench apart from other interactables. Changing the target selection is out of scope.
- **Two `W_Inventory` instances:** the controller's and the station's. Both use UI-only input, so only one can be focused and open from game input. PIE must confirm that `I`/`Tab`/`Esc` close the station screen and that the controller can reopen its own screen afterwards.
- **Active craft not in the listed recipes:** a station craft viewed from the hand screen (see UI). Needs explicit tests.
- **Station moved or destroyed between start and completion:** covered by the `OnEndPlay` cancel and completion re-validation. Both are atomic, with no partial consumption.
- **Hand recipe started at a station, then the station is destroyed:** not cancelled, by design.
- **Enum appends:** new values only at the end. Existing Blueprint switch nodes keep their pins.
- **World Partition:** the placed bench is a new external actor package. MCP `save_assets` cannot save it, so the user saves it with Ctrl+S in the editor. Never Save All.
- **Navigation:** a blocking mesh changes the nav mesh near the spawn. Place it away from the main paths and check click-to-move in PIE.
- **Test count changes:** registering a fourth recipe changes the C5 and C6A expectations in C8C, intentionally and documented.
- **Multiplayer and NPC colonists:** stations have no occupancy or locking. They are safe for the current single player and revisited in Colony.

### Validation Strategy

- Authority behaviour is proved by automation in transient worlds: a spawned `ACharacter` plus actors with `UCraftingStationComponent`; destroy and move them to cover range and loss.
- UI behaviour is proved by widget tests (real `W_CraftingPanel` / `W_Inventory` in a transient world, as in C6/C7).
- World and interaction behaviour, and the final UX, are proved by user-run rendered PIE checklists in C8B and C8C. Results are recorded exactly as reported (PASS / FAIL / NOT TESTED).
- Every checkpoint: build, focused tests, regression (C1-C7, Stacking, G4-G7, ResourceGathering), full suite, Windows cook, and a content audit (only the planned packages change).

### Deferred (not in C8)

Campfire and Forge content, station tiers, fuel or operational state, per-station craft speed, station occupancy and multi-user locking, NPC station use, placing or building stations (Phase 5), greyed-out "Requires Workbench" entries in the hand screen, crafting queue, HUD progress while screens are closed, save/load of active crafts, and station-owned output storage.

## Later Checkpoints (summary)

- **C8** Workstations: COMPLETE / APPROVED (C8A, C8B, C8C). Phase 4 Crafting is complete; Phase 5 Building is the next planned major phase (not started).
- **Crafting queue (deferred, user request after C7 PIE)**: queue up to 5 crafts per player that run one after another; each queued entry can be cancelled with a button or right-click. Needs its own design: whether ingredients are checked per entry at start or at completion (C7 consumes at completion), what happens to later entries when one fails or is cancelled, and how movement cancellation applies to the queue. Not started; must not be mixed into C8 without approval.
- **Inventory/crafting UI/UX pass (deferred, user request after C7 PIE)**: make the combined screen more convenient. Known items from C7 PIE: keep the last craft result and selected recipe across closing/reopening the screen; remove the occasional quantity flicker after a result message disappears; show crafting progress or completion while the screen is closed (HUD); review layout density and the empty Item Details state. Not started.
