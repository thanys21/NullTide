# Inventory Stacking Report

## Result

Inventory Stacking is COMPLETE. The implementation starts from clean post-RG5 checkpoint `5b0f74d` and preserves the established G6 world-drop, G7 storage, RG gathering, and tool-loadout authority boundaries.

## Runtime Model

`UItemFragment_Stackable` is canonical static item-definition configuration. Its `MaxStackSize` is 50 for the authored stackable catalog. `UItemInstance` remains the runtime authority object and now owns `Quantity` as well as its exact `ItemId` and definition class. Quantity is never static definition data.

Stackable additions deterministically fill compatible existing stacks in their current order before staging and appending overflow stacks. The entire add request is preflighted before mutation. Capacity therefore counts occupied stacks/slots, allowing a full inventory to accept more of a definition when a compatible stack has remaining capacity. Failure commits no partial quantity, revision, or change event.

Definitions without `UItemFragment_Stackable`, including Axe and Pickaxe, remain one entry per runtime instance with quantity one. Tool loadout assignment remains exact-ItemId based and was not changed.

An unmerged stack keeps its exact `ItemId` across authority operations. When a whole source stack is fully absorbed into an existing compatible destination stack, the destination identity remains and the source stack identity is removed once; no runtime item is cloned.

## Integration

- Generic `BP_PickUpItem` carries a quantity and routes collection through the quantity-aware legacy compatibility boundary.
- G6 world drop creates one pickup for the whole selected stack. Successful pickup reuses normal atomic add/merge behavior; an unfit whole stack is rejected without changing the world actor or inventory.
- G7 transfers whole stacks between inventory and storage. Destination-compatible stacks merge deterministically; a transfer that cannot fully fit rejects atomically.
- `W_InventorySlot` presents a lower-right quantity label only when quantity exceeds one.

## Manual PIE Acceptance

The user confirmed all requested A-M cases passed. This includes resource merging and overflow, Stone and Scrap behavior, non-stackable Axe/Pickaxe behavior, full-capacity merge/rejection paths, G6 whole-stack drop and re-pickup, G7 transfer and atomic rejection, tool-loadout regression, and no visible gameplay error. No unobserved ItemIds, revisions, or internal state values are claimed here.

## Automated Validation

- Blueprint compile with warnings as errors: `BP_PickUpItem` and `W_InventorySlot` passed.
- `NullTideEditor Win64 Development`: succeeded.
- Focused stacking suite: 7/7 passed.
- M1/RG1/RG3/RG4/RG5/G6/G7 regression matrix: 33/33 passed.
- Full permanent automation: 121/121 passed.

The focused suite covers the fragment contract, ordered fill/overflow, capacity and atomicity, exact quantity removal, storage merge and failure atomicity, pickup compatibility, and whole-stack world drop.

## Cook

Windows cook succeeded using the configured writable local DDC. It cooked 613 packages, skipped 7 by platform, and processed 620 total packages. The cooker reported `Done!` and zero errors. One environment-only warning reported that the NTFS journal is inactive on `D:`; it did not affect cook completion.

## Content Audit

Against `5b0f74d`, Content remains 303 packages with 0 added, 10 modified, and 0 removed. There are no changed map or external-actor packages.

Modified Content:

- `Content/LevelPrototyping/InventorySystem/BP_PickUpItem.uasset`
- `Content/LevelPrototyping/InventorySystem/Items/Item_HealthPotion.uasset`
- `Content/LevelPrototyping/InventorySystem/Items/Item_Meat.uasset`
- `Content/LevelPrototyping/InventorySystem/Items/Item_Sandwich.uasset`
- `Content/LevelPrototyping/InventorySystem/Items/Item_Scrap.uasset`
- `Content/LevelPrototyping/InventorySystem/Items/Item_Stone.uasset`
- `Content/LevelPrototyping/InventorySystem/Items/Item_WaterBottle.uasset`
- `Content/LevelPrototyping/InventorySystem/Items/Item_Wood.uasset`
- `Content/LevelPrototyping/InventorySystem/Items/Item_WoodArrow.uasset`
- `Content/LevelPrototyping/InventorySystem/UI/Widgets/W_InventorySlot.uasset`

## Deferred Scope

- No split stack or partial transfer.
- No drag split.
- No quick bar, weight, or crafting.

## Next Milestone

Crafting Foundation.
