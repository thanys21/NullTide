#include "Building/BuildingPreviewActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Materials/MaterialInstanceDynamic.h"

const FName ABuildingPreviewActor::ColorParameterName(TEXT("Color"));

ABuildingPreviewActor::ABuildingPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PreviewRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PreviewRoot"));
	RootComponent = PreviewRoot;
	SetActorEnableCollision(false);
	SetCanBeDamaged(false);
}

void ABuildingPreviewActor::InitializeFromPieceClass(TSubclassOf<AActor> PieceClass, UMaterialInterface* Material)
{
	for (UStaticMeshComponent* Mesh : PreviewMeshes)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	PreviewMeshes.Reset();
	PreviewMaterial = Material ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;

	const AActor* Defaults = PieceClass ? PieceClass->GetDefaultObject<AActor>() : nullptr;
	if (!Defaults)
	{
		return;
	}
	// Default components (native subobjects, including Blueprint edits to inherited ones) and Blueprint-added
	// templates. Template transforms are taken relative to the piece root.
	TArray<const UStaticMeshComponent*> Templates;
	TInlineComponentArray<UStaticMeshComponent*> DefaultMeshes(Defaults);
	Templates.Append(DefaultMeshes);
	for (const UClass* Class = PieceClass.Get(); Class; Class = Class->GetSuperClass())
	{
		const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Class);
		const USimpleConstructionScript* ConstructionScript = Generated ? Generated->SimpleConstructionScript.Get() : nullptr;
		if (!ConstructionScript)
		{
			continue;
		}
		for (const USCS_Node* Node : ConstructionScript->GetAllNodes())
		{
			if (const UStaticMeshComponent* Template = Node ? Cast<UStaticMeshComponent>(Node->ComponentTemplate) : nullptr)
			{
				Templates.Add(Template);
			}
		}
	}

	for (const UStaticMeshComponent* Template : Templates)
	{
		if (!Template || !Template->GetStaticMesh())
		{
			continue;
		}
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
		Mesh->SetStaticMesh(Template->GetStaticMesh());
		Mesh->SetRelativeTransform(Template->GetRelativeTransform());
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetCastShadow(false);
		Mesh->SetupAttachment(PreviewRoot);
		Mesh->RegisterComponent();
		AddInstanceComponent(Mesh);
		if (PreviewMaterial)
		{
			for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
			{
				Mesh->SetMaterial(Index, PreviewMaterial);
			}
		}
		PreviewMeshes.Add(Mesh);
	}
}

void ABuildingPreviewActor::SetPlacementValid(const bool bValid, const FLinearColor& ValidColor, const FLinearColor& InvalidColor)
{
	bPlacementValid = bValid;
	if (PreviewMaterial)
	{
		PreviewMaterial->SetVectorParameterValue(ColorParameterName, bValid ? ValidColor : InvalidColor);
	}
}
