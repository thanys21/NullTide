# Gameplay G5 - Drag & Drop UI Report

Completed 2026-09-23 from the completed G1-G4 state.

## Design

`UInventoryComponent` remains the only inventory authority. G5 deliberately does not reorder its private ordered `Items` array: snapshot order and the legacy compatibility projection preserve insertion order, and permanent tests assert that contract. A slot-to-slot drop is therefore an accepted UI-only interaction with no membership, order, revision, or `OnInventoryChanged` mutation.

`UInventoryDragDropOperation` captures the source inventory, exact `ItemId`, and exact `UItemInstance`. It validates each action through current authoritative membership and exact instance identity. Its one-shot result is `Invalid`, `Cancelled`, `AcceptedNoMutation`, or `OutsideDropRequested`.

`UInventorySlotWidget` supplies dragging and target feedback, accepts only a distinct current target identity, and clears target/source state on completion. `UInventoryScreenWidget` owns the active operation, handles inventory-background drops as `AcceptedNoMutation`, maps an outside `InventoryWindow` release to `OutsideDropRequested`, cancels a drag hidden by a category filter, and rejects a stale drag after authoritative refresh.

No world pickup is spawned, no item is removed, and no equipment, item-use, or reorder action is implemented.

## Rendered PIE

Rendered pointer interaction was manually exercised because the available Unreal MCP session could not safely drive runtime UMG hit-testing. Only the ten persistent G3 pickups were used.

- Slot-to-slot: pass. A valid source dropped on another valid item did not overlap, replace, reorder, or otherwise change inventory contents.
- Inventory background: pass. A background drop caused no visible inventory change.
- Cancel: pass. The inventory remained open and unchanged, with no visible item-state change.
- Outside-window: pass. The placeholder path caused no visible mutation or removal.
- Filter-hide during drag: pass. Switching category caused no visible mutation or unexpected behavior.
- Close/reopen: pass. No visible duplicate slots or widgets appeared.
- Final inventory count: 10.
- No visible gameplay error or crash occurred.

The revision, delegate count, and runtime-log warning count were not manually inspected. Refresh/invalidation was not manually pointer-exercised. Those behavioral contracts are covered by the G5 native automation tests: valid/stale payload membership, cancel, accepted no-mutation results, revision/order preservation, filter-hide cancellation, and refresh invalidation after authoritative removal.

## Validation

- Build: `NullTideEditor Win64 Development` succeeded.
- Blueprint compilation: 22/22 relevant Blueprints compiled with warnings-as-errors.
- Fresh-process automation: 82/82 passed, 0 warnings, 0 failures. This preserves 79 prior tests and adds three G5-focused tests.
- Windows cook: `/Game/TopDown/Lvl_TopDown` and `/Game/LevelPrototyping/InventorySystem/Items` succeeded with 0 errors. The one warning is the known D: NTFS journal environment warning.
- Content hashes: 276 packages before and after, with zero changed, added, or removed packages. No compiler-only resave needs restoration.

Evidence is in `Saved/Validation/GameplayG5/TestsFinal/index.json`, `Saved/Validation/GameplayG5/cook-windows.log`, and `Saved/Validation/GameplayG5-content-hash-comparison.json`.

## Final Scope

Source-only G5 scope:

- `Source/NullTide/Private/UI/InventoryDragDropOperation.cpp`
- `Source/NullTide/Private/UI/InventoryScreenWidget.cpp`
- `Source/NullTide/Private/UI/InventorySlotWidget.cpp`
- `Source/NullTide/Public/UI/InventoryDragDropOperation.h`
- `Source/NullTide/Public/UI/InventoryScreenWidget.h`
- `Source/NullTide/Public/UI/InventorySlotWidget.h`
- `Source/NullTide/Private/Tests/InventoryUITests.cpp`

There are no G5 Content changes. ItemDefinitions, icons, G2 presentation, G3 actors/map, pickup gameplay, controller, inventory authority, and level geometry are unchanged.

## Known Issues

There are no known G5 gameplay issues. The cook's D: NTFS journal warning is environmental and does not affect cook correctness. The manual PIE limitation is documented above; deterministic coverage for the unobservable refresh path remains in the permanent automation suite.

## G6 Integration

G6 may replace the `OutsideDropRequested` placeholder with an authority-owned world-drop transaction after it revalidates the exact `ItemId` against `UInventoryComponent`. G6 has not started.
