#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Player/CursorFacingComponent.h"
#include "Tests/PlayerTestTypes.h"
#include "UI/GameInputMode.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
const TCHAR* G10PlayerPath = TEXT("/Game/TopDown/Blueprints/BP_TopDownCharacter.BP_TopDownCharacter");
const TCHAR* G10PlayerClassPath = TEXT("/Game/TopDown/Blueprints/BP_TopDownCharacter.BP_TopDownCharacter_C");
const TCHAR* G10ControllerClassPath = TEXT("/Game/TopDown/Blueprints/BP_TopDownController.BP_TopDownController_C");

/** A transient game world with a world context and initialized actors (the G8 fixture). */
struct FG10World
{
	UWorld* World = nullptr;

	FG10World()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (World)
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
	}

	~FG10World()
	{
		if (World)
		{
			GEngine->ShutdownWorldNetDriver(World);
			World->DestroyWorld(true);
			GEngine->DestroyWorldContext(World);
		}
	}
};

/** The camera looks down at the ground from above and slightly behind, like the top-down camera. */
bool G10Yaw(const FVector& Feet, const FVector& GroundPoint, float& OutYaw)
{
	const FVector Camera = GroundPoint + FVector(-600.0, -600.0, 1000.0);
	return UCursorFacingComponent::ComputeFacingYaw(Feet, Camera, (GroundPoint - Camera).GetSafeNormal(), 40.f, OutYaw);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameplayG10FacingMathTest,
	"NullTide.Gameplay.G10.FacingMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameplayG10FacingMathTest::RunTest(const FString& Parameters)
{
	const FVector Feet(100.0, 200.0, 0.0);
	float Yaw = 0.f;
	TestTrue(TEXT("Cursor ahead on +X: yaw 0"), G10Yaw(Feet, Feet + FVector(300.0, 0.0, 0.0), Yaw) && FMath::IsNearlyEqual(Yaw, 0.f, 0.01f));
	TestTrue(TEXT("Cursor on +Y: yaw 90"), G10Yaw(Feet, Feet + FVector(0.0, 300.0, 0.0), Yaw) && FMath::IsNearlyEqual(Yaw, 90.f, 0.01f));
	TestTrue(TEXT("Cursor behind on -X: yaw 180"), G10Yaw(Feet, Feet + FVector(-300.0, 0.0, 0.0), Yaw) && FMath::IsNearlyEqual(FMath::Abs(Yaw), 180.f, 0.01f));
	TestTrue(TEXT("Cursor on -Y: yaw -90"), G10Yaw(Feet, Feet + FVector(0.0, -300.0, 0.0), Yaw) && FMath::IsNearlyEqual(Yaw, -90.f, 0.01f));
	TestTrue(TEXT("Diagonal: yaw 45"), G10Yaw(Feet, Feet + FVector(200.0, 200.0, 0.0), Yaw) && FMath::IsNearlyEqual(Yaw, 45.f, 0.01f));
	TestFalse(TEXT("Inside the dead zone: keep the facing"), G10Yaw(Feet, Feet + FVector(20.0, 20.0, 0.0), Yaw));

	// The plane is the feet's: a raised pawn (on a plateau) aims on its own height.
	const FVector RaisedFeet(0.0, 0.0, 200.0);
	TestTrue(TEXT("A raised pawn aims on its own height"), G10Yaw(RaisedFeet, RaisedFeet + FVector(0.0, 300.0, 0.0), Yaw)
		&& FMath::IsNearlyEqual(Yaw, 90.f, 0.01f));

	TestFalse(TEXT("A horizontal ray never meets the plane"),
		UCursorFacingComponent::ComputeFacingYaw(Feet, FVector(0.0, 0.0, 500.0), FVector(1.0, 0.0, 0.0), 40.f, Yaw));
	TestFalse(TEXT("A ray pointing away from the plane is ignored"),
		UCursorFacingComponent::ComputeFacingYaw(Feet, FVector(0.0, 0.0, 500.0), FVector(0.0, 0.0, 1.0), 40.f, Yaw));

	// Screen dead zone (S1 review): the cursor over the character's body keeps the facing.
	const FVector2D CharacterScreen(640.0, 360.0);
	TestTrue(TEXT("Cursor on the character's body (30 px): keep the facing"),
		UCursorFacingComponent::IsWithinScreenDeadZone(CharacterScreen, FVector2D(660.0, 338.0), 48.f));
	TestFalse(TEXT("Cursor away from the body (100 px): face it"),
		UCursorFacingComponent::IsWithinScreenDeadZone(CharacterScreen, FVector2D(740.0, 360.0), 48.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameplayG10ProductionCharacterTest,
	"NullTide.Gameplay.G10.ProductionCharacter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameplayG10ProductionCharacterTest::RunTest(const FString& Parameters)
{
	const UBlueprint* PlayerBlueprint = LoadObject<UBlueprint>(nullptr, G10PlayerPath);
	UClass* PlayerClass = LoadClass<ACharacter>(nullptr, G10PlayerClassPath);
	UClass* ControllerClass = LoadClass<APlayerController>(nullptr, G10ControllerClassPath);
	if (!TestTrue(TEXT("BP_TopDownCharacter and BP_TopDownController load"), PlayerBlueprint && PlayerClass && ControllerClass)) { return false; }

	TArray<const UCursorFacingComponent*> Facing;
	if (const USimpleConstructionScript* ConstructionScript = PlayerBlueprint->SimpleConstructionScript)
	{
		for (const USCS_Node* Node : ConstructionScript->GetAllNodes())
		{
			if (const UCursorFacingComponent* Component = Node ? Cast<UCursorFacingComponent>(Node->ComponentTemplate) : nullptr)
			{
				Facing.Add(Component);
			}
		}
	}
	if (TestEqual(TEXT("The player has exactly one CursorFacingComponent"), Facing.Num(), 1))
	{
		TestTrue(TEXT("Dead zones: 40 cm on the ground, 48 px on screen"), FMath::IsNearlyEqual(Facing[0]->DeadZone, 40.f)
			&& FMath::IsNearlyEqual(Facing[0]->DeadZonePixels, 48.f));
	}

	const ACharacter* Character = PlayerClass->GetDefaultObject<ACharacter>();
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	TestTrue(FString::Printf(TEXT("Turns to the controller, not to movement, at 1080 deg/s (yaw rate %.0f)"),
			Movement ? Movement->RotationRate.Yaw : -1.0),
		Movement && !Movement->bOrientRotationToMovement && Movement->bUseControllerDesiredRotation
		&& FMath::IsNearlyEqual(Movement->RotationRate.Yaw, 1080.0) && Character && !Character->bUseControllerRotationYaw);
	TestTrue(TEXT("BP_TopDownController shows the cursor by default"), ControllerClass->GetDefaultObject<APlayerController>()->bShowMouseCursor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameplayG10CursorRestoreAndTickGateTest,
	"NullTide.Gameplay.G10.CursorRestoreAndTickGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameplayG10CursorRestoreAndTickGateTest::RunTest(const FString& Parameters)
{
	UClass* ControllerClass = LoadClass<APlayerController>(nullptr, G10ControllerClassPath);
	if (!TestNotNull(TEXT("BP_TopDownController loads"), ControllerClass)) { return false; }
	FG10World Fixture;
	if (!TestNotNull(TEXT("Transient world"), Fixture.World)) { return false; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// A closed screen restores the controller class's default cursor (shown) instead of hiding it.
	APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>(ControllerClass, FTransform::Identity, Params);
	if (!TestNotNull(TEXT("Controller spawns"), Controller)) { return false; }
	Controller->SetShowMouseCursor(false);
	NullTideInput::RestoreGameplayCursor(*Controller);
	TestTrue(TEXT("The gameplay cursor is visible again"), Controller->bShowMouseCursor);

	// The component ticks only for a locally player-controlled pawn: never without a controller, and never for a
	// controller with no local player (NPCs, tests).
	ACharacter* Pawn = Fixture.World->SpawnActor<ACharacter>(ACharacter::StaticClass(), FTransform(FVector(0.0, 0.0, 100.0)), Params);
	if (!TestNotNull(TEXT("Pawn spawns"), Pawn)) { return false; }
	UCursorFacingComponent* Facing = NewObject<UCursorFacingComponent>(Pawn);
	Pawn->AddInstanceComponent(Facing);
	Facing->RegisterComponent();
	Pawn->DispatchBeginPlay();
	TestTrue(TEXT("The component began play"), Facing->HasBegunPlay());
	TestFalse(TEXT("No controller: no tick"), Facing->IsComponentTickEnabled());
	Controller->Possess(Pawn);
	TestTrue(TEXT("Possessed by a player controller without a local player"), Pawn->GetController() == Controller && !Controller->GetLocalPlayer());
	TestFalse(TEXT("A controller without a local player: still no tick"), Facing->IsComponentTickEnabled());
	const FRotator Before = Controller->GetControlRotation();
	Facing->TickComponent(0.016f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("A forced tick changes nothing without a local player"), Controller->GetControlRotation().Equals(Before));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameplayG10TickWhenPlayerControlledTest,
	"NullTide.Gameplay.G10.TickWhenPlayerControlled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameplayG10TickWhenPlayerControlledTest::RunTest(const FString& Parameters)
{
	// The positive side of the gate (S1 review), with the local-player check forced through the test seam.
	FG10World Fixture;
	if (!TestNotNull(TEXT("Transient world"), Fixture.World)) { return false; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>(APlayerController::StaticClass(), FTransform::Identity, Params);
	auto SpawnPawn = [&Fixture, &Params](const FVector& Location)
	{
		ACharacter* Pawn = Fixture.World->SpawnActor<ACharacter>(ACharacter::StaticClass(), FTransform(Location), Params);
		UCursorFacingTestComponent* Facing = Pawn ? NewObject<UCursorFacingTestComponent>(Pawn) : nullptr;
		if (Facing)
		{
			Pawn->AddInstanceComponent(Facing);
			Facing->RegisterComponent();
		}
		return TPair<ACharacter*, UCursorFacingTestComponent*>(Pawn, Facing);
	};

	// Possessed after BeginPlay (the controller-changed delegate turns the tick on).
	const TPair<ACharacter*, UCursorFacingTestComponent*> First = SpawnPawn(FVector(0.0, 0.0, 100.0));
	if (!TestTrue(TEXT("Controller and pawn spawn"), Controller && First.Key && First.Value)) { return false; }
	First.Key->DispatchBeginPlay();
	TestFalse(TEXT("Not possessed yet: no tick"), First.Value->IsComponentTickEnabled());
	Controller->Possess(First.Key);
	TestTrue(TEXT("Possessed by a local player controller: ticks"), First.Value->IsComponentTickEnabled());
	const UCharacterMovementComponent* Movement = First.Key->GetCharacterMovement();
	TestTrue(TEXT("The movement component ticks after the facing (same-frame turn)"), Movement
		&& Movement->PrimaryComponentTick.GetPrerequisites().ContainsByPredicate([&First](const FTickPrerequisite& Prerequisite)
		{
			return Prerequisite.PrerequisiteTickFunction == &First.Value->PrimaryComponentTick;
		}));
	Controller->UnPossess();
	TestFalse(TEXT("Unpossessed: no tick"), First.Value->IsComponentTickEnabled());

	// Possessed before BeginPlay (the normal game start): BeginPlay turns the tick on.
	const TPair<ACharacter*, UCursorFacingTestComponent*> Second = SpawnPawn(FVector(500.0, 0.0, 100.0));
	if (!TestTrue(TEXT("Second pawn spawns"), Second.Key && Second.Value)) { return false; }
	Controller->Possess(Second.Key);
	Second.Key->DispatchBeginPlay();
	TestTrue(TEXT("Already possessed at BeginPlay: ticks"), Second.Value->IsComponentTickEnabled());

	// A controller that is not a local player's never ticks it.
	Second.Value->bTreatAsLocal = false;
	Controller->UnPossess();
	Controller->Possess(Second.Key);
	TestFalse(TEXT("Not a local player's controller: no tick"), Second.Value->IsComponentTickEnabled());
	return true;
}

#endif
