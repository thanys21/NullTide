#include "Building/BuildingDoorActor.h"

#include "Building/BuildingPieceComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
/** Places an engine-cube mesh (100 cm) as a box of Size centred at Center, in the parent's space. */
void PlaceCube(UStaticMeshComponent* Mesh, UStaticMesh* Cube, const FVector& Center, const FVector& Size)
{
	if (Cube)
	{
		Mesh->SetStaticMesh(Cube);
	}
	Mesh->SetRelativeLocation(Center);
	Mesh->SetRelativeScale3D(Size / 100.0);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}
}

ABuildingDoorActor::ABuildingDoorActor()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cube = CubeMesh.Succeeded() ? CubeMesh.Object : nullptr;

	// The inherited PieceMesh is the left frame post.
	PlaceCube(PieceMesh, Cube, FVector(-75.0, 0.0, 110.0), FVector(50.0, 20.0, 220.0));

	FramePostRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FramePostRight"));
	FramePostRight->SetupAttachment(PieceRoot);
	PlaceCube(FramePostRight, Cube, FVector(75.0, 0.0, 110.0), FVector(50.0, 20.0, 220.0));

	FrameLintel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameLintel"));
	FrameLintel->SetupAttachment(PieceRoot);
	PlaceCube(FrameLintel, Cube, FVector(0.0, 0.0, 235.0), FVector(200.0, 20.0, 30.0));

	// The hinge sits at the inner edge of the left post; the leaf extends from it along +X.
	DoorHinge = CreateDefaultSubobject<USceneComponent>(TEXT("DoorHinge"));
	DoorHinge->SetupAttachment(PieceRoot);
	DoorHinge->SetRelativeLocation(FVector(-50.0, 0.0, 0.0));

	DoorLeaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorLeaf"));
	DoorLeaf->SetupAttachment(DoorHinge);
	PlaceCube(DoorLeaf, Cube, FVector(50.0, 0.0, 110.0), FVector(100.0, 8.0, 220.0));

	// Tight overlap-only bounds (see BUILDING_PLAN.md section 8, interaction target ambiguity).
	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(PieceRoot);
	InteractionBounds->SetRelativeLocation(FVector(0.0, 0.0, 110.0));
	InteractionBounds->SetBoxExtent(FVector(100.0, 110.0, 110.0));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractionBounds->SetGenerateOverlapEvents(true);
	InteractionBounds->SetCanEverAffectNavigation(false);
}

EBuildingDoorResult ABuildingDoorActor::CanInteract(const AActor* Interactor) const
{
	// Any pawn may use a door (decision B5-4: no player-controller requirement, so NPCs can use doors later).
	if (!IsValid(Interactor) || !Interactor->IsA<APawn>())
	{
		return EBuildingDoorResult::InvalidInteractor;
	}
	const UBuildingPieceComponent* Piece = GetBuildingPieceComponent();
	if (!Piece || Piece->GetPieceState() != EBuildingPieceState::Active || IsProvisional() || IsActorBeingDestroyed())
	{
		return EBuildingDoorResult::NotActive;
	}
	if (FVector::DistSquared2D(Interactor->GetActorLocation(), GetActorLocation()) > FMath::Square(UseRange))
	{
		return EBuildingDoorResult::OutOfRange;
	}
	return EBuildingDoorResult::Success;
}

EBuildingDoorResult ABuildingDoorActor::ToggleDoor(AActor* Interactor)
{
	return SetDoorOpen(!bIsOpen, Interactor);
}

EBuildingDoorResult ABuildingDoorActor::SetDoorOpen(const bool bOpen, AActor* Interactor)
{
	const EBuildingDoorResult Result = CanInteract(Interactor);
	if (Result != EBuildingDoorResult::Success)
	{
		return Result;
	}
	if (bIsOpen != bOpen)
	{
		bIsOpen = bOpen;
		StartSwing();
	}
	return EBuildingDoorResult::Success;
}

bool ABuildingDoorActor::IsDoorMoving() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(SwingTimer);
}

void ABuildingDoorActor::StartSwing()
{
	if (bIsOpen)
	{
		// Opening: the leaf stops blocking at once, so it never shoves a pawn while it moves.
		SetLeafBlocking(false);
	}
	UWorld* World = GetWorld();
	if (SwingDuration <= 0.f || !World)
	{
		OpenAlpha = bIsOpen ? 1.f : 0.f;
		ApplyOpenAlpha();
		if (World)
		{
			World->GetTimerManager().ClearTimer(SwingTimer);
		}
		if (!bIsOpen)
		{
			SetLeafBlocking(true);
		}
		return;
	}
	if (!World->GetTimerManager().IsTimerActive(SwingTimer))
	{
		World->GetTimerManager().SetTimer(SwingTimer, this, &ABuildingDoorActor::StepSwing, SwingStepSeconds, true);
	}
}

void ABuildingDoorActor::StepSwing()
{
	const float Target = bIsOpen ? 1.f : 0.f;
	OpenAlpha = FMath::FInterpConstantTo(OpenAlpha, Target, SwingStepSeconds, 1.f / SwingDuration);
	ApplyOpenAlpha();
	if (FMath::IsNearlyEqual(OpenAlpha, Target))
	{
		OpenAlpha = Target;
		ApplyOpenAlpha();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(SwingTimer);
		}
		if (!bIsOpen)
		{
			// Fully closed: block again.
			SetLeafBlocking(true);
		}
	}
}

void ABuildingDoorActor::ApplyOpenAlpha()
{
	if (DoorHinge)
	{
		DoorHinge->SetRelativeRotation(FRotator(0.0, OpenYaw * OpenAlpha, 0.0));
	}
}

void ABuildingDoorActor::SetLeafBlocking(const bool bBlocking)
{
	if (DoorLeaf)
	{
		DoorLeaf->SetCollisionEnabled(bBlocking ? ClosedLeafCollision.GetValue() : ECollisionEnabled::NoCollision);
	}
}

void ABuildingDoorActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (DoorLeaf)
	{
		// The component's own setting: a provisional piece has actor collision off, which GetCollisionEnabled() reports.
		ClosedLeafCollision = DoorLeaf->BodyInstance.GetCollisionEnabled(false);
	}
}

void ABuildingDoorActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	// After Super: an EndPlay listener could still have toggled the door.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SwingTimer);
	}
}
