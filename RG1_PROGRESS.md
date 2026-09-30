# RG1 Progress - Generic Timed Resource Node Foundation

## Complete

RG1 is complete on 2026-09-30. The baseline is G7 completion commit `df4305e feat: complete G7 storage and containers` with 280 Content `.uasset` packages.

## Delivered

- Added `AResourceNodeActor` and `EResourceNodeState` with `Idle`, `Gathering`, and `Depleted` states; default `GatherDuration` is two seconds.
- `BeginGatherFromInteractor` validates its interactor, inventory, configured output definition, remaining yield, and active state before starting one native one-shot timer.
- Completion resolves the inventory again and calls `UInventoryComponent::TryAddDefinition` as the sole award transaction. Yield is decremented only after a successful add; invalid/full inventory paths remain atomic.
- `CancelActiveGatherForInteractor` clears the active action without award or yield mutation. Existing move and jump paths on `BP_TopDownCharacter` call it before their original behavior.
- The existing `IA_Interact` mapping is now `F`; `WB_interactionWidget` says `Press "F" to interact`; `GetActiveInteractable` safely handles an empty `InteractableInRange` list while retaining the existing last-added target selection when candidates exist.
- `BP_ResourceNode` is a thin `BPI_Interactable` adapter: `Event Interact(Interactor) -> BeginGatherFromInteractor(Interactor)`. It is configured for `Item_Wood`, `MaxYield = 3`, and a two-second duration.
- One persisted editor-world prototype, `RG1_WoodResourceNode`, is at `(-560, 300, 128)` in `/Game/TopDown/Lvl_TopDown`.
- Added `UGatherProgressPresenterComponent`, `UGatherProgressWidget`, and `W_GatherProgress`. The player-owned presenter lazily reuses one presentation widget and observes the authoritative active node through `IsGathering` and normalized native progress. It does not own a timer or alter gathering authority; `AResourceNodeActor` has no tick.
- Removed all temporary `[RG1InteractProbe]` diagnostics.

## Rendered PIE Acceptance

Manual PIE passed:

- `F` starts gathering and shows `W_GatherProgress`.
- Successful timed completion awards one Wood and hides/resets the progress UI.
- Cancellation before completion hides/resets the UI and awards no Wood; move and jump cancellation preserve that contract.
- Repeated successful gathers add separate Wood runtime items.
- A cancelled gather produces no duplicate award.
- No visible gameplay crash or error was observed.

Manual PIE did not claim exact `RemainingYield`, revision, or state-enum values. Focused native automation covers those authority semantics.

## Validation

- Blueprint validation with warnings-as-errors passed for `W_GatherProgress`, `PlayerInteractionComponent`, `BP_ResourceNode`, and `BP_TopDownCharacter`.
- `NullTideEditor Win64 Development` build succeeded.
- Fresh focused `NullTide.ResourceGathering.RG1` automation passed `5/5`: `Saved/Validation/GameplayRG1/final-focused-tests.log`.
- Fresh full permanent `NullTide` automation passed `93/93`: `Saved/Validation/GameplayRG1/final-full-tests.log`.
- Windows cook passed for `/Game/TopDown/Lvl_TopDown` and `/Game/LevelPrototyping/ResourceGathering`, including required dependencies: 611 packages, 0 errors. Evidence: `Saved/Validation/GameplayRG1/final-cook-windows.log`.

## Final Content Scope

Final SHA-256 comparison: `280 -> 283` packages. Evidence: `Saved/Validation/GameplayRG1-content-hash-comparison-final.json`.

Added:

- `Content/LevelPrototyping/ResourceGathering/BP_ResourceNode.uasset`
- `Content/LevelPrototyping/ResourceGathering/W_GatherProgress.uasset`
- `Content/__ExternalActors__/TopDown/Lvl_TopDown/7/GB/XWOBY1J561HKIKXNPH9WOU.uasset`

Modified baseline packages:

- `Content/LevelPrototyping/interactionSystem/PlayerInteractionComponent.uasset`
- `Content/LevelPrototyping/interactionSystem/WB_interactionWidget.uasset`
- `Content/TopDown/Blueprints/BP_TopDownCharacter.uasset`
- `Content/TopDown/Input/IMC_Default.uasset`

Removed packages: none. No ItemDefinitions, icons, inventory/storage authority assets, G3 pickup packages, G7 storage packages, or unrelated map actor packages changed.

Source scope is limited to the RG1 resource-node authority, focused tests, gathering-progress presenter/widget, and the private storage-test helper rename that resolves an existing unity-build symbol collision.

## Known Notes And Non-Goals

- The host `D:` volume does not expose an NTFS change journal, producing the known uncached-discovery cook warning only.
- Fresh automation commandlets may log a port-8000 bind warning because the running editor owns the MCP endpoint; both final suites still completed successfully.
- Item stacking is not implemented. Separate Wood entries are expected under the current one-item-per-slot inventory model.
- RG1 intentionally excludes tools, stamina, animations, VFX, audio, crafting, resource variants, persistence, and Gameplay Ability System work.

Next milestone: `RG2 - Tree / Rock / Scrap`. RG2 has not been started.
