#include "Items/Fragments/ItemFragment.h"

#include <initializer_list>
#include "Items/Fragments/ItemFragment_Resource.h"
#include "Items/Fragments/ItemFragment_Consumable.h"
#include "Items/Fragments/ItemFragment_Food.h"
#include "Items/Fragments/ItemFragment_Drink.h"
#include "Items/Fragments/ItemFragment_Healing.h"
#include "Items/Fragments/ItemFragment_Equippable.h"
#include "Items/Fragments/ItemFragment_Weapon.h"
#include "Items/Fragments/ItemFragment_Ammo.h"
#include "Items/Fragments/ItemFragment_Durability.h"
#include "Items/Fragments/ItemFragment_Tool.h"
#include "Items/Fragments/ItemFragment_Stackable.h"

namespace
{
bool ValidateNonNegative(FString& OutDiagnostic, std::initializer_list<TPair<const TCHAR*, float>> Fields)
{
	OutDiagnostic.Reset();
	for (const auto& Field : Fields)
	{
		if (!FMath::IsFinite(Field.Value) || Field.Value < 0.0f)
		{
			OutDiagnostic = FString::Printf(TEXT("%s must be finite and non-negative."), Field.Key);
			return false;
		}
	}
	return true;
}
}

bool UItemFragment_Food::ValidateTemplate(FString& OutDiagnostic) const
{
	return ValidateNonNegative(OutDiagnostic, { { TEXT("HungerRestore"), HungerRestore } });
}


bool UItemFragment_Drink::ValidateTemplate(FString& OutDiagnostic) const
{
	return ValidateNonNegative(OutDiagnostic, { { TEXT("HydrationRestore"), HydrationRestore } });
}


bool UItemFragment_Healing::ValidateTemplate(FString& OutDiagnostic) const
{
	return ValidateNonNegative(OutDiagnostic, { { TEXT("HealAmount"), HealAmount } });
}


bool UItemFragment_Weapon::ValidateTemplate(FString& OutDiagnostic) const
{
	return ValidateNonNegative(OutDiagnostic, { { TEXT("Damage"), Damage }, { TEXT("AttackSpeed"), AttackSpeed }, { TEXT("AttackRange"), AttackRange } });
}


bool UItemFragment_Ammo::ValidateTemplate(FString& OutDiagnostic) const
{
	return ValidateNonNegative(OutDiagnostic, { { TEXT("DamageModifier"), DamageModifier } });
}


bool UItemFragment_Durability::ValidateTemplate(FString& OutDiagnostic) const
{
	return ValidateNonNegative(OutDiagnostic, { { TEXT("MaxDurability"), MaxDurability } });
}


bool UItemFragment_Tool::ValidateTemplate(FString& OutDiagnostic) const
{
	return ValidateNonNegative(OutDiagnostic, { { TEXT("Efficiency"), Efficiency } });
}

bool UItemFragment_Stackable::ValidateTemplate(FString& OutDiagnostic) const
{
	OutDiagnostic.Reset();
	if (MaxStackSize < 2)
	{
		OutDiagnostic = TEXT("MaxStackSize must be at least 2.");
		return false;
	}
	return true;
}

