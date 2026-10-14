# Building Plan

Phase 5 of `NULLTIDE_MASTER_PLAN.md`. Building is delivered in small checkpoints (B1-B6). Each checkpoint is validated, documented in `BUILDING_PROGRESS.md` and committed before the next one starts.

**Status:**
- **B0 is APPROVED** (architecture, review refinements and the final decisions in section 10.3).
- **B1 is COMPLETE / APPROVED:** data model, grid coordinates, occupancy and placement validation logic (see `BUILDING_PROGRESS.md`; full automation 198/198, cook PASS, no manual PIE required).
- **B2 design gate is APPROVED (user, 2026-10-14):** decisions D1–D8 in 5.2.13, with D3 option (a). The design activates the piece before payment, so nothing fallible runs after the deduction. It was reviewed independently in two rounds (5.2.14, 5.2.15).
- **B2 implementation is NOT STARTED.** B3–B6 are not started. Phase 5 is IN PROGRESS.

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

Status: **APPROVED (user, 2026-10-14) after independent review rounds 1 and 2.** B2 implementation has not started. B1 does not implement or depend on any of this. No B2 code, G6 change or helper extraction exists.

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

### B2 — Construction Authority (activate first, pay last) — DESIGN APPROVED, NOT STARTED
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

Free (non-grid) placement; building tiers and upgrades; structural support and stability; snapping to arbitrary actors; stairs, roofs and multi-level building; furniture; buildable Storage, Workbench, Campfire and Forge (designed for, not in the MVP); hammer or tool requirement; repair and durability; NPC construction jobs and blueprints (ghost plans built later by colonists); multiplayer and replication; save/load of built structures (Phase 7); nav-based enclosure checks; build menu with categories and icons beyond the MVP build bar; undo; area or drag placement of multiple floors or walls.
