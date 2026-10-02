#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/Fragments/ItemFragmentTypes.h"
#include "ToolLoadoutComponent.generated.h"

class UInventoryComponent;
class UItemInstance;

/** Owns exact inventory identities assigned to the player's passive tool slots. */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class NULLTIDE_API UToolLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UToolLoadoutComponent();

	/** Assigns an existing, matching inventory item to its tool-type slot. */
	UFUNCTION(BlueprintCallable, Category = "Tool Loadout")
	bool EquipTool(EToolType ToolType, FGuid ItemId);

	/** Clears the requested tool slot without changing inventory membership. */
	UFUNCTION(BlueprintCallable, Category = "Tool Loadout")
	bool UnequipTool(EToolType ToolType);

	/** Returns the exact equipped inventory instance, or null after stale/invalid assignment cleanup. */
	UFUNCTION(BlueprintPure, Category = "Tool Loadout")
	UItemInstance* GetEquippedTool(EToolType ToolType);

	UFUNCTION(BlueprintPure, Category = "Tool Loadout")
	bool HasEquippedTool(EToolType ToolType);

private:
	UInventoryComponent* ResolveInventory() const;
	bool IsCompatibleTool(const UItemInstance* Item, EToolType ToolType) const;

	UPROPERTY(Transient)
	TMap<EToolType, FGuid> EquippedItemIds;
};
