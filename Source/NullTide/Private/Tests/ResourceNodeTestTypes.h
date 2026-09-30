#pragma once

#include "ResourceGathering/ResourceNodeActor.h"
#include "ResourceNodeTestTypes.generated.h"

UCLASS(Transient, NotBlueprintable)
class AResourceNodeTestActor : public AResourceNodeActor
{
	GENERATED_BODY()

public:
	void Configure(TSubclassOf<UItemDefinition> InOutputDefinition, int32 InMaxYield)
	{
		OutputItemDefinition = InOutputDefinition;
		MaxYield = InMaxYield;
		RemainingYield = FMath::Max(0, InMaxYield);
		GatherState = RemainingYield > 0 ? EResourceNodeState::Idle : EResourceNodeState::Depleted;
	}

	void CompleteForTest() { CompleteGather(); }

protected:
	virtual bool ScheduleGatherCompletion() override { return true; }
};
