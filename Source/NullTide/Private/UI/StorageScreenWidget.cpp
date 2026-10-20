#include "UI/StorageScreenWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "Storage/StorageComponent.h"
#include "Storage/StorageTransferLibrary.h"
#include "UI/GameInputMode.h"
#include "UI/InventoryUICategory.h"
#include "UI/StorageDragDropOperation.h"
#include "UI/StorageSlotWidget.h"

#define LOCTEXT_NAMESPACE "StorageScreen"

namespace
{
FText ItemDisplayName(const UItemInstance* Item)
{
	const UItemDefinition* Definition = Item ? UInventoryUIFunctionLibrary::GetDefinition(Item->GetDefinitionClass()) : nullptr;
	return Definition ? Definition->ItemName : LOCTEXT("UnknownItem", "item");
}
}

void UStorageScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (TransferAllToStorageButton)
	{
		TransferAllToStorageButton->OnClicked.AddUniqueDynamic(this, &UStorageScreenWidget::MoveAllToStorage);
	}
	if (TransferAllToInventoryButton)
	{
		TransferAllToInventoryButton->OnClicked.AddUniqueDynamic(this, &UStorageScreenWidget::MoveAllToInventory);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UStorageScreenWidget::CloseStorage);
	}
	if (HintText)
	{
		HintText->SetText(LOCTEXT("Hint",
			"LMB info  ·  Shift+LMB move stack  ·  Ctrl+LMB move all of this item  ·  Shift+RMB split  ·  drag to move  ·  Esc / Tab close"));
	}
}

void UStorageScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	RebindPlayerInventory();
	if (ActiveStorage)
	{
		ActiveStorage->OnStorageChanged.AddUniqueDynamic(this, &UStorageScreenWidget::HandleStorageChanged);
	}
	RebuildViews();
	SetKeyboardFocus();
}

void UStorageScreenWidget::NativeDestruct()
{
	ReleaseBindings();
	SelectedItemId.Invalidate();
	Super::NativeDestruct();
}

void UStorageScreenWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IsValid(ActiveStorage))
	{
		CloseStorage();
		return;
	}
	if (ResolvePlayerInventory() != PlayerInventory)
	{
		RebindPlayerInventory();
		RebuildViews();
	}
}

bool UStorageScreenWidget::IsCloseKey(const FKey& Key)
{
	return Key == EKeys::Escape || Key == EKeys::I || Key == EKeys::Tab;
}

FReply UStorageScreenWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// As on the inventory screen: Slate turns Tab into focus navigation on a focusable child (the ALL and Close
	// buttons) before the key bubbles back here, so the close keys are taken in the preview pass (G9 PIE).
	if (IsCloseKey(InKeyEvent.GetKey()))
	{
		CloseStorage();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UStorageScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (IsCloseKey(InKeyEvent.GetKey()))
	{
		CloseStorage();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool UStorageScreenWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	const UStorageDragDropOperation* Operation = Cast<UStorageDragDropOperation>(InOperation);
	if (!Operation)
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}
	const FVector2D ScreenPosition = InDragDropEvent.GetScreenSpacePosition();
	auto IsOver = [&ScreenPosition](const UWidget* Zone)
	{
		return Zone && Zone->GetCachedGeometry().IsUnderLocation(ScreenPosition);
	};
	const UWidget* PlayerZone = PlayerPanel ? PlayerPanel.Get() : PlayerItemsWrapBox.Get();
	const UWidget* StorageZone = StoragePanel ? StoragePanel.Get() : StorageItemsWrapBox.Get();
	if (IsOver(PlayerZone))
	{
		HandleDropOnSide(Operation, true);
	}
	else if (IsOver(StorageZone))
	{
		HandleDropOnSide(Operation, false);
	}
	// A storage drag ends here either way; there is no other drop target on this screen.
	return true;
}

void UStorageScreenWidget::OpenForStorage(UStorageComponent* Storage, const FText& InTitle)
{
	if (!IsValid(Storage))
	{
		CloseStorage();
		return;
	}
	if (ActiveStorage != Storage)
	{
		if (ActiveStorage)
		{
			ActiveStorage->OnStorageChanged.RemoveDynamic(this, &UStorageScreenWidget::HandleStorageChanged);
		}
		ActiveStorage = Storage;
		ActiveStorage->OnStorageChanged.AddUniqueDynamic(this, &UStorageScreenWidget::HandleStorageChanged);
		SelectedItemId.Invalidate();
	}
	StorageTitle = InTitle;
	SetFeedback(FText::GetEmpty());
	RebindPlayerInventory();
	RebuildViews();
	SetKeyboardFocus();
}

void UStorageScreenWidget::CloseStorage()
{
	ReleaseBindings();
	SelectedItemId.Invalidate();
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetShowMouseCursor(false);
		// The first click after closing reaches the game (B9).
		NullTideInput::RestoreGameOnlyInput(*PlayerController);
	}
	RemoveFromParent();
}

void UStorageScreenWidget::SetPlayerInventoryOverride(UInventoryComponent* Inventory)
{
	PlayerInventoryOverride = Inventory;
	RebindPlayerInventory();
	RebuildViews();
}

void UStorageScreenWidget::HandlePlayerInventoryChanged(int32 NewRevision)
{
	RebuildViews();
}

void UStorageScreenWidget::HandleStorageChanged(int32 NewRevision)
{
	RebuildViews();
}

void UStorageScreenWidget::HandleSlotSelected(const FGuid ItemId, const bool bPlayerSide)
{
	if (!FindSideItem(bPlayerSide, ItemId))
	{
		return;
	}
	SelectedItemId = ItemId;
	bSelectedPlayerSide = bPlayerSide;
	// Only restyle: the clicked slot stays in its panel, so a drag can start from it.
	RefreshSelectionVisuals();
	RefreshDetails();
}

void UStorageScreenWidget::HandleSlotActionRequested(const FGuid ItemId, const bool bPlayerSide, const EStorageSlotAction Action)
{
	HandleSlotAction(ItemId, bPlayerSide, Action);
}

bool UStorageScreenWidget::HandleSlotAction(const FGuid ItemId, const bool bPlayerSide, const EStorageSlotAction Action)
{
	UItemInstance* Item = FindSideItem(bPlayerSide, ItemId);
	if (!PlayerInventory || !ActiveStorage || !Item)
	{
		return false;
	}
	if (Action == EStorageSlotAction::Select)
	{
		HandleSlotSelected(ItemId, bPlayerSide);
		return false;
	}
	// Copied first: the authority's events rebuild the panels, and the item may move or merge away.
	const FText Name = ItemDisplayName(Item);
	const int32 Quantity = Item->GetQuantity();
	const TSubclassOf<UItemDefinition> DefinitionClass = Item->GetDefinitionClass();

	switch (Action)
	{
	case EStorageSlotAction::MoveStack:
	{
		const FInventoryStorageTransferResult Result = bPlayerSide
			? UStorageTransferLibrary::MoveInventoryStackToStorage(PlayerInventory, ActiveStorage, ItemId)
			: UStorageTransferLibrary::MoveStorageStackToInventory(ActiveStorage, PlayerInventory, ItemId);
		if (Result.IsSuccess())
		{
			SetFeedback(Result.MovedQuantity < Quantity
				? FText::Format(LOCTEXT("MovedPart", "Moved {0} {1} (no room for the rest)"), FText::AsNumber(Result.MovedQuantity), Name)
				: FText::Format(LOCTEXT("Moved", "Moved {0} {1}"), FText::AsNumber(Result.MovedQuantity), Name));
		}
		else
		{
			SetFeedback(Result.Result == EInventoryStorageTransferResult::DestinationFull ? DescribeFull(!bPlayerSide)
				: LOCTEXT("MoveFailed", "Couldn't move that"));
		}
		return Result.IsSuccess();
	}
	case EStorageSlotAction::MoveAllOfItem:
	{
		const FStorageTransferBatchResult Result = bPlayerSide
			? UStorageTransferLibrary::MoveAllInventoryToStorage(PlayerInventory, ActiveStorage, DefinitionClass)
			: UStorageTransferLibrary::MoveAllStorageToInventory(ActiveStorage, PlayerInventory, DefinitionClass);
		if (Result.IsSuccess())
		{
			SetFeedback(Result.StacksLeft > 0
				? FText::Format(LOCTEXT("MovedAllOfPart", "Moved {0} {1} (no room for the rest)"), FText::AsNumber(Result.MovedQuantity), Name)
				: FText::Format(LOCTEXT("MovedAllOf", "Moved {0} {1}"), FText::AsNumber(Result.MovedQuantity), Name));
		}
		else
		{
			SetFeedback(Result.Result == EInventoryStorageTransferResult::DestinationFull ? DescribeFull(!bPlayerSide)
				: LOCTEXT("MoveFailed", "Couldn't move that"));
		}
		return Result.IsSuccess();
	}
	case EStorageSlotAction::Split:
	{
		const FInventoryOperationResult Result = bPlayerSide
			? PlayerInventory->TrySplitStack(ItemId)
			: ActiveStorage->TrySplitStack(ItemId);
		switch (Result.Result)
		{
		case EInventoryOperationResult::Success:
			SetFeedback(FText::Format(LOCTEXT("Split", "Split {0}: {1} + {2}"), Name,
				FText::AsNumber(Quantity - Result.Quantity), FText::AsNumber(Result.Quantity)));
			break;
		case EInventoryOperationResult::CannotSplit:
			SetFeedback(LOCTEXT("CannotSplit", "Can't split a single item"));
			break;
		case EInventoryOperationResult::CapacityFull:
			SetFeedback(LOCTEXT("NoRoomToSplit", "No free slot to split"));
			break;
		default:
			SetFeedback(LOCTEXT("SplitFailed", "Couldn't split that"));
			break;
		}
		return Result.IsSuccess();
	}
	default:
		return false;
	}
}

bool UStorageScreenWidget::HandleDropOnSide(const UStorageDragDropOperation* Operation, const bool bTargetPlayerSide)
{
	// The same side does nothing (no manual arrangement); a stale id fails the lookup inside.
	if (!Operation || Operation->bFromPlayerSide == bTargetPlayerSide)
	{
		return false;
	}
	return HandleSlotAction(Operation->ItemId, Operation->bFromPlayerSide, EStorageSlotAction::MoveStack);
}

void UStorageScreenWidget::MoveAllToStorage()
{
	if (!PlayerInventory || !ActiveStorage)
	{
		return;
	}
	const FStorageTransferBatchResult Result = UStorageTransferLibrary::MoveAllInventoryToStorage(PlayerInventory, ActiveStorage);
	SetFeedback(Result.IsSuccess()
		? FText::Format(Result.StacksLeft > 0 ? LOCTEXT("MovedSomeToStorage", "Moved {0} items to {1} (no room for the rest)")
			: LOCTEXT("MovedAllToStorage", "Moved {0} items to {1}"), FText::AsNumber(Result.MovedQuantity), GetStorageDisplayTitle())
		: Result.Result == EInventoryStorageTransferResult::NothingToMove ? LOCTEXT("NothingToStore", "Nothing to move")
		: Result.Result == EInventoryStorageTransferResult::DestinationFull ? DescribeFull(false)
		: LOCTEXT("MoveFailed", "Couldn't move that"));
}

void UStorageScreenWidget::MoveAllToInventory()
{
	if (!PlayerInventory || !ActiveStorage)
	{
		return;
	}
	const FStorageTransferBatchResult Result = UStorageTransferLibrary::MoveAllStorageToInventory(ActiveStorage, PlayerInventory);
	SetFeedback(Result.IsSuccess()
		? FText::Format(Result.StacksLeft > 0 ? LOCTEXT("TookSome", "Took {0} items (no room for the rest)")
			: LOCTEXT("MovedAllToInventory", "Took {0} items"), FText::AsNumber(Result.MovedQuantity))
		: Result.Result == EInventoryStorageTransferResult::NothingToMove ? LOCTEXT("NothingToTake", "Nothing to take")
		: Result.Result == EInventoryStorageTransferResult::DestinationFull ? DescribeFull(true)
		: LOCTEXT("MoveFailed", "Couldn't move that"));
}

void UStorageScreenWidget::SortForDisplay(TArray<UItemInstance*>& Items)
{
	Items.RemoveAll([](const UItemInstance* Item) { return !IsValid(Item); });
	Items.StableSort([](const UItemInstance& A, const UItemInstance& B)
	{
		const EInventoryCategory CategoryA = UInventoryUIFunctionLibrary::ResolveItemCategory(&A);
		const EInventoryCategory CategoryB = UInventoryUIFunctionLibrary::ResolveItemCategory(&B);
		if (CategoryA != CategoryB)
		{
			return static_cast<uint8>(CategoryA) < static_cast<uint8>(CategoryB);
		}
		const int32 NameOrder = ItemDisplayName(&A).ToString().Compare(ItemDisplayName(&B).ToString(), ESearchCase::IgnoreCase);
		if (NameOrder != 0)
		{
			return NameOrder < 0;
		}
		if (A.GetQuantity() != B.GetQuantity())
		{
			return A.GetQuantity() > B.GetQuantity();
		}
		return A.GetInstanceId() < B.GetInstanceId();
	});
}

FText UStorageScreenWidget::GetFeedbackText() const
{
	return FeedbackText ? FeedbackText->GetText() : FText::GetEmpty();
}

FText UStorageScreenWidget::GetDetailsNameText() const
{
	return DetailsItemName ? DetailsItemName->GetText() : FText::GetEmpty();
}

FText UStorageScreenWidget::GetDetailsQuantityText() const
{
	return DetailsQuantity ? DetailsQuantity->GetText() : FText::GetEmpty();
}

TArray<UStorageSlotWidget*> UStorageScreenWidget::GetSlots(const bool bPlayerSide) const
{
	TArray<UStorageSlotWidget*> Slots;
	if (const UWrapBox* Panel = bPlayerSide ? PlayerItemsWrapBox.Get() : StorageItemsWrapBox.Get())
	{
		for (UWidget* Child : Panel->GetAllChildren())
		{
			if (UStorageSlotWidget* StorageSlot = Cast<UStorageSlotWidget>(Child))
			{
				Slots.Add(StorageSlot);
			}
		}
	}
	return Slots;
}

UInventoryComponent* UStorageScreenWidget::ResolvePlayerInventory() const
{
	if (UInventoryComponent* Override = PlayerInventoryOverride.Get())
	{
		return Override;
	}
	const APawn* Pawn = GetOwningPlayerPawn();
	return Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
}

void UStorageScreenWidget::RebindPlayerInventory()
{
	if (PlayerInventory)
	{
		PlayerInventory->OnInventoryChanged.RemoveDynamic(this, &UStorageScreenWidget::HandlePlayerInventoryChanged);
	}
	PlayerInventory = ResolvePlayerInventory();
	if (PlayerInventory)
	{
		PlayerInventory->OnInventoryChanged.AddUniqueDynamic(this, &UStorageScreenWidget::HandlePlayerInventoryChanged);
	}
}

void UStorageScreenWidget::ReleaseBindings()
{
	if (PlayerInventory)
	{
		PlayerInventory->OnInventoryChanged.RemoveDynamic(this, &UStorageScreenWidget::HandlePlayerInventoryChanged);
	}
	if (ActiveStorage)
	{
		ActiveStorage->OnStorageChanged.RemoveDynamic(this, &UStorageScreenWidget::HandleStorageChanged);
	}
	PlayerInventory = nullptr;
	ActiveStorage = nullptr;
}

void UStorageScreenWidget::RebuildViews()
{
	ValidateSelection();
	RebuildPanel(PlayerItemsWrapBox, PlayerInventory ? PlayerInventory->GetItemsSnapshot() : TArray<UItemInstance*>(), true);
	RebuildPanel(StorageItemsWrapBox, ActiveStorage ? ActiveStorage->GetItemsSnapshot() : TArray<UItemInstance*>(), false);
	RefreshCounts();
	RefreshDetails();
	if (StorageTitleText)
	{
		StorageTitleText->SetText(StorageTitle);
	}
}

void UStorageScreenWidget::RebuildPanel(UWrapBox* Panel, TArray<UItemInstance*> Items, const bool bPlayerSide)
{
	if (!Panel)
	{
		return;
	}
	Panel->ClearChildren();
	if (!SlotWidgetClass)
	{
		return;
	}
	SortForDisplay(Items);
	for (UItemInstance* Item : Items)
	{
		// Owned by this screen (not the player), so it also builds without a player controller.
		UStorageSlotWidget* StorageSlot = CreateWidget<UStorageSlotWidget>(this, SlotWidgetClass);
		if (!StorageSlot)
		{
			continue;
		}
		StorageSlot->InitializeSlot(Item, bPlayerSide);
		StorageSlot->SetSelected(bPlayerSide == bSelectedPlayerSide && Item->GetInstanceId() == SelectedItemId);
		StorageSlot->OnSlotSelected.AddUniqueDynamic(this, &UStorageScreenWidget::HandleSlotSelected);
		StorageSlot->OnSlotAction.AddUniqueDynamic(this, &UStorageScreenWidget::HandleSlotActionRequested);
		Panel->AddChildToWrapBox(StorageSlot);
	}
}

void UStorageScreenWidget::ValidateSelection()
{
	// A stack that left its side (moved, or merged away) is no longer selected.
	if (SelectedItemId.IsValid() && !FindSideItem(bSelectedPlayerSide, SelectedItemId))
	{
		SelectedItemId.Invalidate();
	}
}

void UStorageScreenWidget::RefreshSelectionVisuals()
{
	for (const bool bSide : { true, false })
	{
		for (UStorageSlotWidget* StorageSlot : GetSlots(bSide))
		{
			StorageSlot->SetSelected(bSide == bSelectedPlayerSide && StorageSlot->GetItemId() == SelectedItemId);
		}
	}
}

void UStorageScreenWidget::RefreshDetails()
{
	const UItemInstance* Item = FindSideItem(bSelectedPlayerSide, SelectedItemId);
	const UItemDefinition* Definition = Item ? UInventoryUIFunctionLibrary::GetDefinition(Item->GetDefinitionClass()) : nullptr;
	if (DetailsIcon)
	{
		// Keep the authored 72 x 72 brush size: matching the texture's own size made a large icon fill the screen (G9 PIE).
		DetailsIcon->SetBrushFromTexture(Definition ? Definition->ItemIcon.Get() : nullptr, false);
		DetailsIcon->SetVisibility(Definition && Definition->ItemIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DetailsItemName)
	{
		DetailsItemName->SetText(Definition ? Definition->ItemName : LOCTEXT("NoSelection", "Select an item"));
	}
	if (DetailsCategory)
	{
		DetailsCategory->SetText(Item ? UInventoryUIFunctionLibrary::GetCategoryDisplayName(
			UInventoryUIFunctionLibrary::ResolveItemCategory(Item)) : FText::GetEmpty());
	}
	if (DetailsQuantity)
	{
		DetailsQuantity->SetText(Item ? FText::Format(LOCTEXT("Quantity", "Quantity: {0}"), FText::AsNumber(Item->GetQuantity()))
			: FText::GetEmpty());
	}
	if (DetailsDescription)
	{
		DetailsDescription->SetText(Definition ? Definition->ItemDescription : FText::GetEmpty());
	}
	if (DetailsCapabilities)
	{
		DetailsCapabilities->SetText(Item ? UInventoryUIFunctionLibrary::BuildCapabilitySummary(Item->GetDefinitionClass())
			: FText::GetEmpty());
	}
}

void UStorageScreenWidget::RefreshCounts()
{
	if (PlayerCountText)
	{
		const int32 Count = PlayerInventory ? PlayerInventory->GetItemCount() : 0;
		const int32 Capacity = PlayerInventory ? PlayerInventory->GetCapacity() : 0;
		PlayerCountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Count, Capacity)));
	}
	if (StorageCountText)
	{
		const int32 Count = ActiveStorage ? ActiveStorage->GetItemCount() : 0;
		const int32 Capacity = ActiveStorage ? ActiveStorage->GetCapacity() : 0;
		StorageCountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Count, Capacity)));
	}
}

void UStorageScreenWidget::SetFeedback(const FText& Message)
{
	if (FeedbackText)
	{
		FeedbackText->SetText(Message);
	}
}

UItemInstance* UStorageScreenWidget::FindSideItem(const bool bPlayerSide, const FGuid ItemId) const
{
	if (!ItemId.IsValid())
	{
		return nullptr;
	}
	return bPlayerSide
		? (PlayerInventory ? PlayerInventory->FindItemById(ItemId) : nullptr)
		: (ActiveStorage ? ActiveStorage->FindItemById(ItemId) : nullptr);
}

FText UStorageScreenWidget::DescribeFull(const bool bTargetPlayerSide) const
{
	return bTargetPlayerSide ? LOCTEXT("InventoryFull", "Your inventory is full")
		: FText::Format(LOCTEXT("StorageFull", "{0} is full"), GetStorageDisplayTitle());
}

FText UStorageScreenWidget::GetStorageDisplayTitle() const
{
	return StorageTitle.IsEmpty() ? LOCTEXT("TheChest", "The chest") : StorageTitle;
}

#undef LOCTEXT_NAMESPACE
