#include "UI/InventoryScreenWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/Widget.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemInstance.h"
#include "UI/InventoryDragDropOperation.h"
#include "UI/InventorySlotWidget.h"

void UInventoryScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	BindControls();

	ObservedController = GetOwningPlayer();
	if (ObservedController)
	{
		ObservedController->OnPossessedPawnChanged.AddUniqueDynamic(this, &UInventoryScreenWidget::HandlePossessedPawnChanged);
	}
	RebindToPawn(GetOwningPlayerPawn());
	SetKeyboardFocus();
}

void UInventoryScreenWidget::NativeDestruct()
{
	ReleaseInventoryBinding();
	if (ObservedController)
	{
		ObservedController->OnPossessedPawnChanged.RemoveDynamic(this, &UInventoryScreenWidget::HandlePossessedPawnChanged);
	}
	ObservedController = nullptr;
	ObservedPawn = nullptr;
	SelectedItemId.Invalidate();
	Super::NativeDestruct();
}

void UInventoryScreenWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	APawn* CurrentPawn = GetOwningPlayerPawn();
	UInventoryComponent* CurrentInventory = CurrentPawn ? CurrentPawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (CurrentPawn != ObservedPawn || CurrentInventory != ObservedInventory)
	{
		RebindToPawn(CurrentPawn);
	}
}

FReply UInventoryScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::I || InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseInventory();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool UInventoryScreenWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (auto* Operation = Cast<UInventoryDragDropOperation>(InOperation))
	{
		if (Operation->IsPayloadValid(ObservedInventory))
		{
			CompleteDragOperation(Operation,
				IsScreenPositionInsideInventoryWindow(InDragDropEvent.GetScreenSpacePosition())
					? EInventoryUIDropResult::AcceptedNoMutation
					: EInventoryUIDropResult::OutsideDropRequested);
		}
		else
		{
			CompleteDragOperation(Operation, EInventoryUIDropResult::Invalid);
		}
		return true;
	}
	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UInventoryScreenWidget::RebuildInventoryView()
{
	if (!InventoryContainerWrapBox)
	{
		return;
	}
	InventoryContainerWrapBox->ClearChildren();

	int32 VisibleCount = 0;
	const int32 TotalCount = ObservedInventory ? ObservedInventory->GetItemCount() : 0;
	bool bSelectionStillVisible = false;
	if (ObservedInventory && SlotWidgetClass)
	{
		for (UItemInstance* Item : ObservedInventory->GetItemsSnapshot())
		{
			if (!IsVisibleInActiveCategory(Item))
			{
				continue;
			}

			auto* SlotWidget = CreateWidget<UInventorySlotWidget>(GetOwningPlayer(), SlotWidgetClass);
			if (!SlotWidget)
			{
				continue;
			}
			SlotWidget->InitializeSlot(Item, ObservedInventory, this);
			SlotWidget->OnSlotSelected.AddUniqueDynamic(this, &UInventoryScreenWidget::HandleSlotSelected);
			const bool bIsSelected = SelectedItemId.IsValid() && Item->GetInstanceId() == SelectedItemId;
			SlotWidget->SetSelected(bIsSelected);
			bSelectionStillVisible |= bIsSelected;
			InventoryContainerWrapBox->AddChildToWrapBox(SlotWidget);
			++VisibleCount;
		}
	}

	if (SelectedItemId.IsValid() && !bSelectionStillVisible)
	{
		SelectedItemId.Invalidate();
	}
	if (FooterCount)
	{
		FooterCount->SetText(FText::FromString(FString::Printf(TEXT("%d shown  |  %d total  |  24 slots"), VisibleCount, TotalCount)));
	}
	RefreshDetails();
	RefreshTabVisuals();
}

void UInventoryScreenWidget::HandleNativeInventoryChanged(const int32 NewRevision)
{
	CancelActiveDragIfInvalidOrHidden();
	RebuildInventoryView();
}

void UInventoryScreenWidget::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	RebindToPawn(NewPawn);
}

void UInventoryScreenWidget::HandleSlotSelected(const FGuid ItemId)
{
	if (!ObservedInventory)
	{
		return;
	}
	UItemInstance* Item = ObservedInventory->FindItemById(ItemId);
	if (!Item || !IsVisibleInActiveCategory(Item))
	{
		SelectedItemId.Invalidate();
	}
	else
	{
		SelectedItemId = ItemId;
	}

	if (InventoryContainerWrapBox)
	{
		for (UWidget* Child : InventoryContainerWrapBox->GetAllChildren())
		{
			if (auto* SlotWidget = Cast<UInventorySlotWidget>(Child))
			{
				SlotWidget->SetSelected(SelectedItemId.IsValid() && SlotWidget->GetItemId() == SelectedItemId);
			}
		}
	}
	RefreshDetails();
}

void UInventoryScreenWidget::CloseInventory()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetShowMouseCursor(false);
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(PlayerController, false);
	}
	RemoveFromParent();
}

void UInventoryScreenWidget::ShowAll() { SetActiveCategory(EInventoryCategory::All); }
void UInventoryScreenWidget::ShowResources() { SetActiveCategory(EInventoryCategory::Resource); }
void UInventoryScreenWidget::ShowConsumables() { SetActiveCategory(EInventoryCategory::Consumable); }
void UInventoryScreenWidget::ShowWeapons() { SetActiveCategory(EInventoryCategory::Weapon); }
void UInventoryScreenWidget::ShowAmmo() { SetActiveCategory(EInventoryCategory::Ammo); }

void UInventoryScreenWidget::BindControls()
{
	if (CloseButton) CloseButton->OnClicked.AddUniqueDynamic(this, &UInventoryScreenWidget::CloseInventory);
	if (TabAllButton) TabAllButton->OnClicked.AddUniqueDynamic(this, &UInventoryScreenWidget::ShowAll);
	if (TabResourceButton) TabResourceButton->OnClicked.AddUniqueDynamic(this, &UInventoryScreenWidget::ShowResources);
	if (TabConsumableButton) TabConsumableButton->OnClicked.AddUniqueDynamic(this, &UInventoryScreenWidget::ShowConsumables);
	if (TabWeaponButton) TabWeaponButton->OnClicked.AddUniqueDynamic(this, &UInventoryScreenWidget::ShowWeapons);
	if (TabAmmoButton) TabAmmoButton->OnClicked.AddUniqueDynamic(this, &UInventoryScreenWidget::ShowAmmo);
}

void UInventoryScreenWidget::RebindToPawn(APawn* Pawn)
{
	if (ActiveDragOperation.IsValid())
	{
		CompleteDragOperation(ActiveDragOperation.Get(), EInventoryUIDropResult::Cancelled);
	}
	ReleaseInventoryBinding();
	ObservedPawn = Pawn;
	ObservedInventory = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (ObservedInventory)
	{
		ObservedInventory->OnInventoryChanged.AddUniqueDynamic(this, &UInventoryScreenWidget::HandleNativeInventoryChanged);
	}
	RebuildInventoryView();
}

void UInventoryScreenWidget::ReleaseInventoryBinding()
{
	if (ObservedInventory)
	{
		ObservedInventory->OnInventoryChanged.RemoveDynamic(this, &UInventoryScreenWidget::HandleNativeInventoryChanged);
	}
	ObservedInventory = nullptr;
}

void UInventoryScreenWidget::SetActiveCategory(const EInventoryCategory NewCategory)
{
	if (ActiveCategory == NewCategory)
	{
		return;
	}
	ActiveCategory = NewCategory;
	CancelActiveDragIfInvalidOrHidden();
	RebuildInventoryView();
}

void UInventoryScreenWidget::RegisterDragOperation(UInventoryDragDropOperation* Operation)
{
	ActiveDragOperation = Operation;
}

void UInventoryScreenWidget::CompleteDragOperation(UInventoryDragDropOperation* Operation, const EInventoryUIDropResult Result)
{
	if (!Operation)
	{
		return;
	}

	const bool bOutsideDropRequest = Result == EInventoryUIDropResult::OutsideDropRequested;
	const bool bPayloadIsValid = bOutsideDropRequest && Operation->IsPayloadValid(ObservedInventory);
	const FGuid ItemId = Operation->ItemId;
	Operation->Complete(Result);
	if (ActiveDragOperation.Get() == Operation)
	{
		ActiveDragOperation = nullptr;
	}
	if (bOutsideDropRequest)
	{
		if (!bPayloadIsValid || !ObservedInventory || !ObservedPawn)
		{
			UE_LOG(LogTemp, Warning, TEXT("G6 rejected an invalid inventory world-drop request for %s."), *ItemId.ToString());
			return;
		}

		const FInventoryWorldDropResult DropResult = ObservedInventory->TryDropItemToWorld(ObservedPawn, ItemId);
		if (DropResult.IsSuccess())
		{
			UE_LOG(LogTemp, Log, TEXT("G6 inventory world drop committed for %s."), *ItemId.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("G6 inventory world drop failed for %s (result %d)."),
				*ItemId.ToString(), static_cast<int32>(DropResult.Result));
		}
		return;
	}

	switch (Result)
	{
	case EInventoryUIDropResult::AcceptedNoMutation:
		UE_LOG(LogTemp, Log, TEXT("G5 inventory UI drop accepted without mutation for %s."), *Operation->ItemId.ToString());
		break;
	case EInventoryUIDropResult::Invalid:
		UE_LOG(LogTemp, Warning, TEXT("G5 rejected a stale or invalid inventory drag payload."));
		break;
	default:
		break;
	}
}

void UInventoryScreenWidget::HandleUnreceivedDragCancellation(UInventoryDragDropOperation* Operation, const FVector2D ScreenPosition)
{
	if (!Operation || Operation != ActiveDragOperation.Get())
	{
		return;
	}
	const bool bInsideInventoryWindow = IsScreenPositionInsideInventoryWindow(ScreenPosition);

	if (!Operation->IsPayloadValid(ObservedInventory))
	{
		CompleteDragOperation(Operation, EInventoryUIDropResult::Invalid);
	}
	else if (bInsideInventoryWindow)
	{
		CompleteDragOperation(Operation, EInventoryUIDropResult::Cancelled);
	}
	else
	{
		CompleteDragOperation(Operation, EInventoryUIDropResult::OutsideDropRequested);
	}
}

bool UInventoryScreenWidget::IsScreenPositionInsideInventoryWindow(const FVector2D ScreenPosition) const
{
	const UWidget* InventoryWindow = GetWidgetFromName(TEXT("InventoryWindow"));
	return InventoryWindow && InventoryWindow->GetCachedGeometry().IsUnderLocation(ScreenPosition);
}

void UInventoryScreenWidget::CancelActiveDragIfInvalidOrHidden()
{
	if (!ActiveDragOperation.IsValid())
	{
		return;
	}

	UInventoryDragDropOperation* Operation = ActiveDragOperation.Get();
	const UItemInstance* Item = Operation->IsPayloadValid(ObservedInventory)
		? ObservedInventory->FindItemById(Operation->ItemId)
		: nullptr;
	CompleteDragOperation(Operation,
		Item && IsVisibleInActiveCategory(Item) ? EInventoryUIDropResult::Cancelled : EInventoryUIDropResult::Invalid);
}

void UInventoryScreenWidget::RefreshDetails()
{
	UItemInstance* Item = ObservedInventory && SelectedItemId.IsValid()
		? ObservedInventory->FindItemById(SelectedItemId)
		: nullptr;
	if (!Item || !IsVisibleInActiveCategory(Item))
	{
		ClearDetails();
		return;
	}
	const UItemDefinition* Definition = UInventoryUIFunctionLibrary::GetDefinition(Item->GetDefinitionClass());
	if (!Definition)
	{
		ClearDetails();
		return;
	}
	if (DetailsIcon) DetailsIcon->SetBrushFromTexture(Definition->ItemIcon.Get(), true);
	if (DetailsItemName) DetailsItemName->SetText(Definition->ItemName);
	if (DetailsItemDescription) DetailsItemDescription->SetText(Definition->ItemDescription);
	if (DetailsCategory) DetailsCategory->SetText(UInventoryUIFunctionLibrary::GetCategoryDisplayName(UInventoryUIFunctionLibrary::ResolveItemCategory(Item)));
	if (CapabilitySummary) CapabilitySummary->SetText(UInventoryUIFunctionLibrary::BuildCapabilitySummary(Item->GetDefinitionClass()));
}

void UInventoryScreenWidget::ClearDetails()
{
	if (DetailsIcon) DetailsIcon->SetBrushFromTexture(nullptr, true);
	if (DetailsItemName) DetailsItemName->SetText(FText::FromString(TEXT("Select an item")));
	if (DetailsItemDescription) DetailsItemDescription->SetText(FText::FromString(TEXT("Item details appear here.")));
	if (DetailsCategory) DetailsCategory->SetText(FText::GetEmpty());
	if (CapabilitySummary) CapabilitySummary->SetText(FText::GetEmpty());
}

void UInventoryScreenWidget::RefreshTabVisuals()
{
	const FLinearColor Active(0.26f, 0.38f, 0.24f, 1.0f);
	const FLinearColor Inactive(0.12f, 0.13f, 0.12f, 1.0f);
	if (TabAllButton) TabAllButton->SetBackgroundColor(ActiveCategory == EInventoryCategory::All ? Active : Inactive);
	if (TabResourceButton) TabResourceButton->SetBackgroundColor(ActiveCategory == EInventoryCategory::Resource ? Active : Inactive);
	if (TabConsumableButton) TabConsumableButton->SetBackgroundColor(ActiveCategory == EInventoryCategory::Consumable ? Active : Inactive);
	if (TabWeaponButton) TabWeaponButton->SetBackgroundColor(ActiveCategory == EInventoryCategory::Weapon ? Active : Inactive);
	if (TabAmmoButton) TabAmmoButton->SetBackgroundColor(ActiveCategory == EInventoryCategory::Ammo ? Active : Inactive);
}

bool UInventoryScreenWidget::IsVisibleInActiveCategory(const UItemInstance* Item) const
{
	return UInventoryUIFunctionLibrary::IsItemVisible(ObservedInventory, Item, ActiveCategory);
}
