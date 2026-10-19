#include "UI/BuildingBarWidget.h"

#include "Building/BuildingDefinition.h"
#include "Building/BuildingPlacementComponent.h"
#include "Building/ConstructionComponent.h"
#include "Components/PanelWidget.h"
#include "Inventory/InventoryComponent.h"
#include "UI/BuildingBarSlotWidget.h"

#define LOCTEXT_NAMESPACE "BuildingBar"

void UBuildingBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Hit-testable so hovering anywhere on the bar counts (the confirm guard); it handles no click itself.
	SetVisibility(ESlateVisibility::Visible);
}

void UBuildingBarWidget::NativeDestruct()
{
	ReleaseBar();
	Super::NativeDestruct();
}

bool UBuildingBarWidget::InitializeBar(UBuildingPlacementComponent* InPlacement)
{
	ReleaseBar();
	Slots.Reset();
	if (SlotContainer)
	{
		SlotContainer->ClearChildren();
	}
	const UConstructionComponent* Construction = InPlacement ? InPlacement->GetConstruction() : nullptr;
	if (!Construction || !SlotContainer || !SlotClass)
	{
		return false;
	}
	Placement = InPlacement;

	const TArray<TSubclassOf<UBuildingDefinition>> Buildings = Construction->GetAvailableBuildings();
	for (int32 Index = 0; Index <= Buildings.Num(); ++Index)
	{
		UBuildingBarSlotWidget* BarSlot = CreateWidget<UBuildingBarSlotWidget>(this, SlotClass);
		if (!BarSlot)
		{
			continue;
		}
		// The building slots follow AvailableBuildings (keys 1-n); the last slot is demolish (C).
		const bool bDemolish = Index == Buildings.Num();
		// The slot index is the building index (demolish: Buildings.Num()), even if a slot failed to create.
		BarSlot->SetupSlot(this, Index, bDemolish ? LOCTEXT("DemolishKey", "C") : FText::AsNumber(Index + 1),
			bDemolish ? nullptr : Buildings[Index]);
		SlotContainer->AddChild(BarSlot);
		Slots.Add(BarSlot);
	}

	if (UInventoryComponent* BuilderInventory = Construction->GetInventory())
	{
		Inventory = BuilderInventory;
		BuilderInventory->OnInventoryChanged.AddUniqueDynamic(this, &UBuildingBarWidget::HandleInventoryChanged);
	}
	RefreshAffordability();
	SetSelection(InPlacement->GetSelectedBuildingIndex(), InPlacement->IsDemolishModeActive());
	return !Slots.IsEmpty();
}

void UBuildingBarWidget::ReleaseBar()
{
	if (UInventoryComponent* BuilderInventory = Inventory.Get())
	{
		BuilderInventory->OnInventoryChanged.RemoveDynamic(this, &UBuildingBarWidget::HandleInventoryChanged);
	}
	Inventory.Reset();
	Placement.Reset();
	bHasShownSelection = false;
}

void UBuildingBarWidget::SetSelection(const int32 SelectedIndex, const bool bDemolish)
{
	if (bHasShownSelection && ShownIndex == SelectedIndex && bShownDemolish == bDemolish)
	{
		return;
	}
	bHasShownSelection = true;
	ShownIndex = SelectedIndex;
	bShownDemolish = bDemolish;
	for (UBuildingBarSlotWidget* BarSlot : Slots)
	{
		if (BarSlot)
		{
			BarSlot->SetSelected(bDemolish ? BarSlot->IsDemolishSlot() : !BarSlot->IsDemolishSlot() && BarSlot->GetSlotIndex() == SelectedIndex);
		}
	}
}

void UBuildingBarWidget::RefreshAffordability()
{
	const UInventoryComponent* BuilderInventory = Inventory.Get();
	for (UBuildingBarSlotWidget* BarSlot : Slots)
	{
		if (!BarSlot)
		{
			continue;
		}
		if (BarSlot->IsDemolishSlot())
		{
			BarSlot->SetAffordable(true);
			continue;
		}
		const UBuildingDefinition* Definition = BarSlot->GetBuilding() ? BarSlot->GetBuilding()->GetDefaultObject<UBuildingDefinition>() : nullptr;
		// The exact read-only preflight of the payment TryBuild makes; advisory here.
		const bool bAffordable = Definition && BuilderInventory
			&& (Definition->Cost.IsEmpty() || BuilderInventory->CanApplyDefinitionTransaction(Definition->Cost, {}).IsSuccess());
		BarSlot->SetAffordable(bAffordable);
	}
}

void UBuildingBarWidget::HandleSlotClicked(const int32 SlotIndex)
{
	UBuildingPlacementComponent* Builder = Placement.Get();
	const TObjectPtr<UBuildingBarSlotWidget>* Found = Slots.FindByPredicate([SlotIndex](const UBuildingBarSlotWidget* Candidate)
	{
		return Candidate && Candidate->GetSlotIndex() == SlotIndex;
	});
	const UBuildingBarSlotWidget* BarSlot = Found ? Found->Get() : nullptr;
	if (!Builder || !BarSlot)
	{
		return;
	}
	if (BarSlot->IsDemolishSlot())
	{
		Builder->ToggleDemolishMode();
	}
	else
	{
		Builder->RequestSelectBuilding(SlotIndex);
	}
}

TArray<UBuildingBarSlotWidget*> UBuildingBarWidget::GetSlots() const
{
	TArray<UBuildingBarSlotWidget*> Result;
	for (UBuildingBarSlotWidget* BarSlot : Slots)
	{
		Result.Add(BarSlot);
	}
	return Result;
}

void UBuildingBarWidget::HandleInventoryChanged(int32 NewRevision)
{
	RefreshAffordability();
}

#undef LOCTEXT_NAMESPACE
