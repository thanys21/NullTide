#pragma once

#include "CoreMinimal.h"
#include "ItemFragmentTypes.generated.h"

UENUM(BlueprintType)
enum class EEquipmentSlot : uint8 { None, MainHand, OffHand };

UENUM(BlueprintType)
enum class EWeaponType : uint8 { None, Sword, Bow };

UENUM(BlueprintType)
enum class EAmmoType : uint8 { None, Arrow };

UENUM(BlueprintType)
enum class EToolType : uint8 { None, Axe, Pickaxe };
