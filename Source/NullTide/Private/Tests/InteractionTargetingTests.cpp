#include "Building/BuildingCraftingStationActor.h"
#include "Building/BuildingStorageActor.h"
#include "Components/SceneComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Interaction/InteractionProbeComponent.h"
#include "Interaction/InteractionTargetingLibrary.h"
#include "Misc/AutomationTest.h"
#include "Tests/InteractionTestTypes.h"
#include "Tests/ResourceNodeTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
const TCHAR* G8InteractablePath = TEXT("/Game/LevelPrototyping/interactionSystem/BPI_Interactable.BPI_Interactable_C");
const TCHAR* G8ChestPath = TEXT("/Game/LevelPrototyping/BuildingSystem/Pieces/BP_BuildChest.BP_BuildChest_C");
const TCHAR* G8BenchPath = TEXT("/Game/LevelPrototyping/BuildingSystem/Pieces/BP_BuildWorkbench.BP_BuildWorkbench_C");
const TCHAR* G8PlayerPath = TEXT("/Game/TopDown/Blueprints/BP_TopDownCharacter.BP_TopDownCharacter");

/** A transient game world with a world context and initialized actors (the B2 fixture setup, no ground needed). */
struct FG8World
{
	UWorld* World = nullptr;

	FG8World()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (World)
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
	}

	~FG8World()
	{
		if (World)
		{
			for (AActor* Actor : FActorRange(World))
			{
				if (Actor)
				{
					Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
				}
			}
			GEngine->ShutdownWorldNetDriver(World);
			World->DestroyWorld(true);
			GEngine->DestroyWorldContext(World);
		}
	}

	/** A plain actor with a scene root at a location (a stand-in interactable for the pure selector). */
	AActor* SpawnMarker(const FVector& Location) const
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		Actor->SetActorLocation(Location);
		return Actor;
	}
};

FString G8Name(const AActor* Actor)
{
	return Actor ? Actor->GetName() : FString(TEXT("none"));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameplayG8FacingSelectionTest,
	"NullTide.Gameplay.G8.FacingSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameplayG8FacingSelectionTest::RunTest(const FString& Parameters)
{
	FG8World Fixture;
	if (!TestNotNull(TEXT("Transient world"), Fixture.World)) { return false; }
	// The viewer at the origin faces +X.
	AActor* Viewer = Fixture.SpawnMarker(FVector::ZeroVector);
	AActor* Front = Fixture.SpawnMarker(FVector(200.0, 0.0, 0.0));
	AActor* FrontFar = Fixture.SpawnMarker(FVector(400.0, 0.0, 0.0));
	AActor* Side = Fixture.SpawnMarker(FVector(100.0, 150.0, 0.0));     // about 56 degrees
	AActor* Wide = Fixture.SpawnMarker(FVector(20.0, 200.0, 0.0));      // about 84 degrees: outside 70
	AActor* Behind = Fixture.SpawnMarker(FVector(-200.0, 0.0, 0.0));
	AActor* Dying = Fixture.SpawnMarker(FVector(150.0, 0.0, 0.0));
	Dying->Destroy();

	using FSelect = UInteractionTargetingLibrary;
	TestEqual(TEXT("Nothing to choose from"), G8Name(FSelect::SelectFacingInteractable(Viewer, {})), FString(TEXT("none")));
	TestTrue(TEXT("Front beats the side"), FSelect::SelectFacingInteractable(Viewer, { Side, Front }) == Front);
	TestTrue(TEXT("Straight ahead: the nearer of two wins the tie"), FSelect::SelectFacingInteractable(Viewer, { FrontFar, Front }) == Front);
	TestTrue(TEXT("Behind and outside the cone are ignored"), FSelect::SelectFacingInteractable(Viewer, { Behind, Wide }) == nullptr);
	TestTrue(TEXT("The side object inside the cone is chosen when nothing is straight ahead"),
		FSelect::SelectFacingInteractable(Viewer, { Behind, Side, Wide }) == Side);
	TestTrue(TEXT("Null and destroyed candidates are ignored"), FSelect::SelectFacingInteractable(Viewer, { nullptr, Dying, Front }) == Front);
	TestTrue(TEXT("The viewer itself is never a candidate"), FSelect::SelectFacingInteractable(Viewer, { Viewer }) == nullptr);
	TestTrue(TEXT("A narrower cone excludes the side object"), FSelect::SelectFacingInteractable(Viewer, { Side }, 30.f) == nullptr);

	// The tie rule is order-independent: the nearest within 1 degree of the smallest angle, whatever the input order.
	AActor* TieFar = Fixture.SpawnMarker(FVector(600.0, 0.0, 0.0));                                       // 0 degrees
	AActor* TieMid = Fixture.SpawnMarker(FVector(400.0, 400.0 * FMath::Tan(FMath::DegreesToRadians(0.9)), 0.0)); // 0.9
	AActor* TieNear = Fixture.SpawnMarker(FVector(200.0, 200.0 * FMath::Tan(FMath::DegreesToRadians(1.8)), 0.0)); // 1.8
	TestTrue(TEXT("Tie order A: the nearer within 1 degree of the best wins"), FSelect::SelectFacingInteractable(Viewer, { TieFar, TieMid, TieNear }) == TieMid);
	TestTrue(TEXT("Tie order B: same result reversed"), FSelect::SelectFacingInteractable(Viewer, { TieNear, TieMid, TieFar }) == TieMid);

	// A depleted resource node advertises no prompt, so it is never the target.
	FActorSpawnParameters NodeParams;
	NodeParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AResourceNodeTestActor* Depleted = Fixture.World->SpawnActor<AResourceNodeTestActor>(FVector(100.0, 0.0, 0.0), FRotator::ZeroRotator, NodeParams);
	if (TestNotNull(TEXT("Resource node spawns"), Depleted))
	{
		Depleted->Configure(nullptr, 0);
		TestTrue(TEXT("A depleted node straight ahead is skipped"), FSelect::SelectFacingInteractable(Viewer, { Depleted, Side }) == Side);
	}

	Viewer->SetActorRotation(FRotator(0.0, 90.0, 0.0));
	TestTrue(TEXT("Turning to +Y: the side object is now the best"), FSelect::SelectFacingInteractable(Viewer, { Side, Front, Behind }) == Side);
	TestEqual(TEXT("No probe: no facing target"), G8Name(FSelect::GetFacingInteractableFor(Viewer)), FString(TEXT("none")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameplayG8ProbeAssetTest,
	"NullTide.Gameplay.G8.ProbeAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameplayG8ProbeAssetTest::RunTest(const FString& Parameters)
{
	UClass* InteractableClass = LoadClass<UInterface>(nullptr, G8InteractablePath);
	UBlueprint* PlayerBlueprint = LoadObject<UBlueprint>(nullptr, G8PlayerPath);
	if (!TestTrue(TEXT("BPI_Interactable and the player Blueprint load"), InteractableClass && PlayerBlueprint)) { return false; }
	TArray<const UInteractionProbeComponent*> Probes;
	if (const USimpleConstructionScript* ConstructionScript = PlayerBlueprint->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : ConstructionScript->GetAllNodes())
		{
			if (const UInteractionProbeComponent* Probe = Node ? Cast<UInteractionProbeComponent>(Node->ComponentTemplate) : nullptr)
			{
				Probes.Add(Probe);
			}
		}
	}
	if (!TestEqual(TEXT("The player has exactly one InteractionProbe"), Probes.Num(), 1)) { return false; }
	const UInteractionProbeComponent* Probe = Probes[0];
	TestTrue(TEXT("Named InteractionProbe, looking for BPI_Interactable"), Probe->GetName().StartsWith(TEXT("InteractionProbe"))
		&& Probe->InteractableInterface == InteractableClass);
	TestTrue(FString::Printf(TEXT("In front of the capsule (%s), radius %.0f, cone %.0f, refresh %.2f s"), *Probe->GetRelativeLocation().ToString(),
		Probe->GetUnscaledSphereRadius(), Probe->FacingConeDegrees, Probe->RefreshInterval),
		Probe->GetRelativeLocation().X > 0.0 && Probe->GetUnscaledSphereRadius() > 0.f && FMath::IsNearlyEqual(Probe->FacingConeDegrees, 70.f)
		&& FMath::IsNearlyEqual(Probe->RefreshInterval, 0.1f));
	TestTrue(TEXT("No collision of its own, no overlap events, never affects navigation"),
		Probe->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Probe->GetGenerateOverlapEvents() && !Probe->CanEverAffectNavigation());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameplayG8TwoInteractablesInFrontTest,
	"NullTide.Gameplay.G8.TwoInteractablesInFront",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameplayG8TwoInteractablesInFrontTest::RunTest(const FString& Parameters)
{
	UClass* InteractableClass = LoadClass<UInterface>(nullptr, G8InteractablePath);
	UClass* ChestClass = LoadClass<ABuildingStorageActor>(nullptr, G8ChestPath);
	UClass* BenchClass = LoadClass<ABuildingCraftingStationActor>(nullptr, G8BenchPath);
	if (!TestTrue(TEXT("BPI_Interactable, BP_BuildChest and BP_BuildWorkbench load"), InteractableClass && ChestClass && BenchClass)) { return false; }
	FG8World Fixture;
	if (!TestNotNull(TEXT("Transient world"), Fixture.World)) { return false; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// A chest and a workbench one cell apart, both within reach of a character between them.
	AActor* Chest = Fixture.World->SpawnActor<AActor>(ChestClass, FVector(100.0, 100.0, 0.0), FRotator::ZeroRotator, Params);
	AActor* Bench = Fixture.World->SpawnActor<AActor>(BenchClass, FVector(100.0, -100.0, 0.0), FRotator::ZeroRotator, Params);
	ACharacter* Character = Fixture.World->SpawnActor<ACharacter>(ACharacter::StaticClass(), FVector(0.0, 0.0, 92.0), FRotator::ZeroRotator, Params);
	if (!TestTrue(TEXT("Chest, bench and character spawn"), Chest && Bench && Character)) { return false; }
	UInteractionProbeComponent* Probe = NewObject<UInteractionProbeComponent>(Character, TEXT("InteractionProbe"));
	Probe->InteractableInterface = InteractableClass;
	Probe->SetupAttachment(Character->GetRootComponent());
	Character->AddInstanceComponent(Probe);
	Probe->RegisterComponent();

	auto FaceYaw = [Character, Probe](const double Yaw)
	{
		Character->SetActorRotation(FRotator(0.0, Yaw, 0.0));
		return Probe->GetFacingInteractable();
	};
	TestTrue(FString::Printf(TEXT("Both interaction volumes overlap the probe ahead (%d candidates)"), Probe->GetInteractableCandidates().Num()),
		FaceYaw(0.0) != nullptr && Probe->GetInteractableCandidates().Num() == 2);
	TestTrue(TEXT("Facing the chest (+45 degrees) targets the chest"), FaceYaw(45.0) == Chest);
	TestTrue(TEXT("Facing the bench (-45 degrees) targets the bench"), FaceYaw(-45.0) == Bench);
	TestTrue(FString::Printf(TEXT("Facing away, nothing is targeted (%s)"), *G8Name(FaceYaw(180.0))), FaceYaw(180.0) == nullptr);
	TestTrue(TEXT("The library reads the same target through the probe"), FaceYaw(45.0) == UInteractionTargetingLibrary::GetFacingInteractableFor(Character));

	// An interactable attached to the character (an equipped item's actor) is never a candidate.
	AActor* Carried = Fixture.World->SpawnActor<AActor>(ChestClass, FVector(150.0, 0.0, 0.0), FRotator::ZeroRotator, Params);
	if (TestNotNull(TEXT("Carried interactable spawns"), Carried))
	{
		Carried->AttachToActor(Character, FAttachmentTransformRules::KeepWorldTransform);
		TestFalse(TEXT("An attached interactable is not a candidate"), Probe->GetInteractableCandidates().Contains(Carried));
		Carried->Destroy();
	}

	// The refresh broadcasts exactly once per change, with the new target as the payload.
	UInteractionTestListener* Listener = NewObject<UInteractionTestListener>();
	Probe->OnFacingInteractableChanged.AddDynamic(Listener, &UInteractionTestListener::HandleFacingInteractableChanged);
	FaceYaw(45.0);
	Probe->RefreshFacingInteractable();
	Probe->RefreshFacingInteractable();
	TestTrue(TEXT("Chest: one broadcast; the repeat refresh is silent"), Listener->Targets.Num() == 1 && Listener->Targets[0].Get() == Chest);
	FaceYaw(-45.0);
	Probe->RefreshFacingInteractable();
	TestTrue(TEXT("Turning to the bench: one more broadcast with the bench"), Listener->Targets.Num() == 2 && Listener->Targets[1].Get() == Bench);
	Bench->Destroy();
	Probe->RefreshFacingInteractable();
	TestTrue(FString::Printf(TEXT("The destroyed target is reported as null (%d broadcasts)"), Listener->Targets.Num()),
		Listener->Targets.Num() == 3 && Listener->Targets[2].IsExplicitlyNull() && Probe->GetLastFacingInteractable() == nullptr);
	Probe->RefreshFacingInteractable();
	TestEqual(TEXT("Nothing in front stays silent"), Listener->Targets.Num(), 3);
	return true;
}

#endif
