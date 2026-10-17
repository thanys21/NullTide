# Building Progress

Plan and design decisions: `BUILDING_PLAN.md`.

## Status

| Checkpoint | Status |
| --- | --- |
| B0 Architecture audit and design | APPROVED (user) |
| B1 Data model, grid coordinates, occupancy and placement validation | COMPLETE / APPROVED (8/8; full automation 198/198; cook PASS; no manual PIE required) |
| B2 Construction authority | COMPLETE / APPROVED (20/20; full automation 219/219; cook PASS; no manual PIE required; commit `bccd0bc`, pushed). Design approved 2026-10-14 (D1–D8, D3 option (a)). |
| B3 First buildable: Wooden Floor | COMPLETE / APPROVED (4/4; full automation 223/223; cook PASS 624/7/631; no manual PIE required; commit `ff04c0e`, pushed). |
| B4 Building mode, ghost preview, input | COMPLETE / APPROVED (7/7; full automation 230/230; cook PASS; manual rendered PIE 8/8 PASS; commit `7ce9be3`; test aid `305e23a`). |
| B5 Wooden wall and wooden door | IMPLEMENTED AND VALIDATED — awaiting commit approval (6/6; full automation 236/236; cook PASS 639/7/646; manual rendered PIE PASS after the rework). Plan approved 2026-10-17 (B5-1 to B5-5; rework R1–R3, D1–D3). |
| B6 | NOT STARTED |

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

## B3 - First Buildable: Wooden Floor

### Baseline and Scope

- **Baseline:** `bccd0bc` (B2 committed and pushed). Full automation 219/219, cook 622/7/629.
- **Scope (user-approved 2026-10-15, `BUILDING_PLAN.md` section 7 B3; decisions B3-1 to B3-3 as recommended):**
  - the production floor piece and definition;
  - `UConstructionComponent` on the player;
  - 4 tests.
- **Not in scope:** input, building mode or UI (B4), walls, doors, demolition, map changes, art. No production C++ change.

### Assets (Unreal MCP; saved by explicit path)

| Asset | Content |
| --- | --- |
| `/Game/LevelPrototyping/BuildingSystem/Pieces/BP_BuildFloor` (new) | Blueprint child of `ABuildingPieceActor`, created with `BlueprintTools.create`. The inherited `PieceMesh` was set on the class default object (`ObjectTools.set_properties`):<br>• `/Engine/BasicShapes/Cube`, relative location (0, 0, 5), scale (2, 2, 0.1), so 200 × 200 × 10 cm with its top at 10 cm;<br>• collision stays the native `BlockAllDynamic`;<br>• default engine material (B3-1).<br>The root is unscaled; no added components; no graph logic. |
| `/Game/LevelPrototyping/BuildingSystem/Definitions/Building_WoodFloor` (new) | Data-only `UBuildingDefinition`: "Wooden Floor" with a description, Foundation, GridCell, 1 × 1, 90°. `PieceActorClass` is `BP_BuildFloor`. Cost [`Item_Wood` × 4], refund 0.5, default range, no icon (B3-3). The data was set on the CDO before the first compile. |
| `/Game/TopDown/Blueprints/BP_TopDownCharacter` (modified) | One `UConstructionComponent` named `ConstructionComponent` (`ActorTools.add_component`), with `AvailableBuildings = [Building_WoodFloor]`. Nothing else changed. |

- **Compile:** all three compiled with warnings as errors.
- **Save-time data validation:** validated 3 assets, no errors.
- **Before saving, other packages were confirmed not dirty:**
  - `Lvl_TopDown`, `BP_TopDownController`;
  - `InventoryManagerComponent`, `Item_Wood`, `BP_PickUpItem`;
  - `W_Inventory`, `BP_Workbench`, `Recipe_Axe`;
  - `PlayerInteractionComponent`.
- **Plan risk R-1 (MCP editing an inherited native component) did not occur.**

### Tests `NullTide.Building.B3.*` (4, in `ConstructionTests.cpp`, reusing the B2 fixture)

| Test | Covers |
| --- | --- |
| `ProductionDefinition` | `Building_WoodFloor` is valid; name, category, mode, footprint, step; cost exactly `Item_Wood` × 4; refund 0.5; default range; piece `BP_BuildFloor`; no icon; `IsDataValid`. |
| `ProductionPieceContract` | `BP_BuildFloor`: <br>• is a child of `ABuildingPieceActor` and adds no SCS components; <br>• has an unscaled root, no `ChildActorComponent` and no custom navigation marked `EvenIfNotCollidable`; <br>• has one mesh, the engine cube, that blocks pawns; <br>• bounds 200 × 200 × 10 cm with its top at 10 cm. |
| `PlayerRegistration` | `BP_TopDownCharacter` has exactly one `UConstructionComponent` with `[Building_WoodFloor]`, still one crafting and one tool-loadout component. |
| `ProductionFloorEndToEnd` | A native builder with production `Item_Wood` × 8. <br>• **Provisional:** the production piece is hidden, has no collision and no navigation influence (runtime check of the Blueprint contract). <br>• **Success:** −4 Wood, +1 revision, +1 event, +1 result. The piece is a `BP_BuildFloor`, Active, registered, visible, collidable and navigation-relevant. <br>• **Support:** a wall on its edge stands at 10 cm, layer 0. <br>• **Then:** same cell → `Occupied`; another cell → `Success`; no Wood → `InsufficientResources`; registry clean. |

### Validation

- **Build:** clean editor build with the editor closed. 0 errors, 0 warnings. Only test code changed.
- **In-editor run** (MCP automation, before the final build): `NullTide.Building` 32/33.
  - The failure was a test-only defect: `ProductionFloorEndToEnd` counted the inventory event raised when the test added its production Wood after binding the listener.
  - Fixed by measuring deltas from just before the build.
- **Final, command line:** `NullTide` **223/223**:
  - Building 33 (B1 9, B2 20, B3 4);
  - Crafting 69, including C5 and C8 player-registration checks;
  - Gameplay 50;
  - Inventory 53;
  - ResourceGathering 18, including RG3 on the player.

  No warnings, errors or ensures.
- **Windows cook** (editor closed): `Done!`, 0 errors, 2 environment warnings. Packages 624 cooked, 7 skipped by platform, 631 total (+2). `Building_WoodFloor` and `BP_BuildFloor` are in the cooked asset registry.
- **Content audit:**
  - 2 added assets (`Content/LevelPrototyping/BuildingSystem/`) and 1 modified (`BP_TopDownCharacter`);
  - no map or Config change;
  - `__ExternalActors__` 91 and `__ExternalObjects__` 3, unchanged.
- **Manual PIE:** none (B3-2; no input path yet).
- **Self-review:**
  - The diff is test code plus the three assets.
  - With no production native change, no fresh-context pass was required by the workflow.
  - The asset contract is enforced by `ProductionPieceContract` and the runtime provisional check.

### Finding for B4 (plan risk R-6, read-only MCP check)

- The `Lvl_TopDown` ground, `Floor_0` (`/Engine/MapTemplates/SM_Template_Map_Floor`, scale 4 × 4 × 1), uses `BlockAllDynamic`, so its object type is **`WorldDynamic`**.
- B1 `ValidatePlacement` accepts only `WorldStatic` or an Active floor as support, and reports a solid non-support actor under a sample as `Blocked`. So every placement on the main ground in PIE would fail with `Blocked`.
- B3 changes nothing here. The B4 plan must decide between two options:
  - change the ground's collision (a map and World Partition change);
  - widen the B1 support rule (for example by mobility, a tag, or an explicit support marker).

## B4 - Building Mode, Ghost Preview and Placement Input

### Baseline and Scope

- **Baseline:** `ff04c0e` (B3 committed and pushed). Full automation 223/223, cook 624/7/631.
- **Scope (user-approved 2026-10-15; B4-1 to B4-5 as recommended; `BUILDING_PLAN.md` section 7 B4):** building mode, the ghost preview, the HUD and placement input for the Wooden Floor.
  - B4-1 changes `Floor_0`'s collision to `BlockAll`.
  - B4-2: `I`/`Tab`/`F` only exit the mode.
  - B4-3: rotation and selection are deferred to B5.
- **Not in scope:** walls, doors, rotation, selection, demolition, art. No change to construction, inventory, crafting, storage or screen code.

### Source

| File | Content |
| --- | --- |
| `Building/BuildingPlacementComponent.h/.cpp` (new) | Player-side UX on the pawn. <br>• **API:** `EnterBuildingMode`, `ExitBuildingMode`, `ToggleBuildingMode`, `ConfirmPlacement`, `UpdatePreviewAt`, `GetPreviewResult`, `GetSelectedBuilding`, `OnBuildingModeChanged`, and the static `DescribeResult`/`DescribeCost`. <br>• **Input:** binds on the local `PlayerController`'s Enhanced Input component (Toggle and Confirm on Started, Cancel on Completed). `IMC_Building` is added while possessed; `IMC_BuildingMode` only in the mode. <br>• **Enter** shows the cursor; every exit restores the remembered state. <br>• **Preview:** one ghost, `CanBuild` when the snapped placement changes or every 0.1 s. <br>• **Confirm:** one `TryBuild` per press, re-snapped at press time. With no cursor target it does nothing. <br>• **HUD:** a result message for 1.5 s, then the live status. <br>• **Exits:** controller change and `EndPlay`. <br>• **Seams:** `GetCursorWorldLocation`, `IsLocalBuilderController`, `CreateHudWidget`. |
| `Building/BuildingPreviewActor.h/.cpp` (new) | Copies the piece class's static meshes (CDO components plus SCS templates) with one dynamic instance of the preview material (`Color` parameter). No collision, not navigation-relevant, no piece component, never registered. |
| `UI/BuildingHudWidget.h/.cpp` (new) | Native HUD base. Bound `BuildingNameText`, `CostText`, `StatusText` and optional `HintText`; hit-test invisible. |
| `NullTide.Build.cs` | Private dependency on `EnhancedInput`. |
| Tests | `BuildingTestTypes.h`: `UBuildingPlacementTestComponent` (scripted cursor, local-controller and HUD seams) and a mode-change listener. `ConstructionTests.cpp`: the B4 section. |

### Assets (Unreal MCP unless noted)

- **New input** (`/Game/LevelPrototyping/BuildingSystem/Input/`):
  - `IA_BuildToggle`, `IA_BuildConfirm`, `IA_BuildCancel`: Boolean, consume input, no triggers.
  - `IMC_Building`: B → Toggle.
  - `IMC_BuildingMode`: LMB → Confirm; RMB, I, Tab, F → Cancel; no per-mapping triggers or modifiers.
- **New** `Materials/M_BuildPreview`: translucent, unlit, two-sided. `Color` (vector) → Emissive, `Opacity` (scalar 0.4) → Opacity.
- **New** `UI/W_BuildingHud`: child of `UBuildingHudWidget`. A root canvas with a top-center vertical box of the four texts. Compiled with warnings as errors.
- **`BP_TopDownCharacter`:** one `BuildingPlacementComponent` configured with:
  - both contexts (priorities 1 and 2) and the three actions;
  - `M_BuildPreview` and `W_BuildingHud`.
- **`Lvl_TopDown` `Floor_0`** (B4-1): collision profile `BlockAllDynamic` → `BlockAll`, object type `WorldStatic`. Mobility stays `Movable`, which confirms that a mobility-based support rule would not have covered it.
  - Changed one external actor package, `__ExternalActors__/TopDown/Lvl_TopDown/2/33/SKUR0WWL7JT7C3OXZMWAQS`.
  - MCP cannot save World Partition actor packages, so the user saved it with Ctrl+S.
- **Test aid (user request, outside the original B4 scope; approved option (a)):**
  - `BP_PickUpItem`'s `Quantity` variable is now Instance Editable, so placed pickups can carry a quantity; pickup logic is unchanged.
  - Two pickups placed near `PlayerStart`, `Item_Wood` × 10 and `Item_Stone` × 10. They add 2 external actor packages (`0/PM/KCABN4UGSM62K8KT67D0L8`, `C/Z5/AEE4KD5VEGN8RL7RX3J62V`). The user may remove them later.
- Before every save, the dirty state of the map, controller, inventory, recipes, widgets and the other building assets was checked; only the intended packages were saved.

### Tests `NullTide.Building.B4.*` (7)

| Test | Covers |
| --- | --- |
| `ModeStateMachine` | Idempotent enter and exit, toggle, one change event per real change, tick only in the mode, one preview. Refused without buildings, without a controller, with a non-local controller, or without a construction component. |
| `CursorState` | The cursor is shown on enter. Each exit restores a hidden or a shown cursor: toggle, controller change, re-possession, and the component's own `EndPlay` (`DestroyComponent`). |
| `PreviewFollowsCursor` | Snapped transform, green; red for `OutOfRange`, `Occupied` and `InsufficientResources`. The ghost has no collision or navigation influence and no piece component. Hidden with no cursor target. The preview never changes the registry or inventory. |
| `ConfirmBuildsOnce` | One confirm → one result, −4 Wood, one piece, the mode stays active, then `Occupied`. No build with no cursor target. Confirm outside the mode does nothing. |
| `ExitPaths` | Toggle, exit, controller change, pawn destroyed, component `EndPlay`. Each removes the ghost and HUD and restores the cursor. The finished build stays Active. |
| `HudContent` | The real `W_BuildingHud` shows "Wooden Floor" and "Wood 8/4", then "Ready to build", "Too far", "Built Wooden Floor" and "4/4". The reason strings are mapped exactly. |
| `ProductionAssets` | Action flags, exact key maps of both contexts without per-mapping triggers, the material's blend mode and parameters, and the character's single configured component. |

### Validation

- **Build:** clean builds with the editor closed. 0 errors, 0 warnings. The base DLL was built, not a hot-reload module.
- **Command line, before the `BP_PickUpItem` change:** `NullTide` **230/230**:
  - Building 40 (B1 9, B2 20, B3 4, B4 7);
  - Crafting 69;
  - Gameplay 50;
  - Inventory 53;
  - ResourceGathering 18.

  No warnings, errors or ensures.
- **In-editor (MCP), after the `BP_PickUpItem` change and the test pickups:** `NullTide` **230/230**, no warnings.
- **First in-editor B4 run:** 1/7. The test controller has no local player, so `IsLocalController()` is false (engine `PlayerController.cpp:304-340`). This is a test-fixture issue, fixed with the `IsLocalBuilderController` seam; production still requires a local controller.
- **Windows cook** (editor closed, before the test aid): `Done!`, 0 errors, 2 environment warnings. Packages 631 cooked, 7 skipped by platform, 638 total (+7). All seven new assets are in the cooked asset registry. Final cook after the test aid (editor closed): `Done!`, 0 errors, 631/7/638 unchanged; `BP_PickUpItem` cooked.
- **Content and World Partition audit:**
  - added: 7 B4 assets and 2 test-pickup actor packages;
  - modified: `BP_TopDownCharacter`, `BP_PickUpItem` and the `Floor_0` actor package;
  - `__ExternalActors__` 93 (91 + 2 pickups), `__ExternalObjects__` 3;
  - no map (`.umap`) change; nothing deleted.
- **Manual rendered PIE (user, 2026-10-17):** "pass hết 8 case", that is all 8 checklist cases PASS:
  1. enter, ghost and HUD;
  2. red with reasons;
  3. build, Wood −4, walk on the floor;
  4. RMB and B exit;
  5. I/Tab/F only exit the mode;
  6. movement in the mode, and the cursor after a screen;
  7. storage, crafting and gathering unchanged;
  8. a floor on a prototype block.

  The sub-items 2b, 3b and 6b were not reported separately.

### Self-Review (including a fresh-context review pass)

The fresh-context review found no P0.

| Finding | Resolution |
| --- | --- |
| P1: `B` could enter the mode while a screen is open | **REJECTED WITH EVIDENCE.** All three screens open with UI-only input: storage and workbench in C++; the inventory in `BP_TopDownController` through `SetInputModeUIOnly` (read through MCP). `FInputModeUIOnly` calls `SetIgnoreInput(true)` (`PlayerController.cpp:6384`), and `UGameViewportClient::InputKey` returns early on `IgnoreInput()` (`GameViewportClient.cpp:767`). |
| Confirm built at a stale, hidden placement with no cursor target | **CONFIRMED**, fixed and tested. |
| The "EndPlay" exit tests never ran the component's `EndPlay` (the pawn unpossesses first) | **CONFIRMED.** Added `DestroyComponent` cases; renamed the case to "Pawn destroyed". |
| The reason-string test could not fail (default branch) | **CONFIRMED.** Exact mappings asserted. |
| Two different local checks; the non-local refusal was untested | **CONFIRMED.** One seam; non-local refusal tested. |
| Per-mapping triggers and modifiers not checked | **CONFIRMED.** Asserted. |
| The mode-change event was broadcast before the first preview | **CONFIRMED.** Moved last; Enter returns the actual state. |
| The ghost ignores nested attachment and inherited SCS overrides | **Deferred to B5** (walls and doors); not reachable with `BP_BuildFloor`. |
| Cursor possibly locked after a screen's GameOnly close | **Checked in PIE** (case 6): PASS. |

### Known Limitations Carried Forward

- Rotation and the build-bar selection are deferred to B5. B4 always places the first registered building at rotation 0.
- The ghost copies meshes with their relative transforms under its root. Nested attachment and child-Blueprint overrides of a parent's SCS components are not copied; B5 must handle this for walls and doors.
- If the level's navigation mesh is static, runtime floors do not update it. Movement is WASD, so there is no impact now; record it for NPC pathing (Phase 6).
- The two test pickups and the `BP_PickUpItem` Instance Editable change are test aids requested by the user.

## B5 - Wooden Wall and Wooden Door

### Baseline and Scope

- **Baseline:** `305e23a` (B4 `7ce9be3` plus the test aid). Full automation 230/230, cook 631/7/638.
- **Scope (user-approved 2026-10-17; B5-1 to B5-5 as recommended; `BUILDING_PLAN.md` section 7 B5):** walls and doors on cell edges, selection with `1`/`2`/`3`, rotation with `R`, the door actor and its interaction, and the ghost's attach-chain composition.
- **Not in scope:** demolition and refunds (B6), locking doors, door art, NPC pathing.

### Source

| File | Content |
| --- | --- |
| `Building/BuildingDoorActor.h/.cpp` (new) | `ABuildingDoorActor : ABuildingPieceActor` (abstract). <br>• **Layout:** the inherited `PieceMesh` is the left post; `FramePostRight`, `FrameLintel` (220–250 cm); a `DoorHinge` at the left post's inner edge with a 100 × 8 × 220 cm `DoorLeaf`; an overlap-only `InteractionBounds` that never affects navigation. <br>• **API:** `ToggleDoor`, `SetDoorOpen`, `CanInteract` → `EBuildingDoorResult` (`Success`, `InvalidInteractor` for a null or non-pawn interactor, `OutOfRange` beyond `UseRange` 300 cm, `NotActive` while not an Active piece, provisional or being destroyed). <br>• **Swing:** 90° over `SwingDuration` (0.3 s) on a looping world timer at 1/60 s; no tick. Toggling mid-swing reverses. <br>• **Leaf collision:** off as soon as opening starts; the authored collision is restored only when fully closed. <br>• `EndPlay` clears the timer after `Super`. |
| `Building/BuildingPlacementComponent.h/.cpp` | `SelectBuildingIndex` (bound to `SelectBuildingActions`, Started; missing indices ignored; persists across exit and enter) and `RotatePlacement` (`RotateAction`, Started; 0 → 3 → 0). The rotation goes to `MakePlacementFromWorld`. A selection change rebuilds the ghost and revalidates at once. |
| `Building/BuildingPreviewActor.cpp` | Each template's transform is composed up its attach chain: the CDO `AttachParent` chain for default components; for SCS templates, parent nodes in the same script, a parent class's node by variable name, or a native parent by name. |
| `Building/BuildingSubsystem.h/.cpp` | **Deviation, see the Self-Review:** new `EdgeEndInset` (12 cm = half a wall thickness + clearance). Edge end samples move from 5 cm to 12 cm from the corners, so a perpendicular wall at a corner is not under a sample. The edge collision box uses the same constant (unchanged value, 88 cm half length). |
| `UI/BuildingHudWidget` | Hint "1-3 select · R rotate · LMB build · RMB / B exit". |
| Tests | `BuildingTestTypes.h`: `ABuildingTestDoorActor` (settable swing), `ABuildingTestTurnedHingeDoorActor`, `UBuildingTestDoorPieceDefinition`. `ConstructionTests.cpp`: the B5 section; the B3 `PlayerRegistration` and B4 `ProductionAssets` assertions now leave the exact lists to B5. |

### Assets (Unreal MCP unless noted)

- **New pieces** (`Pieces/`): `BP_BuildWall` (child of `ABuildingPieceActor`; `PieceMesh` = engine cube at (0, 0, 125), scale (2, 0.2, 2.5)); `BP_BuildDoor` (child of `ABuildingDoorActor`; no added components).
  - `BP_BuildDoor` implements `BPI_Interactable`: **manual step by the user** (Class Settings → Implemented Interfaces). Then through MCP: `EventInteract(Interactor)` → `ToggleDoor(Interactor)`; `GetInteractionType` returns Press (`NewEnumerator0`); `GetHoldDuration` returns 0. The three disabled template events were removed; the CDO does not tick.
- **New definitions** (`Definitions/`): `Building_WoodWall` ("Wooden Wall", Wall, GridEdge, Wood × 6, refund 0.5); `Building_WoodDoor` ("Wooden Door", Door, GridEdge, Wood × 8 + Scrap × 2, refund 0.5).
- **New input:** `IA_BuildRotate`, `IA_BuildSelect1/2/3` (Boolean, consume, no triggers). `IMC_BuildingMode` adds R → Rotate and One/Two/Three → Select (9 mappings).
- **`BP_TopDownCharacter`:** `AvailableBuildings = [Building_WoodFloor, Building_WoodWall, Building_WoodDoor]`; the placement component has `RotateAction` and the three select actions.
- **Test aid (B5-5):** the Wood test pickup is now Wood × 100 (`0/PM/KCABN4UGSM62K8KT67D0L8`, modified); a new Scrap × 10 pickup at (250, 0) (`A/SD/QSK9GQ2UM1LBYM2ZNH9X8X`, added). Saved by the user with Ctrl+S.
- All MCP-edited assets were saved by explicit path; Blueprints compiled with warnings as errors (`BP_BuildWall`, `BP_BuildDoor`, both definitions, `BP_TopDownCharacter`).

### Tests `NullTide.Building.B5.*` (6)

| Test | Covers |
| --- | --- |
| `ProductionDefinitions` | Both definitions valid with exact name, category, GridEdge, cost, refund and piece class; editor data validation. Wall and door piece contracts (unscaled root, no child actors, no `EvenIfNotCollidable`); wall 200 × 20 × 250; door frame and leaf bounds, leaf on the hinge, interaction volume overlap-only; no tick. |
| `EdgeBuild` | Production pieces through the authority: walls on an X and a Y edge (exact cost, edge key, yaw 0/90, edge center); a wall on a floor's edge stands at 10 cm; a door on a walled edge and a wall on a door's edge are refused (`Blocked`, naming the existing piece; the key stays with it); a door at a wall's corner at rotation 1 (yaw 270, Wood −8, Scrap −2, one revision); all resources spent exactly; registry clean. |
| `DoorToggle` | Provisional door refuses (`NotActive`); instant swing toggles leaf collision and hinge yaw; frame always blocks; null, out-of-range and non-pawn interactors refused; timed swing reverses mid-swing, collision returns only when fully closed, no tick; a destroyed door refuses and has no timer. |
| `DoorInterface` | `BP_BuildDoor` implements `BPI_Interactable`; `GetInteractionType` is Press; `Interact` through `ProcessEvent` opens and closes in range, refused out of range; no inventory or registry change. |
| `SelectionAndRotation` | Index 0 by default; select 2 rebuilds the same ghost with four meshes and the HUD name; missing indices ignored; every real door mesh (including the hinged leaf) matches a ghost mesh, also under a turned and scaled hinge; `R` wraps 1, 2, 3, 0 and the ghost flips on odd turns; confirm builds the door at yaw 180; the wall ghost on the door's edge is red; selection persists across exit and enter. |
| `ProductionInput` | `IMC_BuildingMode` maps exactly LMB, RMB, I, Tab, F, R, 1, 2, 3 with no per-mapping triggers; the new actions' flags; the character registers Floor, Wall, Door in order and has the new actions. |

### Validation

- **Build:** clean builds with the editor closed; 0 errors, 0 warnings; base DLL.
- **Command line:** `NullTide` **236/236** (230 + 6), exit code 0, after the review fixes. Building 46 (B1 9, B2 20, B3 4, B4 7, B5 6).
- **In-editor (MCP) Building run** before the corner fix: 45/46, `EdgeBuild` failed with "BP_BuildWall_C_0 is in the way" for a door at a wall's corner. This found the edge-sample deviation below.
- **Windows cook** (editor closed): `Done!`, 0 errors, 2 environment warnings; 639 cooked, 7 skipped, 646 total (+8). All eight new assets are in the cooked asset registry. Re-run after the review fixes (C++ only): `Done!`, 0 errors, 2 environment warnings, 639/7/646 unchanged.
- **Content and World Partition audit:** added 8 assets and 1 actor package; modified `IMC_BuildingMode`, `BP_TopDownCharacter` and 1 actor package; `__ExternalActors__` 94 (93 + Scrap pickup), `__ExternalObjects__` 3; no `.umap` change; nothing deleted.
- **Manual rendered PIE (user, 2026-10-17):** the first run found three issues, fixed in the rework below. The retest of the updated 8-step checklist: "test done, xây chuẩn" (all PASS).

### Self-Review (including a fresh-context review pass)

| Finding | Resolution |
| --- | --- |
| Walls and doors could not meet at a grid corner: edge end samples at 5 cm fell inside a perpendicular wall's 10 cm half thickness (`Blocked`) | **CONFIRMED** by `EdgeBuild`. Fixed with `EdgeEndInset` (12 cm). This is a B1 validation change the plan said B5 would not make. **APPROVED by the user (B5-D1, 2026-10-17).** |
| Edge on an occupied edge reports `Blocked`, not `Occupied` as the plan's test text says | **CONFIRMED.** The B1 order checks the support surface before occupancy, and the existing piece is under the samples. Kept the approved order; tests assert `Blocked` naming the piece and that the key stays with it. **ACCEPTED by the user (B5-D2).** |
| P1: a floor or furniture cannot be placed in a cell next to an existing wall or door (cell samples inset 5 cm and the cell collision box shrunk 2 cm overlap the wall's 10 cm half thickness) | **CONFIRMED by analysis** (`BuildingSubsystem.cpp` cell samples and cell box; not yet tested). **User chose fix now (B5-D3):** cell samples and the cell box are inset by `EdgeEndInset`; `EdgeBuild` builds a floor next to an existing wall. |
| A door could be toggled during or after `EndPlay` | **CONFIRMED.** `CanInteract` refuses a door being destroyed; the timer is cleared after `Super::EndPlay`. Tested. |
| Closing always forced `QueryAndPhysics` on the leaf | **CONFIRMED.** The authored collision is cached in `PostInitializeComponents` (from the body instance, since a provisional piece has actor collision off) and restored. |
| `CanInteract` accepted any actor; B5-4 says a valid pawn | **CONFIRMED.** Now requires a pawn; tested. |
| The ghost-hinge test could not detect a wrong composition order | **CONFIRMED.** Added the turned and scaled hinge case. |
| `HandleSelectInput` (action → index) is not run by a test | **Known gap:** asset wiring is asserted; key behaviour is PIE step 1/4/7. |
| `SelectedIndex` can drift if `AvailableBuildings` changes during the mode | **Known, harmless:** `TryBuild` re-checks registration; nothing changes the list at runtime. |
| The ghost ignores child-Blueprint overrides of inherited SCS components and absolute-transform flags | **Carried limitation:** unreachable today (the pieces add no components, asserted). |

### Rework after the first PIE (plan approved 2026-10-17; `BUILDING_PLAN.md` "B5 rework after the first PIE")

The user's first PIE run found three problems:
- walls and doors built without a floor;
- the door prompt read "None";
- the door's opening side was not visible while building.

**Fixes:**
- **B5-R1, floor requirement:**
  - `UBuildingDefinition::bRequiresFloor` (default false); `Building_WoodWall` and `Building_WoodDoor` set it.
  - `ValidatePlacement` (support stage, after `Uneven`) returns the new `EBuildResult::NeedsFloor` ("Needs a floor") unless every sample on one side of the edge hits an `Active` Foundation. A shared `IsActiveFloorHit` helper serves both this check and the support rule.
- **B5-D3:** cell samples and the cell collision box are inset by `EdgeEndInset` (12 cm; previously 5 cm and 2 cm). The `SampleInset` constant was removed.
- **B5-R2, prompt key:**
  - New `UI/InputPromptLibrary` (`GetKeyMappedToAction`).
  - `WB_interactionWidget`: the GetOwningPlayer → subsystem → `QueryKeysMappedToAction` → Get chain was replaced by `GetKeyMappedToAction(IMC_Default, IA_Interact)`. The rest of the graph is unchanged.
  - Cause: Enhanced Input drops the consumed F → `IA_Interact` mapping while `IMC_BuildingMode` is active (`EnhancedInputSubsystemInterface.cpp:1041`).
- **B5-R3, facing arrow:**
  - `ABuildingPreviewActor` has a `FacingArrow` (cube shaft plus cone head, tinted, no collision, not a preview mesh). `SetFacingArrow` points it along local +Y for edge pieces and local +X for cells, so it turns with `R`.
  - The door already opens towards local +Y, so a door opens where the arrow showed; asserted for rotation 0 and 1 and for the production door through `Interact`.

**Validation:**
- Clean build: 0 warnings.
- Command line before the asset edits: 234/236. The two failures were the expected `bRequiresFloor` assertions (assets not yet edited); every B1–B4 test passed with the D3 change.
- In-editor (MCP) after the asset edits: `NullTide` **236/236** (one environment RHI memory warning).
- Assets: `Building_WoodWall`, `Building_WoodDoor` and `WB_interactionWidget` compiled with warnings as errors and saved by path.
- **Rework fresh-context review:** no P0 or P1.

  | Finding | Resolution |
  | --- | --- |
  | P2 D3 trade-off (thin border blockers) | **NEEDS USER DECISION → decided:** record now, fix with furniture (Known Limitations). |
  | P3 outer wall at a terrain drop | Recorded (Known Limitations). |
  | P3 test gaps | **CONFIRMED, fixed.** Added a wall with floors on both sides (Y(2,0)), a Y edge with a floor on its negative side only (Y(2,−2)), and a diagonal-only floor that gives `NeedsFloor` (X(0,−1)). |
  | P3 the arrow kept a stale tint with a null material | **CONFIRMED, fixed.** The arrow materials are always reassigned (null resets). Tests check the two arrow meshes, the tint with `M_BuildPreview`, and the reset. |
  | P3 the prompt ignores remaps and profiles | Documented on `GetKeyMappedToAction`. |
  | P3 stale `CollisionClearance` comment | Fixed. |
- **Final (editor closed, after these fixes):** clean build, 0 warnings. Command line `NullTide` **236/236**, exit 0, no automation warnings or errors. Windows cook `Done!`, 0 errors, 2 environment warnings, 639/7/646. `WB_interactionWidget`, both definitions and `BP_BuildDoor` are in the cooked asset registry.

### Known Limitations Carried Forward

- `bRequiresFloor` defaults to false; a new edge definition must set it (RW-2).
- **D3 trade-off (rework review P2; user decision 2026-10-17: record now, fix with furniture).**
  - With cell samples at 12 cm and the cell box 12 cm inside the cell, a thin pawn-blocking blocker on a cell border is missed: a fence up to 24 cm thick, or a pole of radius 16 cm or less at a corner.
  - The builder-safety margin is also 7 cm inside the cell.
  - Harmless for 10 cm floors on the current map.
  - Planned fix when furniture lands (after B6): restore the 2 cm box and 5 cm samples for cells, and ignore only `Active` edge pieces registered on the cell's own four edges at the same layer. That is an identity exclusion against the B1 "geometric" rule, so it is approved for that checkpoint.
- **Outer wall at a terrain drop (rework review P3):** a wall with a full floor on one side is still refused by `NoGround` or `Uneven` from the bare side, for example at a raised block's edge more than 25 cm above the outside ground. Possible fix: with `bRequiresFloor` and one full floor side, evaluate support on that side only.
- The prompt key comes from `IMC_Default`'s default mappings, so player remaps and mapping profiles are not reflected.
- An edge on an occupied edge reports `Blocked` ("… is in the way").
- The ghost does not apply child-Blueprint overrides of inherited SCS templates.
- Door collision is not swept: closing on a pawn in the doorway relies on character depenetration (risk R-3, PIE step 5).
- Interaction targets the last overlapped interactable (risk R-4): a door next to storage or the workbench may target the wrong one.
- Navigation with the dynamic nav mesh is PIE-only.

## Next Checkpoint

B5 is implemented and validated (automation 236/236, cook PASS, manual PIE PASS). It waits for the commit approval. Next after B5: B6 (demolition and refund). Phase 5 stays IN PROGRESS.
