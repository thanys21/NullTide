# RG2 Progress - Tree / Rock / Scrap Resource Variants

## Checkpoint: Item_Scrap Catalog Extension Complete

Started from completed RG1 at commit `e27e5e9 feat: complete RG1 timed resource gathering foundation`.

- Catalog audit confirmed `Item_Wood` and `Item_Stone` existed and `Item_Scrap` did not.
- Created `/Game/LevelPrototyping/InventorySystem/Items/Item_Scrap` as a direct `UItemDefinition` Blueprint, using the existing canonical resource pattern.
- Metadata: `Scrap`, `A salvaged scrap material.`
- Canonical fragments: exactly `Resource(Scrap)` and `WorldPresentation`; legacy `Fragments` is empty. No consumable, equippable, weapon, durability, ammo, or custom runtime fragments were added.
- World presentation: `/Engine/BasicShapes/Cube`, scale `(0.32, 0.24, 0.18)`, yaw `30` degrees. The existing generic presentation library applies it successfully in the focused test.
- Added `/Game/Items/Icons/T_Icon_Scrap` as the minimal placeholder icon by duplicating the safe existing Stone catalog icon through Unreal Editor operations. No custom art was added.
- Added Scrap entries to the existing production catalog and generic world-presentation test tables so the new definition is checked through `ValidateFragments`, resolved-fragment lookup, inventory instance creation, and generic presentation application.

## Validation

- `Item_Scrap` compiled with warnings treated as errors and was explicitly saved.
- `NullTideEditor Win64 Development` build passed.
- Focused catalog automation: `11/11` passed, including `NullTide.Gameplay.G1.Catalog.Scrap`.
- Focused generic world-presentation automation: `16/16` passed, including `NullTide.Gameplay.G2.WorldPresentation.Catalog.Scrap`.
- Final checkpoint Content hash: `283 -> 285`; added only `Content/Items/Icons/T_Icon_Scrap.uasset` and `Content/LevelPrototyping/InventorySystem/Items/Item_Scrap.uasset`; modified and removed packages: none. Evidence: `Saved/Validation/GameplayRG2/item-scrap-content-hash-comparison.json`.

## Deliberately Not Started

- No `BP_ResourceTree`, `BP_ResourceRock`, or `BP_ResourceScrap` exists.
- No RG2 resource actors were placed and no map or World Partition external-actor package changed.
- No RG2 world placement, cook, rendered PIE, or RG3 work has started.

## Checkpoint: Resource Variant Authoring Complete

All three variants are direct Blueprint children of `/Game/LevelPrototyping/ResourceGathering/BP_ResourceNode`, preserving its inherited `BPI_Interactable` route and native `AResourceNodeActor` authority. Their local event graphs contain only generated parent calls for inherited standard events; no child implements `Event Interact`, timer, begin/cancel, award, depletion, input, or progress-widget logic.

| Variant | Output | MaxYield | GatherDuration | Presentation override |
| --- | --- | ---: | ---: | --- |
| `BP_ResourceTree` | `Item_Wood` | 5 | 2.0 | Built-in Cylinder, scale `(0.65, 0.65, 2.4)`, location Z `120` |
| `BP_ResourceRock` | `Item_Stone` | 5 | 2.5 | Built-in Sphere, scale `(1.1, 1.0, 0.9)`, location Z `40` |
| `BP_ResourceScrap` | `Item_Scrap` | 4 | 1.5 | Built-in Cube, scale `(1.3, 0.9, 0.5)`, location Z `15`, yaw `30` |

The existing inherited `ResourceNodeMesh` remains the interaction component: its overlap generation is still enabled and no collision profile, physics, input, or interaction-routing setting was changed.

### Validation

- `BP_ResourceTree`, `BP_ResourceRock`, and `BP_ResourceScrap` compiled with warnings treated as errors and were explicitly saved.
- `NullTideEditor Win64 Development` build passed.
- Focused RG1 authority automation passed `5/5`: `Saved/Validation/GameplayRG2/variants-rg1-focused-tests.log`.
- Focused catalog automation passed `11/11`: `Saved/Validation/GameplayRG2/variants-catalog-tests.log`.
- Focused generic world-presentation automation passed `16/16`: `Saved/Validation/GameplayRG2/variants-presentation-tests.log`.
- Full permanent automation passed `95/95`: `Saved/Validation/GameplayRG2/variants-full-tests.log`.
- Content SHA-256 comparison is `285 -> 288`: added only `BP_ResourceTree.uasset`, `BP_ResourceRock.uasset`, and `BP_ResourceScrap.uasset`; modified and removed packages: none. Evidence: `Saved/Validation/GameplayRG2/resource-variant-content-hash-comparison.json`.

## Checkpoint: RG2 World Population Persisted

The editor-world population was authored only through Unreal Editor operations in `/Game/TopDown/Lvl_TopDown`. The level was saved with the editor-level save command, then reloaded before this checkpoint was recorded.

The obsolete generic RG1 prototype `RG1_WoodResourceNode` was removed through the Unreal scene API. Its tracked external-actor package was removed by World Partition during the level save; no `.uasset` file was manually deleted.

| Actor label | Class | Location (cm) |
| --- | --- | --- |
| `RG2_Tree_01` | `BP_ResourceTree` | `(400, -700, 128)` |
| `RG2_Tree_02` | `BP_ResourceTree` | `(650, -1000, 128)` |
| `RG2_Tree_03` | `BP_ResourceTree` | `(900, -750, 128)` |
| `RG2_Rock_01` | `BP_ResourceRock` | `(950, 450, 128)` |
| `RG2_Rock_02` | `BP_ResourceRock` | `(700, 650, 128)` |
| `RG2_Rock_03` | `BP_ResourceRock` | `(1000, 750, 128)` |
| `RG2_Scrap_01` | `BP_ResourceScrap` | `(400, 900, 128)` |
| `RG2_Scrap_02` | `BP_ResourceScrap` | `(650, 1050, 128)` |

The loose tree, rock, and salvage clusters were placed in collision-clear floor regions, separated from the G3 pickup grid and the G7 storage container. No G3 pickup, storage-container, map-geometry, or class-default change was made.

### Reload Verification

After a fresh editor-level reload, the persistent counts were exactly:

- `BP_ResourceTree`: `3`
- `BP_ResourceRock`: `3`
- `BP_ResourceScrap`: `2`
- old generic `BP_ResourceNode` / `RG1_WoodResourceNode`: `0`
- `BP_PickUpItem`: `10`
- `BP_StorageContainer`: `1`

World Partition scope from the `288`-package variant checkpoint is exactly eight added external-actor packages and one removed old RG1 prototype package:

- Added `Content/__ExternalActors__/TopDown/Lvl_TopDown/1/IA/GWYHILTP3KVCYME6YHXFXP.uasset`
- Added `Content/__ExternalActors__/TopDown/Lvl_TopDown/5/9S/IRVBU01YP9H3E7NXMEGKIW.uasset`
- Added `Content/__ExternalActors__/TopDown/Lvl_TopDown/5/AP/8S905THSMPAMZY2ZG1Y1FY.uasset`
- Added `Content/__ExternalActors__/TopDown/Lvl_TopDown/7/6M/VGF8Q3B4SDLP237872BOWV.uasset`
- Added `Content/__ExternalActors__/TopDown/Lvl_TopDown/8/16/EH3W70V2FWXSCCPGPD9ZAM.uasset`
- Added `Content/__ExternalActors__/TopDown/Lvl_TopDown/9/P4/FUZ0PQWFRUXKVVFJHD1O17.uasset`
- Added `Content/__ExternalActors__/TopDown/Lvl_TopDown/B/JQ/8ALXAOO6RPW7IT2BH72EVK.uasset`
- Added `Content/__ExternalActors__/TopDown/Lvl_TopDown/F/3Y/0G1YAB9UAEZ81H3BJRZHVT.uasset`
- Removed `Content/__ExternalActors__/TopDown/Lvl_TopDown/7/GB/XWOBY1J561HKIKXNPH9WOU.uasset`

## Final Rendered PIE Evidence

The following observations were manually confirmed in one rendered PIE session:

- Repeated Tree gathering works and awards `Wood`.
- Repeated Rock gathering works and awards `Stone`.
- Repeated Scrap gathering works and awards `Scrap`.
- Multiple resource variants function correctly in the same PIE session.
- No visible crash or gameplay error was reported during this manual check.

The manual PIE pass did not claim exact `RemainingYield`, inventory revision, or state-enum values. RG1's focused authority automation remains the evidence for timer, cancellation, atomic completion, invalid-input, and depletion semantics.

## Final Validation

- Blueprint warnings-as-errors validation passed for `BP_ResourceNode`, `BP_ResourceTree`, `BP_ResourceRock`, `BP_ResourceScrap`, `BP_TopDownCharacter`, and `W_GatherProgress`: `6/6`, zero warnings and errors.
- `NullTideEditor Win64 Development` build passed.
- Focused RG1 authority automation: `5/5` passed. Evidence: `Saved/Validation/GameplayRG2/final-rg1-focused-tests.log`.
- Catalog automation: `11/11` passed. Evidence: `Saved/Validation/GameplayRG2/final-catalog-tests.log`.
- World-presentation automation: `16/16` passed. Evidence: `Saved/Validation/GameplayRG2/final-presentation-tests.log`.
- Full permanent automation: `95/95` passed. Evidence: `Saved/Validation/GameplayRG2/final-full-tests.log`.
- Windows cook for `/Game/TopDown/Lvl_TopDown` and `/Game/LevelPrototyping/ResourceGathering` passed with `0` errors. Evidence: `Saved/Validation/GameplayRG2/final-cook-windows.log`.

The cook's only warning was the existing environment notice that the D: volume does not have an NTFS journal available for cached asset discovery. It does not affect the Windows cook result. Commandlet test startup also reports unavailable non-Windows SDKs and an MCP port-8000 bind conflict when the editor is open; all requested test runs completed successfully.

### Final Content Hash Audit

`Saved/Validation/GameplayRG2/final-content-hash-comparison.json` compares every Content package against completed RG1 commit `e27e5e9`.

- Baseline: `283` packages.
- Final: `295` packages.
- Unchanged retained RG1 packages: `282`, byte-identical.
- Added: `13` intentional packages: the Scrap icon and definition, three variants, and eight RG2 external actors.
- Removed: `1` intentional package: the old RG1 generic-node external actor.
- Modified baseline packages: `0`.

Two final-compile-only resaves (`BP_ResourceNode` and `PlayerInteractionComponent`) were restored from `e27e5e9` after verifying they had no unsaved editor state and no intentional RG2 changes. They are clean in the final audit. No G3 pickup package, G7 storage package, existing icon, existing ItemDefinition, UI asset, `BP_PickUpItem`, or unrelated map actor package changed.

## RG2 Complete

`RG2 - Tree / Rock / Scrap Resource Variants` is **COMPLETE**.

The next milestone is `RG3 - Tool Requirement`. It has not been started.

### Explicit Non-Goals

- Item stacking is not implemented; separate resource runtime items are expected.
- Tool requirements are not implemented.
- Resource respawn is not implemented.
