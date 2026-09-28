# Gameplay G6 - Drop To World Progress

Started 2026-09-23 from completed G1-G5 state.

## Baseline And Audit

- G5 completed with 82 permanent tests, 22 relevant Blueprint compiles, successful editor build and Windows cook, and a 276-package Content baseline.
- G6 Content baseline hashes are captured in `Saved/Validation/GameplayG6-content-baseline.json` before implementation.
- `UInventoryComponent` is the sole authority. Its private ordered `Items` array is exposed only through `GetItemsSnapshot`, identity lookup, and `TryRemoveItem`; a successful remove rebuilds the legacy projection, increments revision once, and broadcasts `OnInventoryChanged` once.
- The G5 outside-window result is emitted by `UInventoryScreenWidget::CompleteDragOperation`. Slot-to-slot, inventory-background, and cancel behavior remain G5 no-mutation paths.
- `BP_PickUpItem` is the sole generic world pickup. It has an `ItemDefinition` class property, calls `RefreshWorldPresentation` at BeginPlay, and its overlap flow adds that definition through the established compatibility route before destroying itself. Its presentation function uses the G2 world-presentation library.
- The current level is `/Game/TopDown/Lvl_TopDown`; its existing persistent G3 population proves pickup collision and ground interaction. No persistent actor or level geometry will be changed for G6.

## Transaction Contract

G6 adds an authority-owned `UInventoryComponent` drop API. The UI provides only `ItemId` plus its owning pawn/request context. The component resolves the exact current `UItemInstance` and definition again, chooses a transform, spawns only `/Game/LevelPrototyping/InventorySystem/BP_PickUpItem`, assigns its `ItemDefinition`, refreshes G2 presentation, and only then invokes the existing exact-ID removal path.

Ordering is fixed:

1. Validate owning pawn, inventory state, `ItemId`, exact current item, and definition.
2. Resolve a safe transform near the pawn: facing-direction offset with a downward visibility trace that ignores existing generic pickups, then a small set of lateral fallback offsets. Use conservative bounds derived from the presentation mesh, reject genuine world-static/world-dynamic blockers, and allow generic pickups to stack.
3. Spawn the generic pickup with `AdjustIfPossibleButDontSpawnIfColliding`.
4. Assign the reflected `ItemDefinition` property and invoke `RefreshWorldPresentation`.
5. Call `TryRemoveItem` for the exact ID.
6. On removal failure, destroy the spawned actor and return rollback failure.
7. On success, return success. `TryRemoveItem` is the sole revision/event commit.

Any failure before removal leaves inventory, revision, and events unchanged. Spawn followed by failed removal destroys the spawned actor. There is no reorder, item-specific pickup, direct UI mutation, equipment, or item-use work.

## Intended Scope

- Extend native inventory drop authority, G5 screen integration, and focused permanent tests.
- G6 may update `BP_PickUpItem` only where its collision layout must support generic pickup stacking without disabling world collision safety.
- Protect ItemDefinitions, icons, G2 mappings, G3 map/external actors, existing pickup Blueprint behavior, and unrelated UI.

## Collision Resolution

- Probe evidence isolated the sword failure to generic pickup spawning: the visual mesh was a `PhysicsActor` cube with `QueryAndPhysics` collision, so presentation geometry participated in spawn collision.
- `BP_PickUpItem` now uses a `NoCollision` visual `StaticMesh` and a query-only custom sphere trigger that overlaps Pawn only. The established actor-overlap compatibility pickup flow is unchanged.
- The resolver uses the working `Visibility` support trace, explicitly ignores existing `BP_PickUpItem` actors for support and occupancy, derives a conservative placement profile from each definition's world presentation, and retains world-static/world-dynamic blocker checks plus `AdjustIfPossibleButDontSpawnIfColliding`.
- The fix is generic. No definition, weapon type, sword, item-authoring, or map-specific exception was introduced.

## Final Validation And Completion

G6 is complete.

- Manual rendered PIE passed ShortSword, LongSword, Wood, and a consumable drop plus re-pickup; stacked generic pickups were allowed and remained individually collectible; genuine world blockers were rejected; no visible gameplay errors or crashes occurred.
- The authority transaction removes only the exact `ItemId` after successful spawn/configuration. Successful removal is the sole revision and `OnInventoryChanged` commit. Spawn/validation failures leave inventory state unchanged; remove failure destroys the newly spawned pickup and preserves inventory state.
- Final `NullTideEditor Win64 Development` build succeeded as `UnrealEditor-NullTide-0010.dll` after probe cleanup.
- Full permanent automation passed 85/85 with no failures or not-run tests. Focused G6 transaction coverage passed 3/3.
- The non-saving Blueprint commandlet compiled all 27 project Blueprints with 0 Blueprint errors, 0 Blueprint warnings, and 0 failures. Its process summary also records the known host `HttpListener` port-8000 conflict and unavailable NTFS change journal; neither is a project Blueprint diagnostic.
- Windows cook succeeded for `/Game/TopDown/Lvl_TopDown` and `/Game/LevelPrototyping/InventorySystem/Items`, with only the host NTFS journal warning.
- Final hash comparison is 276 baseline packages to 276 current packages, with zero additions/removals and one intentional changed package: `Content/LevelPrototyping/InventorySystem/BP_PickUpItem.uasset`. No compiler-only Content resaves remain.
- Final source scope is the G6 drop authority, its screen integration, and focused tests: `InventoryComponent.cpp/.h`, `InventoryTypes.h`, `InventoryScreenWidget.cpp`, `InventoryComponentTestTypes.h`, `InventoryComponentTests.cpp`, and `InventoryUITests.cpp`.
- Temporary probe diagnostics were removed before the final build and suite.

The next milestone is G7 Storage / Containers. It has not been started.
