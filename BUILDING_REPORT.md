# Phase 5 — Building: Completion Report

**Status:** COMPLETE (B0–B9, 2026-10-14 to 2026-10-19). Design and decisions are in `BUILDING_PLAN.md`; per-checkpoint evidence is in `BUILDING_PROGRESS.md`.

**What the player can do:**
- Press `B` to enter building mode.
- Pick a building from the build bar or with `1`–`5`.
- Place it on a 200 cm grid with a green or red ghost; `R` rotates.
- `LMB` builds and pays the cost from the inventory.
- `C` switches to demolish, which refunds 100% and takes dependent pieces with it.
- Built doors open with `F`. A built chest stores items, and a built workbench is a crafting station.

## 1. Checkpoints

| Checkpoint | Delivered | Commit | Suite after |
| --- | --- | --- | --- |
| B0 | Architecture audit and design; final approval of open decisions. | docs | 190 |
| B1 | Data model, grid coordinates, occupancy registry, read-only placement validation. | `0681261` | 198 |
| B2 | Construction authority: activate first, pay last (design gate D1–D8 approved, D3 option a). | `bccd0bc` | 219 |
| B3 | First buildable: Wooden Floor end to end; `UConstructionComponent` on the player. | `ff04c0e` | 223 |
| B4 | Building mode (`B`), ghost preview, input contexts, text HUD. | `7ce9be3` | 230 |
| B5 | Wooden wall and door, selection `1`–`3`, rotation `R`, floor requirement, facing arrow. | `b20f70a` | 236 |
| B6 | Demolition (`C`), cascade to dependents, refund (100%), storage protection. | `7dd0c1d` | 243 |
| B7 | Buildable chest and workbench (both need a floor), keys `4`/`5`, thin-blocker fix. | `8f4ae66` | 250 |
| B7.1 | Facing-based interaction targeting (forward probe, ±70° cone). | `208b12e` | 253 |
| B8 | Build bar: slots, affordability, click to select, confirm guard. | `e20b30e` | 258 |
| B9 | Polish and closeout: interaction reach 75/75; the first click after closing a screen reaches the game; this report (PIE 3/3 PASS). | this commit | 258 |

## 2. Architecture and Authority

- **Data:** `UBuildingDefinition` is an abstract, const UObject with data-only Blueprint subclasses read from the CDO. It holds the category, piece class, placement mode, footprint, cost, refund fraction, `bRequiresFloor` and display data.
- **World authority:** `UBuildingSubsystem` (`UWorldSubsystem`) owns three things:
  - the grid: 200 cm cells, canonical edges, 50 cm layers, 90° rotation;
  - the occupancy registry, which only the subsystem writes, with all-or-nothing reservations;
  - read-only `ValidatePlacement`, checked in this order: definition, shape, range, support, floor requirement, occupancy, collision, builder safety.
- **Builder authority:** `UConstructionComponent` owns building and demolition.
  - `TryBuild`: validate → reserve → spawn provisional → revalidate → activate → integrity check → **pay last** with one `TryApplyDefinitionTransaction`, which is the commit point.
  - `TryDemolish`: mark every piece `Removing` → one addition-only refund transaction, the commit point → destroy.
- **Player UX:** `UBuildingPlacementComponent` provides the mode, ghost, input, text HUD, build bar and demolish highlights. It never builds by itself; it calls `CanBuild`/`TryBuild` and `CanDemolish`/`TryDemolish`.
- **Pieces:** `ABuildingPieceActor` hosts a `UBuildingPieceComponent` (state Provisional/Active/Removing). Subclasses:
  - `ABuildingDoorActor`: a timer-driven swing;
  - `ABuildingStorageActor`: storage through `FStorageInteraction`;
  - `ABuildingCraftingStationActor`: a station through `FCraftingStationInteraction`.
- **Interaction (B7.1):** `UInteractionProbeComponent` on the player queries `BPI_Interactable` candidates fresh. `UInteractionTargetingLibrary` picks the object the player faces, and `PlayerInteractionComponent` acts on it.
- **Boundaries kept:** Building never changes inventory, crafting, storage or interaction authority. The inventory stays generic.

## 3. Key Approved Decisions

- **B2 D1:** activate before payment, so nothing fallible runs after the deduction.
  - D2 (accepted): an unpaid `Active` piece is observable for one call.
  - D3 (a): a paid piece destroyed during payment still reports `Success`, with no refund.
- **Spawning:** `AlwaysSpawn` plus the authority's own overlap check at the exact grid transform; provisional pieces are hidden.
- **Floor requirement:** walls and doors need an `Active` floor on at least one side; the chest and workbench need one under every sample.
- **Edge end inset:** 12 cm, so walls meet at corners. Cells sample 5 cm inside and ignore only their own `Active` perimeter edges (B7).
- **Refund:** 100% by default (`RefundFraction` 1.0).
- **Demolition rules:**
  - A floor takes the walls, doors and furniture it alone carries.
  - A chest holding items is refused ("Empty it first").
  - A station is allowed, and a craft running at it ends with `StationLost`.
- **Keys:** `C` for demolish (instead of `X`).
- **Build bar:** one flat bar, clickable (GameAndUI while in the mode), placeholder initials, description tooltips, visible only in building mode.
- **Interaction:**
  - the forward probe is 75 cm in radius and 75 cm ahead (B9-1);
  - the cone is ±70°, and angles within 1° tie, won by the nearer object;
  - nothing in front means no prompt.

## 4. Tests and Final Baseline

| Group | Tests |
| --- | --- |
| `NullTide.Building.B1` | 9 (8 + `OverlapOnlyVolumes`) |
| `NullTide.Building.B2` | 20 |
| `NullTide.Building.B3` | 4 |
| `NullTide.Building.B4` | 7 |
| `NullTide.Building.B5` | 6 |
| `NullTide.Building.B6` | 7 |
| `NullTide.Building.B7` | 7 |
| `NullTide.Building.B8` | 5 |
| `NullTide.Gameplay.G8` (interaction targeting) | 3 |
| **Full permanent suite** | **258/258** |

Every checkpoint ran:
- a clean build with the editor closed (0 warnings);
- Blueprint compiles with warnings as errors;
- the full suite;
- a Windows cook (0 errors, 2 environment warnings);
- a Content and World Partition audit (`__ExternalActors__` 91 → 94 with the user's three test pickups after B4, `__ExternalObjects__` 3; the only other map change is the `Floor_0` collision in B4);
- from B4 on, a manual rendered PIE by the user.

## 5. Production Assets (`Content/LevelPrototyping/BuildingSystem/`)

- **Definitions:**
  - `Building_WoodFloor`: Wood × 4;
  - `Building_WoodWall`: Wood × 6;
  - `Building_WoodDoor`: Wood × 8 + Scrap × 2;
  - `Building_WoodChest`: Wood × 12;
  - `Building_Workbench`: Wood × 20 + Stone × 10.
- **Pieces:** `BP_BuildFloor`, `BP_BuildWall`, `BP_BuildDoor`, `BP_BuildChest`, `BP_BuildWorkbench`.
- **Input:**
  - `IMC_Building` (`B`);
  - `IMC_BuildingMode` (12 mappings: LMB, cancel keys, `1`–`5`, `R`, `C`);
  - `IA_*` actions.
- **UI:** `W_BuildingHud`, `W_BuildingBar`, `W_BuildingBarSlot`.
- **Materials:** `M_BuildPreview`.
- **Player (`BP_TopDownCharacter`):**
  - `ConstructionComponent` (five buildings);
  - `BuildingPlacementComponent`;
  - `InteractionProbe`.

## 6. Known Limitations (carried into later phases)

- **Persistence:** built pieces are runtime-only and disappear at session end (Phase 7 save/load). Authored pieces would need their keys resolved from their transform.
- **Support:** there is no structural model, and any `WorldStatic` surface counts as support. An outer wall at a terrain drop of more than 25 cm can be refused from the bare side.
- **Single storey:** the cascade is one level deep. Floor-on-floor stacking on odd terrain picks furniture by layer only.
- **Doors:** collision is not swept; a door closing on a pawn relies on depenetration.
- **Ghost and highlights:** they use each piece's default pose and do not apply child-Blueprint overrides of inherited SCS templates.
- **Input prompts:** they read the default mappings of `IMC_Default`, so player remaps are not reflected.
- **Build bar:**
  - key labels assume five buildings, and the hints say "1-5";
  - icons are placeholders.
- **Interaction:**
  - the player turns only by moving;
  - the angle is measured to each actor's root;
  - reach is tunable on the probe.
- **Navigation:** with walls and floors, navigation is PIE-verified only. NPC pathing around built structures is a Phase 6 concern.
- **Performance:** own-edge exclusion scans the whole occupancy map per validation. Harmless at MVP scale.

## 7. Deferred Features

These were not part of the MVP and are kept in `BUILDING_PLAN.md` section 11:
- free placement;
- tiers and upgrades;
- structural support;
- stairs, roofs and multiple storeys;
- campfire and forge;
- a tool requirement;
- repair and durability;
- NPC construction jobs and blueprints (Phase 6);
- multiplayer;
- save/load (Phase 7);
- build menu categories, tabs and icon art;
- undo;
- area or drag placement.

## 8. Next

Phase 6 — Colony, to be planned with the user. The suggested first checkpoint (master plan) is one NPC, one assigned job, navigation to the target and one reusable job action.
