# RG4 Progress - Yield / Depletion

## Status: COMPLETE

Started from completed RG3 commit `e009e21`.

## Reused Foundation

RG4 retains the existing `AResourceNodeActor` authority introduced in RG1 and extended by RG3. No second yield system was added.

- `BeginPlay` initializes `RemainingYield` to `max(0, MaxYield)` and enters `Idle` or `Depleted` accordingly.
- A successful inventory award consumes exactly one yield, clamped at zero.
- Cancelled gathers, inventory-award failures, missing-tool rejection, invalid interaction, and repeated interaction while gathering consume no yield.
- Once `RemainingYield` reaches zero, `ClearGathering` enters `Depleted`; later gather requests return false before a timer, progress UI, award, or further yield change.
- The existing production variant defaults were audited without changing them:
  - Tree: Wood, `MaxYield=5`, `GatherDuration=2.0`, Axe.
  - Rock: Stone, `MaxYield=5`, `GatherDuration=2.5`, Pickaxe.
  - Scrap: Scrap, `MaxYield=4`, `GatherDuration=1.5`, None.

## RG4 Delta

- Added read-only `GetMaxYield` and `GetOutputItemDefinition` accessors to support exact production-default validation.
- Added `ShouldShowInteractionPromptFor`. It is presentation-only and returns false solely for a depleted `AResourceNodeActor`.
- Updated only `/Game/LevelPrototyping/interactionSystem/PlayerInteractionComponent`: its existing active-target function now suppresses the F prompt for a depleted resource node. The established empty-array guard and last-added target behavior remain intact for valid candidates.
- Added `ResourceYieldTests.cpp` with five focused RG4 cases:
  - Tree, Rock, and Scrap production defaults.
  - Successful yield consumption, final depletion, later rejection, and no negative yield.
  - Cancellation, inventory rejection, missing-tool rejection, and prompt predicate behavior without yield consumption.
- Renamed one test-local helper in `ToolLoadoutTests.cpp` to resolve a latent unity-build duplicate symbol discovered by a clean full build. This has no gameplay or test-contract behavior change.

## Validation

- Blueprint validation with warnings-as-errors passed for `PlayerInteractionComponent`, `BP_ResourceNode`, `BP_ResourceTree`, `BP_ResourceRock`, and `BP_ResourceScrap`.
- Only `PlayerInteractionComponent` was saved; no Save All operation was used.
- `NullTideEditor Win64 Development` passed via direct UnrealBuildTool invocation, `Result: Succeeded`.
- Focused RG4 automation: `5/5` passed.
- Focused RG1 automation: `5/5` passed.
- Focused RG3 automation: `3/3` passed.
- Inventory M1 regression: `9/9` passed.
- Full permanent automation: `109/109` passed, zero failures and skips. This is the prior `104` permanent tests plus five RG4 cases.
- Rendered PIE acceptance passed by manual observation:
  - Tree, Rock, and Scrap each deplete after their configured successful gathers and suppress their prompt afterward.
  - Movement and Jump cancel safely without consuming yield.
  - Missing-tool rejection is safe.
  - RG3 tool loadout, G6 drop-to-world, and G7 storage regressions passed.
  - No visible gameplay warning, error, or crash was observed.
- Final Windows cook reached `LogCook: Display: Done!` using `UE-LocalDataCachePath=C:\Users\loithienly\AppData\Local\UnrealEngine\Common\DerivedDataCache`.
  - The commandlet returned nonzero solely because its MCP HTTP listener could not bind `127.0.0.1:8000` while the healthy editor already owned that port.
  - The unrelated D: NTFS journal notice remained informational; it did not prevent cooking.

## Content Audit

Compared with the clean RG3 checkpoint `e009e21`:

- Baseline Content packages: `301`.
- Current Content packages: `303`.
- Added: two approved user-authored external actors, neither attributable to RG4:
  - `Content/__ExternalActors__/TopDown/Lvl_TopDown/1/7G/SV9CRK4S4Y1RR3KNHL61B1.uasset` (`G3_Pickup_ShortSword2`, configured as `Item_Axe`).
  - `Content/__ExternalActors__/TopDown/Lvl_TopDown/8/YJ/8PI4Y1RRVSMV2UQDOXVNOR.uasset` (`G3_Pickup_ShortSword3`, configured as `Item_Pickaxe`).
- Removed: `0`.
- RG4-modified Content: exactly `Content/LevelPrototyping/interactionSystem/PlayerInteractionComponent.uasset`.
- No other baseline Content package is modified. No resource variant defaults, map actors, item definitions, tool assets, inventory UI, storage, or drop-to-world Content changed by RG4.

## Deliberately Deferred

- Respawn/regeneration, random yield, tool efficiency, durability, stacking, and RG5.

## Completion

`RG4 - Yield / Depletion` is complete. The next milestone is `RG5 - Inventory Integration`; it has not been started.
