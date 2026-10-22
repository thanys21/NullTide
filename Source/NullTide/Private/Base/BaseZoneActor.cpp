#include "Base/BaseZoneActor.h"

#include "Base/BaseZoneComponent.h"
#include "Building/BuildingSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RingMaterialAsset(
		TEXT("/Game/LevelPrototyping/BaseSystem/Materials/MI_BaseZoneRing.MI_BaseZoneRing"));
	RingMaterial = RingMaterialAsset.Object;
	RingSouth = CreateRingStrip(TEXT("RingSouth"), CubeMesh.Object);
	RingNorth = CreateRingStrip(TEXT("RingNorth"), CubeMesh.Object);
	RingWest = CreateRingStrip(TEXT("RingWest"), CubeMesh.Object);
	RingEast = CreateRingStrip(TEXT("RingEast"), CubeMesh.Object);
}

UStaticMeshComponent* ABaseZoneActor::CreateRingStrip(const FName Name, UStaticMesh* Mesh)
{
	UStaticMeshComponent* Strip = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Strip->SetupAttachment(ZoneRoot);
	Strip->SetStaticMesh(Mesh);
	Strip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Strip->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Strip->SetGenerateOverlapEvents(false);
	Strip->SetCanEverAffectNavigation(false);
	Strip->SetCastShadow(false);
	Strip->SetUsingAbsoluteRotation(true);
	Strip->SetUsingAbsoluteScale(true);
	return Strip;
}

TArray<UStaticMeshComponent*> ABaseZoneActor::GetRingStrips() const
{
	return { RingSouth, RingNorth, RingWest, RingEast };
}

TArray<FBox2D> ABaseZoneActor::ComputeRingStrips(const FIntRect& ZoneCells)
{
	const double Size = UBuildingSubsystem::CellSize;
	const FVector2D Min(ZoneCells.Min.X * Size, ZoneCells.Min.Y * Size);
	const FVector2D Max(ZoneCells.Max.X * Size, ZoneCells.Max.Y * Size);
	return {
		FBox2D(FVector2D(Min.X, Min.Y), FVector2D(Max.X, Min.Y + Size)),
		FBox2D(FVector2D(Min.X, Max.Y - Size), FVector2D(Max.X, Max.Y)),
		FBox2D(FVector2D(Min.X, Min.Y + Size), FVector2D(Min.X + Size, Max.Y - Size)),
		FBox2D(FVector2D(Max.X - Size, Min.Y + Size), FVector2D(Max.X, Max.Y - Size)),
	};
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

	// Lay the ring strips flat over the outer ring (the engine cube is 100 cm, centred).
	const TArray<FBox2D> Boxes = ComputeRingStrips(Cells);
	const TArray<UStaticMeshComponent*> Strips = GetRingStrips();
	const double StripZ = GetActorLocation().Z + RingLift + RingThickness * 0.5;
	for (int32 Index = 0; Index < Strips.Num(); ++Index)
	{
		const FVector2D StripCenter = Boxes[Index].GetCenter();
		const FVector2D StripSize = Boxes[Index].GetSize();
		Strips[Index]->SetWorldLocationAndRotation(FVector(StripCenter.X, StripCenter.Y, StripZ), FRotator::ZeroRotator);
		Strips[Index]->SetWorldScale3D(FVector(StripSize.X / 100.0, StripSize.Y / 100.0, RingThickness / 100.0));
		Strips[Index]->SetMaterial(0, RingMaterial);
	}
}
