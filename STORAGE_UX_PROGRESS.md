# G9 — Storage UI/UX: Progress

## Baseline and Scope

- **Baseline:** `f641f5f` (Phase 5 Building complete). Full automation 258/258, cook 648/7/655.
- **Scope:** `STORAGE_UX_PLAN.md`, approved by the user on 2026-10-20 (S-1 to S-9).
  - Shift+LMB moves a stack.
  - Ctrl+LMB moves every stack of an item.
  - Shift+RMB splits.
  - Drag moves.
  - `ALL →` / `← ALL`.
  - Info panel in the middle.
  - Default sort.
  - No `→` / `←` buttons.

## Source

| File | Content |
| --- | --- |
| `Inventory/InventoryTypes.h` | Appended:<br>• `EInventoryOperationResult::CannotSplit`<br>• `EInventoryStorageTransferResult::NothingToMove`<br>• `FInventoryStorageTransferResult::MovedQuantity`<br>• new `FStorageTransferBatchResult` (`Result`, `MovedQuantity`, `MovedStacks`, `StacksLeft`) |
| `Inventory/InventoryStacking.h/.cpp` | `SplitStack`: Q ≥ 2 and a free slot. `floor(Q/2)` goes into a new stack (unique id, staged with `StageNewStacks`) inserted right after the source. |
| `Inventory/InventoryComponent.h/.cpp` | `TrySplitStack` follows the `TryRemoveItemQuantity` pattern: `CanOperate`, Busy guard, legacy projection prepared and rebuilt. Then +1 revision and one broadcast. |
| `Storage/StorageComponent.h/.cpp` | `TrySplitStack`: guard, the shared helper, +1 revision, one broadcast. |
| `Storage/StorageTransferLibrary.h/.cpp` | Moves:<br>• `MoveInventoryStackToStorage` / `MoveStorageStackToInventory`: the exact transfer when the whole stack fits, else `MovePartOfStack` with the largest fit (`FindLargestFit`, a binary search over `BuildAddPlan`).<br>• `MoveAllInventoryToStorage` / `MoveAllStorageToInventory(…, OnlyDefinition)`: under one guard, each stack as much as fits; one notification if anything moved.<br>Shared helpers: `PreflightMove`, `CommitNotifications`.<br>`TransferExactItem` now reports `MovedQuantity`. |
| `UI/StorageSlotWidget.h/.cpp` | `EStorageSlotAction`, pure `ResolveSlotAction`, `HandleSlotInput`, `OnSlotAction`. `NativeOnPreviewMouseButtonDown`: select plus `DetectDragIfPressed`, or a modified action. `NativeOnDragDetected` creates a `UStorageDragDropOperation` with `W_InventoryDragVisual`. `QuantityText` badge; the tooltip is the name. |
| `UI/StorageDragDropOperation.h` (new) | Payload: `ItemId`, `bFromPlayerSide`. |
| `UI/StorageScreenWidget.h/.cpp` | One selection (either side) drives the details panel (`Details*`).<br>`HandleSlotAction`, `HandleDropOnSide`, `NativeOnDrop` (panel geometry).<br>`MoveAllToStorage` / `MoveAllToInventory`.<br>`SortForDisplay` (category → name → quantity → id).<br>`FeedbackText` messages; `HintText` set natively.<br>Selection no longer rebuilds the panels.<br>Slots are owned by the screen.<br>`SetPlayerInventoryOverride` test seam.<br>Removed `TransferToStorageButton` / `TransferToInventoryButton` and their handlers (references checked: only `W_StorageScreen`'s tree). |
| `Tests/StorageUXTests.cpp` (new) | `NullTide.Inventory.G9.*` (8). |

## Assets (Unreal MCP, no manual step)

- **`W_StorageScreen`:**
  - The screen is wrapped as `ScreenDim` (black, 45%, content centred) → `WindowBackground` (dark, padding 20).
  - The header keeps only `CloseButton`, on the right.
  - `PanelsRow` has three columns:
    - `PlayerPanel`: `INVENTORY`, the grid, the count, `ALL →`.
    - `DetailsSize` (280 wide): `DetailsBackground` → `DetailsBox` with `DetailsIcon` (72), `DetailsItemName`, `DetailsCategory`, `DetailsQuantity`, `DetailsDescription`, `DetailsCapabilities`.
    - `StoragePanel`: `StorageTitleText` (moved here), the grid, the count, `← ALL`.
  - `FooterBox`: `FeedbackText`, `HintText`.
  - The grids use explicit wrap 476 and 6 px padding.
  - The old transfer buttons were removed.
- **`W_StorageSlot`:**
  - new `QuantityText` badge (amber, right-aligned);
  - `ItemName` at 11 pt, filling the row;
  - `DragVisualClass` = `W_InventoryDragVisual`.
- Both compiled with warnings as errors and saved by path. No map change.

## Tests `NullTide.Inventory.G9.*` (8)

| Test | Covers |
| --- | --- |
| `MoveStackAsMuchAsFits` | A whole fit keeps the id (30). With a full storage, a partial fit moves 20 into the open stack and leaves 5 under the same id. No fit gives `DestinationFull` with nothing changed. Reverse into a full inventory moves 45. A stale id is refused. One event and one revision per side. |
| `MoveAllOfItem` | 50 + 20 of one item move as 2 stacks; other items stay; one event per side. Again gives `NothingToMove` with no event. Back the other way works. |
| `MoveAll` | Storage with 2 free slots: the first 2 inventory stacks move with their ids, and the rest stays in order (`StacksLeft` 2). Then `DestinationFull` with no event. An empty source gives `NothingToMove`. |
| `SplitStack` | 37 becomes 19 (same id) + 18 (new id, right after). One event. A single item gives `CannotSplit`; a full inventory gives `CapacityFull`; nothing changes. Storage 9 becomes 5 + 4 with one storage event. |
| `SlotInputMapping` | Nine button and modifier cases, including Ctrl winning over Shift and RMB without Shift doing nothing. |
| `ScreenActions` | The real `W_StorageScreen` on test containers:<br>• select shows the quantity;<br>• Shift move gives one event per side and "Moved 30";<br>• split in storage gives "15 + 15";<br>• a single item gives "single";<br>• drop on the same panel does nothing; drop on the other panel moves; a stale payload does nothing;<br>• ALL → shows "Moved", again "Nothing"; ← ALL gives one event per side and "Took";<br>• Ctrl+LMB moves every stack of the item. |
| `DefaultSort` | Production items: Stone, Wood ×9, Wood ×3, Meat, ShortSword, then a misc test item. The container order is unchanged. |
| `ProductionScreen` | `W_StorageScreen` binds all 18 native widgets; the removed buttons are gone; the slot class is `W_StorageSlot`; the slot binds 5 widgets; the drag visual is `W_InventoryDragVisual`. |

## Validation

- **Build:** clean with the editor closed; 0 warnings (first attempt).
- **Tests:**
  - Command line before the assets: G9 authority 6/6, plus G7, Stacking, B6 and B7. Exit 0.
  - In-editor after the assets: G9 **8/8**; full `NullTide` **266/266**, 0 failed.
  - One test carried the environment warning `LogRHICore` (reserved GPU memory over budget, live editor).
- **Content and World Partition audit:**
  - modified `W_StorageScreen` and `W_StorageSlot`;
  - no added asset, no map change;
  - `__ExternalActors__` 94, `__ExternalObjects__` 3.

- After the review fixes, with the editor closed:
  - rebuilt, 0 warnings; command line **266/266**, exit 0;
  - Windows cook `Done!`, 0 errors, 2 environment warnings, 648/7/655 (unchanged).

## Self-Review (fresh-context review pass; no P0 or P1)

| Finding | Resolution |
| --- | --- |
| P2: a fast second click on a slot arrives as a double click (bubbling, no preview pass; `SButton` turns it into a press), so a second Shift+RMB split or a Shift+LMB on the next sorted stack was lost | **CONFIRMED, fixed.** `UStorageSlotWidget::NativeOnMouseButtonDoubleClick` resolves and dispatches like a single click; a plain double click only selects. PIE step 5 covers it. |
| P3: a batch that failed unexpectedly before moving anything reported `DestinationFull` ("… is full") | **CONFIRMED, fixed.** The first non-full failure code is returned. |
| P3: ALL feedback used an empty title and did not mention leftovers | **CONFIRMED, fixed** ("The chest" fallback; "(no room for the rest)"; other failures say "Couldn't move that"). |
| P3: tests called the screen directly (not the slot → screen wiring) and used unguarded `FindItemById(...)->GetQuantity()` | **CONFIRMED, fixed.** `ScreenActions` selects and moves through a real slot's `HandleSlotInput`; quantities go through `G9Qty` (−1 when missing). |
| Speculative: focus may land on a removed slot button after a move | **Fixed as a precaution.** `W_StorageSlot` `ItemButton` `IsFocusable` off; `ProductionScreen` asserts it. |
| P3: each action rebuilds the views twice (one per side's event) | **Recorded** (correct, small cost). |
| P3: the new-stack branch of a partial move cannot run with valid data; partial moves only ever merge, because a free slot would take the whole stack | **Recorded** (only over-limit legacy data would create new stacks there). |
| P3: a split reserves ids in its own container only | **Recorded** (`FGuid::NewGuid` collisions are practically impossible, and a later transfer would refuse safely). |

Confirmed by the reviewer:
- atomicity, `FindLargestFit` correctness and monotonicity;
- a single stacking rule set;
- no over-limit stacks;
- unique ids;
- one notification per side;
- legacy projection parity;
- guarded broadcasts;
- slot lifetime across rebuilds;
- stale-payload handling;
- a strict weak sort order;
- appended enums;
- no unity-build name clashes.

## First PIE (user, 2026-10-20) and Fixes

**User:** everything else done; two findings.
1. **The details icon filled the screen** (screenshot: the Stone icon took the whole middle column and pushed the texts out).
   - **Cause:** `SetBrushFromTexture(..., bMatchSize = true)` replaced the authored 72 × 72 brush with the texture's own size.
   - **Fix:** `false`, so the 72 × 72 size stays.
   - **Also in the screenshot:** the empty inventory column collapsed, and slot names were cut to one letter. Fixed:
     - `PlayerGridSize` / `StorageGridSize` size boxes (width 476, so 4 slots per row; minimum height 312, 4 rows);
     - slot `ItemName` collapsed (the name stays in the tooltip and the details panel);
     - the quantity badge sits right and bottom.
2. **Tab did not close the storage screen** (I and Esc did).
   - **Cause:** the close keys were handled only in `NativeOnKeyDown`, and a focused child (ALL and Close buttons) turns Tab into focus navigation first.
   - **Fix:** `IsCloseKey` (I, Tab, Esc) in `NativeOnPreviewKeyDown` and `NativeOnKeyDown`, as on the inventory screen. The hint now says "Esc / Tab close".

**Tests:** `ProductionScreen` asserts the close keys, and that a real `Item_Stone` icon keeps a 72 × 72 details brush with a texture set.

**Validation after the fixes** (editor closed):
- build clean;
- command line **266/266**, exit 0;
- cook `Done!` 648/7/655, 0 errors.

**PIE retest (user, 2026-10-20): PASS** ("storage thì ok rồi").
- The details icon is 72 × 72; the columns stay sized; Tab closes.
- Every other step passed in the first PIE.
- **Separate finding:** overlapping text in the *inventory* screen's details panel (`W_Inventory`). It predates G9, since G9 did not change that screen. It is handled as G9.1 (user-approved).

## Next Checkpoint

G9 is COMPLETE / APPROVED (automation 266/266, cook PASS, PIE PASS). Next: G9.1, the inventory details layout fix (approved). Then Phase 6 Colony is planned with the user. Then Phase 6 Colony is planned with the user.

## G9.1 - Inventory Screen Details Layout Fix

- **Baseline:** `b07464e` (G9). Approved by the user on 2026-10-20 (plan: `STORAGE_UX_PLAN.md` G9.1).
- **Asset (`W_Inventory`, MCP):**
  - Fonts that were still at the 24 pt UMG default:
    - `ToolActionText`: 12 regular, muted;
    - `ToolActionButtonText`: 14 bold;
    - `AxeSlotLabel` / `PickaxeSlotLabel`: 12 bold;
    - `AxeToolName` / `PickaxeToolName`: 12 regular.
  - New `DetailsScroll` scroll box (slot Fill) holds `DetailsItemDescription`, `CapabilityHeading` and `CapabilitySummary`. Long details scroll instead of overlapping, and the tool panel stays at the bottom.
  - All bound names are kept.
  - Compiled with warnings as errors and saved by path.
- **C++:** `UInventoryScreenWidget::RefreshToolSlot` shows only the item name, or "Empty", under the AXE / PICKAXE label. This removes the "AXE / Axe / Axe" repetition. No test or doc referenced the old format.
- **Test:** `NullTide.Inventory.G9.InventoryDetailsLayout`:
  - six texts below 20 pt;
  - three detail texts inside `DetailsScroll`;
  - its slot fills.
- **Validation** (editor closed):
  - build clean; the first attempt failed on a test format specifier (`%d` for a float font size), fixed to `%.0f`;
  - command line **267/267**, exit 0 (the existing `CraftingUITests` binding check of `W_Inventory` still passes);
  - cook `Done!` 648/7/655, 0 errors;
  - Content: `W_Inventory` modified; `__ExternalActors__` 94, `__ExternalObjects__` 3.
- **Self-review (fresh-context review pass; no P0, P1 or P2):**

  | Finding | Resolution |
  | --- | --- |
  | Anything depending on the old "`<slot>\n<item>`" format | None found (`Source/`, docs); `ToolType` is still used. |
  | P3: no test checks that the tool panel stays outside the scroll area | **CONFIRMED, fixed:** `ToolActionPanel`'s parent is `DetailsScroll`'s parent, not the scroll box. Rebuilt; **267/267**, exit 0. |
  | P3: the `RefreshToolSlot` text change itself is not covered by automation | **Recorded:** it would need a test seam on the tool loadout refresh; the change is covered by the manual PIE (Character column). |
- **Manual rendered PIE (user, 2026-10-20): PASS** ("ok done").
  - Short Sword details no longer overlap; Axe and Pickaxe show the tool panel at the bottom.
  - The Character column shows only the equipped name or "Empty".
- **G9.1 is COMPLETE / APPROVED.** Next: Phase 6 Colony, to be planned with the user.
