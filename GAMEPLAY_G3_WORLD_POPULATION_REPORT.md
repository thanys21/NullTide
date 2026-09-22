# Gameplay G3 World Population Report

## Status

Gameplay G3 is complete as of 2026-09-22. G1/G2 remain unchanged from commit `89b773c09229c275af9764e41778b61167a099bf`. G4 has not been started.

## Final Population

Every actor is the existing generic class `/Game/LevelPrototyping/InventorySystem/BP_PickUpItem.BP_PickUpItem_C`.

| Actor label | ItemDefinition | Location | Actor GUID | External actor package |
| --- | --- | --- | --- | --- |
| G3_Pickup_HealthPotion | Item_HealthPotion | (-900, -600, 128) | `C67913A1-4300-55B9-A2E0-83A96EDC36F9` | `B/GI/ITHQVC0338EP9254L3I0I1.uasset` |
| G3_Pickup_ShortSword | Item_ShortSword | (-900, -300, 128) | `56918EAC-4F64-20E3-B45D-F2B66FF2EAAC` | `E/8K/ESLR750XUODV4CZ61M4ZMD.uasset` |
| G3_Pickup_Wood | Item_Wood | (-900, 0, 128) | `D01F7C27-47E8-74DA-5A30-239FBA84EA37` | `D/VJ/M3RM8HR4BE1SRMFMYDNVSU.uasset` |
| G3_Pickup_Sandwich | Item_Sandwich | (-900, 300, 128) | `90DA4D03-40F9-7693-4EBC-EEB7403B86A4` | `5/II/5CMSAWXYY5N3PS8I40JJ7Q.uasset` |
| G3_Pickup_Stone | Item_Stone | (-900, 600, 128) | `61AF9D23-4AC9-A1BA-07EC-8B91CBC59ECD` | `D/RA/KE7WKY0AQNWDLF3B1C8GQ9.uasset` |
| G3_Pickup_Meat | Item_Meat | (-1200, -600, 128) | `AC93A46D-46B2-3BF6-F579-10B1736797EC` | `3/F2/CDKMSMXJR5WUTL4WLXC0D3.uasset` |
| G3_Pickup_LongSword | Item_LongSword | (-1200, -300, 128) | `FF72E259-4952-CCFD-116A-959B3CB9BFAF` | `1/FB/BORI28TYN4Y73S3KO2TGEX.uasset` |
| G3_Pickup_WoodBow | Item_WoodBow | (-1200, 0, 128) | `1116127B-48AE-8777-4A97-9497ECF37F19` | `5/OP/6X02P915FENB4A94XG7S3V.uasset` |
| G3_Pickup_WoodArrow | Item_WoodArrow | (-1200, 300, 128) | `35522350-4802-8840-7E7D-95931B55F4E7` | `5/QV/U3CNVTZZG9XJK7ZT9E4C2I.uasset` |
| G3_Pickup_WaterBottle | Item_WaterBottle | (-1200, 600, 128) | `B4F53E71-4C1A-4BDA-2EEF-4F8D8EBDBEA0` | `1/1A/2XGXDR1RVO05RDZ5PDV8WG.uasset` |

The package prefix for every row is `Content/__ExternalActors__/TopDown/Lvl_TopDown/`. ShortSword and Wood are the retained, moved existing actors. The other eight actors are new. `Content/TopDown/Lvl_TopDown.umap` did not require an actor-descriptor save and remains byte-identical to baseline.

## Persistent Audit

A fresh editor reload found exactly ten production pickups and ten unique definitions. Labels, transforms, GUIDs, generic class identity, G2 mesh/scale/rotation presentation, and trigger bounds all passed. Adjacent centers are 300 cm apart; 128 cm trigger radii leave a 44 cm gap.

All ten ItemDefinitions remained byte-identical to the audited G3 baseline. Therefore their G1 metadata/icons, canonical `ItemFragments`, empty legacy `Fragments`, and single G2 world-presentation fragment remain unchanged. No item-specific pickup Blueprint exists, and no stale production `Item_Sword` string reference was found.

## Validation Results

- Blueprint compile: 21/21 passed with `warnings_as_errors=true`.
- Permanent tests: 68/68 passed, 0 failed, fresh process, automation exit code 0.
- Rendered PIE: passed using only the ten persistent map actors; no runtime-added pickup or definition was used.
- PIE inventory: count 0 to 10, revision 1 to 11, ten unique native `ItemInstance` IDs, one `InventoryComponent`, and compatibility projection equal to the authoritative snapshot.
- PIE pickup lifecycle: each actor awarded its assigned definition and was destroyed after the successful award; zero pickup actors remained.
- PIE presentation/UI: all ten G2 presentations matched before collection, ten inventory slots resolved the correct metadata, and all ten G1 icons rendered.
- PIE runtime log: no gameplay warning/error line was emitted during the successful proof. Four later Slate-inspector warnings came from an obsolete screenshot ref and are validation-tool noise, not runtime behavior.
- Build: `NullTideEditor Win64 Development` succeeded.
- Cook: Windows cook completed for `/Game/TopDown/Lvl_TopDown` and `/Game/LevelPrototyping/InventorySystem/Items`; 0 errors, 1 warning.

## Content Scope

The baseline contained 268 Content packages. The final state contains 276:

- Modified: 2 existing external-actor packages, ShortSword and Wood.
- Added: 8 external-actor packages, one for every missing definition.
- Removed: 0.
- Map package changed: no.
- ItemDefinition/icon/`BP_PickUpItem`/inventory/UI/source/config changes: none.
- Existing icons: 10/10 byte-identical to baseline.
- Unrelated compiler-only resaves: none.

The exact SHA-256 comparison is in `Saved/Validation/GameplayG3/hash-comparison.json`. `git diff --check` passes.

## Evidence

- `Saved/Validation/GameplayG3/fresh-population.json`
- `Saved/Validation/GameplayG3/blueprint-compiles.json`
- `Saved/Validation/GameplayG3/pie-persistent-collection.json`
- `Saved/Validation/GameplayG3/g3-inventory-ui.png`
- `Saved/Validation/GameplayG3/tests-final.log`
- `Saved/Validation/GameplayG3/TestsFinal/index.json`
- `Saved/Validation/GameplayG3/build-final.log`
- `Saved/Validation/GameplayG3/cook-windows-final.log`
- `Saved/Validation/GameplayG3/content-before.json`
- `Saved/Validation/GameplayG3/content-after.json`
- `Saved/Validation/GameplayG3/hash-comparison.json`

## Known Issues

No G3 gameplay issue is known. The cook's sole warning is environmental: the NTFS change journal is disabled on engine volume `D:`, so Asset Registry discovery there is uncached. It does not affect cooked output correctness.

The G2 presentation intentionally uses simple engine primitive meshes. Replacing those visuals is outside G3.

## Next Milestone

Gameplay G4: Inventory UI redesign. It was intentionally not started.
