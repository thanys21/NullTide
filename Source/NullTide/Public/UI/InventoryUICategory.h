#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InventoryUICategory.generated.h"

class UItemDefinition;
class UItemInstance;
class UInventoryComponent;

UENUM(BlueprintType)
enum class EInventoryCategory : uint8
{
	All,
	Resource,
	Consumable,
	Weapon,
	Ammo,
	Misc
};

/** Read-only UI projection of canonical item definition data. */
UCLASS()
class NULLTIDE_API UInventoryUIFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	static EInventoryCategory ResolveCategory(TSubclassOf<UItemDefinition> DefinitionClass);

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	static EInventoryCategory ResolveItemCategory(const UItemInstance* Item);

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	static FText GetCategoryDisplayName(EInventoryCategory Category);

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	static FText BuildCapabilitySummary(TSubclassOf<UItemDefinition> DefinitionClass);

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	static bool IsCurrentItem(const UInventoryComponent* Inventory, const UItemInstance* Item);

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	static bool IsItemVisible(const UInventoryComponent* Inventory, const UItemInstance* Item, EInventoryCategory Category);

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	static bool IsSelectedItemValid(const UInventoryComponent* Inventory, FGuid SelectedItemId, EInventoryCategory Category);

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	static bool IsDragPayloadValid(const UInventoryComponent* Inventory, FGuid ItemId, const UItemInstance* Item);

	static const UItemDefinition* GetDefinition(TSubclassOf<UItemDefinition> DefinitionClass);
};
