#include "UI/StorageScreenWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/InventoryComponent.h"
#include "Items/ItemInstance.h"
#include "Storage/StorageComponent.h"
#include "UI/StorageSlotWidget.h"

void UStorageScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (TransferToStorageButton) TransferToStorageButton->OnClicked.AddUniqueDynamic(this, &UStorageScreenWidget::TransferToStorage);
	if (TransferToInventoryButton) TransferToInventoryButton->OnClicked.AddUniqueDynamic(this, &UStorageScreenWidget::TransferToInventory);
	if (CloseButton) CloseButton->OnClicked.AddUniqueDynamic(this, &UStorageScreenWidget::CloseStorage);
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
	SelectedPlayerItemId.Invalidate();
	SelectedStorageItemId.Invalidate();
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
	APawn* Pawn = GetOwningPlayerPawn();
	UInventoryComponent* CurrentInventory = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (CurrentInventory != PlayerInventory)
	{
		RebindPlayerInventory();
		RebuildViews();
	}
}

FReply UStorageScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::I)
	{
		CloseStorage();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
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
		SelectedStorageItemId.Invalidate();
	}
	StorageTitle = InTitle;
	RebindPlayerInventory();
	RebuildViews();
	SetKeyboardFocus();
}

void UStorageScreenWidget::CloseStorage()
{
	ReleaseBindings();
	SelectedPlayerItemId.Invalidate();
	SelectedStorageItemId.Invalidate();
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetShowMouseCursor(false);
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(PlayerController, false);
	}
	RemoveFromParent();
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
	if (bPlayerSide)
	{
		SelectedPlayerItemId = PlayerInventory && PlayerInventory->ContainsItem(ItemId) ? ItemId : FGuid();
	}
	else
	{
		SelectedStorageItemId = ActiveStorage && ActiveStorage->ContainsItem(ItemId) ? ItemId : FGuid();
	}
	RebuildViews();
}

void UStorageScreenWidget::TransferToStorage()
{
	if (!PlayerInventory || !ActiveStorage || !SelectedPlayerItemId.IsValid())
	{
		return;
	}
	const FGuid TransferredId = SelectedPlayerItemId;
	if (PlayerInventory->TryTransferItemToStorage(ActiveStorage, TransferredId).IsSuccess())
	{
		SelectedPlayerItemId.Invalidate();
	}
	RebuildViews();
}

void UStorageScreenWidget::TransferToInventory()
{
	if (!PlayerInventory || !ActiveStorage || !SelectedStorageItemId.IsValid())
	{
		return;
	}
	const FGuid TransferredId = SelectedStorageItemId;
	if (ActiveStorage->TryTransferItemToInventory(PlayerInventory, TransferredId).IsSuccess())
	{
		SelectedStorageItemId.Invalidate();
	}
	RebuildViews();
}

void UStorageScreenWidget::RebindPlayerInventory()
{
	if (PlayerInventory)
	{
		PlayerInventory->OnInventoryChanged.RemoveDynamic(this, &UStorageScreenWidget::HandlePlayerInventoryChanged);
	}
	APawn* Pawn = GetOwningPlayerPawn();
	PlayerInventory = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
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
	ValidateSelections();
	RebuildPanel(PlayerItemsWrapBox, PlayerInventory ? PlayerInventory->GetItemsSnapshot() : TArray<UItemInstance*>(), true, SelectedPlayerItemId);
	RebuildPanel(StorageItemsWrapBox, ActiveStorage ? ActiveStorage->GetItemsSnapshot() : TArray<UItemInstance*>(), false, SelectedStorageItemId);
	RefreshCounts();
	if (StorageTitleText) StorageTitleText->SetText(StorageTitle);
}

void UStorageScreenWidget::RebuildPanel(UWrapBox* Panel, const TArray<UItemInstance*>& Items, const bool bPlayerSide, const FGuid SelectedItemId)
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
	for (UItemInstance* Item : Items)
	{
		if (!IsValid(Item))
		{
			continue;
		}
		UStorageSlotWidget* StorageSlot = CreateWidget<UStorageSlotWidget>(GetOwningPlayer(), SlotWidgetClass);
		if (!StorageSlot)
		{
			continue;
		}
		StorageSlot->InitializeSlot(Item, bPlayerSide);
		StorageSlot->SetSelected(Item->GetInstanceId() == SelectedItemId);
		StorageSlot->OnSlotSelected.AddUniqueDynamic(this, &UStorageScreenWidget::HandleSlotSelected);
		Panel->AddChildToWrapBox(StorageSlot);
	}
}

void UStorageScreenWidget::ValidateSelections()
{
	if (SelectedPlayerItemId.IsValid() && (!PlayerInventory || !PlayerInventory->ContainsItem(SelectedPlayerItemId)))
	{
		SelectedPlayerItemId.Invalidate();
	}
	if (SelectedStorageItemId.IsValid() && (!ActiveStorage || !ActiveStorage->ContainsItem(SelectedStorageItemId)))
	{
		SelectedStorageItemId.Invalidate();
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
