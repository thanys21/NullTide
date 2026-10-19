# Building Plan

Phase 5 of `NULLTIDE_MASTER_PLAN.md`. Building is delivered in small checkpoints (B1-B6). Each checkpoint is validated, documented in `BUILDING_PROGRESS.md` and committed before the next one starts.

**Status:**
- **B0 is APPROVED** (architecture, review refinements and the final decisions in section 10.3).
- **B1 is COMPLETE / APPROVED:** data model, grid coordinates, occupancy and placement validation logic (see `BUILDING_PROGRESS.md`; full automation 198/198, cook PASS, no manual PIE required).
- **B2 design gate is APPROVED (user, 2026-10-14):** decisions D1–D8 in 5.2.13, with D3 option (a). The design activates the piece before payment, so nothing fallible runs after the deduction. It was reviewed independently in two rounds (5.2.14, 5.2.15).
- **B2 is COMPLETE / APPROVED** (20 B2 tests, full automation 219/219, cook PASS; commit `bccd0bc`). **B3 (Wooden Floor) is COMPLETE / APPROVED** (4 B3 tests, full automation 223/223, cook PASS; commit `ff04c0e`). **B4 (building mode, ghost, input) is COMPLETE / APPROVED** (7 B4 tests, full automation 230/230, cook PASS, manual PIE 8/8 PASS; commit `7ce9be3`). **B5 plan APPROVED (2026-10-17; B5-1 to B5-5 as recommended), in progress** (section 7 B5). B4–B6 are not started. Phase 5 is IN PROGRESS.

Baseline: `bbfcc08` (Phase 4 Crafting complete, C8A–C8C pushed). Full automation 190/190, Windows cook PASS.

---

## 1. Architecture Audit (verified against the current source and assets)

### 1.1 Reusable systems

| System | What exists (verified) | Reuse for Building |
| --- | --- | --- |
| Item model | `UItemDefinition` (abstract, const, data-only BP subclasses, read from the CDO, `TSubclassOf`). `UItemInstance` (ItemId, definition class, quantity; private mutators). Production resources: `Item_Wood`, `Item_Stone`, `Item_Scrap`. | Costs reference canonical `TSubclassOf<UItemDefinition>`. No new item type is needed for the MVP. |
| Inventory authority | `UInventoryComponent::TryApplyDefinitionTransaction(Removals, Additions)` (native only) and its exact read-only preflight `CanApplyDefinitionTransaction`. Removal-only and addition-only calls are allowed (only both-empty is rejected as `InvalidQuantity`). Duplicate definitions are aggregated. One revision and one `OnInventoryChanged` on success; nothing on failure. It returns `Busy` inside a mutation guard and always re-plans from live contents. | **Sufficient.** Construction cost = removal-only transaction; refund = addition-only transaction. No new inventory primitive is needed. `FInventoryDefinitionQuantity` (`DefinitionClass`, `Quantity` ≥ 1; `EditAnywhere`, `BlueprintType`) can be reused directly as the cost entry type. |
| Spawn-first precedent | `TryDropItemToWorld` (G6): `SpawnActorDeferred` with `AdjustIfPossibleButDontSpawnIfColliding`, then the inventory removal; if the removal fails, `RollbackWorldDrop` destroys the spawned actor (`RemovalFailedRolledBack`). | The **ordering** is reused: an actor exists before the cost is committed, and a failed commit destroys it. The **spawn flag is not reused**: building spawns `AlwaysSpawn` as a provisional actor and validates collision itself at the exact grid transform (section 4.4), so the engine never moves a piece off the grid. |
| Crafting authority pattern | `UCraftingComponent` on `BP_TopDownCharacter`: registered list (`AvailableRecipes`), exact `Can*` preflight, `Try*` commit inside a reentrancy guard (`bCraftInProgress`), exactly-once result delegates, native movement binding, `EndPlay` cleanup. | The construction authority follows the same shape (registered buildings, `CanBuild`/`TryBuild`, guard, one notification per request). |
| Station context pattern | `UCraftingStationComponent` marks any actor as a station; `ACraftingStationActor` is a thin host. Context, never authority. | `UBuildingPieceComponent` marks any actor as a placed building piece, so Storage and Workbench can become buildable by adding the component to a new BP. Their existing authorities stay untouched. |
| Storage | `AStorageContainerActor` + `UStorageComponent` (`GetItemCount`, `IsFull`, `Capacity`, `Revision`); transfers keep ItemIds. | Demolition must check `GetItemCount() == 0` (never delete contents). |
| Interaction | `PlayerInteractionComponent` (BP) uses capsule overlap with `BPI_Interactable`. `Interact(Interactor)` is Press/Hold via `E_InteractionType`. The last overlapped actor wins. `F` is `IA_Interact`. | The door uses `BPI_Interactable` (Press) to open and close. Building mode does not change interaction. |
| Navigation | `RecastNavMesh` with `RuntimeGeneration=Dynamic` (`Config/DefaultEngine.ini`). | Walls placed at runtime update the nav mesh automatically. |
| Tests | Transient-world fixtures with a world context and initialized actors (C8A `FC8AFixture`, CQTest-style), deferred timer advance, real widget tests. | Building tests reuse the same fixture style. |

### 1.2 Input and controller (verified)

- `IMC_Default` mappings: `Space` Jump, `W/A/S/D` Move, `F` Interact, `Q` DropItem, `I` and `Tab` OpeningInventory. **No mouse buttons are mapped.**
- `BP_TopDownController` (parent `APlayerController`): the EventGraph only handles `IA_OpeningInventory` (creates `W_Inventory`).
  - The template click-to-move functions `MoveTo` (`SimpleMoveToLocation`), `Follow` and `GetLocationUnderCursor` (`GetHitResultUnderCursorByChannel`, `TraceTypeQuery1`) still exist, but **nothing calls them**: there is no `IA_SetDestination` asset or mapping. Click-to-move is effectively not wired in the current game.
  - Controller defaults: `bShowMouseCursor = true`, `bEnableClickEvents = true`.
- `BP_TopDownCharacter` (parent `ACharacter`) adds `IMC_Default` at priority 0 in `BeginPlay` and handles `IA_Jump` and `IA_Move` (WASD), with the gather-cancel calls added in RG1.
- **Cursor conflict:** `UInventoryScreenWidget::CloseInventory` and `UStorageScreenWidget::CloseStorage` set `SetShowMouseCursor(false)` and `GameOnly` input. After the first screen closes, the cursor is hidden for the rest of the session (this is why the C7 click-to-move PIE case was NOT TESTED). Building mode needs a visible cursor and must assert it itself.
- **PIE caveat:** in the editor, `Esc` stops the PIE session unless a widget handles it. Building-mode cancel must also work without `Esc` (right mouse button).

### 1.3 World Partition and actor ownership

- `Lvl_TopDown` is a World Partition map with external actor packages (91) and external objects (3). Authored actors (pickups, resource nodes, `BP_StorageContainer`, `C8B_Workbench`) live in those packages.
- Actors spawned at runtime in PIE or game are **not** external actor packages. They live in the persistent level, are never streamed out and are discarded when the session ends. Building never writes map or external-actor assets.
- Persisting player-built structures is Phase 7 (Save/Load) and is out of Phase 5 scope.

### 1.4 Conflicts and gaps found

1. **No building-related input exists.** Mouse buttons are free, so building can own them without touching `IMC_Default`.
2. **The cursor is hidden after any screen closes** (see 1.2). Building mode must show the cursor on entry, keep it shown while active, and restore the previous state on exit.
3. **No grid or occupancy concept exists.** A world-level registry is needed so validation does not depend only on physics overlaps.
4. **`BP_Door` (prototype, not placed) has its own timeline open/close and no building semantics.** It is not reused (section 2.4).
5. **There is no hammer tool type** (`EToolType` is `None, Axe, Pickaxe`). The MVP builds without a tool (approved).
6. **The interaction interface is a Blueprint interface** (verified in review round 1 through MCP, read-only):
   - `BPI_Interactable` (`/Game/LevelPrototyping/interactionSystem/BPI_Interactable`) declares `Interact(Interactor)`, `GetInteractionType() -> E_InteractionType` and `GetHoldDuration() -> float`.
   - `PlayerInteractionComponent` adds an overlapped actor only if `DoesObjectImplementInterface(OtherActor, BPI_Interactable_C)`, and calls the three functions as interface messages.
   - `GetActiveInteractable` also asks the native `ShouldShowInteractionPromptFor` (ResourceGathering) whether to show the prompt.
   - **A native C++ class cannot implement a Blueprint-generated interface**, so "a native door actor implementing `BPI_Interactable`" is only possible through a Blueprint child that declares the interface (section 2.4).

---

## 2. Recommended Building Architecture

```
UBuildingDefinition (data-only BP subclasses: Building_WoodFloor, Building_WoodWall, Building_WoodDoor)
        │  cost (FInventoryDefinitionQuantity[]), piece actor class, footprint, placement mode, rotation, refund
        ▼
UConstructionComponent  ── player's construction authority (on BP_TopDownCharacter; reusable by future NPCs)
        │  CanBuild / TryBuild / CanDemolish / TryDemolish, registered buildings, guard, events
        ├── UBuildingSubsystem (UWorldSubsystem): grid math, occupancy registry, authoritative ValidatePlacement
        └── UInventoryComponent::TryApplyDefinitionTransaction  (only resource authority)
                                    ▲
UBuildingPlacementComponent ────────┘  player UX only: build mode, selection, ghost preview, rotate, confirm/cancel
        (on BP_TopDownCharacter; own IMC_Building; never mutates inventory or spawns real pieces)

ABuildingPieceActor (thin native host: root + mesh + UBuildingPieceComponent) → BP_BuildFloor / BP_BuildWall / BP_BuildDoor
UBuildingPieceComponent: marks any actor as a placed piece (definition, cells, rotation, bPlayerBuilt)
```

### 2.1 Building definition: definition class (recommended) versus Data Assets

| Option | Pros | Cons |
| --- | --- | --- |
| **`UBuildingDefinition` abstract, const UObject; data-only BP subclasses; `TSubclassOf`; read from the CDO** (recommended) | Same pattern as items, recipes and stations, so the team already knows how to author, reference, validate (`IsDataValid`) and test it. No Asset Manager setup. | Class identity is the ID (renames need redirects, as for items). |
| `UPrimaryDataAsset` instances | Asset Manager IDs, async loading, bundles. | A new convention in the project; needs Asset Manager configuration; different from every existing definition. |
| One C++ subclass per building | — | Rejected: the request forbids it, and it does not scale. |

**`UBuildingDefinition` fields (MVP):**
- Display: `BuildingName`, `BuildingDescription`, `BuildingIcon`.
- `Category`: `EBuildingCategory { Foundation, Wall, Door, Furniture, Station }`, used only for UI grouping and placement defaults.
- `PieceActorClass`: `TSubclassOf<AActor>` that must carry a `UBuildingPieceComponent`; validated.
- `PlacementMode`: `EBuildingPlacementMode { GridCell, GridEdge }`. Floors occupy cells; walls and doors occupy cell edges. `Free` is deferred.
- `Footprint`: `FIntPoint` in cells (GridCell, MVP 1×1). GridEdge pieces occupy exactly one edge.
- `AllowedRotationStep`: degrees, 90 in the MVP (edges use 0 or 90).
- `Cost`: `TArray<FInventoryDefinitionQuantity>`, validated (non-null, non-abstract, `ValidateFragments`, quantity ≥ 1, no duplicates; the same rules as recipes).
- `RefundFraction`: 0..1, default 0.5 (approved).
- `BuildRangeOverride`: optional; 0 uses the component default.
- Reserved for later, not added in the MVP: `Tier`, `UpgradesTo`, `RequiredTool`, `RequiredStation`, support or stability.

`ValidateBuilding(OutDiagnostic)` (BlueprintPure) and `IsDataValid` follow `UCraftingRecipeDefinition::ValidateRecipe`.

### 2.2 Grid Coordinate Contract

All grid math lives in `UBuildingSubsystem` (pure, deterministic functions with no world state, unit-tested in B1). `S` = cell size = **200 cm** (approved).

**Origin and axes.** The grid origin is the world origin `(0, 0)` in X/Y and never moves (no per-level offset in the MVP). Cell `(ix, iy)` covers `[ix·S, (ix+1)·S) × [iy·S, (iy+1)·S)`.

**World → cell.** `ix = FloorToInt(X / S)`, `iy = FloorToInt(Y / S)`. Floor division (not truncation) keeps negative coordinates correct: `X = -1` lies in cell `-1`, not `0`. A point exactly on a boundary belongs to the cell on its positive side.

**Cell → world.** Center `((ix + 0.5)·S, (iy + 0.5)·S)`.

**Edges** (walls and doors). Every edge has one canonical key `(Axis, ix, iy)`:
- `Axis = X`: the edge runs along X, from `(ix·S, iy·S)` to `((ix+1)·S, iy·S)`. This is the south edge of cell `(ix, iy)` and the north edge of cell `(ix, iy−1)`.
- `Axis = Y`: the edge runs along Y, from `(ix·S, iy·S)` to `(ix·S, (iy+1)·S)`. This is the west edge of cell `(ix, iy)` and the east edge of cell `(ix−1, iy)`.
- The canonical form means a shared edge between two cells is one key, so two walls can never sit on the same edge by addressing it from different cells.

**Edge snapping (deterministic).**
1. For a cursor point in cell `(ix, iy)`, take local `u = X/S − ix` and `v = Y/S − iy` in `[0, 1)`.
2. Distances: south `v`, north `1 − v`, west `u`, east `1 − u`.
3. Pick the minimum. Ties are broken in the fixed order South, West, North, East.
4. Map to the canonical key: south `(X, ix, iy)`, north `(X, ix, iy+1)`, west `(Y, ix, iy)`, east `(Y, ix+1, iy)`.

**Rotation mapping.**
- **GridCell** (floor): the yaw is `Rot · 90°` with `Rot ∈ {0, 1, 2, 3}` (`R` cycles it).
  - The footprint `W × H` (in cells) becomes `H × W` for odd `Rot`.
  - The anchor cell is the cursor cell; the occupied cells are `{anchor + (i, j)}` for `0 ≤ i < W'` and `0 ≤ j < H'` in the +X/+Y directions after rotation.
  - The actor transform is the center of the rotated footprint.
  - For the MVP 1×1 floor, rotation only affects visuals.
- **GridEdge** (wall, door): the axis comes from snapping (X edge → yaw 0°, Y edge → yaw 90°). `R` adds 180°, which only flips which side a door opens towards. The occupancy key is the same for both facings.
- The resolved yaw is always an exact multiple of 90°. The transform is recomputed from the key and facing on every request, never taken from the UI.

**Height and layers.** A placement has an explicit `Layer = RoundToInt(SurfaceZ / 50 cm)`.
- `SurfaceZ` is the highest valid surface hit of the downward traces, for example:
  - ground z ≈ 0 gives layer 0;
  - the central plateau top z ≈ 200 gives layer 4;
  - the top of a floor piece on the ground (z ≈ 10) gives layer 0.
- **Floors** are placed at `SurfaceZ` and occupy `(Floor, ix, iy, Layer)`.
- **Walls and doors** stand on the ground or on a floor's top surface. They occupy `(Edge, Axis, ix, iy, Layer)`, where `Layer` comes from the surface under the edge midpoint.
- Different layers can coexist in the same X/Y. This only happens with real height differences such as overhangs; normal terrain gives one layer per column.
- The 50 cm quantum and the 25 cm unevenness tolerance (section 4.1) are tuned so that a floor top (10 cm) stays in its ground layer while the plateau stays a separate layer.

`FBuildingPlacement` (USTRUCT) carries the key and facing:
- `PlacementMode`, `Anchor (FIntPoint)`, `EdgeAxis`, `Rotation (0-3)`, `Layer`;
- the resolved `WorldTransform` (output only).

### 2.3 Occupancy Lifecycle

**Categories (independent key spaces).**
- `Floor` (cells) and `Edge` (walls and doors) are separate categories. A floor cell and the walls on its edges never conflict.
- A wall and a door compete for the same `Edge` key, so a door cannot be placed on an edge holding a wall.
- `Furniture` (cells, a separate category from `Floor`) is reserved for later (Storage, Workbench as buildables).

**Key.** `FBuildingOccupancyKey { Category, Axis (None/X/Y), X, Y, Layer }`, hashable. The registry is `TMap<FBuildingOccupancyKey, FBuildingOccupant>`. An occupant is either:
- `Reserved`, held by a reservation handle owned by a `UConstructionComponent` (weak pointer), or
- `Occupied`, held by a `TWeakObjectPtr<UBuildingPieceComponent>`.

**States of a piece** (`UBuildingPieceComponent::State`):
1. `Provisional`: spawned by the construction authority, hidden, no collision, no navigation, not interactable. Its keys are held as `Reserved` by the authority's reservation handle, not by the piece.
2. `Active`: finalized, keys `Occupied`, visible, collision on.
3. `Removing`: demolition in progress; keys still occupied until `EndPlay`.

**Registration rules.**
- **Only the subsystem mutates the registry**, through:
  - `ReserveKeys(Owner, Keys) -> FBuildingReservationHandle`;
  - `CommitReservation(Handle, Piece)`;
  - `ReleaseReservation(Handle)`;
  - `RegisterActivePiece(Piece, Keys)`;
  - `UnregisterPiece(Piece)`.
- **Reserve:** fails (changing nothing, invalid handle) if any key is `Reserved` or `Occupied` by anyone. It is all-or-nothing. Reservations whose owning component is gone count as free and are purged.
- **Commit:** converts exactly the handle's keys to `Occupied` by `Piece` and invalidates the handle. It cannot fail while the handle is valid; it fails cleanly (changing nothing) for an invalid or stale handle.
- **Register active** is used for pieces that start already finalized (authored building pieces in a future level, or loaded saves in Phase 7). It is all-or-nothing, and from `BeginPlay` only when `State == Active`.
- **Idempotent and duplicate-free:**
  - The piece stores its own key list.
  - Registering the same piece twice with the same keys is a no-op.
  - Registering different keys while it already holds keys is rejected (a piece occupies exactly one footprint).
- **Unregister** removes every key the piece holds (`Reserved` or `Occupied`). It runs from `UBuildingPieceComponent::EndPlay` (all reasons) and `OnComponentDestroyed`, so it also covers rollback destruction, demolition, level unload and PIE end.
- **No stale or ghost occupancy:**
  - Every lookup validates the weak pointer, and an entry whose piece is gone counts as free and is purged.
  - The subsystem clears everything in `Deinitialize`.
  - Debug builds have an `AuditRegistry()` check (also used by tests) that every entry points to a live piece holding that key, and every live piece's keys point back to it.
- **Rollback:** a provisional piece holds no keys itself. The authority's handle holds them as `Reserved`. Every rollback path calls `ReleaseReservation(Handle)` and destroys the provisional actor, so a failed placement leaves no occupancy. If the construction component itself ends play mid-request, its reservations are purged as stale.

### 2.4 Door Actor (refined per review)

- **`ABuildingDoorActor`** (native, subclass of `ABuildingPieceActor`) owns all door behaviour:
  - `bIsOpen`;
  - `ToggleDoor(AActor* Interactor)` / `SetDoorOpen(bool)` (BlueprintCallable), with the same range and player-controller checks as `ACraftingStationActor::GetOpenCraftingResult`;
  - a hinged leaf mesh component rotated by a short timer-driven interpolation (no tick when idle);
  - leaf collision blocks pawns only while closed, and navigation updates through the dynamic nav mesh;
  - interaction is refused while the piece is `Provisional`.
- **`BP_BuildDoor`** (Blueprint child) is data and presentation only: meshes and materials, plus the `BPI_Interactable` implementation.
  - `Interact(Interactor)` → `ToggleDoor(Interactor)`;
  - `GetInteractionType` returns Press (`NewEnumerator0`);
  - `GetHoldDuration` returns 0.
- **Why a Blueprint child:** the interaction interface is a Blueprint interface (section 1.4, item 6), and native classes cannot implement it. The child declares it with no logic of its own. No unrelated Blueprint is duplicated.
- **How the interface gets onto `BP_BuildDoor`** (decision 10.2-1): the MCP toolsets cannot add an interface to a Blueprint (verified in C8B). Options:
  - (a) **Recommended:** a one-time manual editor step by the user, *Class Settings → Implemented Interfaces → Add `BPI_Interactable`*, on the freshly created `BP_BuildDoor`; then the forwarding graph is written through MCP. This is the precedent of G7, where `BP_StorageContainer`'s interface wiring was a manual editor step.
  - (b) A native `UNullTideInteractable` C++ interface plus a change to `PlayerInteractionComponent` to also accept it. This is larger, and it touches a protected production interaction asset.
- The prototype `BP_Door` is not reused and not modified.

### 2.5 Placed Pieces

- `UBuildingPieceComponent` (on any actor):
  - `BuildingDefinition`, `State`, its occupancy key list, `Facing`;
  - `bPlayerBuilt` (set only by the construction authority);
  - `BuilderId` (reserved for NPCs).
  - Registration follows section 2.3.
- `ABuildingPieceActor`: thin native host (unscaled root, mesh, piece component; no tick), the same shape as `ACraftingStationActor`.
  - It provides `SetProvisional(bool)`: hidden in game, actor collision off, `CanEverAffectNavigation` off for its primitives, and interaction bounds disabled while provisional.
  - Floor and wall are Blueprint children with placeholder engine meshes. The door is in section 2.4.
- **Storage and Workbench later:** `BP_BuildStorage` or `BP_BuildWorkbench` = the existing actor with a `UBuildingPieceComponent` (and `ABuildingPieceActor`-equivalent provisional handling). `UStorageComponent` and `UCraftingStationComponent` keep their authority unchanged. Demolition rules (section 6) protect their contents and crafts.

---

## 3. Placement System and Input Flow

### 3.1 Recommended input strategy (minimal controller change)

- **New `IMC_Building` (priority 1, separate from `IMC_Default`).** `UBuildingPlacementComponent` adds it through the local player's `UEnhancedInputLocalPlayerSubsystem` and binds its actions natively, the same way the character adds `IMC_Default`. **No `IMC_Default` or `BP_TopDownController` change.**
  - `IA_BuildToggle` — `B`: enter or exit building mode. This one mapping is always active (it lives in `IMC_Building`, which is added at possession; the other actions are ignored while building mode is off).
  - `IA_BuildConfirm` — Left mouse button.
  - `IA_BuildCancel` — Right mouse button (works in PIE, where `Esc` stops the session).
  - `IA_BuildRotate` — `R` (and mouse wheel later).
  - `IA_BuildSelect` — `1`/`2`/`3` (Floor/Wall/Door) for the MVP build bar.
- `B`, `R`, `1`–`3` and the mouse buttons are currently unused, so there are no conflicts.

### 3.2 Coexistence with existing input

| Existing input | In building mode |
| --- | --- |
| WASD move, Space jump | Still work. The player walks to place pieces; the preview follows the cursor. |
| Click-to-move | Not wired today, so there is no conflict. If it is restored later, `IMC_Building` (higher priority) consumes LMB while building mode is active. |
| `F` interact, `I`/`Tab` inventory, storage and station screens | Opening any UI-only screen **exits building mode** (preview removed) before the screen takes input (approved with the separate `IMC_Building`). This avoids LMB conflicts and the cursor-hiding close path. |
| `Esc` | Closes screens as today. In a packaged game `Esc` also cancels building mode; in PIE use RMB. |
| UI-only screens | Building mode never runs while a screen is open (see above). |

### 3.3 Flow

1. `B` enters building mode: show the cursor (remember the previous state), add the HUD build bar, select the last used or first registered building, spawn one ghost preview actor.
2. Every frame: trace under the cursor (`GetHitResultUnderCursorByChannel`, visibility), snap to a cell or edge, apply rotation, ask `UBuildingSubsystem::ValidatePlacement` (cheap, read-only), and tint the ghost green or red. A short reason text appears on the HUD ("Blocked", "Too far", "Not enough Wood").
3. `R` rotates, and `1`/`2`/`3` change the selection.
4. LMB calls `UConstructionComponent::TryBuild(Definition, Placement)` once; the result is shown from `OnBuildResult`. Building mode stays active for repeated placement.
5. RMB, or `B` again, exits: destroy the ghost and restore the cursor state. `EndPlay` and possession changes also exit.

The ghost preview is a client-side `ABuildingPreviewActor` (mesh copied from the piece class CDO, translucent green/red material, **no collision, no piece component, never registered**).

---

## 4. Placement Validation

### 4.1 Rules and the B1/B2 Boundary

**B1: `UBuildingSubsystem::ValidatePlacement`** (implemented; read-only; no inventory or construction dependency). It checks, in this order:

| Rule | Check (as implemented) | Result code |
| --- | --- | --- |
| Definition structure | Concrete, valid `UBuildingDefinition` (`ValidateBuilding`). | `InvalidDefinition` |
| Placement mode | Mode equals the definition's; an edge axis only for `GridEdge`; rotation 0–3; a builder exists. | `InvalidPlacement` |
| Placement distance | 2D distance from the builder to the piece center ≤ build range (default 600 cm, or the definition's override). | `OutOfRange` |
| Ground, slope, layer | Pawn-channel traces at the footprint corners (inset) and center, or along both sides of an edge. Support is `WorldStatic` or an `Active` Foundation piece; a solid non-support actor at a sample → `Blocked`. Normal Z ≥ cos 30°; spread ≤ 25 cm; surface = highest sample; `Layer = RoundToInt(SurfaceZ / 50)`. | `NoGround`, `Blocked`, `TooSteep`, `Uneven` |
| Occupancy | All keys (`Floor`/`Edge`/`Furniture`, with layer) free, except an explicitly ignored reservation. | `Occupied` |
| Collision | Box from 1 cm above the support surface (floor 10 cm, wall 250 cm; shrunk 2 cm; walls shortened at the ends); Pawn-channel overlap, blocking hits only (overlap-only volumes never block); the builder and the caller's `IgnoreActor` are excluded; the ground is excluded geometrically, not by identity. | `Blocked` (with the blocking actor) |
| Builder safety | The builder's root collision must not touch the box inflated by 5 cm. | `BlocksBuilder` |

**B2: `UConstructionComponent`** (not implemented; B2 scope). Around the B1 validation it adds:

| Rule | Check | Result code |
| --- | --- | --- |
| Registration | The definition is in the builder's `AvailableBuildings`. | `NotAvailable` |
| Piece class | `PieceActorClass` derives from `ABuildingPieceActor` (B2 contract, review R1). | `InvalidDefinition` (with a diagnostic) |
| Inventory | `CanApplyDefinitionTransaction(Cost, {})` on the builder's inventory (preflight), then the atomic `TryApplyDefinitionTransaction(Cost, {})` as the **last** step (the payment, after activation). | `InvalidInventory`, `InsufficientResources`, `ResourcesRejectedRolledBack` |
| Transaction | Reentrancy guard, reservation, provisional spawn, revalidation, activation, integrity check, payment, fail-closed rollback (section 5.2.3, approved). | `BuildInProgress`, `SpawnFailed`, `TransformMismatch`, `BuildInterrupted`, … |

### 4.2 Preview versus final validation

- The preview (B4) calls the same B1 `ValidatePlacement` (plus the B2 registration and inventory preflight for the HUD reason), but only for feedback; its result is never stored or trusted.
- `TryBuild` (B2) re-runs B1 validation and the B2 preflight inside the construction guard. It then reserves the keys, spawns the piece **deferred with `AlwaysSpawn` as a provisional actor** (hidden, no collision, no navigation, no interaction), and revalidates collision manually at the exact snapped transform; the engine's spawn adjustment is never used. Only then does it commit the cost through the inventory transaction, which re-plans from live contents. Any failure rolls back (section 5). A green preview therefore never guarantees construction; a world or inventory change between preview and click is caught.

### 4.3 Blocking paths

- MVP rule: reject only if the builder would be trapped (`BlocksBuilder`). The dynamic nav mesh rebuilds around walls.
- A "does this enclose the player or cut off resources" path check (nav query) is deferred (approved: no support system; path safety limited to `BlocksBuilder`).

### 4.4 Validation Semantics (refined per review)

**Spawn collision handling.**
- The authority does **not** rely on the engine's spawn adjustment:
  - `AdjustIfPossibleButDontSpawnIfColliding` can nudge a piece off the grid;
  - `DontSpawnIfColliding` fails without a reason.
- Instead the piece is spawned deferred with `AlwaysSpawn` in the `Provisional` state (no collision), then **our own** overlap test runs at the exact grid transform. That test excludes the provisional actor itself and the ground or floor it stands on, and checks the `WorldStatic`, `WorldDynamic`, `Pawn` and `PhysicsBody` object types.
- After `FinishSpawning` the authority verifies that the actor's transform equals the resolved grid transform (tolerance 0.1 cm and 0.01°). Any drift (for example, from a construction script) fails with `TransformMismatch` and rolls back.

**Different terrain heights.**
- Ground and surface traces run at the four footprint corners (inset 5 cm) plus the center, from `SurfaceZ + 300` down 600 cm.
  - Every sample must hit `WorldStatic` geometry or an `Active` floor piece.
  - Normal Z ≥ cos 30° (`TooSteep` otherwise).
  - Highest minus lowest sample ≤ 25 cm (`Uneven` otherwise).
- A cell straddling the plateau edge or a ramp is rejected; a cell fully on the plateau or the ground is accepted on its own layer.
- Walls use samples along the edge (both ends and the midpoint) with the same rules. A wall cannot hang over the plateau edge.

**Player trapping.**
- `BlocksBuilder`: the piece's collision box must not overlap the builder's capsule (expanded by 5 cm).
- `Blocked` also covers any other pawn.
- Full enclosure (walls around the player with no door) is not prevented in the MVP. The door (B5) and demolition (B6) are the ways out; see decision 10.2-4 about checkpoint order.

**Dynamic navigation after building.**
- `RuntimeGeneration=Dynamic` is verified. Active pieces affect navigation through their collision primitives; provisional pieces do not.
- Floors are 10 cm high, under the agent's default 45 cm max step height, and walkable.
- Walls block. A closed door blocks; an open door leaf moves out of the doorway and the dirty area rebuilds.
- B5 verifies with PIE pathing. Automated nav-mesh queries need built nav data in a transient world; whether that is feasible is decided in B5 (otherwise PIE only).

**Concurrent and reentrant requests.**
- Everything runs on the game thread; there is no true concurrency.
- **Same builder:** one `bBuildInProgress` guard shared by `TryBuild` and `TryDemolish`. A nested request (for example from an `OnInventoryChanged` or `OnBuildResult` listener) returns `BuildInProgress` without side effects or notifications. A later independent request works normally.
- **Different builders** (a future NPC reacting to an event during another builder's commit): the first builder holds `Reserved` occupancy keys from reservation until activation (then the keys are `Occupied` by its piece) or rollback, so the second builder's `ReserveKeys` fails with `Occupied`. Two pieces can never claim one key.
- **Input repeat:** each LMB press triggers at most one `TryBuild` (pressed trigger, not held), and the guard drops nested calls. Duplicate buildings from one click are impossible.

---

## 5. Construction Authority and Atomicity

`UConstructionComponent` (on `BP_TopDownCharacter`, the same pattern as `UCraftingComponent`):

- `AvailableBuildings` (`EditDefaultsOnly`, authored order, duplicates and invalid entries skipped and reported by `IsDataValid`).
- `GetAvailableBuildings`, `CanBuild(Definition, Placement)` (pure, exact preflight), `TryBuild(Definition, Placement)` (callable), `CanDemolish(Piece)`, `TryDemolish(Piece)`.
- `OnBuildResult(Definition, Result)` and `OnDemolishResult(Piece, Result)` fire exactly once per non-reentrant request.
- `FBuildingPlacement` (USTRUCT): grid anchor (`FIntPoint`), edge axis, rotation, resolved world transform (computed by the subsystem, never trusted from UI without re-snapping).

**`TryBuild` sequence — APPROVED (B2 gate, D1, 2026-10-14): activate first, pay last.** The authoritative sequence is in 5.2.3, the fail-closed rollback in 5.2.4, and the result contract in 5.2.3 step 9. In summary:

1. guard;
2. resolve and validate, including the `ABuildingPieceActor` piece class and the inventory preflight;
3. reserve;
4. spawn deferred `AlwaysSpawn` as provisional;
5. revalidate;
6. activate (commit the reservation, `Active`, visible, collidable);
7. integrity check;
8. **pay**, which is the commit point: one `TryApplyDefinitionTransaction(Cost, {})`;
9. build the result once, broadcast `OnBuildResult` once, and return.

Every failure before step 8 is a fail-closed rollback that never touches the inventory. Nothing fallible runs after the payment.

The B0 baseline (pay, then finalize, with a respawn → refund → world-pickup recovery chain) is **superseded**. Its review is kept in 5.2.2 and its evaluated alternative in 5.2.7 (Option A).

**Guarantees** (the full statement is in 5.2.7):
- **Resources:** consumed only by the one atomic removal at step 8, and only while the piece is `Active` and occupies exactly its keys.
- **Failure before step 8:** leaves the inventory unchanged by B2, the registry clean, and no usable piece. Transactions run by callbacks keep their own effects.
- **Revisions and events:** success means exactly one inventory revision and one `OnInventoryChanged`. A failure means none caused by B2.
- **One piece:** at most one piece per successful call. A provisional piece is never visible, collidable, interactable or counted as occupancy.
- **Item quantities:** never changed outside `UInventoryComponent`.
- **Durability:** nothing is claimed across a crash. Phase 5 persists nothing.

**Result codes:** the B1 codes, with the B2 codes appended (D4; 5.2.12): `BuildInProgress`, `NotAvailable`, `InvalidInventory`, `InsufficientResources`, `SpawnFailed`, `TransformMismatch`, `BuildInterrupted`, `ResourcesRejectedRolledBack`. The B0 codes `SuccessRecovered`, `FinalizeFailedRefunded` and `FinalizeFailedDropped` are dropped; they were never implemented.

### 5.1 Authority Boundaries (preserved)

| System | Stays the only authority for | Building's relation |
| --- | --- | --- |
| `UInventoryComponent` | All item quantities | Building only calls `CanApplyDefinitionTransaction` / `TryApplyDefinitionTransaction` (cost: removals only; refund: additions only). No new inventory code; the inventory never includes building types. |
| `UCraftingComponent` | Crafting and the player's active craft | Untouched. Demolishing a future buildable workbench only destroys the actor; the existing `StationLost` path cancels a dependent craft. |
| `UCraftingStationComponent` / `ACraftingStationActor` | Station context | Untouched. A buildable workbench adds a `UBuildingPieceComponent` beside it. |
| `UStorageComponent` / `UStorageTransferLibrary` | Storage contents | Untouched. Demolition only reads `GetItemCount()` and refuses when it is not zero. |
| `AResourceNodeActor`, `UToolLoadoutComponent` | Gathering and tools | Untouched; they only appear as blocking actors in validation. |
| `PlayerInteractionComponent`, `BPI_Interactable` | Interaction routing | Untouched; the door is a normal `BPI_Interactable` implementer. |
| `UBuildingSubsystem` | Grid, occupancy, placement validation (new) | — |
| `UConstructionComponent` | Build and demolish transactions per builder (new) | — |

### 5.2 B2 Design Gate — APPROVED (user, 2026-10-14; D1–D8, D3 option (a))

The B0 recovery direction after a successful cost commit was **respawn the building → exact inventory refund → world pickups**. It was accepted as a direction only. This section is the gate review. It replaces that chain with an ordering in which **nothing fallible runs after payment** (Option B, approved). Sections 4.1, 5 and 7 (B2) now follow it. No unconditional resource-preservation claim is made beyond the guarantee statement in 5.2.7. The original checklist (respawn, refund, pickup and terminal failure; retention; revision counts; fault injection) is answered in 5.2.3 to 5.2.8.

#### 5.2.1 Source facts verified for this gate (commit `0681261`)

| Fact | Source | Consequence for B2 |
| --- | --- | --- |
| `TryApplyDefinitionTransaction` broadcasts `OnInventoryChanged` **synchronously, inside `TGuardValue bMutationInProgress`**, after `++Revision`. | `InventoryComponent.cpp` transaction tail | Listeners run after the deduction and before the call returns. They cannot mutate this inventory (`Busy`), but they can destroy actors, call other inventories, the subsystem or another builder. |
| Failed transactions (`NotInitialized`, `Busy`, preflight failure, staging failure) return **before** `++Revision` and the broadcast. | same | Every pre-commit failure costs 0 revisions and 0 events. |
| `CanApplyDefinitionTransaction` checks `CanOperate` and the plan, **not** `bMutationInProgress`. There is no public "busy" query. | same | A `TryBuild` nested in this inventory's own broadcast passes the preflight and gets `Busy` only at the commit. This is reachable and must roll back. |
| `TryDropItemToWorld` is resolve → spawn → remove → rollback. The spawn and transform seams are protected virtual. The pickup class comes from a **file-local** `GetWorldPickupClass()`. | `InventoryComponent.h:132-141`, `.cpp:151, 614-740` | Building cannot spawn a world pickup without a new shared helper (an inventory/G6 refactor). |
| `ReserveKeys` is all-or-nothing, purges stale entries first, and returns unique, never-reused ids. | `BuildingSubsystem.cpp:331` | Matches B2 needs. |
| `CommitReservation` fails without change when the handle is stale or foreign, **the owner is no longer valid**, the piece is invalid or registered, or the key sets differ. On success it consumes the handle. | `:381` | Commit is deterministic given a live owner and an unregistered piece with the reserved keys. |
| `ReleaseReservation` matches the owner by `FObjectKey`, so it works even after the owner object became garbage. | `:362` | Rollback can always release its own handle. |
| A reservation is held by its **owner** (the construction component), not by a piece. A dead owner makes it count as free; it is purged on the next `ReserveKeys` or `PurgeStaleEntries`. | `IsOccupantAlive` | Destroying a provisional piece never invalidates the reservation. Owner death does. |
| `UBuildingPieceComponent` unregisters on `EndPlay` and `OnComponentDestroyed`. `InitializePiece` is refused while registered. `SetPieceState` does not touch the registry. A piece registers itself on `BeginPlay` only when `Active` with keys. | `BuildingPieceComponent.cpp` | A committed piece that is destroyed frees its keys by itself. A deferred spawn initialized as `Provisional` never self-registers. |
| `SetProvisional` is actor-wide for hidden and collision. It records navigation primitives at call time. Engine nav relevance requires query collision unless a primitive has `EvenIfNotCollidable` custom geometry. | `BuildingPieceActor.cpp`, engine `UPrimitiveComponent::IsNavigationRelevant` | Blueprint-added primitives created during `FinishSpawning` are also non-colliding and therefore not nav-relevant while provisional. A B2 test asserts this. No B1 change is needed. |

#### 5.2.2 Review of the proposed nine-step transaction (pay, then activate)

| # | Step | Mutation done when it fails | Cleanup | Registry | Actor | Inventory | Result | Notification |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | Guard | none | none | — | — | — | `BuildInProgress` | none (nested) |
| 2 | Resolve and validate (B1 `ValidatePlacement`) | none | none | unchanged | none | unchanged | B1 code | `OnBuildResult` ×1 |
| 3 | Registered, inventory preflight | none | none | unchanged | none | unchanged | `NotAvailable`, `InvalidInventory`, `InsufficientResources` | ×1 |
| 4 | Reserve | none | none | unchanged | none | unchanged | `Occupied` | ×1 |
| 5 | Spawn deferred `AlwaysSpawn`, provisional | reservation | destroy partial actor, release | clean | destroyed | unchanged | `SpawnFailed` | ×1 |
| 6 | Revalidate transform, surface, collision | reservation, actor | destroy, release | clean | destroyed | unchanged | `TransformMismatch`, `Blocked`, `NoGround`… | ×1 |
| 7 | **Commit removal** | reservation, actor | destroy, release | clean | destroyed | unchanged (the commit failed atomically) | `ResourcesRejectedRolledBack` | ×1 |
| — | **window** | **resources deducted**, listeners run | — | — | — | — | — | — |
| 8 | Commit reservation, activate | **payment**, reservation, actor | **recovery chain needed** | — | — | — | — | — |
| 9 | Broadcast | — | — | — | — | — | `Success` | ×1 |

**Finding.** Steps 1–7 are clean and reversible. The window between 7 and 8 is real: `OnInventoryChanged` listeners run after the deduction. In it, the provisional actor can be destroyed, the builder can end play (which makes step 8's `CommitReservation` fail, because the owner is dead), or the world can change. Every post-payment failure needs compensation, and compensation can itself fail. With the nine-step order, no design can promise exact preservation without a durable compensation record, and Phase 5 has no persistence.

#### 5.2.3 Approved design: activate first, pay last (Option B)

Move every fallible step before the payment. **The successful return of the one cost transaction is the commit point.** Nothing after it can fail.

| # | Step | On failure |
| --- | --- | --- |
| 1 | `bBuildInProgress` guard (shared with the future `TryDemolish`). | Nested: return `BuildInProgress`; no side effects, no `OnBuildResult`. |
| 2 | Resolve: definition registered in `AvailableBuildings`; builder inventory resolved and `CanOperate`; B1 `ValidatePlacement(Def, Request, Builder)` (re-snaps the request and never trusts a UI transform); `CanApplyDefinitionTransaction(Cost, {})`. `PieceActorClass` must derive from `ABuildingPieceActor`, because B2 relies on its `SetProvisional` (review R1). B1 accepts any actor that carries a `UBuildingPieceComponent`. Composition hosts such as a buildable Storage or Workbench need a shared provisional contract first, and they are outside the MVP. | Specific code; nothing reserved or spawned. An unsupported piece class returns `InvalidDefinition` with a diagnostic. |
| 3 | `Handle = ReserveKeys(this, Resolved.Keys)`. | `Occupied` (defensive: no code runs between steps 2 and 3, so this cannot be reached in practice). |
| 4 | `SpawnActorDeferred(PieceActorClass, Resolved.WorldTransform, Owner = builder, AlwaysSpawn)` → `InitializePiece(Def, Resolved.Placement, Keys, Provisional, bPlayerBuilt = true)` → `SetProvisional(true)` → `FinishSpawning`. Require a valid, non-garbage actor of class `ABuildingPieceActor` with its piece component. | Destroy any partial actor, `ReleaseReservation`, `SpawnFailed`. |
| 5 | Revalidate: transform equals the resolved one (0.1 cm, 0.01°); `ValidatePlacement(Def, Request, Builder, IgnoreReservation = Handle, IgnoreActor = Piece)` succeeds with identical keys, layer and transform. | Destroy, release, the B1 code or `TransformMismatch`. |
| 6 | **Activate (unpaid):** `SetPieceState(Active)` → `CommitReservation(Handle, this, Piece)` → `SetProvisional(false)`. The keys become `Occupied` by the piece and the handle is consumed. | Commit refused (for example, the owner died): destroy, release, `BuildInterrupted`. |
| 7 | **Integrity check** (extended per review R2). All of the following must hold:<br>• the actor and piece are valid and not pending kill;<br>• the piece is `Active` and occupies exactly the reserved keys: `GetOccupancyKeys()` equals the reserved key set, and `GetOccupyingPiece(Key) == Piece` for every key (review R2-2; `IsPieceRegistered` alone does not prove who holds each key);<br>• the actor is still at the resolved transform (same tolerance as step 5);<br>• the actor is not provisional, not hidden, and has actor collision enabled;<br>• the component has not ended play;<br>• the inventory is still valid.<br>Engine callbacks fired by step 6 (overlap events, collision and navigation updates) could have changed any of these. Two documented limitations (D2) are **not** re-checked:<br>• a world blocker created by such a callback;<br>• per-primitive navigation relevance after activation (review R2-3). `SetProvisional(false)` clears its restored-primitive list, so B2 cannot read it without a B1 change. Navigation is verified in B5 (section 3).<br>The same changes could equally happen one frame after a successful build. | **Fail-closed rollback** (5.2.4); `BuildInterrupted`. |
| 8 | **Pay (commit point):** `Inventory->TryApplyDefinitionTransaction(Cost, {})`. | `Busy`, `InsufficientResources`, `NotInitialized` and so on: fail-closed rollback, `ResourcesRejectedRolledBack`. The inventory is untouched because the commit is atomic. |
| 9 | Success (result contract per reviews R3 and R2-1):<br>• **Construction:** build the result once, **after** `TryApplyDefinitionTransaction` returns. That call returns only after its own `OnInventoryChanged` broadcast, so destruction during payment is already known. The result holds the code `Success`, `Piece` as a weak reference to the paid piece, and the resolved placement.<br>• **Destroyed during payment:** if the piece is no longer valid when the transaction returns, log a Warning and set the diagnostic to "destroyed during payment" (D3).<br>• **Broadcast and return:** broadcast that one result through `OnBuildResult`, inside the guard, then return the same result. The code and diagnostic never change after construction.<br>• **Destroyed during `OnBuildResult`:** a listener may destroy the piece. Then only the weak `Piece` becomes invalid for later listeners and for the caller. No diagnostic is added and no Warning is logged, because this is normal gameplay reacting to a finished build. | — |

**Why this closes the window.** The only work after the deduction is the inventory's own broadcast, our guard reset and our one `OnBuildResult`. At the instant of payment the piece is `Active`, registered, visible and collidable. Whatever a listener does afterwards is the normal lifecycle of a finished building, the same as gameplay destroying it one frame later. No respawn, refund, pickup or pending record is needed.

**What it costs (decision D2).** For the length of one synchronous call, an *unpaid* `Active` piece can be observed by engine callbacks caused by step 6: collision-settings changes, possible overlap begin and end events, and nav-octree dirtying. It is never rendered, because no frame passes. If payment then fails, the piece is destroyed in the same call. That fires the matching end events, and its nav area rebuilds unchanged. B2 itself makes no inventory mutation before step 8 (review R4). A transaction run by a callback, such as an overlap handler or a test seam, is independent: it keeps its own effects, revision and event, and B2 neither undoes nor counts it.

**Content evidence for D2** (read-only MCP inspection, review round 1). No current overlap handler can grant, consume or move items, move actors or change collision because a building piece became active:
- **C++:** no overlap or hit handler in `Source/`.
- **`BP_PickUpItem` `ActorBeginOverlap`:** adds its item only when the other actor has an `InventoryManagerComponent`, so a piece is a no-op.
- **`PlayerInteractionComponent`:** its capsule overlap only adds or removes `BPI_Interactable` actors in the in-range list. Floors do not implement the interface. The B5 door will: an unpaid door that is rolled back is destroyed, which fires the end overlap that removes it. Interaction itself needs player input, so it cannot run inside `TryBuild`.
- **`BP_StorageContainer`, `BP_ResourceNode`:** empty `ActorBeginOverlap`. `BP_ResourceRock`, `BP_ResourceScrap` and `BP_ResourceTree` only call the parent.
- **`BP_Workbench`:** no overlap events.
- **Coverage, completed in round 2:** a read-only scan for overlap and hit event names found no handlers in:
  - `Lvl_TopDown.umap` (the only map, including its Level Blueprint);
  - `BP_TopDownCharacter`, which contains no overlap reference at all;
  - every `__ExternalActors__` and `__ExternalObjects__` package.
  
  The seven assets listed above are the only matches in Content.

Each new piece Blueprint (B3+) must be inspected for the same contract (see 5.2.14).

#### 5.2.4 Fail-closed rollback (steps 6–8)

Run in order; every step is safe to repeat:
1. `SetProvisional(true)` and `SetPieceState(Provisional)`: hidden, no collision, no navigation.
2. `UBuildingSubsystem::UnregisterPiece(Piece)` if it is registered, or `ReleaseReservation(Handle, this)` if the handle is unconsumed. Both are no-ops when there is nothing to remove.
3. `Destroy()`.
4. If the actor is not pending kill after `Destroy()`, which is not expected for a runtime-spawned actor, log an **Error** with the actor, definition and placement. The leftover is inert: hidden, non-colliding, unregistered, `Provisional`, so it is neither a free building nor occupancy. `AuditRegistry` stays clean.

Rollback never touches the inventory, so a rollback failure can never lose or duplicate items.

#### 5.2.5 State transition table (one `TryBuild` call)

| From | Event | To | Registry | Piece actor | Inventory |
| --- | --- | --- | --- | --- | --- |
| Idle | guard taken, validation and preflight pass | Validated | — | — | — |
| Validated | `ReserveKeys` ok | Reserved | keys `Reserved` (owner = component) | — | — |
| Reserved | spawn ok | Provisional | `Reserved` | hidden, no collision, `Provisional`, unregistered | — |
| Provisional | revalidation ok | Revalidated | `Reserved` | same | — |
| Revalidated | activation and integrity ok | Activated (unpaid) | keys `Occupied` by piece; handle consumed | `Active`, visible, collidable | — |
| Activated | payment ok (**commit point**) | Committed → Idle | `Occupied` | `Active` | −Cost, Revision +1, 1 event |
| Reserved / Provisional / Revalidated | failure | RolledBack → Idle | released | destroyed | unchanged |
| Activated | failure (integrity or payment) | RolledBack → Idle | unregistered | deactivated, destroyed (or inert, Error) | unchanged |
| any | nested `TryBuild` | (unchanged) | — | — | — |

A reservation never outlives one `TryBuild` call. Between calls a builder holds no registry state.

#### 5.2.6 Failure and recovery matrix (all cases requested by the gate)

The "Option A" column records the evaluated alternative (pay at step 7, then recover; 5.2.7). It is **not** a fallback: if D1 is rejected, B2 returns to design review before anything is implemented (D8, review R2-4).

| Case | Approved (Option B) | Option A (evaluated alternative; needs a new design review) |
| --- | --- | --- |
| Provisional actor destroyed during `OnInventoryChanged` | Cannot happen: no provisional actor exists at payment. The paid `Active` piece destroyed by a listener → `Success`, Warning, no refund (D3). Keys are freed by its `EndPlay`. | Detected after the commit → recovery chain. |
| Reservation handle invalid | Consumed at step 6 before payment. If the commit is refused (dead owner) → rollback, `BuildInterrupted`, nothing paid. | The commit after payment fails → respawn reserves again or skips to refund. |
| Another actor on the reserved keys | Registry: impossible, because the subsystem refuses (reserved or occupied, all-or-nothing). World blocker spawned before step 5 → `Blocked` pre-payment. A blocker spawned by an engine callback during step 6 is **not** re-validated (documented limitation). | The respawn revalidation fails → refund. |
| Respawn failure | Not applicable (no respawn). | Attempted **once**. Failure = no reservation, spawn null, transform mismatch, blocked, or interrupted activation → refund. |
| Refund `Busy` | Not applicable. | Unreachable after the commit returns (this inventory is free again); handled defensively as a refund failure → pickups. |
| Refund capacity changed | Not applicable. | Full refund first (one transaction); otherwise per-line refunds (one transaction each); remaining lines → pickups. |
| Pickup spawn failure | Not applicable. | That line stays in the pending record. |
| Partial pickups | Not applicable. | Successful lines are removed from the record; failed lines stay; `RecoveryFailed`. |
| Builder `EndPlay` | Before step 8 → rollback, `BuildInterrupted`, nothing paid. During the step 8 broadcast → `Success` (paid, piece `Active`). | Record lost with the component (Error log); not durable. |
| World `EndPlay` | Cannot begin inside a synchronous game-thread call. `Deinitialize` clears the registry between frames. | same |
| Reentrant `TryBuild` (same builder, from `OnInventoryChanged`, `OnBuildResult` or an engine callback) | `BuildInProgress`, no side effects, no `OnBuildResult`. | same |
| Second builder from a callback | Its own guard; same keys → `Occupied` (step 3 reservation or step 6 occupancy); other keys → normal independent build. | same |
| `TryBuild` nested in this inventory's own broadcast | Preflight passes; step 8 returns `Busy` → rollback, `ResourcesRejectedRolledBack`, 0 revisions. | same |
| Retry after an earlier failure | Independent; no state carries over (no reservation or record survives a call). | Retry resolves the pending record first (exactly once per attempt id). |

#### 5.2.7 Terminal-failure contract: options evaluated

**Option A — post-payment recovery chain plus a pending-compensation record.**
- Record: per attempt `FGuid AttemptId`, the builder, the definition, and the remaining `TArray<FInventoryDefinitionQuantity>`. It is decremented only after a verified successful refund transaction or a verified pickup spawn, which makes compensation exactly-once per attempt.
- Result `RecoveryFailed` with an **Error** log (attempt id, definition, remaining lines, builder). A developer API retries the refund later.
- **Limitations:**
  - **runtime only**: lost on builder `EndPlay`, level unload, quit and **crash**, so it is not durable;
  - Phase 7 would have to serialize it;
  - it needs a G6 extraction for pickups;
  - three new fallible branches each need tests;
  - reentrancy rules apply inside every compensation broadcast;
  - the player still sees a deduction without a building until the retry succeeds.

**Option B — reorder so the payment is the last fallible step (recommended).**
- Exactly-once holds by construction: one atomic transaction is the commit point.
- No partial recovery or retry state exists.
- The reentrancy surface is the same as crafting's.
- Object lifetime is checked before payment only.
- Survival across level unload, quit or crash: nothing is in flight between frames. A crash loses the inventory and the buildings of the session together; Phase 5 persists neither.
- **Phase 7:** if saves run between frames, a save sees either (not paid, no piece) or (paid, `Active` piece), and there is no compensation record to persist. B2 does not enforce this; it is a Phase 7 requirement: a save must never run inside `TryBuild`, for example from a listener.
- **Limitations:**
  - the unpaid-active observation interval (D2);
  - no refund if gameplay destroys the paid piece inside the payment broadcast (D3);
  - blockers created by engine callbacks during activation are not re-validated.

**Honest guarantee statement (if B is approved).** Within a `TryBuild` call, resources are deducted only by the one atomic transaction, and only while the built piece is `Active` and registered. Every failure before it leaves the inventory unchanged by B2, the registry clean and no usable piece. Transactions run independently by callbacks keep their own effects (R4). Nothing is claimed about durability across a crash: Phase 5 has no persistence for anything.

#### 5.2.8 Inventory revision and `OnInventoryChanged` matrix (per `TryBuild` call; builder inventory)

| Case | Option B: Revision / events / `OnBuildResult` | Option A: Revision / events |
| --- | --- | --- |
| Validation failure | 0 / 0 / 1 | 0 / 0 |
| Reservation failure | 0 / 0 / 1 | 0 / 0 |
| Spawn failure | 0 / 0 / 1 | 0 / 0 |
| Transform mismatch, collision change | 0 / 0 / 1 | 0 / 0 |
| Cost rejection at commit (`Busy`, short) | 0 / 0 / 1 | 0 / 0 |
| Interrupted activation | 0 / 0 / 1 | n/a |
| Success | +1 / 1 / 1 | +1 / 1 |
| Respawn recovery | n/a | +1 / 1 |
| Full refund | n/a | +2 / 2 |
| Per-line refund (k lines succeed) | n/a | +1+k / 1+k |
| Pickup compensation only | n/a | +1 / 1 (pickups do not touch the inventory) |
| Partial pickup | n/a | +1+k / 1+k (k refunded lines) |
| Terminal failure | n/a | +1 / 1 plus any refunded lines |
| Nested request | the nested call 0 / 0 / 0; the outer call is unchanged | same |

A test that mutates the inventory itself (a fault-injection removal) adds exactly its own +1 / 1. All quantities change only through `UInventoryComponent`; building never touches stacks.

#### 5.2.9 Occupancy and lifecycle contract

- **B1 API sufficiency:** sufficient. B2 uses only `ValidatePlacement`, `ReserveKeys`, `CommitReservation`, `ReleaseReservation`, `UnregisterPiece`, `IsPieceRegistered`, `GetOccupyingPiece`, `UBuildingPieceComponent::GetOccupancyKeys`, `AuditRegistry`, `InitializePiece`, `SetPieceState` and `SetProvisional`, with no bypass. **No B1 API change is required.** The only B1 file change is appending the B2 `EBuildResult` codes (already planned in B1).
- **Ownership and identity:** the owner is the `UConstructionComponent`; the handle is a unique `int64` id, never reused, held in a local variable for one call.
- **Release:** at steps 4–6 on any failure (`ReleaseReservation`, which works even if the owner became garbage). Otherwise consumed by `CommitReservation` at step 6.
- **When keys become occupied:** at step 6, before payment, with holder = piece.
- **Piece `EndPlay`:** cannot invalidate a reservation (the holder is the owner). After step 6 it unregisters its occupancy, which is also the rollback path.
- **Forced piece destruction:** before step 6 the handle is released by rollback; after step 6 the piece's `EndPlay` unregisters; after payment it is normal destruction.
- **Owner `EndPlay`:** sets `bEndPlayed`, which the integrity check reads. When idle there is nothing to clean. In flight, the reservation is stale and released by rollback (and would be purged anyway).
- **Audit:** `AuditRegistry()` is empty and `GetReservedKeyCount() == 0` after every B2 test call.
- **Duplicate prevention:**
  - the guard;
  - all-or-nothing reservation;
  - `CommitReservation` refusing registered pieces;
  - `InitializePiece` refused while registered;
  - one spawn per call.

#### 5.2.10 Test matrix `NullTide.Building.B2.*` (transient world; native fixtures only)

All run in the CQTest-style world fixture (`FB1World` pattern). Each asserts the result code, exact inventory contents, the Revision delta, the `OnInventoryChanged` and `OnBuildResult` counts, the live piece-actor count, `AuditRegistry` empty and reserved keys 0.

**Deterministic injection.**
- Fixture piece actor classes in `BuildingTestTypes.h`:
  - one that destroys itself in `BeginPlay` (spawn failure);
  - one that offsets itself in `OnConstruction` (transform drift);
  - one that spawns a blocking box at its own location in `BeginPlay` (collision change).
- A test subclass `UConstructionTestComponent` overriding two protected virtual seams in the G6 style: `SpawnProvisionalPiece` (may return null) and `ActivatePiece` (calls Super, then runs a test lambda).
- Test listeners on `OnInventoryChanged` and `OnBuildResult`.
- No production debug switch.

| Test | Scenario and key assertions |
| --- | --- |
| `Success` | Exact cost removed; +1 / 1 / 1; the piece is `Active`, registered with exact keys, visible and collidable; `bPlayerBuilt`. |
| `ValidationFailures` | `InvalidDefinition`, `NotAvailable`, `OutOfRange`, `Occupied` (existing piece and another owner's reservation), `Blocked`; each 0 / 0 / 1, no actor spawned. |
| `InventoryPreflight` | `InsufficientResources`, `InvalidInventory` (no inventory, `NotInitialized`); 0 / 0 / 1; nothing reserved. |
| `SpawnFailure` | Self-destroying class and seam returning null → `SpawnFailed`; reservation released. |
| `TransformMismatch` | Drifting class → `TransformMismatch`; piece destroyed; keys free. |
| `CollisionChangedDuringSpawn` | Blocker-spawning class → `Blocked` at revalidation; 0 / 0 / 1. |
| `BlockerAfterPreview` | `CanBuild` success, the test spawns a blocker, then `TryBuild` → `Blocked`. |
| `InventoryBusyAtCommit` | `TryBuild` from the builder inventory's `OnInventoryChanged` → `ResourcesRejectedRolledBack`; only the trigger's +1 / 1; piece destroyed; keys free. |
| `ResourcesChangedDuringActivation` | The `ActivatePiece` seam removes Wood through the inventory → payment short → rollback; only the seam's +1 / 1. |
| `ActivationInterrupted` | The seam destroys the piece, then (separately) ends the builder's play → `BuildInterrupted`; 0 / 0 / 1. |
| `PieceDestroyedAfterPayment` | An `OnInventoryChanged` listener destroys the paid piece → every `OnBuildResult` listener and the caller see `Success`, an invalid `Piece` and the "destroyed during payment" diagnostic; one Warning; +1 / 1 / 1; keys free. |
| `PieceDestroyedDuringBuildResult` | Two ordered `OnBuildResult` listeners: the first sees a valid `Piece` and destroys it; the second and the caller see `Success`, an invalid `Piece` and an empty diagnostic; no Warning; +1 / 1 / 1; keys free (R3, R2-1). |
| `BuilderEndedDuringPayment` | A listener destroys the builder pawn → `Success`; the piece stays `Active`; the guard is released; `OnBuildResult` fires exactly once and the caller receives the same result (R3). |
| `ActivationCallbackAltersPiece` | The `ActivatePiece` seam (after Super) moves the piece off the resolved transform, hides it, disables its collision, sets it provisional or unregisters it, each in its own case → `BuildInterrupted`, 0 / 0 / 1, piece destroyed, keys free, `AuditRegistry` empty (R2, R2-2). |
| `UnsupportedPieceClass` | A definition whose `PieceActorClass` is a plain `AActor` with a `UBuildingPieceComponent` → `CanBuild` and `TryBuild` both return `InvalidDefinition`; nothing is reserved or spawned (preflight parity, R1). |
| `Reentrancy` | Nested `TryBuild` from `OnInventoryChanged` and from `OnBuildResult` → `BuildInProgress`, 0 extra events; one piece. |
| `TwoBuilders` | B targets A's keys from A's seam → `Occupied`; B on other keys succeeds; each inventory +1 / 1. |
| `ProvisionalInvariants` | Inside the seam before activation: hidden, actor collision off, no nav-relevant primitive, `Provisional`, unregistered, keys `Reserved`, `GetOccupyingPiece` null. |
| `RetryAfterFailure` | After each failure kind, the same placement then succeeds (no residue). |
| `ExactlyOnce` | Over a mixed sequence, `OnBuildResult` count = non-nested call count; Revision delta = success count. |
| **B1 regression** `NullTide.Building.B1.OverlapOnlyVolumes` | Overlap-only sphere (pickup-like) and box (interaction-bounds-like) over the footprint → `ValidatePlacement` `Success`; a `BlockAllDynamic` control → `Blocked`. Profiles mirror the production ones, confirmed read-only at implementation time. |

**Not unit-testable without an extra seam (covered by code review):** `ReserveKeys` failing after a successful validation (unreachable), and `Destroy()` failing in rollback (the inert-actor Error path).

#### 5.2.11 G6 pickup-helper extraction: assessment

- **Option B: do not extract.** No building path spawns pickups. G6 and the inventory are untouched, and the previously planned B2 refactor is removed from scope.
- **Option A only:** extract a native-only `SpawnWorldPickupForDefinition(World, Definition, Quantity, Transform)` holding today's file-local `GetWorldPickupClass` plus the body of `SpawnAndConfigureWorldDrop`:
  - preserved: deferred spawn with `AdjustIfPossibleButDontSpawnIfColliding`, reflected `ItemDefinition`/`Quantity`, destroy on failure, `FinishSpawning`, `RefreshWorldPresentation`;
  - the G6 virtual seams keep routing through it;
  - building would also need its own transform resolution (the current one is pawn-based);
  - validated by the unchanged G6 suite plus a helper test;
  - risks: test subclass overrides and the reflected property names.

#### 5.2.12 Exact B2 scope (approved, Option B)

- **New:** `Building/ConstructionComponent.h/.cpp`:
  - `AvailableBuildings` (with `IsDataValid`);
  - `GetAvailableBuildings`, `CanBuild` (exact read-only preflight: steps 2 and B1 validation), `TryBuild` (5.2.3);
  - the `OnBuildResult` delegate;
  - the guard, `EndPlay` flag, and two protected virtual seams.
  - The result struct carries the code, the placed actor as a weak reference (R3), the resolved placement and a diagnostic.
- **Changed:** `BuildingTypes.h` appends `BuildInProgress`, `NotAvailable`, `InvalidInventory`, `InsufficientResources`, `SpawnFailed`, `TransformMismatch`, `BuildInterrupted` and `ResourcesRejectedRolledBack`. The B0 codes `SuccessRecovered`, `FinalizeFailedRefunded` and `FinalizeFailedDropped` are dropped (never implemented).
- **Tests:** `BuildingTestTypes.h` fixtures, a new `ConstructionTests.cpp` (5.2.10) and the B1 overlap regression.
- **Not in B2:**
  - `TryDemolish`/`CanDemolish` (B6; same guard);
  - assets, `BP_TopDownCharacter`, map or World Partition changes (B3+);
  - input or UI;
  - any change to inventory, G6, crafting, storage or interaction source.

#### 5.2.13 Decisions — APPROVED by the user (2026-10-14)

**Approved:** D1, D2, D4, D5, D6, D7 and D8 as written below, and **D3 option (a)**, the recommended option. Under D3 (a):
- the result stays `Success`;
- the weak `Piece` reads invalid;
- destruction during payment adds a Warning and the diagnostic;
- there is no refund.

The distinct-code alternative is not adopted.

- **D1:** adopt Option B (activate first, pay last; the payment is the commit point) and drop the post-payment recovery chain (respawn → refund → pickups) together with its three result codes.
- **D2:** accept that an unpaid `Active` piece is observable by synchronous engine callbacks for the length of one call (never rendered; destroyed if payment fails).
- **D3 (revised after review):** a paid piece may be destroyed during payment (by an `OnInventoryChanged` listener) or during `OnBuildResult`.
  - **Recommended:** the result stays `Success`, because the transaction committed and the piece was built. `Piece` is a weak reference that reads invalid once the piece is gone. Only destruction **during payment** adds a Warning and the "destroyed during payment" diagnostic, because it is known when the result is built. Destruction during `OnBuildResult` changes nothing but the weak reference (step 9).
  - **Alternative (raised by the review):** a distinct result code for destruction during payment. It cannot cover destruction during `OnBuildResult`, because the result has already been broadcast.
  - **No refund in either case.** This is gameplay policy and needs explicit approval. "Materials are deducted only when the build succeeds" (user direction) holds: the build did succeed, and another system destroyed the finished piece afterwards.
- **D4:** the new result code `BuildInterrupted`, plus the appended codes in 5.2.12.
- **D5:** test seams = two protected virtual hooks (`SpawnProvisionalPiece`, `ActivatePiece`) plus fixture actors; no debug switches.
- **D6:** no G6 extraction in B2.
- **D7:** B2 is build-only; demolition stays in B6.
- **D8 (revised after review):** Option A (5.2.7: a runtime-only pending-compensation record, `RecoveryFailed`, the G6 extraction) stays documented as the evaluated alternative. It is **not** a pre-approved fallback, because its record is not durable. If D1 is rejected, B2 returns to design review.

#### 5.2.14 Independent review round 1 (Codex, against `ab6a1d2`)

Codex re-verified every 5.2.1 source fact and confirmed it. Codex also confirmed:
- the B1 API is sufficient;
- B2 needs no change to inventory, G6 or crafting source;
- the dropped B0 codes do not exist in `EBuildResult`.

Finding resolution by Claude:

| ID | Severity | Resolution | Change |
| --- | --- | --- | --- |
| R1 | P1 | **CONFIRMED.** `PieceActorClass` is `TSubclassOf<AActor>`. `ValidateBuilding` only requires a `UBuildingPieceComponent` (`BuildingDefinition.cpp:99-111`). B2 needs `ABuildingPieceActor::SetProvisional`. | Step 2 preflight requires `ABuildingPieceActor` (`InvalidDefinition` + diagnostic); test `UnsupportedPieceClass`. No B1 change. |
| R2 | P1 | **CONFIRMED** (spec gap). The read-only content audit found no current handler that triggers it. | Step 7 also checks the transform, the provisional flag, hidden and actor collision; test `ActivationCallbackAltersPiece`. A callback-created blocker stays a documented limitation under D2, because the same blocker could appear one frame after a successful build. |
| R3 | P1 | **CONFIRMED** (spec gap). | The result `Piece` is a weak reference (construction timing corrected in round 2, R2-1); tests `PieceDestroyedDuringBuildResult` and an extended `BuilderEndedDuringPayment`. |
| R4 | P2 | **CONFIRMED.** The wording contradicted the `ResourcesChangedDuringActivation` test. | The guarantee now covers B2's own mutations only. |
| D3 | — | **NEEDS USER DECISION.** | Revised options in 5.2.13. |
| D8 | — | **CONFIRMED.** | Option A is no longer a pre-approved fallback. |

Also accepted from the review:
- The Phase 7 "saves between frames" point is now stated as a Phase 7 requirement, not a B2 guarantee.
- **Evidence for future checkpoints:**
  - B3+ inspects each new piece Blueprint, read-only through MCP before review, for construction-script or added primitives that bypass `SetProvisional`, custom navigation geometry, and overlap handlers.
  - B5 re-checks `PlayerInteractionComponent` with the door.
- **Plan hygiene (done on 2026-10-14, after D1 was approved):** the following were rewritten to match 5.2.3, 5.2.10 and 5.2.12, before any B2 code:
  - the B0 sequence, recovery and result-code text in section 5;
  - the B0 B2 entry in section 7;
  - the transaction row in 4.1;
  - 10.3, which is marked as superseded.

The test matrix is now 20 `NullTide.Building.B2.*` tests plus the B1 regression.

**User direction (2026-10-14, after round 1)** (consistent with D1 and with the B4 UX in section 3):
- Construction is **instant**. There is no build timer: the piece is placed with the mouse cursor on the grid cells and is done.
- Materials are deducted **only when the build succeeds**, never for an unfinished build.

D1–D8 are formally decided after the independent review round 2. D3 is still open.

#### 5.2.15 Independent review round 2 (Codex, uncommitted revision on `ab6a1d2`)

Round-1 closure reported by Codex:
- **CLOSED:** R1 and R4.
- **NOT CLOSED:**
  - R2, until navigation was addressed (R2-3);
  - R3, until the result timing was fixed (R2-1);
  - D8, until the wording was fixed (R2-4).

Codex also confirmed:
- a weak `Piece` in a `BlueprintType` result struct is an existing UE 5.8 reflected pattern;
- the four new or extended tests can be injected deterministically;
- the instant, cursor-based, no-timer direction matches D1 and the B4 UX.

Finding resolution by Claude:

| ID | Severity | Resolution | Change |
| --- | --- | --- | --- |
| R2-1 | P1 | **CONFIRMED.** `TryApplyDefinitionTransaction` broadcasts before it returns (`InventoryComponent.cpp:988-992`). "Built at the commit point" was therefore ambiguous, and a shared diagnostic for destruction during `OnBuildResult` cannot be delivered. | Step 9: the result is built once after the transaction returns. Destruction during payment adds a Warning and a diagnostic. Destruction during `OnBuildResult` only invalidates the weak `Piece`, with no Warning. `PieceDestroyedAfterPayment` and `PieceDestroyedDuringBuildResult` now assert what each listener and the caller receive. D3 text aligned. |
| R2-2 | P2 | **CONFIRMED.** `IsPieceRegistered` only checks for a registry record. `GetOccupyingPiece` and `GetOccupancyKeys` exist (`BuildingSubsystem.h:95`, `BuildingPieceComponent.h:38`). | Step 7 compares the key set and every key's occupant. 5.2.9 lists both queries. `ActivationCallbackAltersPiece` adds an "unregisters" case. No B1 change. |
| R2-3 | P2 | **Resolved as an explicit deferral.** After `SetProvisional(false)` the restored-primitive list is cleared (`BuildingPieceActor.cpp:44-51`), so B2 cannot check it without a B1 change. No current handler changes navigation. | Step 7 documents it as a D2 limitation. Navigation is verified in B5. |
| R2-4 | P2 | **CONFIRMED.** | The 5.2.6 introduction and column heading now call Option A an evaluated alternative that needs a new design review. |
| D2 coverage | — | **Evidence completed.** Codex noted that `BP_TopDownCharacter`, the map or Level Blueprint, and the external packages were not covered. | A read-only scan found no overlap or hit handlers there (5.2.3 content evidence). |

Status after round 2: R1–R4, R2-1 to R2-4 and D8 are resolved in this text.

**User decision (2026-10-14):** D3 option (a); D1, D2 and D4–D8 approved. The gate is closed. B2 is implemented exactly as 5.2.3–5.2.12 describe.

Status: **APPROVED (user, 2026-10-14) after independent review rounds 1 and 2.** B2 is implemented as described (see `BUILDING_PROGRESS.md`, B2). No G6 change or helper extraction exists.

**NPC extensibility:** the authority takes its builder from its owner, and the subsystem is world-level. A future colonist with `UInventoryComponent` and `UConstructionComponent` (or a job system calling the same subsystem plus inventory transaction) builds through the same rules, without a second authority.

---

## 6. Demolition and Refund

| Rule | Proposal |
| --- | --- |
| What can be demolished | Only `Active` pieces with `bPlayerBuilt = true` (set by `TryBuild`). `Provisional` and `Removing` pieces are refused. Authored map actors (`BP_StorageContainer`, `C8B_Workbench`, pickups, nodes) are never demolishable: they have no piece component or `bPlayerBuilt = false`. |
| How | A demolish mode inside building mode (`X` toggles, LMB on a piece; approved), routed to `UConstructionComponent::TryDemolish`. Range rule as for building (600 cm). |
| Refund | `floor(Cost.Quantity × 0.5)` per cost line (approved); lines that round to 0 are skipped. Floor (Wood ×4) refunds Wood ×2, Wall (Wood ×6) Wood ×3, Door (Wood ×8 + Scrap ×2) Wood ×4 + Scrap ×1. |
| Capacity | `CanApplyDefinitionTransaction({}, Refund)` must succeed; otherwise demolition is **refused** (`RefundDoesNotFit`) and nothing changes (approved). |
| Atomicity | Guard → validate → set `State = Removing` → `TryApplyDefinitionTransaction({}, Refund)` → on success `Destroy` the piece (the piece unregisters in `EndPlay`). If the refund commit fails, set the state back to `Active`; the piece stays and nothing changed. A refund that rounds to nothing destroys the piece without a transaction. One revision and one event when there is a refund. |
| Storage with items | Refused while `UStorageComponent::GetItemCount() > 0` (`StorageNotEmpty`). Contents are never deleted. |
| Active Workbench | Allowed (approved). The existing C8A behaviour cancels a dependent timed craft with `StationLost` (nothing consumed), and the C8C station screen closes itself. |
| Door, wall supporting nothing | Allowed (no support model in the MVP). |
| NPC jobs (later) | `UBuildingPieceComponent` reserves a builder or owner ID; demolition permissions can be extended without changing the authority. |

---

## 7. MVP Scope and Checkpoints

**Smallest playable loop:** `B` → pick Floor → green or red ghost on a 200 cm grid → LMB builds a wooden floor, consuming Wood → RMB exits. Walls and doors follow, then demolition.

**Single test structure first:** yes. B3 delivers only the Wooden Floor end to end through the authority (no input or UI yet), so the data model, validation, spawn-first atomicity and rollback are proven before any UX or edge placement.

### B1 — Data Model, Grid Coordinates, Occupancy and Validation (logic only) — COMPLETE / APPROVED
**Scope (approved to stay small):** data model, grid coordinate contract, occupancy registry and placement validation logic. No construction transaction, no spawning by an authority, no input, no UI, no production assets.
- **Source:**
  - `Building/BuildingTypes.h`: `FBuildingPlacement`, `FBuildingOccupancyKey`, `FBuildingReservationHandle`, `EBuildResult`, `EBuildingCategory`, `EBuildingPlacementMode`, `EBuildingOccupancyCategory`, `EBuildingEdgeAxis`, `EBuildingPieceState`.
  - `Building/BuildingDefinition.h/.cpp`: definition, `ValidateBuilding`, `IsDataValid`.
  - `Building/BuildingPieceComponent.h/.cpp`: state, keys, facing, `bPlayerBuilt`, unregister on `EndPlay` and destroy; register on `BeginPlay` only when `Active`.
  - `Building/BuildingPieceActor.h/.cpp`: thin host and `SetProvisional`.
  - `Building/BuildingSubsystem.h/.cpp`:
    - pure grid math (world↔cell, edges, snapping, rotation, layers);
    - the registry (reserve, commit, release, register-active, unregister, query, audit);
    - `ValidatePlacement` (definition, range, ground, slope, unevenness, occupancy, collision, builder safety; no inventory rule).
- **Assets:** none (native test fixtures only). `ABuildingDoorActor` is **not** in B1.
- **Tests `NullTide.Building.B1.*`:**
  - `DefinitionValidation`: costs (null, abstract, invalid item data, quantity < 1, duplicates), actor class without a piece component, footprint < 1, rotation step not 90, refund outside 0..1.
  - `GridCellMath`: positive, negative and boundary coordinates, cell centers, round trips.
  - `GridEdgeSnapping`: four sides, the tie-break order, canonical keys for shared edges, negative cells.
  - `RotationMapping`: footprint rotation for 1×1 and 2×1, edge yaw, exact multiples of 90°.
  - `LayerMapping`: ground 0, floor top 10 cm stays layer 0, plateau 200 cm is layer 4.
  - `OccupancyLifecycle`: reserve all-or-nothing; commit; release; register-active from `BeginPlay`; unregister on `EndPlay` and destroy; idempotent registration; different keys refused for the same piece; stale reservation and piece purge; `AuditRegistry` clean after every scenario.
  - `OccupancyCategories`: floor and wall at the same X/Y coexist; wall and door on the same edge conflict; two layers in one column coexist.
  - `PlacementValidation` (transient world): no ground; slope; uneven straddling a 200 cm step; blocked by a static actor, a pawn and a physics body; `BlocksBuilder`; out of range; occupied; valid floor on ground and on a raised platform.
- **PIE:** none. **Content/WP:** no change. **Cook:** yes (source only).
- **Implemented** (details in `BUILDING_PROGRESS.md`, B1). Choices made during implementation:
  - **Result codes:** `EBuildResult` contains only the B1 validation codes, plus `InvalidPlacement` for shape errors and a missing builder. The B2 codes listed in section 5 are appended in B2.
  - **Definition rule:** walls and doors must be `GridEdge`, every other category `GridCell`; edge pieces must keep a 1×1 footprint.
  - **Traces and overlaps use the Pawn channel.** Anything solid to pawns counts; overlap-only volumes (interaction bounds, pickup spheres) never block. A sample hitting a solid non-support actor reports `Blocked`. The failure priority across samples is `NoGround`, `Blocked`, `TooSteep`, `Uneven`.
  - **Edge samples** are taken on both sides of the edge (±8 cm), so a wall on a floor's boundary stands on the floor's top.
  - **Ground exclusion is geometric:** the collision box starts 1 cm above the highest support sample. No actor is ignored except the builder and the caller's `IgnoreActor`.
  - **Reservations purge stale entries first**, so a dead holder never blocks a new reservation.

### B2 — Construction Authority (activate first, pay last) — COMPLETE / APPROVED (`bccd0bc`)
The design gate is approved (5.2, 2026-10-14). The exact scope is in 5.2.12 and the test matrix in 5.2.10.
- **Source:**
  - `Building/ConstructionComponent.h/.cpp`:
    - `AvailableBuildings` (with `IsDataValid`);
    - `GetAvailableBuildings`, `CanBuild` (exact read-only preflight), `TryBuild` (5.2.3);
    - the `OnBuildResult` delegate;
    - the shared guard, the `EndPlay` flag, and the two protected virtual seams `SpawnProvisionalPiece` and `ActivatePiece`.
    - The result struct carries the code, a weak `Piece`, the resolved placement and a diagnostic.
  - `BuildingTypes.h` appends the B2 `EBuildResult` codes (5.2.12).
  - Registration, the `ABuildingPieceActor` class contract and the inventory preflight are checked around the unchanged B1 `ValidatePlacement` (section 4.1). The subsystem stays inventory-free.
  - No G6 extraction (D6). No change to inventory, G6, crafting, storage or interaction source. No B1 API change.
- **Assets:** none.
- **Tests:** 20 `NullTide.Building.B2.*` tests (5.2.10), plus the B1 regression `OverlapOnlyVolumes`. `AuditRegistry` is empty and no keys are reserved after every test.
- **PIE:** none. **Content/WP:** no change. **Cook:** yes (source only).
- **Review:** run a self-review that includes a fresh-context review pass over the native code. Then stop before the commit with the handoff report (`AGENTS.md`).

### B3 — First Buildable: Wooden Floor (no UI) — COMPLETE / APPROVED (`ff04c0e`)

**Goal:** the first production building exists and builds end to end through the B2 authority, on the real player Blueprint. There is no input or UI yet, so the player cannot trigger a build in PIE until B4.

**Baseline:** `bccd0bc` (B2 committed and pushed). Full automation 219/219, cook 622/7/629.

#### Audit (2026-10-15, repo; the MCP checks marked "B3 step" run with the editor open)

| Fact | Source | Consequence |
| --- | --- | --- |
| `Item_Wood` exists and has `ItemFragment_Stackable`, `ItemFragment_Resource` and `ItemFragment_WorldPresentation`. | `Content/LevelPrototyping/InventorySystem/Items/Item_Wood` (asset strings) | It is the cost item for Wood ×4 (approved in B0). |
| `/Game/LevelPrototyping/BuildingSystem/` does not exist yet. | filesystem | B3 creates it. |
| C5 added `CraftingComponent` to `BP_TopDownCharacter` through MCP (`ActorTools.add_component`) and set its list with `ObjectTools.set_properties`. | `CRAFTING_PROGRESS.md` C5 | Same path for `UConstructionComponent`. |
| Existing tests on `BP_TopDownCharacter` count only `UCraftingComponent` (exactly one) and `UToolLoadoutComponent` (exactly one). | `CraftingComponentTests.cpp:295`, `ToolLoadoutTests.cpp:131` | Adding a construction component breaks no existing test. |
| A freshly created data Blueprint fails a warnings-as-errors compile while its data is still invalid. | `CRAFTING_PROGRESS.md` C8A notes | Set `Building_WoodFloor` data on the CDO before its first compile and save. |
| The player's inventory is `InventoryManagerComponent`, a `UInventoryComponent` subclass that imports the legacy array on `BeginPlay`. | `CLAUDE.md`, `CRAFTING_PROGRESS.md` C5 | `UConstructionComponent::GetInventory` resolves it through `FindComponentByClass`, the same as crafting. No code change. |
| `ABuildingPieceActor` is `Abstract, Blueprintable`. It has an unscaled `PieceRoot`, a `PieceMesh` (`BlockAllDynamic`) and the piece component, all native. | `BuildingPieceActor.h/.cpp` | `BP_BuildFloor` only sets the inherited mesh's static mesh, transform and material. It adds no components and no graph logic. |

#### Scope

**Assets (Unreal MCP only; saved by explicit path; never Save All):**
1. **`/Game/LevelPrototyping/BuildingSystem/Pieces/BP_BuildFloor`**:
   - Blueprint child of `ABuildingPieceActor`.
   - The inherited `PieceMesh` gets `/Engine/BasicShapes/Cube`, relative scale (2, 2, 0.1) and relative location (0, 0, 5), so the piece is 200 × 200 × 10 cm with its top at 10 cm (`FloorThickness`). This is the B1/B2 test-piece layout.
   - The root stays unscaled.
   - Material: decision B3-1.
   - No added components, no event graph logic.
2. **`/Game/LevelPrototyping/BuildingSystem/Definitions/Building_WoodFloor`**: data-only Blueprint of `UBuildingDefinition`:
   - `BuildingName` "Wooden Floor"; a short `BuildingDescription`;
   - `Category` Foundation, `PlacementMode` GridCell, `Footprint` 1 × 1, `RotationStep` 90;
   - `PieceActorClass` `BP_BuildFloor`;
   - `Cost` [`Item_Wood` × 4];
   - `RefundFraction` 0.5, `BuildRangeOverride` 0;
   - `BuildingIcon`: decision B3-3.
   - Data is set on the CDO before the first compile.
3. **`BP_TopDownCharacter`**: add one `UConstructionComponent` named `ConstructionComponent` with `AvailableBuildings = [Building_WoodFloor]`. No other change (no graph, input or existing component edits).

**Source:** tests only (`ConstructionTests.cpp` or a new `BuildingProductionTests.cpp`). **No production C++ change** is planned. If MCP cannot set the inherited mesh properties (see risk R-1), STOP and ask the user. Do not add native piece classes or defaults without approval.

**Not in B3:**
- input, building mode, ghost preview, HUD (B4);
- walls, doors (B5);
- demolition (B6);
- map changes; placed pieces; art.

#### Tests `NullTide.Building.B3.*` (planned, 4)

| Test | Assertions |
| --- | --- |
| `ProductionDefinition` | `Building_WoodFloor` loads and `ValidateBuilding` succeeds. Category, mode, footprint and rotation step are as listed. Cost is exactly [`Item_Wood` × 4]. Refund 0.5, default range. `PieceActorClass` is `BP_BuildFloor`. `IsDataValid` passes. |
| `ProductionPieceContract` | `BP_BuildFloor`: <br>• is a child of `ABuildingPieceActor`; <br>• has an unscaled root; <br>• has no `ChildActorComponent`; <br>• has no primitive with custom navigable geometry `EvenIfNotCollidable`; <br>• has no added inventory, crafting or construction component; <br>• `PieceMesh` is the engine cube, blocks Pawn, and spans 200 × 200 × 10 cm with its top at 10 cm (bounds check on a spawned instance). |
| `PlayerRegistration` | `BP_TopDownCharacter` has exactly one `UConstructionComponent` (SCS template) with `AvailableBuildings == [Building_WoodFloor]`. The existing single crafting and tool-loadout components are unchanged. |
| `ProductionFloorEndToEnd` | A transient world (the B2 fixture) and a builder with a native inventory holding `Item_Wood` × 8 and a seam-enabled construction component registered with `Building_WoodFloor`. In order: <br>• **Provisional:** inside the seam, the production piece is hidden, has no actor collision and no navigation-relevant primitive. This proves the Blueprint content contract at runtime. <br>• **Success:** Wood −4, revision +1, one event, one result. The piece is a `BP_BuildFloor` instance, Active, registered at the cell, visible and collidable. <br>• **Support:** a test wall validated on the floor's edge stands at `SurfaceZ` 10, layer 0. <br>• **Occupied:** a second floor on the same cell. <br>• **Insufficient:** after a second success, a third floor is `InsufficientResources`. <br>• **Clean:** `AuditRegistry` stays clean. |

#### Validation

- Build: tests only, editor closed for the final run.
- Blueprint compile with warnings as errors: `BP_BuildFloor`, `Building_WoodFloor`, `BP_TopDownCharacter`.
- Save-time data validation: no errors.
- Focused tests: `NullTide.Building.B3`.
- Regression: `NullTide.Building`, Crafting C5/C8 (character registration), RG3 (tool loadout on the character).
- Full suite: expected 219 + 4 = 223.
- Windows cook with the editor closed: `BP_BuildFloor` and `Building_WoodFloor` must appear as cooked (hard references from the character's `AvailableBuildings` and from `PieceActorClass`).
- Content audit:
  - **Expected:** 2 added assets and 1 modified (`BP_TopDownCharacter`).
  - **Map:** no change; World Partition `__ExternalActors__` 91 and `__ExternalObjects__` 3.
  - **Dirty assets:** the dirty-package list must be checked before saving.
- **B3 step (read-only MCP, for B4):** record the collision object type of the `Lvl_TopDown` ground actors. `ValidatePlacement` only accepts `WorldStatic` or an Active floor as support. B3 changes nothing here; a mismatch is reported for the B4 plan.
- Manual PIE: none (decision B3-2).

#### Decisions — APPROVED by the user (2026-10-15): B3-1, B3-2 and B3-3 as recommended

- **B3-1 Floor material.**
  - **Recommended:** keep the engine cube's default material (a clear placeholder; art comes later).
  - Alternative: reuse a project prototype material (for example `MI_PrototypeGrid_TopDark`) so the floor stands out from the grid ground.
- **B3-2 Runtime proof on the real character.**
  - **Recommended:** prove the wiring with the SCS/CDO registration test, and the build path with a native builder that uses the production definition, piece and item. Spawning the real `BP_TopDownCharacter` (graphs, widgets, legacy import) in a transient test world is left to B4 PIE, where the player first builds through input.
  - Alternative: also spawn `BP_TopDownCharacter` in a test world and build through its own component. This is riskier: its `BeginPlay` graph may need a controller or UI.
- **B3-3 Icon and text.**
  - **Recommended:** name and description now, no icon (the HUD comes in B4).
  - Alternative: reuse an existing texture as a temporary icon.

#### Self-critique (open risks)

- **R-1 MCP and inherited components.** Setting `PieceMesh` properties on the Blueprint's inherited native component template is the riskiest step. If MCP cannot do it, the options are:
  - add the floor mesh as a Blueprint component (this breaks the "no added components" rule and needs `PieceMesh` emptied);
  - a native default in a floor subclass;
  - a manual editor step.

  STOP and ask; do not choose silently.
- **R-2 Dirty assets.** Creating Blueprints or adding a component can dirty other packages (C8B: duplicating dirtied the source). Before saving, list the dirty packages and save only the three B3 assets.
- **R-3 Empty definition.** An empty `Building_WoodFloor` is invalid. Set its data before the first compile, or the warnings-as-errors compile and the save validation fail.
- **R-4 Character Blueprint compile.** Adding a component recompiles `BP_TopDownCharacter`. Its graphs (C7/C8 crafting, input) must still compile with warnings as errors. Only the new component's properties are set.
- **R-5 Cook coverage.** If the cook does not list both new assets, a reference is soft or missing. Check the cook log, not only the package count.
- **R-6 Map support surface (B4 risk).** If the `Lvl_TopDown` ground is not `WorldStatic`, every PIE placement would fail with `Blocked` or `NoGround`. The B3 MCP check reports it early. Fixing it is B4 scope (a decision then), not B3.
- **R-7 Piece contract drift.** The B2 limitations (Blueprint primitives, child actors, scaled root) are enforced by `ProductionPieceContract` and the provisional check in `ProductionFloorEndToEnd`, so a later art pass that breaks them fails a test.

### B4 — Building Mode, Ghost Preview and Placement Input — COMPLETE / APPROVED (`7ce9be3`)

**Goal:** the player builds the Wooden Floor in PIE.
- `B` enters building mode.
- A ghost follows the cursor on the grid, green or red with a reason.
- LMB builds through `UConstructionComponent::TryBuild`.
- RMB or `B` exits.

This is the first Building checkpoint with manual rendered PIE.

**Baseline:** `ff04c0e` (B3 committed and pushed). Full automation 223/223, cook 624/7/631.

#### Audit (2026-10-15)

| Fact | Source | Consequence |
| --- | --- | --- |
| Normal gameplay has the cursor **hidden** and input mode **GameOnly**. Every screen's close path calls `SetShowMouseCursor(false)` and `SetInputMode_GameOnly`. Storage and workbench open with `SetShowMouseCursor(true)` and `SetInputMode_UIOnlyEx`. | `InventoryScreenWidget.cpp:245-251`, `StorageScreenWidget.cpp:88-96`, `StorageContainerActor.cpp:75-76`, `CraftingStationActor.cpp:130-131` | Building mode must show the cursor and restore it. Building mode must not be active when a screen opens, or a screen's close path and the mode's exit path would fight over the cursor. |
| Input: Enhanced Input (`EnhancedPlayerInput`, `EnhancedInputComponent`). Viewport capture `CapturePermanently_IncludingInitialMouseDown`, lock `LockOnCapture`. | `Config/DefaultInput.ini:75-80` | Mapping contexts are added through the local player's `UEnhancedInputLocalPlayerSubsystem`. A higher-priority context whose actions consume input blocks lower-priority mappings of the same key. |
| **The ground `Floor_0` is the only map geometry with a collision override:**<br>• `BlockAllDynamic`, so it is `WorldDynamic`;<br>• it also saves a `Mobility` value;<br>• external actor package `__ExternalActors__/TopDown/Lvl_TopDown/2/33/SKUR0WWL7JT7C3OXZMWAQS`.<br>The 53 prototype blocks (cube, ramp, quarter cylinder, cylinder) use the `StaticMeshActor` default (`BlockAll`, `WorldStatic`). | B3 MCP read-back; package string scan | B1 `ValidatePlacement` treats `Floor_0` as a non-support blocker, so every placement on the main ground is `Blocked`. The other level geometry already supports. See decision B4-1. |
| No click-to-move, `B`, `R`, LMB or RMB game mapping exists. `I`/`Tab` (`IA_OpeningInventory`) and `F` (`IA_Interact`) open screens. | `BUILDING_PLAN.md` 3.1–3.2 (B0 audit), `CLAUDE.md` | No conflict for the building keys; `I`/`Tab`/`F` need decision B4-2. |
| HUD precedent: `UGatherProgressPresenterComponent` creates and owns its widget. UMG assets on native bases were created through MCP in C8C. | `CLAUDE.md`, `CRAFTING_PROGRESS.md` C8C | Same pattern for the building HUD. |

#### Scope (with the recommended decisions)

**Source:**
- **`Building/BuildingPlacementComponent.h/.cpp` (new), on `BP_TopDownCharacter`.** It is the player-side UX only; `UConstructionComponent` stays the authority.
  - **Configuration:** the two mapping contexts and their actions, the preview material, the HUD widget class and the preview refresh interval.
  - **API:** `EnterBuildingMode`, `ExitBuildingMode`, `ToggleBuildingMode`, `ConfirmPlacement`, `IsBuildingModeActive`, `GetSelectedBuilding` (the first registered building, decision B4-3), `GetPreviewResult`, and an `OnBuildingModeChanged` delegate.
  - **Enter:**
    - requires a local player controller and a construction component with at least one building;
    - remembers `bShowMouseCursor` and shows the cursor (input mode stays GameOnly);
    - adds `IMC_BuildingMode`;
    - spawns one preview actor and creates the HUD;
    - enables tick.
  - **Tick (building mode only):**
    - trace under the cursor (Visibility) through a protected virtual seam;
    - snap with `MakePlacementFromWorld`;
    - re-run `CanBuild` when the snapped placement changes or the refresh interval passes;
    - move and tint the ghost; update the HUD.
  - **Confirm:** one `TryBuild` per LMB press with the placement snapped at press time. The mode stays active. The HUD shows the result from `OnBuildResult`.
  - **Exit** (B, RMB, `I`/`Tab`/`F`, `EndPlay`, controller change): destroy the ghost, remove the HUD and `IMC_BuildingMode`, restore the remembered cursor state, disable tick. It is safe to repeat.
- **`Building/BuildingPreviewActor.h/.cpp` (new):**
  - Copies the static meshes (mesh, relative transform) from the piece class: CDO components plus Blueprint SCS templates.
  - Applies one dynamic material instance with a `Color` parameter (green or red).
  - Has no collision, no navigation influence, no piece component and is never registered. It ignores the Visibility trace, so the cursor never hits it.
- **`UI/BuildingHudWidget.h/.cpp` (new), a native base:**
  - bound text widgets for the building name, cost (have/need) and status or reason;
  - fixed hint text "LMB build · RMB / B exit".
- **No change** to `UConstructionComponent`, `UBuildingSubsystem`, inventory, crafting, storage or the screens. (B4-1 alternative (b) would change B1.)

**Assets (Unreal MCP; saved by explicit path):**
- `/Game/LevelPrototyping/BuildingSystem/Input/`:
  - `IA_BuildToggle`, `IA_BuildConfirm`, `IA_BuildCancel` (all digital, consume input);
  - `IMC_Building` (`B` → `IA_BuildToggle`), added at possession;
  - `IMC_BuildingMode`, higher priority, added only in building mode: LMB → `IA_BuildConfirm`; RMB, `I`, `Tab` and `F` → `IA_BuildCancel` (decision B4-2).
- `/Game/LevelPrototyping/BuildingSystem/Materials/M_BuildPreview`: translucent, unlit, a `Color` vector parameter (decision B4-4).
- `/Game/LevelPrototyping/BuildingSystem/UI/W_BuildingHud`: a child of `UBuildingHudWidget`, created and laid out through MCP. It sits at the top center and does not block the cursor (hit-test invisible).
- `BP_TopDownCharacter`: one `BuildingPlacementComponent`, configured with the assets above.
- **`Lvl_TopDown` `Floor_0`** (decision B4-1 (a)): collision profile `BlockAllDynamic` → `BlockAll` (`WorldStatic`), saved with `SceneTools.save_actor`. One external actor package changes; no actor is added or deleted.

**Not in B4:**
- walls, doors, edge placement;
- rotation and the build-bar selection (B5, decision B4-3);
- demolition (B6);
- art; save/load.

#### Tests `NullTide.Building.B4.*` (planned, 7, native fixtures)

| Test | Assertions |
| --- | --- |
| `ModeStateMachine` | Enter and exit are idempotent. Enter is refused without a construction component, without a registered building, or without a local controller. `OnBuildingModeChanged` fires once per real change. Tick is only on in building mode. One preview actor exists in building mode, none outside. |
| `CursorState` | Enter shows the cursor. Every exit path (toggle, cancel, `EndPlay`, controller change) restores the remembered state, both when it was hidden and when it was shown. |
| `PreviewFollowsCursor` | A seam cursor hit produces the snapped transform. Green when `CanBuild` succeeds; red with the reason for `Occupied`, `OutOfRange` and `InsufficientResources`. The preview has no collision, is not navigation-relevant, has no piece component, is never registered, and never changes the registry or inventory (Revision unchanged). |
| `ConfirmBuildsOnce` | One confirm → exactly one `OnBuildResult`, Wood −4, one piece; the mode stays active. A second confirm on the same cell → `Occupied`, no new piece. Confirm outside building mode does nothing. The HUD status shows the result. |
| `ExitPaths` | Cancel, toggle, `EndPlay` and controller change each destroy the ghost, remove the HUD and stop ticking. A build that already finished is unaffected. |
| `HudContent` | The real `W_BuildingHud` binds its widgets. Name "Wooden Floor"; cost "Wood 8 / 4" style have/need; reason text for red states. |
| `ProductionAssets` | `BP_TopDownCharacter` has exactly one placement component, configured. `IMC_Building` and `IMC_BuildingMode` map exactly the keys above, and their actions consume input. `M_BuildPreview` is translucent with a `Color` parameter. `W_BuildingHud` derives from `UBuildingHudWidget`. |

Input contexts reaching the local player subsystem, key routing, and the `Floor_0` change are proven in PIE and by MCP read-back. A transient test world has no real local player or viewport.

#### Manual rendered PIE (user) — required

1. `B` enters building mode: the cursor shows, a ghost follows it on the 200 cm grid, the HUD shows "Wooden Floor" and the cost.
2. Green on free ground near the player. Red with a reason: too far, occupied, not enough Wood.
3. LMB builds a floor: Wood −4 in the inventory; the floor stays; adjacent floors build; the player walks onto a floor.
4. RMB exits; `B` exits; the cursor hides again.
5. In building mode, `I`, `Tab` and `F` only exit building mode. A second press opens the inventory, storage or workbench as before.
6. WASD and jump still work in building mode. There is no cursor or focus lock after exits and after opening and closing screens.
7. Storage, crafting (hand and workbench) and gathering are unchanged.
8. The ground and the prototype blocks both support floors. A floor on a block top sits on the block's height.

#### Validation

- **Build:** clean build with the editor closed.
- **Compile with warnings as errors:** `BP_TopDownCharacter`, `W_BuildingHud`, `M_BuildPreview` (material compile).
- **Data validation** on save.
- **Tests:**
  - focused `NullTide.Building.B4`;
  - regression `NullTide.Building`, Crafting C5/C8, RG3, and Gameplay G6 (world drops trace the ground);
  - full suite, expected 223 + 7 = 230.
- **Cook:** the new assets are cooked.
- **Content and World Partition audit:**
  - the new input, material and UMG assets, plus `BP_TopDownCharacter`;
  - **exactly one external actor package modified (`Floor_0`)**: still 91 `__ExternalActors__` and 3 `__ExternalObjects__`, none added or deleted.
- **Manual PIE:** the checklist above, results recorded exactly as reported by the user.
- **Self-review:** includes a fresh-context review pass (native code changes).

#### Decisions — APPROVED by the user (2026-10-15): B4-1 to B4-5 as recommended

- **B4-1 Ground support surface.**
  - **Recommended: (a)** change `Floor_0`'s collision profile from `BlockAllDynamic` to `BlockAll` (`WorldStatic`), the same as every other map geometry. One external actor package changes; pawn collision is unchanged (both block pawns). Revert if PIE shows a side effect.
  - (b) Widen the B1 support rule (for example static mobility or a support tag). This changes approved B1 code. It may not even cover `Floor_0`, which saves a non-default `Mobility`.
- **B4-2 `I`/`Tab`/`F` in building mode.**
  - **Recommended:** they only exit building mode. `IMC_BuildingMode` consumes them, so no screen opens on that press. Opening needs a second press. This is deterministic: no screen ever opens while the mode owns the cursor, and there is no ordering race with the screens' cursor-hiding close paths.
  - Alternative: exit and open in the same press. This needs a cursor hand-off between the mode's exit and the screen's open, which depends on handler order.
- **B4-3 Rotation and selection.**
  - **Recommended:** defer `R` and the `1`/`2`/`3` build bar to B5, where walls and doors need them. With one registered building and a symmetric 1 × 1 floor, they add assets and tests without visible effect. B4 selects the first registered building.
  - Alternative: include them now as B0 planned.
- **B4-4 Preview material.**
  - **Recommended:** a new translucent unlit `M_BuildPreview` with a `Color` parameter, created through MCP. If MCP cannot author the material graph, STOP and ask.
  - Fallback: tint an existing opaque project material (`M_FlatCol`).
- **B4-5 Preview refresh.**
  - **Recommended:** re-run `CanBuild` when the snapped placement changes and at most every 0.1 s otherwise, so inventory and world changes appear quickly without a full validation every frame.
  - Alternative: every frame.

#### Self-critique (open risks)

- **R-1 MCP authoring.** The riskiest assets to author through MCP are:
  - input mapping contexts (key mappings and consume flags);
  - a material graph (translucent, a parameter);
  - the UMG layout (C8C precedent exists).

  The fallbacks need approval:
  - native runtime-created mapping contexts;
  - an opaque fallback material.

  STOP and ask rather than choosing silently.
- **R-2 Cursor ownership.** The mode must restore only the state it changed. If anything opens a UI-only screen without these keys (none today), the mode could restore "hidden" over an open screen. Mitigations:
  - B4-2 consumes the keys;
  - `ExitPaths` covers the in-mode exits;
  - the PIE step 6 check.
- **R-3 Mapping context lifetime.** `IMC_Building` is added at possession (controller-changed delegate) and removed on unpossess and `EndPlay`. `IMC_BuildingMode` exists only while the mode is active. A leaked context would consume LMB or `I`/`Tab`/`F` forever. PIE steps 4–6 cover it; `ExitPaths` checks the component's own bookkeeping.
- **R-4 Map change.** Changing `Floor_0` dirties one World Partition external actor package.
  - Save only that actor; never Save All.
  - Audit that no other package changed.
  - Run G6 (drops trace the ground) and the gathering regression.
  - If a regression appears, revert the profile.
- **R-5 Preview cost.** `CanBuild` runs traces and an overlap. The throttle (B4-5) bounds it; one ghost only.
- **R-6 Static navigation.** If the level's nav mesh is static, runtime floors do not update it. Movement is WASD, so there is no impact now. Record it for NPC pathing (Phase 6).
- **R-7 Test reach.** Native tests cannot drive real key presses or a real local player. Input routing is proven in PIE. The component exposes native entry points (`ToggleBuildingMode`, `ConfirmPlacement`, the cursor seam), so its logic is fully tested without input.

### B5 — Wooden Wall and Wooden Door — COMPLETE / APPROVED (`b20f70a`)

**Goal:** the player builds walls and doors on cell edges with the same building mode, picks the building with `1`/`2`/`3` and rotates with `R`, and opens or closes a built door with `F`. A small room with a door is buildable in PIE.

**Baseline:** `305e23a` (B4 `7ce9be3` and the test aid). Full automation 230/230, cook 631/7/638.

#### Audit (2026-10-17)

| Fact | Source | Consequence |
| --- | --- | --- |
| Edge math, edge snapping (tie order S, W, N, E), canonical edge keys, edge surface samples (±8 cm across the edge), edge collision boxes (250 cm high, 20 cm thick, ends shortened) and the shared `Edge` occupancy category for walls and doors all exist and are tested. | B1 (`BuildingSubsystem.cpp`, `NullTide.Building.B1.*`) | B5 adds no grid or validation code. |
| `UConstructionComponent` is placement-mode agnostic: it validates through `ValidatePlacement`, reserves the resolved keys and checks the exact resolved transform. | B2 (`ConstructionComponent.cpp`) | Edge pieces need no authority change, only tests. A door is an `ABuildingPieceActor`, so it is constructible. |
| `UBuildingPlacementComponent` always places the first registered building at rotation 0. `MakePlacementFromWorld` already snaps GridEdge requests to the nearest edge and takes a rotation. | B4 | B5 adds selection and rotation. The preview already handles edge placements. |
| The ghost copies each mesh with its relative transform under its root. Nested attachment (a leaf under a hinge) and child-Blueprint overrides of SCS templates are not composed. | B4 review, carried limitation | The door's leaf is nested under a hinge, so the ghost must compose the attach chain. |
| `PlayerInteractionComponent` (Blueprint) tracks actors implementing `BPI_Interactable` that overlap the player's capsule. `F` press calls `GetInteractionType`; Press calls `Interact(Owner)` on the active interactable. | MCP graph read (B2 audit), `CLAUDE.md` | The door needs an overlap-only interaction volume and the `BPI_Interactable` implementation on `BP_BuildDoor`. |
| MCP cannot add a Blueprint interface. Approved in B0: a one-time manual editor step by the user on `BP_BuildDoor` (decision 10.3 "Door interface"). | `BUILDING_PLAN.md` 2.4, 10.3 | One manual step in B5. The forwarding graph is then written through MCP. |
| Interface contract tests exist for `BP_Workbench`: `ImplementsInterface`, `GetInteractionType` and `Interact` through `ProcessEvent`. | `CraftingStationTests.cpp:1158-1239` | The same pattern applies to `BP_BuildDoor`. |
| Navigation uses `RuntimeGeneration=Dynamic`. | `Config/DefaultEngine.ini:50-53` | Active walls and a closed leaf update the nav mesh. A transient test world has no nav mesh, so pathing is PIE-only. |
| Approved costs: Wall Wood × 6; Door Wood × 8 + Scrap × 2. | `BUILDING_PLAN.md` 10.1 | A 2 × 2 room with one door needs about 66 Wood and 2 Scrap. The test pickups hold 10 Wood (decision B5-5). |

#### Scope (with the recommended decisions)

**Source:**
- **`Building/BuildingDoorActor.h/.cpp` (new):** `ABuildingDoorActor : ABuildingPieceActor`.
  - **Components:**
    - a frame (two posts and a lintel, all blocking);
    - a `DoorHinge` scene component with the `DoorLeaf` mesh;
    - an overlap-only `InteractionBounds` box (the workbench profile).
  - **State:** `bIsOpen`, `IsDoorOpen()`, `IsDoorMoving()`.
  - **`ToggleDoor(AActor* Interactor)` / `SetDoorOpen(bool, AActor* Interactor)`.** Both return an `EBuildingDoorResult`:
    - `Success`;
    - `InvalidInteractor`;
    - `OutOfRange` (the interactor is beyond `UseRange`, default 300 cm);
    - `NotActive` (the piece is not `Active`, including `Provisional`).
  - **The leaf swings 90°** over `SwingDuration` (default 0.3 s) on a world timer, with no tick when idle. Toggling while it moves reverses it (B5-3).
  - **Leaf collision** turns off when opening starts and back on only when the leaf is fully closed, so a pawn in the doorway is never pushed (B5-3).
  - The frame always blocks.
- **`Building/BuildingPlacementComponent`:**
  - **Selection:** `SelectBuildingIndex(int32)`, bound to `IA_BuildSelect1/2/3` (B5-2). It picks from `GetAvailableBuildings` in authored order, ignores missing indices, rebuilds the ghost for the new piece class and forces revalidation. The selection persists across enter and exit.
  - **Rotation:** `RotatePlacement()` on `IA_BuildRotate` (`R`) rotates by one quarter turn, wrapping 3 → 0. It is passed to `MakePlacementFromWorld`. For edge pieces it flips the facing (door swing side).
  - **HUD hint:** "1-3 select · R rotate · LMB build · RMB / B exit".
- **`Building/BuildingPreviewActor`:** composes each template's transform up its attach chain (CDO `AttachParent` chain; SCS parent nodes). The leaf ghost therefore sits where the real leaf is.
- **No change** to the subsystem, construction authority, inventory, crafting, storage, `PlayerInteractionComponent` or the screens.

**Assets (Unreal MCP; saved by explicit path; one manual step):**
- **`Pieces/BP_BuildWall`:** child of `ABuildingPieceActor`. `PieceMesh` = engine cube, relative location (0, 0, 125), scale (2, 0.2, 2.5): 200 × 20 × 250 cm along the edge.
- **`Pieces/BP_BuildDoor`:**
  - child of `ABuildingDoorActor`, with engine cube placeholders for the frame and leaf;
  - **manual step (user):** *Class Settings → Implemented Interfaces → Add `BPI_Interactable`*;
  - **then through MCP:** `Interact(Interactor)` → `ToggleDoor(Interactor)`; `GetInteractionType` → Press (`NewEnumerator0`); `GetHoldDuration` → 0.
- **`Definitions/Building_WoodWall`:** "Wooden Wall", Wall, GridEdge, Wood × 6, refund 0.5.
- **`Definitions/Building_WoodDoor`:** "Wooden Door", Door, GridEdge, Wood × 8 + Scrap × 2, refund 0.5.
- **Input:** `IA_BuildRotate` and `IA_BuildSelect1/2/3` (Boolean, consume, no triggers). Added to `IMC_BuildingMode`: `R` → Rotate, `1`/`2`/`3` → Select.
- **`BP_TopDownCharacter`:** `ConstructionComponent.AvailableBuildings = [Building_WoodFloor, Building_WoodWall, Building_WoodDoor]`; the placement component gets the new actions.
- **Test aid (B5-5):** the existing Wood pickup becomes Wood × 100, and one Scrap × 10 pickup is added near `PlayerStart`. This is one more external actor package; saved with Ctrl+S by the user.

**Not in B5:**
- demolition and refunds (B6);
- locking doors;
- door animation art;
- NPC pathing;
- buildable storage or workbench.

#### Tests `NullTide.Building.B5.*` (planned, 6)

| Test | Assertions |
| --- | --- |
| `ProductionDefinitions` | Both definitions are valid with the listed costs, category, mode and piece class. Wall and door piece contracts: <br>• unscaled root, no child actors, no `EvenIfNotCollidable` navigation; <br>• wall mesh 200 × 20 × 250; <br>• door frame and leaf present, with the interaction volume overlap-only. |
| `EdgeBuild` | Through the authority with production pieces: <br>• a wall on an X edge and on a Y edge (exact cost, `Edge` key, yaw 0 / 90, Active); <br>• a wall on a floor's edge stands at 10 cm; <br>• a door on a walled edge and a wall on a door's edge are `Occupied`; <br>• a door builds on a free edge, with Wood −8 and Scrap −2; <br>• rotation 1 flips the facing by 180°; <br>• the registry stays clean. |
| `DoorToggle` | With a zero swing duration, opening clears the leaf collision and closing restores it. Refusals: provisional, out of range, null interactor. With a timer swing, toggling mid-swing reverses, collision returns only when fully closed, and there is no tick when idle. |
| `DoorInterface` | `BP_BuildDoor` implements `BPI_Interactable`. `GetInteractionType` is Press. `Interact` through `ProcessEvent` toggles the door from range and is refused out of range (the C8B pattern). |
| `SelectionAndRotation` | `1`/`2`/`3` select in registered order and ignore missing indices. The ghost mesh set changes with the selection. `R` wraps 0 → 3 → 0. The ghost's door leaf sits at the composed hinge transform. The HUD name follows the selection. Confirm builds the selected building at the selected rotation. |
| `ProductionInput` | `IMC_BuildingMode` maps exactly LMB, RMB, I, Tab, F, R, 1, 2, 3 to the right actions with no per-mapping triggers. `BP_TopDownCharacter` registers Floor, Wall and Door in that order and configures the new actions. |

The existing B4 `ProductionAssets` test is updated for the larger `IMC_BuildingMode`.

#### Manual rendered PIE (user) — required

1. `2` selects Wall: the ghost snaps to the cell edge nearest the cursor, and the HUD shows "Wooden Wall · Wood x/6".
2. Walls build on edges, on the ground and on a floor's edge. Adjacent walls meet at corners.
3. `R` rotates; on a door it flips the swing side.
4. `3` selects Door; a door builds in a wall gap, with Wood −8 and Scrap −2.
5. `F` near the door opens and closes it smoothly. The player walks through the open door; a closed door blocks.
6. A room of floors, walls and a door: the player can always leave through the door. A wall is never placed on the player (`Too close to you`).
7. `1` returns to Floor; the B4 behaviour is unchanged (build, red reasons, exits, cursor).
8. Storage, crafting (hand and workbench) and gathering are unchanged.

#### Validation

- **Build:** clean build with the editor closed.
- **Compile with warnings as errors:** `BP_BuildWall`, `BP_BuildDoor`, `BP_TopDownCharacter`, `W_BuildingHud`.
- **Data validation** on save.
- **Tests:**
  - focused `NullTide.Building.B5`;
  - regression `NullTide.Building`, Crafting C5/C8, RG3, G6;
  - full suite, expected 230 + 6 = 236.
- **Cook:** the new pieces, definitions and actions are cooked.
- **Content and World Partition audit:**
  - **added:** the new assets and one test pickup actor;
  - **modified:** the character, `IMC_BuildingMode` and the Wood pickup actor;
  - no `.umap` change.
- **Manual PIE:** the checklist above.
- **Self-review:** with a fresh-context review pass.

#### Decisions — APPROVED by the user (2026-10-17): B5-1 to B5-5 as recommended

- **B5-1 Checkpoint order (B0 decision 10.2-4, never formally approved).**
  - **Recommended:** keep B5 before B6 (demolition). Runtime pieces are not saved, so a self-enclosure in PIE ends with the session; the door and `BlocksBuilder` cover normal play.
  - Alternatives: swap B5 and B6; or add a debug "demolish all player-built" command.
- **B5-2 Selection input.**
  - **Recommended:** three Boolean actions `IA_BuildSelect1/2/3` on `1`/`2`/`3`. These are simple MCP assets with no modifiers.
  - Alternative: one Axis1D action with per-key scalar modifiers. That means instanced modifiers authored through MCP, which is riskier.
- **B5-3 Door geometry and motion.**
  - **Recommended:**
    - a 100 cm leaf centered in the 200 cm edge, with 50 cm frame posts and a lintel from 220 to 250 cm;
    - the leaf hinges at one post and swings 90° away from the facing side in 0.3 s;
    - leaf collision is off from the start of opening until the leaf is fully closed;
    - toggling mid-swing reverses.
  - Alternative: an instant open/close with no animation.
- **B5-4 Door interaction rules.**
  - **Recommended:** `Active` pieces only; any interactor within `UseRange` (300 cm, as for stations) with a valid pawn; no player-controller requirement, so future NPCs can use doors.
  - Alternative: player controller required (the station rule in 2.4).
- **B5-5 PIE resources.**
  - **Recommended:** set the existing test Wood pickup to 100 and add a Scrap × 10 test pickup (a map change, test aid, saved by the user).
  - Alternative: the player gathers the resources.

#### Implementation deviations — NEEDS USER DECISION (2026-10-17)

- **B5-D1 Edge end samples (subsystem change).** The audit row above says B5 adds no validation code. In practice, edge end samples at 5 cm fell inside a perpendicular wall's 10 cm half thickness, so walls and doors could not meet at a grid corner (`Blocked`, found by `B5.EdgeBuild`). Implemented: `UBuildingSubsystem::EdgeEndInset` = 12 cm (half a wall thickness + clearance) for the edge end samples; the edge collision box uses the same constant (value unchanged). **Recommended:** approve.
- **B5-D2 `Blocked`, not `Occupied`, on an occupied edge.** The `EdgeBuild` row says `Occupied`. The approved B1 order checks the support surface before occupancy, and the existing wall or door is under the new piece's samples, so the result is `Blocked` naming that piece; the key stays with it. **Recommended:** accept; optionally a later HUD tweak.
- **B5-D3 Floors next to existing edge pieces (B1 limitation found in review).** Cell samples (inset 5 cm) and the cell collision box (shrunk 2 cm) overlap an edge piece's 10 cm half thickness, so a floor or furniture cannot be placed in a cell already bounded by a wall or door (`Blocked`). Floors first, then walls, works. **Options:** (a) fix now in B5: inset cell samples and the cell box by `EdgeEndInset`, plus a test; (b) defer to B6 or a polish checkpoint.

**User decisions (2026-10-17, after the first PIE):** D1 approved; D2 accepted; D3 option (a), fix now in B5.

#### B5 rework after the first PIE — PLAN APPROVED (user, 2026-10-17), IMPLEMENTED AND VALIDATED (PIE PASS)

PIE findings (user): walls and doors build without a floor; the door prompt reads "None"; the door's opening side is not visible while building.

**User decisions:**
- **B5-R1:** edge pieces need a floor on at least one side.
- **B5-R2:** the prompt reads its key from `IMC_Default`.
- **B5-R3:** an arrow on the ghost, attached to the piece and rotating with `R` (not following the character); a built door always opens towards the side the arrow showed.
- **D3:** fix now.

**Cause of "None" (verified):**
- `WB_interactionWidget` builds its text once, on construct, from `QueryKeysMappedToAction(IA_Interact)`.
- While building mode is on, `IMC_BuildingMode` (priority 2, consuming) maps F to Cancel. Enhanced Input then drops the lower-priority F → `IA_Interact` mapping from the applied set (`EnhancedInputSubsystemInterface.cpp:1041`), so the query returns no key and `GetKeyDisplayName` of the default key is "None".
- A door built next to the player becomes overlapping at activation, while the mode is still on, so its prompt is built at exactly that moment.

**Design:**
- **R1, floor requirement (data-driven):**
  - New `UBuildingDefinition::bRequiresFloor` (default false), set true on `Building_WoodWall` and `Building_WoodDoor`.
  - `ValidatePlacement` (support stage, after `Uneven` and before occupancy): for an edge piece with `bRequiresFloor`, all three samples on at least one side of the edge must hit an `Active` Foundation piece (the existing `IsSupportSurface` piece branch). Otherwise the new `EBuildResult::NeedsFloor` (appended to the enum) and the HUD text "Needs a floor".
  - A wall on a room's outer boundary is valid (floor on the inside). The test definitions keep the default, so the B1 tests are unchanged.
- **D3, floors next to walls:** cell surface samples are inset by `EdgeEndInset` (12 cm) instead of 5 cm, and the cell collision box is shrunk horizontally by `EdgeEndInset` instead of 2 cm. An edge piece's 10 cm half thickness then never reaches a neighbouring cell's samples or box. Walls standing on a floor's edge still see the floor (their ±8 cm samples are unchanged).
- **R2, prompt key:**
  - New native `UInputPromptLibrary::GetKeyMappedToAction(const UInputMappingContext*, const UInputAction*)`, BlueprintPure: the first key in the context's default mappings for that action, or `EKeys::Invalid`.
  - `WB_interactionWidget` uses it with `IMC_Default` and `IA_Interact` instead of the live query. During building mode the prompt therefore still reads "PRESS F" (accepted with R2; in the mode, F exits).
  - `PlayerInteractionComponent` is unchanged.
- **R3, facing arrow and door side:**
  - `ABuildingPreviewActor` gets a flat arrow (an engine cube shaft plus an engine cone head, about 90 cm, 2 cm above the surface), tinted with the same green/red material, no collision, no navigation, not counted in `GetPreviewMeshCount`.
  - It points along the piece's facing: local +Y for edge pieces (the side a door opens to), local +X for cell pieces. Being part of the ghost's transform, it moves and rotates with the placement.
  - The door already opens towards its local +Y (the hinge yaw is +90°), and an odd `R` turns the piece 180°. So "opens where the arrow showed" holds without changing the door; tests assert it.
- **No change** to the construction authority, inventory, `PlayerInteractionComponent`, or the door's interaction rules.

**Assets (MCP):**
- `Building_WoodWall` and `Building_WoodDoor`: `bRequiresFloor = true`.
- `WB_interactionWidget`: the key node chain replaced, compiled with warnings as errors. Its other nodes are unchanged.

**Tests (B5 count stays 6; assertions added):**
- `EdgeBuild`:
  - walls and doors on bare ground → `NeedsFloor` with nothing consumed;
  - a wall with a floor on one side builds, both on a room's outer edge and between two floors;
  - a floor builds in a cell already bounded by a wall and a door (D3).
- `SelectionAndRotation`: the arrow points along the ghost's facing and flips with `R`; it is not counted as a piece mesh; it has no collision.
- `DoorToggle` / `DoorInterface`: the opened leaf lies on the facing (+Y) side, for rotation 0 and rotation 1.
- New library assertion in `ProductionInput`: `GetKeyMappedToAction(IMC_Default, IA_Interact)` is F.
- `HudContent` reason list: unchanged in B4; `NeedsFloor` → "Needs a floor" is asserted in B5.
- Full suite expected **236**.

**Self-critique:**
- **RW-1:** the cell inset change (D3) touches every floor validation. B1 tests near cell boundaries (support at corners, plateau edges) may shift by 7 cm. Any failing B1 test is examined, and changed only with evidence.
- **RW-2:** `bRequiresFloor` defaults to false, so a new edge definition can forget it. `ValidateBuilding` does not enforce it, because walls on WorldStatic blocks may be wanted later. Recorded.
- **RW-3:** the arrow shows the swing side but not the hinge side. The leaf is centred, so the hinge side is cosmetic.
- **RW-4:** "None" can still appear for any other prompt that queries live mappings. Only `WB_interactionWidget` does today.
- **RW-5:** editing `WB_interactionWidget` touches the shared interaction prompt (storage, workbench, nodes). Compiled and checked by the existing C8B and RG tests, and PIE.
- **RW-6 (rework review P2; user decision 2026-10-17: record, fix with furniture).** With D3, a thin pawn-blocking blocker on a cell border (≤ 24 cm thick, or a pole of radius ≤ 16 cm at a corner) is missed by cell validation, and builder safety starts 7 cm inside the cell.
  - Harmless for floors today.
  - Planned fix with furniture (after B6): 2 cm box and 5 cm samples for cells, ignoring only `Active` edge pieces registered on the cell's own four edges at the same layer.

#### Self-critique (open risks)

#### Self-critique (open risks)

- **R-1 MCP interface graph authoring.** After the manual interface step, the three interface functions must be written through MCP (`write_graph_dsl`). If MCP cannot author interface event or function graphs, STOP and ask. The fallback is the user wiring three nodes by hand.
- **R-2 Edge preview snapping.** The edge choice flips between the four sides near a cell's diagonal. B1's tie order makes this deterministic, but it may feel jumpy in PIE. Accepted for the MVP; PIE step 1 checks it.
- **R-3 Door collision and the trapped-pawn case.** Closing with a pawn in the doorway: the leaf collision returns only when fully closed and is not swept. A pawn that stays in the doorway would overlap the closed leaf; character movement depenetrates it. Alternative: refuse closing while a pawn overlaps the doorway (more code). Recorded for PIE step 5.
- **R-4 Interaction target ambiguity.** `PlayerInteractionComponent` uses the last overlapped interactable. A door next to storage or the workbench may target the wrong actor. Keep the door's bounds tight (`BUILDING_PLAN.md` section 8).
- **R-5 Door piece under construction.** The door's leaf and frame must respect `SetProvisional` (actor-level hidden and collision). The B3 piece contract plus the B2 provisional check, run on the production door, prove it. The interaction volume is overlap-only, so B1 validation ignores it (the `OverlapOnlyVolumes` regression).
- **R-6 Ghost transform composition.** A wrong composition misplaces the leaf ghost. The test asserts the composed hinge transform against a spawned door's real leaf.
- **R-7 Map change for test resources.** One modified and one added external actor package. Saved by the user with Ctrl+S, then audited with `git status`.

### B6 — Demolition and Refund — COMPLETE / APPROVED (`7dd0c1d`)

**Goal:** in building mode, `X` switches to demolish. The piece under the cursor is highlighted with its refund, and LMB removes it and returns half its cost. Only player-built pieces can be demolished, and nothing is ever lost.

**Baseline:** `b20f70a` (B5). Full automation 236/236, cook 639/7/646.

#### Audit (2026-10-17)

| Fact | Source | Consequence |
| --- | --- | --- |
| Approved rules: player-built `Active` pieces only; range 600 cm; refund `floor(Quantity × 0.5)` per line, zero lines skipped; refused when the refund does not fit (`RefundDoesNotFit`); refund first, then destroy; storage with items refused; demolishing a workbench is allowed (C8 `StationLost`). | Section 6, 10.1 | B6 implements section 6 as written. Only the open points below need decisions. |
| `TryBuild` sets `bPlayerBuilt = true` through `InitializePiece`; authored or test pieces default to false. | `ConstructionComponent.cpp:334-335`, `BuildingPieceComponent.h:75` | `bPlayerBuilt` is the demolition permission. |
| `SetPieceState` is public and does not touch the registry. Keys stay occupied until the piece's `EndPlay` or destruction unregisters them. | `BuildingPieceComponent.h:51-52`, 2.3 | `Removing` → refund → `Destroy`; the keys free themselves. |
| One `bBuildInProgress` guard; nested requests return `BuildInProgress` without notifying. | `ConstructionComponent.cpp:194-221` | `TryDemolish` shares the guard (4.4). |
| Refund = an addition-only `TryApplyDefinitionTransaction`; exact preflight `CanApplyDefinitionTransaction`. | 1.1, `InventoryComponent` | No inventory change. |
| `UWorld::DestroyActor` fails only for the world settings or for a networked actor without authority. | engine `LevelActor.cpp:856-880` | After the refund, `Destroy` cannot fail on the server. `CanDemolish` requires authority, so a client can never be refunded without the piece going. |
| A crafting component binds the active station actor's `OnEndPlay` and cancels with `StationLost`. | `CraftingComponent.cpp:384-407` | A future buildable workbench needs no demolition code. |
| No buildable storage or workbench exists yet. The authored `BP_StorageContainer` and `BP_Workbench` have no piece component. | B3–B5 content | The storage rule can only be tested with a native fixture (B6-3). |
| The cursor uses `GetHitResultUnderCursorByChannel(Visibility)`. Pieces block Visibility (`BlockAllDynamic`); the door's `InteractionBounds` only overlaps, so single traces ignore it. | `BuildingPlacementComponent.cpp:484`, `BuildingDoorActor.cpp` | Hovering selects the piece actor itself. |
| B5: walls and doors require a floor along one side (`bRequiresFloor`). | B5 rework | Demolishing a floor can leave a wall that no longer meets its own rule (B6-2). |

#### Scope (with the recommended decisions)

**Source:**
- **`UConstructionComponent`:**
  - `CanDemolish(AActor* Piece)` (exact read-only preflight) and `TryDemolish(AActor* Piece)`.
  - `OnDemolishResult(Definition, Result)`, fired exactly once per non-reentrant request.
  - Static `ComputeRefund(const UBuildingDefinition&)`.
  - **Order in `TryDemolish`:**
    1. guard;
    2. validate: authority, a piece component that is `Active` and `bPlayerBuilt`, a valid definition, range, the storage rule, the floor-support rule (B6-2), an active inventory, the refund preflight;
    3. set `Removing`;
    4. one addition-only refund transaction;
    5. on failure, set `Active` again (nothing changed); on success, `Destroy`.

    A refund that rounds to nothing destroys without a transaction.
  - **`FBuildingDemolishResult`:** `Result`, `Definition`, the `Refund` lines, a weak `Piece`, `Diagnostic`.
  - **New `EBuildResult` codes (appended):** `NotDemolishable` (not a player-built `Active` piece), `RefundDoesNotFit`, `StorageNotEmpty`, `SupportsPieces`. Reused codes: `OutOfRange`, `BuildInProgress`, `InvalidInventory`.
- **`UBuildingSubsystem`:** `FindDependentEdgePieces(const UBuildingPieceComponent& Floor)`, a registry read (B6-2). For each cell key of the floor, it looks at the four edge keys on the same layer. An occupant whose definition has `bRequiresFloor` depends on the floor when the cell on the other side of that edge has no `Active` Foundation.
- **`UBuildingPlacementComponent`, demolish sub-mode (B6-1, B6-4):**
  - `ToggleDemolishMode` is bound to `IA_BuildDemolish` (Started) and works only in building mode.
  - In demolish mode, the tick resolves the piece under the cursor. A new test seam, `GetCursorHitActor`, does the trace.
  - It runs `CanDemolish` on the throttle used for the placement preview. The ghost is rebuilt from the target's class and placed on it, scaled 1.02 to avoid z-fighting, with the arrow hidden. It is orange when demolishable and red when refused, and hidden when there is no piece under the cursor.
  - LMB calls `TryDemolish` once, on the target re-resolved at press time.
  - **HUD:**
    - name "Demolish: Wooden Wall";
    - the cost line becomes "Refund: Wood 3";
    - status "Ready to demolish" or the reason;
    - hint "X build · LMB demolish · RMB / B exit".
  - `X` again returns to build mode with the previous selection. Every exit resets to build mode.
- **Reason texts:**
  - `NotDemolishable` → "Can't demolish this";
  - `RefundDoesNotFit` → "Inventory full";
  - `StorageNotEmpty` → "Empty it first";
  - `SupportsPieces` → "Remove the walls first".
- **No change** to inventory, crafting, storage, `PlayerInteractionComponent` or the screens.

**Assets (MCP):**
- `IA_BuildDemolish` (Boolean, consume, no triggers). `IMC_BuildingMode` adds X → Demolish (10 mappings).
- `BP_TopDownCharacter`: the placement component gets `DemolishAction`.
- No map change.

**Not in B6:**
- buildable storage or workbench;
- cascade demolition;
- a repair or damage system;
- save/load of pieces;
- the D3 thin-blocker fix (with furniture).

#### Tests `NullTide.Building.B6.*` (planned, 7)

| Test | Assertions |
| --- | --- |
| `DemolishRules` | Refused with nothing changed: null; an actor without a piece; a piece with `bPlayerBuilt` false; a provisional piece (inside `BeforeActivate`); out of range. `CanDemolish` equals `TryDemolish` for every case and never mutates. |
| `RefundRounding` | Production refunds: floor Wood 2, wall Wood 3, door Wood 4 + Scrap 1. A fixture definition with quantity 1 refunds nothing and is destroyed without a transaction (no revision, no event). |
| `RefundDoesNotFit` | Full inventory → `RefundDoesNotFit`; the piece stays `Active` and registered; no revision, no event, one result. |
| `ExactlyOnceAndRegistry` | Success: one revision, one inventory event, one result; the piece is gone; its keys are free; the same cell can be rebuilt. A nested `TryDemolish` and a nested `TryBuild` from `OnInventoryChanged` return `BuildInProgress` without notifying. A listener destroying the piece during the refund still gives `Success`. |
| `FloorSupport` | A floor under a wall with no other floor → `SupportsPieces`. With a floor on the other side → allowed. A floor with only a door on another edge also counts. Demolishing the wall first, then the floor → both allowed. |
| `StorageAndStation` | Fixture piece with a `UStorageComponent`: items → `StorageNotEmpty`; empty → allowed. Fixture piece with a `UCraftingStationComponent`: a timed station craft in progress → demolish succeeds and the craft is cancelled with `StationLost`, nothing consumed. |
| `DemolishMode` | `X` toggles only in building mode. The ghost is on the hovered piece: orange if demolishable, red if refused, hidden over nothing. HUD texts. LMB demolishes once and refunds. `X` returns to the previous selection. Exit resets to build mode. |

- `B5.ProductionInput` changes from exactly 9 mappings to 10; B6 asserts X → `IA_BuildDemolish` and the character's `DemolishAction`.
- Full suite expected **243** (236 + 7).

#### Manual rendered PIE (user)

1. In building mode, `X`: HUD "Demolish". Hover a wall: orange highlight, "Refund: Wood 3".
2. LMB on a wall: it disappears and Wood +3. The edge can be built again.
3. Demolish a door, open and closed: Wood +4, Scrap +1.
4. A floor under a wall with no other floor beside it: red, "Remove the walls first". After the wall is gone, the floor can be demolished: Wood +2.
5. Authored storage, the workbench, resource nodes and pickups: no highlight, or red "Can't demolish this".
6. With a full inventory: red "Inventory full", nothing changes.
7. `X` returns to building with the same selection; RMB or B exits; the cursor and other screens are unchanged.
8. Storage, crafting and gathering are unchanged.

#### Decisions — awaiting the user

- **B6-1 Where `X` lives.**
  - **Recommended:** in `IMC_BuildingMode` (active only in building mode), like `R` and 1–3.
  - Alternative: in `IMC_Building` (always active while possessed), so that `X` from normal play enters building mode directly in demolish.
- **B6-2 A floor that a wall or door stands on.**
  - **Recommended:** refuse (`SupportsPieces`, "Remove the walls first") while an edge piece with `bRequiresFloor` has no other floor beside it.
  - Alternatives: (b) allow, leaving the wall standing without a floor (it is not removed); (c) cascade, demolishing the dependent walls with their refunds (more transactions and more failure paths).
- **B6-3 Storage and workbench protections.** No buildable storage or workbench exists yet.
  - **Recommended:** implement the storage-not-empty check now, generically (any piece actor with a `UStorageComponent`), and test it and the existing `StationLost` path with native fixtures.
  - Alternative: defer both until a buildable storage or workbench exists.
- **B6-4 Demolish highlight.**
  - **Recommended:** the existing ghost rebuilt on the target (scaled 1.02), orange or red, plus HUD text.
  - Alternative: HUD text only, without a highlight.

#### User decisions (2026-10-18) and the revised design — APPROVED (user, 2026-10-18; R-11 read as approved: storage is always refused)

**Decisions:**
- **B6-1:** demolish is reached only from building mode (`B`). Inside it, **`C`** (not `X`) switches between build and demolish. `IA_BuildDemolish` maps `C` in `IMC_BuildingMode`; `C` is unused in `IMC_Default`.
- **B6-2: (c) cascade.** Demolishing a floor also demolishes every wall and door that would be left without a floor. **Every piece that will be removed is highlighted.**
- **B6-3:** a piece that is a storage (has a `UStorageComponent`) is **refused, empty or not** (`StorageNotEmpty` becomes `IsStorage`, "Storage can't be demolished"). In a cascade, one storage piece refuses the whole request. A workbench stays allowed (C8 `StationLost`; not changed by the user).
- **B6-4:** a ghost on every affected piece (follows from B6-2).

**Revised design (replaces `SupportsPieces` and the single-piece flow):**
- **Affected set.** `CanDemolish(Target)` and `TryDemolish(Target)` compute `S = {Target} ∪ dependents`.
  - When the target is a floor, its dependents come from `UBuildingSubsystem::FindDependentEdgePieces(Floor)`: edge pieces on the floor's cell edges (same layer) with `bRequiresFloor` whose other side has no `Active` floor once this floor is gone.
  - Depth 1: nothing depends on a wall or door, and floors never depend on walls.
  - A wall or door target gives `S = {Target}`.
- **Validation of `S`:**
  - authority;
  - every piece `Active` and `bPlayerBuilt`;
  - no piece in `S` is a storage;
  - the target within range (600 cm); dependents are adjacent, so not range-checked;
  - an active inventory;
  - the summed refund fits.

  Any failure refuses the whole request, and nothing changes.
- **Refund:** the per-piece refunds (`floor(Quantity × 0.5)` per line) are summed per item. One addition-only transaction gives one revision and one event for the whole cascade. If the total does not fit: `RefundDoesNotFit`, nothing removed.
- **Order:**
  1. guard;
  2. validate `S`;
  3. every piece in `S` → `Removing`;
  4. one refund transaction;
  5. on failure, all back to `Active`; on success, `Destroy` every piece (dependents first, then the target).

  A total refund of nothing destroys without a transaction. `OnDemolishResult` fires once.
- **`FBuildingDemolishResult`:** `Result`, the target's `Definition`, weak `Pieces` (all of `S`, target first), summed `Refund`, `Diagnostic`.
- **Codes (appended):** `NotDemolishable`, `RefundDoesNotFit`, `IsStorage`; reused `OutOfRange`, `BuildInProgress`, `InvalidInventory`.
- **Demolish mode:**
  - `C` toggles it.
  - Hovering resolves the target and `CanDemolish`. One ghost per piece in `S` (a small pool of preview actors, scaled 1.02, arrow hidden): orange when allowed, red on the target alone when refused, hidden over nothing.
  - HUD: "Demolish: Wooden Floor + 2 walls", "Refund: Wood 8", status or reason, hint "C build · LMB demolish · RMB / B exit".
  - LMB runs one `TryDemolish` on the target re-resolved at press time.

**Tests (still 7; expected full suite 243):**
- `FloorSupport` becomes **`Cascade`**:
  - a floor with two walls and a door on edges with no other floor → all four removed, one transaction, refund Wood 2+3+3+4 and Scrap 1;
  - a wall with a floor on its other side stays;
  - a summed refund that does not fit → nothing removed;
  - a storage fixture among the dependents → `IsStorage`, nothing removed;
  - a wall target alone removes only the wall.
- `StorageAndStation`: a storage fixture piece is refused, empty and with items; a station fixture piece → `StationLost`, nothing consumed.
- `DemolishMode`: `C` toggles only in building mode; the ghost count equals the affected set and follows the hover; red on refusal; HUD text.
- `ProductionInput`: C → `IA_BuildDemolish`.

**PIE changes:**
- Step 1 uses `C`.
- Step 4 becomes: "Hover a floor that carries walls: the floor and those walls are all highlighted. LMB removes them all, refunding their sum."

**User decision after the first B6 PIE (2026-10-18): refund 100%.**
- Demolition returns the full cost; this replaces the B0 decision of 50% with floor rounding.
- `UBuildingDefinition::RefundFraction` now defaults to 1.0, and `Building_WoodFloor`, `Building_WoodWall` and `Building_WoodDoor` are set to 1.0. The field and its 0..1 validation stay, so a building can still be given a partial refund later.
- Storage refusal could not be checked in PIE because no buildable storage exists yet; it is covered by the native fixture test.

**Added risks:**
- **R-8 Cascade size:** a floor has at most 4 edges, so at most 4 dependents.
- **R-9 One transaction for several pieces:** the refund preflight and commit use the summed lines, so capacity is all-or-nothing.
- **R-10 Destroying several actors:** each `EndPlay` unregisters its own keys. A listener that destroys one of them during the refund broadcast makes that `Destroy` a no-op.
- **R-11 B6-3 interpretation:** "if it is a chest, reject" is read as **always refuse storage**, not only when it holds items. The user can correct this.

#### Self-critique (open risks)

- **R-1 Refund, then destroy:** the refund is the commit point.
  - `Destroy` cannot fail with authority (engine `LevelActor.cpp:856-880`).
  - A listener that destroys the piece during the refund broadcast makes our `Destroy` a no-op; the result is still `Success`.
  - A listener calling `TryDemolish` or `TryBuild` gets `BuildInProgress`.
- **R-2 `Removing` during the broadcast:** the piece is not `Active`, so it supports nothing for listeners, while its keys stay occupied until `EndPlay` (a rebuild attempted inside the broadcast gives `Occupied`). This is intended.
- **R-3 The floor-support rule uses registry keys and layers, not traces.** A wall standing on a floor of another layer is not counted. Acceptable at the MVP's single-storey scale.
- **R-4 A door demolished mid-swing:** its timer is cleared in `EndPlay` (B5).
- **R-5 Highlight overlay:** a translucent ghost slightly larger than an opaque piece; PIE step 1 checks the look. A hidden arrow and no collision are asserted.
- **R-6 Demolishing the floor under the player:** the player drops 10 cm. No refusal is planned.
- **R-7 Network:** a client is refused before any refund (authority check). Multiplayer is otherwise out of scope.

### B7 — Buildable Chest and Workbench — PLAN APPROVED (user, 2026-10-19; chest and workbench need a floor), IMPLEMENTED AND VALIDATED

**Goal:** the two remaining Phase 5 production examples ("Storage", "Campfire / Workbench", `NULLTIDE_MASTER_PLAN.md` Phase 5) become buildable. The player builds a wooden chest and a workbench in building mode (`4`, `5`), uses them with `F` exactly like the authored ones, and can demolish them under the B6 rules. This checkpoint also fixes the D3 thin-blocker trade-off (decided in B5 for "when furniture lands").

**Baseline:** `7dd0c1d` (B6). Full automation 243/243, cook 640/7/647.

#### Audit (2026-10-19)

| Fact | Source | Consequence |
| --- | --- | --- |
| Construction only spawns piece classes deriving from `ABuildingPieceActor` (it relies on `SetProvisional`). | `ConstructionComponent.cpp:68-77` (B2 decision) | The buildable chest and workbench need native hosts deriving from `ABuildingPieceActor`. |
| `AStorageContainerActor` roots on a **scaled** mesh (1.1, 0.7, 0.7) and holds `UStorageComponent`, `InteractionBounds`, `ContainerTitle`, `StorageScreenClass`; `OpenStorageForInteractor` creates or reuses the screen and sets UI-only input. | `StorageContainerActor.cpp:12-80` | Not usable as a piece (actor scale ≠ 1 fails the B2 transform check). Its open logic is extracted into a shared helper, so both actors use one implementation. |
| `ACraftingStationActor` has an unscaled `StationRoot` and implements `GetOpenCraftingResult` / `OpenCraftingForInteractor` (about 40 lines). | `CraftingStationActor.cpp:18-34`, C8B | Same extraction: a shared helper used by the authored and the buildable workbench. |
| `UCraftingStationComponent` and the crafting component work with any owner actor (`StationLost` through the station actor's `OnEndPlay`). | C8A, B6 `StorageAndStation` test | No crafting change. |
| `Recipe_ShortSword` requires `Station_Workbench`. | `CRAFTING_PROGRESS.md:535-537` | PIE can craft at a built workbench. |
| B6 refuses every storage piece (`IsStorage`), decided while no buildable storage existed. | B6 decision B6-3 | A built chest could never be removed. B7-2 asks to relax it to "only while it holds items". |
| Cells are validated with a 12 cm inset since B5 (D3), so thin border blockers are missed. | B5-D3, RW-6 | B7-5 restores 2 cm / 5 cm and ignores only the Active edge pieces on the cell's own edges. |
| Furniture keys (`Furniture` category) are separate from floor keys; a floor demolished under furniture leaves it floating. | B1, B6 | B7-4 adds furniture standing on a floor to the B6 cascade. |
| MCP cannot add a Blueprint interface. | B5 (door) | Two manual editor steps (`BPI_Interactable` on `BP_BuildChest` and `BP_BuildWorkbench`), as for the door. |
| Items available: Wood, Stone, Scrap and others. | `Content/.../Items` | Costs below use Wood and Stone. |

#### Scope (with the recommended decisions)

**Source:**
- **Shared interaction helpers (no behaviour change for the authored actors):**
  - `Storage/StorageInteraction` (static): `OpenStorageScreen(Interactor, Storage, Title, ScreenClass, CachedScreen)`, the body of `AStorageContainerActor::OpenStorageForInteractor`, which now calls it.
  - `Crafting/CraftingStationInteraction` (static): `GetOpenResult(...)` and `Open(...)`, the bodies of `ACraftingStationActor::GetOpenCraftingResult` / `OpenCraftingForInteractor`, which now call them.
- **`Building/BuildingStorageActor` (new, abstract):** `ABuildingPieceActor` + `UStorageComponent` + overlap-only `InteractionBounds`, with `ContainerTitle`, `StorageScreenClass` and `OpenStorageForInteractor` (through the helper). It refuses to open while the piece is not `Active`.
- **`Building/BuildingCraftingStationActor` (new, abstract):** `ABuildingPieceActor` + `UCraftingStationComponent` + overlap-only `InteractionBounds`, with `CraftingScreenClass`, `GetOpenCraftingResult` and `OpenCraftingForInteractor` (through the helper). It is not usable while not `Active`.
- **Demolition (B6 changes):**
  - B7-2: storage is refused only while `GetItemCount() > 0` (new `StorageNotEmpty`, "Empty it first"); an empty chest is demolished and refunded. `IsStorage` stays in the enum, unused.
  - B7-4: `FindDependentPieces` also returns furniture registered on the floor's cells (same layer rules as walls). The cascade therefore takes an empty chest or a workbench on the floor, and refuses when the chest holds items.
- **D3 fix (B7-5):** cell samples back at 5 cm and the cell collision box back at 2 cm. Hits and overlaps from `Active` edge pieces registered on the cell's own four edges (same layer rules) are ignored. A thin world blocker on the border blocks again.
- **Selection:** `IA_BuildSelect4/5` on `4`/`5` (B7-6). `SelectBuildingActions` gets five entries. HUD hint "1-5 select".

**Assets (MCP; two manual steps):**
- `Pieces/BP_BuildChest` (child of `ABuildingStorageActor`): engine-cube placeholder about 110 × 70 × 70 cm; `ContainerTitle` "Wooden Chest"; `StorageScreenClass` = `W_StorageScreen`. **Manual:** add `BPI_Interactable`; MCP writes `Interact` → `OpenStorageForInteractor`, Press, hold 0.
- `Pieces/BP_BuildWorkbench` (child of `ABuildingCraftingStationActor`): the authored bench's placeholder (160 × 80 × 80 cm); `Station_Workbench`; `CraftingScreenClass` = `W_Inventory`. **Manual:** add `BPI_Interactable`; MCP writes `Interact` → `OpenCraftingForInteractor`, Press, hold 0.
- `Definitions/Building_WoodChest`: "Wooden Chest", Furniture, GridCell 1 × 1, **Wood × 12** (B7-3).
- `Definitions/Building_Workbench`: "Workbench", Station, GridCell 1 × 1, **Wood × 20 + Stone × 10** (B7-3).
- `IA_BuildSelect4/5`, mapped to Four and Five in `IMC_BuildingMode` (12 mappings).
- `BP_TopDownCharacter`: `AvailableBuildings` = Floor, Wall, Door, Chest, Workbench; five select actions.
- No map change. The existing test pickups (Wood × 100, Stone × 10) cover the costs; one Stone pickup may need +10 (B7-3).

**Not in B7:**
- campfire and forge;
- chest or bench art;
- moving placed furniture;
- multi-cell furniture;
- save/load.

#### Tests `NullTide.Building.B7.*` (planned, 7)

| Test | Assertions |
| --- | --- |
| `ProductionDefinitions` | Chest and workbench definitions are valid (category, GridCell, costs, full refund, piece class). Piece contracts: unscaled root, no child actors, storage or station component, overlap-only interaction bounds. `BP_BuildChest` and `BP_BuildWorkbench` implement `BPI_Interactable` (Press). |
| `ChestStorage` | A built chest's storage accepts and returns items through `UStorageTransferLibrary` (same ItemId). `OpenStorageForInteractor` is refused while provisional and without a player controller (the screen itself needs a viewport, as in G-tests). |
| `WorkbenchStation` | A built workbench is a usable `Station_Workbench`; `CanCraftRecipeAtStation(Recipe_ShortSword)` follows range; a timed craft at it is cancelled with `StationLost` when it is demolished. |
| `StorageDemolishRules` | An empty chest is demolished and refunded (Wood 12); a chest with items is refused (`StorageNotEmpty`) with nothing changed. |
| `FurnitureCascade` | A floor carrying an empty chest and a wall → all removed with the summed refund; a chest with items under the cascade refuses the whole request. |
| `ThinBlockerRegression` (D3) | A thin world blocker on a cell border blocks a floor (`Blocked`) again; a wall and a door on the cell's own edges still do not; a floor next to a wall on another cell's edge still builds. The B5 corner and floor-next-to-wall cases stay green. |
| `ProductionInput` | `IMC_BuildingMode` 12 mappings (adds 4, 5); the character registers the five buildings in order and five select actions. `B5.ProductionInput` and `B6` assertions are updated to the new counts. |

Expected full suite **250** (243 + 7).

#### Manual rendered PIE (user)

1. `4` selects the chest: the HUD shows "Wooden Chest · Wood x/12"; it builds on the ground and on a floor.
2. `F` at the built chest opens the storage screen; items move in and out; `I`/`Tab`/`Esc` close it.
3. `5` builds a workbench (Wood −20, Stone −10); `F` opens the crafting screen titled "WORKBENCH"; Short Sword crafts there.
4. Demolish an empty chest: Wood +12. A chest with items: red, "Empty it first".
5. Demolish a floor carrying a chest and a wall: everything highlighted, all removed, full refund. With items in the chest: refused.
6. Demolish the workbench while crafting the Short Sword: the craft is cancelled, nothing lost.
7. The authored storage and workbench behave exactly as before and cannot be demolished.
8. Floors, walls and doors next to each other still build as in B5 (D3 regression).

#### User decisions (2026-10-19) — APPROVED: B7-1 to B7-6 as recommended, plus one change

- **The chest needs a floor under it.** `bRequiresFloor` is extended to GridCell pieces: every surface sample of the cell must hit an `Active` floor, else `NeedsFloor`. `Building_WoodChest` sets it. The workbench keeps the default (ground or floor); the user can change this later.
- The chest therefore always stands on a floor, so B7-4 (furniture in the floor cascade) always applies to it.
- **Tests:** `ChestStorage` also asserts that a chest on bare ground is refused (`NeedsFloor`) and that one on a floor builds.
- **After the B7 PIE (user, 2026-10-19):** the workbench must need a floor too. `Building_Workbench` sets `bRequiresFloor`; tests and the PIE checklist are updated.

#### Decisions (as proposed)

- **B7-1 Scope.** Recommended: chest and workbench together in B7 (one pattern, shared helpers). Alternative: B7 chest only, B8 workbench.
- **B7-2 Demolishing a chest.** Recommended: allowed when empty (full refund); refused while it holds items. Alternative: keep B6 (storage always refused), so a built chest is permanent.
- **B7-3 Costs.** Recommended: chest Wood × 12; workbench Wood × 20 + Stone × 10. Raise the Stone test pickup to 20 if needed.
- **B7-4 Furniture on a demolished floor.** Recommended: part of the cascade (a chest with items refuses the whole request). Alternative: refuse demolishing a floor that carries furniture.
- **B7-5 D3 fix.** Recommended: restore the 2 cm / 5 cm cell inset and ignore only `Active` edge pieces registered on the cell's own edges. This is an identity exclusion, against the B1 "geometric" rule; it was agreed in B5 for this checkpoint.
- **B7-6 Selection keys.** Recommended: `4` and `5` (new actions). Alternative: a build-bar UI (deferred feature).

#### Self-critique (open risks)

- **R-1 Refactoring the authored storage and workbench.** The extraction must not change behaviour. G-tests (storage), C8B/C8C (workbench) and PIE step 7 cover it.
- **R-2 Furniture layer rules.** Furniture on a floor top uses the same layer tolerance as walls (L and the top's layer). Covered by `FurnitureCascade`.
- **R-3 D3 identity exclusion.** It ignores pieces by registry key, not by geometry; a wall on the cell's edge whose mesh is wider than 20 cm would still be ignored. Production walls and doors are 20 cm.
- **R-4 Furniture on a cell bounded by walls.** With the 2 cm box, a chest mesh smaller than the cell never meets the walls; validation ignores the walls on the cell's edges anyway.
- **R-5 Two manual editor steps** (the interfaces). If MCP cannot write the interface graphs, STOP and ask.
- **R-6 Interaction target ambiguity** (B5 R-4): a chest next to a door may target the wrong one; bounds are kept tight.
- **R-7 Station screen while the bench is demolished:** the C8C screen closes itself when its station becomes unusable; PIE step 6 checks it.

### B7.1 — Facing-Based Interaction Targeting — COMPLETE / APPROVED (user, 2026-10-19; I-1 to I-4 as recommended; PIE accepted, reach to be tuned later)

**Implementation notes (2026-10-19; behaviour as approved, mechanics moved native):**
- The probe is a native `UInteractionProbeComponent` (a `USphereComponent`). It queries its candidates fresh (`OverlapMultiByChannel` on the Pawn channel, the channel interaction volumes already answer for the capsule) instead of relying on Blueprint overlap bindings. Cached overlap state missed volumes in a world without play (test finding).
- The 0.1 s refresh timer lives in the probe, which broadcasts `OnFacingInteractableChanged` only on change. `PlayerInteractionComponent` binds that delegate in BeginPlay to a new function `OnFacingInteractableChanged(NewTarget)` that calls `RenderInteractionWidget`. No Blueprint timer.
- `GetActiveInteractable` returns `GetFacingInteractableFor(GetOwner)`. The prompt filter (`ShouldShowInteractionPromptFor`) runs inside the native selector.
- The capsule overlap bindings and `InteractableInRange` stay untouched. They no longer choose the target; an overlap only re-renders the prompt, which is idempotent.
- Self-review: the probe component has `NoCollision` and no overlap events. It only places and sizes the query, so it is even less intrusive than "overlap-only": other volumes no longer get overlap events from it. It also ignores actors attached to the owner, and the tie rule takes two passes (order-independent).
- R-2: no hold lock was added. The hold path already re-resolved the target on every update before B7.1, and the character only turns while moving, so the target is stable while standing and holding. PIE step 4 checks hold gathering.

**PIE finding (user, B7):** "the character's hitbox is too broad, so objects placed near each other block each other's interaction". The user asks to adjust the character's interaction area towards the direction the character is facing.

**Audit (MCP graph read, 2026-10-19):**

| Fact | Source | Consequence |
| --- | --- | --- |
| `PlayerInteractionComponent` (Blueprint) binds the character **capsule**'s begin/end overlap and adds every overlapping `BPI_Interactable` actor to `InteractableInRange`. | `PlayerInteractionComponent` EventGraph | Anything whose interaction bounds touch the capsule is a candidate, including objects behind the character. |
| `GetActiveInteractable` returns the **last** element of `InteractableInRange`, if `ShouldShowInteractionPromptFor` allows it, else none. | `PlayerInteractionComponent:GetActiveInteractable` | The most recently overlapped object always wins, whatever the character faces; a filtered last element hides valid others. |
| `RenderInteractionWidget` runs only on overlap begin/end. | EventGraph | Turning in place never changes the target or the prompt. |
| Interaction bounds are large: storage 130 × 130 × 110, stations 150 × 120 × 110, door 100 × 110 × 110 (half extents). | the actor constructors | Placed pieces one cell apart overlap the capsule together. |
| The character faces its movement (`bOrientRotationToMovement`, yaw rate 640). | `BP_TopDownCharacter` `CharMoveComp` | The actor's forward vector is the facing direction. |

**Design (recommended):**
- **Forward interaction probe:** a sphere on `BP_TopDownCharacter` named `InteractionProbe`, overlap-only, never affecting navigation. It sits about 90 cm in front of the capsule centre with a radius of about 90 cm, and turns with the character.
  - `PlayerInteractionComponent` binds the probe's overlaps instead of the capsule's, falling back to the capsule if no probe exists.
  - Objects behind the character no longer become candidates.
- **Facing selection (native):** `UInteractionTargetingLibrary::SelectFacingInteractable(Pawn, Candidates)`, BlueprintPure.
  - Among valid candidates, it picks the one with the smallest 2D angle between the pawn's forward vector and the direction to the candidate; ties go to the shorter distance.
  - Candidates outside a ±70° cone are ignored. If none is in front, there is no target and no prompt.
  - `GetActiveInteractable` filters the whole list with `ShouldShowInteractionPromptFor`, then calls the selector.
- **Refresh:** a 0.1 s looping timer in `PlayerInteractionComponent` re-evaluates the target and re-renders the prompt **only when the target changes**, so turning in place switches the prompt. There is no per-frame tick.
- **Unchanged:** `BPI_Interactable`, every interactable actor, hold interactions (gathering), the prompt widget, building mode.

**Assets (MCP):**
- `BP_TopDownCharacter` gets the `InteractionProbe` sphere.
- `PlayerInteractionComponent` graphs are edited as follows:
  - the BeginPlay binding moves to the probe;
  - `GetActiveInteractable` filters, then selects;
  - a refresh timer is added.

  Compiled with warnings as errors.

**Tests (`NullTide.Gameplay.G8.*`, planned 3):**
- `FacingSelection`, native:
  - front beats side;
  - behind and outside the cone are ignored;
  - a tie goes to the nearer;
  - invalid and destroyed actors are ignored;
  - empty input gives none.
- `ProbeAsset`: the character has exactly one overlap-only `InteractionProbe` in front of the capsule that does not affect navigation; `PlayerInteractionComponent` compiles.
- `TwoInteractablesInFront`: a chest and a bench one cell apart, with the pawn facing each in turn. The selector returns the one faced, using the real actors' locations.
- Expected suite: 250 + 3 after B7.

**PIE:**
1. A chest and a workbench side by side: facing each shows its own prompt, and `F` opens the one faced.
2. Turning in place switches the prompt without moving.
3. An object behind the character shows no prompt.
4. Doors, pickups, resource nodes (hold to gather), the authored storage and workbench all still work.

**Decisions — APPROVED (user, 2026-10-19): I-1 to I-4 as recommended.**
- **I-1 Approach.**
  - Recommended: forward probe plus facing selection.
  - Alternatives:
    - (b) facing selection only, keeping capsule overlaps, so objects behind can still be chosen when nothing is in front;
    - (c) only shrink each object's interaction bounds.
- **I-2 Probe and cone.** Recommended: radius 90 cm, 90 cm forward, ±70°. Tuned in PIE.
- **I-3 Nothing in front.** Recommended: no prompt. Alternative: fall back to the nearest candidate.
- **I-4 Order.** Recommended: commit B7 first (with the review fixes and the workbench floor rule), then implement B7.1 before B8 (UI).

**Self-critique (open risks):**
- **R-1 Blueprint graph edits through MCP** (`PlayerInteractionComponent` is shared by every interaction). If MCP cannot express the timer or the binding change, STOP and ask; the fallback is a small native interaction component.
- **R-2 Hold interactions:** the active target switching mid-hold could cancel or redirect a gather. Lock the target while a hold is in progress.
- **R-3 Pickups use the same route:** a pickup lying beside the character but outside the cone gets no prompt. PIE step 4 checks it.
- **R-4 Facing while standing still:** the character keeps its last movement direction, so the player turns by moving. Acceptable for top-down WASD.

### B8 — Building UI (build bar) — COMPLETE / APPROVED (user, 2026-10-19: B8-1 to B8-5 as recommended; PIE 5/5 PASS)

**Why:**
- B0 section 3.3 mentions a "HUD build bar", but B4–B7 implemented only the text HUD (`W_BuildingHud`: name, cost, status, hint) and number keys.
- With five buildings (B7), keys alone no longer show what can be built or what it costs.

**Audit (source, 2026-10-19):**

| Fact | Source | Consequence |
| --- | --- | --- |
| The HUD is display-only and sets itself `HitTestInvisible` so it never takes clicks from placement. | `UI/BuildingHudWidget.cpp:10-12` | A clickable bar needs the HUD root `SelfHitTestInvisible` and the bar itself hit-testable. |
| The HUD is created on entering building mode (`AddToPlayerScreen`) and removed on every exit. The placement component pushes texts in `RefreshHud`, which runs on every preview update (every tick in the mode). | `BuildingPlacementComponent.cpp:303, 349-353, 720-752, 782-794` | The bar lives and dies with the HUD; selection can be pushed from `RefreshHud` cheaply if it no-ops when unchanged. |
| Building mode only shows the cursor; it does not change the input mode. The resting mode is GameOnly: screens restore `SetInputMode_GameOnly` on close. | `BuildingPlacementComponent.cpp:281-282`; `InventoryScreenWidget.cpp:251`; `StorageScreenWidget.cpp:96` | — |
| The viewport capture mode is `CapturePermanently_IncludingInitialMouseDown`. | `Config/DefaultInput.ini:75` | In GameOnly mode the viewport keeps the mouse, so UMG buttons do not reliably receive clicks. A clickable bar needs GameAndUI (capture only during mouse down) while building mode is active. |
| Keys `1`–`5` leave demolish first, then call `SelectBuildingIndex`; missing indices are ignored. | `BuildingPlacementComponent.cpp:192-225` | A slot click must take the same path. |
| LMB confirm is an Enhanced Input action (`Started`) that builds or demolishes. | `BuildingPlacementComponent.cpp:162-172` | A click on the bar that reaches the viewport would also build. The confirm handler needs a guard. |
| Definitions already hold `BuildingName`, `BuildingDescription` and `BuildingIcon` (no art assigned). | `Building/BuildingDefinition.h:22-29` | No data-model change. |
| `CanApplyDefinitionTransaction` is the exact read-only inventory preflight; `OnInventoryChanged` fires once per change. | `Inventory/InventoryComponent.h:59, 130` | Affordability = `CanApplyDefinitionTransaction(Cost, {})` success, refreshed on `OnInventoryChanged`. |
| The B4 tests create the real `W_BuildingHud` in a transient world through the `CreateHudWidget` test seam. | `Tests/ConstructionTests.cpp:272-277, 1860-1871` | B8 widget tests use the same fixture. |
| MCP can create widget Blueprints and add, move and wrap widgets (`UMGToolSet`). | MCP toolset listing | The UMG assets can be authored without a manual step. |

**Design (recommended):**
- **Native widgets** (the inventory-slot and crafting-entry pattern):
  - `UBuildingBarWidget` (`UI/BuildingBarWidget.h/.cpp`, abstract):
    - binds a `SlotContainer` panel and has a `SlotClass`;
    - `InitializeBar(UBuildingPlacementComponent*)` builds one slot per registered building in `AvailableBuildings` order, plus a last demolish slot (`C`), once per HUD;
    - `SetSelection(Index, bDemolish)` highlights one slot (no-op when unchanged);
    - it binds the builder inventory's `OnInventoryChanged` and recomputes affordability, and unbinds in `NativeDestruct`;
    - `IsBarHovered()`.
  - `UBuildingBarSlotWidget` (`UI/BuildingBarSlotWidget.h/.cpp`, abstract):
    - a non-focusable `SlotButton`, plus `KeyText` ("1"–"5", "C"), `NameText`, `CostText` (`Wood 4`, `Stone 10`), `IconImage` and an `InitialText` placeholder;
    - a selected highlight and an unaffordable grey-out;
    - the tooltip is `BuildingDescription` when it is not empty;
    - a click forwards to the bar.
  - `UBuildingHudWidget` gets an optional `BuildBar` (`BindWidgetOptional`), and its root becomes `SelfHitTestInvisible`.
- **Placement component:**
  - Enter: after creating the HUD, `InitializeBuildBar`. Then input mode GameAndUI (no widget focus, no mouse lock, cursor kept during capture).
  - Exit (every exit path; one place): `SetInputMode_GameOnly`, the resting mode the screens restore too.
  - `RefreshHud` pushes `SetSelection(SelectedIndex, bDemolishModeActive)`.
  - New public `RequestSelectBuilding(Index)` (leave demolish, then `SelectBuildingIndex`). The `1`–`5` input and slot clicks both use it. The demolish slot calls `ToggleDemolishMode`.
  - Confirm guard: `HandleConfirmInput` does nothing while the cursor is over the bar. The protected virtual seam `IsCursorOverBuildBar()` asks the HUD.
- **Display only:** the bar never builds, demolishes or changes the inventory. It reads definitions and the inventory, and calls only `RequestSelectBuilding` / `ToggleDemolishMode`.
- **Assets (MCP):**
  - new `UI/W_BuildingBar` and `UI/W_BuildingBarSlot`;
  - `W_BuildingHud` gets `BuildBar` at the bottom centre. The existing texts stay where they are.
  - The hint text adds "click".
  - All compiled with warnings as errors.

**Decisions (user):**
- **B8-1 Layout.**
  - Recommended: one flat bar of 5 buildings plus Demolish.
  - Alternative: category tabs (floors and walls, furniture, stations). Deferred: with 5 items, tabs add clicks without benefit.
- **B8-2 Clicking slots.**
  - Recommended: clickable, with GameAndUI input while building mode is active, a guard so a bar click never builds, and non-focusable buttons so keys keep working.
  - Alternative: display-only bar with keys only, no input-mode change (lowest risk).
- **B8-3 Icons.**
  - Recommended: show `BuildingIcon` when set; otherwise a coloured square with the name's initial. No art in B8.
- **B8-4 Tooltip.**
  - Recommended: the description as a tooltip, and fill five one-line descriptions on the definitions by MCP (data only).
  - Alternative: no tooltip.
- **B8-5 Visibility.**
  - Recommended: only in building mode (the HUD's lifetime).
  - Alternative: always visible.

**Failure, reentrancy and lifetime:**
- A slot click runs inside the button's handler.
  - `RequestSelectBuilding` → `RefreshPreviewForSelection` → `RefreshHud` → `SetSelection` only re-styles slots; nothing rebuilds or destroys the bar during the click.
  - No bar action can exit the mode.
- The bar holds the placement component and the inventory weakly.
  - On exit the HUD is removed and `NativeDestruct` unbinds the inventory delegate, so a removed bar never reacts to later inventory changes.
  - Re-entering creates a fresh HUD and bar.
- No building is registered: building mode does not start (existing rule), so there is no empty bar. A missing `SlotClass` or `SlotContainer` gives no bar and no crash; the text HUD still works.
- Affordability is advisory display. `TryBuild` keeps its own exact preflight.

**Tests (`NullTide.Building.B8.*`, planned 5):**
- `ProductionAssets`:
  - `W_BuildingHud` binds a `BuildBar` of class `W_BuildingBar`, whose `SlotClass` is `W_BuildingBarSlot`;
  - the slot button is not focusable;
  - the HUD root does not block hit tests.
- `BarSlots`:
  - the real HUD has 5 + 1 slots in `AvailableBuildings` order;
  - keys "1"–"5" and "C"; names and cost texts; the initial placeholder with no icon; the tooltip text.
- `SelectionSync`:
  - `SelectBuildingIndex` and `ToggleDemolishMode` move the highlight;
  - a slot click selects (leaving demolish) and the demolish slot enters demolish;
  - a click never builds: no `OnBuildResult`, inventory revision unchanged.
- `Affordability`:
  - empty inventory: all greyed;
  - adding Wood updates the slots through `OnInventoryChanged` alone;
  - the result equals `CanApplyDefinitionTransaction`;
  - after exit, inventory changes do not touch the removed bar.
- `ConfirmGuard`: with the cursor over the bar, confirm input does nothing; otherwise it builds once.
- Expected suite: 253 + 5.

**PIE:**
1. `B` shows the bar at the bottom: 5 buildings plus Demolish, the selected one highlighted.
2. Keys `1`–`5` and `C` move the highlight; clicking a slot selects it without building.
3. Costs grey out and update while picking up or spending Wood.
4. LMB in the world still builds and demolishes; `R` rotates; RMB and `B` exit.
5. After exiting, `F`, `I` and `Tab` work as before; the cursor and input behave as before entering.

**Self-critique (open risks):**
- **R-1 Input mode:** GameAndUI is the standard way to make UMG clickable with the cursor shown, but the capture behaviour is only proven in PIE. If clicks still fail, or keys stop reaching Enhanced Input, fall back to B8-2's display-only option. That is a small code change, not an asset-only change, because the bar forces itself `Visible` in `NativeOnInitialized`: no input-mode switch, and the bar `HitTestInvisible`.
- **R-2 Restoring GameOnly on exit** assumes nothing else expects a different mode at that time. Screens cannot be open in building mode because their keys are consumed (B4), and they restore GameOnly themselves.
- **R-3 Focus:** a focusable button would take keyboard focus on click, so `1`–`5`, `R` and `C` would stop working (the C8C Tab lesson). The slot sets its button non-focusable, and `ProductionAssets` asserts it.
- **R-4 Hover under the bar:** the ghost still follows the cursor beneath the bar; the confirm guard prevents a build there. This is acceptable.
- **R-5 MCP UMG authoring:** creating the two widget Blueprints and placing `BuildBar` relies on `UMGToolSet` (`CreateWidgetBlueprint`, `AddWidget`). If an anchor or property cannot be set, STOP and ask (a manual editor step).

**Dependencies:** B1 → B2 → B3 → B4 → B5 → B6 → B7 → B8. B5 needs B4 for PIE; B6 needs B3–B5 content. Each checkpoint gets a build, focused tests, regressions (Crafting, Inventory, Gameplay, ResourceGathering), the full suite, the cook, a content and WP audit, docs, and a commit only after review.

---

## 8. Risks and Edge Cases

- **Cursor state:** screens hide the cursor on close. Building mode must own the cursor while active and restore it; it exits before any screen opens.
- **PIE `Esc`:** stops PIE; the RMB cancel is mandatory.
- **Interaction target ambiguity:** `PlayerInteractionComponent` picks the last overlapped interactable; doors next to storage or the workbench may target the wrong actor. Keep the door bounds tight.
- **Physics versus registry divergence:** the occupancy registry and the physics overlap must agree. Mitigations: only the subsystem mutates the registry, unregister on `EndPlay` and destroy, purge stale entries, and `AuditRegistry` in every test.
- **Trapping and navigation:** walls can enclose the player or cut off resource nodes; the MVP only prevents placing on the builder. Dynamic nav mesh rebuild cost with many pieces is acceptable at MVP scale.
- **Walls before demolition:** B5 makes enclosure possible before B6 provides demolition (decision 10.2-4).
- **World Partition:** runtime pieces are not saved and disappear at session end (expected until Phase 7). Pieces must never be created as editor-placed external actors.
- **Spawn collision handling:** the engine's spawn adjustment is avoided. The piece spawns `AlwaysSpawn` as provisional, and the authority's own overlap test at the exact grid transform decides; a transform drift fails with `TransformMismatch`.
- **Finalization after payment:** resolved by the approved B2 gate (5.2). The piece is activated before payment, so no fallible step follows the deduction.
  - **D2 (accepted):** an unpaid `Active` piece is observable by synchronous engine callbacks for one call. Callback-created blockers and per-primitive navigation are not re-checked.
  - **D3 (a) (accepted):** a paid piece destroyed by a listener still reports `Success`, with no refund.
- **Reentrancy and cross-builder races:** the per-builder guard plus subsystem reservations; nested calls return `BuildInProgress` without notifications.
- **Height layers:** the 50 cm layer quantum and 25 cm unevenness tolerance are tuned for the current map (ground 0, plateau 200). A future map with gentle hills may need different tolerances.
- **Door interface wiring:** needs one manual editor step (decision 10.2-1); until it is done, the door piece builds but cannot be opened. The B5 contract test catches a missing interface.
- **Provisional state completeness:** every collision, overlap or interaction primitive on a piece Blueprint must respect `SetProvisional`. Child Blueprints adding extra colliding components could leak; the B2 and B5 tests assert no collision while provisional.

---

## 9. Validation Strategy

- **Automation** (transient worlds with a world context and initialized actors) for definitions, grid math, validation rules, occupancy, construction atomicity and rollback, reentrancy, demolition and refund.
- **Real widget tests** for the HUD where possible; key routing via the preview and key-down handlers, as in the C8C Tab fix.
- **User-run rendered PIE** for B4–B6, recorded exactly as PASS / FAIL / NOT TESTED / N/A.
- **Every checkpoint:** a clean build, focused tests, the Crafting, Inventory, Gameplay and ResourceGathering regressions, the full suite (baseline 190), BP and UMG compile with warnings as errors, the Windows cook with the editor closed, and a Content and World Partition audit (expected: no map or external actor changes in any B checkpoint).

---

## 10. Decisions

### 10.1 Approved in review round 1

- Data-only `UBuildingDefinition`.
- `UConstructionComponent` as the construction authority.
- `UBuildingSubsystem` as the world grid, placement and occupancy authority.
- Separate `IMC_Building`.
- 200 cm grid; 90° rotation steps.
- No structural support system; no hammer.
- Costs: Floor Wood ×4, Wall Wood ×6, Door Wood ×8 + Scrap ×2.
- 50% refund with floor rounding; refuse demolition when the full refund cannot fit.
- `X` toggles demolition mode; Workbench demolition may trigger `StationLost`.
- Build range 600 cm.

Refinements required by the review and integrated here:
- native door actor (2.4);
- provisional construction (5); the finalization recovery was later superseded by the approved B2 gate (5.2);
- grid coordinate contract (2.2);
- occupancy lifecycle (2.3);
- validation semantics (4.4);
- authority boundaries (5.1);
- B1 limited to data model, grid, occupancy and validation logic.

### 10.2 Still open (final review)

1. **Door interface wiring.** The native `ABuildingDoorActor` holds all logic; `BP_BuildDoor` must declare the Blueprint interface `BPI_Interactable`, which native classes cannot implement and MCP cannot add.
   - **Recommended:** one manual editor step by the user in B5 (Class Settings → Implemented Interfaces → `BPI_Interactable`); everything else through MCP and C++.
   - Alternative: a native C++ interaction interface plus changes to the protected `PlayerInteractionComponent` so it accepts both.
2. **Layer and terrain tolerances:** layer quantum 50 cm, unevenness tolerance 25 cm, maximum slope 30°, floor thickness 10 cm, wall height 250 cm.
3. **Finalization recovery order:** respawn the piece (the player keeps what they paid for), then an exact refund, then dropped pickups. This requires extracting a small generic world-pickup spawn helper from the G6 drop code, with no behaviour change. *(Superseded: the B2 gate (section 5.2) was approved with D1 on 2026-10-14; no recovery chain and no G6 extraction.)*
4. **Checkpoint order for walls versus demolition:** B5 (walls and doors) before B6 (demolition) means a player could fully enclose themselves in B5 PIE without a way to remove walls except the door.
   - **Recommended:** keep the order and require a door in B5 PIE rooms.
   - Alternative: swap B5 and B6 (demolition first, floors only), or add a temporary debug "demolish all player-built" console command in B5.
5. **Spawn collision method:** `AlwaysSpawn` plus our own overlap test at the exact grid transform (recommended), instead of the engine's `DontSpawnIfColliding` or `Adjust` modes.
6. **Provisional visibility:** a provisional piece is fully hidden (recommended), versus visible as a ghost. It exists for one function call only, so it is never visible to the player in practice.

### 10.3 Final B0 Approval (user)

- **Door interface:** `BP_BuildDoor` receives `BPI_Interactable` through a one-time manual editor step in B5. The door is not part of B1.
- **Tolerances:**
  - grid 200 cm, origin (0,0);
  - height quantum 50 cm, maximum unevenness 25 cm, maximum slope 30°;
  - floor thickness 10 cm, wall height 250 cm;
  - 90° rotation steps;
  - GridCell for floors, GridEdge for walls and doors;
  - no free placement in the MVP.
- **Spawn method and provisional visibility:** as recommended (items 5 and 6).
- **Post-payment recovery chain (respawn → exact refund → world pickup): accepted as a direction only.** **SUPERSEDED (2026-10-14):** the B2 design gate (5.2) was approved with D1 (activate first, pay last), so no post-payment recovery exists. The text below is kept as history.
  - **Design gate before B2 implementation:** every recovery branch, including total recovery failure (respawn, refund and pickup spawn all failing), needs a specified contract and tests (section 5.2, OPEN) before the design may claim resource preservation in every engine failure.
  - Until that gate is passed, the plan does not claim an unconditional guarantee.
  - B1 does not modify G6 or extract world-pickup helpers.

---

## 11. Deferred Features

Free (non-grid) placement; building tiers and upgrades; structural support and stability; snapping to arbitrary actors; stairs, roofs and multi-level building; furniture; buildable Storage, Workbench, Campfire and Forge (designed for, not in the MVP); hammer or tool requirement; repair and durability; NPC construction jobs and blueprints (ghost plans built later by colonists); multiplayer and replication; save/load of built structures (Phase 7); nav-based enclosure checks; build menu categories, tabs and icon art beyond the B8 build bar; undo; area or drag placement of multiple floors or walls.
