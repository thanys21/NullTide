#include "Building/BuildingDefinition.h"

#include "Building/BuildingPieceComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "Items/ItemDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
bool IsConcreteClass(const UClass* Class)
{
	return Class && !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
}

bool ValidateCost(const TArray<FInventoryDefinitionQuantity>& Cost, FString& OutDiagnostic)
{
	if (Cost.IsEmpty())
	{
		OutDiagnostic = TEXT("Cost is empty.");
		return false;
	}
	for (int32 Index = 0; Index < Cost.Num(); ++Index)
	{
		const UClass* Class = Cost[Index].DefinitionClass.Get();
		if (!IsConcreteClass(Class))
		{
			OutDiagnostic = FString::Printf(TEXT("Cost[%d] has a null or abstract item definition."), Index);
			return false;
		}
		FString ItemDiagnostic;
		if (!Class->GetDefaultObject<UItemDefinition>()->ValidateFragments(ItemDiagnostic))
		{
			OutDiagnostic = FString::Printf(TEXT("Cost[%d] (%s) has invalid item data: %s"), Index, *Class->GetPathName(), *ItemDiagnostic);
			return false;
		}
		if (Cost[Index].Quantity < 1)
		{
			OutDiagnostic = FString::Printf(TEXT("Cost[%d] quantity %d is below 1."), Index, Cost[Index].Quantity);
			return false;
		}
		for (int32 Earlier = 0; Earlier < Index; ++Earlier)
		{
			if (Cost[Earlier].DefinitionClass.Get() == Class)
			{
				OutDiagnostic = FString::Printf(TEXT("Cost[%d] and Cost[%d] duplicate item definition %s."), Earlier, Index, *Class->GetPathName());
				return false;
			}
		}
	}
	return true;
}
}

bool UBuildingDefinition::ClassHasPieceComponent(const UClass* ActorClass)
{
	if (!ActorClass || !ActorClass->IsChildOf(AActor::StaticClass()))
	{
		return false;
	}
	// Native default subobjects of the class default object.
	if (UObject* Defaults = ActorClass->GetDefaultObject(false))
	{
		TArray<UObject*> Subobjects;
		Defaults->GetDefaultSubobjects(Subobjects);
		for (const UObject* Subobject : Subobjects)
		{
			if (Subobject && Subobject->IsA<UBuildingPieceComponent>())
			{
				return true;
			}
		}
	}
	// Components added in a Blueprint (this class or a Blueprint parent).
	for (const UClass* Class = ActorClass; Class; Class = Class->GetSuperClass())
	{
		const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Class);
		const USimpleConstructionScript* ConstructionScript = Generated ? Generated->SimpleConstructionScript.Get() : nullptr;
		if (!ConstructionScript)
		{
			continue;
		}
		for (const USCS_Node* Node : ConstructionScript->GetAllNodes())
		{
			if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<UBuildingPieceComponent>())
			{
				return true;
			}
		}
	}
	return false;
}

bool UBuildingDefinition::ValidateBuilding(FString& OutDiagnostic) const
{
	OutDiagnostic.Reset();
	const UClass* ActorClass = PieceActorClass.Get();
	if (!IsConcreteClass(ActorClass))
	{
		OutDiagnostic = TEXT("PieceActorClass is null or abstract.");
		return false;
	}
	if (!ClassHasPieceComponent(ActorClass))
	{
		OutDiagnostic = FString::Printf(TEXT("PieceActorClass %s has no UBuildingPieceComponent."), *ActorClass->GetName());
		return false;
	}
	if (Footprint.X < 1 || Footprint.Y < 1)
	{
		OutDiagnostic = FString::Printf(TEXT("Footprint %d x %d must be at least 1 x 1."), Footprint.X, Footprint.Y);
		return false;
	}
	if (PlacementMode == EBuildingPlacementMode::GridEdge && Footprint != FIntPoint(1, 1))
	{
		OutDiagnostic = TEXT("GridEdge buildings occupy one edge; Footprint must be 1 x 1.");
		return false;
	}
	const bool bEdgeCategory = Category == EBuildingCategory::Wall || Category == EBuildingCategory::Door;
	if (bEdgeCategory != (PlacementMode == EBuildingPlacementMode::GridEdge))
	{
		OutDiagnostic = TEXT("Walls and doors use GridEdge; every other category uses GridCell.");
		return false;
	}
	if (RotationStep != 90)
	{
		OutDiagnostic = FString::Printf(TEXT("RotationStep %d is not supported; use 90."), RotationStep);
		return false;
	}
	if (!ValidateCost(Cost, OutDiagnostic))
	{
		return false;
	}
	if (RefundFraction < 0.f || RefundFraction > 1.f)
	{
		OutDiagnostic = FString::Printf(TEXT("RefundFraction %.3f must be between 0 and 1."), RefundFraction);
		return false;
	}
	if (BuildRangeOverride < 0.f)
	{
		OutDiagnostic = FString::Printf(TEXT("BuildRangeOverride %.1f is negative."), BuildRangeOverride);
		return false;
	}
	return true;
}

EBuildingOccupancyCategory UBuildingDefinition::GetOccupancyCategory() const
{
	switch (Category)
	{
	case EBuildingCategory::Wall:
	case EBuildingCategory::Door:
		return EBuildingOccupancyCategory::Edge;
	case EBuildingCategory::Furniture:
	case EBuildingCategory::Station:
		return EBuildingOccupancyCategory::Furniture;
	case EBuildingCategory::Foundation:
	default:
		return EBuildingOccupancyCategory::Floor;
	}
}

const UBuildingDefinition* UBuildingDefinition::ResolveValidBuilding(const TSubclassOf<UBuildingDefinition> BuildingClass,
	FString& OutDiagnostic)
{
	const UClass* Class = BuildingClass.Get();
	if (!IsConcreteClass(Class))
	{
		OutDiagnostic = TEXT("Building definition class is null or abstract.");
		return nullptr;
	}
	const UBuildingDefinition* Definition = Class->GetDefaultObject<UBuildingDefinition>();
	if (!IsValid(Definition) || !Definition->ValidateBuilding(OutDiagnostic))
	{
		return nullptr;
	}
	return Definition;
}

#if WITH_EDITOR
EDataValidationResult UBuildingDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	// The abstract native root is never authored; only concrete building assets are validated.
	if (GetClass()->HasAnyClassFlags(CLASS_Abstract))
	{
		return Result;
	}
	FString Diagnostic;
	if (!ValidateBuilding(Diagnostic))
	{
		Context.AddError(FText::FromString(Diagnostic));
		return EDataValidationResult::Invalid;
	}
	return CombineDataValidationResults(Result, EDataValidationResult::Valid);
}
#endif
