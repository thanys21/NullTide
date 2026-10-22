#include "NpcTestTypes.h"

#include "Base/BaseZoneActor.h"
#include "Base/BaseZoneComponent.h"
#include "Base/BaseZoneSubsystem.h"
#include "Building/BuildingSubsystem.h"
#include "Components/Button.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Npc/NpcSubsystem.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
const TCHAR* S4TraderDefinitionPath = TEXT("/Game/LevelPrototyping/NpcSystem/Definitions/NPC_Trader.NPC_Trader_C");
const TCHAR* S4TraderClassPath = TEXT("/Game/LevelPrototyping/NpcSystem/BP_Npc_Trader.BP_Npc_Trader_C");
const TCHAR* S4TalkClassPath = TEXT("/Game/LevelPrototyping/NpcSystem/UI/W_NpcTalk.W_NpcTalk_C");
const TCHAR* S4InteractablePath = TEXT("/Game/LevelPrototyping/interactionSystem/BPI_Interactable.BPI_Interactable_C");
const TCHAR* S4QuinnPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple");
const TCHAR* S4AnimClassPath = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C");

/** A transient game world with a world context and initialized actors, but not begun play (timers never fire). */
struct FS4World
{
	UWorld* World = nullptr;

	FS4World()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (World)
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
	}

	~FS4World()
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

	bool IsValid(FAutomationTestBase& Test) const
	{
		return Test.TestTrue(TEXT("S4 test world with zone and NPC subsystems"), World && World->GetSubsystem<UBaseZoneSubsystem>()
			&& World->GetSubsystem<UNpcSubsystem>());
	}

	/** A zone of Size cells centred on CenterCell, configured before construction and registered like a placed one. */
	ABaseZoneActor* SpawnZone(const FIntPoint& CenterCell, const FIntPoint& Size, const TArray<TSubclassOf<UNpcDefinition>>& Residents,
		bool bAllowsBuilding = true) const
	{
		const FTransform Transform(UBuildingSubsystem::CellToWorldCenter(CenterCell));
		ABaseZoneActor* Zone = World->SpawnActorDeferred<ABaseZoneActor>(ABaseZoneActor::StaticClass(), Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Zone)
		{
			Zone->GetZone()->SizeInCells = Size;
			Zone->GetZone()->Residents = Residents;
			Zone->GetZone()->bAllowsBuilding = bAllowsBuilding;
			Zone->FinishSpawning(Transform);
			Zone->DispatchBeginPlay();
		}
		return Zone;
	}

	ACharacter* SpawnCharacter(const FVector& Location) const
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<ACharacter>(ACharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
	}
};

bool S4InInterior(const ANpcCharacter* Npc, const ABaseZoneActor* Zone)
{
	return Npc && Zone && Zone->GetZone()->GetBuildableCells().Contains(UBuildingSubsystem::WorldToCell(Npc->GetActorLocation()));
}

FString S4CallEnumGetter(UObject* Target, UFunction* Function)
{
	TArray<uint8> Buffer;
	Buffer.SetNumZeroed(FMath::Max<int32>(Function->ParmsSize, 1));
	Target->ProcessEvent(Function, Buffer.GetData());
	const FProperty* Return = Function->GetReturnProperty();
	for (TFieldIterator<FProperty> It(Function); It && !Return; ++It)
	{
		Return = It->HasAnyPropertyFlags(CPF_OutParm) ? *It : nullptr;
	}
	if (const FByteProperty* Byte = CastField<FByteProperty>(Return); Byte && Byte->Enum)
	{
		return Byte->Enum->GetDisplayNameTextByValue(*Byte->ContainerPtrToValuePtr<uint8>(Buffer.GetData())).ToString();
	}
	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Return))
	{
		const int64 Value = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(Buffer.GetData()));
		return EnumProperty->GetEnum()->GetDisplayNameTextByValue(Value).ToString();
	}
	return FString();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNpcS4DefinitionValidationTest,
	"NullTide.Npc.S4.DefinitionValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNpcS4DefinitionValidationTest::RunTest(const FString& Parameters)
{
	FString Diagnostic;
	TestTrue(TEXT("The test definition is valid"), GetDefault<UNpcTestDefinition>()->ValidateNpc(Diagnostic));

	struct FCase { const TCHAR* Name; TFunction<void(UNpcDefinition&)> Break; };
	const FCase Cases[] = {
		{ TEXT("No name"), [](UNpcDefinition& D) { D.NpcName = FText::GetEmpty(); } },
		{ TEXT("No greetings"), [](UNpcDefinition& D) { D.GreetingLines.Reset(); } },
		{ TEXT("An empty greeting"), [](UNpcDefinition& D) { D.GreetingLines.Add(FText::GetEmpty()); } },
		{ TEXT("No actor class"), [](UNpcDefinition& D) { D.NpcActorClass = nullptr; } },
		{ TEXT("An abstract actor class"), [](UNpcDefinition& D) { D.NpcActorClass = ANpcCharacter::StaticClass(); } },
		{ TEXT("Zero speed"), [](UNpcDefinition& D) { D.WalkSpeed = 0.f; } },
		{ TEXT("Negative idle time"), [](UNpcDefinition& D) { D.IdleTimeMin = -1.f; } },
		{ TEXT("Idle range reversed"), [](UNpcDefinition& D) { D.IdleTimeMin = 5.f; D.IdleTimeMax = 2.f; } },
	};
	for (const FCase& Case : Cases)
	{
		UNpcTestDefinition* Definition = NewObject<UNpcTestDefinition>();
		Case.Break(*Definition);
		const bool bValid = Definition->ValidateNpc(Diagnostic);
		TestTrue(FString::Printf(TEXT("%s is refused (%s)"), Case.Name, *Diagnostic), !bValid && !Diagnostic.IsEmpty());
	}
	TestNull(TEXT("A broken definition does not resolve"), UNpcDefinition::ResolveValidNpc(UNpcBrokenTestDefinition::StaticClass(), Diagnostic));
	TestNull(TEXT("The abstract root does not resolve"), UNpcDefinition::ResolveValidNpc(UNpcDefinition::StaticClass(), Diagnostic));

	UClass* TraderDefinition = LoadClass<UNpcDefinition>(nullptr, S4TraderDefinitionPath);
	const UNpcDefinition* Trader = UNpcDefinition::ResolveValidNpc(TraderDefinition, Diagnostic);
	TestTrue(FString::Printf(TEXT("NPC_Trader is valid (%s)"), *Diagnostic), Trader != nullptr);
	if (Trader)
	{
		TestTrue(TEXT("NPC_Trader: named Trader, with greetings"), Trader->NpcName.ToString() == TEXT("Trader") && Trader->GreetingLines.Num() >= 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNpcS4ResidentsTest,
	"NullTide.Npc.S4.Residents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNpcS4ResidentsTest::RunTest(const FString& Parameters)
{
	FS4World Fixture;
	if (!Fixture.IsValid(*this))
	{
		return false;
	}
	UNpcSubsystem* Npcs = Fixture.World->GetSubsystem<UNpcSubsystem>();
	UBaseZoneSubsystem* Zones = Fixture.World->GetSubsystem<UBaseZoneSubsystem>();
	const TSubclassOf<UNpcDefinition> Tester = UNpcTestDefinition::StaticClass();

	// The broken resident is reported once.
	AddExpectedMessagePlain(TEXT("resident 1 skipped"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	ABaseZoneActor* Camp = Fixture.SpawnZone(FIntPoint(0, 0), FIntPoint(5, 7), { Tester, UNpcBrokenTestDefinition::StaticClass(), Tester }, false);
	ABaseZoneActor* Home = Fixture.SpawnZone(FIntPoint(20, 0), FIntPoint(5, 5), {});
	if (!TestTrue(TEXT("Zones spawned"), Camp && Home))
	{
		return false;
	}
	const TArray<ANpcCharacter*> Residents = Npcs->GetResidents(Camp->GetZone());
	TestEqual(TEXT("Two valid residents spawn; the broken one is skipped"), Residents.Num(), 2);
	bool bAllHome = Residents.Num() == 2;
	for (const ANpcCharacter* Npc : Residents)
	{
		bAllHome &= S4InInterior(Npc, Camp) && Npc->GetHomeZone() == Camp->GetZone() && Npc->GetNpcDefinition() == Tester
			&& Cast<ANpcTestAIController>(Npc->GetController()) && Npc->GetNameLabel()->Text.ToString() == TEXT("Tester");
	}
	TestTrue(TEXT("Each stands in the camp's interior, knows its home and kind, is AI-controlled and labelled"), bAllHome);
	TestTrue(TEXT("A zone without residents has none"), Npcs->GetResidents(Home->GetZone()).IsEmpty());

	Zones->RegisterZone(Camp->GetZone());
	TestEqual(TEXT("Registering again spawns nobody new"), Npcs->GetResidents(Camp->GetZone()).Num(), 2);

	// The camp is a zone but never buildable, and shows no ring.
	const FIntPoint Inside = UBuildingSubsystem::WorldToCell(Camp->GetActorLocation());
	TestTrue(TEXT("The camp is a zone, not buildable"), Zones->FindZoneAt(Camp->GetActorLocation()) == Camp->GetZone()
		&& Zones->IsCellInZone(Inside) && !Zones->IsCellBuildable(Inside));
	bool bRingHidden = true;
	for (const UStaticMeshComponent* Strip : Camp->GetRingStrips())
	{
		bRingHidden &= !Strip->IsVisible();
	}
	bool bRingShown = true;
	for (const UStaticMeshComponent* Strip : Home->GetRingStrips())
	{
		bRingShown &= Strip->IsVisible();
	}
	TestTrue(TEXT("The camp hides its ring; a building zone shows it"), bRingHidden && bRingShown);
	TestTrue(TEXT("The building zone stays buildable"), Zones->IsCellBuildable(UBuildingSubsystem::WorldToCell(Home->GetActorLocation())));

	const TWeakObjectPtr<ANpcCharacter> First = Residents.IsEmpty() ? nullptr : Residents[0];
	Camp->Destroy();
	TestTrue(TEXT("Destroying the zone removes its residents"), !First.IsValid() && Npcs->GetResidents(Camp->GetZone()).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNpcS4WanderTest,
	"NullTide.Npc.S4.Wander",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNpcS4WanderTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("The NPC controller never ticks"), GetDefault<ANpcAIController>()->PrimaryActorTick.bCanEverTick);
	TestFalse(TEXT("NPCs never tick"), GetDefault<ANpcTestCharacter>()->PrimaryActorTick.bCanEverTick);

	FS4World Fixture;
	if (!Fixture.IsValid(*this))
	{
		return false;
	}
	ABaseZoneActor* Camp = Fixture.SpawnZone(FIntPoint(0, 0), FIntPoint(5, 7), { UNpcTestDefinition::StaticClass() }, false);
	const TArray<ANpcCharacter*> Residents = Camp ? Fixture.World->GetSubsystem<UNpcSubsystem>()->GetResidents(Camp->GetZone())
		: TArray<ANpcCharacter*>();
	ANpcCharacter* Npc = Residents.IsEmpty() ? nullptr : Residents[0];
	ANpcTestAIController* AI = Npc ? Cast<ANpcTestAIController>(Npc->GetController()) : nullptr;
	if (!TestTrue(TEXT("A resident with the test controller"), AI != nullptr))
	{
		return false;
	}
	const FIntRect Interior = Camp->GetZone()->GetBuildableCells();
	TestTrue(TEXT("Possessed: idle with a timer"), AI->GetNpcState() == ENpcAIState::Idle && AI->IsIdleTimerActive());

	AI->SetWanderSeed(42);
	TArray<FIntPoint> First;
	int32 Outside = 0;
	for (int32 Index = 0; Index < 200; ++Index)
	{
		FIntPoint Cell;
		Outside += AI->ChooseWanderCell(Cell) && Interior.Contains(Cell) ? 0 : 1;
		if (Index < 20)
		{
			First.Add(Cell);
		}
	}
	TestEqual(TEXT("200 wander cells, all in the camp's interior"), Outside, 0);
	AI->SetWanderSeed(42);
	bool bSame = true;
	for (const FIntPoint& Expected : First)
	{
		FIntPoint Cell;
		bSame &= AI->ChooseWanderCell(Cell) && Cell == Expected;
	}
	TestTrue(TEXT("The same seed gives the same cells"), bSame);

	AI->TestStartWander();
	TestTrue(TEXT("Wander: one move into the interior"), AI->GetNpcState() == ENpcAIState::Moving && AI->MoveRequests.Num() == 1
		&& Interior.Contains(UBuildingSubsystem::WorldToCell(AI->MoveRequests[0])) && !AI->IsIdleTimerActive());
	AI->TestFinishMove();
	TestTrue(TEXT("Move finished: idle again with a timer"), AI->GetNpcState() == ENpcAIState::Idle && AI->IsIdleTimerActive());

	AI->bProjectSucceeds = false;
	AI->ProjectCalls = 0;
	AI->TestStartWander();
	TestTrue(TEXT("No reachable point: 5 tries, then idle"), AI->ProjectCalls == ANpcAIController::MaxWanderAttempts
		&& AI->MoveRequests.Num() == 1 && AI->GetNpcState() == ENpcAIState::Idle && AI->IsIdleTimerActive());
	AI->bProjectSucceeds = true;
	AI->bMoveSucceeds = false;
	AI->TestStartWander();
	TestTrue(TEXT("Moves refused: idle"), AI->MoveRequests.Num() == 1 && AI->GetNpcState() == ENpcAIState::Idle && AI->IsIdleTimerActive());
	AI->bMoveSucceeds = true;

	// A request that finishes synchronously and fails (already at the goal) must not arm the idle timer for the walk
	// the next attempt starts.
	AI->bFinishSynchronously = true;
	AI->FailNextMoves = 1;
	AI->TestStartWander();
	TestTrue(TEXT("Synchronous finish, then a real move: walking with no idle timer"), AI->GetNpcState() == ENpcAIState::Moving
		&& AI->MoveRequests.Num() == 2 && !AI->IsIdleTimerActive());
	AI->bFinishSynchronously = false;
	AI->TestFinishMove();

	// A projection that drifts out of the interior is never used.
	AI->ProjectOffset = FVector(5000.0, 0.0, 0.0);
	AI->TestStartWander();
	TestTrue(TEXT("Projected outside the interior: no move, idle"), AI->MoveRequests.Num() == 2 && AI->GetNpcState() == ENpcAIState::Idle
		&& AI->IsIdleTimerActive());
	AI->ProjectOffset = FVector::ZeroVector;

	ACharacter* Partner = Fixture.SpawnCharacter(Npc->GetActorLocation() + FVector(0.0, 200.0, 0.0));
	AI->TestStartWander();
	AI->BeginTalk(Partner);
	TestTrue(TEXT("Talking: no timer, facing the partner"), AI->GetNpcState() == ENpcAIState::Talking && !AI->IsIdleTimerActive()
		&& FMath::IsNearlyEqual(Npc->GetActorRotation().Yaw, 90.0, 1.0));
	const int32 Moves = AI->MoveRequests.Num();
	AI->TestStartWander();
	AI->TestFinishMove();
	TestTrue(TEXT("While talking nothing restarts"), AI->MoveRequests.Num() == Moves && AI->GetNpcState() == ENpcAIState::Talking);
	AI->ResumeWander();
	TestTrue(TEXT("Resumed: idle with a timer"), AI->GetNpcState() == ENpcAIState::Idle && AI->IsIdleTimerActive());

	// An NPC without a home zone never wanders.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ANpcTestCharacter* Stray = Fixture.World->SpawnActor<ANpcTestCharacter>(ANpcTestCharacter::StaticClass(), FVector(5000.0, 0.0, 100.0),
		FRotator::ZeroRotator, Params);
	ANpcTestAIController* StrayAI = Stray ? Cast<ANpcTestAIController>(Stray->GetController()) : nullptr;
	if (TestNotNull(TEXT("A stray NPC is AI-controlled"), StrayAI))
	{
		StrayAI->TestStartWander();
		TestTrue(TEXT("Without a home zone: no timer, no move"), !StrayAI->IsIdleTimerActive() && StrayAI->MoveRequests.IsEmpty()
			&& StrayAI->ProjectCalls == 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNpcS4TalkFlowTest,
	"NullTide.Npc.S4.TalkFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNpcS4TalkFlowTest::RunTest(const FString& Parameters)
{
	UClass* TalkClass = LoadClass<UNpcTalkWidget>(nullptr, S4TalkClassPath);
	FS4World Fixture;
	if (!TestNotNull(TEXT("W_NpcTalk loads"), TalkClass) || !Fixture.IsValid(*this))
	{
		return false;
	}
	ABaseZoneActor* Camp = Fixture.SpawnZone(FIntPoint(0, 0), FIntPoint(5, 7), { UNpcTestDefinition::StaticClass() }, false);
	const TArray<ANpcCharacter*> Residents = Camp ? Fixture.World->GetSubsystem<UNpcSubsystem>()->GetResidents(Camp->GetZone())
		: TArray<ANpcCharacter*>();
	ANpcTestCharacter* Npc = Residents.IsEmpty() ? nullptr : Cast<ANpcTestCharacter>(Residents[0]);
	ANpcTestAIController* AI = Npc ? Cast<ANpcTestAIController>(Npc->GetController()) : nullptr;
	ACharacter* Player = Fixture.SpawnCharacter(Npc ? Npc->GetActorLocation() + FVector(-200.0, 0.0, 0.0) : FVector::ZeroVector);
	ACharacter* Loner = Fixture.SpawnCharacter(Npc ? Npc->GetActorLocation() + FVector(0.0, -150.0, 0.0) : FVector::ZeroVector);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>(APlayerController::StaticClass(), FTransform::Identity, Params);
	if (!TestTrue(TEXT("NPC, AI, player and controller"), Npc && AI && Player && Loner && Controller))
	{
		return false;
	}
	Controller->Possess(Player);
	Npc->SetTalkScreenClass(TalkClass);
	// Begun play like in a level, so destroying it routes EndPlay.
	Npc->DispatchBeginPlay();

	auto NothingChanged = [this, Npc, AI](const TCHAR* What, const ENpcTalkResult Expected, AActor* Interactor)
	{
		const ENpcTalkResult Result = Npc->GetTalkResult(Interactor);
		const bool bOpened = Npc->OpenTalkForInteractor(Interactor);
		TestTrue(FString::Printf(TEXT("%s: refused as %s, nothing changed"), What,
				*StaticEnum<ENpcTalkResult>()->GetNameStringByValue(static_cast<int64>(Result))),
			Result == Expected && !bOpened && !Npc->IsTalking() && Npc->ShowCount == 0 && AI->GetNpcState() == ENpcAIState::Idle);
	};
	NothingChanged(TEXT("No interactor"), ENpcTalkResult::InvalidInteractor, nullptr);
	NothingChanged(TEXT("An interactor that is not a pawn"), ENpcTalkResult::InvalidInteractor, Camp);
	NothingChanged(TEXT("A pawn without a player controller"), ENpcTalkResult::NoPlayerController, Loner);
	Player->SetActorLocation(Npc->GetActorLocation() + FVector(-400.0, 0.0, 0.0));
	NothingChanged(TEXT("Out of range"), ENpcTalkResult::OutOfRange, Player);
	Player->SetActorLocation(Npc->GetActorLocation() + FVector(-200.0, 0.0, 0.0));
	Npc->SetTalkScreenClass(nullptr);
	NothingChanged(TEXT("No talk screen"), ENpcTalkResult::NoTalkScreen, Player);
	Npc->SetTalkScreenClass(TalkClass);

	TestTrue(TEXT("In range: opens"), Npc->OpenTalkForInteractor(Player));
	UNpcTalkWidget* Panel = Npc->GetTalkWidget();
	const FString Greeting = Panel ? Panel->GetGreetingText().ToString() : FString();
	TestTrue(TEXT("The panel shows the name and one of the greetings, and was shown once"), Panel && Panel->IsTalkOpen()
		&& Panel->GetNpcNameText().ToString() == TEXT("Tester") && (Greeting == TEXT("Hello") || Greeting == TEXT("Hi")) && Npc->ShowCount == 1);
	TestTrue(TEXT("The NPC stops and faces the player"), AI->GetNpcState() == ENpcAIState::Talking
		&& FMath::IsNearlyEqual(FMath::Abs(Npc->GetActorRotation().Yaw), 180.0, 1.0));
	TestFalse(TEXT("A second request while talking is refused (Busy)"), Npc->OpenTalkForInteractor(Player));
	TestTrue(TEXT("Busy left the panel as it was"), Npc->GetTalkWidget() == Panel && Npc->ShowCount == 1);

	UButton* Bye = Panel ? Cast<UButton>(Panel->GetWidgetFromName(TEXT("ByeButton"))) : nullptr;
	if (TestNotNull(TEXT("Bye button"), Bye))
	{
		Bye->OnClicked.Broadcast();
	}
	TestTrue(TEXT("Bye closes the panel and the NPC walks again"), Panel && !Panel->IsTalkOpen() && !Npc->IsTalking()
		&& AI->GetNpcState() == ENpcAIState::Idle && AI->IsIdleTimerActive());
	Panel->CloseTalk();
	TestTrue(TEXT("Closing again does nothing"), !Npc->IsTalking() && AI->GetNpcState() == ENpcAIState::Idle);
	TestTrue(TEXT("I, Tab and Esc are close keys; E is not"), UNpcTalkWidget::IsCloseKey(EKeys::I) && UNpcTalkWidget::IsCloseKey(EKeys::Tab)
		&& UNpcTalkWidget::IsCloseKey(EKeys::Escape) && !UNpcTalkWidget::IsCloseKey(EKeys::E));

	TestTrue(TEXT("Talks again"), Npc->OpenTalkForInteractor(Player) && Npc->ShowCount == 2);
	UNpcTalkWidget* Second = Npc->GetTalkWidget();
	Npc->Destroy();
	TestTrue(TEXT("The NPC ending play closes its panel"), Second && !Second->IsTalkOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNpcS4ProductionWiringTest,
	"NullTide.Npc.S4.ProductionWiring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNpcS4ProductionWiringTest::RunTest(const FString& Parameters)
{
	UClass* TraderClass = LoadClass<ANpcCharacter>(nullptr, S4TraderClassPath);
	UClass* TraderDefinition = LoadClass<UNpcDefinition>(nullptr, S4TraderDefinitionPath);
	UClass* TalkClass = LoadClass<UNpcTalkWidget>(nullptr, S4TalkClassPath);
	UClass* InteractableClass = LoadClass<UInterface>(nullptr, S4InteractablePath);
	if (!TestTrue(TEXT("BP_Npc_Trader, NPC_Trader, W_NpcTalk and BPI_Interactable load"), TraderClass && TraderDefinition && TalkClass
		&& InteractableClass))
	{
		return false;
	}
	TestTrue(TEXT("NPC_Trader spawns BP_Npc_Trader"), TraderDefinition->GetDefaultObject<UNpcDefinition>()->NpcActorClass == TraderClass);

	ANpcCharacter* Trader = TraderClass->GetDefaultObject<ANpcCharacter>();
	TestTrue(TEXT("BP_Npc_Trader implements BPI_Interactable"), TraderClass->ImplementsInterface(InteractableClass));
	UFunction* GetType = Trader->FindFunction(FName(TEXT("GetInteractionType")));
	TestTrue(TEXT("Interaction type is Press"), GetType && S4CallEnumGetter(Trader, GetType) == TEXT("Press"));
	if (UFunction* GetHold = Trader->FindFunction(FName(TEXT("GetHoldDuration"))); TestNotNull(TEXT("GetHoldDuration is implemented"), GetHold))
	{
		TArray<uint8> Buffer;
		Buffer.SetNumZeroed(FMath::Max<int32>(GetHold->ParmsSize, 1));
		Trader->ProcessEvent(GetHold, Buffer.GetData());
		const FProperty* Return = GetHold->GetReturnProperty();
		for (TFieldIterator<FProperty> It(GetHold); It && !Return; ++It)
		{
			Return = It->HasAnyPropertyFlags(CPF_OutParm) ? *It : nullptr;
		}
		const FNumericProperty* Numeric = CastField<FNumericProperty>(Return);
		TestTrue(TEXT("Hold duration is 0"), Numeric
			&& FMath::IsNearlyZero(Numeric->GetFloatingPointPropertyValue(Numeric->ContainerPtrToValuePtr<void>(Buffer.GetData()))));
	}
	TestTrue(TEXT("AI controller and no actor tick"), Trader->AIControllerClass == ANpcAIController::StaticClass()
		&& !Trader->PrimaryActorTick.bCanEverTick && Trader->AutoPossessAI == EAutoPossessAI::PlacedInWorldOrSpawned);
	FClassProperty* ScreenProperty = FindFProperty<FClassProperty>(ANpcCharacter::StaticClass(), TEXT("TalkScreenClass"));
	TestTrue(TEXT("Talk screen W_NpcTalk"), ScreenProperty && ScreenProperty->GetObjectPropertyValue_InContainer(Trader) == TalkClass);
	USkeletalMeshComponent* Mesh = Trader->GetMesh();
	TestTrue(TEXT("Quinn mesh with ABP_Unarmed"), Mesh && Mesh->GetSkeletalMeshAsset()
		&& Mesh->GetSkeletalMeshAsset()->GetPathName() == S4QuinnPath && Mesh->GetAnimClass()
		&& Mesh->GetAnimClass()->GetPathName() == S4AnimClassPath);

	FS4World Fixture;
	if (Fixture.IsValid(*this))
	{
		const UNpcTalkWidget* Panel = CreateWidget<UNpcTalkWidget>(Fixture.World, TalkClass);
		TestTrue(TEXT("W_NpcTalk binds its name, greeting and Bye"), Panel && Panel->HasRequiredWidgets());
	}
	return true;
}

#endif
