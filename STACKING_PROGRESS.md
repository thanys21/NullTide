# Inventory Stacking Progress

## Status

Inventory Stacking is COMPLETE.

## Baseline and Scope

- Baseline: `5b0f74d` (clean post-RG5 checkpoint).
- Content package count: 303 -> 303.
- No map or World Partition external-actor package changed.
- Modified Content is limited to `BP_PickUpItem`, eight stackable item definitions, and `W_InventorySlot`.

## Implemented Contract

- `UItemFragment_Stackable` is static item-definition data with `MaxStackSize = 50`.
- Runtime quantity is owned by `UItemInstance`; every stack retains one exact `ItemId` and has `Quantity >= 1`.
- Stackable definitions: Wood, Stone, Scrap, WoodArrow, Sandwich, Meat, WaterBottle, and HealthPotion.
- Tools and other definitions without the fragment remain non-stackable.
- Adds fill compatible existing stacks in deterministic inventory order, then append new stacks only when necessary.
- Capacity counts occupied stacks/slots, not total item quantity. A full inventory can accept an add when an existing compatible stack has room.
- Multi-quantity add and pickup preflight the whole request; failure leaves inventory, storage, revision, and events unchanged.
- An unmerged stack preserves its exact `ItemId`; when a whole source stack is fully merged, the destination stack keeps its identity and the source identity is removed exactly once, never cloned.
- G6 drops transfer an entire selected stack to the generic pickup and preserve its quantity.
- G7 transfer moves a whole stack and merges it at the destination when capacity permits; failures are atomic.
- Inventory slots display `xN` only for quantities greater than one.

## Validation

- Affected Blueprint compile, warnings as errors: `BP_PickUpItem`, `W_InventorySlot` passed.
- `NullTideEditor Win64 Development`: succeeded.
- Focused stacking automation: 7/7 passed.
- Requested M1/RG1/RG3/RG4/RG5/G6/G7 regression matrix: 33/33 passed.
- Full permanent automation: 121/121 passed.
- Windows cook: succeeded. 613 cooked, 7 platform-skipped, 620 total; zero errors.
- Rendered PIE manual acceptance: all requested A-M cases passed. Stackable resources merged correctly; tools remained non-stackable; whole-stack drop/pickup and storage transfer behavior passed; no visible gameplay error was observed.

## Known Warning

- Windows cook emitted one environment-only warning: the NTFS journal is inactive on `D:`. Cooking completed with zero errors.

## Deferred

- No split stack or partial transfer.
- No drag split, quick bar, weight, or crafting.

## Next Milestone

Crafting Foundation. Do not start it as part of this milestone.
