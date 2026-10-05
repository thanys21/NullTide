# RG5 Progress - Inventory Integration

## Status: COMPLETE

Started from completed RG4 commit `bd8cc3a`.

## Authority Contract

RG5 reuses the established reward path:

`AResourceNodeActor::CompleteGather` -> `UInventoryComponent::TryAddDefinition`

- A successful gather adds one configured definition through the existing inventory authority, then consumes one yield.
- Full-inventory rejection is atomic: no item, yield, revision, or event mutation is committed.
- Freeing one slot allows a later retry to add exactly one reward and resume normal yield behavior.
- Cancellation, missing-tool rejection, and repeated interaction award nothing.
- UI refresh remains event and `GetItemsSnapshot` driven. No alternate inventory authority or world-pickup fallback was added.

## RG5 Scope

- Added `Source/NullTide/Private/Tests/ResourceInventoryIntegrationTests.cpp` with five permanent RG5 tests covering configured rewards, full-capacity atomicity and retry, cancellation, and repeated interaction.
- Updated `InventoryScreenWidget.h/.cpp` and `W_Inventory` to provide one context-sensitive tool button:
  - selected matching tool not equipped: `Equip Tool`;
  - selected exact equipped tool: `Unequip Tool`;
  - selected non-tool: disabled.
- The action delegates with the selected exact `ItemId` to the existing `UToolLoadoutComponent`; it does not clone, remove, or reorder inventory items.
- Normalized inventory opening to the canonical Enhanced Input route:
  - `IMC_Default`: `IA_OpeningInventory` maps to `I`;
  - `BP_TopDownController`: `IA_OpeningInventory.Started` is the only production open route;
  - the raw `I` Blueprint input event was removed;
  - `W_Inventory` continues to close through its native `I`/Escape close path, restoring cursor and game input.
- No Equip/Unequip InputAction or IMC mapping was introduced. Tool action remains a native/UMG button delegate.

## Rendered PIE Evidence

Manual acceptance passed for gathering rewards, full-capacity rejection and retry, cancellation, repeated interaction, G6 drop-to-world, G7 storage transfer, and RG3 loadout regressions. Scrap remains gatherable with empty tool slots by design because its required tool is `None`.

Input smoke validation also confirmed: `I` opens one inventory, `I` closes it, `I` reopens it once, and Tab does not open inventory. No visible gameplay crash or error was reported.

## Final Validation

- Blueprint validation with warnings as errors: `BP_TopDownController` and `W_Inventory` passed.
- `NullTideEditor Win64 Development`: succeeded. UnrealBuildTool recorded `Result: Succeeded` in 5.72 seconds; the BatchFiles/.NET wrapper returned a non-authoritative environment exit after that completed result.
- Focused RG5 automation: `5/5` passed in a fresh commandlet using the required memory-DDC fallback.
- Focused RG4: `5/5` passed.
- Focused RG3: `3/3` passed.
- Focused RG1: `5/5` passed.
- Inventory M1 regression: `9/9` passed.
- Full permanent suite: `114/114` passed, with zero failures and zero skips.
- Windows cook reached `Done!`: `613` packages cooked, `0` incrementally skipped, `7` skipped by platform, `620` total.

The final cook command exited nonzero only because this environment's Installed DDC graph has no writable filesystem node and its local Zen service cannot start. It used `-DDC-ForceMemoryCache -SkipZenStore`, completed the cook, and did not report a gameplay/content cook failure. The D: NTFS journal notice is informational.

## Content Audit

Compared with `bd8cc3a`:

- Packages: `303 -> 303`.
- Added: `0`; removed: `0`.
- RG5 modified Content packages:
  - `Content/LevelPrototyping/InventorySystem/UI/Widgets/W_Inventory.uasset`
  - `Content/TopDown/Blueprints/BP_TopDownController.uasset`
  - `Content/TopDown/Input/IMC_Default.uasset`
- Preserved pre-existing anomaly, not attributed to RG5:
  - `Content/LevelPrototyping/interactionSystem/PlayerInteractionComponent.uasset`

No other Content package differs from the RG4 baseline.

## Non-Goals

No stacking, split stack, quick bar, weight, crafting, or Phase 4 implementation was started.

## Next Milestone

Inventory Stacking.
