#include "Building/BuildingPreviewActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

const FName ABuildingPreviewActor::ColorParameterName(TEXT("Color"));

namespace
{
/** Transform of a default (CDO) component relative to the piece root, composed up its attach chain. */
FTransform ComposeDefaultToRoot(const USceneComponent* Component, const AActor& Defaults)
{
	FTransform Result = FTransform::Identity;
	const USceneComponent* Root = Defaults.GetRootComponent();
	for (const USceneComponent* Current = Component; Current && Current != Root; Current = Current->GetAttachParent())
	{
		Result = Result * Current->GetRelativeTransform();
	}
	return Result;
}

const USCS_Node* FindScsNodeInHierarchy(const UClass* PieceClass, const FName VariableName)
{
	for (const UClass* Class = PieceClass; Class; Class = Class->GetSuperClass())
	{
		const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Class);
		const USimpleConstructionScript* ConstructionScript = Generated ? Generated->SimpleConstructionScript.Get() : nullptr;
		if (const USCS_Node* Node = ConstructionScript ? ConstructionScript->FindSCSNode(VariableName) : nullptr)
		{
			return Node;
		}
	}
	return nullptr;
}

/**
 * Transform of a Blueprint-added template relative to the piece root: up its SCS parents (same script, or a parent
 * class's node by variable name), then a native parent's default attach chain.
 */
FTransform ComposeScsToRoot(const USCS_Node* Node, const USimpleConstructionScript* ConstructionScript,
	const UClass* PieceClass, const AActor& Defaults)
{
	FTransform Result = FTransform::Identity;
	// The attach graph is a tree; the bound only guards against corrupt data.
	for (int32 Depth = 0; Node && Depth < 64; ++Depth)
	{
		if (const USceneComponent* Template = Cast<USceneComponent>(Node->ComponentTemplate))
		{
			Result = Result * Template->GetRelativeTransform();
		}
		if (USCS_Node* Parent = ConstructionScript ? ConstructionScript->FindParentNode(const_cast<USCS_Node*>(Node)) : nullptr)
		{
			Node = Parent;
			continue;
		}
		if (Node->ParentComponentOrVariableName.IsNone())
		{
			break;
		}
		if (Node->bIsParentComponentNative)
		{
			TInlineComponentArray<USceneComponent*> DefaultComponents(&Defaults);
			for (const USceneComponent* Native : DefaultComponents)
			{
				if (Native && Native->GetFName() == Node->ParentComponentOrVariableName)
				{
					return Result * ComposeDefaultToRoot(Native, Defaults);
				}
			}
			break;
		}
		Node = FindScsNodeInHierarchy(PieceClass, Node->ParentComponentOrVariableName);
		ConstructionScript = Node ? Node->GetSCS() : nullptr;
	}
	return Result;
}
}

namespace
{
/** Ghost geometry never collides, affects navigation or casts shadows. */
void MakeGhostMesh(UStaticMeshComponent* Mesh)
{
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetCastShadow(false);
}
}

ABuildingPreviewActor::ABuildingPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PreviewRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PreviewRoot"));
	RootComponent = PreviewRoot;
	SetActorEnableCollision(false);
	SetCanBeDamaged(false);

	// Facing arrow: a flat shaft and a cone head along the arrow root's +X, just above a floor's top.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	FacingArrow = CreateDefaultSubobject<USceneComponent>(TEXT("FacingArrow"));
	FacingArrow->SetupAttachment(PreviewRoot);
	FacingArrow->SetRelativeLocation(FVector(0.0, 0.0, 12.0));
	ArrowShaft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowShaft"));
	ArrowShaft->SetupAttachment(FacingArrow);
	ArrowShaft->SetStaticMesh(CubeMesh.Succeeded() ? CubeMesh.Object : nullptr);
	ArrowShaft->SetRelativeLocation(FVector(50.0, 0.0, 0.0));
	ArrowShaft->SetRelativeScale3D(FVector(0.6, 0.08, 0.02));
	MakeGhostMesh(ArrowShaft);
	ArrowHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowHead"));
	ArrowHead->SetupAttachment(FacingArrow);
	ArrowHead->SetStaticMesh(ConeMesh.Succeeded() ? ConeMesh.Object : nullptr);
	// The cone's tip is its +Z; pitch -90 turns it to +X.
	ArrowHead->SetRelativeLocationAndRotation(FVector(87.5, 0.0, 0.0), FRotator(-90.0, 0.0, 0.0));
	ArrowHead->SetRelativeScale3D(FVector(0.24, 0.24, 0.25));
	MakeGhostMesh(ArrowHead);
}

void ABuildingPreviewActor::SetFacingArrowVisible(const bool bVisible)
{
	FacingArrow->SetVisibility(bVisible, true);
}

void ABuildingPreviewActor::SetFacingArrow(const bool bEdgePiece)
{
	// Edge pieces face local +Y (the side a door opens to); cell pieces face local +X.
	FacingArrow->SetRelativeRotation(FRotator(0.0, bEdgePiece ? 90.0 : 0.0, 0.0));
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
	// Null resets to the mesh's own material, so a previous tint never lingers.
	ArrowShaft->SetMaterial(0, PreviewMaterial);
	ArrowHead->SetMaterial(0, PreviewMaterial);

	const AActor* Defaults = PieceClass ? PieceClass->GetDefaultObject<AActor>() : nullptr;
	if (!Defaults)
	{
		return;
	}
	// Default components (native subobjects, including Blueprint edits to inherited ones) and Blueprint-added
	// templates, each with its transform composed up its attach chain to the piece root.
	TArray<TPair<const UStaticMeshComponent*, FTransform>> Templates;
	TInlineComponentArray<UStaticMeshComponent*> DefaultMeshes(Defaults);
	for (const UStaticMeshComponent* Default : DefaultMeshes)
	{
		Templates.Emplace(Default, ComposeDefaultToRoot(Default, *Defaults));
	}
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
				Templates.Emplace(Template, ComposeScsToRoot(Node, ConstructionScript, PieceClass.Get(), *Defaults));
			}
		}
	}

	for (const TPair<const UStaticMeshComponent*, FTransform>& Entry : Templates)
	{
		const UStaticMeshComponent* Template = Entry.Key;
		if (!Template || !Template->GetStaticMesh())
		{
			continue;
		}
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
		Mesh->SetStaticMesh(Template->GetStaticMesh());
		Mesh->SetRelativeTransform(Entry.Value);
		MakeGhostMesh(Mesh);
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
