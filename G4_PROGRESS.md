# Gameplay G4 Progress

## Baseline - 2026-09-22

Gameplay G1, G2, and G3 are complete at commit `40aa59f9140bab187deef618598846558f8e6197`. The initial Git status and diff are clean. SHA-256 hashes for all 276 Content packages are captured in `Saved/Validation/GameplayG4/content-before.json`; supporting metadata is in `Saved/Validation/GameplayG4/baseline.json`.

The editor is open on `/Game/TopDown/Lvl_TopDown`, with no PIE session, open asset editor, selected actor, or dirty tracked package before G4 editing. The permanent suite contains 68 tests at baseline.

All ten ItemDefinitions were audited through Unreal MCP. Their G1 metadata and icons remain valid, canonical `ItemFragments` persist, and deprecated `Fragments` arrays remain empty. Category projection from canonical fragments produces the intended catalog counts: All 10, Resource 2, Consumable 4, Weapon 3, and Ammo 1. No ItemDefinition or icon requires modification for G4.

## Existing UI Audit

### W_Inventory

The current hierarchy is a root `CanvasPanel` containing a full-screen translucent black `BackgroundColor` image and a centered 856x652 `InventoryContainerWrapBox`. It has no header, title, close control, category tabs, structured content regions, details view, equipment placeholder, or footer.

`W_Inventory` stores `InventoryComponent`, `OwningController`, and `BoundPawn`. `SetupInventoryObserver` releases prior bindings, stores the owning controller, binds `OnPossessedPawnChanged`, and rebinds to the current pawn. `RebindInventory` releases the old inventory delegate, clears slots, resolves the native `UInventoryComponent`, binds `OnInventoryChanged`, and refreshes. Tick detects controller, pawn, or component replacement. Destruct releases inventory and controller bindings. This M5 lifecycle behavior is valid and must remain intact.

`RefreshInventory` clears the wrap box and projects `UInventoryComponent::GetItemsSnapshot()` into one `W_InventorySlot` per item. Before creating a slot it validates membership through `ContainsItem(ItemId)` and exact object identity through `FindItemById(ItemId)`. It does not maintain a second authoritative data store. Refresh is idempotent, but every refresh recreates all slot widgets; there is no category projection or selection preservation.

### W_InventorySlot

The current hierarchy is a 75x75 `SizeBox` containing only a variable `ItemIcon` image. `SetItemInstance` stores the native item and source inventory, validates ItemId membership and exact object identity, resolves the `UItemDefinition` CDO, and displays its icon/name/description. The legacy definition-class fallback is used only when no native item is supplied. Destruct clears all references and display state.

The slot has no click notification, selection identity, category state, category visual, explicit hover visual, dragging state, stale visual, or drag operation. Its only tooltip is the item description. There are no inventory or slot event dispatchers.

### Input And Cursor

`BP_TopDownController` handles the existing `I` hotkey by creating `W_Inventory`, adding it to the viewport, showing the mouse cursor, and setting UI-only input mode with no mouse lock. `W_Inventory::OnKeyDown` handles `I` by hiding the cursor, restoring game-only input, and removing itself. The character's Enhanced Input mapping includes `IA_OpeningInventory`, but current production opening remains the controller's raw `I` event. G4 will preserve this established open/close flow and input intent unless validation proves a minimal controller change is necessary.

## Authority And Metadata Audit

`UInventoryComponent` remains the sole authority through `Items`, `TryAddDefinition`, `TryRemoveItem`, `GetItemsSnapshot`, `FindItemById`, `ContainsItem`, `GetItemCount`, `GetRevision`, and `OnInventoryChanged`. `UItemInstance` identity is its native `FGuid InstanceId`; selection and drag payloads will use that identity. `UItemDefinition` remains static metadata and resolves canonical fragments through `GetResolvedFragments`, `FindFragmentByClass`, and `HasFragmentByClass`.

Primary UI category will be derived from canonical fragments in this precedence order: Weapon, Ammo, Resource, Consumable, otherwise Misc. A Weapon with Equippable and Durability remains Weapon. Capability text will derive from fragment values, including weapon damage/speed/range, durability and equipment slot, ammo type/modifier, resource type, healing, hunger, and hydration. No item name or icon mapping will be hardcoded.

No existing selection, category, filtering, details, equipment, tooltip-composition, or drag/drop architecture was found.

## Intended Design And Scope

G4 will use a small native UI support layer plus the existing Blueprint widget assets. Native helpers will own category/capability projection, identity-safe lifecycle behavior, and drag payload validation; UMG assets will own the visual hierarchy and styling. This keeps behavior testable and avoids a mutable duplicate inventory store while preserving `UInventoryComponent -> GetItemsSnapshot() -> W_Inventory -> filtered view -> W_InventorySlot`.

The inventory grid will remain a `WrapBox`. It is the lowest-risk continuation of the existing responsive layout, supports one reusable slot class, and leaves each 72x72 slot independent for later drag/drop without imposing inventory ordering semantics.

Selection will store only `SelectedItemId`. A refresh preserves selection only while the exact authoritative instance still exists and remains visible under the active filter. Filtering out or removing the selected item clears selection and details consistently.

Expected intentional scope:

- Modify `W_Inventory` and `W_InventorySlot` through Unreal MCP.
- Add one `W_InventoryDragVisual` UMG asset through Unreal MCP.
- Add focused native UI category, capability-summary, inventory-screen, reusable-slot, drag-visual, and drag-operation support under `Source/NullTide`.
- Add focused native tests for new category/capability projection logic.
- Update `NullTide.Build.cs` only for required UMG/Slate module dependencies.
- Modify `BP_TopDownController` only if final input validation demonstrates a real need; otherwise leave it byte-identical.
- Create `GAMEPLAY_G4_INVENTORY_UI_REPORT.md` and finalize this progress document.

Protected scope:

- No ItemDefinition, icon, canonical fragment template, G2 world-presentation asset, G3 external actor/map package, pickup gameplay, inventory authority, character gameplay, or level geometry change.
- No equipment gameplay, item-use gameplay, inventory reordering, world-drop transaction, or pickup spawning from UI.
- No item-specific slot class or item-name-specific UI logic.

## Current Checkpoint - 2026-09-22

Baseline and audit are complete. G4 implementation is now in progress and is intentionally limited to the recorded scope.

The focused native UI layer has been added under `Source/NullTide/Public/UI` and `Source/NullTide/Private/UI`: canonical category/capability projection, an identity-based inventory screen base, one reusable slot base with normal/hovered/selected/dragging/stale states, an identity-only drag operation, and a drag visual base. `NullTide.Build.cs` now includes the required UMG/Slate dependencies. Ten catalog projection test cases have been added; additional focused validity/filter/drag membership assertions required by the resume brief are still unfinished.

`NullTideEditor Win64 Development` builds successfully with the new source. Existing `W_Inventory` and `W_InventorySlot` have been reparented to the native bases, and `W_InventoryDragVisual` has been created through Unreal MCP. The old Blueprint Construct/Destruct/Tick entry nodes were removed so the native lifecycle is the sole active lifecycle path; the old helper functions remain inert for serialized compatibility.

The new designer trees and prototype styling are authored in the editor: structured header and close control, five category tabs, visual Head/Body/Main Hand/Off Hand placeholders, retained responsive `InventoryContainerWrapBox`, details panel, count footer, category-aware 72x72 reusable slot, and icon/name drag visual. Native `BindWidget` fields and slot/drag class defaults are assigned. The three intentional widget assets are currently dirty and must be compiled with warnings as errors, saved explicitly, reloaded, and runtime validated before they are considered complete. The map is clean and no protected G1/G2/G3 asset has been edited.

Current editor state: `/Game/TopDown/Lvl_TopDown`, no PIE session. Next work is focused helper test completion, compile/save/reload of the three UMG assets, then full Blueprint, automation, rendered PIE, build, cook, and hash-scope validation.

## Complete - 2026-09-23

G4 is complete. The three intended widgets were explicitly compiled and saved, reloaded in a fresh editor process, and compiled again with warnings treated as errors. `W_Inventory` and `W_InventorySlot` retain their native parents; all required `BindWidget` references, native lifecycle ownership, category-tab/details/equipment/footer layout, reusable slot states, and `W_InventoryDragVisual` class defaults persisted. No Blueprint lifecycle graph is active alongside the native lifecycle.

The native helper coverage is complete: 11 G4 tests cover category resolution and precedence, canonical capability summaries, selection identity and clearing, filtered visibility, and valid/stale drag payload membership. The fresh-process permanent suite passed 79/79 with no warnings or failures; this preserves the 68-test baseline and adds 11 focused G4 tests.

Rendered PIE used only the ten persistent G3 pickups. Collection reached count 10 at revision 11 with ten unique `UItemInstance` IDs and one `UInventoryComponent`; all ten UI icons resolved from the authoritative snapshot. Category counts are All 10, Resource 2, Consumable 4, Weapon 3, and Ammo 1. Details were verified for Wood, Health Potion, Short Sword, and Wood Arrow. Selection uses `SelectedItemId`; changing a filter clears a filtered-out selection. Hover tooltips work. Repeated tab switching remains projection-only and produces no duplicate slots. Closing and reopening leaves exactly one inventory widget.

The drag foundation validates the exact authoritative `ItemId`, creates the configured `W_InventoryDragVisual`, and does not mutate inventory. A completed outside drop emitted the placeholder diagnostic only; a canceled drag emitted no drop request. Both cases retained count 10 and revision 11, with no remove, reorder, spawn, or world-drop implementation.

Final validation:

- `NullTideEditor Win64 Development`: succeeded.
- Relevant Blueprint compiles with warnings-as-errors: 22/22 succeeded.
- Windows cook for `/Game/TopDown/Lvl_TopDown` and `/Game/LevelPrototyping/InventorySystem/Items`: succeeded, 0 errors, one known environmental D: NTFS journal warning.
- Content hash comparison: baseline 276 packages, final 277; only `W_Inventory` and `W_InventorySlot` changed and `W_InventoryDragVisual` was added. No removals. Icons, ItemDefinitions, map/external actors, `BP_PickUpItem`, and `BP_TopDownController` are byte-identical to baseline.

Validation evidence is in `Saved/Validation/GameplayG4`, including `TestsFinal/index.json`, `pie-collection.json`, `pie-ui-runtime.json`, `cook-windows-final.log`, and `content-hash-comparison.json`.

The next milestone is G5. G5 was not started; no world-drop, equipment gameplay, reordering, or item-use transaction was implemented in G4.
