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

## Next Checkpoint

S1 COMPLETE (full automation 271/271, cook PASS, manual PIE 7/7 PASS). Next: S2 base authority, after its detail plan is approved.
