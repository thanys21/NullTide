# Building Plan

Phase 5 of `NULLTIDE_MASTER_PLAN.md`. Building is delivered in small checkpoints (B1-B6). Each checkpoint is validated, documented in `BUILDING_PROGRESS.md` and committed before the next one starts.

**Status:**
- **B0 is APPROVED** (architecture, review refinements and the final decisions in section 10.3).
- **B1 is COMPLETE / APPROVED:** data model, grid coordinates, occupancy and placement validation logic (see `BUILDING_PROGRESS.md`; full automation 198/198, cook PASS, no manual PIE required).
- **B2 is NEXT — DESIGN REVIEW REQUIRED:** the design gate in section 5.2 is OPEN and must be resolved before any B2 implementation. B3–B6 are not started. Phase 5 is IN PROGRESS.

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
| Inventory | `CanApplyDefinitionTransaction(Cost, {})` on the builder's inventory (preflight), then the atomic `TryApplyDefinitionTransaction(Cost, {})` at commit. | `InvalidInventory`, `InsufficientResources`, `ResourcesRejectedRolledBack` |
| Transaction | Reentrancy guard, reservation, provisional spawn, revalidation, cost commit, finalize, rollback (section 5). | `BuildInProgress`, `SpawnFailed`, `TransformMismatch`, … |

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
- **Different builders** (a future NPC reacting to an event during another builder's commit): the first builder holds `Reserved` occupancy keys from validation until finalize or rollback, so the second builder's `ReserveKeys` fails with `Occupied`. Two pieces can never claim one key.
- **Input repeat:** each LMB press triggers at most one `TryBuild` (pressed trigger, not held), and the guard drops nested calls. Duplicate buildings from one click are impossible.

---

## 5. Construction Authority and Atomicity

`UConstructionComponent` (on `BP_TopDownCharacter`, the same pattern as `UCraftingComponent`):

- `AvailableBuildings` (`EditDefaultsOnly`, authored order, duplicates and invalid entries skipped and reported by `IsDataValid`).
- `GetAvailableBuildings`, `CanBuild(Definition, Placement)` (pure, exact preflight), `TryBuild(Definition, Placement)` (callable), `CanDemolish(Piece)`, `TryDemolish(Piece)`.
- `OnBuildResult(Definition, Result)` and `OnDemolishResult(Piece, Result)` fire exactly once per non-reentrant request.
- `FBuildingPlacement` (USTRUCT): grid anchor (`FIntPoint`), edge axis, rotation, resolved world transform (computed by the subsystem, never trusted from UI without re-snapping).

**`TryBuild` sequence (provisional actor state, refined per review):**

| Step | Action | On failure |
| --- | --- | --- |
| 1 | Reentrancy guard `bBuildInProgress`. | Nested call: `BuildInProgress`; no side effects, no notification. |
| 2 | Resolve the placement through the subsystem (re-snap the key, layer and transform from the request; never trust a UI transform). Check registration in `AvailableBuildings` (B2), run the B1 `UBuildingSubsystem::ValidatePlacement`, then the B2 inventory preflight `CanApplyDefinitionTransaction(Cost, {})` (section 4.1). | The specific `EBuildResult`; nothing spawned, nothing reserved. |
| 3 | `Handle = ReserveKeys(this, Keys)` in the subsystem. | `Occupied`; nothing spawned. |
| 4 | `SpawnActorDeferred(PieceActorClass, ResolvedTransform, Owner = builder, AlwaysSpawn)`. Before `FinishSpawning`, set the piece component (definition, keys, facing, `State = Provisional`, `bPlayerBuilt = true`) and call `SetProvisional(true)` (hidden, no collision, no navigation, not interactable). Then `FinishSpawning`. | Invalid actor, or no `UBuildingPieceComponent`: `ReleaseReservation(Handle)`, destroy any partial actor, `SpawnFailed`. |
| 5 | Revalidate at the final state: the actor transform equals the resolved transform, the collision overlap at the exact transform is clear (the provisional actor excluded), and ground and layer are unchanged. | `TransformMismatch`, `Blocked`, `NoGround`: destroy the provisional actor, `ReleaseReservation(Handle)`, inventory untouched. |
| 6 | **Commit the cost (the last fallible step):** `Inventory->TryApplyDefinitionTransaction(Cost, {})`, one atomic removal. | `Busy`, `InsufficientResources` and so on: destroy the provisional actor, `ReleaseReservation(Handle)`, `ResourcesRejectedRolledBack`, inventory untouched. |
| 7 | **Finalize (designed to be infallible):** `CommitReservation(Handle, Piece)` (cannot fail while the handle is valid), `SetProvisional(false)` (collision, visibility, navigation on), `State = Active`. | See "finalization recovery" below. |
| 8 | Broadcast `OnBuildResult(Success)` inside the guard (listeners cannot start a nested build). | — |

**Finalization recovery** (goal: never a successful deduction with no building; accepted as a direction, gated before B2, see section 10.3):
- After step 6 the only possible failure is that the provisional actor was destroyed during the inventory's `OnInventoryChanged` broadcast. Step 6 broadcasts synchronously, and a listener could destroy actors.
- The authority checks `IsValid(Provisional) && State == Provisional && Handle valid` right after step 6. If that check fails, it compensates in this order:
  1. **Respawn** a new piece at the same resolved transform and keys. It reuses the still-valid handle, otherwise reserves again. It goes through steps 4, 5 and 7 without charging again. Result `SuccessRecovered`: the player paid and has the building.
  2. If the respawn is impossible (the keys were taken or the transform is blocked by the new world state), **refund the exact cost** with `TryApplyDefinitionTransaction({}, Cost)`. Result `FinalizeFailedRefunded`.
  3. If the refund cannot fit (only possible if a listener filled the inventory in the same frame), **drop the cost as world pickups** at the builder through the existing world-drop spawn path; this needs a small generic `SpawnWorldPickupForDefinition(Definition, Quantity, Transform)` helper reused from G6. Result `FinalizeFailedDropped`. Resources are never deleted.
- Every recovery branch logs a warning and is covered by forced-failure tests in B2 (a test listener destroys the provisional actor or fills the inventory during the commit).

**Guarantees:**
- Resources are consumed only by the single atomic removal in step 6, and only after a provisional actor exists and has been revalidated at its final transform.
- Goal (not yet a guarantee): no outcome deducts resources without a building, a refund or dropped pickups. Total recovery failure (all three branches failing) still needs a specified contract and tests at the B2 design gate.
- Any failure before step 6 leaves the inventory, the registry and the world unchanged: the provisional actor is destroyed and its reservation released.
- Success means exactly one inventory revision and one `OnInventoryChanged` (step 6). A failure before step 6 means none. Recovery branch 2 adds exactly one more revision and event for the refund.
- At most one piece per successful call. A provisional piece is never visible, collidable, interactable or counted as occupancy (it only holds reservations).
- No item quantity is changed outside `UInventoryComponent`.

**Result codes:** `EBuildResult { Success, SuccessRecovered, InvalidDefinition, NotAvailable, InvalidInventory, BuildInProgress, OutOfRange, NoGround, TooSteep, Uneven, Occupied, Blocked, BlocksBuilder, InsufficientResources, SpawnFailed, TransformMismatch, ResourcesRejectedRolledBack, FinalizeFailedRefunded, FinalizeFailedDropped }`.

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

### 5.2 B2 Design Gate — OPEN (must be resolved and approved before B2 implementation)

The recovery direction after a successful cost commit is **respawn the building → exact inventory refund → world pickups**. It is a direction only. **No unconditional resource-preservation claim is made** until this gate is closed. Before any B2 code, the B2 design must specify, and the user must approve:

1. **Respawn failure:** which conditions count as failure (keys no longer reservable, blocked transform, spawn returns null, the new actor fails revalidation), and whether respawn is attempted once or retried.
2. **Refund failure:** the behaviour when `TryApplyDefinitionTransaction({}, Cost)` returns `Busy`, `InventoryFull`/capacity or `NotInitialized`, including whether a `Busy` refund can be deferred safely.
3. **World-pickup failure:** the behaviour when the world-pickup spawn also fails for some or all cost lines (no safe transform, spawn null), including partial success across lines.
4. **Terminal failure reporting:** the result code (for example a dedicated `RecoveryFailed`), the log severity, and what the player sees.
5. **Recovery state retention and diagnosis:** whether lost resources are recorded (for example on the construction component or a diagnostic log entry with definition, quantities and builder), whether that record survives until a save exists (Phase 7), and how a developer inspects it.
6. **Inventory revision and event counts per branch:** commit only, commit plus respawn, commit plus refund, commit plus partial or no refund plus pickups, and terminal failure. Each needs exact revision and `OnInventoryChanged` counts.
7. **Fault-injection tests per branch:** a test seam (for example a test-only hook or test listeners) that forces respawn failure, refund failure and pickup-spawn failure independently and together, with assertions on result codes, inventory contents, revisions, events, occupancy (`AuditRegistry` clean) and world actors.

Status: **OPEN**. B1 does not implement or depend on any of this. G6 is unchanged, and no world-pickup helper has been extracted.

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

### B2 — Construction Authority (provisional build, atomic cost)
- **Source:**
  - `Building/ConstructionComponent.h/.cpp`: `AvailableBuildings`, `CanBuild`/`TryBuild`, the guard, events, the provisional sequence of section 5, finalization recovery, and `EndPlay` releasing reservations.
  - Registration (`AvailableBuildings`) and the inventory preflight are checked by `UConstructionComponent` around the unchanged B1 `ValidatePlacement` (section 4.1); the subsystem stays inventory-free.
  - A small generic world-pickup spawn helper is extracted from the existing G6 drop code for recovery branch 3. This is an internal refactor with no behaviour change, proven by the G6 tests.
- **Assets:** none.
- **Tests `NullTide.Building.B2.*`:**
  - success consumes the exact cost with one revision and one event; the piece is `Active`, registered, visible and collidable;
  - insufficient resources: nothing spawned, nothing reserved, nothing changed;
  - occupied at reserve time → `Occupied`, nothing spawned;
  - a blocker appears between `CanBuild` and `TryBuild` → `Blocked`, inventory untouched (the preview is not trusted);
  - a forced transform mismatch → `TransformMismatch`, provisional destroyed, reservation released;
  - the inventory becomes `Busy` or short at commit → `ResourcesRejectedRolledBack`, provisional destroyed, reservation released, no inventory change;
  - finalization recovery: a listener destroys the provisional actor during the commit → `SuccessRecovered` (piece respawned, cost charged once); keys also taken → `FinalizeFailedRefunded` (exact refund, one extra revision); inventory also filled → `FinalizeFailedDropped` (pickups spawned, nothing lost);
  - a provisional piece is never visible, collidable, interactable or `Occupied`;
  - a reentrant `TryBuild` from `OnInventoryChanged` or `OnBuildResult` → `BuildInProgress`, one piece only, no extra notification;
  - two builders: the second cannot claim keys reserved by the first;
  - exactly-once notifications;
  - `EndPlay` of the builder mid-sequence leaves no reservation;
  - `AuditRegistry` clean after every test.
- **PIE:** none. **Content/WP:** no change.

### B3 — First Buildable: Wooden Floor (no UI)
- **Assets (MCP):** `/Game/LevelPrototyping/BuildingSystem/Pieces/BP_BuildFloor` (child of `ABuildingPieceActor`, placeholder `/Engine/BasicShapes/Cube` 200×200×10), `/Game/LevelPrototyping/BuildingSystem/Definitions/Building_WoodFloor` (Wood ×4, approved), `BP_TopDownCharacter`: add `UConstructionComponent` with `AvailableBuildings = [Building_WoodFloor]`.
- **Tests:** production asset contract; an end-to-end build of the production floor in a transient world (cost consumed once, piece registered, second floor on the same cell → `Occupied`).
- **PIE:** none required (no input yet). **Cook:** the new assets are cooked through the character reference. **Content/WP:** only the new assets and `BP_TopDownCharacter`; no map change.

### B4 — Building Mode, Ghost Preview and Placement Input
- **Source:** `Building/BuildingPlacementComponent.h/.cpp` (enter and exit, selection, per-frame cursor trace and snap, rotation, confirm and cancel, cursor state save and restore, exit when a UI-only screen opens), `Building/BuildingPreviewActor.h/.cpp`, a small HUD widget base `UI/BuildingHudWidget.h/.cpp` (selected piece, reason text).
- **Assets (MCP):** `IMC_Building` and `IA_BuildToggle`, `IA_BuildConfirm`, `IA_BuildCancel`, `IA_BuildRotate`, `IA_BuildSelect` under `/Game/LevelPrototyping/BuildingSystem/Input/`; `M_BuildPreview` (translucent, a color parameter); `W_BuildingHud`; `BP_TopDownCharacter` adds `UBuildingPlacementComponent`.
- **Tests:** mode state machine; exit on screen open, `EndPlay` and possession change; selection; rotation steps; a confirm calls `TryBuild` exactly once per press; cursor state restored; the preview never registers occupancy or collides.
- **PIE (user):** enter and exit with `B`; ghost follows the cursor on the grid; green and red feedback with reasons; build a floor with LMB; RMB cancels; WASD still works; `F`/`I`/`Tab` exit building mode cleanly; no cursor or focus lock; storage, crafting and gathering unchanged.
- **Content/WP:** new input, material and UMG assets plus the character; no map change.

### B5 — Wooden Wall and Wooden Door
- **Source:**
  - Edge placement end to end. The math and validation already exist from B1; B5 adds the authority path for edge pieces.
  - `Building/BuildingDoorActor.h/.cpp` (section 2.4): open/close state, `ToggleDoor`, leaf interpolation, leaf collision and navigation, refused while provisional.
- **Assets (MCP):**
  - `BP_BuildWall` (200×20×250);
  - `BP_BuildDoor`: child of `ABuildingDoorActor`; frame and leaf meshes; `BPI_Interactable` added by the one-time manual editor step of decision 10.2-1, then the forwarding graph written through MCP;
  - `Building_WoodWall`, `Building_WoodDoor`, registration on the character.
- **Tests:**
  - edge build through the authority;
  - wall and door occupancy, including a door on a walled edge → `Occupied`;
  - `ToggleDoor` toggles leaf collision and state and is refused while provisional;
  - `BP_BuildDoor` implements `BPI_Interactable` with Press, and `Interact` forwards to `ToggleDoor` (production contract test, as for `BP_Workbench`);
  - walls block the nav mesh (path query in a test world if feasible, otherwise PIE).
- **PIE (user):** build a small room; walls align on edges; the door opens and closes with `F`; the player walks through the open door; the closed door blocks; the player is never trapped by a placement on them; storage, workbench and gathering are unchanged.

### B6 — Demolition and Refund
- **Source:** `CanDemolish`/`TryDemolish`, demolish mode in the placement component, refund rules, storage and workbench protections, demolish results and text.
- **Assets:** an `IA_BuildDemolish` (`X`) mapping in `IMC_Building`; HUD text.
- **Tests:** player-built only; refund rounding; the refund does not fit → refused, piece stays; storage with items refused; an active workbench → `StationLost` (approved); exactly-once events; the cell becomes free again; reentrancy.
- **PIE (user):** demolish floor, wall and door; refund received; authored storage, workbench and nodes cannot be demolished; nothing is lost.

**Dependencies:** B1 → B2 → B3 → B4 → B5 → B6. B5 needs B4 for PIE; B6 needs B3–B5 content. Each checkpoint gets a build, focused tests, regressions (Crafting, Inventory, Gameplay, ResourceGathering), the full suite, the cook, a content and WP audit, docs, and a commit only after review.

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
- **Finalization after payment:** a listener destroying the provisional actor during the cost commit is the only post-payment failure; the recovery chain (respawn, then refund, then drop) is the accepted direction, but total recovery failure still needs a contract and tests at the B2 design gate before any "nothing is lost" guarantee is claimed. These branches are rare and must be covered by forced tests.
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
- provisional construction with finalization recovery (5);
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
3. **Finalization recovery order:** respawn the piece (the player keeps what they paid for), then an exact refund, then dropped pickups. This requires extracting a small generic world-pickup spawn helper from the G6 drop code, with no behaviour change.
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
- **Post-payment recovery chain (respawn → exact refund → world pickup): accepted as a direction only.**
  - **Design gate before B2 implementation:** every recovery branch, including total recovery failure (respawn, refund and pickup spawn all failing), needs a specified contract and tests (section 5.2, OPEN) before the design may claim resource preservation in every engine failure.
  - Until that gate is passed, the plan does not claim an unconditional guarantee.
  - B1 does not modify G6 or extract world-pickup helpers.

---

## 11. Deferred Features

Free (non-grid) placement; building tiers and upgrades; structural support and stability; snapping to arbitrary actors; stairs, roofs and multi-level building; furniture; buildable Storage, Workbench, Campfire and Forge (designed for, not in the MVP); hammer or tool requirement; repair and durability; NPC construction jobs and blueprints (ghost plans built later by colonists); multiplayer and replication; save/load of built structures (Phase 7); nav-based enclosure checks; build menu with categories and icons beyond the MVP build bar; undo; area or drag placement of multiple floors or walls.
