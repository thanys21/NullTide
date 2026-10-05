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
#include "Items/Fragments/ItemFragment_Tool.h"
#include "Items/ItemInstance.h"
#include "ToolLoadout/ToolLoadoutComponent.h"
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
	ObservedToolLoadout = nullptr;
	SelectedItemId.Invalidate();
	Super::NativeDestruct();
}

void UInventoryScreenWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	APawn* CurrentPawn = GetOwningPlayerPawn();
	UInventoryComponent* CurrentInventory = CurrentPawn ? CurrentPawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	UToolLoadoutComponent* CurrentToolLoadout = CurrentPawn ? CurrentPawn->FindComponentByClass<UToolLoadoutComponent>() : nullptr;
	if (CurrentPawn != ObservedPawn || CurrentInventory != ObservedInventory || CurrentToolLoadout != ObservedToolLoadout)
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
	RefreshToolLoadout();
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
	RefreshToolLoadout();
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
	if (EquipToolButton) EquipToolButton->OnClicked.AddUniqueDynamic(this, &UInventoryScreenWidget::ExecuteSelectedToolAction);
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
	ObservedToolLoadout = Pawn ? Pawn->FindComponentByClass<UToolLoadoutComponent>() : nullptr;
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
	ObservedToolLoadout = nullptr;
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

void UInventoryScreenWidget::RefreshToolLoadout()
{
	RefreshToolSlot(EToolType::Axe, AxeToolIcon, AxeToolName);
	RefreshToolSlot(EToolType::Pickaxe, PickaxeToolIcon, PickaxeToolName);

	const EToolType SelectedToolType = GetSelectedToolType();
	const bool bCanEquip = ObservedToolLoadout && SelectedToolType != EToolType::None;
	const UItemInstance* EquippedItem = bCanEquip ? ObservedToolLoadout->GetEquippedTool(SelectedToolType) : nullptr;
	const bool bSelectedToolIsEquipped = EquippedItem && EquippedItem->GetInstanceId() == SelectedItemId;
	if (EquipToolButton) EquipToolButton->SetIsEnabled(bCanEquip);
	if (ToolActionButtonText)
	{
		ToolActionButtonText->SetText(FText::FromString(bSelectedToolIsEquipped ? TEXT("Unequip Tool") : TEXT("Equip Tool")));
	}
	if (ToolActionText)
	{
		if (!ObservedToolLoadout)
		{
			ToolActionText->SetText(FText::FromString(TEXT("Tool loadout unavailable.")));
		}
		else if (SelectedToolType == EToolType::None)
		{
			ToolActionText->SetText(FText::FromString(TEXT("Select an Axe or Pickaxe to manage its slot.")));
		}
		else if (bSelectedToolIsEquipped)
		{
			ToolActionText->SetText(FText::FromString(TEXT("Selected tool is equipped.")));
		}
		else
		{
			ToolActionText->SetText(FText::FromString(SelectedToolType == EToolType::Axe
				? TEXT("Selected Axe can fill the Axe slot.")
				: TEXT("Selected Pickaxe can fill the Pickaxe slot.")));
		}
	}
}

void UInventoryScreenWidget::RefreshToolSlot(const EToolType ToolType, UImage* Icon, UTextBlock* Name)
{
	UItemInstance* Item = ObservedToolLoadout ? ObservedToolLoadout->GetEquippedTool(ToolType) : nullptr;
	const UItemDefinition* Definition = Item ? UInventoryUIFunctionLibrary::GetDefinition(Item->GetDefinitionClass()) : nullptr;
	if (Icon) Icon->SetBrushFromTexture(Definition ? Definition->ItemIcon.Get() : nullptr, true);
	if (Name)
	{
		const FString SlotName = ToolType == EToolType::Axe ? TEXT("Axe") : TEXT("Pickaxe");
		Name->SetText(FText::FromString(Definition
			? FString::Printf(TEXT("%s\n%s"), *SlotName, *Definition->ItemName.ToString())
			: FString::Printf(TEXT("%s\nEmpty"), *SlotName)));
	}
}

EToolType UInventoryScreenWidget::GetSelectedToolType() const
{
	const UItemInstance* Item = ObservedInventory && SelectedItemId.IsValid()
		? ObservedInventory->FindItemById(SelectedItemId)
		: nullptr;
	const UItemDefinition* Definition = Item ? UInventoryUIFunctionLibrary::GetDefinition(Item->GetDefinitionClass()) : nullptr;
	const UItemFragment_Tool* Tool = Definition
		? Cast<UItemFragment_Tool>(Definition->FindFragmentByClass(UItemFragment_Tool::StaticClass()))
		: nullptr;
	return Tool ? Tool->ToolType : EToolType::None;
}

void UInventoryScreenWidget::ExecuteSelectedToolAction()
{
	const EToolType ToolType = GetSelectedToolType();
	if (ObservedToolLoadout && ToolType != EToolType::None && SelectedItemId.IsValid())
	{
		if (const UItemInstance* EquippedItem = ObservedToolLoadout->GetEquippedTool(ToolType);
			EquippedItem && EquippedItem->GetInstanceId() == SelectedItemId)
		{
			ObservedToolLoadout->UnequipTool(ToolType);
		}
		else
		{
			ObservedToolLoadout->EquipTool(ToolType, SelectedItemId);
		}
	}
	RefreshToolLoadout();
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
