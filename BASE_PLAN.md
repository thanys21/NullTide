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
| S6 | House NPCs + closeout — **ON HOLD (user, 2026-10-24)** | Summon item, room detection, requirements (bed, table, chair, room size), move-in rule; `BASE_REPORT.md` | Room detection cases (open, closed, door, size), requirements | Place the item in a valid room and the NPC moves in |

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

---

# S4 - NPC Foundation: Detail Plan

**Status:** COMPLETE (manual PIE PASS). APPROVED (user, 2026-10-23: N-1 to N-5 below; N-4 and N-5 "tùy ý", so the recommendations apply).
- **Baseline:** `b2d5413` (S3). Full automation 279/279, cook 650/7/657.
- **Goal:**
  - a Trader NPC lives in its own camp and strolls inside it;
  - the player presses F to talk;
  - the talk panel shows its name, a greeting and Bye. Trading is S5.

## S4.1 User direction (2026-10-23)

- **N-1 Two kinds of NPC:**
  - **House NPCs** are summoned with an item and move in when the player's house meets the requirements (a bed, a table, a chair, a room size). This belongs to S6 (housing), whose scope is revised below.
  - **Outdoor NPCs** have their own base and need no house. The Trader is one.
- **N-2:** the Trader walks inside its own area ("khu vực của trader").
- **N-3:** the talk panel has Bye only in S4. Trade comes with S5.
- **N-4 / N-5 ("tùy ý"):** the Quinn mannequin with a floating name label; one checkpoint.

## S4.2 Audit (2026-10-23)

| Fact | Source | Consequence |
| --- | --- | --- |
| The module depends on Core, CoreUObject, Engine, InputCore, UMG, Slate, SlateCore and EnhancedInput only. | `NullTide.Build.cs:11-13` | Add `AIModule` and `GameplayTasks` (public: `ANpcAIController.h` includes `AIController.h`) and `NavigationSystem` (private). |
| The level uses a dynamic nav mesh. | `DefaultEngine.ini:13-53` | Built walls update paths. Pathing is PIE evidence; test worlds have no nav mesh, so movement goes through seams. |
| Interaction runs through the Blueprint interface `BPI_Interactable` (`Interact`, `GetInteractionType` = Press, `GetHoldDuration` = 0). Adding it to a new Blueprint is a **manual editor step**; MCP then wires `EventInteract` → a native call. | `BUILDING_PROGRESS.md:565,766-772` | `BP_Npc_Trader` needs that one manual step. |
| The facing probe finds `BPI_Interactable` actors through a Pawn-channel overlap query. | `CLAUDE.md` (B7.1, B9) | A character capsule blocks Pawn, so an NPC is targeted like any interactable. |
| Screens open UI-only with a cursor and focus (`FCraftingStationInteraction::Open`), close on I/Tab/Esc in `NativeOnPreviewKeyDown`, and return through `RestoreGameplayCursor` + `RestoreGameOnlyInput`. | `CraftingStationInteraction.cpp:59-101`; `CLAUDE.md` (UI) | The talk panel follows the same contract. |
| `Floor_0` spans ±2000 cm (cells −10..9). `BaseZone_Start` (15 × 15, centre cell (0, 0)) covers cells −7..7. | MCP bounds read (S2) | A separate camp does not fit beside the starter zone as placed. The starter zone moves to centre cell (−3, 0) (cells −10..4, still 15 × 15, `PlayerStart` inside). The camp takes cells 5..9. |
| Mannequin assets exist: `SKM_Quinn_Simple`, `ABP_Unarmed`. | `Content/Characters/Mannequins/` | No new art. |

## S4.3 Design

- **Zones gain two fields** (`UBaseZoneComponent`):
  - **`bAllowsBuilding`** (default true). A zone with false is an NPC's own base: `IsCellBuildable` ignores it, and its actor hides the red ring (nothing there is "buildable"). `FindZoneAt` and the tag still see it ("Trader Camp").
  - **`Residents`** (`TArray<TSubclassOf<UNpcDefinition>>`): the outdoor NPCs that live there.
  - `UBaseZoneSubsystem` broadcasts native `OnZoneRegistered` / `OnZoneUnregistered`, so `Base/` never depends on `Npc/`.
- **`UNpcDefinition`** (new area `Npc/`; abstract, const; data-only Blueprints `Content/LevelPrototyping/NpcSystem/Definitions/NPC_*`):
  - fields: `NpcName`, `GreetingLines` (one random per talk), `NpcActorClass` (`TSubclassOf<ANpcCharacter>`), `WalkSpeed` (150 cm/s), `IdleTimeMin` / `IdleTimeMax` (3 / 8 s);
  - `ValidateNpc` / `IsDataValid` report the first problem.
  - Production: `NPC_Trader` ("Trader", 3 greetings, `BP_Npc_Trader`).
- **`UNpcSubsystem`** (`UWorldSubsystem`) is the only outdoor-resident authority.
  - On `OnZoneRegistered` (with world authority) it spawns each valid resident once, on a random interior cell centre of the zone (the zone minus its ring).
  - It records the home zone and definition on the NPC.
  - On `OnZoneUnregistered` it destroys that zone's residents when the zone actor itself is being destroyed; any other unregistration (level teardown, end of play) only forgets them, because destroying actors inside that pass would race it. Zones are always loaded and never removed in play (Z-5), so a zone that unregisters and comes back does not happen today (self-review P3-2, accepted).
  - `GetResidents(Zone)`.
  - It never touches inventories, building or zones.
- **`ANpcCharacter`** (abstract; Blueprint `BP_Npc_Trader`):
  - an `ACharacter` with `NameLabel` (a `UTextRenderComponent` above the head, fixed to face the top-down camera);
  - orient-to-movement on, `MaxWalkSpeed` from the definition, no actor tick;
  - `AIControllerClass = ANpcAIController`, auto-possessed when spawned.
  - **Talk:**
    - `GetTalkResult(Interactor)` (`ENpcTalkResult`: `Ready`, `InvalidInteractor`, `NoPlayerController`, `OutOfRange` beyond `TalkRange` 300 cm, `NoTalkScreen`, `Busy`) is read-only.
    - `OpenTalkForInteractor(Interactor)` opens `W_NpcTalk` (UI-only, cursor, focus), faces the player and pauses the AI. A rejected request changes nothing.
    - `EndTalk()` resumes.
    - `EndPlay` closes an open panel.
  - Seams: `CreateTalkWidget`, `ShowTalkWidget`.
- **`ANpcAIController`** (native; controller tick off; timers plus the engine's path following):
  - **Idle** for a random time between `IdleTimeMin` and `IdleTimeMax`, then **wander**: a random interior cell of the home zone (`FRandomStream`) → `ProjectToNavigation` (seam) → `RequestWanderMove` (seam, `MoveToLocation`); a projected point outside the interior is not used, and a synchronous finish during the request never starts idling (self-review P2-1).
  - Move completion or failure returns to idle.
  - A failed projection or move tries up to 5 cells, then idles.
  - **Talk:** stop movement, clear the timer, turn the pawn toward the player. **Resume:** idle again.
  - Without a home zone it never wanders.
- **Talk panel** `UNpcTalkWidget` (abstract, `UI/`; `W_NpcTalk` in `NpcSystem/UI/`):
  - `NpcNameText`, `GreetingText`, `ByeButton`.
  - Bye, I, Tab and Esc close it: it is removed, `RestoreGameplayCursor` + `RestoreGameOnlyInput` run, and the NPC's `EndTalk` is called.
  - Closing twice is a no-op.
- **Content (MCP, plus one manual step):**
  - `NPC_Trader`;
  - `BP_Npc_Trader` (parent `ANpcCharacter`; Quinn mesh and `ABP_Unarmed`; `TalkScreenClass = W_NpcTalk`; **manual: add `BPI_Interactable`**, then MCP wires `EventInteract` → `OpenTalkForInteractor`, Press, hold 0);
  - `W_NpcTalk`.
- **Level:**
  - move `BaseZone_Start` to centre cell (−3, 0);
  - add `TraderCamp` (`ABaseZoneActor`, centre cell (7, 0), 5 × 9, `ZoneName` "Trader Camp", `bAllowsBuilding` false, `Residents` [`NPC_Trader`]);
  - the user saves (Ctrl+S): one changed and one new external actor.

## S4.4 S6 scope revised (for later) — ON HOLD

**Status (user, 2026-10-24): ON HOLD.** "khoan làm NPC trong nhà đi, tạm stop lại trong plan". House NPCs are not planned or built until the user resumes them; the scope below is kept for that time.

S6 becomes house NPCs:
- a summoning item per NPC;
- room detection on the building grid;
- requirements: a bed, a table, a chair and a minimum room size;
- the NPC moves into a valid room of the player's base.

It is planned in detail when S6 starts.

## S4.5 Invariants

- NPCs are never player-controlled and never change the player's inventory (S5 trading is one atomic transaction the player starts).
- `UNpcSubsystem` is the only outdoor-resident authority: one spawn per zone registration, removal on unregistration, authority only.
- An NPC's wander targets always lie in its home zone's interior.
- No tick in project code.
- A rejected talk request changes no input, cursor or widget state.
- A non-building zone never makes a cell buildable.

## S4.6 Tests (`NullTide.Npc.S4.*`, planned 5)

- **`DefinitionValidation`:**
  - each invalid case reports: no name, no greetings, no actor class, a bad idle range, a bad speed;
  - a test definition is valid;
  - `NPC_Trader` is valid.
- **`Residents`:**
  - a zone with residents spawns each once in its interior, with home zone and definition;
  - registering again does not duplicate;
  - an invalid definition is skipped;
  - destroying the zone removes its residents;
  - a non-building zone is not buildable and hides its ring, but `FindZoneAt` finds it.
- **`Wander`** (test controller with seams):
  - 200 targets all in the home interior, reproducible with the same seed;
  - idle → move → completion → idle;
  - failed projections retry, then idle;
  - talking stops, turns the pawn and schedules nothing; resuming idles;
  - no home zone means no wander;
  - controller tick off.
- **`TalkFlow`** (test NPC, viewport-free panel):
  - each refusal (out of range, no player controller, no screen class, busy) changes nothing;
  - a valid request fills the name and one greeting, pauses the AI and faces the player;
  - Bye closes and resumes;
  - I, Tab and Esc close;
  - closing twice is a no-op;
  - the NPC ending play closes the panel.
- **`ProductionWiring`:**
  - `BP_Npc_Trader`: `BPI_Interactable` (Press, hold 0), Quinn mesh and `ABP_Unarmed`, `W_NpcTalk`, `ANpcAIController`, no actor tick;
  - `W_NpcTalk` binds its widgets;
  - `NPC_Trader` points to `BP_Npc_Trader`.

Expected suite: 279 + 5 = 284.

## S4.7 PIE

1. The starter zone (moved left) and the Trader Camp (right, no red ring) are both visible. "Trader Camp" shows at the top right inside the camp, and building there says "Outside the build zone".
2. The Trader stands in the camp with its name above its head, strolls and pauses, and never leaves the camp.
3. F facing the Trader opens the panel (name, greeting, Bye). The Trader stops and turns to you.
4. Bye, Esc, Tab or I closes it; the Trader walks again; the cursor stays and the first click works.

## S4.8 Self-Critique (open risks)

- **R-S4-1 Manual interface step** on `BP_Npc_Trader` (known MCP limit).
- **R-S4-2 Navigation:** the nav mesh must cover the camp. This is PIE evidence; the wander logic is tested through seams.
- **R-S4-3 Moving `BaseZone_Start`** shifts the buildable area 6 cells left (still around `PlayerStart`). Anything already built in a PIE session is not saved, so nothing is lost.
- **R-S4-4 The name label** faces a fixed camera direction (the top-down camera does not rotate). A rotating camera would need a billboard later.
- **R-S4-5 Overlapping zones:** a non-building zone overlapping a building zone does not block building there; zones are authored without overlap for now.
- **R-S4-6 Module dependencies:** a Build.cs change; the game target is built to confirm.

---

# T1 - Test Ground: Plan

**Status:** COMPLETE (manual PIE PASS, 2026-10-23). APPROVED (user, 2026-10-23: "duyệt plan"). It runs right after S4 is committed, as its own checkpoint. It is a level-only change, with no code.
- **User direction (2026-10-23):** clear the terrain, or make one flat ground, so testing is easy. Keep only a few small bumps to test that building is refused.
- **Audit (MCP actor list, 2026-10-23):** `Lvl_TopDown` is still the top-down template on a 40 × 40 m `Floor_0`:
  - the raised centre platform: `SM_Cube13`–`16`, `SM_Ramp9`–`12`, `SM_QuarterCylinder5`–`12`, `SM_Cylinder` cap;
  - border walls: `SM_Cube2`–`5`, `SM_Cube17`–`20`;
  - corner blocks and ramps: `SM_Cube`, `6`–`12`, `SM_Ramp`–`8`, `SM_QuarterCylinder`–`4`;
  - 8 columns (`SM_Cylinder2`–`9`);
  - `SM_Cube14`, `Brush1`.

  The Trader camp holds a ramp, two columns and part of the east wall.

**Plan:**
1. Delete the template geometry above (about 45 static mesh actors and the brush). Keep the sky, lights, fog, `PlayerStart`, the pickups, the resource nodes, `C8B_Workbench`, `BP_StorageContainer` and both zones.
2. Scale `Floor_0` to 100 × 100 m (`BlockAll` stays), and grow `NavMeshBoundsVolume` to match.
3. Add three small test bumps inside the starter zone, away from the spawn:
   - a 40 cm step (Uneven);
   - a ramp of about 35° (TooSteep);
   - a small rock block (Blocked).
4. Rearrange the zones:
   - the starter zone 21 × 21 around `PlayerStart`;
   - `TraderCamp` 9 × 9 in open ground about 30 m away;
   - nothing overlaps.
5. Move only the items that the new layout affects. Ground items fall onto the floor (Z ≈ 0).

**Validation:**
- full automation unchanged (the tests use their own worlds);
- cook;
- `git status` / World Partition audit (deleted and added external actors listed);
- PIE: walk freely, the bumps refuse building with their reasons, the Trader walks in an empty camp.

---

# S5 - Trading (Shop, Wallet, Haggling): Detail Plan

**Status:** COMPLETE (manual PIE PASS). APPROVED (user, 2026-10-23; C-1 to C-3 confirmed in S5.4).
- Implementation runs in internal steps: clock and wallet → data and pricing → trade and haggle → UI.
- **Baseline:** `5c7403c` (T1). Full automation 284/284, cook 655/7/662.

## S5.1 User direction (2026-10-23)

- **Trade screen:** your inventory on the left; the exchange area in the middle (what you sell, what you buy); the Trader's store on the right.
- **Money:** coins, kept **apart from the inventory** in a wallet and shown on the player's HUD ("làm chung luôn": in this checkpoint, no split).
- **Rates:** you sell at **75%** of an item's value and buy at **100%**.
- **Store:** a fixed catalog, unlimited.
- **Haggling:**
  - solve one random simple problem (+ − × ÷) within **10 seconds**;
  - a correct answer gives a **random 5–20% discount**;
  - **3 haggled trades per day per NPC**, each with its own random discount;
  - the result is **kept per NPC** after the talk ends, until a trade uses it or a **new day** resets it.
- **Limits per trade:** up to **16 item types / stacks** on the selling side and 16 on the buying side.
- **Clicks:** clicking a stackable item asks for a **quantity**; a non-stackable item goes straight into the trade.

## S5.2 Audit (2026-10-23)

| Fact | Source | Consequence |
| --- | --- | --- |
| `TryApplyDefinitionTransaction` is atomic; `CanApplyDefinitionTransaction` is its exact preflight. | `InventoryComponent.h:54-68` | The item part of a trade is one inventory transaction. |
| Building pays inside a sequence whose last fallible step is the commit point ("activate first, pay last"); nothing fallible runs after it. | `CLAUDE.md` (B2); `ConstructionComponent.cpp` | Wallet + inventory: validate both, commit the inventory (the commit point), then apply the wallet change, which cannot fail at that point. |
| Capabilities are fragments; `UItemFragment_Stackable` holds `MaxStackSize`. | `Items/Fragments/` | `UItemFragment_TradeValue` marks tradable items. Stackability decides whether the quantity picker shows. |
| There is no day or clock system. | `Source/NullTide/Public` (no Day/Clock types) | A minimal game clock is needed for "per day" and "new day". |
| The talk panel (S4) ends the NPC's talk on close; the base tag (S3) sits at the top right. | `NpcTalkWidget.*`; `BaseZonePresenceComponent.*` | The trade screen opens from the talk panel. The coin readout sits on the HUD under the base tag. |

## S5.3 Design

**Game clock (minimal, new area `World/`)**
- `UGameClockSubsystem` (`UWorldSubsystem`):
  - `GetDay()` (starts at 1);
  - `GetTimeOfDay01()`;
  - `OnNewDay`, driven by a timer (no tick);
  - `DayLengthSeconds` (C-1, recommended **20 real minutes**).
- Test seam: `AdvanceToNextDay()`. No save (Phase 7), no lighting change.

**Wallet**
- `UWalletComponent` on `BP_TopDownCharacter`: `GetCoins()` (int32 ≥ 0) and `OnCoinsChanged`.
- Its mutators (`TryAddCoins`, `TrySpendCoins`) are reachable only by the trade authority (a friend), the same rule as inventory mutators.
- `StartingCoins` is a test aid: 0 in code, **200** on `BP_TopDownCharacter` for PIE.
- **HUD readout** (`UWalletHudWidget` / `W_WalletHud`): the coin count with an icon, under the base tag, always visible for the local player, refreshed by `OnCoinsChanged`. The component's timer-free pattern follows S3.

**Item values**
- `UItemFragment_TradeValue` (`Value` ≥ 1 coin) on the 13 production items.
- Starting values (data):
  - Wood 2, Stone 3, Scrap 5, Meat 6, Water Bottle 4, Sandwich 12;
  - Health Potion 25, Wood Arrow 1, Wood Bow 40, Axe 30, Pickaxe 30, Short Sword 45, Long Sword 80.

**NPC store data** (`UNpcDefinition`)
- `StoreItems`, the catalog: Stone, Scrap, Health Potion, Sandwich, Wood Arrow, Wood Bow, Axe, Pickaxe.
- `SellRate` 0.75 and `BuyRate` 1.0.
- `HaggleTradesPerDay` 3, `HaggleDiscountMin/Max` 0.05 / 0.20, `HaggleTimeLimit` 10 s.
- `MaxLinesPerSide` 16.
- `ValidateNpc` checks every field.

**Prices** (per side totals)
- `SellTotal = floor(Σ value × qty × 0.75)` and `BuyTotal = ceil(Σ value × qty × 1.0)`.
- `Discount = floor(BuyTotal × d)`, where `d` is the stored haggle roll (5–20%).
- `Net = SellTotal − (BuyTotal − Discount)`. Positive means coins are received; negative means coins are paid.
- With the maximum discount, buying costs 80% against selling at 75%, so buying and reselling never makes a profit.

**Trade authority** (native, on the NPC; only the talk partner)
- `QuoteTrade(Trader, Request)` is the exact read-only preflight. `TryTrade(Trader, Request)` commits.
- Checks:
  - the talk partner, with an inventory and a wallet;
  - 1–16 lines per side (stacks are summed by item);
  - every sold item has a value; every bought item is in the catalog;
  - quantities ≥ 1;
  - the wallet covers `−Net`;
  - the inventory preflight (sold items present; bought items fit after the removals).
- **Commit order:** guard → the checks → **one** `TryApplyDefinitionTransaction(Sell, Buy)` (the commit point) → the wallet adds or spends `Net` (cannot fail: the amount was checked under the guard, and the wallet has no other writer here) → the stored discount, if used, is consumed → `OnTradeResult` once.
- Results (`ENpcTradeResult`): `Success`, `NotTalking`, `NoInventory`, `NoWallet`, `EmptyTrade`, `TooManyLines`, `NotTradable`, `NotInStore`, `InvalidQuantity`, `NotEnoughCoins`, `InsufficientItems`, `InventoryFull`, `InventoryBusy`, `TradeInProgress`.

**Haggling** (authority on the NPC, kept per NPC)
- State on the NPC:
  - `HaggleDay`;
  - `HaggleTradesUsedToday` (0–3);
  - the stored discount (none, or 5–20%);
  - the active problem (the answer and its deadline), which stays on the NPC and never goes to the UI.
- `StartHaggle(Trader)` works for the talk partner when no discount is stored and fewer than 3 haggled trades happened today.
  - It creates one problem: a + b or a − b (10–99, never negative), a × b (2–12 × 2–9), or a ÷ b (an exact quotient 2–12, divisor 2–9).
  - It starts a 10 s world timer.
- `SubmitHaggleAnswer(Trader, Value)`:
  - correct and in time → roll the discount `d` ∈ [5%, 20%] and store it;
  - wrong or timed out → no discount (C-2: the attempt is used up).
- A stored discount survives the end of the talk and applies to this NPC's next trade, which consumes it and counts 1 of the 3 daily haggled trades.
- `OnNewDay` clears the stored discount and resets the count.
- A seeded `FRandomStream` keeps it testable.

**UI**
- **The talk panel** gets a **Trade** button when the NPC has a store.
- **`W_NpcTrade`** (UI-only; `UNpcTradeScreenWidget`) has three columns.
  - **Left – Your inventory:** the tradable stacks (name, quantity, sell value).
  - **Middle – Exchange:**
    - "You sell" and "You buy" (at most 16 lines each; LMB removes a line);
    - totals: sell (75%), buy, discount, net (+ receive / − pay), wallet;
    - **Haggle**: the problem, a 10-second countdown, the answer box and OK, then the result ("−12%" or "No discount"; "2/3 haggled trades left today");
    - **TRADE** (disabled with its reason), **Clear**, and feedback.
  - **Right – Store:** the catalog with buy prices.
  - **Picking:** a non-stackable item moves straight in. A stackable item opens a quantity picker (a slider plus a number, 1..available when selling, 1..stack size when buying; OK / Cancel).
  - **Back** returns to the talk; Esc/I/Tab close everything. Lists refresh on `OnInventoryChanged` and `OnCoinsChanged`.
- **Widgets:**
  - `UNpcTradeScreenWidget`;
  - `UNpcTradeItemRowWidget` (a reusable row);
  - `UTradeQuantityPickerWidget`;
  - `UWalletHudWidget`.

  Test seams: viewport-free creation and explicit trader, wallet and clock.

**Content (MCP)**
- trade values on the 13 items;
- `NPC_Trader`'s store data;
- `W_NpcTrade`, `W_NpcTradeItemRow`, `W_TradeQuantityPicker`, `W_WalletHud`;
- the Trade button in `W_NpcTalk`;
- `WalletComponent` (`StartingCoins` 200) and the wallet HUD on `BP_TopDownCharacter`.

There is no coin item and no map change.

## S5.4 Decisions (user) and open points

Recorded from 2026-10-23: section S5.1. Confirmed by the user (2026-10-23):
- **C-1 Day cycle:**
  - each cycle is a **day of 20 real minutes, then a night of 10 real minutes**;
  - the day number advances when a new day starts (after the night);
  - the HUD shows "Day N" (and Day/Night) clearly, for save/load later.
  - The clock exposes `GetDay()`, `IsNight()`, `GetPhaseProgress01()`, `OnNewDay` and `OnPhaseChanged`.
  - The night has no lighting change in S5 (out of scope; a later visual pass).
- **C-2 Haggle attempts:** **3 per NPC per day.**
  - A **wrong or timed-out answer uses 1** and, if attempts remain, the player may try a new random problem.
  - A **correct answer** stores the discount. The attempt is **used when a trade applies the discount**, and then a new problem can be tried if attempts remain.
  - Only one discount is stored at a time. A new day restores 3 attempts and clears a stored discount.
- **C-3:** trading itself is unlimited; only haggling is limited.

**Status update:** APPROVED (user, 2026-10-23; the plan with C-1 to C-3 as confirmed).

## S5.5 Invariants

- **One trade:** guard → checks → one inventory transaction (the commit point) → an infallible wallet change → discount consumption → one `OnTradeResult`. Any failure before the commit changes nothing.
- The wallet is never negative, and only the trade authority changes it.
- Prices, the haggle problem, its answer, the timer and the discount live on the NPC (authority), never trusted from the UI. `QuoteTrade` is exact and read-only.
- At most 3 haggle attempts per NPC per day (a wrong or timed-out answer uses one; a stored discount uses one when a trade applies it), each discount in [5%, 20%]. A stored discount applies once.
- No tick: the clock and the haggle timer are timers.

## S5.6 Tests (`NullTide.Npc.S5.*` and `NullTide.World.S5.*`, planned 8)

- **`World.S5.GameClock`:**
  - the day starts at 1;
  - the timer advances the day;
  - `OnNewDay` fires once per day;
  - `AdvanceToNextDay` works.
- **`Npc.S5.Wallet`:**
  - starting coins;
  - add and spend, never negative;
  - one `OnCoinsChanged` per change;
  - no public mutator;
  - the HUD readout updates.
- **`Npc.S5.TradeData`:**
  - fragment and NPC validation (each field);
  - the production values and store.
- **`Npc.S5.Pricing`:**
  - sell, buy, discount and net;
  - rounding on cheap items;
  - the 16-line limit;
  - summing duplicate stacks.
- **`Npc.S5.TradeTransaction`:**
  - selling, buying, both;
  - not enough coins or items, the reward does not fit, and paying frees the slot;
  - not the partner; not tradable or not in the store; empty; too many lines;
  - a nested call;
  - one inventory revision, one wallet change and one result;
  - `QuoteTrade` matches `TryTrade` and changes nothing.
- **`Npc.S5.Haggle`:**
  - problems are integers in range and reproducible;
  - correct within 10 s → a discount in [5, 20]%;
  - wrong or timed out → none (per C-2);
  - the discount survives the end of the talk and is used by the next trade only;
  - 3 haggled trades per day, then refused;
  - a new day resets it;
  - per NPC (two NPCs do not share).
- **`Npc.S5.TradeScreen`** (the real widgets, viewport-free):
  - the columns;
  - a non-stackable item moves straight in; a stackable one opens the picker, and OK / Cancel work;
  - the 16-line cap;
  - the totals match `QuoteTrade`;
  - TRADE is disabled with its reason, then trades once;
  - the haggle flow with the countdown;
  - Back, the close keys, and the NPC ending play.
- **`Npc.S5.ProductionWiring`:** the widget bindings; the Trade button; the wallet (200) and HUD on the player; the store data.

Expected suite: 284 + 8 = 292.

## S5.7 PIE

1. The HUD shows 200 coins (and Day 1).
2. Talk → Trade: three columns, store prices.
3. Sell 20 Wood (the picker asks for 20): +30 coins on TRADE.
4. Haggle: solve within 10 s → "−N%". Close and talk again: the discount is still there. Buy a Health Potion at the discounted price.
5. Let the timer run out: "No discount". After 3 haggled trades: "No haggles left today".
6. Too few coins: TRADE disabled, "Not enough coins". Esc: the Trader walks again.

## S5.8 Self-Critique (open risks)

- **R-S5-1 Two stores (inventory + wallet):** atomic by ordering. The wallet change after the inventory commit cannot fail (the amount was checked under the guard, the wallet has no other writer here, and it is clamped at int32 max). Tested by the transaction cases.
- **R-S5-2 Exploits:** buying at 80% at best against selling at 75% means no profit loop. Retrying a haggle (C-2) is bounded by 10 s and fresh problems.
- **R-S5-3 Day length** is a placeholder until a real day/night cycle exists; it is data.
- **R-S5-4 Screen size:** three columns need about 1200 × 650 px; this is checked in PIE.
- **R-S5-5 Fragments on 13 items** via MCP (instanced arrays on CDOs). If MCP cannot add them, a manual step may be needed.
- **R-S5-6 Size:** this is the largest checkpoint of Phase 6 (by user choice, in one go). It is implemented in internal steps (clock and wallet → data and pricing → trade and haggle → UI), each built and tested before the next.
- **R-S5-7 Persistence:** coins, the day and the haggle state reset when play restarts until save/load (Phase 7).

## S5.9 Implementation notes (deviations from S5.3, within the approved behaviour)

- **The HUD readout** is `UPlayerStatusHudComponent` + `UPlayerStatusWidget` (`BaseSystem/UI/W_PlayerStatus`), showing both coins and "Day N · Day/Night", instead of `UWalletHudWidget` / `W_WalletHud`.
- **The haggle limit** is `HaggleAttemptsPerDay` (C-2 counts attempts, not haggled trades). Discounts are whole percents (`HaggleDiscountMin/MaxPercent` 5 / 20), not fractions.
- **Day reset is lazy:** each haggle call compares the stored day with the clock's day. This replaces an `OnNewDay` binding per NPC: no binding lifetimes, and the same effect.
  - A problem given on one day and answered or expired on the next neither earns a discount nor spends the new day's attempt (self-review).
- **The day and night lengths** are code constants on `UGameClockSubsystem` (1200 s / 600 s), not data. A data asset or setting can come with the day/night visuals.
- **Trade results** add `NoStore`. Overflow is refused with `InvalidQuantity`: a side worth more than int32 coins, or a sale the wallet cannot hold.
- **Quantity lines** below 1 are refused before lines of one item are summed.
- **The trade screen** follows the clock's `OnPhaseChanged`, so a new day refreshes the totals and the haggle state while it is open.
- **The trade screen** lives on the NPC (`OpenTrade` / `CloseTrade`; `Back` returns to the talk, `Close` ends it). Its row buttons are made non-focusable in the asset (a `UButton`'s focusability is fixed at construction in 5.8).

