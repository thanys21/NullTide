# Phase 6 — Base & NPCs: Progress

Plan and decisions: `BASE_PLAN.md` (P-1 to P-10 approved 2026-10-21).

## S1 - Cursor Facing

### Baseline and Scope

- **Baseline:** `3de996b` (G9.1). Full automation 267/267, cook 648/7/655.
- **Scope:** `BASE_PLAN.md` S1, approved 2026-10-21. The character faces the mouse (P-1 smooth, 1080°/s), and the cursor is always visible in gameplay (P-2). The backwards-walk look is accepted (P-3).

### Source

| File | Content |
| --- | --- |
| `Player/CursorFacingComponent.h/.cpp` (new area `Player/`) | `UCursorFacingComponent`: pure `ComputeFacingYaw` (feet-plane intersection, dead zone, parallel or away rays refused) and `DeadZone` 40.<br>Ticks only for a pawn controlled by a player controller with a local player (`RefreshTickEnabled` on `BeginPlay` and `ReceiveControllerChangedDelegate`).<br>Pauses while a UI-only screen is open (`IsGameInputActive`: the game viewport does not ignore input; protected virtual seam).<br>Per tick: keep the facing when the cursor is within `DeadZonePixels` (48) of the character's screen position (`IsWithinScreenDeadZone`), else `DeprojectMousePositionToWorld`, aim on the capsule-bottom plane, `SetControlRotation(0, Yaw, 0)`.<br>A tick prerequisite of the character movement component (added on `BeginPlay`, removed on `EndPlay`).<br>Protected virtual `IsLocalPlayerController` seam. |
| `UI/GameInputMode.h/.cpp` | New `RestoreGameplayCursor(PC)`: the controller class's default cursor visibility. |
| `UI/InventoryScreenWidget.cpp`, `UI/StorageScreenWidget.cpp` | `CloseInventory` / `CloseStorage` restore the gameplay cursor instead of hiding it. |
| `Tests/PlayerTestTypes.h` (new) | `UCursorFacingTestComponent`: forces the local-player and input-mode seams (a test world has neither a local player nor a viewport). |
| `Tests/CursorFacingTests.cpp` (new) | `NullTide.Gameplay.G10.*` (4). |

### Assets (Unreal MCP)

- **`BP_TopDownCharacter`:**
  - `CharMoveComp`: `bOrientRotationToMovement` false, `bUseControllerDesiredRotation` true, `RotationRate` yaw 1080 (was orient-to-movement, 640);
  - new `CursorFacing` (`UCursorFacingComponent`, dead zone 40; `DeadZonePixels` keeps the class default 48).
- Compiled with warnings as errors and saved by path. No map change.

### Audit notes (MCP reads)

- The camera is a `SpringArm` with absolute rotation (pitch −50, yaw 45, arm 1400) and no pawn control rotation.
- `IA_Move` builds its directions from `GetWorldRotation(Camera)`.

So the control rotation moves neither the camera nor WASD.

### Tests `NullTide.Gameplay.G10.*` (4)

| Test | Covers |
| --- | --- |
| `FacingMath` | Yaw 0, 90, 180, −90 and 45 through an angled top-down camera ray. The dead zone keeps the facing. A raised pawn aims on its own plane. Horizontal rays and rays pointing away are refused. Screen dead zone: 30 px inside, 100 px outside. |
| `ProductionCharacter` | Exactly one `CursorFacingComponent` (dead zone 40 cm, 48 px). Orient-to-movement off, controller desired rotation on, yaw rate 1080, controller rotation yaw off. `BP_TopDownController` shows the cursor by default. |
| `CursorRestoreAndTickGate` | `RestoreGameplayCursor` shows a hidden cursor again (spawned `BP_TopDownController`). No tick without a controller, nor with a player controller that has no local player; a forced tick changes nothing. |
| `TickWhenPlayerControlled` | Test component on a spawned character: no tick before possession; tick after `Possess` with the movement-component prerequisite in place; no tick after `UnPossess`; tick when possessed before `DispatchBeginPlay`; no tick when the controller is not local. |

### Validation

- **First pass (before the self-review):** build clean; in-editor G10 3/3, full 270/270 (one test carried the live-editor `LogRHICore` environment warning).
- **Final build (editor closed, after the review fixes):** the first attempt failed (`UGameViewportClient::IgnoreInput` is not const, C2662); fixed by holding a non-const viewport pointer. Then `Result: Succeeded`, 0 warnings. `UnrealEditor.modules` names `UnrealEditor-NullTide.dll`. Rebuilt once more after a header comment update (`Result: Succeeded`).
- **Tests (command line, `Automation RunTests NullTide`, `-NullRHI`):** **271/271** Success, 0 failed, `EXIT CODE: 0`. G10 4/4.
- **Cook (`-run=Cook -TargetPlatform=Windows -Map=/Game/TopDown/Lvl_TopDown`):** exit 0, `Success - 0 error(s), 2 warning(s)` (the known environment warnings: `PlatformFileJournal` on D:, the MCP EULA notice). Packages 648 cooked, 7 skipped by platform, 655 total (unchanged).
- **Content and World Partition audit:**
  - modified `BP_TopDownCharacter`;
  - no added asset, no map change;
  - `__ExternalActors__` 94, `__ExternalObjects__` 3 (unchanged).
- **Manual PIE (user, 2026-10-22): 7/7 PASS.** Turning while walking in every direction; the F prompt follows the mouse; crossing the character's body does not flip it; the cursor stays visible after closing inventory and storage, and no turning while they are open; building mode keeps facing, also after a build-bar click; crafting and gathering are not cancelled by turning alone; a world drop lands toward the cursor.

### Self-review (fresh-context review subagent)

| Finding | Classification | Resolution |
| --- | --- | --- |
| P2: the pause gate used viewport focus. A click on building mode's HUD (GameAndUI) moves focus to a widget and would stop facing; a UI-only screen without focus would not pause. | CONFIRMED | Gate on the input mode: `!GameViewport->IgnoreInput()` (UI-only sets it, GameOnly and GameAndUI clear it; `PlayerController.cpp` `SetInputMode`). |
| P2: no test proved the component ticks for a player-controlled pawn. | CONFIRMED | `IsLocalPlayerController` seam, `UCursorFacingTestComponent`, test `TickWhenPlayerControlled`. |
| P3: moving the cursor across the character's body flips the facing (with the angled camera, the ground point under the body lies behind the feet). | CONFIRMED | Screen dead zone `DeadZonePixels` 48 around the projected actor location. |
| P3: the control rotation could apply one frame late when movement ticks first. | CONFIRMED | Tick prerequisite on the character movement component. |
| Building-mode exit should also restore the class-default cursor. | REJECTED WITH EVIDENCE | Building mode restores the cursor state it remembered on entry (the B4 contract, tested at `ConstructionTests.cpp:1654-1668`); in gameplay that state is "shown", so the result is the same. |
| A world drop now spawns toward where the character faces (the cursor), not the movement direction. | Recorded (behaviour change by design) | Added to the PIE checklist. |

S1 COMPLETE (full automation 271/271, cook PASS, manual PIE 7/7 PASS; `0a3e243`).

## S2 - Build Zones

### Baseline and Scope

- **Baseline:** `0a3e243` (S1). Full automation 271/271, cook 648/7/655.
- **Scope:** `BASE_PLAN.md` S2, approved 2026-10-22 (Z-1 to Z-6). Every building must stand inside a base zone's buildable area. The outermost ring of a zone is never buildable; walls and doors fit on every edge of a buildable cell. Zones are authored for the game, any number, and never removed.

### Source

| File | Content |
| --- | --- |
| `Base/BaseZoneComponent.h/.cpp` (new area `Base/`) | `UBaseZoneComponent`: `SizeInCells` (default 15 × 15, min 3 × 3); static `ComputeZoneCells` / `ComputeBuildableCells`; registers its rectangle on `BeginPlay` (fixed while registered), unregisters on `EndPlay`. |
| `Base/BaseZoneSubsystem.h/.cpp` | `UBaseZoneSubsystem`: idempotent `RegisterZone` / `UnregisterZone`, `PurgeStaleZones`; `GetZones` (ordered by rectangle, then path), `IsCellInZone`, `IsCellBuildable`, `FindZoneAt`, `IsPlacementBuildable` (cells: every footprint cell; edges: one bordering cell). |
| `Base/BaseZoneActor.h/.cpp` | `ABaseZoneActor`: `ZoneRoot`, `Zone`, `ZoneBounds` (no collision, no overlaps, no navigation, hidden in game, absolute rotation and scale; sized in `OnConstruction` to the snapped rectangle). Not spatially loaded; never ticks. |
| `Building/BuildingTypes.h` | `EBuildResult::OutsideBuildZone` appended. |
| `Building/BuildingSubsystem.h/.cpp` | `ValidatePlacement`: the zone check right after range for every definition (no zone subsystem counts as outside). |
| `Building/BuildingPlacementComponent.cpp` | `DescribeResult(OutsideBuildZone)`: "Outside the build zone". |
| `Tests/BaseTestTypes.h` (new) | `NullTideBaseTests::SpawnBaseZone` (deferred spawn, size set before construction, `DispatchBeginPlay`) and `FixtureZoneSize` 101 × 101. |
| `Tests/BuildingTests.cpp`, `Tests/ConstructionTests.cpp` | `FB1World` / `FB2World` spawn the fixture zone. No assertion changed. |
| `Tests/BaseZoneTests.cpp` (new) | `NullTide.Base.S2.*` (5). |

### Level (Unreal MCP)

- `Lvl_TopDown`: one `ABaseZoneActor` labelled `BaseZone_Start` at (100, 100, 0), the centre of cell (0, 0) under `PlayerStart` (0, 0, 302).
  - Size 15 × 15: cells −7..7, outline x/y −1400..1600 cm (MCP bounds read), inside `Floor_0` (±2000 cm). Buildable cells −6..6.
  - `bIsSpatiallyLoaded` false (MCP read).
- Saved by the user in the editor (Ctrl+S).

### Tests `NullTide.Base.S2.*` (5)

| Test | Covers |
| --- | --- |
| `ZoneMath` | Zone and buildable rectangles: 15 × 15, 4 × 6 (even sizes extend +X / +Y), 3 × 3 (one buildable cell), negative cells, the minimum clamp; ring cells in the zone but not buildable; 13 × 13 buildable; default size. |
| `Registry` | Registration on `BeginPlay`, idempotent register and unregister, order independent of registration; ring and buildable cells; two touching zones keep a 2-cell strip; `FindZoneAt`; the edge rule (all four sides of the buildable area allowed; inside the ring, on the outer line, between two rings refused); a destroyed zone unregisters; a stale zone is purged; null refused. The plan listed the edge rule under `ZoneMath`; it needs the subsystem, so it lives here. |
| `PlacementRule` | Through `ValidatePlacement` with test definitions: no zone; a buildable cell; a ring cell and corner; a 2 × 1 floor half on the ring and turned inward; walls on all four sides of the buildable area; a wall inside the ring and on the outer line; out of range wins over the zone; outside the zone wins over no ground (and a second zone exposes `NoGround`); the registry unchanged. |
| `BuildOutsideZone` | `CanBuild` and `TryBuild` on a ring cell: `OutsideBuildZone`, nothing spawned, paid or registered, one `OnBuildResult`; one cell inward builds and pays once. |
| `ProductionRules` | `ABaseZoneActor` defaults (not spatially loaded, 15 × 15, the outline has no collision, overlaps or navigation and is hidden in game, no tick); the HUD text; the outline matches the snapped rectangle, also for a rotated actor. |

### Validation

- **Build (editor closed):** `Result: Succeeded`, 0 warnings, first attempt.
- **Tests (command line, `Automation RunTests NullTide`, `-NullRHI`, before the review fixes):** **276/276** Success, 0 failed, `EXIT CODE: 0`. Base S2 5/5. No existing building test needed a change beyond the fixture zone.

### Self-review (fresh-context review subagent)

No P1 finding. Rectangle and edge math, the read-only `ValidatePlacement`, every build path (preflight, revalidation, preview), the enum append (no other switch, no asset reference) and registration lifetime were checked without issues.

| Finding | Classification | Resolution |
| --- | --- | --- |
| P2: `ProductionRules` calls `AActor::GetIsSpatiallyLoaded`, which is declared only `#if WITH_EDITOR` (`Actor.h:2583`). Dev automation tests compile in non-shipping game targets, so the game target would not compile. | CONFIRMED | The assertion is wrapped in `#if WITH_EDITOR`. |
| P2: `BaseZone_Start` not in the level. | REJECTED WITH EVIDENCE | The review read `git status` before the user saved the map: the actor was already placed through MCP (bounds and `bIsSpatiallyLoaded` read back); the save is the user's Ctrl+S step. |
| P3: even sizes are not centred on an actor snapped to a grid corner. | REJECTED (by design) | Z-2 / S2.3: the extra row or column goes to +X / +Y, and the editor outline shows the real area. Documented on `ComputeZoneCells`. |
| P3: no upper size limit (int32 overflow for huge sizes). | CONFIRMED | `MaxSizeInCells` 1000 (`ClampMax` and a clamp in `ComputeZoneCells`); `ZoneMath` covers it. |
| P3: a zone component on a building piece would register on its provisional spawn. | CONFIRMED (future) | Not reachable in S2. The class comment requires a future buildable zone source to register only once Active; recorded as R-S2-8. |
| P3: docs not updated. | REJECTED WITH EVIDENCE | Updated while the review ran (`CLAUDE.md` Base zones section and Tests list, master plan, this file). |
| P3: no test that a registered rectangle stays fixed. | CONFIRMED | `Registry` changes the size and moves the actor after registration; the rectangle and the queries stay the same. |
| P3: `ProductionRules` checks the class default, not the placed instance; `BuildOutsideZone` covers a cell piece only. | Accepted | The placed instance was read through MCP. Edges go through the same `ValidatePlacement` (covered by `PlacementRule`). |

### Validation after the review fixes

- **Build (editor closed):** `NullTideEditor` `Result: Succeeded`, 0 warnings.
- **Game target:** `NullTide Win64 Development` `Result: Succeeded` (compiles `BaseZoneTests.cpp`), 0 warnings. This checks the P2 fix.
- **Tests (command line):** **276/276** Success, 0 failed, `EXIT CODE: 0`.
- **Cook:** exit 0, `Success - 0 error(s), 2 warning(s)` (the known `PlatformFileJournal` and MCP notices). Packages 648 / 7 / 655 (unchanged).
- **Content and World Partition audit (after the user's Ctrl+S):**
  - one new external actor package, `__ExternalActors__/TopDown/Lvl_TopDown/1/X3/K3AJDQ28JPTE97Y62B9VK1.uasset` (`BaseZone_Start`);
  - no other Content change; `Lvl_TopDown.umap` is unchanged;
  - `__ExternalActors__` 94 → 95, `__ExternalObjects__` 3.
- **Manual PIE (user, 2026-10-22): 5/5 PASS.**
  - Building inside the starter zone works as before.
  - The ring and the outside are refused ("Outside the build zone", nothing spent).
  - Walls build along the edge of the buildable area and are refused on the outer line.
  - Demolish is unchanged.
  - The editor outline follows the zone's size.

## Next Checkpoint

S2 COMPLETE (full automation 276/276, cook PASS, manual PIE 5/5 PASS; `b890e2e`).

## S3 - Base UX

### Baseline and Scope

- **Baseline:** `b890e2e` (S2). Full automation 276/276, cook 648/7/655.
- **Scope:** `BASE_PLAN.md` S3, approved 2026-10-22:
  - U-1: a red ring fill;
  - U-2: always visible;
  - U-3: the tag at the top right.

### Source

| File | Content |
| --- | --- |
| `Base/BaseZoneActor.h/.cpp` | 4 ring strips (`RingSouth/North/West/East`): engine cube, no collision, overlaps, navigation or shadow, absolute transform. Laid in `OnConstruction` from `ComputeRingStrips` (2 cm thick, 1 cm above the actor). `RingMaterial` defaults to `MI_BaseZoneRing` (ConstructorHelpers, the project's existing pattern). |
| `Base/BaseZoneComponent.h` | `ZoneName` (FText, default "Base"). |
| `Base/BaseZonePresenceComponent.h/.cpp` (new) | `UBaseZonePresenceComponent`: local player gate (seam), 0.25 s looping timer while possessed by a local player, `RefreshZone` (`FindZoneAt`), `OnBaseZoneChanged` once per change (null on leaving or losing the local player), the tag created on first entry, placed at the top right (position first: `SetPositionInViewport` resets anchors), collapsed outside, removed on `EndPlay`. |
| `UI/BaseZoneTagWidget.h/.cpp` (new) | `UBaseZoneTagWidget`: `ZoneNameText` (BindWidget), hit-test invisible, `SetZoneName`. |
| `Tests/BaseZoneTestTypes.h` (new) | `UBaseZonePresenceTestComponent` (local gate forced, viewport-free tag), `UBaseZoneTestListener`. |
| `Tests/BaseZoneTests.cpp` | `NullTide.Base.S3.*` (3). |

### Assets (Unreal MCP)

- `Content/LevelPrototyping/BaseSystem/Materials/MI_BaseZoneRing`: parent `M_BuildPreview`, `Color` (0.95, 0.15, 0.1), `Opacity` 0.35 (the ghost uses 0.4). It was created before the native code that references it.
- `Content/LevelPrototyping/BaseSystem/UI/W_BaseZoneTag`: parent `UBaseZoneTagWidget`.
  - `TagBorder` (dark, alpha 0.65, padding 14 × 6) holds `ZoneNameText` (Roboto Bold 18, letter spacing 80, light green, default "Base").
  - Compiled.
- `BP_TopDownCharacter`: new `BaseZonePresence` (`TagWidgetClass = W_BaseZoneTag`). Compiled with warnings as errors.
- No map change: `BaseZone_Start` takes the new strips and its name from class defaults.

### Tests `NullTide.Base.S3.*` (3)

| Test | Covers |
| --- | --- |
| `RingGeometry` | For 5 × 5, 3 × 3 and 4 × 6 zones, the 4 strip boxes cover each ring cell exactly once and nothing else. On a spawned zone (also rotated): the strip bounds match, 1-3 cm above the actor, visible in game, `MI_BaseZoneRing`. No collision, overlaps, navigation or shadow; visibility and pawn traces pass through. |
| `PresenceAndTag` | No timer without a controller or without a local player. Entering a zone: one change and the zone's name. The tag is at the top right (anchors and alignment (1, 0)). Moving within a zone (ring included): nothing. A touching zone: one change and its name. Leaving: null and collapsed. Re-entering reuses the tag. Unpossession stops the timer and forgets the zone. `EndPlay` releases the tag. |
| `ProductionWiring` | `BP_TopDownCharacter` has exactly one `BaseZonePresence` with `W_BaseZoneTag`. `W_BaseZoneTag` binds `ZoneNameText`. The default zone name is "Base". The default ring material is `MI_BaseZoneRing` and is red. |

### Validation

- **Build (editor closed):** `Result: Succeeded`, 0 warnings. The editor started with no "Failed to find" constructor message.
- **Tests (in-editor, MCP `StartsWith:NullTide.Base`, before the review fixes):** 8/8 (S2 5, S3 3), 0 warnings.

### Self-review (fresh-context review subagent)

No P1 finding. The reviewer traced in engine source:
- the strip geometry;
- absolute transforms on a rotated actor;
- the placed `BaseZone_Start` taking its new subobjects from the class defaults;
- traces, the probe and navigation passing through the strips;
- timer and widget lifetime;
- broadcast semantics;
- the top-right slot math (offsets (−24, 24), auto-size);
- the game-target compile.

All were found clean.

| Finding | Classification | Resolution |
| --- | --- | --- |
| P2: a zone destroyed under the player: its weak pointer reads null like "no zone", so the tag stays and no leave event fires (also on unpossession). Not reachable today (zones are never removed). | CONFIRMED | `bInZone` tracks the last lookup; "was in a zone, now none" is a change in `RefreshZone` and `RefreshTimer`. `PresenceAndTag` destroys a zone under the player. |
| P3: the tag stays bound to the first local controller after a controller swap. | CONFIRMED | `RefreshTimer` drops a tag owned by another player controller and rebuilds it for the new one. |
| P3: silent failure without `TagWidgetClass` or when `AddToPlayerScreen` fails (and the widget was cached). | CONFIRMED | A warning is logged and nothing is cached, so the next zone change retries. |
| P3: the test seam never calls `AddToPlayerScreen`. | Accepted | The reviewer traced the production slot order in engine source; the real screen is a PIE step. |
| P3: the first broadcast can happen during `BeginPlay`, before Blueprint listeners bind. | Accepted (documented) | Listeners read `GetCurrentZone()` when they bind; no Blueprint listens today. |
| P3: the ConstructorHelpers hard reference to `MI_BaseZoneRing`. | Accepted | This is the project's pattern. The reviewer confirmed that the World Partition cook reruns construction, so the strips' material is a real cook dependency. The asset existed before the code. `BaseSystem/` is committed with the code. |
| P3: if `M_BuildPreview` disables the depth test, the ring would draw through walls and the character. | REJECTED WITH EVIDENCE | PIE: the ring does not draw over the character or walls. |
| P3: restart required for the layout change. | REJECTED WITH EVIDENCE | It was a full build with the editor closed, then a restart. |

- **Manual PIE (user, 2026-10-22/23, before the review fixes, which touch only edge cases): 5/5 PASS.**
  - A red ring around the starter zone.
  - The ghost is red on the ring and green inside.
  - "Base" shows at the top right inside the zone and disappears outside.
  - The tag stays while inventory or a chest is open.
  - The ring does not draw over the character or walls.

### Validation after the review fixes

- **Build (editor closed):** `NullTideEditor` `Result: Succeeded`, 0 warnings.
- **Game target:** `NullTide Win64 Development` `Result: Succeeded`.
- **Tests (command line):** **279/279** Success, 0 failed, `EXIT CODE: 0`; Base S3 3/3.
- **Cook:** exit 0, `Success - 0 error(s), 2 warning(s)` (the known environment notices). Packages **650 / 7 / 657**: +2, which are `MI_BaseZoneRing` and `W_BaseZoneTag` (the cook writes to the Zen project store, so there are no loose files to list).
- **Content and World Partition audit:**
  - modified `BP_TopDownCharacter`;
  - new `BaseSystem/Materials/MI_BaseZoneRing` and `BaseSystem/UI/W_BaseZoneTag`;
  - no map change: `__ExternalActors__` 95, `__ExternalObjects__` 3.

## Next Checkpoint

S3 COMPLETE (full automation 279/279, cook PASS, manual PIE 5/5 PASS). Next: S4 NPC foundation, after its detail plan is approved.
