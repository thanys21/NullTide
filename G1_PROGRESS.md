# Gameplay G1 Progress

## Reconstructed state - 2026-09-21

Baseline: 1df186297a1cf5174ab30d552c75fb1ebc0106b7. Initial git status and diff are empty. Neither G1_PROGRESS.md nor GAMEPLAY_G1_ITEM_CATALOG_REPORT.md existed. Saved/Validation is absent. Resume is based on this checkout and live Unreal MCP evidence; no prior G1 asset changes were found or reverted.

Read AGENTS.md, INVENTORY_V2_M8_REPORT.md, source tree, item definition/instance/fragments and existing tests. M8 report records 43 passing tests, Blueprint/PIE/build/cook passes, but these are historical results, not current G1 verification. Latest commit removes three external actors; preserve that committed state.

Completed foundation: UItemDefinition, canonical capability fragments, native inventory authority and existing UI. All ten existing T_Icon_* textures are present under /Game/Items/Icons. Item_Wood and Item_Sword are saved/clean Blueprint definitions directly parented to /Script/NullTide.ItemDefinition, with empty legacy arrays and canonical M6 fragments. Current metadata uses old descriptions and engine placeholder icons. EWeaponType has None=0 and Sword=1; Bow is pending.

| Definition | Exists | Icon | Fragments | Status | Action |
| --- | --- | --- | --- | --- | --- |
| Item_HealthPotion | No | Existing T_Icon_HealthPotion | Pending | Pending | Create metadata and Consumable/Healing(50) |
| Item_ShortSword | No; Item_Sword exists | Existing T_Icon_ShortSword; old definition uses AICON-Red | Weapon/Equippable/Durability preserved from M6 | Partial | Audit/migrate Sword; assign metadata/icon |
| Item_Wood | Yes | AICON-Green; T_Icon_Wood available | Resource(Wood) | Partial | Assign description/icon; preserve fragment |
| Item_Sandwich | No | Existing T_Icon_Sandwich | Pending | Pending | Create Consumable/Food(30) |
| Item_Stone | No | Existing T_Icon_Stone | Pending | Pending | Create Resource(Stone) |
| Item_Meat | No | Existing T_Icon_Meat | Pending | Pending | Create Consumable/Food(20) |
| Item_LongSword | No | Existing T_Icon_LongSword | Pending | Pending | Create Weapon(Sword,18,0.8,190)/MainHand/Durability(150) |
| Item_WoodBow | No | Existing T_Icon_WoodBow | Pending | Pending | Append Bow enum; Weapon(Bow,8,0.8,800)/MainHand/Durability(80) |
| Item_WoodArrow | No | Existing T_Icon_WoodArrow | Pending | Pending | Create Ammo(Arrow,1) |
| Item_WaterBottle | No | Existing T_Icon_WaterBottle | Pending | Pending | Create Consumable/Drink(30) |

Definitions folder: /Game/LevelPrototyping/InventorySystem/Items.
Item_Sword registry referencer: /Game/__ExternalActors__/TopDown/Lvl_TopDown/E/8K/ESLR750XUODV4CZ61M4ZMD. Wood referencer: /Game/__ExternalActors__/TopDown/Lvl_TopDown/D/VJ/M3RM8HR4BE1SRMFMYDNVSU. Native tests also reference Sword's old path/name; migrate those expectations without dropping tests. BP_Sword is the separate legacy hold-interaction world actor, not an inventory definition.

Current modifications: this progress document only. No content or source changes made yet.
Current G1 validation: build, permanent tests, Blueprint compile, rendered PIE, all-ten UI smoke, cook and final content hashes all pending.

Exact next action: capture baseline Content hashes, finish definition/subclass/graph/map reference audit, then use Unreal-aware Sword rename and repair only required referencers. Append Bow without changing existing enum values, author only missing catalog data, add focused permanent catalog tests, build/restart editor, validate all requested stages. Stop after G1; do not start G2.

## Checkpoint 2 - Audit, rename and native tests

Captured baseline Content SHA256 in Saved/Validation/GameplayG1/content-before.json. MCP audited 18 Blueprint assets/graphs, all loaded ItemDefinition subclasses and both pickup actors. No graph literal Item_Sword references; only two production definition subclasses (remaining native subclasses are existing test fixtures). W_InventorySlot already resolves native instance -> class defaults -> icon, resolved name/description and tooltip; no UI modification needed.

Unreal MCP AssetTools.move renamed Item_Sword to Item_ShortSword, preserving its three inline CDO templates. Old package is currently a redirector. The existing sword pickup now resolves Item_ShortSword_C. MCP scene.save_actor cannot save the external actor (wrapper asset-existence check rejects external packages); an MCP-driven editor Python call to EditorLoadingAndSavingUtils.save_packages saved exactly its loaded external package successfully. Old Item_Sword now has zero registry referencers. Evidence: migration-save.json. No unrelated asset was saved. Eleven loaded Blueprints are compiler-dirty after graph inspection/rename; do not save them indiscriminately.

Native edits: append EWeaponType::Bow (None=0 and Sword=1 unchanged); update three existing test files to canonical ShortSword path/name; add ten parameterized permanent G1 catalog cases in ItemCatalogTests.cpp. Tests check authored metadata/icon/configuration, direct native parent, canonical/legacy arrays, direct CDO ownership, duplicate owned templates and unchanged reflected template data after acquisition. Compilation has not run yet.

Current Content edits: added Item_ShortSword, replaced Item_Sword with redirector, modified only the sword pickup external package. Other eight new definitions and all G1 metadata assignments remain pending. Editor close via Slate MCP has stalled; resolving the desktop close state before full build/restart. Exact next action: complete editor shutdown, build NullTideEditor Win64 Development, then author remaining catalog with Bow loaded. G1 tests/Blueprint/PIE/cook/final hashes remain pending.

## Checkpoint 3 - Catalog and first validation pass

Full NullTideEditor Win64 Development build passed (UE5.8.2/MSVC14.44 on this machine). First attempt required access to the UBT profile cache; compilation then found and fixed a const-pointer assertion in the new test. Final build exit0, 3.78s. No build/config settings changed.

All ten definitions now exist, have the exact requested names/descriptions/icons and canonical capability values. Authored missing assets via MCP-driven Unreal Python BlueprintFactory/new_object/editor save APIs. Wood and ShortSword templates were preserved. Existing icons were referenced, not imported or duplicated. authored-catalog.json records metadata and CDO ownership; exact numeric capability assertions passed in permanent tests.

All 53 permanent tests passed: original43 + G1 catalog10, zero failures/skips/test warnings/errors (native-tests-first.json). All21 requested Blueprints compiled warnings_as_errors=true (blueprint-compiles.json). Fresh disk reload still pending for the new definitions.

Rendered PIE passed: one inventory component; empty initialization revision1; moved only the existing PIE pickup actors into pawn overlap, confirming Wood then ShortSword awards and pickup destruction, revisions2/3. Existing W_Inventory was created before pickups through its existing native widget factory; it refreshed without reopening. Added the other8 definitions through the same runtime InventoryComponent for controlled G1 UI validation only. All10 existing W_InventorySlot children resolve the exact live native instance, name, description, tooltip and existing icon; ten unique GUIDs, matching compatibility projection, revision11. No persistent pickups/map/UI changes. PIE stopped. Evidence: pie-initial.json, pie-pickups.json, pie-catalog.json, pie-catalog.png. Diagnostic Python API-binding attempts were corrected; assertions passed.

Shutdown from the first editor saved eleven compiler-dirty Blueprints. Baseline packages were extracted read-only from HEAD to Saved/Validation/GameplayG1/Baseline. Unreal loaded each baseline into a separate /Temp package; all eleven match current reflected class schemas, complete CDO defaults and node/pin graph snapshots after package-prefix normalization. resave-inspection.json records equal=true for each. Authorized compiler-only byte restoration is next. Current three dirty packages (controller and two inventory widgets) are read/compile side effects; no dirty map packages.

Exact next action: restore verified compiler-only resaves from HEAD, reload/close editor safely; scoped Item_Sword redirector cleanup; fresh-process tests/compiles; Windows Lvl_TopDown cook including the unplaced catalog; final reference/hash scope and report. G1 only.

## Checkpoint 4 - G1 complete

Restored all eleven verified compiler-only Blueprint resaves from HEAD. Unreal reloaded the restored packages successfully. A final MCP dirty-state check covers all 21 relevant Blueprints and reports zero dirty assets, so no compiler-only resave remains either on disk or in the active editor.

Cleaned the old Item_Sword redirector with a scoped Unreal ResavePackages `-fixupredirects` commandlet after confirming zero referencers. The old package and redirector no longer exist. A fresh process scanned all 268 current Content packages and found no old Item_Sword dependency. Item_ShortSword has the intended single production referencer, the existing Lvl_TopDown pickup external actor; the map reload resolves its two pickups as Item_ShortSword_C and Item_Wood_C.

Final validation passed:

- NullTideEditor Win64 Development build: pass (exit 0, target up to date).
- Permanent automation tests: 53/53 passed, including 10 G1 catalog cases; 0 warnings, failures or skipped tests.
- Relevant Blueprint compiles: 21/21 passed with warnings_as_errors=true.
- Fresh saved-state catalog audit: 10/10 direct ItemDefinition children load with exact metadata, icons and capability values; canonical fragments are CDO-owned; all legacy Fragments arrays are empty.
- Rendered PIE: pass; Wood and ShortSword pickups awarded through InventoryComponent, then all 10 items displayed through the existing inventory/slot UI with exact names, descriptions, tooltips and icons.
- Windows full cook: pass for /Game/TopDown/Lvl_TopDown plus /Game/LevelPrototyping/InventorySystem/Items; 590 packages, 0 errors, 1 environment-only D: NTFS journal warning.

Final baseline comparison against commit 1df186297a1cf5174ab30d552c75fb1ebc0106b7 is exact: nine item definitions added, Item_Sword removed, Item_Wood and one ShortSword pickup external actor modified. All ten existing icon textures are byte-identical to baseline. Source scope is Bow appended at enum value 2, three existing test path/name migrations, and the new 10-case catalog test. No runtime inventory authority or UI code changed.

Final report: GAMEPLAY_G1_ITEM_CATALOG_REPORT.md. Evidence remains under Saved/Validation/GameplayG1. G1 is complete. Do not start G2.
