#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuildingPreviewActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * Client-side ghost of a building piece for placement feedback. It copies the piece class's static meshes (default
 * components and Blueprint-added templates) with one tinted translucent material. It has no collision, no navigation
 * influence and no piece component, ignores every trace, and is never registered with the building subsystem.
 */
UCLASS(NotBlueprintable, Transient)
class NULLTIDE_API ABuildingPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	ABuildingPreviewActor();

	/** Parameter of the preview material that receives the valid/invalid color. */
	static const FName ColorParameterName;

	/** Builds the preview meshes from a piece class. Replaces any previous meshes. */
	void InitializeFromPieceClass(TSubclassOf<AActor> PieceClass, UMaterialInterface* Material);

	void SetPlacementValid(bool bValid, const FLinearColor& ValidColor, const FLinearColor& InvalidColor);

	bool IsPlacementValid() const { return bPlacementValid; }
	int32 GetPreviewMeshCount() const { return PreviewMeshes.Num(); }
	UMaterialInstanceDynamic* GetPreviewMaterial() const { return PreviewMaterial; }

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> PreviewRoot;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> PreviewMeshes;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PreviewMaterial;

	bool bPlacementValid = false;
};
