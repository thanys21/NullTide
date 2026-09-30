# Gameplay RG2 - Tree / Rock / Scrap Resource Variants

## Outcome

RG2 is complete. It extends the completed RG1 timed-node foundation with one salvage catalog item, three configuration-only resource variants, and a persisted eight-node prototype population. The starting checkpoint is RG1 completion commit `e27e5e9`.

## Catalog Extension

`/Game/LevelPrototyping/InventorySystem/Items/Item_Scrap` is a direct `UItemDefinition` Blueprint for generic salvage material.

- Metadata: `Scrap`, `A salvaged scrap material.`
- Canonical `ItemFragments`: `Resource(Scrap)` and `WorldPresentation`.
- Legacy `Fragments` remains empty.
- World presentation: engine Cube, scale `(0.32, 0.24, 0.18)`, yaw `30`.
- `/Game/Items/Icons/T_Icon_Scrap` is the minimal safe placeholder icon duplicated from the existing Stone icon through Unreal Editor operations.

No new fragment type, bespoke runtime logic, item behavior, or custom art pipeline was introduced.

## Variant Design

All variants are thin Blueprint children of `BP_ResourceNode`, which is the existing Blueprint adapter for native `AResourceNodeActor` authority. They inherit the `BPI_Interactable` route, RG1 timer, progress presentation observation, award transaction, cancellation, and depletion behavior. None adds an interaction event, timer, award, cancellation, input listener, or progress-widget logic.

| Variant | Output definition | Max yield | Duration | Prototype mesh override |
| --- | --- | ---: | ---: | --- |
| `BP_ResourceTree` | `Item_Wood` | 5 | 2.0 s | Cylinder, `(0.65, 0.65, 2.4)`, Z `120` |
| `BP_ResourceRock` | `Item_Stone` | 5 | 2.5 s | Sphere, `(1.1, 1.0, 0.9)`, Z `40` |
| `BP_ResourceScrap` | `Item_Scrap` | 4 | 1.5 s | Cube, `(1.3, 0.9, 0.5)`, Z `15`, yaw `30` |

The inherited overlap-compatible interaction component and collision setup were preserved.

## World Population

The obsolete editor-world `RG1_WoodResourceNode` prototype was removed through Unreal Editor operations. World Partition removed its external-actor package during the level save; no `.uasset` file was manually deleted.

| Label | Class | Location (cm) |
| --- | --- | --- |
| `RG2_Tree_01` | `BP_ResourceTree` | `(400, -700, 128)` |
| `RG2_Tree_02` | `BP_ResourceTree` | `(650, -1000, 128)` |
| `RG2_Tree_03` | `BP_ResourceTree` | `(900, -750, 128)` |
| `RG2_Rock_01` | `BP_ResourceRock` | `(950, 450, 128)` |
| `RG2_Rock_02` | `BP_ResourceRock` | `(700, 650, 128)` |
| `RG2_Rock_03` | `BP_ResourceRock` | `(1000, 750, 128)` |
| `RG2_Scrap_01` | `BP_ResourceScrap` | `(400, 900, 128)` |
| `RG2_Scrap_02` | `BP_ResourceScrap` | `(650, 1050, 128)` |

After save and fresh editor-level reload: Tree `3`, Rock `3`, Scrap `2`, generic RG1 node `0`, G3 pickup actors `10`, and G7 storage containers `1`.

## Rendered PIE Evidence

Manual rendered PIE confirmed that Tree, Rock, and Scrap can each be gathered repeatedly and award Wood, Stone, and Scrap respectively. Multiple variants worked in the same session, with no visible crash or gameplay error reported.

The manual pass did not inspect exact remaining yields, revisions, state transitions, cancellation, depletion, G6 world-drop, or G7 storage behavior. RG1 focused automation remains the evidence for authority, cancellation, atomicity, invalid input, and depletion semantics.

## Validation

- Blueprint warnings-as-errors: `6/6` clean: `BP_ResourceNode`, all three variants, `BP_TopDownCharacter`, and `W_GatherProgress`.
- `NullTideEditor Win64 Development`: passed.
- RG1 focused automation: `5/5` passed.
- Catalog automation: `11/11` passed.
- World-presentation automation: `16/16` passed.
- Full permanent automation: `95/95` passed.
- Windows cook of `/Game/TopDown/Lvl_TopDown` plus `/Game/LevelPrototyping/ResourceGathering`: passed, `0` errors.

Evidence is retained in `Saved/Validation/GameplayRG2/`, including final focused, catalog, presentation, full-suite, cook, and content-comparison logs.

## Content Scope

Final comparison against RG1 commit `e27e5e9`: `283 -> 295` Content packages.

- Added `13`: `T_Icon_Scrap`, `Item_Scrap`, the three resource variants, and eight World Partition external actors.
- Removed `1`: the old RG1 generic-node external actor package.
- Modified baseline packages: `0`.
- Retained baseline packages byte-identical: `282`.

`Saved/Validation/GameplayRG2/final-content-hash-comparison.json` records exact paths and SHA-256 values for additions. The compiler-only resaves of `BP_ResourceNode` and `PlayerInteractionComponent` were restored to the RG1 commit before the final comparison. Existing item definitions, icons, pickup packages, storage packages, inventory/storage UI, and unrelated map actors are unchanged.

## Known Environment Warnings

- Windows cook reports the existing D: NTFS journal availability warning for uncached asset discovery.
- Commandlet startup reports unavailable non-Windows platform SDKs and may report an MCP port-8000 bind conflict while an editor is running. Requested tests completed successfully.

## Non-Goals

- Stacking is not implemented; repeated successful gathers create separate runtime items.
- Tool requirements are not implemented.
- Resource respawn is not implemented.
- No custom art, VFX, audio, animations, tools, crafting, or RG3 work was added.

## Next Milestone

`RG3 - Tool Requirement` is the next milestone and was not started by this work.
