# Gameplay G3 Progress

## Baseline - 2026-09-22

Gameplay G1 and G2 are complete at commit `89b773c09229c275af9764e41778b61167a099bf`. The initial Git status and diff are clean. Baseline SHA-256 hashes for all 268 Content packages are captured in `Saved/Validation/GameplayG3/content-before.json`; supporting baseline metadata is in `Saved/Validation/GameplayG3/baseline.json`.

The current editor level is `/Game/TopDown/Lvl_TopDown`, with no PIE session and no dirty map package before G3 editing. `BP_PickUpItem` remains the sole production pickup Blueprint. Its construction and BeginPlay paths call `RefreshWorldPresentation`; its overlap path still awards through `TryAddDefinitionToLegacyInventory` and destroys the actor only on success.

All ten ItemDefinitions load with their G1 names, descriptions, icons, and canonical capability fragments, exactly one G2 WorldPresentation fragment, and empty legacy `Fragments` arrays. No G1/G2 asset requires modification for G3.

## Existing Population

Lvl_TopDown initially contains exactly two `BP_PickUpItem` actors:

| Definition | Actor label | Actor GUID | Location |
| --- | --- | --- | --- |
| Item_ShortSword | BP_PickUpItem | `56918EAC-4F64-20E3-B45D-F2B66FF2EAAC` | (216, -167, 342) |
| Item_Wood | BP_PickUpItem2 | `D01F7C27-47E8-74DA-5A30-239FBA84EA37` | (216, 8, 333) |

Both actors are valid generic pickups and will be retained. Their existing 128 cm trigger bounds overlap, so both require intentional placement adjustment for the final display grid.

## Intended Population

The selected area is the unobstructed flat floor at `X=-900/-1200`, `Y=-600..600`, ground `Z=0`, near and directly reachable from the playable start. Root `Z=128` rests each 128 cm trigger bound on the floor. Adjacent centers are 300 cm apart, leaving a 44 cm gap between trigger volumes.

| Row | Definition | Intended actor label | Intended location |
| --- | --- | --- | --- |
| 1 | Item_HealthPotion | G3_Pickup_HealthPotion | (-900, -600, 128) |
| 1 | Item_ShortSword | G3_Pickup_ShortSword | (-900, -300, 128) |
| 1 | Item_Wood | G3_Pickup_Wood | (-900, 0, 128) |
| 1 | Item_Sandwich | G3_Pickup_Sandwich | (-900, 300, 128) |
| 1 | Item_Stone | G3_Pickup_Stone | (-900, 600, 128) |
| 2 | Item_Meat | G3_Pickup_Meat | (-1200, -600, 128) |
| 2 | Item_LongSword | G3_Pickup_LongSword | (-1200, -300, 128) |
| 2 | Item_WoodBow | G3_Pickup_WoodBow | (-1200, 0, 128) |
| 2 | Item_WoodArrow | G3_Pickup_WoodArrow | (-1200, 300, 128) |
| 2 | Item_WaterBottle | G3_Pickup_WaterBottle | (-1200, 600, 128) |

## Intended Scope

- Retain and move the existing ShortSword and Wood actors; modify only their existing external-actor packages.
- Add eight generic `BP_PickUpItem` actors, producing eight new external-actor packages.
- Save the Lvl_TopDown map package only if Unreal requires an actor-descriptor update.
- Do not modify ItemDefinitions, icons, `BP_PickUpItem`, inventory authority, UI assets, level geometry, or G1/G2 source.
- Do not create item-specific pickup Blueprints.

## Current Checkpoint - 2026-09-22

The persistent population is complete and saved through Unreal Editor operations. The two original actors were retained, moved, and relabeled; eight generic `BP_PickUpItem` actors were added with the missing definitions. A fresh editor reload verified exactly ten persistent generic pickups, one instance of every catalog definition, the exact planned labels and transforms, and the expected G2 mesh/scale/rotation presentation. Evidence is captured in `Saved/Validation/GameplayG3/fresh-population.json`.

The current Git scope is exactly two modified existing external-actor packages plus eight added external-actor packages. `Lvl_TopDown.umap` did not require a descriptor save. No ItemDefinition, icon, `BP_PickUpItem`, inventory, UI, level-geometry, or native-source package is changed. The one compiler-only `BP_PickUpItem` resave produced during validation was restored from `HEAD`.

All 21 relevant Blueprints compile successfully with `warnings_as_errors=true`; evidence is captured in `Saved/Validation/GameplayG3/blueprint-compiles.json`. Rendered PIE is currently stopped.

## Completion - 2026-09-22

Gameplay G3 is complete.

- Fresh reload: 10/10 persistent generic `BP_PickUpItem` actors, one per catalog definition, at the exact planned labels and transforms.
- Trigger layout: adjacent 128 cm trigger bounds retain a 44 cm gap.
- Blueprint validation: 21/21 relevant Blueprints compiled with `warnings_as_errors=true`.
- Permanent automation: 68/68 tests passed in a fresh commandlet process.
- Rendered PIE: all ten persistent actors were visible, awarded the correct definitions, and were destroyed after successful award. Inventory finished at count 10/revision 11 with ten unique `ItemInstance` IDs, one `InventoryComponent`, a matching compatibility projection, and ten rendered icons.
- Build: `NullTideEditor Win64 Development` succeeded.
- Cook: Windows cook for `/Game/TopDown/Lvl_TopDown` and `/Game/LevelPrototyping/InventorySystem/Items` succeeded with 0 errors and the known D: NTFS journal warning.
- Content hash scope: 268 baseline packages to 276 final packages; exactly two existing external-actor packages changed, eight external-actor packages added, and zero packages removed.
- Protected scope: map package, ItemDefinitions, icons, `BP_PickUpItem`, inventory/UI assets, native source, and config are unchanged from the G3 baseline. All ten icon hashes are byte-identical.
- Reference audit: no stale production `Item_Sword` reference and no item-specific pickup Blueprint exists.
- Compiler-only cleanup: the only compiler-dirty `BP_PickUpItem` state was discarded; no unrelated resave remains.

Full evidence and exact package mapping are recorded in `GAMEPLAY_G3_WORLD_POPULATION_REPORT.md` and `Saved/Validation/GameplayG3`.

## Next Milestone

Gameplay G4: Inventory UI redesign. G4 has not been started.
