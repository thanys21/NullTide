#pragma once

#include "ResourceGathering/ResourceNodeActor.h"
#include "ResourceNodeTestTypes.generated.h"

UCLASS(Transient, NotBlueprintable)
class AResourceNodeTestActor : public AResourceNodeActor
{
	GENERATED_BODY()

public:
	void Configure(TSubclassOf<UItemDefinition> InOutputDefinition, int32 InMaxYield,
		EToolType InRequiredToolType = EToolType::None)
	{
		OutputItemDefinition = InOutputDefinition;
		MaxYield = InMaxYield;
		RequiredToolType = InRequiredToolType;
		RemainingYield = FMath::Max(0, InMaxYield);
		GatherState = RemainingYield > 0 ? EResourceNodeState::Idle : EResourceNodeState::Depleted;
	}

	void CompleteForTest() { CompleteGather(); }

protected:
	virtual bool ScheduleGatherCompletion() override { return true; }
};
