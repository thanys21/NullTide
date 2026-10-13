# Building Progress

Plan and design decisions: `BUILDING_PLAN.md`.

## Status

| Checkpoint | Status |
| --- | --- |
| B0 Architecture audit and design | APPROVED (user) |
| B1 Data model, grid coordinates, occupancy and placement validation | COMPLETE / APPROVED (8/8; full automation 198/198; cook PASS; no manual PIE required) |
| B2 Construction authority | NEXT — DESIGN REVIEW REQUIRED (design gate OPEN; see `BUILDING_PLAN.md` section 5.2). Not started. |
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

### B2 Design Gate — OPEN (required before B2 implementation)

The post-payment recovery chain **respawn → exact refund → world pickup** is accepted as a direction only. Before B2 is implemented, the design must:
- specify each branch: respawn failure, exact-refund failure, world-pickup failure (including partial), terminal failure reporting, recovery-state retention and diagnosis, inventory revision and event counts per branch, and fault-injection tests per branch (the full checklist is in `BUILDING_PLAN.md` section 5.2);
- record what the system guarantees in that case and how it is surfaced (result code, log, telemetry).

Until then, no "resources are always preserved" claim is made. B1 did not modify G6 or extract any world-pickup helper.

## Next Checkpoint

B1 is complete and approved (committed as the B1 milestone). Next: the B2 design review. The section 5.2 design gate must be resolved and approved before any B2 implementation. Phase 5 stays IN PROGRESS.
