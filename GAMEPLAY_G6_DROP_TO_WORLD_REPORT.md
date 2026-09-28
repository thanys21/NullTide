# Gameplay G6 - Drop To World Report

## Status

G6 is complete. G7 Storage / Containers is the next milestone and has not been started.

## Implementation

`UInventoryComponent::TryDropItemToWorld` is the authority-owned transaction. The UI supplies only the requested `ItemId` and owner context. The component revalidates the exact current item and definition, resolves a safe world transform, spawns the generic `BP_PickUpItem`, assigns its `ItemDefinition`, refreshes G2 presentation, and then calls the existing exact-ID removal path.

The ordering is atomic from inventory ownership's perspective:

1. Validation and placement failure leave inventory, revision, and events unchanged.
2. Spawn/configuration completes before removal.
3. Remove failure destroys the spawned actor and returns rollback failure.
4. Successful `TryRemoveItem` is the only revision increment and `OnInventoryChanged` broadcast.

Duplicate definitions remain distinct because every operation targets the runtime `ItemId`; removal preserves the relative order of all remaining item instances and rebuilds the legacy compatibility projection from that authoritative order.

## Placement And Collision

The placement resolver uses a downward `Visibility` support trace with the owning pawn and existing generic pickup actors ignored. It derives a conservative extent and vertical clearance from the definition's configured world-presentation mesh, scale, and rotation, then checks world-static and world-dynamic occupancy while retaining the pickup ignore list. Existing pickups may therefore stack, while real world blockers remain rejected.

`BP_PickUpItem` separates presentation from interaction: its visual static mesh is `NoCollision`; its query-only sphere trigger overlaps Pawn only. The existing actor-overlap compatibility pickup route and G2 `RefreshWorldPresentation` behavior are preserved. Spawn policy remains `AdjustIfPossibleButDontSpawnIfColliding`.

## Rendered PIE

Manual rendered pointer interaction was required because the available MCP session cannot safely drive runtime UMG drag hit-testing. Final PIE validation passed:

- ShortSword, LongSword, Wood, and a consumable each dropped and re-picked up successfully.
- Overlapping generic pickups were permitted and each remained individually collectible.
- A real world blocker was rejected.
- No visible gameplay errors or crashes occurred.

This confirms the G6 outside-window route spawns a generic pickup, removes only the requested identity after success, presents the dropped definition through G2, and restores the same definition through the established overlap pickup flow.

## Validation

- Editor build: `NullTideEditor Win64 Development` succeeded, final hot-reload binary `UnrealEditor-NullTide-0010.dll`.
- Automation: 85/85 permanent tests passed. Focused G6 transaction tests: 3/3 passed.
- Blueprints: 27 project Blueprints compiled with 0 Blueprint errors, 0 Blueprint warnings, and 0 failures.
- Windows cook: succeeded for `/Game/TopDown/Lvl_TopDown` and `/Game/LevelPrototyping/InventorySystem/Items`.
- Content hashes: 276 baseline packages and 276 current packages; zero added/removed; one intentional change, `Content/LevelPrototyping/InventorySystem/BP_PickUpItem.uasset`.

## Scope And Risks

Content scope is limited to `BP_PickUpItem.uasset`. Source scope is limited to the G6 authority API, screen request integration, and focused tests: `InventoryComponent.cpp/.h`, `InventoryTypes.h`, `InventoryScreenWidget.cpp`, `InventoryComponentTestTypes.h`, `InventoryComponentTests.cpp`, and `InventoryUITests.cpp`. No ItemDefinitions, icons, G2 mappings, G3 actors/map, or unrelated widgets changed. Temporary `[G6DropProbe]` diagnostics were removed before final validation.

Known environment-only warnings: the non-saving Blueprint commandlet records a host `HttpListener` port-8000 bind conflict, and commandlets report the unavailable NTFS change journal on `D:`. The Blueprint compiler itself reported zero Blueprint errors/warnings; cook succeeded with only the journal warning.
