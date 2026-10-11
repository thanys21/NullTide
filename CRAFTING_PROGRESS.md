# Crafting Progress

Plan and design decisions: `CRAFTING_PLAN.md`.

## Status

| Checkpoint | Status |
| --- | --- |
| C1 Recipe Data Model | COMPLETE |
| C2 Inventory Quantity Queries | Next, not started |
| C3-C8 | Not started |

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

## Next Checkpoint

C2 - Inventory Quantity Queries. Do not start it until C1 is reviewed.
