#include "ItemFragmentTestTypes.h"
#include "InventoryComponentTestTypes.h"
#include "Compatibility/LegacyInventoryCompatibilityLibrary.h"
#include "Items/Fragments/ItemFragment_Weapon.h"
#include "Items/Fragments/ItemFragment_Equippable.h"
#include "Items/Fragments/ItemFragment_Durability.h"
#include "Items/Fragments/ItemFragment_Consumable.h"
#include "Items/Fragments/ItemFragment_Food.h"
#include "Items/Fragments/ItemFragment_Drink.h"
#include "Items/Fragments/ItemFragment_Healing.h"
#include "Items/Fragments/ItemFragment_Ammo.h"
#include "Items/Fragments/ItemFragment_Tool.h"
#include "Items/ItemInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectHash.h"
#include <limits>

UInventoryFragmentTemplateTestDefinition::UInventoryFragmentTemplateTestDefinition()
{
	UItemFragment_Resource* Resource = CreateDefaultSubobject<UItemFragment_Resource>(TEXT("ResourceTemplate"));
	Resource->ResourceType = TEXT("TestResource");
	UItemFragment_Weapon* Weapon = CreateDefaultSubobject<UItemFragment_Weapon>(TEXT("WeaponTemplate"));
	Weapon->WeaponType = EWeaponType::Sword;
	Weapon->Damage = 12.5f;
	Weapon->AttackRange = 125.0f;
	UItemFragment_Equippable* Equippable = CreateDefaultSubobject<UItemFragment_Equippable>(TEXT("EquipTemplate"));
	Equippable->EquipmentSlot = EEquipmentSlot::MainHand;
	UItemFragment_Durability* Durability = CreateDefaultSubobject<UItemFragment_Durability>(TEXT("DurabilityTemplate"));
	Durability->MaxDurability = 100.0f;
	ItemFragments = { Resource, Weapon, Equippable, Durability };
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FInventoryFragmentValidationTest,
	"NullTide.Inventory.M6", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FInventoryFragmentValidationTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (const TCHAR* Name : { TEXT("EmptyFragmentsValid"), TEXT("CanonicalArrayValid"), TEXT("LegacyFallbackValid"),
		TEXT("DualSourcesRejected"), TEXT("NullCanonicalRejected"), TEXT("NullLegacyRejected"),
		TEXT("DuplicateCapabilityRejected"), TEXT("ParentChildAmbiguityRejected"), TEXT("SiblingAmbiguityRejected"),
		TEXT("SingleLookupAndMissingQuery"), TEXT("DeterministicOrderAndSnapshotIsolation"),
		TEXT("InvalidAddIsAtomic"), TEXT("InvalidImportIsAtomic"), TEXT("LegacyBoundaryRejectsInvalidData"),
		TEXT("RuntimeCreationPreservesTemplates"), TEXT("AllFoundationCapabilitiesCompose"), TEXT("InvalidStaticNumbersRejected"), TEXT("AuthoredTemplatesPersistAndRemainStatic") })
	{
		Names.Add(Name);
		Commands.Add(Name);
	}
}

bool FInventoryFragmentValidationTest::RunTest(const FString& Case)
{
	const TStrongObjectPtr<UInventoryTestItemDefinition> Definition(NewObject<UInventoryTestItemDefinition>());
	UItemFragment_Resource* Resource = NewObject<UItemFragment_Resource>(Definition.Get());
	UItemFragment_Weapon* Weapon = NewObject<UItemFragment_Weapon>(Definition.Get());
	UItemFragmentLegacyTestCapability* Legacy = NewObject<UItemFragmentLegacyTestCapability>(Definition.Get());
	TArray<UItemFragment*> Resolved;
	FString Diagnostic;

	if (Case == TEXT("EmptyFragmentsValid"))
	{
		TestTrue(TEXT("Zero fragments is valid"), Definition->ResolveFragments(Resolved, Diagnostic));
		TestEqual(TEXT("Empty result"), Resolved.Num(), 0);
		TestTrue(TEXT("No validation diagnostic"), Diagnostic.IsEmpty());
	}
	else if (Case == TEXT("CanonicalArrayValid"))
	{
		Definition->ItemFragments = { Resource, Weapon };
		TestTrue(TEXT("Canonical source validates"), Definition->ResolveFragments(Resolved, Diagnostic));
		TestEqual(TEXT("Canonical entries resolved"), Resolved.Num(), 2);
		TestTrue(TEXT("Legacy source remains empty"), Definition->Fragments.IsEmpty());
	}
	else if (Case == TEXT("LegacyFallbackValid"))
	{
		Definition->Fragments = { Legacy };
		TestTrue(TEXT("Legacy source validates"), Definition->ResolveFragments(Resolved, Diagnostic));
		TestTrue(TEXT("Fallback contains exact legacy template"), Resolved.Num() == 1 && Resolved[0] == Legacy);
		TestEqual(TEXT("Legacy capability query"), Definition->FindFragmentByClass(UItemFragmentLegacyTestCapability::StaticClass()), static_cast<UItemFragment*>(Legacy));
	}
	else if (Case == TEXT("DualSourcesRejected"))
	{
		Definition->ItemFragments = { Resource };
		Definition->Fragments = { Legacy };
		TestFalse(TEXT("No merging of both sources"), Definition->ResolveFragments(Resolved, Diagnostic));
		TestEqual(TEXT("Failure publishes no partial result"), Resolved.Num(), 0);
		TestTrue(TEXT("Clear source conflict"), Diagnostic.Contains(TEXT("both populated")));
		TestTrue(TEXT("No capability query on invalid definition"), !Definition->HasFragmentByClass(UItemFragment_Resource::StaticClass()));
	}
	else if (Case == TEXT("NullCanonicalRejected") || Case == TEXT("NullLegacyRejected"))
	{
		const bool bCanonical = Case == TEXT("NullCanonicalRejected");
		if (bCanonical) { Definition->ItemFragments = { Resource, nullptr }; }
		else { Definition->Fragments = { Legacy, nullptr }; }
		TestFalse(TEXT("Null entry rejected"), Definition->ResolveFragments(Resolved, Diagnostic));
		TestEqual(TEXT("Valid prefix not published"), Resolved.Num(), 0);
		TestTrue(TEXT("Source/index diagnostic"), Diagnostic.Contains(bCanonical ? TEXT("ItemFragments[1]") : TEXT("Fragments[1]")));
	}
	else if (Case == TEXT("DuplicateCapabilityRejected") || Case == TEXT("ParentChildAmbiguityRejected") || Case == TEXT("SiblingAmbiguityRejected"))
	{
		if (Case == TEXT("DuplicateCapabilityRejected")) { Definition->ItemFragments = { Resource, NewObject<UItemFragment_Resource>(Definition.Get()) }; }
		else if (Case == TEXT("ParentChildAmbiguityRejected")) { Definition->ItemFragments = { Resource, NewObject<UItemFragmentResourceTestChild>(Definition.Get()) }; }
		else { Definition->ItemFragments = { NewObject<UItemFragmentResourceTestChild>(Definition.Get()), NewObject<UItemFragmentResourceTestSibling>(Definition.Get()) }; }
		TestFalse(TEXT("Capability ambiguity rejected"), Definition->ResolveFragments(Resolved, Diagnostic));
		TestEqual(TEXT("No first-match behavior"), Resolved.Num(), 0);
		TestTrue(TEXT("Diagnostic identifies shared capability"), Diagnostic.Contains(TEXT("ItemFragment_Resource")) && Diagnostic.Contains(TEXT("ambiguous")));
	}
	else if (Case == TEXT("SingleLookupAndMissingQuery"))
	{
		UItemFragmentResourceTestChild* Child = NewObject<UItemFragmentResourceTestChild>(Definition.Get());
		Definition->ItemFragments = { Child, Weapon };
		TestEqual(TEXT("Base capability finds its single subclass"), Definition->FindFragmentByClass(UItemFragment_Resource::StaticClass()), static_cast<UItemFragment*>(Child));
		TestTrue(TEXT("Has matches same resolver"), Definition->HasFragmentByClass(UItemFragment_Weapon::StaticClass()));
		TestNull(TEXT("Missing capability"), Definition->FindFragmentByClass(UItemFragment_Drink::StaticClass()));
		TestNull(TEXT("Null query"), Definition->FindFragmentByClass(nullptr));
		TestNull(TEXT("Overbroad root query is ambiguous"), Definition->FindFragmentByClass(UItemFragment::StaticClass()));
		TestTrue(TEXT("Independent capability composition stays valid"), Definition->ValidateFragments(Diagnostic));
	}
	else if (Case == TEXT("DeterministicOrderAndSnapshotIsolation"))
	{
		Definition->ItemFragments = { Weapon, Resource };
		const auto First = Definition->GetResolvedFragments();
		const auto Second = Definition->GetResolvedFragments();
		TestTrue(TEXT("Deterministic identity/order"), First == Second && First.Num() == 2 && First[0] == Weapon && First[1] == Resource);
		auto Copy = First;
		Copy.Reset();
		TestEqual(TEXT("Array copy cannot change definition membership"), Definition->ItemFragments.Num(), 2);
	}
	else if (Case == TEXT("InvalidAddIsAtomic"))
	{
		const TStrongObjectPtr<UInventoryComponent> Inventory(NewObject<UInventoryComponent>());
		const TStrongObjectPtr<UInventoryComponentTestListener> Listener(NewObject<UInventoryComponentTestListener>());
		Inventory->OnInventoryChanged.AddDynamic(Listener.Get(), &UInventoryComponentTestListener::HandleInventoryChanged);
		const auto Existing = Inventory->TryAddDefinition(UInventoryTestItemDefinition::StaticClass());
		const auto Before = Inventory->GetItemsSnapshot();
		const int32 Revision = Inventory->GetRevision();
		const int32 Events = Listener->EventCount;
		TestTrue(TEXT("Invalid canonical data rejected"), Inventory->TryAddDefinition(UInventoryCanonicalInvalidTestDefinition::StaticClass()).Result == EInventoryOperationResult::InvalidDefinitionData);
		TestTrue(TEXT("Authoritative membership preserved"), Inventory->GetItemsSnapshot() == Before && Inventory->ContainsItem(Existing.ItemId));
		TestEqual(TEXT("No failure revision"), Inventory->GetRevision(), Revision);
		TestEqual(TEXT("No failure notification"), Listener->EventCount, Events);
	}
	else if (Case == TEXT("InvalidImportIsAtomic"))
	{
		const TStrongObjectPtr<UInventoryCutoverTestManager> Inventory(NewObject<UInventoryCutoverTestManager>());
		FindFProperty<FBoolProperty>(UInventoryComponent::StaticClass(), TEXT("bLegacyInventoryMode"))->SetPropertyValue_InContainer(Inventory.Get(), true);
		Inventory->InventoryItems = { UInventoryFragmentTemplateTestDefinition::StaticClass(), UInventoryCanonicalInvalidTestDefinition::StaticClass() };
		const auto Seed = Inventory->InventoryItems;
		TestTrue(TEXT("Canonical invalid seed rejects import"), Inventory->InitializeFromLegacyInventory().Result == EInventoryOperationResult::InvalidDefinitionData);
		TestEqual(TEXT("No valid-prefix native import"), Inventory->GetItemCount(), 0);
		TestEqual(TEXT("No failed-import revision"), Inventory->GetRevision(), 0);
		TestTrue(TEXT("Original seed preserved"), Inventory->InventoryItems == Seed);
	}
	else if (Case == TEXT("LegacyBoundaryRejectsInvalidData"))
	{
		const TStrongObjectPtr<UInventoryLegacyTestManager> Inventory(NewObject<UInventoryLegacyTestManager>());
		TestTrue(TEXT("Legacy adapter accepts canonical configuration"), ULegacyInventoryCompatibilityLibrary::TryAddDefinitionToLegacyInventory(Inventory.Get(), UInventoryFragmentTemplateTestDefinition::StaticClass()).IsSuccess());
		TestTrue(TEXT("Legacy adapter rejects canonical null"), ULegacyInventoryCompatibilityLibrary::TryAddDefinitionToLegacyInventory(Inventory.Get(), UInventoryCanonicalInvalidTestDefinition::StaticClass()).Result == EInventoryOperationResult::InvalidDefinitionData);
		TestEqual(TEXT("Only successful legacy add retained"), Inventory->InventoryItems.Num(), 1);
	}
	else if (Case == TEXT("RuntimeCreationPreservesTemplates"))
	{
		const UItemDefinition* Template = GetDefault<UInventoryFragmentTemplateTestDefinition>();
		const auto Before = Template->GetResolvedFragments();
		const UItemFragment_Weapon* BeforeWeapon = CastChecked<UItemFragment_Weapon>(Before[1]);
		const float Damage = BeforeWeapon->Damage;
		const float MaxDurability = CastChecked<UItemFragment_Durability>(Before[3])->MaxDurability;
		const TStrongObjectPtr<UInventoryComponent> Inventory(NewObject<UInventoryComponent>());
		const auto First = Inventory->TryAddDefinition(UInventoryFragmentTemplateTestDefinition::StaticClass());
		const auto Second = Inventory->TryAddDefinition(UInventoryFragmentTemplateTestDefinition::StaticClass());
		TestTrue(TEXT("Canonical items acquired successfully"), First.IsSuccess() && Second.IsSuccess());
		TestTrue(TEXT("One independent ID per acquired unit"), First.ItemId != Second.ItemId);
		TestTrue(TEXT("Definition template identities/order unchanged"), Template->GetResolvedFragments() == Before);
		for (const UItemFragment* Fragment : Before) { TestEqual(TEXT("Definition owns shared static template"), static_cast<const UObject*>(Fragment->GetOuter()), static_cast<const UObject*>(Template)); }
		TestEqual(TEXT("Weapon config unchanged"), BeforeWeapon->Damage, Damage);
		TestEqual(TEXT("Durability config unchanged"), CastChecked<UItemFragment_Durability>(Before[3])->MaxDurability, MaxDurability);
		TestNull(TEXT("No CurrentDurability field"), FindFProperty<FProperty>(UItemFragment_Durability::StaticClass(), TEXT("CurrentDurability")));
	}
	else if (Case == TEXT("AllFoundationCapabilitiesCompose"))
	{
		Definition->ItemFragments = { Resource, Weapon, NewObject<UItemFragment_Equippable>(Definition.Get()),
			NewObject<UItemFragment_Durability>(Definition.Get()), NewObject<UItemFragment_Consumable>(Definition.Get()),
			NewObject<UItemFragment_Food>(Definition.Get()), NewObject<UItemFragment_Drink>(Definition.Get()),
			NewObject<UItemFragment_Healing>(Definition.Get()), NewObject<UItemFragment_Ammo>(Definition.Get()), NewObject<UItemFragment_Tool>(Definition.Get()) };
		TestTrue(TEXT("All ten independent capabilities validate"), Definition->ResolveFragments(Resolved, Diagnostic));
		TestEqual(TEXT("Ten capabilities, no implicit behavior"), Resolved.Num(), 10);
	}
	else if (Case == TEXT("InvalidStaticNumbersRejected"))
	{
		Definition->ItemFragments = { Weapon };
		Weapon->Damage = -1.0f;
		TestFalse(TEXT("Negative static damage rejected"), Definition->ValidateFragments(Diagnostic));
		TestTrue(TEXT("Numeric diagnostic"), Diagnostic.Contains(TEXT("Damage")));
		Weapon->Damage = std::numeric_limits<float>::infinity();
		TestFalse(TEXT("Nonfinite static damage rejected"), Definition->ValidateFragments(Diagnostic));
	}
	else if (Case == TEXT("AuthoredTemplatesPersistAndRemainStatic"))
	{
		const FArrayProperty* CanonicalProperty = FindFProperty<FArrayProperty>(UItemDefinition::StaticClass(), TEXT("ItemFragments"));
		TestTrue(TEXT("Canonical array is readonly and contains instanced references"), CanonicalProperty &&
			CanonicalProperty->HasAllPropertyFlags(CPF_BlueprintReadOnly | CPF_ContainsInstancedReference));
		for (const TCHAR* Name : { TEXT("Wood"), TEXT("ShortSword") })
		{
			const FString Path = FString::Printf(TEXT("/Game/LevelPrototyping/InventorySystem/Items/Item_%s.Item_%s_C"), Name, Name);
			UClass* Class = LoadClass<UItemDefinition>(nullptr, *Path);
			if (!TestNotNull(TEXT("Authored definition loads"), Class)) { continue; }
			TestEqual(TEXT("Native parent unchanged"), Class->GetSuperClass(), UItemDefinition::StaticClass());
			const UItemDefinition* Authored = Class->GetDefaultObject<UItemDefinition>();
			const bool bWood = FString(Name) == TEXT("Wood");
			const int32 ExpectedCount = bWood ? 1 : 3;
			TestTrue(TEXT("Authored canonical data valid"), Authored->ValidateFragments(Diagnostic));
			TestTrue(TEXT("Legacy source empty"), Authored->Fragments.IsEmpty());
			const auto Before = Authored->GetResolvedFragments();
			TestEqual(TEXT("Exact serialized fragment count"), Before.Num(), ExpectedCount);
			TArray<UObject*> DirectChildren;
			GetObjectsWithOuter(Authored, DirectChildren, EGetObjectsFlags::None);
			int32 OwnedFragmentCount = 0;
			for (const UObject* Child : DirectChildren) { if (Child->IsA<UItemFragment>()) { ++OwnedFragmentCount; } }
			TestEqual(TEXT("No duplicate serialized owned templates"), OwnedFragmentCount, ExpectedCount);
			for (const UItemFragment* Fragment : Before)
			{
				TestEqual(TEXT("Inline template directly owned by definition CDO"), static_cast<const UObject*>(Fragment->GetOuter()), static_cast<const UObject*>(Authored));
				TestTrue(TEXT("Template class is static and inline-instanced"), Fragment->GetClass()->HasAllClassFlags(CLASS_Const | CLASS_DefaultToInstanced | CLASS_EditInlineNew));
			}
			if (bWood)
			{
				const auto* ResourceTemplate = Cast<UItemFragment_Resource>(Authored->FindFragmentByClass(UItemFragment_Resource::StaticClass()));
				TestTrue(TEXT("Wood resource type persisted"), ResourceTemplate && ResourceTemplate->ResourceType == TEXT("Wood"));
			}
			else
			{
				const auto* WeaponTemplate = Cast<UItemFragment_Weapon>(Authored->FindFragmentByClass(UItemFragment_Weapon::StaticClass()));
				const auto* EquipTemplate = Cast<UItemFragment_Equippable>(Authored->FindFragmentByClass(UItemFragment_Equippable::StaticClass()));
				const auto* DurabilityTemplate = Cast<UItemFragment_Durability>(Authored->FindFragmentByClass(UItemFragment_Durability::StaticClass()));
				TestTrue(TEXT("Sword weapon defaults persisted"), WeaponTemplate && WeaponTemplate->WeaponType == EWeaponType::Sword && WeaponTemplate->Damage == 10.0f && WeaponTemplate->AttackSpeed == 1.0f && WeaponTemplate->AttackRange == 150.0f);
				TestTrue(TEXT("Sword equipment defaults persisted"), EquipTemplate && EquipTemplate->EquipmentSlot == EEquipmentSlot::MainHand);
				TestTrue(TEXT("Sword maximum durability persisted"), DurabilityTemplate && DurabilityTemplate->MaxDurability == 100.0f);
			}
			const TStrongObjectPtr<UInventoryComponent> Inventory(NewObject<UInventoryComponent>());
			const auto First = Inventory->TryAddDefinition(Class);
			const auto Second = Inventory->TryAddDefinition(Class);
			TestTrue(TEXT("Authored definition acquires two distinct units"), First.IsSuccess() && Second.IsSuccess() && First.ItemId != Second.ItemId);
			TestTrue(TEXT("Runtime acquisition shares unchanged configuration templates"), Authored->GetResolvedFragments() == Before);
			DirectChildren.Reset();
			GetObjectsWithOuter(Authored, DirectChildren, EGetObjectsFlags::None);
			OwnedFragmentCount = 0;
			for (const UObject* Child : DirectChildren) { if (Child->IsA<UItemFragment>()) { ++OwnedFragmentCount; } }
			TestEqual(TEXT("Runtime acquisition creates no definition template copies"), OwnedFragmentCount, ExpectedCount);
		}
	}
	else { AddError(TEXT("Unknown M6 test case")); }
	return true;
}
#endif




