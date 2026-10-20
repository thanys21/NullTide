# G9 — Storage UI/UX: Plan

**Status:** COMPLETE / APPROVED (user, 2026-10-20: S-1 to S-9 as written below; PIE PASS after the first-PIE fixes in `STORAGE_UX_PROGRESS.md`).
- **Baseline:** `f641f5f` (Phase 5 Building complete). Full automation 258/258.
- **Request (user, 2026-10-20), before Phase 6:**
  1. UI/UX for storage.
  2. Shift + LMB moves to the chest and back.
  3. Ctrl + LMB moves the whole stack to the chest and back.
  4. Shift + RMB splits a stack 50%.
  5. An "ALL" button on both sides.
- **Clarifications (user, 2026-10-20):**
  - LMB selects and shows the item's information in the middle.
  - Shift + LMB moves that stack.
  - Ctrl + LMB moves every stack of that item.
  - No `→` / `←` buttons.
  - Drag and drop also transfers.
  - Items are sorted by default; the player cannot arrange them manually yet.
  - Later, loot chests that spawn items must work the same way.
- **Approval scope:** the request explicitly approves split-stack and partial transfer, which `NULLTIDE_MASTER_PLAN.md` deferred (`:1841`; "Partial Storage Transfer … Must remain atomic").

## 1. Audit (source, 2026-10-20)

| Fact | Source | Consequence |
| --- | --- | --- |
| Transfers move whole stacks only. The destination is planned with `FInventoryStacking::BuildAddPlan` (merges first, then new stacks), and a no-fit gives `DestinationFull`. The source `UItemInstance` moves (rename) unless it fully merges. | `Storage/StorageTransferLibrary.cpp:30-125` (plan at `:66-71`) | "As much as fits" and the batches are new authority code in the same library, the only friend of both containers. |
| Each successful transfer bumps both revisions once and broadcasts `OnInventoryChanged` and `OnStorageChanged` once. Failures change nothing. | `StorageTransferLibrary.cpp:158-166, 199-207` | The batches (Ctrl, ALL) keep one bump and one broadcast per side. |
| Storage has no mutator besides the transfer; capacity is 24 stacks, the same as the inventory. | `Storage/StorageComponent.h:23-65`; `Inventory/InventoryComponent.h:160-164` | Split needs a mutator on each container. |
| No split exists. New stacks come only from `FInventoryStacking::StageNewStacks` (unique `FGuid`, `NewObject`, `Initialize`). | `Inventory/InventoryStacking.cpp:177-215` | Split reuses `StageNewStacks`. |
| `UItemInstance::SetQuantity` rejects values below 1, but not above `MaxStackSize`. | `Items/ItemInstance.cpp:23-31` | Split and partial code only lower a source or use planned quantities. |
| `BuildAddPlanForEntries` is the only stacking and capacity rule set. | `InventoryStacking.cpp:60-110`; `CLAUDE.md` | "As much as fits" uses a search over that planner, not a second formula. |
| The storage screen selects with `UButton::OnClicked` only. It has no mouse overrides, modifiers, drag or transfer-all. It rebuilds all slots on each change event. | `UI/StorageScreenWidget.cpp:112-243`; `UI/StorageSlotWidget.cpp:11-46` | Modifier clicks and drag go into the slot's `NativeOnPreviewMouseButtonDown`. |
| `UInventorySlotWidget` already selects and starts a drag in `NativeOnPreviewMouseButtonDown` (`DetectDragIfPressed`, left button). Its drag payload is tied to one inventory. | `UI/InventorySlotWidget.cpp:83-91`; `UI/InventoryDragDropOperation.h:27-42` | The same pattern; storage gets its own small payload type (side + `ItemId`). |
| The inventory screen already has a details panel (`DetailsIcon`, `DetailsItemName`, `DetailsItemDescription`, `DetailsCategory`) built from `UInventoryUIFunctionLibrary` (`ResolveCategory`, `GetCategoryDisplayName`, `BuildCapabilitySummary`). | `UI/InventoryScreenWidget.h:106-115, 187-188`; `UI/InventoryUICategory.h` | The middle panel reuses the same read-only helpers. |
| Item display data: `ItemName`, `ItemDescription`, icon; categories are `Resource, Consumable, Weapon, Ammo, Misc`. | `Items/ItemDefinition.h:21-27`; `InventoryUICategory.h` | Basis for the default sort. |
| The legacy inventory mode exists (`IsLegacyInventoryMode`); transfers prepare a legacy projection. | `InventoryComponent.h:109, 170`; `StorageTransferLibrary.cpp:22-28` | The new operations follow the same guard and projection rules. |
| No test references the storage screen or slot widgets. | `Tests/` | G9 adds the first storage UI tests. |

## 2. Design (recommended)

### 2.1 Authority (native, atomic, generic; in `UStorageTransferLibrary` unless stated)

**Move one stack, as much as fits** (Shift + LMB, drag and drop): `TransferInventoryStackToStorage` / `TransferStorageStackToInventory(…, ItemId)`.
- The whole stack fits: exactly today's whole-stack transfer (the identity moves).
- Only part fits: it moves the largest N that fits, found by a binary search over `BuildAddPlanForEntries`.
  - The source keeps its `ItemId` with N fewer.
  - The destination receives the planned merges and new stacks (new ids).
- Nothing fits: `DestinationFull`, and nothing changes.
- One revision bump and one broadcast per side.

**Move every stack of one item, or everything** (Ctrl + LMB, ALL): `TransferAllInventoryToStorage` / `TransferAllStorageToInventory(…, OnlyDefinition = null)`.
- Under one mutation guard, it visits the source stacks in container order and moves as much of each as fits, with the same rules as above. With `OnlyDefinition` set, only stacks of exactly that class are visited.
- Leftovers stay.
- Result: `FStorageTransferBatchResult` {moved units, moved stacks, stacks left}.
- One bump and one broadcast per side if anything moved; if nothing moved, `DestinationFull` (or `NothingToMove`) and no event.

**Split in place** (Shift + RMB): `UInventoryComponent::TrySplitStack(ItemId)` and `UStorageComponent::TrySplitStack(ItemId)` share one `FInventoryStacking::SplitStack` helper.
- **Requirements:** quantity 2 or more and a free slot. Otherwise nothing happens (`CannotSplit` / full), as the user decided.
- **Result:** the new stack has `floor(Q/2)` and a new `ItemId` and sits right after the source; the source keeps `ceil(Q/2)` and its id.
- One bump and one broadcast.

**Result codes:** appended only, never reordered: `EInventoryStorageTransferResult::NothingToMove`, and a split code (`CannotSplit`) on the operation result.

**Generic:** nothing in the authority knows about the screen, building or crafting. A future loot chest is just another `UStorageComponent` filled by its own (future) authority, and it gets the same screen and operations.

### 2.2 UI (`UStorageScreenWidget`, `UStorageSlotWidget`; Blueprints `W_StorageScreen`, `W_StorageSlot`)

**Layout (three columns):**
- **Left, Inventory:** header `Inventory 12 / 24`, a grid of slots, footer button `ALL →`.
- **Middle, item info:** icon, name, category, quantity ("× 37"), description and capability summary for the selected stack; empty with a hint when nothing is selected.
- **Right, chest:** header with the chest's title and `5 / 24`, a grid, footer button `← ALL`.
- **Bottom bar:**
  - controls hint: `LMB info · Shift+LMB move stack · Ctrl+LMB move all of this item · Shift+RMB split · drag to move · Esc close`;
  - a feedback line: "Moved 12 Wood", "Moved 3 stacks", "Chest is full", "No room to split".
- The `→` / `←` selected-transfer buttons are removed (user decision S-5).
- **Slot:** icon, quantity badge, selection border; the name in a tooltip.

**Input on a slot (both panels):**

| Input | Action |
| --- | --- |
| LMB | Select and show it in the middle (drag can start) |
| Shift + LMB | Move this stack to the other side (as much as fits) |
| Ctrl + LMB | Move every stack of this item to the other side (as much as fits) |
| Shift + RMB | Split 50% in place (nothing if there is no free slot) |
| Drag a slot onto the other panel | Same as Shift + LMB |
| Drop on the same panel | Nothing (no manual arrangement) |

- The slot's `NativeOnPreviewMouseButtonDown`:
  - resolves the action with a pure, tested `ResolveSlotAction(Button, Shift, Ctrl)`;
  - for plain LMB it selects and calls `DetectDragIfPressed` (the inventory-slot pattern);
  - for a modified click it broadcasts `OnSlotAction(ItemId, bPlayerSide, Action)` and handles the event.
- **Drag:** a small `UStorageDragDropOperation` (source side + `ItemId`) with the existing drag visual. The screen's `NativeOnDrop` resolves the target panel under the cursor and calls the move once.
- The screen calls the authority exactly once per action and shows the result in the feedback line. Widgets never mutate items.

**Default sort (display only):**
- Order: category (`Resource, Consumable, Weapon, Ammo, Misc`), then name, then quantity (largest first), then `ItemId` for a stable order.
- The containers' own order is unchanged, so the G7 order contracts and the tests stay valid, and the player cannot rearrange.
- After a split, the larger half shows first.

### 2.3 Assets (MCP)

- **`W_StorageScreen` relayout** (three columns plus the bottom bar):
  - keeps every name the native class binds;
  - removes `TransferToStorageButton` / `TransferToInventoryButton` (the bindings become unused and are deleted from the C++ after a reference check);
  - adds `TransferAllToStorageButton`, `TransferAllToInventoryButton`, the `Details*` widgets, `HintText` and `FeedbackText`.
- **`W_StorageSlot` restyle:** icon, quantity badge, selection border.
- All compiled with warnings as errors. No map change.

## 3. Decisions

**Answered (user, 2026-10-20):**
- **S-1** LMB selects and shows the info in the middle. Shift + LMB moves the stack.
- **Ctrl + LMB** moves every stack of that item.
- **S-2** Moves as much as fits; the rest stays. Approved.
- **S-3** Split in place; with no free slot, no split. Approved.
- **S-4** ALL moves everything that fits; the rest stays. Approved.
- **S-5** No `→` / `←` buttons. Drag and drop also moves.
- **S-6** The storage screen only, kept generic so loot chests reuse it.
- **Sorting** is automatic; there is no manual arrangement.

**Proposed details (part of the final approval):**
- **S-7 Sort key:** category → name → quantity (largest first). The sort is display-only.
- **S-8 Drag target:** the whole other panel accepts a drop. Dropping on the same panel does nothing.
- **S-9 Middle panel:** the same content as the inventory's details panel (icon, name, category, description, capability summary) plus the quantity.

## 4. Failure, Reentrancy, Lifetime

- Every authority call validates first and commits last. Failures change no revision and fire no event, the G7 contract. The batches commit once.
- **Busy guards:** both sides' mutation guards block nested calls (`Busy`).
- **Rebuild during a click or drop:**
  - The handler's transfer fires events that rebuild the panels and remove the slot that is handling the input. The current `OnClicked` path already does this (G7, PIE-proven).
  - Handlers touch no member after the authority call returns; the screen copies what it needs first.
- **Drag payload:** a drop re-checks that the item is still on the source side (by `ItemId`). A stale payload does nothing.
- **Selection:** the middle panel follows the selected `ItemId`. If that stack leaves (moved or fully merged), the selection clears. After a split, the source stays selected.
- **Destroyed chest mid-action:** `NativeTick` already closes the screen; the authority checks `CanOperate()`.
- **Legacy inventory mode:** the new operations follow the existing projection and guard rules, or refuse in legacy-authoritative mode (confirmed at implementation; the production player uses the native store).

## 5. Tests (`NullTide.Inventory.G9.*`, planned 8)

- **`MoveStackAsMuchAsFits`:**
  - a whole fit keeps the identity;
  - a partial fit leaves the source id with the remainder, merges first, then adds new ids;
  - no fit is atomic;
  - one event per side.
- **`MoveAllOfItem`:** only stacks of that exact class move, in order, as much as fits; other items stay; one event per side.
- **`MoveAll`:** everything that fits moves; leftovers stay; one event per side; nothing to move means no event.
- **`SplitStack`:**
  - inventory and storage: floor/ceil, a unique new id right after the source;
  - quantity 1 and a full container are refused with nothing changed;
  - one event.
- **`SlotInputMapping`:** `ResolveSlotAction` for every combination (LMB, Shift+LMB, Ctrl+LMB, Shift+RMB; RMB alone and Ctrl+RMB do nothing).
- **`ScreenActions`:** a real `W_StorageScreen` on a test inventory and storage.
  - Each slot action and each ALL button calls the authority once with the right side, and the feedback text is set.
  - A drop on the other panel moves; a drop on the same panel does nothing; a stale payload does nothing.
- **`DefaultSort`:** the displayed slot order follows category → name → quantity, while the container order is unchanged.
- **`ProductionScreen`:** `W_StorageScreen` binds every native widget (including the new ones; the removed buttons are gone); its slot class is set; the slot binds its widgets.

Expected suite: 258 + 8 = 266. The G7, Stacking, B6 and B7 storage tests stay green.

## 6. PIE

1. Open a chest: three columns (inventory, info, chest), counts, the hint and the ALL buttons. Items are sorted.
2. LMB shows the item in the middle; Shift + LMB moves that stack each way; when the other side is nearly full, it moves only what fits.
3. Ctrl + LMB on Wood moves every Wood stack.
4. Dragging a slot to the other panel moves it; dropping on the same panel does nothing.
5. Shift + RMB splits; with no free slot, nothing happens and a message shows.
6. `ALL →` and `← ALL` move everything that fits.
7. Close (Esc, I, Close); the first click afterwards reaches the game.

## 7. Self-Critique (open risks)

- **R-1 Shift + RMB** reaches the slot's preview handler even if `UButton` handles the right button; the preview pass runs first. PIE step 5 confirms it.
- **R-2 Drag versus click:** plain LMB both selects and may start a drag (the inventory-slot pattern). A click without movement only selects. PIE step 4 covers this.
- **R-3 Partial-move identity:** a partial move gives the moved part new ids or merges it; only a full move keeps the id, as in G7.
- **R-4 "As much as fits" by search** costs log2(Q) planner calls per stack, at most 24 stacks. That is negligible.
- **R-5 Removing `TransferToStorageButton` / `TransferToInventoryButton`** removes `BindWidgetOptional` properties. References are checked (C++, `W_StorageScreen`) before deletion.
- **R-6 MCP UMG authoring of the relayout** (three columns, grids, anchors): if a property cannot be set, STOP and ask for a manual step.
- **R-7 Legacy inventory mode** semantics are checked at implementation. If they cannot be supported, refuse and STOP to report.

---

# G9.1 — Inventory Screen Details Layout Fix

**Status:** COMPLETE / APPROVED (user, 2026-10-20; PIE PASS). G9 is committed as `b07464e`.

**Finding (user PIE, screenshot, 2026-10-20):** on the inventory screen (`W_Inventory`, which G9 did not change), selecting the Short Sword overlaps the tool hint and the Equip Tool button onto the capability list. The Character column shows "AXE / Axe / Axe".

**Audit (MCP read of `W_Inventory`; source):**
- The window is fixed at 760 high (`WindowSize`).
- `DetailsPanel` is a vertical box: icon, name, category, description, capability heading and summary (5 lines for a weapon), the collapsed `FutureActionArea`, and `ToolActionPanel`.
- `ToolActionText`, `ToolActionButtonText`, `AxeSlotLabel`, `PickaxeSlotLabel`, `AxeToolName` and `PickaxeToolName` still use the UMG default font, **24 pt bold**. The content overflows the fixed height, so the last rows draw over the capability list.
- `UInventoryScreenWidget::RefreshToolSlot` writes "`<slot>\n<item>`" (`InventoryScreenWidget.cpp:485-488`) under a label that already names the slot. Hence "AXE / Axe / Axe".

**Fix:**
1. **Fonts:**
   - `ToolActionText`: 12 regular, muted;
   - `ToolActionButtonText`: 14 bold;
   - `AxeSlotLabel` / `PickaxeSlotLabel`: 12 bold;
   - `AxeToolName` / `PickaxeToolName`: 12 regular.
2. **Scroll area:** the description, capability heading and capability summary move into a new `DetailsScroll` scroll box that fills the remaining height. Long text scrolls instead of overlapping; the tool panel stays at the bottom.
3. **C++:** `RefreshToolSlot` shows only the item name, or "Empty". All bound widget names stay.
4. **Test:** `NullTide.Inventory.G9.InventoryDetailsLayout`:
   - the six texts are below 20 pt;
   - the three detail texts sit in a `UScrollBox`;
   - `DetailsScroll`'s slot fills.

**Not in scope:** any other inventory-screen change.
**Risk:** MCP moves keep the widgets' bindings (checked by the existing `CraftingUITests` binding test and the new test).
