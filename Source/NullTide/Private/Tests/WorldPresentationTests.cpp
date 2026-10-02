#include "InventoryComponentTestTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Items/Fragments/ItemFragment_WorldPresentation.h"
#include "Items/ItemDefinition.h"
#include "Items/ItemWorldPresentationLibrary.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FPresentationEntry
{
	const TCHAR* Id;
	const TCHAR* MeshPath;
	FVector Scale;
	FRotator Rotation;
	int32 G1FragmentCount;
};

const FPresentationEntry PresentationCatalog[] = {
	{ TEXT("HealthPotion"), TEXT("/Engine/BasicShapes/Sphere.Sphere"), FVector(0.25, 0.25, 0.35), FRotator::ZeroRotator, 2 },
	{ TEXT("ShortSword"), TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0.08, 0.08, 0.65), FRotator::ZeroRotator, 3 },
	{ TEXT("Wood"), TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0.55, 0.22, 0.22), FRotator(0.0, 15.0, 0.0), 1 },
	{ TEXT("Sandwich"), TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0.50, 0.35, 0.12), FRotator(0.0, 45.0, 0.0), 2 },
	{ TEXT("Stone"), TEXT("/Engine/BasicShapes/Sphere.Sphere"), FVector(0.32, 0.40, 0.28), FRotator::ZeroRotator, 1 },
	{ TEXT("Scrap"), TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0.32, 0.24, 0.18), FRotator(0.0, 30.0, 0.0), 1 },
	{ TEXT("Axe"), TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0.22, 0.60, 0.12), FRotator(0.0, 45.0, 0.0), 1 },
	{ TEXT("Pickaxe"), TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0.60, 0.22, 0.12), FRotator(0.0, 45.0, 0.0), 1 },
	{ TEXT("Meat"), TEXT("/Engine/BasicShapes/Sphere.Sphere"), FVector(0.45, 0.28, 0.22), FRotator(0.0, 0.0, 20.0), 2 },
	{ TEXT("LongSword"), TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0.06, 0.08, 0.90), FRotator::ZeroRotator, 3 },
	{ TEXT("WoodBow"), TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0.08, 0.65, 0.40), FRotator::ZeroRotator, 3 },
	{ TEXT("WoodArrow"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(0.05, 0.05, 0.90), FRotator(90.0, 0.0, 0.0), 1 },
	{ TEXT("WaterBottle"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(0.22, 0.22, 0.50), FRotator::ZeroRotator, 2 }
};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FWorldPresentationCatalogTest, "NullTide.Gameplay.G2.WorldPresentation.Catalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FWorldPresentationCatalogTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (const FPresentationEntry& Entry : PresentationCatalog)
	{
		Names.Add(Entry.Id);
		Commands.Add(Entry.Id);
	}
}

bool FWorldPresentationCatalogTest::RunTest(const FString& Parameters)
{
	const FPresentationEntry* Entry = nullptr;
	for (const FPresentationEntry& Candidate : PresentationCatalog)
	{
		if (Parameters == Candidate.Id) { Entry = &Candidate; break; }
	}
	if (!TestNotNull(TEXT("Known presentation entry"), Entry)) { return false; }

	const FString DefinitionPath = FString::Printf(
		TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_%s.Item_%s_C"), Entry->Id, Entry->Id);
	UClass* DefinitionClass = LoadClass<UItemDefinition>(nullptr, *DefinitionPath);
	if (!TestNotNull(TEXT("Production definition loads"), DefinitionClass)) { return false; }
	const UItemDefinition* Definition = DefinitionClass->GetDefaultObject<UItemDefinition>();
	TestTrue(TEXT("Legacy Fragments remain empty"), Definition->Fragments.IsEmpty());
	FString Diagnostic;
	TestTrue(TEXT("Complete canonical definition validates: ") + Diagnostic, Definition->ValidateFragments(Diagnostic));
	const TArray<UItemFragment*> Fragments = Definition->GetResolvedFragments();
	TestEqual(TEXT("G1 capabilities plus one presentation"), Fragments.Num(), Entry->G1FragmentCount + 1);

	int32 PresentationCount = 0;
	const UItemFragment_WorldPresentation* Presentation = nullptr;
	for (const UItemFragment* Fragment : Fragments)
	{
		if (const auto* Candidate = Cast<UItemFragment_WorldPresentation>(Fragment))
		{
			++PresentationCount;
			Presentation = Candidate;
		}
	}
	TestEqual(TEXT("Exactly one WorldPresentation"), PresentationCount, 1);
	if (!TestNotNull(TEXT("WorldPresentation resolves"), Presentation)) { return false; }
	TestTrue(TEXT("Definition CDO directly owns presentation"), Presentation->GetOuter() == Definition);
	UStaticMesh* ExpectedMesh = LoadObject<UStaticMesh>(nullptr, Entry->MeshPath);
	TestNotNull(TEXT("Expected Engine mesh loads"), ExpectedMesh);
	TestTrue(TEXT("Exact world mesh"), Presentation->WorldMesh.Get() == ExpectedMesh);
	TestTrue(TEXT("Exact world scale"), Presentation->WorldScale.Equals(Entry->Scale));
	TestTrue(TEXT("Exact world rotation"), Presentation->WorldRotationOffset.Equals(Entry->Rotation));

	const TStrongObjectPtr<UStaticMeshComponent> MeshComponent(NewObject<UStaticMeshComponent>());
	TestTrue(TEXT("Generic application succeeds: ") + Diagnostic,
		UItemWorldPresentationLibrary::ApplyItemWorldPresentation(DefinitionClass, MeshComponent.Get(), Diagnostic));
	TestTrue(TEXT("Generic component receives mesh"), MeshComponent->GetStaticMesh().Get() == ExpectedMesh);
	TestTrue(TEXT("Generic component receives scale"), MeshComponent->GetRelativeScale3D().Equals(Entry->Scale));
	TestTrue(TEXT("Generic component receives rotation"), MeshComponent->GetRelativeRotation().Equals(Entry->Rotation));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FWorldPresentationValidationTest, "NullTide.Gameplay.G2.WorldPresentation.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FWorldPresentationValidationTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (const TCHAR* Case : { TEXT("DefaultsAndValidConfiguration"), TEXT("NullMeshRejected"), TEXT("NonPositiveScaleRejected"),
		TEXT("NonFiniteRotationRejected"), TEXT("DuplicateRejected") })
	{
		Names.Add(Case);
		Commands.Add(Case);
	}
}

bool FWorldPresentationValidationTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UInventoryTestItemDefinition> Definition(NewObject<UInventoryTestItemDefinition>());
	UItemFragment_WorldPresentation* First = NewObject<UItemFragment_WorldPresentation>(Definition.Get());
	First->WorldMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Definition->ItemFragments = { First };
	if (Parameters == TEXT("DefaultsAndValidConfiguration"))
	{
		TestTrue(TEXT("Default scale is one"), First->WorldScale.Equals(FVector::OneVector));
		TestTrue(TEXT("Default rotation is zero"), First->WorldRotationOffset.Equals(FRotator::ZeroRotator));
		FString Diagnostic;
		TestTrue(TEXT("Valid configured presentation passes validation: ") + Diagnostic,
			Definition->ValidateFragments(Diagnostic));
		return true;
	}
	if (Parameters == TEXT("NullMeshRejected"))
	{
		First->WorldMesh = nullptr;
	}
	else if (Parameters == TEXT("NonPositiveScaleRejected"))
	{
		First->WorldScale = FVector(1.0, 0.0, 1.0);
	}
	else if (Parameters == TEXT("NonFiniteRotationRejected"))
	{
		First->WorldRotationOffset.Yaw = NAN;
	}
	else
	{
		UItemFragment_WorldPresentation* Second = NewObject<UItemFragment_WorldPresentation>(Definition.Get());
		Second->WorldMesh = First->WorldMesh;
		Definition->ItemFragments.Add(Second);
	}
	FString Diagnostic;
	TestFalse(TEXT("Invalid presentation is rejected"), Definition->ValidateFragments(Diagnostic));
	TestFalse(TEXT("Useful validation diagnostic"), Diagnostic.IsEmpty());
	return true;
}
#endif
