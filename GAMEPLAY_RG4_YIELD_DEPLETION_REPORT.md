# Gameplay RG4 - Yield / Depletion Report

## Status

`RG4 - Yield / Depletion` is COMPLETE. The implementation began from completed RG3 commit `e009e21`.

## Authority And Rules

RG4 reuses the existing `AResourceNodeActor` authority. No parallel yield system was introduced.

- `RemainingYield` initializes from `MaxYield` and is clamped at zero.
- A successful inventory award consumes exactly one yield.
- Cancellation, invalid input, missing-tool rejection, repeated interaction while gathering, and failed inventory award leave yield unchanged.
- Reaching zero transitions the node to `Depleted`; later interaction cannot start a timer, show progress, award an item, or consume additional yield.
- `ShouldShowInteractionPromptFor` is presentation-only and hides the F prompt for depleted resource nodes. The prior empty-candidate guard and last-added active-target behavior are preserved.

Production defaults remain unchanged:

- Tree: Wood, `MaxYield=5`, `GatherDuration=2.0`, Axe.
- Rock: Stone, `MaxYield=5`, `GatherDuration=2.5`, Pickaxe.
- Scrap: Scrap, `MaxYield=4`, `GatherDuration=1.5`, no tool.

## Implementation Scope

- Added read-only `GetMaxYield` and `GetOutputItemDefinition` accessors for exact default validation.
- Updated `PlayerInteractionComponent` to suppress the interaction prompt only for depleted resource nodes.
- Added five permanent RG4 tests in `ResourceYieldTests.cpp` for production defaults, yield/depletion, and rejection/cancellation atomicity.
- Renamed a test-local helper in `ToolLoadoutTests.cpp` to resolve a clean unity-build duplicate symbol. No gameplay behavior changed.

## Validation

- Blueprint validation passed with warnings-as-errors for `PlayerInteractionComponent`, `BP_ResourceNode`, `BP_ResourceTree`, `BP_ResourceRock`, and `BP_ResourceScrap`.
- `NullTideEditor Win64 Development`: passed (`Result: Succeeded`).
- RG4 automation: `5/5` passed.
- RG1 automation: `5/5` passed.
- RG3 automation: `3/3` passed.
- Inventory M1 regression: `9/9` passed.
- Full permanent suite: `109/109` passed, zero failures and skips.

Manual rendered PIE acceptance passed:

- Tree, Rock, and Scrap depletion passed.
- Movement and Jump cancellation passed.
- Missing-tool rejection passed.
- RG3 loadout, G6 drop-to-world, and G7 storage regressions passed.
- No visible gameplay warning, error, or crash was observed.

## Cook

The final Windows cook used the recovered writable DDC path:

`C:\Users\loithienly\AppData\Local\UnrealEngine\Common\DerivedDataCache`

The cook completed with `LogCook: Display: Done!`. The process exit was nonzero because the commandlet could not bind its MCP HTTP listener to `127.0.0.1:8000`, already owned by the healthy editor. This did not interrupt cooking. The D: NTFS journal message was informational only.

## Content Audit

Against `e009e21`:

- Baseline packages: `301`.
- Current packages: `303`.
- RG4 Content modification: `Content/LevelPrototyping/interactionSystem/PlayerInteractionComponent.uasset` only.
- Removed baseline packages: `0`.
- Approved existing user-authored map additions, preserved and not attributed to RG4:
  - `Content/__ExternalActors__/TopDown/Lvl_TopDown/1/7G/SV9CRK4S4Y1RR3KNHL61B1.uasset`: `G3_Pickup_ShortSword2`, `Item_Axe`.
  - `Content/__ExternalActors__/TopDown/Lvl_TopDown/8/YJ/8PI4Y1RRVSMV2UQDOXVNOR.uasset`: `G3_Pickup_ShortSword3`, `Item_Pickaxe`.

No other baseline Content package changed.

## Non-Goals

RG4 does not add regeneration, respawn, random yield, tool efficiency or speed bonuses, durability, stacking, or any RG5 work.

## Next Milestone

`RG5 - Inventory Integration` is next and has not been started.
