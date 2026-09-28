# Gameplay G7 - Storage / Containers Progress

Started from baseline commit `c425009914bbf00be966cd2a27dc275f2b3335a6` (`feat: complete G6 drop-to-world inventory flow`).

## Baseline

- G6 is complete: 85 permanent tests, 27 project Blueprint compiles, successful editor build/cook, and 276 Content packages.
- G7 Content baseline hashes were captured in `Saved/Validation/GameplayG7-content-baseline.json`: 276 packages.
- Worktree was clean before G7 changes.
- Existing interaction is `PlayerInteractionComponent` plus `BPI_Interactable`; its `Interact(Interactor)` event is the G7 integration route.
- `W_Inventory` remains the G4/G5/G6 player-inventory UI and is protected until storage presentation requires a minimal integration.

## Authority Checkpoint

- Added `UStorageComponent`: a dedicated ordered runtime-item authority with a default fixed capacity of 24 and storage revision/change notifications.
- Added a stateless transfer coordinator. It moves the existing `UItemInstance` object between component outers, preserving the exact `ItemId`; source removal preserves order and destination insertion appends deterministically.
- Inventory now exposes the same 24-slot capacity contract so either authority can reject a full transfer destination. Existing G6 flows remain unchanged below capacity.
- All validation and the only fallible ownership move occur before membership/revision/event commits. Successful transfer increments and broadcasts exactly once on each authority; failure leaves both collections unchanged.
- `NullTideEditor Win64 Development` was rebuilt successfully from a clean editor state as `UnrealEditor-NullTide.dll`.

## Authority Test Checkpoint

- Added 3 permanent focused G7 tests in `StorageComponentTests.cpp`; all 3 pass.
- `InventoryToStoragePreservesExactIdentity` verifies Inventory -> Storage success, exact `ItemId` and `UItemInstance` identity/outer preservation, duplicate-definition distinction, source-order preservation, deterministic destination append, total-count conservation, and exactly-once revisions/events.
- `StorageToInventoryAndCapacityBoundary` verifies the reverse path has the same identity and append semantics, then proves the 23 -> 24 destination transition succeeds and the 24 -> 25 transition rejects without revision changes.
- `FailuresAreAtomic` verifies stale source IDs and null destinations reject without mutation, and exercises both full-storage and full-inventory destinations with a valid exact source item. It verifies membership, revisions, and events remain unchanged on the full-storage failure and membership/revisions remain unchanged on the reverse full-inventory failure.
- Focused G7 authority automation: 3/3 passed.
- Full permanent automation: 88/88 passed in a fresh command-line editor process with exit code 0. The prior 85 tests remain green; G7 adds 3.
- No authority defect was found. SHA-256 Content comparison is exact: 276 baseline packages and 276 current packages, with zero added, missing, or changed packages.

## World Container Native Checkpoint

- Added `AStorageContainerActor` with one `UStorageComponent`, a generic title, a simple built-in cube prototype mesh, interaction bounds, and `OpenStorageForInteractor`. The latter is the only function the existing `BPI_Interactable` event needs to call; it does not introduce another interaction system.
- Added `UStorageScreenWidget` and `UStorageSlotWidget`. The screen maintains independent player/storage exact-ID selections, subscribes to both authority change events, clears moved or stale selections during refresh, restores game input on close, and closes safely when its storage authority becomes invalid.
- Explicit transfer buttons call only `TryTransferItemToStorage` and `TryTransferItemToInventory`; no G5 drag/drop behavior is modified.
- The native bridge builds successfully. Fresh full automation remains 88/88 with exit code 0.
- No Content package has been created or modified. SHA-256 comparison remains 276 -> 276 with zero added, missing, or changed packages.
- The earlier MCP availability blocker is resolved. The active UnrealEditor process responds to tool discovery and asset/widget operations.

## Content Integration Checkpoint

- Created and saved `/Game/LevelPrototyping/InventorySystem/UI/Widgets/W_StorageSlot` with native parent `UStorageSlotWidget`. Its bound designer widgets are `SelectionBorder`, `ItemButton`, `ItemIcon`, and `ItemName`.
- Created and saved `/Game/LevelPrototyping/InventorySystem/UI/Widgets/W_StorageScreen` with native parent `UStorageScreenWidget`. Its minimal dual-panel tree has independent player and storage item grids, count labels, exact-ID transfer buttons, close button, and title. Its native `SlotWidgetClass` is set to `W_StorageSlot`.
- Both widgets compiled successfully through Unreal MCP after authoring and after their final class/default configuration.
- `BP_StorageContainer` was created temporarily with native parent `AStorageContainerActor`, and its title/screen class were configured in the active editor session. It was intentionally not saved: this MCP build exposes no Blueprint-interface registration operation, and its Slate accessibility surface cannot target the Class Settings `Implemented Interfaces` control.
- Therefore the required `BPI_Interactable` `Interact(Interactor) -> OpenStorageForInteractor(Interactor)` route is not yet authored. No incomplete container asset, map package, or world-partition external actor package is present on disk.
- Current intentional Content delta is exactly two new widget packages. The level, G3 pickup external actors, ItemDefinitions, icons, G2 presentation, and G4-G6 UI assets remain untouched.

## Interface And Validation Checkpoint

- Manual editor integration completed `BP_StorageContainer`: it has native parent `AStorageContainerActor`, implements the existing `BPI_Interactable` interface, and its `EventInteract(Interactor)` graph calls `OpenStorageForInteractor(Interactor)` directly. Its `StorageScreenClass` resolves to `W_StorageScreen`.
- `W_StorageSlot`, `W_StorageScreen`, and `BP_StorageContainer` were recompiled through Unreal MCP. The container Blueprint was compiled with warnings treated as errors; all three completed cleanly. Widget parents remain `UStorageSlotWidget` and `UStorageScreenWidget`.
- `NullTideEditor Win64 Development` build succeeded on the current source state.
- Fresh focused G7 authority automation passed 3/3. Fresh full permanent automation passed 88/88 with exit code 0.
- Current SHA-256 Content comparison is 276 baseline packages to 279 current packages. The exact added packages are `BP_StorageContainer`, `W_StorageScreen`, and `W_StorageSlot`; there are no missing packages and no changed map or world-partition external-actor packages.
- The active `/Game/TopDown/Lvl_TopDown` state contains exactly ten `BP_PickUpItem` actors and zero `BP_StorageContainer` actors. Since there is also no map/external-actor hash delta, the claimed prototype placement is not persisted or present in the currently loaded map. This validation did not recreate, move, or save an actor.
- At this checkpoint `Content/LevelPrototyping/Interactable/Door/BP_Door.uasset` was reported as an unrelated modification. It was not saved or modified during this validation; the later lifecycle revalidation confirms that it is no longer modified.

## Final Regression Revalidation

- A subsequent repository/editor verification again found zero `BP_StorageContainer` actors in the loaded `/Game/TopDown/Lvl_TopDown`, while all ten persisted G3 `BP_PickUpItem` actors remain present. No placement, map, or world-partition external-actor package is added or changed against the G7 baseline.
- `BP_Door.uasset` was then still listed as an unrelated modified Content package. That historical condition is superseded by the later lifecycle revalidation, which finds no changed baseline Content package.
- `W_StorageSlot`, `W_StorageScreen`, and `BP_StorageContainer` again compiled cleanly; the native container Blueprint compile used warnings-as-errors.
- `NullTideEditor Win64 Development` again succeeded.
- Fresh final focused G7 automation passed 3/3 with zero failures. Fresh final full permanent automation passed 88/88 with zero failures.
- At that point the Content comparison remained 276 -> 279 with the three G7 additions and the out-of-scope `BP_Door` change. The later lifecycle revalidation supersedes this: the three additions remain and no baseline Content package is changed.
- Manual PIE acceptance remains deliberately unclaimed: opening/closing storage, both transfer directions, exact identity, capacity rejection/rollback, selection cleanup, ordering, reopen state, and G5/G6 regression behavior still require rendered manual validation after the persisted prototype is present.

## Storage Screen Lifecycle And Slot Layout Fix

- The first rendered PIE integration reached `BP_StorageContainer -> OpenStorageForInteractor -> W_StorageScreen` successfully, which confirmed the native parent, interface route, and screen-class configuration work end to end.
- Root cause of the reopen failure: `CloseStorage` correctly called `RemoveFromParent`, but `AStorageContainerActor` retained a valid reusable `ActiveStorageScreen` UObject. The former `IsValid` guard therefore skipped both creation and `AddToViewport` on the next interaction, leaving the screen off viewport.
- `OpenStorageForInteractor` now deliberately supports the reusable-screen lifecycle: it creates the screen only when necessary and, on every open, calls `AddToViewport` when the retained widget is not already in the viewport before rebinding the exact `UStorageComponent`. `CloseStorage` remains responsible for releasing authority bindings, clearing selections, restoring game input, and removing the screen from the viewport. No delay, forced garbage collection, or duplicate-screen workaround was added.
- Root cause of the stretched player slot: `W_StorageSlot` had no bounded desired size, and `UImage::SetBrushFromTexture(..., true)` allowed the selected item's source brush size to become the WrapBox child desired size.
- `W_StorageSlot` now has a clipped `112 x 72` root `SizeBox`, a clipped `52 x 52` icon `SizeBox`, an automatic centered icon slot, and a fill text slot with wrapping/ellipsis. This keeps icon presentation bounded and allows the player and storage grids to emit multiple reusable cells, while retaining the existing native bindings and selection behavior.
- Post-fix validation: `NullTideEditor Win64 Development` build succeeded; `W_StorageSlot` and `W_StorageScreen` compiled cleanly; `BP_StorageContainer` compiled cleanly with warnings treated as errors; fresh focused G7 authority automation passed 3/3; fresh full permanent automation passed 88/88.
- Current SHA-256 Content comparison remains 276 baseline packages to 279 current packages: only `BP_StorageContainer`, `W_StorageScreen`, and `W_StorageSlot` are added; no baseline Content package changed or is missing. `BP_Door.uasset` is no longer modified in the current worktree.
- Rendered manual revalidation of close/reopen, grid sizing, and transfer persistence is pending on this repaired build. It must not be inferred from the automated authority results.

## Final UI Acceptance And Persistence Blocker

- Rendered PIE manual acceptance passed: storage opens and closes; the same container reopens repeatedly; the player grid uses bounded correctly sized slots; Player -> Storage and Storage -> Player transfers both work; the exact selected item moves between authorities; counts update; storage contents persist across close/reopen for the current PIE session; no duplicate or lost item was observed; and no visible gameplay crash or error occurred.
- The temporary `[G7TransferProbe]` diagnostics used to isolate the transfer path were removed after this confirmation. No drag transfer, quick transfer, multi-select, stacking, or bulk-transfer behavior was added.
- Final asset validation passed: `W_StorageSlot` and `W_StorageScreen` compiled cleanly, and `BP_StorageContainer` compiled cleanly with warnings treated as errors.
- Final source validation passed: `NullTideEditor Win64 Development` built successfully after diagnostic removal. Fresh command-line automation passed focused G7 authority tests 3/3 and the full permanent suite 88/88, both with exit code 0.
- Windows cook passed for `/Game/TopDown/Lvl_TopDown` and `/Game/LevelPrototyping/InventorySystem`, cooking 606 packages with zero errors. Its sole warning is the known host NTFS journal unavailability on `D:`.
- The active editor world contains exactly one `BP_StorageContainer` and exactly ten `BP_PickUpItem` actors. No pickup actor was moved or altered during final validation. `BP_Door.uasset` is clean: the final hash comparison has no changed baseline package.
- Blocking persistence defect: the live container's external-actor package is missing from disk. `SceneTools.save_actor` failed with `Asset does not exist: /Game/__ExternalActors__/TopDown/Lvl_TopDown/3/W6/U1QNQ63T6EWT6WJ50FD7C3`; saving the current level with the editor's `Ctrl+S` did not materialize it. A temporary replacement actor was immediately removed after the same save failure, leaving exactly the original one live container.
- Final SHA-256 scope is therefore still 276 baseline packages to 279 current packages: the only additions are `BP_StorageContainer`, `W_StorageScreen`, and `W_StorageSlot`; no map package or external-actor package exists on disk for the required prototype. This prevents G7 completion despite all runtime, build, Blueprint, automation, and cook checks passing.
- Future UX backlog only, explicitly out of G7 scope: Shift-click and double-click quick transfer, Player <-> Storage drag transfer, Ctrl-click multi-select, and bulk/transfer-all actions.

## Persistence Recheck

- A later correction identified the prior placement as having occurred in PIE rather than the editor world. The requested verification was performed from a clean state: MCP reported an active `UEDPIE_0` world, PIE was stopped, and `/Game/TopDown/Lvl_TopDown` was explicitly reloaded before actor inspection.
- Current repository/editor result after that reload: zero `BP_StorageContainer` actors are present in the editor world; all ten G3 `BP_PickUpItem` actors are present. SHA-256 scope remains 276 -> 279 with only the three authored G7 asset packages and no added external-actor package.
- Consequently, the claimed editor-world placement and save are not present in the CURRENT repository state. Final build/test/Blueprint/cook reruns and G7 completion are deferred until the container survives an editor-world reload and creates exactly one external-actor package on disk.

## Completion

- Persistence root cause resolved: the earlier prototype had been placed in `UEDPIE_0` rather than the editor world. One `BP_StorageContainer` was then placed and saved in the editor `/Game/TopDown/Lvl_TopDown` world and survived a clean level reload.
- The persisted container package is `Content/__ExternalActors__/TopDown/Lvl_TopDown/8/GY/6F98E6K8AN86AWO1V190TS.uasset`.
- Final editor audit finds exactly one `BP_StorageContainer` and exactly ten G3 `BP_PickUpItem` actors. No pickup actor was moved or modified.
- Final SHA-256 comparison: 276 baseline packages -> 280 current packages. Added only: `BP_StorageContainer`, `W_StorageScreen`, `W_StorageSlot`, and the single container external-actor package above. Missing: none. Changed baseline packages: none. `BP_Door.uasset`, ItemDefinitions, icons, `BP_PickUpItem`, G2 mappings, G3 pickup packages, and existing G4-G6 UI packages remain unchanged.
- Final Blueprint validation is clean for `W_StorageSlot`, `W_StorageScreen`, and `BP_StorageContainer`; the container compile used warnings-as-errors. Final `NullTideEditor Win64 Development` build succeeded. Fresh focused G7 tests passed 3/3 and the full permanent suite passed 88/88. Windows cook of `/Game/TopDown/Lvl_TopDown` plus `/Game/LevelPrototyping/InventorySystem` succeeded with zero errors and only the host NTFS journal warning on `D:`.
- G7 Storage / Containers is COMPLETE. Phase 2 - Items & Inventory is COMPLETE. The next milestone is Phase 3 - Resource Gathering; it has not been started. Future UX backlog remains out of scope: Shift-click/double-click quick transfer, Player <-> Storage drag transfer, Ctrl-click multi-select, and bulk/transfer-all.
