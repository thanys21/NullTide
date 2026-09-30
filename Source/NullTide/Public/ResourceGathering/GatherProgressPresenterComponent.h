#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GatherProgressPresenterComponent.generated.h"

class UGatherProgressWidget;

/** Player-owned observer that maintains one progress widget for the active gather. */
UCLASS(ClassGroup = (ResourceGathering), meta = (BlueprintSpawnableComponent))
class NULLTIDE_API UGatherProgressPresenterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGatherProgressPresenterComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditDefaultsOnly, Category = "Resource Gathering|UI")
	TSubclassOf<UGatherProgressWidget> ProgressWidgetClass;

private:
	void RefreshGatherProgress();
	UGatherProgressWidget* EnsureProgressWidget();

	UPROPERTY(Transient)
	TObjectPtr<UGatherProgressWidget> ProgressWidget;
};
