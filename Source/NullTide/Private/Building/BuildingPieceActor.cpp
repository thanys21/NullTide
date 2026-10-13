#include "Building/BuildingPieceActor.h"

#include "Building/BuildingPieceComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ABuildingPieceActor::ABuildingPieceActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PieceRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PieceRoot"));
	SetRootComponent(PieceRoot);

	PieceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PieceMesh"));
	PieceMesh->SetupAttachment(PieceRoot);
	PieceMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	BuildingPiece = CreateDefaultSubobject<UBuildingPieceComponent>(TEXT("BuildingPiece"));
}

void ABuildingPieceActor::SetProvisional(const bool bInProvisional)
{
	if (bProvisional == bInProvisional)
	{
		return;
	}
	bProvisional = bInProvisional;
	SetActorHiddenInGame(bInProvisional);
	SetActorEnableCollision(!bInProvisional);
	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	if (bInProvisional)
	{
		// Remember which primitives affected navigation so finalizing restores exactly that.
		NavigationRelevantPrimitives.Reset();
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive && Primitive->CanEverAffectNavigation())
			{
				NavigationRelevantPrimitives.Add(Primitive);
				Primitive->SetCanEverAffectNavigation(false);
			}
		}
		return;
	}
	for (const TWeakObjectPtr<UPrimitiveComponent>& Primitive : NavigationRelevantPrimitives)
	{
		if (Primitive.IsValid())
		{
			Primitive->SetCanEverAffectNavigation(true);
		}
	}
	NavigationRelevantPrimitives.Reset();
}
