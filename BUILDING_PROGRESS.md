# Building Progress

Plan and design decisions: `BUILDING_PLAN.md`.

## Status

| Checkpoint | Status |
| --- | --- |
| B0 Architecture audit and design | APPROVED (user) |
| B1 Data model, grid coordinates, occupancy and placement validation | COMPLETE / APPROVED (8/8; full automation 198/198; cook PASS; no manual PIE required) |
| B2 Construction authority | IMPLEMENTED AND VALIDATED — awaiting user acceptance (20/20; full automation 219/219; cook PASS; no manual PIE required). Design approved 2026-10-14 (D1–D8, D3 option (a)). |
| B3–B6 | NOT STARTED |

## B1 - Data Model, Grid Coordinates, Occupancy and Placement Validation

### Baseline and Scope

- Baseline: `bbfcc08` (Phase 4 Crafting complete and pushed). Full automation 190/190.
- Scope (user-approved): data model, grid coordinate contract, occupancy registry and read-only placement validation logic.
- Out of scope: construction transaction, cost consumption, spawning by an authority, input, UI, door, demolition, production assets, map changes.
- **COMPLETE / APPROVED** by the user after the final audit below. No manual PIE required. Committed together with the approved B0 documentation.

### Source Files (all new)

- `Public/Building/BuildingTypes.h`:
  - `EBuildingCategory`, `EBuildingPlacementMode`, `EBuildingOccupancyCategory`, `EBuildingEdgeAxis`, `EBuildingPieceState`;
  - `EBuildResult`, B1 codes only: `Success, InvalidDefinition, InvalidPlacement, OutOfRange, NoGround, TooSteep, Uneven, Occupied, Blocked, BlocksBuilder`. B2 appends its codes.
  - `FBuildingOccupancyKey` (hashable), `FBuildingReservationHandle`, `FBuildingPlacement`, `FBuildingPlacementResult`.
- `Public/Building/BuildingDefinition.h` + `Private/Building/BuildingDefinition.cpp`:
  - abstract, const, data-only definition;
  - `ValidateBuilding`, `IsDataValid`, `GetOccupancyCategory`;
  - `ClassHasPieceComponent` (native default subobjects and Blueprint SCS components);
  - `ResolveValidBuilding`.
- `Public/Building/BuildingPieceComponent.h` + `Private/Building/BuildingPieceComponent.cpp`:
  - definition, state, placement, keys, `bPlayerBuilt`;
  - `InitializePiece` is refused while the piece is registered;
  - registers on `BeginPlay` only when `Active` with keys; unregisters on `EndPlay` and `OnComponentDestroyed`.
- `Public/Building/BuildingPieceActor.h` + `Private/Building/BuildingPieceActor.cpp`:
  - abstract thin host (unscaled `PieceRoot`, `PieceMesh` with `BlockAllDynamic`, `BuildingPiece`);
  - `SetProvisional`: hidden, no actor collision, navigation removed; it restores exactly the primitives that affected navigation before.
- `Public/Building/BuildingSubsystem.h` + `Private/Building/BuildingSubsystem.cpp` (`UWorldSubsystem`):
  - constants;
  - grid math;
  - the occupancy registry (the only writer);
  - `AuditRegistry`;
  - `ValidatePlacement`.
- Tests:
  - `Private/Tests/BuildingTestTypes.h`: test piece actor (200×200×10 slab), test floor, wide floor, wall, door, furniture definitions, reservation owner.
  - `Private/Tests/BuildingTests.cpp`.
- No existing file changed except the docs. No Inventory, Crafting, Storage, Gathering, interaction or UI code was touched.

### Constants (approved)

| Constant | Value |
| --- | --- |
| Cell size / origin | 200 cm / world (0, 0) |
| Layer height | 50 cm (`Layer = RoundToInt(SurfaceZ / 50)`) |
| Max unevenness / slope | 25 cm / 30° |
| Floor thickness / wall height / wall thickness | 10 / 250 / 20 cm |
| Default build range | 600 cm (2D, builder to piece center) |
| Sample inset / collision clearance / builder clearance | 5 / 2 / 5 cm |
| Surface trace | hint ± 300 cm |

### Grid Contract (as implemented)

- `WorldToCell` uses floor division (`FloorToInt32(X / 200)`). Negative coordinates are correct, and an exact boundary belongs to the positive cell.
- Edge snapping:
  - the nearest side of the containing cell, with ties resolved South, West, North, East;
  - canonical keys: south `(X, ix, iy)`, north `(X, ix, iy+1)`, west `(Y, ix, iy)`, east `(Y, ix+1, iy)`. A shared edge is one key from both sides.
- Rotation:
  - cells: yaw `Rotation × 90`, footprint W×H swaps on odd turns, occupied cells extend +X/+Y from the anchor;
  - edges: yaw 0 (X) or 90 (Y), plus 180 for odd rotation, with the same key for both facings.
- The transform is always recomputed from the key, rotation and resolved surface; the validated result never trusts the caller's transform, surface or layer.

### Occupancy Registry (as implemented)

- **Storage:** `Occupancy` (key → occupant: reserved by a reservation id, or occupied by a piece), `Reservations` (id → owner and keys), `Pieces` (`FObjectKey` → piece and keys).
- **Identity:** holders are tracked with `FObjectKey` plus a weak pointer, so a piece that is being destroyed or is already garbage can still be unregistered correctly. Handle ids come from a monotonic `int64` counter starting at 1 and are never reused.
- **`ReserveKeys(Owner, Keys)`:**
  - all-or-nothing;
  - rejects an invalid owner, empty or duplicate keys, and any reserved or occupied key;
  - purges stale entries first, so dead holders never block.
- **`ReleaseReservation(Handle, Owner)`:** only the owning object can release; stale, unknown and foreign handles release nothing.
- **`CommitReservation(Handle, Owner, Piece)`:**
  - owner match, live owner, valid piece not already registered, and the piece's keys set-equal to the reserved keys;
  - converts exactly those keys and consumes the handle, so a second commit fails.
- **`RegisterActivePiece(Piece)`:** all-or-nothing; idempotent for the same footprint; a registered piece cannot claim a second footprint. `InitializePiece` is also refused while registered.
- **`UnregisterPiece(Piece)`:** removes only keys this piece holds; safe for never-registered or destroyed pieces. Called from the piece's `EndPlay` and `OnComponentDestroyed`, so destroyed actors, component destruction and world teardown leave no occupancy.
- **`PurgeStaleEntries`:** drops reservations with dead owners, records of dead pieces and any orphaned entry.
- **`AuditRegistry`** reports:
  - reserved keys of unknown reservations;
  - keys missing from their reservation;
  - dead reservation owners;
  - impossible ids;
  - occupied keys of unregistered or destroyed pieces;
  - reservations or pieces that lost a key;
  - a piece whose current key list differs from its registration.
- **Categories:** `Floor`, `Edge` (walls and doors share it) and `Furniture` are separate key spaces; the layer is part of the key.

### Placement Validation (as implemented)

`ValidatePlacement(Definition, Requested, Builder, IgnoreReservation, IgnoreActor)` is read-only and deterministic. It checks, in order:

1. **Definition:** a valid concrete definition → otherwise `InvalidDefinition`.
2. **Placement shape:** mode equals the definition's; an edge axis only for edges; rotation 0–3; a builder exists → otherwise `InvalidPlacement`.
3. **Build range:** horizontal distance from the builder to the piece center (from the requested hint) ≤ 600 cm → otherwise `OutOfRange`.
4. **Support surface:**
   - Pawn-channel line traces from hint +300 to −300 at the samples: cells use the four inset corners plus the center; edges use both ends (inset) and the midpoint on both sides of the edge (±8 cm), so a wall on a floor's boundary sees that floor.
   - Any miss → `NoGround`.
   - A sample hitting a solid actor that is not support (support = `WorldStatic`, or an `Active` Foundation piece) → `Blocked`, naming that actor.
   - Any normal below cos 30° → `TooSteep`.
   - Spread > 25 cm → `Uneven`.
   - The surface Z is the highest sample; the layer and transform are recomputed from it.
5. **Occupancy:** any key reserved or occupied, except the ignored reservation → `Occupied`.
6. **World and pawn collision:**
   - A box from 1 cm above the support surface to the piece height (floor 10 cm; wall 250 cm, shortened by half a wall thickness at each end so walls meeting at a corner don't block), shrunk by 2 cm.
   - A Pawn-channel overlap; only blocking overlaps count (overlap-only triggers such as interaction bounds or pickup spheres don't block).
   - The builder and `IgnoreActor` are excluded.
   - The first blocker by actor name → `Blocked`.
   - **The ground is never excluded by identity:** it cannot overlap because the box starts above the highest support sample, while an unrelated blocker inside the volume is still found.
7. **Builder safety:** the builder's root collision overlapping the box inflated by 5 cm → `BlocksBuilder`.

### Validation

- `NullTideEditor Win64 Development` **clean** build (Clean, then Build, editor closed): succeeded, 0 compiler warnings.
- Focused `NullTide.Building.B1`: **8/8** passed.
  - `DefinitionValidation`:
    - five valid fixtures;
    - 17 invalid cases with diagnostics: actor class null, abstract or without a piece component; footprint; edge footprint; category and mode mismatch both ways; rotation step; empty, null, abstract, invalid-data, zero-quantity and duplicate cost; refund below 0 and above 1; negative range;
    - piece-component detection; `ResolveValidBuilding`; `IsDataValid`.
  - `GridCellMath`: boundaries, negatives (-0.001 → -1, -200.001 → -2), height independence, round trips over 49 cells, cursor snapping.
  - `GridEdgeSnapping`: four sides, three tie cases, shared edges from both sides (X and Y), a point exactly on an edge line, four negative-cell sides, edge centers, wall cursor snapping.
  - `RotationMapping`: normalization of -1, 5 and -4; 2×1 footprint cells and centers for all four rotations; exact yaw; edge yaw 0/90/180/270; facing-independent edge key.
  - `LayerMapping`: 0, 10 cm floor top, 24.9 vs 25 cm, 200 and 210 cm plateau, -30 cm; same column on two layers gives different keys.
  - `OccupancyLifecycle`:
    - invalid requests; all-or-nothing; foreign and stale release, including a stale handle against a newer reservation of the same key; unique ids;
    - foreign and mismatched commit refused; commit; double commit refused; idempotent re-registration; re-initialization refused;
    - destroyed never-begun piece; `BeginPlay` registration (Active only); component destruction; actor `EndPlay`;
    - dead piece and dead owner with no callbacks: counted free, reported by the audit (3 issues), purged (2 keys), then reusable;
    - `AuditRegistry` clean after every mutation sequence.
  - `OccupancyCategories`: floor plus wall on its edge coexist; a door on the walled edge conflicts (same canonical key from opposite sides); furniture separate; a second layer coexists for floors and edges.
  - `PlacementValidation` (transient world with ground, a 200 cm platform and a 35° slab):
    - success on the ground (layer 0) and the platform (layer 4); hint not trusted;
    - invalid definition (null, abstract); invalid placement (mode, axis, rotation, no builder);
    - `OutOfRange`, `NoGround`, `TooSteep`, `Uneven` (straddling the platform edge);
    - reservation conflict and the ignore handle;
    - a WorldStatic crate inside the cell → `Blocked` (the crate, not the ground); a blocker in the next cell does not block; a physics body and another pawn block; the cell under the builder → `BlocksBuilder`;
    - a wall on the ground; a wall on a floor's boundary (surface 10 cm, layer 0, not blocked by the floor);
    - floor on floor → `Occupied`; floor next to floor → `Success`; a non-Active floor does not support (`Blocked`);
    - read-only and deterministic; audit clean.
- Regressions in the same full run: Crafting 69, Inventory 53, Gameplay 50, ResourceGathering 18, all passed.
- Full permanent automation `NullTide`: **198/198** (190 + 8 B1).
- Windows cook (editor closed): `Done!`, `Success - 0 error(s), 2 warning(s)` (environment only); 622 cooked, 7 platform-skipped, 629 total. Unchanged from C8C, as expected: B1 adds no assets.
- Content and World Partition audit: no Content change; `__ExternalActors__` 91 and `__ExternalObjects__` 3, unchanged; no map change.
- Rendered PIE: not required (no gameplay-visible behaviour).

### Development Notes

- First test run: the `DefinitionValidation` duplicate-cost case did `Cost.Add(Cost[0])` (an element referencing its own array), which hit a `TArray` assertion. The test now copies the element first (the same pitfall as in C1).
- Second run: `OccupancyLifecycle` expected `PurgeStaleEntries` to free two keys, but `ReserveKeys` had already purged the dead piece (by design: reservations purge stale entries first). The test now kills both holders together before checking the audit and the purge. No production change.
- `ClassHasPieceComponent` needed the non-const `GetDefaultSubobjects` (compile fix).

### Final Audit (before commit)

- **Evidence:**
  - Every B1 source and test file was last modified before the clean build.
  - The editor DLL from the clean build is newer than every source file.
  - The full-suite log (198/198, B1 8/8) and the cook log (`Done!`, 0 errors, 622/7/629) were produced after that build.
  - Only documentation changed after the validation runs, so no rerun was needed.
- **Boundaries:**
  - B1 source has no reference to `UConstructionComponent`, `AvailableBuildings`, `UInventoryComponent` or inventory transactions.
  - The only inventory include is the generic `FInventoryDefinitionQuantity` type (`Inventory/InventoryTypes.h`) used for cost data; the inventory does not depend on Building.
  - No tracked source, Content or Config file differs from `bbfcc08`.
- **Code review** (handle identity, owner validation, all-or-nothing reservation, idempotent registration, stale-handle release, single commit, `EndPlay`/destruction cleanup, stale purge, canonical edges, negative coordinates, layers, deterministic snapping, unrelated blockers, overlap-only volumes, no change to existing authorities): no defects found.
- **Test gap (not a defect):** overlap-only volumes have no dedicated test. The code handles them by using a blocking-only Pawn-channel trace for support and by keeping only `bBlockingHit` overlaps. Suggested for B2: a test with an overlap-only box in the footprint expecting `Success`.

### Known Limitations and Risks Carried Forward

- **Static props as support:** any `WorldStatic` surface counts as support. A floor can therefore sit on top of a large flat `WorldStatic` prop if it covers every sample. Production resource nodes and props must be checked in B3/B4 PIE; a support whitelist (for example landscape or level geometry only) can be added if needed.
- **Bumps between samples:** a terrain bump between surface samples that rises above the highest sample would block a floor (conservative); acceptable on the current flat map.
- **Authored pieces:** an `Active` piece with no keys does not register on `BeginPlay`. Authored building pieces in a level would need their keys resolved from their transform (not needed until Phase 7 save/load or authored bases).
- **NPC builders:** the builder is any actor; range and builder safety use its location and root collision.

### B2 Design Gate — APPROVED (user, 2026-10-14)

Design review only, against baseline `0681261` (full suite 198/198). No code or assets were changed. The approved design is in `BUILDING_PLAN.md` section 5.2.

**User decision:** D1, D2 and D4–D8 approved, and D3 option (a): a paid piece destroyed after payment reports `Success`, its weak `Piece` reads invalid, and there is no refund. Sections 4.1, 5, 7, 8 and 10 of the plan were rewritten to the approved design; the B0 recovery chain is marked superseded.

- **Audit findings:**
  - `OnInventoryChanged` fires synchronously inside the inventory's mutation guard, after the deduction.
  - Failed transactions cost 0 revisions and 0 events.
  - `CanApplyDefinitionTransaction` does not report `Busy`.
  - The G6 pickup class is file-local, so building cannot spawn pickups without an inventory refactor.
  - The B1 registry API is sufficient: reservations are owner-held, `CommitReservation` refuses a dead owner, and `ReleaseReservation` works by object key.
- **Finding:** with the B0 order (pay, then finalize), listeners run in a post-payment window. Any failure there needs compensation that can itself fail, and Phase 5 cannot make that durable.
- **Recommendation (D1):** activate the piece (commit the reservation, make it visible and collidable) **before** the single atomic cost transaction. The payment becomes the commit point with nothing fallible after it. Every earlier failure is a fail-closed rollback that never touches the inventory.
- **Removed if approved:** the respawn → refund → pickup chain, its result codes and the G6 helper extraction. No pending-compensation record is needed.
- **Remaining trade-offs (D2, D3):**
  - **D2:** an unpaid `Active` piece is observable by engine callbacks within one call.
  - **D3 (revised):** a paid piece destroyed after payment reports `Success` with an invalid weak `Piece` and no refund. Destruction during payment also logs a Warning and sets a diagnostic. A distinct result code is the alternative.
- **D8 (revised):** Option A is not a pre-approved fallback. If D1 is rejected, B2 returns to design review.
- **Tests specified:** 20 `NullTide.Building.B2.*` tests plus the B1 regression `OverlapOnlyVolumes`. Faults are injected through fixture actors, two protected virtual seams and inventory listeners; there are no debug switches.

**Independent review round 1 (Codex, against `ab6a1d2`; resolutions in `BUILDING_PLAN.md` 5.2.14):**
- All 5.2.1 source facts confirmed.
- **CONFIRMED:**
  - **R1 (P1):** B2 requires an `ABuildingPieceActor` piece class, checked at preflight.
  - **R2 (P1):** the step 7 integrity check also covers transform, provisional, hidden and collision.
  - **R3 (P1):** the result `Piece` is a weak reference, with destruction-during-result tests.
  - **R4 (P2):** the guarantee wording covers B2's own mutations only.
- **D3:** NEEDS USER DECISION.
- **D8:** revised.
- **Evidence obtained (read-only MCP and source scan):**
  - no C++ overlap or hit handlers;
  - `BP_PickUpItem` only reacts to actors with an `InventoryManagerComponent`;
  - `PlayerInteractionComponent` only tracks `BPI_Interactable` actors in range (relevant to the B5 door);
  - the storage and resource-node overlap events are empty;
  - `BP_Workbench` has none.
  - No current content reacts to an unpaid active piece by moving items, actors or collision.
- No code, assets, tests or Git state changed.

**Independent review round 2 (Codex; resolutions in `BUILDING_PLAN.md` 5.2.15):**
- Round 1 status before this round's fixes: R1 and R4 CLOSED. R2, R3 and D8 needed further work.
- **R2-1 (P1, CONFIRMED):** the result is built after the cost transaction returns. Destruction during payment gives a Warning and a diagnostic. Destruction during `OnBuildResult` only invalidates the weak `Piece`.
- **R2-2 (P2, CONFIRMED):** step 7 checks every key's occupant through `GetOccupyingPiece` and `GetOccupancyKeys`.
- **R2-3 (P2):** the navigation recheck is explicitly deferred to B5 as a D2 limitation.
- **R2-4 (P2, CONFIRMED):** Option A is labelled an evaluated alternative, not a fallback.
- **D2 evidence completed:** `Lvl_TopDown` and its Level Blueprint, `BP_TopDownCharacter`, and all external actor and object packages have no overlap or hit handlers.
- **User direction:** instant placement on the grid with the cursor; materials are deducted only when the build succeeds.
- D3 is still open. D1–D8 await formal approval. No code, assets, tests or Git state changed.

B1 stays COMPLETE. G6, the inventory and B1 source are unchanged.

## B2 - Construction Authority (activate first, pay last)

### Baseline and Scope

- **Baseline:** `6460ef6` (approved B2 design docs). B1 source unchanged since `0681261`. Full automation 198/198.
- **Scope (user-approved, `BUILDING_PLAN.md` 5.2.12):** `UConstructionComponent`, the appended `EBuildResult` codes, test fixtures and 20 `NullTide.Building.B2.*` tests, plus the B1 regression `OverlapOnlyVolumes`.
- **Out of scope:**
  - assets; `BP_TopDownCharacter` (B3);
  - input or UI (B4);
  - demolition (B6);
  - G6 extraction;
  - any change to inventory, crafting, storage or interaction source.

### Source Files

- **New:** `Building/ConstructionComponent.h/.cpp`:
  - `AvailableBuildings` (`EditDefaultsOnly`, plus `IsDataValid`);
  - `GetAvailableBuildings`, `IsBuildingAvailable`, `CanBuild`, `TryBuild`, `IsBuildInProgress`, `GetInventory`;
  - `OnBuildResult`;
  - `FBuildingConstructionResult` (code, weak `Piece`, resolved placement, diagnostic);
  - the protected virtual seams `SpawnProvisionalPiece` and `ActivatePiece`;
  - `BeginPlay`/`EndPlay` tracking of `bEndPlayed`.
- **Changed:** `Building/BuildingTypes.h` appends `BuildInProgress`, `NotAvailable`, `InvalidInventory`, `InsufficientResources`, `SpawnFailed`, `TransformMismatch`, `BuildInterrupted` and `ResourcesRejectedRolledBack` (existing values not reordered).
- **Tests:**
  - `Tests/BuildingTestTypes.h` (fixtures):
    - a plain actor with a piece component;
    - self-destroying, drifting and blocker-spawning pieces with their definitions;
    - `UConstructionTestComponent` (seam hooks);
    - `UConstructionTestListener`.
  - New `Tests/ConstructionTests.cpp`.
  - `Tests/BuildingTests.cpp`: B1 `OverlapOnlyVolumes`.
- **Additions beyond the 5.2.12 list (harmless, recorded):** `IsBuildingAvailable`, `IsBuildInProgress`, `GetInventory`, `BeginPlay`.

### As Implemented

- **`TryBuild` (5.2.3):**
  1. guard;
  2. validate: definition, registration, an `ABuildingPieceActor` piece class, the component not ended, an active inventory, B1 `ValidatePlacement`, cost preflight;
  3. `ReserveKeys`;
  4. deferred `AlwaysSpawn`, `InitializePiece(Provisional, player-built)` and `SetProvisional(true)` before `FinishSpawning`;
  5. exact transform check (0.1 cm, 0.01°, scale) and `ValidatePlacement` ignoring the own reservation and the piece, with identical keys, layer and transform;
  6. `SetPieceState(Active)`, `CommitReservation`, `SetProvisional(false)`;
  7. integrity check: valid, Active, exact key set, `GetOccupyingPiece` per key, transform, not provisional or hidden, collision on, builder not ended, inventory active;
  8. one `TryApplyDefinitionTransaction(Cost, {})`;
  9. the result built once after the payment returns, broadcast once inside the guard, returned.
- **Fail-closed rollback (5.2.4):** provisional again, `UnregisterPiece`, `ReleaseReservation` (both by object key), `Destroy`. A piece that survives `Destroy` logs an Error. The rollback never touches the inventory.
- **Cost preflight mapping:**
  - `InsufficientQuantity` → `InsufficientResources`;
  - `NotInitialized` or `Busy` → `InvalidInventory`;
  - rejected cost data → `InvalidDefinition`.
- **D3 (a):** a piece destroyed during payment reports `Success`, with an invalid weak `Piece`, the "destroyed during payment" diagnostic, a Warning and no refund. Destruction during `OnBuildResult` only invalidates the weak `Piece`.

### Validation

- **Build:** clean Development editor build with the editor closed. 0 errors, 0 compiler warnings. The base `UnrealEditor-NullTide.dll` was rebuilt, not a hot-reload module. An earlier build with the editor open produced a hot-reload `-0001` module, which was discarded; all results below come from the clean build.
- **Focused:** `NullTide.Building` 29/29 (B1 9/9 including `OverlapOnlyVolumes`, B2 20/20), command line `-NullRHI`.
- **Full suite:** `NullTide` **219/219**:
  - Building 29;
  - Crafting 69;
  - Gameplay 50;
  - Inventory 53;
  - ResourceGathering 18.

  No warnings, errors or ensures during the run. The expected D3 Warning is asserted by `PieceDestroyedAfterPayment`.
- **Windows cook** (editor closed): `Done!`, 0 errors, 2 environment warnings (NTFS journal on D:, MCP EULA notice). Packages 622 cooked, 7 skipped by platform, 629 total (unchanged).
- **Content and World Partition audit:** no Content, Config or map change. `__ExternalActors__` 91 and `__ExternalObjects__` 3, unchanged.
- **Manual PIE:** none required (no asset, input or UI).

### Tests `NullTide.Building.B2.*` (20)

Each test asserts:
- the result code;
- the exact revision, inventory-event, build-result and Wood deltas;
- the live piece count;
- a free cell and a clean `AuditRegistry` with no reserved keys after every failure.

`BuildInterrupted` cases also assert the diagnostic, which proves the branch.

| Test | Covers |
| --- | --- |
| `Success` | `CanBuild` read-only, exact cost +1 / 1 / 1, piece Active, player-built, registered, visible, collidable and owned by the builder; a 90° build at the exact yaw |
| `ValidationFailures` | `InvalidDefinition` (null, abstract), `NotAvailable`, `OutOfRange`, `Occupied` (existing piece, another owner's reservation), `Blocked`; `CanBuild` agrees |
| `InventoryPreflight` | `InsufficientResources`, `InvalidInventory` (no inventory, legacy mode) |
| `SpawnFailure` | Piece destroyed while spawning; seam returns null |
| `TransformMismatch` | Piece drifts in `OnConstruction` |
| `CollisionChangedDuringSpawn` | Blocker spawned by the piece → `Blocked` at revalidation |
| `BlockerAfterPreview` | Green preview, then a blocker → `Blocked` |
| `InventoryBusyAtCommit` | Build from the inventory's own broadcast → `Busy` → `ResourcesRejectedRolledBack` |
| `ResourcesChangedDuringActivation` | Seam removes Wood → payment short → rollback; only the seam's +1 / 1 |
| `ActivationInterrupted` | Piece destroyed after activation; builder ended after activation; builder ended before the commit (registry refuses) |
| `PieceDestroyedAfterPayment` | D3 Warning, diagnostic and no refund, seen identically by the listener and the caller |
| `PieceDestroyedDuringBuildResult` | Order-independent: one listener sees a valid piece and destroys it; the other listener and the caller see it invalid, with no diagnostic |
| `BuilderEndedDuringPayment` | `Success`, piece stays Active, guard released, one result, +1 / 1 / 1 |
| `ActivationCallbackAltersPiece` | Moved, hidden, collision disabled, made provisional, unregistered → `BuildInterrupted` with the matching diagnostic |
| `UnsupportedPieceClass` | B1-valid plain actor → `InvalidDefinition` in `CanBuild` and `TryBuild` |
| `Reentrancy` | Nested `CanBuild`/`TryBuild` from `OnInventoryChanged` and `OnBuildResult` → `BuildInProgress`, no extra events |
| `TwoBuilders` | Second builder `Occupied` on the reserved and on the activated cell; succeeds elsewhere |
| `ProvisionalInvariants` | Hidden, no collision, no navigation-relevant primitive, unregistered, key reserved, no occupant; navigation restored after activation |
| `RetryAfterFailure` | After Insufficient, Occupied, Blocked at validation and at revalidation, TransformMismatch, SpawnFailed, Interrupted and Rejected, the same cell succeeds |
| `ExactlyOnce` | Mixed sequence: results = non-nested calls (5), revisions = successes (2) |

### Self-Review (including a fresh-context review pass)

The fresh-context review (a read-only review subagent) found no P0 or P1 defect. Every step and rollback path matches 5.2.3 and 5.2.4. It found no path that charges twice, and no path that charges without an Active registered piece outside the accepted D3 window.

P2 findings and their resolution (fixed before the clean build, then the full validation above was run):

| Finding | Resolution |
| --- | --- |
| `bEndPlayed` never reset; preflight ignored it | **CONFIRMED.** Reset in `BeginPlay`; preflight refuses an ended component (`InvalidPlacement`). |
| `GetAvailableBuildings` listed unconstructible buildings | **CONFIRMED.** Filtered by the `ABuildingPieceActor` contract (shared helper, also used by preflight and `IsDataValid`). |
| Coarse cost-preflight mapping | **CONFIRMED.** Mapped per code (see above). |
| Result struct placed in the B1 file | **CONFIRMED.** Moved to `ConstructionComponent.h`, as 5.2.12 says. |
| Missing assertions | **CONFIRMED.** Added: deltas in `BuilderEndedDuringPayment`, branch diagnostics, a rotated build, and more failure kinds in `RetryAfterFailure`. "No Warning" in `PieceDestroyedDuringBuildResult` is verified by code inspection (no log on that path; the full-run log has no warnings). |
| Blueprint-added primitives, child actors and scaled roots are not covered by native tests | **Deferred to B3** as an asset contract (see Known Limitations). |
| Rollback acts on whatever actor a seam returns | **REJECTED:** only a test seam could return a foreign actor; production always spawns a fresh one. |

### Known Limitations Carried Forward

- **D2 (accepted):** callback-created blockers and per-primitive navigation are not re-checked after activation.
- **B3 piece Blueprint contract.** Inspect each piece read-only through MCP before review:
  - Primitives that a Blueprint adds are masked only by actor-level hidden and collision. Custom navigable geometry marked `EvenIfNotCollidable` would leak navigation.
  - `ChildActorComponent` children are separate actors and are not provisional. They would also block their own piece at revalidation.
  - The piece root must be unscaled, because the exact-transform check compares scale.
- **Wiring:** the component is not on `BP_TopDownCharacter` yet (B3).

## Next Checkpoint

B2 is implemented and validated. It waits for the user's acceptance and commit approval.

After that, B3 (Wooden Floor end to end: assets through MCP, `UConstructionComponent` on `BP_TopDownCharacter`) needs its own plan and approval. B3 must apply the piece Blueprint contract listed under the B2 known limitations. Phase 5 stays IN PROGRESS.
