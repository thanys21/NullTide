#include "Base/BaseZoneActor.h"

#include "Base/BaseZoneComponent.h"
#include "Building/BuildingSubsystem.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"

ABaseZoneActor::ABaseZoneActor()
{
	PrimaryActorTick.bCanEverTick = false;
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif

	ZoneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ZoneRoot"));
	RootComponent = ZoneRoot;

	Zone = CreateDefaultSubobject<UBaseZoneComponent>(TEXT("Zone"));

	ZoneBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneBounds"));
	ZoneBounds->SetupAttachment(ZoneRoot);
	ZoneBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneBounds->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	ZoneBounds->SetGenerateOverlapEvents(false);
	ZoneBounds->SetCanEverAffectNavigation(false);
	ZoneBounds->SetHiddenInGame(true);
	ZoneBounds->SetUsingAbsoluteRotation(true);
	ZoneBounds->SetUsingAbsoluteScale(true);
	ZoneBounds->ShapeColor = FColor(80, 200, 255);
	ZoneBounds->SetLineThickness(4.f);
}

void ABaseZoneActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Outline the snapped rectangle; the buildable area is one cell inside it.
	const FIntRect Cells = Zone->GetZoneCells();
	const double Size = UBuildingSubsystem::CellSize;
	const FVector Center((Cells.Min.X + Cells.Max.X) * 0.5 * Size, (Cells.Min.Y + Cells.Max.Y) * 0.5 * Size,
		GetActorLocation().Z);
	ZoneBounds->SetBoxExtent(FVector(Cells.Width() * 0.5 * Size, Cells.Height() * 0.5 * Size, BoundsHalfHeight));
	ZoneBounds->SetWorldLocationAndRotation(Center, FRotator::ZeroRotator);
}
