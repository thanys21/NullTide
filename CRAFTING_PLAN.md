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

## Later Checkpoints (summary)

- **C2** `UInventoryComponent` quantity queries: total quantity per definition across stacks, can-consume and can-add-outputs checks.
- **C3/C4** One atomic craft transaction: preflight ingredients and output capacity, consume stacks in inventory order, add outputs through the stacking authority, one coherent revision/event.
- **C5** `UCraftingComponent` as the single crafting authority.
- **C6** Minimal crafting UI that only observes authority state.
- **C7** Timed crafting; choose reserve-at-start or consume-at-completion before implementing.
- **C8** Workstations.
