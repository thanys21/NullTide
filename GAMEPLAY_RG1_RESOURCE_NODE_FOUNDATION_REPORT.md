# Gameplay RG1 - Resource Node Foundation Report

## Result

`RG1 - Generic Timed Resource Node Foundation` is complete. Baseline: G7 completion commit `df4305e feat: complete G7 storage and containers`.

## Authority Model

`AResourceNodeActor` owns the `Idle`, `Gathering`, and `Depleted` state model, its configured output definition, maximum and remaining yield, and a one-shot `GatherDuration` timer. `BeginGatherFromInteractor` validates the interactor, inventory, output, yield, and current state before starting. Repeated begin requests while gathering are rejected.

On timer completion, the node resolves the inventory again and calls `UInventoryComponent::TryAddDefinition`. It only decrements yield after that transaction succeeds. Cancellation clears the active timer/action without awarding or changing yield. Move and jump retain their original behavior after calling the shared cancellation route.

`BP_ResourceNode` is a thin interaction adapter with the existing interface route: `Interact(Interactor) -> BeginGatherFromInteractor(Interactor)`. Its prototype is persisted at `(-560, 300, 128)` in `/Game/TopDown/Lvl_TopDown`.

## Interaction And Presentation

- The existing interact mapping changed from `E` to `F`; the interaction prompt now displays `F`.
- `PlayerInteractionComponent` safely returns no target for an empty interaction list instead of accessing index `-1`; existing last-added target behavior remains unchanged when candidates exist.
- `W_GatherProgress` is minimal functional feedback: a small `Gathering...` label and progress bar near the lower screen center.
- `UGatherProgressPresenterComponent` is player-owned presentation. It observes the active authoritative node and reads native normalized progress. It creates at most one widget, reuses it across gathers, and hides/resets it after completion or cancellation.
- No second gathering timer, resource-node tick, stacking, tool requirement, stamina, animation, VFX, audio, crafting, or resource variants were added.

## Rendered PIE Acceptance

Manual rendered PIE passed:

- `F` starts gathering and shows the progress widget.
- Completion after `GatherDuration` awards one Wood and resets/hides the widget.
- Cancellation before completion, including movement/jump cancellation, awards no Wood and resets/hides the widget.
- Repeated successful gathers produce separate Wood runtime entries, as designed for the current non-stacking inventory.
- A cancelled gather never produced a duplicate award; no visible crash or gameplay error was observed.

Exact internal remaining-yield values, revisions, and enum transitions were not manually claimed; they are covered by automation.

## Validation

- Blueprint compile with warnings-as-errors: `W_GatherProgress`, `PlayerInteractionComponent`, `BP_ResourceNode`, `BP_TopDownCharacter` passed.
- `NullTideEditor Win64 Development`: passed.
- Focused `NullTide.ResourceGathering.RG1`: `5/5` passed.
- Full permanent `NullTide`: `93/93` passed.
- Windows cook for `/Game/TopDown/Lvl_TopDown` and `/Game/LevelPrototyping/ResourceGathering`: passed, 611 packages, 0 errors.

## Scope And Hashes

Final Content SHA-256 comparison is `280 -> 283`: no removals.

Added Content packages:

- `BP_ResourceNode.uasset`
- `W_GatherProgress.uasset`
- `__ExternalActors__/TopDown/Lvl_TopDown/7/GB/XWOBY1J561HKIKXNPH9WOU.uasset`

Modified baseline Content packages:

- `PlayerInteractionComponent.uasset`
- `WB_interactionWidget.uasset`
- `BP_TopDownCharacter.uasset`
- `IMC_Default.uasset`

No ItemDefinitions, icons, inventory/storage authority assets, G3 pickups, G7 storage Content, or unrelated level packages changed. Source scope is the resource-node authority/tests, progress presentation component/widget, and an existing storage-test helper rename needed for unity-build symbol isolation.

## Known Environment Warnings

The final cook has only the known host `D:` NTFS-journal warning. Fresh commandlets may log that port 8000 is in use by the active editor MCP endpoint; final test runs completed successfully.

## Next Milestone

`RG2 - Tree / Rock / Scrap`. Not started.
