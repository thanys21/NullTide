# Gameplay G5 - Drag & Drop UI Progress

Started 2026-09-23 from the completed G1-G4 state.

## Baseline

- G4 completed with 79 permanent automation tests, 22 relevant Blueprint compiles, rendered ten-pickup PIE proof, a successful editor build, and a successful Windows cook.
- Content baseline hashes were captured before G5 work in `Saved/Validation/GameplayG5-content-baseline.json`.
- The only G4 Content assets are `W_Inventory`, `W_InventorySlot`, and `W_InventoryDragVisual`; all G1-G3 content remains protected.

## G5 Audit

`UInventoryComponent::Items` is a private ordered `TArray`. `GetItemsSnapshot()` preserves that order, and `RebuildLegacyProjection()` mirrors it into the legacy compatibility array. Existing permanent tests explicitly require imported, added, and legacy API item order to be preserved. There is no reorder API or independently meaningful slot-order model.

**Decision: G5 uses the no-mutation slot-drop path.** A valid source-to-target slot drop is accepted as an inventory UI interaction, clears drag state on both slots, preserves selection when still visible, and leaves membership, authoritative order, revision, and `OnInventoryChanged` unchanged. G5 will not add a reorder API because that would expand authority semantics beyond the established contracts without a gameplay requirement.

The current G4 foundation contains identity-only `UInventoryDragDropOperation`, source-slot reset, source-item membership/exact-instance validation, source drag visual creation, slot preview mouse selection/drag detection, and a root-widget drop placeholder. It does not distinguish a target slot, inventory background, filter change during drag, refresh invalidation, or a structured drop result.

## Intended Scope

- Extend the focused native UI support layer only: operation lifecycle/result, reusable slot drop/target feedback, and inventory-screen background/outside drop handling.
- Add focused permanent G5 native tests without weakening the 79 existing tests.
- Recompile and explicitly save only `W_Inventory`, `W_InventorySlot`, and `W_InventoryDragVisual` if their designer behavior requires it.
- Preserve all ItemDefinitions, icons, G2 presentation, G3 map/external actors, pickup gameplay, controller, inventory authority, and level geometry.
- No world spawning, removal, placement, equipment gameplay, item use, or authoritative reorder implementation.

## Current Checkpoint

The native G5 implementation is complete and compiled. `UInventoryDragDropOperation` now captures the source inventory plus exact item identity, exposes a compact `Invalid` / `Cancelled` / `AcceptedNoMutation` / `OutsideDropRequested` result, and can be completed only once. `UInventorySlotWidget` registers source drags, shows source and target feedback, validates distinct current source/target identities, and handles a valid slot-to-slot drop without authority mutation. `UInventoryScreenWidget` owns the active-operation lifecycle, distinguishes drops inside `InventoryWindow` from outside-window placeholder requests using cached geometry, cancels active drags when a filter hides them, and invalidates them safely when authority changes.

No UMG asset edit was necessary: the existing G4 `InventoryWindow` and slot hierarchy provide the required behavior. The three widgets compiled successfully in the rebuilt editor, and the remaining 19 required Blueprints compiled with `warnings_as_errors=true` (22/22 total).

The full permanent automation suite passed 82/82 with no failures: the 79 G4 tests plus three G5 tests covering cancellation, valid payload membership, stale rejection, accepted slot/background and outside-request results without mutation, revision/order preservation, filter-hide cancellation, and refresh invalidation after authoritative removal.

Rendered PIE has started successfully with the rebuilt module. Fresh-process automation also passed 82/82 with no warnings or failures; evidence is `Saved/Validation/GameplayG5/TestsFinal/index.json`. Windows cook for `/Game/TopDown/Lvl_TopDown` and `/Game/LevelPrototyping/InventorySystem/Items` succeeded with 0 errors and the known environmental D: NTFS journal warning only; evidence is `Saved/Validation/GameplayG5/cook-windows.log`. Final G5 Content comparison is clean: 276 packages before and after, with no changed, added, or removed packages in `Saved/Validation/GameplayG5-content-hash-comparison.json`. There are no compiler-only asset resaves to restore.

## Complete - 2026-09-23

G5 is complete. Rendered pointer interaction was exercised manually because the active Unreal MCP session did not expose safe runtime UMG hit-test execution. The test used only the ten persistent G3 pickups. The final inventory count was 10; no duplicate slots or widgets were visibly present after close/reopen; and no visible gameplay error or crash occurred.

Manual PIE observations: a valid source-to-target slot drag did not overlap, replace, reorder, or otherwise change inventory contents; dropping on empty inventory background caused no visible change; cancelling a drag left the inventory open and unchanged; outside-window drop caused no item removal or visible mutation; and changing a category during drag caused no mutation or unexpected presentation. Revision, delegate binding counts, and runtime log warnings were not manually inspected. The permanent G5 automation suite covers the non-mutating revision/order behavior, payload membership and stale rejection, filter-hide cancellation, and refresh invalidation after authoritative removal; no claim is made that refresh invalidation was manually pointer-exercised.

Final validation remains: 82/82 fresh-process automation passed with no warnings or failures, 22/22 relevant Blueprints compiled with warnings-as-errors, `NullTideEditor Win64 Development` built successfully, and the requested Windows cook completed with 0 errors and the known D: NTFS journal warning only. Content hashes remain 276 to 276 with no changed, added, or removed package; no compiler-only resaves require restoration.

The final report is `GAMEPLAY_G5_DRAG_DROP_UI_REPORT.md`. G6 was not started. Its integration point is the existing `OutsideDropRequested` result/placeholder, where G6 may add an authority-owned world-drop transaction after revalidating the exact `ItemId`; G5 performs no spawn, removal, placement, reorder, equipment, or use action.
