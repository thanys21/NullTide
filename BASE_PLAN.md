# Phase 6 — Base & NPCs: Plan

**Status:** DESIGN APPROVED (user, 2026-10-21: P-1 to P-10 as recommended; details may be adjusted during implementation). S1 detail below awaits approval.
- **Baseline:** `3de996b` (G9.1). Full automation 267/267, cook 648/7/655.
- **User direction (2026-10-20), replacing the master plan's "Phase 6 — Colony":**
  - **no colonists**: the player controls one character only;
  - Phase 6 creates a **safe area / base** where the player builds and **interacts with NPCs**;
  - the **character faces the mouse cursor**.
- The earlier draft `COLONY_PLAN.md` (worker colonists, job board) is dropped. It was never committed.

## 1. Audit (2026-10-20)

| Fact | Source | Consequence |
| --- | --- | --- |
| The character turns toward its movement: `CharMoveComp` `bOrientRotationToMovement` true, yaw rate 640, `bUseControllerDesiredRotation` false. `BP_TopDownCharacter` `bUseControllerRotationYaw` false. Movement is WASD (`AddMovementInput`). | MCP read of `BP_TopDownCharacter` (CDO, `CharMoveComp`); `BUILDING_PLAN.md:34-36` | Facing the cursor means: stop orienting to movement and drive the control rotation's yaw from the cursor. |
| `BP_TopDownController` shows the cursor by default (`bShowMouseCursor` true, click events on). Closing the inventory or storage screen hides it (`SetShowMouseCursor(false)`). The storage, workbench and building mode paths show it. | MCP read of the controller CDO; `InventoryScreenWidget.cpp:250`; `StorageScreenWidget.cpp:171`; `StorageInteraction.cpp:49`; `CraftingStationInteraction.cpp:97`; `BuildingPlacementComponent.cpp:297-298,388` | After the first screen closes, the gameplay cursor disappears. Cursor facing needs it visible at all times in gameplay. |
| Interaction targets what the character faces: the probe uses the actor's forward vector, within ±70°, reach 150 cm. | `InteractionTargetingLibrary.cpp`; `CLAUDE.md` (B7.1, B9) | With cursor facing, interaction automatically follows the mouse direction. No targeting change is needed. |
| Crafting cancels on more than 10 cm of horizontal movement. Gathering cancels on move/jump input. | `CraftingComponent.cpp:419-426`; `CRAFTING_PLAN.md:213` | Turning in place does not cancel crafting or gathering. |
| No AI code or assets exist. The module lacks `AIModule` / `NavigationSystem`. The level has a dynamic nav mesh (`RuntimeGeneration=Dynamic`, a bounds volume, `RecastNavMesh`). | `NullTide.Build.cs:11-13`; `DefaultEngine.ini:13-53`; level external actors | NPCs need the AI modules. Navigation is ready (pathing is PIE evidence; test worlds have no nav mesh). |
| Building: `ValidatePlacement` checks definition, shape, range, support, floor requirement, occupancy, collision and builder safety. Definitions carry data flags such as `bRequiresFloor`. Building is allowed anywhere today. | `BuildingSubsystem.cpp`; `BuildingDefinition.h`; `CLAUDE.md` | A "must be inside the base" rule fits as one more data-driven check, with its own result code. |
| Inventory transactions: `TryApplyDefinitionTransaction(Removals, Additions)` is the atomic multi-item commit (crafting and building use it). | `InventoryComponent.h` | Trading with an NPC reuses it (pay and receive in one commit). |
| `BPI_Interactable` is a Blueprint interface. MCP cannot add an interface to a new Blueprint. B5 and B7 needed a one-time manual editor step. | `CLAUDE.md` (Crafting, Building) | Each new interactable Blueprint (base flag, NPC) needs that manual step, or duplicating an existing interactable. |
| The master plan's Phase 7 save list includes "Colonists". | `NULLTIDE_MASTER_PLAN.md:1196-1210` | To be updated to "base and NPCs" when this plan is approved. |

**References** (design research, 2026-10-20):
- Core Keeper: the Core as the base anchor; NPCs move in when an enclosed room has a bed, door, light and 4–63 floor tiles.
- Necesse: a settlement flag claims the settlement.
- Valheim: you build only near a workbench.
- Terraria: NPC housing.

## 2. Phase 6 Scope

**Gameplay goal:** the player picks a spot, plants a **Base Flag**, and gets a visible base area: the safe zone. The player builds inside it. Once a base exists, a **Trader NPC** comes to live there. The player can talk to and trade with the Trader. Throughout, the character faces the mouse.

**In scope:**
- cursor facing;
- the base anchor and its area;
- the inside-the-base building rule;
- base visuals and HUD;
- one NPC (a Trader) with idle behaviour inside the base;
- talk and trade;
- optionally, housing.

**Not in scope** (later phases):
- hostile creatures and actual "safety" effects (Phase 8 Combat: no hostile spawns or entry inside the base);
- quests (Phase 9);
- save/load (Phase 7);
- multiple bases;
- base upgrades;
- more NPC roles.

## 3. Recommended Design

### 3.1 Cursor facing (S1)
- **`UCursorFacingComponent`** (native) on `BP_TopDownCharacter`. It is active only while the pawn is controlled by a local player controller in game input; it does nothing during UI-only screens.
  - Each frame it deprojects the mouse into a world ray. It intersects the ray with the **horizontal plane at the character's height**, not the hit under the cursor, so walls, trees or the cursor over tall objects do not skew the aim.
  - It sets the controller's yaw toward that point.
  - It has a **dead zone** (about 40 cm around the character) to avoid jitter when the cursor sits on the character.
- **Character:** `bOrientRotationToMovement` off, `bUseControllerDesiredRotation` on, yaw rate about 1080°/s, so turning is smooth but quick.
- **Cursor always visible in gameplay:** closing the inventory and storage screens restores a visible cursor instead of hiding it. Building mode already shows it.
- **Interaction follows automatically:** the B7.1 probe now targets what the mouse points toward, within 150 cm and ±70°.
- **Pure, tested helper:** `ComputeFacingYaw(PawnLocation, RayOrigin, RayDirection, DeadZone, OutYaw)`.

### 3.2 Base and safe zone (S2, S3)
- **`UBaseSubsystem`** (`UWorldSubsystem`) is the **authority over base areas**.
  - Bases register and unregister there.
  - Queries: `IsInsideBase(Location)`, `FindBaseAt(Location)`, `GetBaseBounds`.
- **`ABaseFlagActor`** (native; Blueprint `BP_BaseFlag`) is a **buildable** piece (a new building definition `Building_BaseFlag`, `GridCell`, with a cost).
  - Its area is a **square of N × N grid cells centred on the flag's cell** (recommended 15 × 15 = 30 m), aligned to the building grid.
  - **One base per world** in Phase 6. A second flag is refused (`BaseLimit`).
  - A flag can only be placed outside any base.
- **Building rule:** a new data flag `UBuildingDefinition::bRequiresBase` (true on the production pieces: floor, wall, door, chest, workbench). `ValidatePlacement` refuses with `OutsideBase` ("Build inside your base"). The flag itself does not require a base.
- **Demolishing the flag** is refused while any other player piece or a resident NPC is inside its area (`BaseNotEmpty`). Otherwise it is demolished and refunded like other pieces.
- **Safety effects:** now, NPCs live only inside the base, and the HUD shows "Base" when the player is inside. Later (Phase 8): no hostile spawns or entry. The authority exposes `IsInsideBase` for that.
- **UX (S3):** the base boundary is drawn on the ground (a thin line or decal) while in building mode and while placing the flag (preview area). A small HUD tag shows when the player is inside the base. The flag's prompt opens a small info panel (base name and size).

### 3.3 NPCs (S4, S5)
- **`UNpcDefinition`** (data-only Blueprint, the items/recipes/stations pattern): name, role, greeting lines, trade offers, arrival rule.
- **`ANpcCharacter`** (native; Blueprint `BP_Npc_Trader`): an `ACharacter` with a placeholder look (the player mesh, tinted) and a name label.
  - Interaction: `BPI_Interactable` (manual interface step) → talk.
- **`ANpcAIController`** (simple, native, timer-driven, no tick):
  - idles and wanders between random reachable points inside its base (`MoveTo` on the nav mesh);
  - stops and faces the player while talking.
  - **No jobs.**
- **Arrival:** when a base exists, the Trader arrives after a short delay at the flag (spawned by the base subsystem, one per base). If the base is removed, the Trader leaves (is removed).
- **Talk panel (S4):** NPC name, greeting line, and buttons [Trade] [Bye]. It closes on Esc, Tab or I, like the other screens.
- **Trading (S5):** the Trader's offers are data (`FTradeOffer`: cost lines → reward lines, for example Wood × 20 → Stone × 5).
  - The trade screen lists offers with affordability, the crafting-panel pattern.
  - A trade is **one atomic `TryApplyDefinitionTransaction`** on the player inventory (cost removed, reward added, or nothing).
  - The NPC has unlimited stock in Phase 6.

### 3.4 Housing (S6, optional)
- A buildable **Bed** and **room detection** on the building grid: an enclosed area of floors bounded by walls and doors.
- The Trader only moves in when the base has a valid room with a bed, and it uses that room as its home. This is the Core Keeper rule; it gives building a purpose.

## 4. Checkpoints

| # | Checkpoint | Content | Tests | PIE |
| --- | --- | --- | --- | --- |
| S1 | Cursor facing | `UCursorFacingComponent`, character rotation settings, cursor always visible after screens close | Facing math (plane, dead zone), component config on the production character, cursor restore on close | Turning with the mouse while walking in any direction; interaction prompt follows the mouse; screens and building mode unaffected |
| S2 | Base authority | `UBaseSubsystem`, `ABaseFlagActor`, `Building_BaseFlag`, `bRequiresBase` + `OutsideBase`, one-base limit, flag demolish rule | Inside/outside, grid-aligned bounds, limit, rule on production definitions, demolish refusal and success; existing building tests updated to provide a base where production pieces are placed | Plant a flag; building inside works, outside refused |
| S3 | Base UX | Boundary visual (flag ghost and building mode), "Base" HUD tag, flag info panel | Boundary actor lifetime and size, HUD tag state | See the area, walk in and out |
| S4 | NPC foundation | AI modules, `UNpcDefinition`, `ANpcCharacter`, `ANpcAIController` (wander in base), arrival and leaving, talk panel | Arrival and removal with the base, definition validation, talk panel through the real widget; wander logic through a movement seam | The Trader arrives, wanders, faces you, talks |
| S5 | Trading | Offers data, trade screen, atomic barter | Affordable or not, atomic success and failure (inventory full), one event | Trade Wood for Stone |
| S6 | Housing (optional) + closeout | Bed, room detection, move-in rule; `BASE_REPORT.md` | Room detection cases (open, closed, door, size) | Build a room with a bed and the Trader moves in |

Each checkpoint gets its own plan detail, approval, validation, review, PIE and commit, as in Phase 5.

## 5. Decisions (user)

- **P-1 Turning feel.** Recommended: smooth and quick (controller desired rotation, about 1080°/s). Alternative: instant snap (`bUseControllerRotationYaw`).
- **P-2 Cursor.** Recommended: always visible in gameplay. Alternative: a custom crosshair cursor (art later).
- **P-3 Walking backwards.** The current animation is speed-based, so walking away from the cursor looks like running backwards ("moonwalk").
  - Recommended: accept it in S1 and add a strafe locomotion set later, when animation assets exist.
  - Alternative: block S1 until strafe animations exist.
- **P-4 Base anchor.**
  - Recommended: a buildable **Base Flag** (the player chooses the spot).
  - Alternatives: a fixed safe zone around `PlayerStart`, or both (a starter zone plus the flag).
- **P-5 Base size.** Recommended: 15 × 15 cells (30 m), grid-aligned. Alternatives: 11 × 11, 21 × 21, or a circular radius.
- **P-6 Building rule.**
  - Recommended: production pieces only inside the base (`bRequiresBase`), as with Valheim's workbench radius; this gives the base meaning.
  - Alternative: build anywhere, with the base only for NPCs and (later) safety.
- **P-7 Number of bases.** Recommended: one in Phase 6. Alternative: several.
- **P-8 First NPC role.** Recommended: a **Trader** (talk and barter). Alternatives: a quest giver (needs Phase 9 systems), or a craftsman (repairs or special recipes).
- **P-9 NPC arrival.**
  - Recommended: arrives as soon as a base exists (S4), with housing as an optional later checkpoint (S6).
  - Alternative: housing required from the start.
- **P-10 Roadmap.**
  - Recommended: rename Phase 6 to **"Base & NPCs"** in the master plan and drop worker colonists from the roadmap.
  - Phase 7 save list: "Colonists" becomes "base, NPCs".

## 6. Invariants

- The player controls one character only. NPCs are never player-controlled and never work for the player.
- `UBaseSubsystem` is the only authority over base areas. Building asks it; it never builds.
- NPCs never change the player's inventory except through a trade, which is one atomic inventory transaction the player starts.
- No per-frame tick except cursor facing, which is inherently per frame and runs on the local player only.
- Existing authorities (inventory, building, crafting, storage, interaction) keep their contracts. New rules are data flags with their own result codes.

## 7. Self-Critique (open risks)

- **R-1 Moonwalk animation** when moving away from the cursor (P-3). It is a visual issue, not functional.
- **R-2 Gamepad** has no cursor. Gamepad aiming (right stick) is out of scope and noted for later.
- **R-3 Interaction now follows the mouse.** That is likely an improvement, but it changes feel. PIE in S1 confirms that F still picks the expected object.
- **R-4 Building tests:** `bRequiresBase` on the production definitions affects the existing production building tests (B3–B8 production pieces). Their fixtures must provide a base, or test definitions keep the flag off. S2 updates them explicitly; no test is deleted.
- **R-5 Blueprint interfaces:** `BP_BaseFlag` and `BP_Npc_Trader` need the manual `BPI_Interactable` step, or duplication from an interactable Blueprint.
- **R-6 NPC pathing** is PIE evidence only (no nav mesh in test worlds), as in earlier plans. The wander logic is tested through a seam.
- **R-7 World Partition and the dynamic nav mesh:** NPC movement depends on runtime nav working in the partitioned level. Confirm in S4 PIE.
- **R-8 Docs:** "Colony" references in the master plan and the reports must be updated consistently once approved.

---

# S1 — Cursor Facing: Detail Plan

**Status:** APPROVED (user, 2026-10-21: "duyệt").

**Audit additions (MCP reads, 2026-10-21):**
- The camera is on `SpringArm` with **absolute rotation** (pitch −50, yaw 45, arm 1400) and `bUsePawnControlRotation` false.
- `IA_Move` builds its directions from the **`Camera` world rotation** (`GetWorldRotation(Camera)` → yaw).

So changing the control rotation moves neither the camera nor the WASD directions. No test depends on the rotation settings.

**Design:**
- **`UCursorFacingComponent`** (new area `Player/`, `Public/Player/CursorFacingComponent.h`), added to `BP_TopDownCharacter`.
  - It ticks only while the owner is a pawn locally controlled by an `APlayerController`. The tick is **disabled otherwise**, so NPCs and test pawns never tick.
- **Each frame:**
  1. `DeprojectMousePositionToWorld`. No mouse position means no change.
  2. Intersect the ray with the **horizontal plane at the character's feet** (capsule bottom). The mouse points at the ground, so the aim matches what the player sees under the cursor even with the angled camera.
  3. If the point is outside the dead zone (40 cm), `SetControlRotation(0, Yaw, 0)`.
- **Pauses** while a UI-only screen has focus (inventory, storage, workbench). The character keeps its facing, and the decision is behind a protected virtual seam. Building mode (GameAndUI) keeps facing.
- **Pure helper:** `static bool ComputeFacingYaw(const FVector& PawnFeet, const FVector& RayOrigin, const FVector& RayDirection, float DeadZone, float& OutYaw)`. It returns false when the ray is parallel to the plane, points away from it, or the point is inside the dead zone.
- **Character (MCP, `BP_TopDownCharacter` `CharMoveComp`):** `bOrientRotationToMovement` false, `bUseControllerDesiredRotation` true, yaw rate 1080 (P-1).
- **Cursor (P-2):** `CloseInventory` and `CloseStorage` restore the cursor to the **controller class default** (`BP_TopDownController`: shown) instead of hiding it.
  - This lives in a small helper next to `RestoreGameOnlyInput` (`UI/GameInputMode`): `RestoreGameplayCursor(PC)`.
  - Building mode keeps restoring its remembered state (shown).

**Tests (`NullTide.Gameplay.G10.*`, planned 3):**
- **`FacingMath`:**
  - the cursor in front, behind and to the side gives the expected yaw;
  - a point inside the dead zone gives false;
  - a ray parallel to or pointing away from the plane gives false;
  - a different pawn height is handled by the feet plane.
- **`ProductionCharacter`:**
  - `BP_TopDownCharacter` has exactly one `UCursorFacingComponent` (dead zone 40);
  - `CharMoveComp`: orient-to-movement off, controller desired rotation on, yaw rate 1080;
  - `BP_TopDownController` default cursor shown.
- **`CursorRestoreAndTickGate`:**
  - `RestoreGameplayCursor` sets a hidden cursor back to the controller-class default (spawned `BP_TopDownController` in a transient world);
  - the component does not tick on an unpossessed pawn or an AI-controlled pawn.

Expected suite: 267 + 3 = 270.

**Self-review amendments (2026-10-22, within the approved design; details in `BASE_PROGRESS.md`):**
- The pause gate is the input mode (`IgnoreInput` on the game viewport, set by UI-only), not viewport focus: a click inside building mode's HUD moves focus but must not stop facing.
- A second dead zone on screen (`DeadZonePixels` 48) keeps the facing while the cursor crosses the character's body; the 40 cm ground dead zone stays.
- The component is a tick prerequisite of the character movement component, so the turn applies in the same frame.
- A protected `IsLocalPlayerController` seam and a fourth test (`TickWhenPlayerControlled`) cover the positive tick path. Suite 271.

**PIE:**
1. Walk in every direction while the character turns to the mouse. Walking away from the cursor looks like running backwards (accepted, P-3).
2. The F prompt follows the mouse direction (chest or workbench to the left or right as the mouse moves).
3. Open and close the inventory and a chest: the cursor stays visible and facing resumes.
4. Building mode: placement, rotation and demolish behave as before; the character faces the cursor.
5. Crafting and gathering are not cancelled by turning in place.

**Risks:**
- Facing changes which object F targets (expected).
- The deprojection uses the player's viewport; split-screen is not supported (none exists).

---

# S2 - Build Zones: Detail Plan

**Status:** COMPLETE (manual PIE 5/5 PASS, 2026-10-22). APPROVED (user, 2026-10-22: Z-1 to Z-6 below; Z-4 confirmed with the diagram: "đúng, và 4 cạnh của ô chấm đều xây tường được").
- **Baseline:** `0a3e243` (S1). Full automation 271/271, cook 648/7/655.
- **User direction (2026-10-22):** the safe area where the player builds a base may later be **several zones**. Adding a zone must be all it takes to allow building there. Zones are created for the game by an admin/designer and are never removed during play.
- **This replaces:**
  - section 3.2 and the S2 row in section 4;
  - **P-4** (a buildable Base Flag as the anchor);
  - **P-6** (a per-definition `bRequiresBase` flag);
  - **P-7** (one base per world).

## S2.1 Audit (2026-10-22)

| Fact | Source | Consequence |
| --- | --- | --- |
| `ValidatePlacement` checks, in this order: definition, shape, builder/world, range, then the surface traces (support, slope, unevenness, floor requirement), occupancy, collision, builder safety. It is read-only. | `BuildingSubsystem.cpp` `ValidatePlacement` | The zone rule is one more read-only step. It goes after range and before the traces: it is cheap and deterministic, and the player gets the most useful message ("outside the zone", not "uneven"). |
| Result codes are append-only, each with a HUD text in `UBuildingPlacementComponent::DescribeResult`. | `BuildingTypes.h` `EBuildResult`; `BuildingPlacementComponent.cpp` `DescribeResult` | Append `OutsideBuildZone` and its text. The ghost turns red through the existing `CanBuild` path. |
| `TryBuild` revalidates after the provisional spawn. | `CLAUDE.md` (B2 steps 1-7) | The zone rule also holds at commit time with no extra code. |
| The grid is world-aligned, 200 cm cells (`WorldToCell`). An edge borders exactly two cells. | `BuildingSubsystem.h` | Zones are rectangles of whole cells, so "inside" is exact. |
| Every building test world comes from two fixtures: `FB1World` (B1, 4 uses) and `FB2World` (B2-B8, 52 uses). Test coordinates stay within ±5000 cm; the one 5000 is a cell-math row and a box below the ground. | `BuildingTests.cpp:114`; `ConstructionTests.cpp:393` | Because every building now needs a zone (Z-3), both fixtures register one large test zone. Assertions stay unchanged. |
| `Lvl_TopDown` is World Partition. Actors stream by distance unless `bIsSpatiallyLoaded` is false. | `CLAUDE.md` (Content) | A streamed-out zone would stop allowing building. Zone actors are always loaded. |

## S2.2 Decisions (user, 2026-10-22)

- **Z-1:** one zone first, for testing. It is placed in the level; the buildable Base Flag is deferred. Several zones are supported by design.
- **Z-2:** a rectangle whose length and width are set separately (`SizeInCells` X and Y).
- **Z-3:** **every** building must be inside a zone. There is no per-definition flag.
- **Z-4:**
  - The **outermost ring of cells** of a zone is not buildable.
  - Walls of the cells next to it can be built, so a wall may stand on the line between the buildable area and the ring.
  - Confirmed: **all four edges of every buildable cell** take walls and doors.
  - **Purpose (user):** however the player builds, the ring stays free, so the outside of a base is always walkable. A perimeter wall reaches only half its thickness (10 cm) into the ring cell.
- **Z-5:** zones are never removed. An admin/designer creates them for the game.
- **Z-6:** the size is adjustable per zone; 15 × 15 for now.

## S2.3 Design

- **`UBaseZoneComponent`** (new area `Base/`).
  - It turns any actor into a zone: `SizeInCells` (FIntPoint, default 15 × 15, minimum 3 × 3), centred on the owner's grid cell.
  - Odd sizes are centred exactly; for an even size the extra row or column goes to +X or +Y.
  - It computes two rectangles:
    - `ZoneCells` (the whole zone, for later safety and NPC queries);
    - `BuildableCells` (`ZoneCells` minus its outer ring, Z-4).
  - It registers on `BeginPlay` and unregisters on `EndPlay`. The rectangle is fixed while registered.
- **`ABaseZoneActor`** (native, placeable, not abstract).
  - It holds an unscaled root, the `UBaseZoneComponent` and a `ZoneBounds` box.
  - `ZoneBounds` has no collision, is hidden in game and sized in `OnConstruction` to the snapped rectangle, so a designer sees the exact area.
  - It also sets `bIsSpatiallyLoaded = false`.
  - **To add a zone:** place one in a level and set its size.
- **`UBaseZoneSubsystem`** (`UWorldSubsystem`) is the only zone registry.
  - `RegisterZone` / `UnregisterZone` are idempotent, and dead entries are purged.
  - Queries:
    - `IsCellInZone(Cell)`;
    - `IsCellBuildable(Cell)`;
    - `FindZoneAt(WorldLocation)`;
    - `GetZones()`;
    - `IsPlacementBuildable(Definition, Placement)`.
  - **Rule:**
    - a cell piece needs every footprint cell buildable;
    - an edge piece needs at least one of its two bordering cells buildable.

    So walls are allowed around the buildable area (on its border with the outer ring), and refused inside the ring or on the zone's outer line.
  - Zones combine by union of their buildable rectangles. Two touching zones therefore leave a 2-cell unbuildable strip between them, which is intended (each keeps its own border).
  - Zones are 2D: every height layer.
  - The subsystem never builds and never touches pieces or inventories.
- **Building rule (Z-3):** `ValidatePlacement` asks `IsPlacementBuildable` right after range for every definition and returns `OutsideBuildZone` (appended), with the HUD text "Outside the build zone". No definition data changes.
- **What zones leave unchanged:** demolition, interaction, crafting and storage. Demolition needs no zone; zones are never removed anyway (Z-5).
- **Level:** one `ABaseZoneActor`, `BaseZone_Start`, 15 × 15 (13 × 13 buildable), around `PlayerStart` in `Lvl_TopDown`.
  - It is placed via MCP and saved by the user with Ctrl+S.
  - Expect one new external actor (`__ExternalActors__` 94 → 95). Its centre is checked against `Floor_0` during implementation.
- **Test fixtures:** `FB1World` and `FB2World` spawn one test zone of 101 × 101 cells centred on the origin and dispatch its `BeginPlay`.
  - That covers cells -50..50, with -49..49 buildable; tests stay within ±25 cells.
  - S2 tests build their own small zones in separate worlds.
- **Later:** S3 draws every zone's border in building mode and shows a HUD tag inside a zone. S4 places NPCs in a zone. Phase 8 safety queries the same subsystem.

## S2.4 Invariants

- `UBaseZoneSubsystem` is the only zone registry. Building asks it during validation; it never builds.
- Zone queries are read-only, exact (whole cells) and independent of registration order.
- `ValidatePlacement` stays read-only. Its order becomes: definition, shape, builder, range, **zone**, support surface, occupancy, collision, builder safety.
- No tick.
- Every building needs a buildable zone. No test or debug switch bypasses it; tests provide a zone.

## S2.5 Tests (`NullTide.Base.S2.*`, planned 5)

- **`ZoneMath`:**
  - zone and buildable rectangles for odd, even and non-square sizes (for example 15 × 15, 4 × 6, 3 × 3 with a single buildable cell), including negative cells;
  - minimum size clamp;
  - the edge rule: an edge between a buildable cell and the ring is allowed; an edge inside the ring or on the outer line is refused.
- **`Registry`:**
  - zone actors register on `BeginPlay` and unregister on `EndPlay`;
  - repeated register and unregister are idempotent, and a stale entry is purged;
  - `FindZoneAt` and the cell queries do not depend on registration order;
  - with two zones, each one's buildable cells are allowed.
- **`PlacementRule`** (test definitions):
  - with no zone: `OutsideBuildZone`;
  - inside the buildable area: `Success`;
  - a ring cell: refused;
  - a 2 × 2 floor half on the ring: refused;
  - perimeter walls of the buildable area: allowed;
  - a wall on the outer line: refused;
  - out of range still wins over the zone, and outside the zone wins over no ground;
  - the subsystem's registry is unchanged.
- **`BuildOutsideZone`:** `TryBuild` on a ring cell spawns nothing, keeps the inventory and `Revision`, and fires `OnBuildResult` once with `OutsideBuildZone`. The same piece one cell inward succeeds.
- **`ProductionRules`:**
  - `ABaseZoneActor` defaults: not spatially loaded, size 15 × 15;
  - `ZoneBounds` has no collision and is hidden in game;
  - `DescribeResult(OutsideBuildZone)` returns "Outside the build zone".

**Existing tests:** `FB1World` and `FB2World` provide the large test zone. No assertion changes and no deletions are expected. Any test that still fails because of the zone is listed in the progress doc with its fix.

Expected suite: 271 + 5 = 276.

## S2.6 PIE

1. Inside the starter zone: floors, walls, door, chest and workbench build as before.
2. On the outermost ring and outside the zone, the ghost is red with "Outside the build zone"; a click spends nothing.
3. Walls along the edge of the buildable area build; walls on the zone's outer line do not.
4. Demolish works as before.
5. In the editor, the zone outline shows the exact area, and changing its size updates it.

## S2.7 Self-Critique (open risks)

- **R-S2-1 Mutual dependency:** `Base/` uses the building grid, and `ValidatePlacement` asks the zone subsystem. They are in the same module and include each other only in `.cpp` files. Accepted.
- **R-S2-2 Fixture zones:** a test world fixture that does not provide a zone fails loudly (`OutsideBuildZone`). It cannot pass silently. Tests outside `FB1World` / `FB2World` that build (none found) would surface the same way.
- **R-S2-3 Zone size changed at runtime** is not tracked (fixed while registered). Zones are static level actors (Z-5).
- **R-S2-4 World Partition:** if a zone were spatially loaded by mistake, building would stop while it is streamed out. `ProductionRules` checks the class default.
- **R-S2-5 Layers:** zones are 2D.
- **R-S2-6 Map edit:** MCP cannot save actor packages. The user saves `Lvl_TopDown` with Ctrl+S, then `git status` is audited.
- **R-S2-8 Future zone sources (self-review):** a zone component registers on `BeginPlay` whatever its owner is. A buildable Base Flag would therefore register on its provisional spawn and make its own placement buildable. That work must register only once the piece is `Active`.
- **R-S2-7 Feel:** building is no longer allowed anywhere. The test pickups near `PlayerStart` and the authored workbench and chest are not building pieces, so they are unaffected. Whether the starter area is big enough is a PIE check.

---

# S3 - Base UX: Detail Plan

**Status:** COMPLETE (manual PIE 5/5 PASS). APPROVED (user, 2026-10-22: U-1 ring fill; U-2 "luôn luôn", always visible; U-3 "hiện ở góc phải màn hình", the tag at the top right).
- **Baseline:** `b890e2e` (S2). Full automation 276/276, cook 648/7/655.
- **Goal:** the player always sees where building is not allowed, and knows when they are inside a base.
- **Revises:** the S3 row in section 4. The flag ghost and the flag info panel are dropped, because there is no buildable flag (Z-1). The building-mode HUD already says "Outside the build zone" (S2).

## S3.1 Audit (2026-10-22)

| Fact | Source | Consequence |
| --- | --- | --- |
| `ABaseZoneActor` already lays out its editor outline in `OnConstruction` from the snapped rectangle (absolute rotation and scale). | `BaseZoneActor.cpp` | U-2 (always visible): the zone actor itself owns the ring visual, laid out the same way, visible in game and in the editor. Building mode needs no change. |
| The ghost is tinted through `M_BuildPreview`'s colour parameter (`ABuildingPreviewActor::ColorParameterName`). | `BuildingPreviewActor.*`; `Materials/M_BuildPreview` | A material instance asset of it (`MI_BaseZoneRing`, red) colours the ring. There is no material graph work and no runtime material instance. |
| The cursor ground point comes from a trace (`GetCursorWorldLocation`), and placement traces use the Pawn channel. | `BuildingPlacementComponent.h:149`; `BuildingSubsystem.cpp` | Ring meshes must have no collision, or they would shift the ghost or support pieces. |
| Gameplay HUD pieces are owned by player components that create their widget on demand (`UGatherProgressPresenterComponent`). The interaction probe uses a timer and broadcasts only on change (B7.1). | `GatherProgressPresenterComponent.*`; `InteractionProbeComponent.*` | The base tag uses a presence component on a timer with a change event. No tick. |

## S3.2 Design

- **Ring visual (always, U-1 / U-2):** `ABaseZoneActor` gets 4 `UStaticMeshComponent` strips (`RingSouth`, `RingNorth`, `RingWest`, `RingEast`).
  - Each is the engine cube, laid in `OnConstruction` over the zone's outer ring:
    - south and north over the full width;
    - west and east between them;
    - 2 cm thick, at the zone actor's height + 1 cm.
  - The strips have no collision, ignore every channel, do not affect navigation, cast no shadow and are visible in game.
  - Absolute rotation and scale, like `ZoneBounds`.
  - Material: `RingMaterial` (`EditAnywhere`, default `MI_BaseZoneRing`).
  - Pure helper `ComputeRingStrips(ZoneCells) → 4 world-space 2D boxes` for tests.
  - The ring is exactly the no-build border, so its inner edge is the boundary of the buildable area.
- **Base tag (U-3):**
  - `UBaseZoneComponent::ZoneName` (FText, default "Base") is set per zone.
  - `UBaseZonePresenceComponent` (`Base/`, on `BP_TopDownCharacter`):
    - runs only for a pawn controlled by a local player controller (the S1 gate, a protected virtual seam);
    - a 0.25 s timer, no tick, looks up `FindZoneAt(owner location)`;
    - on change it broadcasts `OnBaseZoneChanged(NewZone)` once (null when leaving) and updates the tag;
    - the timer runs while possessed by a local player and stops on unpossession and `EndPlay`.
  - `UBaseZoneTagWidget` (abstract UMG base in `UI/`), Blueprint `W_BaseZoneTag`:
    - one `ZoneNameText` (BindWidget), not hit-testable;
    - created on first entry and added to the player screen at the **top right** (anchors and alignment (1, 0), inset 24 px), collapsed outside a zone, removed on `EndPlay`.
  - Test seams: `CreateTagWidget`, `RefreshZone()`.
- **Assets (MCP):**
  - `Content/LevelPrototyping/BaseSystem/Materials/MI_BaseZoneRing` (parent `M_BuildPreview`, red);
  - `Content/LevelPrototyping/BaseSystem/UI/W_BaseZoneTag` (parent `UBaseZoneTagWidget`);
  - `BP_TopDownCharacter`: add `BaseZonePresence` with `TagWidgetClass = W_BaseZoneTag`;
  - `BaseZone_Start` keeps its defaults (name "Base"), so there is no map change.
- **Unchanged:** zone rules and registry, building validation and building mode, input, and every screen.

## S3.3 Invariants

- The ring and the tag only read zone data; neither changes zones, building, inventory or input.
- Ring meshes never collide or affect traces and navigation.
- No tick: the tag uses a timer, and the ring is laid out at construction.
- The tag exists only for the local player's pawn.

## S3.4 Tests (`NullTide.Base.S3.*`, planned 3)

- **`RingGeometry`:**
  - for 5 × 5, 3 × 3 and 4 × 6 zones, the 4 strips cover exactly the ring cells, each once;
  - on a spawned zone actor, even rotated, the strips match those boxes, sit at the zone height and are visible in game;
  - they have no collision, navigation or shadow;
  - a line trace through a strip hits nothing;
  - they use `RingMaterial`.
- **`PresenceAndTag`** (test presence component, `W_BaseZoneTag`):
  - no timer before possession or without a local player;
  - entering a zone broadcasts once and shows its name at the top right;
  - moving within it broadcasts nothing;
  - moving into a touching zone broadcasts that zone;
  - leaving broadcasts null and collapses the tag;
  - unpossession stops the timer;
  - `EndPlay` removes the widget.
- **`ProductionWiring`:**
  - `BP_TopDownCharacter` has exactly one `BaseZonePresence` with `TagWidgetClass` `W_BaseZoneTag`;
  - `W_BaseZoneTag` binds `ZoneNameText`;
  - the default `ZoneName` is "Base";
  - the zone actor's default `RingMaterial` is `MI_BaseZoneRing` (red).

Expected suite: 276 + 3 = 279.

## S3.5 PIE

1. The starter zone shows a red border on its outer ring, at all times (also visible in the editor viewport).
2. In building mode, the ghost is red over the border and green inside; the ring does not move the ghost.
3. Walk into the zone: "Base" appears at the top right; walk out: it disappears.
4. Open inventory or a chest inside the zone: the tag stays, and nothing else changes.

## S3.6 Self-Critique (open risks)

- **R-S3-1 Uneven ground:** the strips lie flat at the zone actor's height. On uneven ground they can sink or float. The starter area is flat; projecting onto terrain is deferred.
- **R-S3-2 Translucency:** the ring and the ghost share the material, so sorting may flicker where they overlap. The ghost is red there anyway.
- **R-S3-3 Always visible:** a red border around every base is always on screen (U-2). Its colour and opacity are in `MI_BaseZoneRing`, so they can be tuned without code.
- **R-S3-4 Existing placed zone:** `BaseZone_Start` is an instance of the native class, so the new strips come from class defaults. No resave is needed; the audit confirms no map change.
- **R-S3-5 Top-right HUD:** the build bar is at the bottom. Whether the interaction prompt or the gather progress overlaps the top right is checked in PIE.
- **R-S3-6 MCP asset creation:** a material instance and a widget Blueprint with a native parent follow the B8 path. If MCP cannot set a parent, one manual editor step may be needed.

