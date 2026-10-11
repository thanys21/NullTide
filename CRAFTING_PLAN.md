# Crafting Plan

Phase 4 of `NULLTIDE_MASTER_PLAN.md`. Crafting is delivered in small checkpoints (C1-C8). Each checkpoint is validated, documented in `CRAFTING_PROGRESS.md` and committed before the next one starts.

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

## Later Checkpoints (summary)

- **C7** Timed crafting; choose reserve-at-start or consume-at-completion before implementing.
- **C8** Workstations.
