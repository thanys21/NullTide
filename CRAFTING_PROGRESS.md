# Crafting Progress

Plan and design decisions: `CRAFTING_PLAN.md`.

## Status

| Checkpoint | Status |
| --- | --- |
| C1 Recipe Data Model | COMPLETE |
| C2 Inventory Quantity Queries | COMPLETE |
| C3/C4 Atomic Craft Transaction (one checkpoint) | COMPLETE |
| C5 Crafting Authority | NEXT |
| C6-C8 | Not started |

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

## Next Checkpoint

C5 - Crafting Authority. NEXT; not started.
