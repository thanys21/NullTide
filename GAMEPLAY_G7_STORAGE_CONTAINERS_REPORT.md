# Gameplay G7 - Storage / Containers Report

## Status

Complete. Baseline: `c425009914bbf00be966cd2a27dc275f2b3335a6`.

## Authority

- `UStorageComponent` is a generic ordered runtime-item authority with a default capacity of 24, revision tracking, and change events.
- Inventory and storage transfer the exact existing `UItemInstance`; its `ItemId` and UObject identity are preserved.
- Transfers are atomic. Validation and the fallible ownership move complete before membership, revision, or event commits. Failure leaves both authorities unchanged; success removes from the source without disturbing remaining order and appends to the destination deterministically.
- Focused tests cover success in both directions, duplicate definitions, stale IDs, null/full destinations, capacity `23 -> 24` success and `24 -> 25` rejection, rollback, exact identity, order, count conservation, and once-only revision/event semantics.

## World And UI

- `AStorageContainerActor` owns one `UStorageComponent`, interaction bounds, a generic title, and `OpenStorageForInteractor`.
- `BP_StorageContainer` implements `BPI_Interactable`; `Interact(Interactor)` calls `OpenStorageForInteractor`, with `W_StorageScreen` as its screen class.
- `W_StorageScreen` and `W_StorageSlot` provide separate exact-ID selections and explicit Player -> Storage / Storage -> Player buttons. No G5 drag/drop behavior was changed.
- The reusable-screen lifecycle re-adds the retained screen to the viewport after close, then safely rebinds the active storage. Close releases bindings, clears selections, restores game input, and removes the widget.
- Storage slots use clipped bounded `112 x 72` cells and `52 x 52` icon regions so source icon brush size cannot expand the grid.

## Persistent Population

- One `BP_StorageContainer` survives an editor-world reload in `/Game/TopDown/Lvl_TopDown`.
- Its World Partition external-actor package is `Content/__ExternalActors__/TopDown/Lvl_TopDown/8/GY/6F98E6K8AN86AWO1V190TS.uasset`.
- All ten existing G3 `BP_PickUpItem` actors remain present and unchanged.

## Manual PIE

Rendered manual acceptance passed: open, close, and repeated reopen; bounded slot/grid sizing; transfer in both directions; correct selected-item movement; correct counts; storage contents retained during close/reopen in the same PIE session; no duplicate/lost item; and no visible crash or error. Exact identity, revision/event behavior, ordering, stale-ID rejection, atomic failures, and capacity boundaries are evidenced by automation rather than unobserved PIE revision values.

## Validation

- Blueprint validation: `W_StorageSlot`, `W_StorageScreen`, and `BP_StorageContainer` compiled cleanly; container warnings-as-errors enabled.
- Editor build: `NullTideEditor Win64 Development` succeeded.
- Focused G7 authority automation: 3/3 passed.
- Full permanent automation: 88/88 passed.
- Windows cook: `/Game/TopDown/Lvl_TopDown` and `/Game/LevelPrototyping/InventorySystem` succeeded with 0 errors.
- Known environment warning: NTFS change journal unavailable on `D:`; it does not affect the cook result.

## Final Scope

Source scope is limited to storage authority/transfer/container/UI classes, focused G7 tests, and the required inventory capacity/transfer integration. Content hash comparison is `276 -> 280`: exactly four added packages, `BP_StorageContainer`, `W_StorageScreen`, `W_StorageSlot`, and the one persisted external-actor package. No baseline Content package changed or is missing; `BP_Door`, ItemDefinitions, icons, G2 mappings, G3 pickup packages, `BP_PickUpItem`, and existing G4-G6 UI assets are unchanged.

## Non-Goals And Next

Stacking, equipment, quick bar, crafting, weight, advanced sorting, multiple complex container types, and persistence/save systems remain out of scope. Future UX backlog only: Shift-click quick transfer, double-click transfer, Player <-> Storage drag transfer, Ctrl-click multi-select, and bulk transfer. Phase 2 - Items & Inventory is complete. Next milestone: Phase 3 - Resource Gathering, not started.
