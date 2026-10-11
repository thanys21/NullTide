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

## Later Checkpoints (summary)

- **C5** `UCraftingComponent` as the single crafting authority.
- **C6** Minimal crafting UI that only observes authority state.
- **C7** Timed crafting; choose reserve-at-start or consume-at-completion before implementing.
- **C8** Workstations.
