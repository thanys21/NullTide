# Gameplay G2 Progress

## Status - Complete 2026-09-22

Gameplay G2 world presentation is complete. G3 has not been started.

Baseline commit: `4da396d9295a5ceb1870b9fdcef1e4c09108a564` (`feat: complete G1 item catalog and icons`). Baseline and final SHA-256 inventories are stored at `Saved/Validation/GameplayG2/content-before.json` and `Saved/Validation/GameplayG2/content-after.json`.

## Delivered Architecture

- `UItemFragment_WorldPresentation` owns a required hard `UStaticMesh` reference, a positive finite relative scale, and a finite rotation offset.
- `UItemWorldPresentationLibrary` resolves the definition CDO, validates canonical fragments, finds the presentation fragment, and applies it to an existing `UStaticMeshComponent`. Invalid input returns a diagnostic and logs a warning without changing pickup award behavior.
- Each of the ten definitions owns exactly one `G2_WorldPresentation` subobject in canonical `ItemFragments`; legacy `Fragments` remain empty.
- `BP_PickUpItem` remains the sole production pickup Blueprint. Its `RefreshWorldPresentation` function runs from Construction Script and BeginPlay, reuses the existing mesh component, and retains `/Engine/BasicShapes/Cube.Cube` as the visible fallback.
- The overlap graph is unchanged: it awards through `TryAddDefinitionToLegacyInventory` and destroys the pickup only after a successful award.

## Presentation Mapping

| Definition | Mesh | Scale | Rotation offset |
| --- | --- | --- | --- |
| Item_HealthPotion | Sphere | (0.25, 0.25, 0.35) | (0, 0, 0) |
| Item_ShortSword | Cube | (0.08, 0.08, 0.65) | (0, 0, 0) |
| Item_Wood | Cube | (0.55, 0.22, 0.22) | (0, 15, 0) |
| Item_Sandwich | Cube | (0.50, 0.35, 0.12) | (0, 45, 0) |
| Item_Stone | Sphere | (0.32, 0.40, 0.28) | (0, 0, 0) |
| Item_Meat | Sphere | (0.45, 0.28, 0.22) | (0, 0, 20) |
| Item_LongSword | Cube | (0.06, 0.08, 0.90) | (0, 0, 0) |
| Item_WoodBow | Cube | (0.08, 0.65, 0.40) | (0, 0, 0) |
| Item_WoodArrow | Cylinder | (0.05, 0.05, 0.90) | (90, 0, 0) |
| Item_WaterBottle | Cylinder | (0.22, 0.22, 0.50) | (0, 0, 0) |

## Final Validation

- `NullTideEditor Win64 Development`: succeeded in a fresh build process.
- Permanent automation: 68/68 passed, 0 failed, 0 not run, including 15 focused G2 tests.
- Blueprints: 21/21 compiled with `warnings_as_errors=true` after a fresh editor reload.
- Fresh reload: all ten definitions retained their G1 metadata, icon, capability fragments, one owned presentation fragment, and empty legacy fragment array.
- PIE presentation: all ten mesh/scale/rotation mappings passed on the generic pickup; the two real level pickups were also staged and rendered.
- PIE interaction/UI: Wood and ShortSword were awarded by real overlap, successful pickup actors were destroyed, ten unique inventory entries were projected, and ten icon slots rendered.
- Windows cook: `/Game/TopDown/Lvl_TopDown` and the item directory completed with 0 errors and one unrelated NTFS journal warning.
- Scope hashes: 268 baseline packages and 268 final packages; exactly 11 changed, 0 added, and 0 removed.
- All ten icon textures are byte-identical to baseline. No map, external-actor, item-specific pickup Blueprint, or unrelated compiler-only package changed.
- No stale production `Item_Sword` reference remains.

## Final Scope

Content changes are exactly `BP_PickUpItem` plus the ten `Item_*` definitions. Native changes are the new fragment, helper library, focused G2 test file, and narrow expectation updates in two existing catalog/fragment test files. Documentation is this file and `GAMEPLAY_G2_WORLD_PRESENTATION_REPORT.md`.

Evidence is under `Saved/Validation/GameplayG2`. The full completion record is in `GAMEPLAY_G2_WORLD_PRESENTATION_REPORT.md`.

Next milestone: Gameplay G3 permanent world population, intentionally not started.
