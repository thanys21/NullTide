# Gameplay G4 - Inventory UI Report

Completed 2026-09-23 from the G1/G2/G3 baseline commit `40aa59f9140bab187deef618598846558f8e6197`.

## Result

G4 delivers a read-only, authority-safe inventory UI. `UInventoryComponent` remains the sole mutable inventory authority. The UI projects `GetItemsSnapshot()` into a filtered WrapBox and stores only an identity-based `SelectedItemId`; it does not own a second inventory store or mutate catalog data.

## Widget Hierarchy

`W_Inventory` is reparented to `UInventoryScreenWidget` and contains:

- Full-screen dimmer and centered inventory window.
- Header with title and close command.
- All, Resource, Consumable, Weapon, and Ammo tabs.
- Visual-only Head, Body, Main Hand, and Off Hand equipment placeholders.
- `InventoryContainerWrapBox` backpack grid.
- Details panel with icon, name, category, description, canonical capability summary, and visual action placeholder.
- Footer showing projected count, authoritative total, and fixed 24-slot capacity.

`W_InventorySlot` is reparented to `UInventorySlotWidget`: one 72x72 reusable category-aware slot with normal, hover, selected, dragging, and stale presentation states, icon, category marker, tooltip, and durability indicator. It is not category-specific.

`W_InventoryDragVisual` is the configured `UInventoryDragVisualWidget` class used by the drag operation. It displays an item icon/name and carries no authority.

## Behavior

Category derives from canonical fragments with precedence Weapon, Ammo, Resource, Consumable, Misc. The ten persistent G3 items produced these projection counts:

| Tab | Count |
| --- | ---: |
| All | 10 |
| Resource | 2 |
| Consumable | 4 |
| Weapon | 3 |
| Ammo | 1 |

Selection stores only `FGuid SelectedItemId`. It survives an ordinary refresh while the exact authoritative item remains visible and clears when that item is removed or hidden by the active filter. Details are derived from the definition CDO and canonical fragments. Rendered validation confirmed Wood (`Type: Wood`), Health Potion (`Healing: 50`), Short Sword (damage, speed, range, durability, Main Hand), and Wood Arrow (ammo type and modifier), with their icon, name, description, and category.

Hover tooltips resolve name, description, and category from the same item definition. Filtering only changes the visible projection; it does not alter `UInventoryComponent`, its revision, or its snapshot order. Repeated switching returned to exactly ten slots with no duplicates.

Drag payloads hold the exact item ID and a reference to the candidate instance. Both are verified against the current authoritative inventory before a drop request can be accepted. An outside drop logs the G4 placeholder diagnostic only. A cancel clears visual drag state. Neither action removes, reorders, spawns, or otherwise mutates inventory. World drop remains explicitly out of scope.

## Validation

- Native build: `NullTideEditor Win64 Development` succeeded.
- Blueprint compilation: 22/22 relevant assets compiled with warnings-as-errors enabled.
- Automation: 79/79 passed, 0 warnings, 0 failures. This is the 68-test baseline plus 11 focused G4 tests.
- Rendered PIE: collected exactly ten persistent G3 pickups; count 10, revision 11, ten unique instances, one `UInventoryComponent`, one inventory widget, and ten resolved slot icons. Collection used no runtime-added item definitions.
- Drag proof: a validated outside drop emitted `G4 inventory drop requested` and retained count 10/revision 11; a cancelled drag emitted no drop request and retained the same state.
- Windows cook: `/Game/TopDown/Lvl_TopDown` plus `/Game/LevelPrototyping/InventorySystem/Items` completed with 0 errors and one known D: NTFS journal availability warning.

## Final Scope

Content baseline comparison: 276 packages before, 277 after.

- Modified: `Content/LevelPrototyping/InventorySystem/UI/Widgets/W_Inventory.uasset` and `W_InventorySlot.uasset`.
- Added: `Content/LevelPrototyping/InventorySystem/UI/Widgets/W_InventoryDragVisual.uasset`.
- No other Content package changed or was removed. Icons, all ItemDefinitions, G2 presentation, G3 map/external actors, `BP_PickUpItem`, and `BP_TopDownController` are byte-identical to baseline.

Source scope is `NullTide.Build.cs`, the focused `UI` implementation under `Source/NullTide/Public/UI` and `Private/UI`, and G4 automation test files. No G1/G2/G3 gameplay source was changed.

Evidence: `Saved/Validation/GameplayG4/TestsFinal/index.json`, `pie-collection.json`, `pie-ui-runtime.json`, `build-final.log`, `cook-windows-final.log`, and `content-hash-comparison.json`.

## Known Issues

There are no known G4 gameplay issues. The cook warning is environmental: the NTFS change journal is unavailable on engine volume `D:`, so asset discovery on that volume is uncached. It does not affect output correctness.

Next milestone: G5. It was not started.
