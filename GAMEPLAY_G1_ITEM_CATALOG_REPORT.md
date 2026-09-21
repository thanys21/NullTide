# Gameplay G1 Item Catalog Report

## Result

G1 is complete from baseline commit `1df186297a1cf5174ab30d552c75fb1ebc0106b7`. The ten-item catalog is saved, reload-stable, tested, visible through the existing inventory UI, and included in a successful Windows cook. G2 was not started.

## Catalog

All definitions are direct Blueprint children of `/Script/NullTide.ItemDefinition`. Every definition uses its existing `/Game/Items/Icons/T_Icon_*` texture, stores capabilities in canonical `ItemFragments`, directly owns those inline templates on its CDO, and has an empty legacy `Fragments` array.

| Definition | Capabilities |
| --- | --- |
| Item_HealthPotion | Consumable, Healing(50) |
| Item_ShortSword | Weapon(Sword, 10, 1.0, 150), Equippable(MainHand), Durability(100) |
| Item_Wood | Resource(Wood) |
| Item_Sandwich | Consumable, Food(30) |
| Item_Stone | Resource(Stone) |
| Item_Meat | Consumable, Food(20) |
| Item_LongSword | Weapon(Sword, 18, 0.8, 190), Equippable(MainHand), Durability(150) |
| Item_WoodBow | Weapon(Bow, 8, 0.8, 800), Equippable(MainHand), Durability(80) |
| Item_WoodArrow | Ammo(Arrow, 1) |
| Item_WaterBottle | Consumable, Drink(30) |

`EWeaponType::Bow` was appended as value 2. Existing serialized values remain `None=0` and `Sword=1`.

## Sword Migration

`Item_Sword` was renamed through Unreal AssetTools to `Item_ShortSword`. Its existing Weapon, Equippable and Durability templates were preserved, and the Lvl_TopDown pickup external actor was saved with the canonical class reference.

Before cleanup, the old package had zero asset-registry referencers. A scoped Unreal `ResavePackages -fixupredirects` pass removed the redirector; the old package no longer exists. Fresh-process verification scanned 268 Content packages and found no stale Item_Sword dependency. Item_ShortSword has exactly one production asset referencer: `/Game/__ExternalActors__/TopDown/Lvl_TopDown/E/8K/ESLR750XUODV4CZ61M4ZMD`.

Historical Markdown and validation logs retain narrative mentions of the old name as evidence. They are not production references.

## Final Scope

Content changes against the baseline are exactly:

- Added nine definitions: Item_HealthPotion, Item_LongSword, Item_Meat, Item_Sandwich, Item_ShortSword, Item_Stone, Item_WaterBottle, Item_WoodArrow and Item_WoodBow.
- Modified Item_Wood metadata/icon while preserving its Resource template.
- Modified only the existing ShortSword pickup external actor in Lvl_TopDown.
- Removed Item_Sword after Unreal-aware redirector fixup.
- Left all ten icon texture files byte-identical to their baseline SHA-256 hashes.

Source changes are exactly:

- `Source/NullTide/Public/Items/Fragments/ItemFragmentTypes.h`: append Bow.
- `Source/NullTide/Private/Tests/InventoryComponentTests.cpp`: canonical ShortSword path.
- `Source/NullTide/Private/Tests/InventoryCutoverTests.cpp`: canonical ShortSword path/name.
- `Source/NullTide/Private/Tests/ItemFragmentTests.cpp`: canonical ShortSword path.
- `Source/NullTide/Private/Tests/ItemCatalogTests.cpp`: ten permanent parameterized catalog cases.

Documentation changes are this report and `G1_PROGRESS.md`. Eleven inspected compiler-only Blueprint resaves were restored byte-for-byte from HEAD after reflected schema, CDO default and graph equivalence checks. No unrelated Content resave remains in the hash diff, and all 21 relevant assets are clean in the active editor.

## Validation

| Check | Result |
| --- | --- |
| NullTideEditor Win64 Development | Pass, exit 0 |
| Permanent automation suite | 53 passed, 0 warnings, 0 failed, 0 not run |
| Relevant Blueprint compiles | 21/21 pass, `warnings_as_errors=true` |
| Fresh definition reload | 10/10 pass |
| Icon persistence | 10/10 exact references; source textures byte-identical |
| Fragment persistence | Exact canonical values pass; direct CDO ownership pass |
| Legacy fragments | Empty on 10/10 definitions |
| ShortSword references | Final pickup reference pass; no stale production Item_Sword reference |
| Rendered PIE | Pass; two real pickups plus all-ten inventory UI smoke |
| Windows cook | Pass; 590 packages, 0 errors |

PIE exercised the existing `InventoryComponent`, `W_Inventory` and `W_InventorySlot` flow. Wood and ShortSword were awarded by moving their existing PIE pickup actors into overlap; the other eight definitions were added through the same runtime component for the all-ten display check. The UI refreshed without reopening, showed ten unique native instances, and resolved exact names, descriptions, tooltips and icons. No PIE-only state was persisted.

The Windows commandlet performed a full cook of `/Game/TopDown/Lvl_TopDown` and `/Game/LevelPrototyping/InventorySystem/Items`. It completed 590 packages with zero errors. Its sole warning reports that the NTFS journal is disabled on the engine's `D:` volume, which only prevents cached asset discovery.

## Evidence

Evidence is preserved in `Saved/Validation/GameplayG1`, including baseline/current hashes, scope and icon comparisons, redirector fixup, final build/tests, fresh saved-state audit, Blueprint compiles, reference checks, rendered PIE JSON/screenshot, and the Windows cook log.

## Remaining Risks

- Validation cooked the Windows target but did not stage or launch a packaged executable; that is outside G1's requested cook check.
- Fresh map loading still emits the project's existing navmesh serialized-maxTiles reconstruction warning. It did not fail Blueprint validation, PIE or cook and is unrelated to the catalog.
- The cook's disabled `D:` NTFS journal warning may slow future asset discovery but does not affect output correctness.

No known G1 functional or migration risk remains. G1 is complete; G2 has not started.
