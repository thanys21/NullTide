# Null Tide — Master Project Plan

> **Purpose:** Single source of truth for Claude Code / Codex / human development.
>
> **Project:** Null Tide  
> **Engine:** Unreal Engine 5.8.2  
> **Genre:** Top-down Survival Colony Builder  
> **Core pitch:** Project Zomboid + Core Keeper, starting from one survivor and growing toward a colony.  
> **Primary platform:** PC / Steam  
> **Current development state:** Crafting C2 (Inventory Quantity Queries) complete, awaiting review. Crafting C3 (Atomic Ingredient Consumption) is next.

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
- OpenAI Codex
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

These rules apply to Claude Code, Codex, and human edits.

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
8. Git commit
```

Do not fabricate manual PIE observations.

Automation may prove hidden semantics such as:

- exact ItemId identity;
- inventory revisions;
- atomic mutations;
- timer/state behavior;
- stale-slot clearing.

Manual PIE should record only visible behavior actually observed.

## Git discipline

After every completed milestone:

```bash
git status
git add .
git commit -m "<milestone message>"
git push
```

Do not begin the next major milestone before the previous clean checkpoint is committed.

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

## Multi-agent rule

Claude and Codex may both be used, but:

```text
ONLY ONE AGENT OWNS WRITES AT A TIME.
```

Example:

```text
Claude → implementation
Codex  → audit/review

or

Codex  → implementation
Claude → audit/review
```

Never allow both to modify Unreal assets concurrently.

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
I → IA_OpeningInventory
   → IMC_Default
   → BP_TopDownController
   → W_Inventory
```

`Tab` does not open inventory.

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
Crafting C2 Quantity Queries        ✅ (awaiting review)

NEXT:
Crafting C3 Atomic Ingredient Consumption
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

## C3 — Atomic Ingredient Consumption

Crafting must never consume partial ingredients.

Required contract:

```text
preflight all ingredients
→ preflight output capacity
→ commit ingredient consumption
→ commit outputs
```

If any stage cannot complete:

```text
inventory remains unchanged
```

Ingredient removal should deterministically consume stacks in inventory order unless another explicit rule is adopted.

Example:

```text
Need Wood x12

Wood stacks:
5
10

Result:
first stack removed
second stack becomes 3
```

All inventory revisions/events should represent one coherent operation if architecture supports it.

---

## C4 — Craft Output

Outputs enter the normal inventory stacking authority.

Example:

```text
recipe → WoodArrow x20
```

should:

```text
merge partial arrow stack
→ overflow to a new stack if needed
```

Output-capacity preflight must prevent ingredient loss.

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

## Crafting — C3 Atomic Ingredient Consumption

C2 is complete and awaiting review (see `CRAFTING_PROGRESS.md`). Start C3 only after C2 is approved.

Claude/Codex instruction:

```text
1. Read CRAFTING_PLAN.md and CRAFTING_PROGRESS.md, especially the
   C2 "Transaction-Design Issues for C3/C4".
2. Agree the C3 scope with the user and record it in CRAFTING_PLAN.md
   (C3 alone, or C3+C4 as one transaction).
3. Keep UInventoryComponent generic: remove-by-definition and batch
   commits take definitions and quantities, never recipes.
4. Consume stacks in inventory order inside one mutation guard,
   one revision, one event; re-run preflight inside the guard.
5. Do NOT build crafting UI yet (C6).
6. Build + focused tests + regression + full suite + cook.
7. Update CRAFTING_PROGRESS.md and this plan.
8. STOP for review.
```

This keeps Crafting incremental and prevents inventory authority from becoming coupled to UI.

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
Manual Git commit
```

Only then advance the roadmap.

---

# 17. Current Test Baseline

At the completion of Crafting C2:

```text
Focused Crafting C2                       5/5
Regression C1 + Stacking + M1           20/20
Full permanent suite                  130/130
```

Previous baselines: Crafting C1 full 125/125; Inventory Stacking full 121/121.

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
- Crafting C2 Inventory Quantity Queries (awaiting review)

Current crafting:
- UCraftingRecipeDefinition data-only Blueprint recipes, validated by ValidateRecipe
- Recipe_Axe (Wood x3 + Stone x5 → Axe x1), Recipe_WoodArrow (Wood x1 → WoodArrow x5)
- generic inventory queries: GetTotalQuantity, CanRemoveDefinitionQuantity,
  CanAddDefinitionQuantity, CanAddDefinitionQuantities
- UCraftingQueryLibrary: read-only ingredient / output-capacity checks
  (output capacity checked before consumption: conservative)
- no consumption, authority or UI yet

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
- I inventory
- Enhanced Input via IMC_Default

Current validation baseline:
- Crafting C2 5/5
- regression C1 + Stacking + M1 20/20
- full 130/130

Next:
Crafting C3 Atomic Ingredient Consumption (after C2 review).

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

Phase 4  Crafting                           ⏭ IN PROGRESS
         C1 Recipe Data Model               ✅
         C2 Quantity Queries                ✅ (awaiting review)
         C3 Atomic Ingredient Consumption   ⏭ NEXT
         C4 Outputs
         C5 Crafting Authority
         C6 Minimal Crafting UI
         C7 Timed Crafting
         C8 Workstations

Phase 5  Building

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
