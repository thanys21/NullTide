# Null Tide — Master Project Plan

> **Purpose:** Single source of truth for Claude Code and human development.
>
> **Project:** Null Tide  
> **Engine:** Unreal Engine 5.8.2  
> **Genre:** Top-down Survival Colony Builder  
> **Core pitch:** Project Zomboid + Core Keeper, starting from one survivor and growing toward a colony.  
> **Primary platform:** PC / Steam  
> **Current development state:** Phase 4 Crafting COMPLETE (C1-C8). Crafting C8 Workstations COMPLETE: C8A, C8B and C8C approved (C8C manual rendered PIE 13 PASS + 1 N/A, Tab focus retest 6/6 PASS; full automation 190/190). Phase 5 Building IN PROGRESS: B0 APPROVED; B1 (data model, grid, occupancy, placement validation) COMPLETE / APPROVED (full automation 198/198, cook PASS); B2 (construction authority: activate first, pay last) COMPLETE / APPROVED (full automation 219/219, cook PASS); B3 (Wooden Floor: first buildable, construction component on the player) IMPLEMENTED AND VALIDATED, awaiting user acceptance (full automation 223/223, cook PASS). Crafting queue and the inventory/crafting UI-UX pass are deferred.

---

# 1. Project Vision

Null Tide is a top-down survival colony-builder where the player begins as a single survivor and gradually rebuilds a functioning settlement.

The intended gameplay loop is:

```text
Explore
  ↓
Gather resources
  ↓
Manage inventory / tools / storage
  ↓
Craft
  ↓
Build
  ↓
Grow colony
  ↓
Automate / specialize colonists
  ↓
Explore deeper regions / story
```

The game should favor:

- clear systemic interactions;
- reusable data-driven systems;
- simple but expandable combat;
- inventory and world interactions with exact runtime identity;
- systems that can later support colony automation;
- minimal one-off item subclasses;
- modular Unreal architecture;
- production-safe authority boundaries.

---

# 2. Technical Environment

## Unreal Engine

```text
Engine:
D:\game\epic\UE_5.8

Project:
C:\Users\loithienly\Documents\Unreal Projects\NullTide

Main map:
/Game/TopDown/Lvl_TopDown
```

## Editor / IDE

- Unreal Engine 5.8.2
- Visual Studio Code
- Visual Studio 2022 C++ toolchain
- Claude Code
- OpenAI Codex (optional read-only reviewer, only when the user brings it in)
- Unreal MCP
- Git / GitHub

## Standard Build

PowerShell:

```powershell
& "D:\game\epic\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
  NullTideEditor Win64 Development `
  -Project="C:\Users\loithienly\Documents\Unreal Projects\NullTide\NullTide.uproject" `
  -WaitMutex
```

Generate project files:

```powershell
& "D:\game\epic\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
  -projectfiles `
  -project="C:\Users\loithienly\Documents\Unreal Projects\NullTide\NullTide.uproject" `
  -game `
  -engine `
  -progress
```

---

# 3. Critical Development Rules

These rules apply to Claude Code, any other agent, and human edits.

## Unreal asset safety

- Never edit `.uasset` files directly.
- Never manually delete World Partition `__ExternalActors__` `.uasset` files.
- Blueprint / UMG / map / asset modifications must go through Unreal Editor or Unreal MCP.
- Do not use `Save All` unless explicitly requested.
- Save only assets intentionally changed by the current task.
- Before restoring or replacing a `.uasset`, close Unreal Editor to avoid file locks.

## Build / hot reload

- Prefer clean builds over Hot Reload for structural native changes.
- Live Coding is disabled.
- If `UnrealEditor.exe` holds `UnrealEditor-NullTide.dll`, close the editor before rebuilding.
- If Unreal is force-closed and shows **Restore Packages**, use **Skip Restore** unless unsaved asset work was explicitly approved.
- Duplicate automation registrations can indicate stale Hot Reload state; trust tests only after a clean process/module restart.

## Validation

Every milestone should normally use:

```text
1. focused automation
2. relevant regression suites
3. full permanent automation
4. rendered PIE manual acceptance
5. Windows cook
6. Content/map audit
7. report/progress docs
8. self-review (including a fresh-context review pass for native code) and the handoff report
9. user-approved Git commit
```

These steps run inside the nine-step checkpoint workflow in `AGENTS.md` (section "Development Workflow — Claude Plans and Implements, User Approves"). That workflow also adds the plan self-critique and user approval gate before implementation, and it defines the self-review and the handoff report.

Do not fabricate manual PIE observations.

Automation may prove hidden semantics such as:

- exact ItemId identity;
- inventory revisions;
- atomic mutations;
- timer/state behavior;
- stale-slot clearing.

Manual PIE should record only visible behavior actually observed.

## Git discipline

Commits and pushes are authorized by the user (see the workflow in `AGENTS.md`):

- Commit only after the user explicitly approves that commit.
- Stage selectively: only the approved files or hunks (`git add <path>`, `git add -p <path>`). Do not use `git add .` or `git add -A`.
- Inspect `git diff --cached` before committing. Nothing unapproved may be staged.
- Keep unrelated changes in separate commits.
- Push only on a separate explicit user instruction. Never push automatically.
- Do not start the next checkpoint or milestone without user approval, and never before the previous clean checkpoint is committed.

---

# 4. AI / MCP Workflow

## Claude Code

Claude runs from:

```powershell
cd "C:\Users\loithienly\Documents\Unreal Projects\NullTide"
claude
```

Project MCP:

```json
{
  "mcpServers": {
    "unreal-mcp": {
      "type": "http",
      "url": "http://127.0.0.1:8000/mcp"
    }
  }
}
```

Claude should use Unreal MCP for:

- Blueprint inspection/modification;
- UMG;
- actors;
- map changes;
- ItemDefinition assets;
- asset validation;
- Unreal automation.

## Agent roles

The user decided on 2026-10-14 that Claude does both planning and implementation, and the user approves:

```text
Claude Code → plans, self-critiques, implements, validates, self-reviews; sole write owner (code, tests, docs, Content, Git)
User        → approves plans and decisions, manual PIE, commit and push authorization
```

No standing reviewer agent is used. Another agent takes part only when the user explicitly brings one in for a specific task, and then only as a read-only reviewer. Only Claude writes to the repository or modifies Unreal assets.

The detailed workflow is in `AGENTS.md`, section "Development Workflow — Claude Plans and Implements, User Approves", and that section is the authority. It covers roles, the checkpoint steps, the self-review, the handoff report and the handling of findings.

---

# 5. Core Architecture

## Items

### UItemDefinition

Static canonical item configuration.

Contains:

- display metadata;
- icon;
- canonical fragments;
- world presentation;
- capability configuration.

Avoid item-specific C++ subclasses.

### UItemInstance

Runtime inventory/storage identity.

Owns:

- exact `ItemId`;
- definition class;
- runtime state;
- stack `Quantity`.

### Canonical fragments

Current fragment-oriented item architecture includes capabilities such as:

```text
Resource
Consumable
Food
Drink
Healing
Equippable
Weapon
Ammo
Durability
Tool
WorldPresentation
Stackable
```

Legacy `Fragments` remains compatibility-only.

Production definitions use canonical `ItemFragments`.

---

# 6. Inventory Authority

`UInventoryComponent` is the sole player inventory authority.

Responsibilities include:

- ordered runtime item stacks;
- capacity;
- exact ItemId lookup;
- add/remove;
- stacking;
- inventory snapshots/events.

UI must observe inventory snapshots/events.

Do not create a second inventory authority.

Default capacity:

```text
24 occupied slots/stacks
```

---

# 7. Interaction

Current interaction input:

```text
F → Interact
```

Architecture:

```text
PlayerInteractionComponent
      ↓
BPI_Interactable
      ↓
Interact(Interactor)
```

Inventory input:

```text
I or Tab → IA_OpeningInventory
   → IMC_Default
   → BP_TopDownController
   → W_Inventory (inventory + character + crafting)
```

`Tab` opens the same screen as `I` (approved in C6A; the old rule "Tab does not open inventory" is retired). While the screen is open, `I`, `Tab` or `Esc` closes it (`UInventoryScreenWidget::NativeOnKeyDown`).

Tool Equip/Unequip is NOT Enhanced Input.

It uses:

```text
W_Inventory button
→ UInventoryScreenWidget::ExecuteSelectedToolAction()
→ UToolLoadoutComponent
```

---

# 8. Completed Roadmap

---

# Phase 1 — Foundation ✅

Initial Unreal project and interaction foundation.

Completed areas include:

- project creation;
- top-down gameplay direction;
- player movement;
- interaction interface;
- core project structure;
- C++ + Blueprint integration.

---

# Phase 2 — Items & Inventory ✅

## G1 — Item Catalog ✅

Established canonical ItemDefinition + fragment architecture.

## G2 — World Presentation ✅

Generic item world presentation driven by definitions.

## G3 — Pickup Population ✅

Generic `BP_PickUpItem` actors populate the world.

Pickup logic uses the central inventory authority.

## G4 — Inventory UI ✅

Inventory UI implemented.

## G5 — Drag / Drop Semantics ✅

Inventory drag/drop semantics established.

## G6 — Drop To World ✅

Atomic flow:

```text
validate
→ determine safe transform
→ spawn generic pickup
→ apply presentation
→ remove exact ItemId
→ rollback spawned pickup on removal failure
```

World pickup behavior preserves runtime identity/quantity as required.

## G7 — Storage / Containers ✅

Implemented:

```text
UStorageComponent
AStorageContainerActor
UStorageScreenWidget
UStorageSlotWidget
BP_StorageContainer
W_StorageScreen
W_StorageSlot
```

Storage:

- ordered;
- capacity-bound;
- exact identity aware;
- atomic transfers;
- integrated with inventory.

Phase 2 complete.

---

# Phase 3 — Resource Gathering ✅

## RG1 — Generic Timed Resource Node ✅

Implemented `AResourceNodeActor`.

States:

```text
Idle
Gathering
Depleted
```

Config:

```text
OutputItemDefinition
MaxYield
RemainingYield
GatherDuration
```

Gathering:

```text
F once
→ timed gather
→ inventory award
→ yield decrement only after successful inventory add
```

Movement / Jump cancel gathering.

Progress UI:

```text
W_GatherProgress
UGatherProgressPresenterComponent
```

No resource actor tick.

---

## RG2 — Tree / Rock / Scrap ✅

Production variants:

### Tree

```text
Output: Wood
MaxYield: 5
GatherDuration: 2.0
```

### Rock

```text
Output: Stone
MaxYield: 5
GatherDuration: 2.5
```

### Scrap

```text
Output: Scrap
MaxYield: 4
GatherDuration: 1.5
```

World population:

```text
3 Trees
3 Rocks
2 Scrap nodes
```

---

## RG3 — Tool Requirement ✅

Created:

```text
Item_Axe
Item_Pickaxe
```

Tool configuration:

```text
Tree  → Axe
Rock  → Pickaxe
Scrap → None
```

Tool system:

```text
UToolLoadoutComponent
```

Stores exact inventory-owned `ItemId`.

Player does NOT actively hold/switch tools.

Correct tool only needs to be equipped in the dedicated tool slot.

Tool UI:

```text
Axe Slot
Pickaxe Slot
```

Context-sensitive button:

```text
selected tool not equipped
→ Equip Tool

selected exact equipped tool
→ Unequip Tool

selected non-tool
→ disabled
```

Tools remain inventory-owned.

If equipped tool leaves inventory, its slot clears safely.

---

## RG4 — Yield / Depletion ✅

Reused RG1 yield authority.

Rules:

```text
success → consume exactly 1 yield
cancel → consume 0
inventory failure → consume 0
missing tool → consume 0
```

At zero:

```text
state → Depleted
future gather rejected
F prompt suppressed
```

No regeneration yet.

---

## RG5 — Inventory Integration ✅

Gather reward authority:

```text
AResourceNodeActor::CompleteGather
→ UInventoryComponent::TryAddDefinition
```

Full-inventory rejection is atomic.

If reward cannot fit:

```text
no reward
no yield consumption
no inventory revision
no event
```

Freeing space allows retry.

Inventory UI refresh remains:

```text
OnInventoryChanged
+ GetItemsSnapshot
```

No direct UI injection from resource nodes.

Phase 3 complete.

---

# Inventory Stacking ✅

Inserted after RG5 and before Crafting.

## Static configuration

`UItemFragment_Stackable`

```text
MaxStackSize = 50
```

Stackable production definitions:

```text
Wood
Stone
Scrap
WoodArrow
Sandwich
Meat
WaterBottle
HealthPotion
```

Non-stackable examples:

```text
Axe
Pickaxe
ShortSword
LongSword
WoodBow
```

## Runtime model

`UItemInstance` owns:

```text
ItemId
DefinitionClass
Quantity
```

One runtime ItemId represents one stack.

## Add semantics

```text
add quantity
→ fill compatible stacks in deterministic order
→ create overflow stacks only when needed
```

Capacity counts stacks/slots rather than units.

Example:

```text
Wood 48/50
+5 Wood

→ 50/50
→ 3/50
```

## Atomicity

Multi-quantity adds preflight the full operation.

If the entire quantity cannot fit:

```text
no partial merge
no new stack
no revision change
no event
```

## G6 stack drop

Current behavior:

```text
selected stack
→ drop entire stack
→ world pickup stores quantity
```

No split-stack UI yet.

Pickup back into inventory uses normal atomic stack merge rules.

## G7 storage stacks

Storage supports whole-stack transfer.

Destination can merge compatible stacks.

Failed whole-stack transfer is atomic.

## UI

Inventory slot shows:

```text
xN
```

only when `Quantity > 1`.

## Validation completed

```text
Stacking focused: 7/7
Regression matrix: 33/33
Full automation: 121/121
Rendered PIE A-M: PASS
Windows cook: PASS
```

No map or World Partition changes from stacking.

---

# 9. Current World Snapshot

Resource nodes:

```text
3 Tree
3 Rock
2 Scrap
```

Storage:

```text
1 BP_StorageContainer
```

Player crafting (C5):

```text
BP_TopDownCharacter
  CraftingComponent (UCraftingComponent)
    AvailableRecipes:
      Recipe_Axe        Wood x3 + Stone x5 → Axe x1
      Recipe_WoodArrow  Wood x1 → WoodArrow x5
      Recipe_Pickaxe    Wood x2 + Stone x3 → Pickaxe x1 (timed, 3 s)
```

Crafting is the right column of the inventory screen (I or Tab). Since C6B the CRAFT button crafts the selected recipe through UCraftingComponent and shows a result message.

Generic pickups exist for production/test catalog items.

Some user-authored Axe/Pickaxe pickups may exist in addition to approved RG3 pickups.

Do NOT delete or rename pickup actors merely because their actor label is stale.

Always audit:

```text
Actor Label
ItemDefinition
Quantity
Transform
External Actor package
Git history / approved scope
```

before cleanup.

---

# 10. Current Status

```text
Phase 1 Foundation                  ✅
Phase 2 Items & Inventory           ✅
Phase 3 Resource Gathering          ✅
Inventory Stacking                  ✅
Crafting C1 Recipe Data Model       ✅
Crafting C2 Quantity Queries        ✅
Crafting C3/C4 Atomic Transaction   ✅
Crafting C5 Crafting Authority      ✅
Crafting C6A Crafting UI Foundation  ✅
Crafting C6B Craft Execution from UI ✅
Crafting C7 Timed Crafting          ✅
Crafting C8A Station Data/Authority  ✅
Crafting C8B Workbench Actor/Interaction ✅
Crafting C8C Workstation Crafting UI  ✅
Phase 4 Crafting                    ✅

NEXT:
Phase 5 Building: IN PROGRESS — B0 approved, B1, B2 complete; B3 implemented, awaiting acceptance (see BUILDING_PROGRESS.md)
```

---

# 11. Upcoming Roadmap

---

# Phase 4 — Crafting

The next work begins with **Crafting Foundation**.

Do not implement the entire crafting phase in one change.

Use incremental checkpoints.

---

## C1 — Crafting Data Model

Goal:

Define recipes as static data.

Recommended architecture:

```text
UCraftingRecipeDefinition
```

or equivalent canonical recipe asset.

Recipe fields should conceptually support:

```text
RecipeId / Definition identity
DisplayName
Icon

Ingredients[]
  ItemDefinition
  Quantity

Outputs[]
  ItemDefinition
  Quantity

CraftDuration
Optional workstation requirement
```

Example:

```text
Stone Axe

Ingredients:
Wood x3
Stone x5

Output:
Axe x1
```

Rules:

- recipe data is static;
- runtime crafting state does not belong in recipe assets;
- quantities use the inventory stacking authority;
- do not hard-code recipes in widgets.

Checkpoint:

```text
recipe data model
+ tests
+ no production UI yet
```

Agreed decisions (details in `CRAFTING_PLAN.md`):

- Recipes are data-only Blueprint subclasses of an abstract, const `UCraftingRecipeDefinition`, referenced by `TSubclassOf`, the same pattern as `UItemDefinition`.
- No workstation field in C1; it is designed in C8.
- First production recipes: `Recipe_Axe` (Wood x3 + Stone x5 → Axe x1) and `Recipe_WoodArrow` (Wood x1 → WoodArrow x5).

---

## C2 — Inventory Quantity Queries

Before consuming recipes, provide authoritative quantity queries.

Needed concepts:

```text
GetTotalQuantity(ItemDefinition)

CanConsumeRecipe(recipe)

CanAddRecipeOutputs(recipe)
```

Must sum quantities across multiple stacks.

Example:

```text
Wood:
stack A = 20
stack B = 8

GetTotalQuantity(Wood) = 28
```

No UI dependency.

Implemented (details in `CRAFTING_PROGRESS.md`):

- Generic `UInventoryComponent` queries: `GetTotalQuantity`, `CanRemoveDefinitionQuantity`, `CanAddDefinitionQuantity`, `CanAddDefinitionQuantities`. The inventory never depends on crafting types.
- `TryAddDefinitionQuantity` and the add queries share one preflight built on `FInventoryStacking::BuildAddPlan`.
- Recipe queries live in `UCraftingQueryLibrary` (`CheckRecipeIngredients`, `CheckRecipeOutputCapacity`).
- Output capacity is checked before consumption. This is conservative (never approves a craft that cannot fit) but can reject a full inventory that consumption would free. The exact check belongs to C3/C4.

---

## C3/C4 — Atomic Craft Transaction (one checkpoint)

Ingredient consumption (C3) and output commit (C4) are **one atomic inventory transaction**. They are never separate gameplay transactions.

Required contract:

```text
validate all ingredient and output requests
→ plan deterministic ingredient consumption (inventory order)
→ project the post-removal inventory
→ plan all outputs against that projection
→ commit removals + outputs together
→ Revision +1, OnInventoryChanged once
```

If any stage cannot complete:

```text
inventory remains unchanged
(no quantity change, no ItemId removed or created, no revision, no event)
```

Example:

```text
Need Wood x12, Wood stacks 5, 10
→ first stack removed, second stack becomes 3

Wood 50/50, recipe Wood x10 → Wood x5
→ projected 40/50 → final 45/50 (same stack)

recipe → WoodArrow x5 with WoodArrow 48/50
→ merge to 50/50, overflow 3 into a new stack
```

Implemented (details in `CRAFTING_PROGRESS.md`):

- Generic `UInventoryComponent::TryApplyDefinitionTransaction(Removals, Additions)` (native only). The inventory never depends on crafting types.
- Projection uses `FInventoryStackEntry { Item, DefinitionClass, Quantity }`; `BuildAddPlan` now wraps `BuildAddPlanForEntries`, the single stacking rule set for normal adds, storage transfers and craft outputs.
- `FCraftingTransaction::TryCraftRecipeAtomic` is the authoritative craft path; `UCraftingQueryLibrary` checks stay advisory.
- Exact-class matching is kept for removals, totals and stacking.

---

## C5 — Crafting Component / Authority

Create one runtime crafting authority.

Potential architecture:

```text
UCraftingComponent
```

Responsibilities:

- known recipes;
- validation;
- start/cancel craft;
- ingredient consumption;
- output creation;
- crafting state.

Avoid crafting authority inside widgets.

Implemented (complete; details in `CRAFTING_PROGRESS.md`):

- `UCraftingComponent` on `BP_TopDownCharacter`, registered recipes `Recipe_Axe`, `Recipe_WoodArrow` (authored order).
- Blueprint API: `GetAvailableRecipes`, `IsRecipeAvailable`, `CanCraftRecipe` (exact read-only preflight), `TryCraftRecipe`, `GetInventory`, `OnCraftingResult`.
- Crafting goes through `FCraftingTransaction::TryCraftRecipeAtomic` → `UInventoryComponent::TryApplyDefinitionTransaction`; the component stores no inventory state.
- Instant crafting only; start/cancel and crafting state arrive with C7 timed crafting.
- Approved design decisions: `OnCraftingResult` fires for success and failure (preflights never notify); reentrant `TryCraftRecipe` returns `CraftInProgress` silently; `FCraftingTransactionResult` is reused with `RecipeNotAvailable`/`CraftInProgress` appended; `CanCraftRecipe` does not report busy states; crafting result enums carry `ScriptName = "...ResultCode"`.

---

## C6 — Minimal Crafting UI

Create minimal UI only after authority/tests exist.

Suggested:

```text
Recipe List
Recipe Details
Ingredients
Craft Button
```

Show:

```text
Wood  8 / 10
Stone 5 / 5
```

Craft button:

```text
enabled only when recipe can craft
```

UI observes crafting/inventory authority.

C6 is split into two checkpoints:

- **C6A — Crafting UI Foundation** (complete; manual rendered PIE 12/12 PASS, user-confirmed): crafting shown inside the inventory screen as the right column (inventory left, character middle, crafting right); read-only data binding; CRAFT enabled only when the exact `CanCraftRecipe` succeeds, but not wired; `Tab` added as an inventory key.
- **C6B — Craft Execution from UI** (complete; manual rendered PIE 12/12 PASS, user-confirmed): wire CRAFT to `UCraftingComponent::TryCraftRecipe`, feedback from `OnCraftingResult`, rendered PIE acceptance.

---

## C7 — Timed Crafting

Optional once instant crafting works.

Flow:

```text
StartCraft
→ timer/progress
→ commit craft at completion
```

Need clear cancellation semantics.

Decide whether ingredients are:

```text
reserved at start

or

validated/consumed at completion
```

before implementation.

Do not mix approaches.

Implemented (complete; manual rendered PIE 12 PASS, 1 NOT TESTED; details in `CRAFTING_PROGRESS.md`):

- **Consume at completion.** Start runs the exact preflight only; completion re-validates and runs the C3/C4 atomic transaction; an inventory change during the craft makes completion fail atomically.
- One active craft per player (`CraftInProgress` for a second start); no queue, no reservations.
- `UCraftingComponent::TryStartCraft` is the UI entry (instant recipes craft immediately, timed recipes return `Started`); `TryCraftRecipe` stays instant-only (`TimedCraftRequired`); `CancelCraft`, `IsCrafting`, `GetActiveCraftRecipe`, `GetActiveCraftProgress`, `GetActiveCraftRemainingSeconds`; events `OnCraftStarted`, `OnCraftCancelled` (Explicit, Movement, Jump), `OnCraftingResult` at completion.
- Cancellation: explicit, movement (more than 10 cm horizontally, detected natively from character movement, including click-to-move) and jump/fall. Closing the UI, storage and inventory changes do not cancel. `EndPlay` clears everything silently.
- Production timed recipe `Recipe_Pickaxe` (Wood x2 + Stone x3 → Pickaxe x1, 3 s); Axe and WoodArrow stay instant.

---

## C8 — Workstations

Later:

```text
Campfire
Workbench
Forge
etc.
```

Recipe may require workstation type.

Do not add this to the first crafting checkpoint unless necessary.

Approved design (details in `CRAFTING_PLAN.md`, C8):

- Stations are context, not crafting authorities. `UCraftingComponent` stays the player's only crafting authority; there is no station inventory or transaction.
- Station kinds are `UCraftingStationDefinition` data-only Blueprint subclasses (`StationName`, `StationIcon`, `UseRange` default 300 cm). A recipe's `RequiredStation` names one kind by exact class; null means a hand recipe.
- All recipes stay registered on the player's `AvailableRecipes`. A station lists its own recipes first, then hand recipes; the `I`/`Tab` screen lists hand recipes only.
- Station access (kind, lifetime, range) is checked at query, start and completion. Destroying a station cancels a dependent timed craft (`StationLost`); hand recipes never depend on a station. One active craft per player across all contexts.
- Station-aware API takes the station explicitly: `GetRecipesForStation`, `CanCraftRecipeAtStation`, `TryStartCraftAtStation`, `GetActiveCraftStation`.
- C8C reuses `W_Inventory`, opened by `F` on the station, with the crafting panel in station context.
- Checkpoints:
  - C8A: data model and authority. COMPLETE / APPROVED; adds `Station_Workbench` and `Recipe_ShortSword` (Wood x2 + Scrap x4 -> ShortSword x1, 4 s, Workbench), not yet registered.
  - C8B: `ACraftingStationActor`, `BP_Workbench`, one bench placed in `Lvl_TopDown`. COMPLETE / APPROVED.
  - C8C: UI integration, `Recipe_ShortSword` registered fourth, `BP_Workbench` opens `W_Inventory`. COMPLETE / APPROVED.

---

# Phase 5 — Building

After basic crafting is stable.

Planned topics:

```text
Building definitions
Placement preview
Grid / free placement decision
Collision validation
Resource cost
Atomic build confirmation
Cancel placement
Demolish
Refund rules
```

Initial production examples:

```text
Floor
Wall
Door
Storage
Campfire / Workbench
```

Building costs should use stack-aware inventory quantity authority.

B0 design APPROVED (see `BUILDING_PLAN.md`); progress in `BUILDING_PROGRESS.md`. B1 is COMPLETE / APPROVED. B2 is COMPLETE / APPROVED (`BUILDING_PLAN.md` section 5.2, decisions D1–D8, D3 option (a)). Checkpoints:
- B1 data model and grid validation core;
- B2 atomic construction authority;
- B3 Wooden Floor end to end;
- B4 building mode, ghost preview and input;
- B5 Wooden Wall and Door;
- B6 demolition and refund.

---

# Phase 6 — Colony

After the player can gather/craft/build.

Planned systems:

```text
Colonists
Needs
Jobs
Work priorities
Beds
Storage hauling
Resource jobs
Crafting jobs
Building jobs
Schedules
Recruitment
```

Start simple.

First colony checkpoint should probably be:

```text
one NPC
→ assign one job
→ navigate to target
→ perform one reusable job action
```

Do not begin with complex RimWorld-level simulation.

---

# Phase 7 — Save / Load

Persist important runtime state.

Likely scope:

```text
Player transform
Inventory stacks
ItemId / Quantity
Tool loadout
Storage contents
Resource-node RemainingYield / Depleted
Placed buildings
Colonists
World progression
```

Exact runtime identity must be considered carefully.

Do not rely on raw UObject pointer persistence.

---

# Phase 8 — Combat

Combat remains intentionally lightweight.

Potential sequence:

```text
Health / damage
Weapon use
Melee
Ranged
Enemy actor
Basic AI
Loot
Death
```

Do not let combat architecture break item/tool authority.

---

# Phase 9 — Story / Exploration

Null Tide world progression.

Possible systems:

```text
Points of interest
Environmental storytelling
Quests/events
Region progression
Null Tide phenomenon
Special resources
Colony story events
```

Keep story systems data-driven when possible.

---

# Phase 10 — Demo / Vertical Slice

Goal:

A small polished playable loop.

Target loop:

```text
Spawn
→ collect basic tools
→ gather Wood / Stone / Scrap
→ manage stack inventory
→ store resources
→ craft useful items
→ build first structures
→ establish first colony functionality
→ encounter basic danger/story hook
```

Demo preparation:

```text
UI polish
Audio
Visual feedback
Performance
Save/load
Onboarding
Bug fixing
Packaging
Steam build
```

---

# 12. Deferred Inventory Features

These are deliberately outside the completed stacking milestone.

## Split Stack

Needed eventually.

Potential UX:

```text
Right click stack
→ Split

or

Shift drag
```

Do not implement until required by Crafting/storage UX.

## Partial Storage Transfer

Current storage transfers whole stacks.

Later allow:

```text
transfer N
```

Must remain atomic.

## Quick Bar

Future:

```text
1-9 item/tool shortcuts
```

Needs careful relationship with:

```text
inventory ItemId
tool loadout
consumables
weapons
```

## Weight

Potential future system.

Do not conflate:

```text
slot capacity
stack quantity
weight capacity
```

They are separate concepts.

## Crafting Queue (user request after C7 PIE)

Needed later, not part of C7 or C8 unless approved:

```text
queue up to 5 crafts per player
→ they run one after another
→ each queued entry can be cancelled (button or right-click)
```

Needs its own design: when each entry's ingredients are checked (C7 consumes at completion), what happens to later entries when one fails or is cancelled, and how movement cancellation applies to the whole queue.

## Inventory / Crafting UI-UX Pass (user request after C7 PIE)

Make the combined inventory + character + crafting screen more convenient. Known items:

```text
keep last craft result + selected recipe across close/reopen
remove the occasional quantity flicker after a result message clears
show crafting progress/completion while the screen is closed (HUD)
review layout density and the empty Item Details state
```

---

# 13. Tool / Equipment Future

Current ToolLoadout:

```text
Axe
Pickaxe
```

Future equipment may be separate:

```text
Weapon
Helmet
Chest
Legs
Accessory
```

Do not force tools into a general equipment architecture prematurely.

Planned hand / weapon model (user direction, not yet designed):

```text
Primary weapon slot
Secondary weapon slot
Two-handed weapons occupy both hands
Torch is held in the free (unused) hand
```

Item and recipe definitions stay class-based data-only Blueprints so these capabilities can be added as fragments later.

Potential future additions:

```text
Durability
Tool efficiency
Gather speed
Tool tiers
Repair
```

These remain deferred.

---

# 14. Resource Gathering Future

Current nodes permanently deplete.

Possible later additions:

```text
Resource regeneration
Respawn
Random yield ranges
Rare drops
Tool efficiency multiplier
Biome resource tables
Node health / hit-based gathering
Animation / VFX / audio
```

Do not implement without a dedicated milestone.

---

# 15. Recommended Next Immediate Checkpoint

## Building — B4 Building Mode, Ghost Preview and Placement Input (after B3 is accepted)

Phase 4 Crafting is COMPLETE (C1–C8; see `CRAFTING_PROGRESS.md`). Phase 5 Building is IN PROGRESS:
- B0 architecture is APPROVED (`BUILDING_PLAN.md`).
- B1 (data model, grid coordinates, occupancy, placement validation) is COMPLETE / APPROVED.
- B2 (construction authority, activate first and pay last) is COMPLETE / APPROVED (`bccd0bc`).
- **B3 (Wooden Floor) is IMPLEMENTED AND VALIDATED, awaiting user acceptance.** `BP_BuildFloor`, `Building_WoodFloor` and `UConstructionComponent` on `BP_TopDownCharacter`; 4 B3 tests; full automation 223/223; cook PASS 624/7/631.

Claude instruction (only after B3 is accepted and committed):

```text
1. Read BUILDING_PLAN.md (sections 3, 4.2, 7 B4) and BUILDING_PROGRESS.md
   (B3, including the ground support-surface finding).
2. Write the B4 plan with its self-critique and STOP for user approval.
   It must decide how the Lvl_TopDown ground (Floor_0, WorldDynamic,
   BlockAllDynamic) becomes a support surface: change the ground's collision
   (map/World Partition change) or widen the B1 support rule.
3. B4 adds the placement component (mode, cursor snap, rotation, confirm and
   cancel, exit before any UI-only screen), the ghost preview and the HUD,
   and IMC_Building input. The preview only calls CanBuild; confirm calls
   TryBuild once per press.
4. No walls, doors or demolition in B4.
5. Build + focused tests + regression + full suite + BP/UMG compile + cook +
   content/WP audit + the user's manual rendered PIE.
6. Update BUILDING_PROGRESS.md and this plan. STOP before commit with the
   handoff report (AGENTS.md workflow).
```

This keeps Building incremental and keeps inventory, crafting, storage and interaction authority unchanged.

---

# 16. Milestone Definition of Done

A milestone is not complete just because code compiles.

Required evidence:

```text
Implementation
    +
Focused automated tests
    +
Relevant regressions
    +
Full automation
    +
Rendered PIE when gameplay-visible
    +
Windows cook
    +
Content/map audit
    +
Progress/report docs
    +
Self-review (fresh-context pass for native code), findings resolved
    +
User-approved Git commit
```

Only then advance the roadmap.

---

# 17. Current Test Baseline

At the completion of Crafting C7:

```text
Focused Crafting C7                                                              9/9
Regression C1-C6B + Stacking + M1 + G4/G5/G6 + G7 + ResourceGathering         95/95
Full permanent suite                                                       169/169
```

At the completion of Crafting C8A (approved):

```text
Focused Crafting C8A                                                            11/11
Crafting regression C1-C8A                                                      59/59
Full permanent suite                                                       180/180
```

C8A Windows cook (editor closed): `Done!`, 0 errors, 2 environment warnings (619 cooked / 7 platform-skipped / 626 total). The new `Station_Workbench` and `Recipe_ShortSword` are not cooked until C8C registers the recipe; the final C8C cook must verify them. No rendered PIE for C8A (no station actor or UI yet; approved without PIE).

At the completion of Crafting C8B (approved):

```text
Focused Crafting C8B                                                              3/3
Crafting C1-C8B                                                                 62/62
Full permanent suite                                                       183/183
```

C8B Windows cook (editor closed): `Done!`, 0 errors, 2 environment warnings (621 cooked / 7 platform-skipped / 628 total); `BP_Workbench` and `Station_Workbench` cooked. One new World Partition external actor package (`C8B_Workbench`). Manual rendered PIE for C8B (performed and reported by the user): 8 PASS, 1 N/A (no door in `Lvl_TopDown`), 0 FAIL, 0 NOT TESTED.

At the completion of Crafting C8C (approved; Phase 4 Crafting complete):

```text
Focused Crafting C8C                                                              7/7
Crafting C1-C8C                                                                 69/69
Full permanent suite                                                       190/190
```

C8C Windows cook (editor closed): `Done!`, 0 errors, 2 environment warnings (622 cooked / 7 platform-skipped / 629 total); `Station_Workbench`, `Recipe_ShortSword`, `BP_Workbench`, `W_Inventory` and `W_CraftingPanel` are all cooked. Content: `BP_Workbench` and `BP_TopDownCharacter` modified; World Partition unchanged. Manual rendered PIE for C8C (performed and reported by the user): 13 PASS, 1 N/A (station destruction, automation-covered), 0 FAIL. The user then found that `Tab` did not close the screen after a child widget took keyboard focus; it is fixed in `UInventoryScreenWidget::NativeOnPreviewKeyDown` with a regression test, and the user retest passed 6/6.

At the completion of Building B1 (approved):

```text
Focused Building B1                                                               8/8
Full permanent suite                                                       198/198
```

B1 clean build: 0 compiler warnings. Windows cook (editor closed): `Done!`, 0 errors (622/7/629, unchanged; B1 adds no assets). No Content, map or World Partition change.

At the completion of Building B2 (approved):

```text
Focused Building B2                                                             20/20
Building B1 (+ OverlapOnlyVolumes regression)                                    9/9
Full permanent suite                                                       219/219
```

B2 clean build (editor closed): 0 compiler warnings. Windows cook: `Done!`, 0 errors, 2 environment warnings (622/7/629, unchanged; B2 adds no assets). No Content, map or World Partition change (`__ExternalActors__` 91, `__ExternalObjects__` 3). No manual PIE required.

At the completion of Building B3 (implemented and validated, awaiting user acceptance):

```text
Focused Building B3                                                              4/4
Building B1-B3                                                                 33/33
Full permanent suite                                                       223/223
```

B3 clean build: 0 compiler warnings (test code only). Windows cook: `Done!`, 0 errors, 2 environment warnings (624/7/631, +2: `Building_WoodFloor` and `BP_BuildFloor`, both in the cooked asset registry). Content: 2 added assets and `BP_TopDownCharacter` modified; no map or World Partition change (`__ExternalActors__` 91, `__ExternalObjects__` 3). No manual PIE required.

Manual rendered PIE for C7 (performed and reported by the user): 12 PASS, 1 NOT TESTED. The click-to-move cancel case could not be run (cursor hidden in game mode) and remains a deferred manual verification; displacement cancel is covered by automation. Final pre-commit Windows cook: `Done!`, 0 errors (619/7/626).

Previous baselines: Crafting C6B full 159/159 (manual rendered PIE 12/12 PASS, user-confirmed); Crafting C6A full 153/153 (manual rendered PIE 12/12 PASS, user-confirmed); Crafting C5 full 147/147; Crafting C3/C4 full 138/138; Crafting C2 full 130/130; Crafting C1 full 125/125; Inventory Stacking full 121/121.

Future work must preserve these unless test totals intentionally increase.

---

# 18. Current Known Environment Notes

## DDC

A writable DDC path has been used previously:

```text
C:\Users\loithienly\AppData\Local\UnrealEngine\Common\DerivedDataCache
```

Some environments have required:

```text
-DDC-ForceMemoryCache
-SkipZenStore
```

These are environment workarounds, not gameplay changes.

## MCP

Expected Unreal MCP endpoint:

```text
http://127.0.0.1:8000/mcp
```

Check:

```powershell
netstat -ano | findstr :8000
```

## Hot reload

If tests behave inconsistently after Hot Reload:

```text
close editor
→ clean build
→ restart editor
→ rerun automation
```

Do not trust duplicate registered automation bodies.

---

# 19. Claude Code Operating Instructions

When Claude starts a task:

```text
1. Read CLAUDE.md.
2. Read this master plan.
3. Read the latest milestone progress/report.
4. Check git status.
5. Audit before modifying.
6. State intended scope.
7. Prefer existing authority.
8. Implement the smallest coherent checkpoint.
9. Validate.
10. Stop before the next milestone.
```

Follow the checkpoint workflow in `AGENTS.md`: stop after a new plan (with its self-critique) for user approval, and stop before every commit with the handoff report defined in `CLAUDE.md`.

When touching Unreal Content:

```text
Use Unreal MCP.
Never binary-edit assets.
Never manually delete external actor packages.
Never Save All.
```

When uncertain whether an actor/file is intentional:

```text
STOP.
Audit.
Ask the user.
```

Do not clean up content merely because its label, transform, or name looks unusual.

---

# 20. Project State Summary for New AI Sessions

Use this abbreviated context when resuming:

```text
Null Tide is a UE5.8 top-down survival colony builder.

Completed:
- Phase 1 Foundation
- Phase 2 Items & Inventory
- G1-G7
- Phase 3 Resource Gathering
- RG1-RG5
- Inventory Stacking
- Crafting C1 Recipe Data Model
- Crafting C2 Inventory Quantity Queries
- Crafting C3/C4 Atomic Craft Transaction
- Crafting C5 Crafting Authority
- Crafting C6A Crafting UI Foundation
- Crafting C6B Craft Execution from UI
- Crafting C7 Timed Crafting (manual PIE 12 PASS, 1 NOT TESTED)

Current crafting:
- UCraftingRecipeDefinition data-only Blueprint recipes, validated by ValidateRecipe
- Recipe_Axe (Wood x3 + Stone x5 → Axe x1), Recipe_WoodArrow (Wood x1 → WoodArrow x5)
- generic inventory queries: GetTotalQuantity, CanRemoveDefinitionQuantity,
  CanAddDefinitionQuantity, CanAddDefinitionQuantities
- UCraftingQueryLibrary: advisory read-only ingredient / output-capacity checks
  (output capacity checked before consumption: conservative)
- UInventoryComponent::TryApplyDefinitionTransaction: atomic remove + add by
  exact definition class, one revision, one event (native only)
- FCraftingTransaction::TryCraftRecipeAtomic: authoritative craft path (native only)
- UCraftingComponent on BP_TopDownCharacter (Recipe_Axe, Recipe_WoodArrow):
  GetAvailableRecipes, CanCraftRecipe (exact), TryCraftRecipe, OnCraftingResult
- C6A crafting UI (read-only): right column of W_Inventory (W_CraftingPanel),
  inventory left, character middle; CRAFT enabled by exact CanCraftRecipe
- C6B: CRAFT calls UCraftingComponent::TryCraftRecipe once; result message from
  OnCraftingResult (kept 3 s); duplicate/reentrant requests ignored
- C7: TryStartCraft (timed recipes run on a world timer and consume at completion),
  CancelCraft; movement > 10 cm / jump cancel; closing UI does not cancel;
  CRAFT becomes CANCEL with a progress bar; Recipe_Pickaxe is timed (3 s)

Current inventory:
- UInventoryComponent is authority
- 24 occupied-slot capacity
- stack Quantity on UItemInstance
- MaxStackSize generally 50 for stackable catalog
- atomic multi-quantity add
- exact ItemId semantics
- whole-stack drop/storage transfer

Current gathering:
- Tree → Wood, Axe, yield 5
- Rock → Stone, Pickaxe, yield 5
- Scrap → Scrap, no tool, yield 4
- finite depletion

Current tools:
- passive Axe/Pickaxe ToolLoadout
- exact ItemId assignments
- context-sensitive Equip/Unequip inventory button

Current input:
- F interact
- I or Tab inventory (inventory + character + crafting screen)
- Enhanced Input via IMC_Default

Current validation baseline:
- Building B3 4/4, B2 20/20, B1 9/9 (8 + OverlapOnlyVolumes); Crafting C8C 7/7 (C8B 3/3, C8A 11/11)
- crafting C1-C8C 69/69
- full 223/223 (219 + 4 Building B3)

Next:
Phase 5 Building: user acceptance and commit of B3, then the B4 plan (building mode, ghost preview, input; must resolve the WorldDynamic ground support surface). Crafting queue, UI-UX pass and Campfire/Forge stay deferred.

Do not start split-stack, quick bar, weight, or unrelated refactors unless explicitly approved.
```

---

# 21. High-Level Roadmap

```text
Phase 1  Foundation                         ✅

Phase 2  Items & Inventory                  ✅
         G1 Catalog                         ✅
         G2 World Presentation              ✅
         G3 Pickups                         ✅
         G4 Inventory UI                    ✅
         G5 Drag/drop semantics             ✅
         G6 Drop to World                   ✅
         G7 Storage                         ✅

Phase 3  Resource Gathering                 ✅
         RG1 Timed Resource Node            ✅
         RG2 Tree/Rock/Scrap                ✅
         RG3 Tool Requirement               ✅
         RG4 Yield/Depletion                ✅
         RG5 Inventory Integration          ✅

         Inventory Stacking                 ✅

Phase 4  Crafting                           ✅
         C1 Recipe Data Model               ✅
         C2 Quantity Queries                ✅
         C3/C4 Atomic Craft Transaction     ✅
                (one transaction: consume + output)
         C5 Crafting Authority              ✅
         C6A Crafting UI Foundation         ✅
         C6B Craft Execution from UI        ✅
         C7 Timed Crafting                  ✅
         C8 Workstations                    ✅
                C8A Station data/authority  ✅
                C8B Workbench actor          ✅
                C8C Station crafting UI      ✅

Phase 5  Building                           ⏭ IN PROGRESS (B0 ✅, B1 ✅, B2 ✅, B3 implemented — awaiting acceptance)

Phase 6  Colony

Phase 7  Save / Load

Phase 8  Combat

Phase 9  Story / Exploration

Phase 10 Demo / Vertical Slice
```

---

# 22. Final Principle

Null Tide should grow by extending stable authority layers instead of replacing them.

Prefer:

```text
static data
→ runtime authority
→ events/snapshots
→ UI presentation
```

over:

```text
Blueprint-specific special cases
→ duplicated state
→ UI-owned gameplay
```

Every future milestone should preserve that direction.
