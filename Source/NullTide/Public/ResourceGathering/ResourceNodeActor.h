#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/Fragments/ItemFragmentTypes.h"
#include "ResourceNodeActor.generated.h"

class UInventoryComponent;
class UItemDefinition;
class UToolLoadoutComponent;

UENUM(BlueprintType)
enum class EResourceNodeState : uint8
{
	Idle,
	Gathering,
	Depleted
};

/** Generic, timer-driven source of single ItemDefinition rewards. */
UCLASS(Blueprintable)
class NULLTIDE_API AResourceNodeActor : public AActor
{
	GENERATED_BODY()

public:
	AResourceNodeActor();

	virtual void BeginPlay() override;

	/** Called by a BPI_Interactable adapter after the existing interaction system accepts F. */
	UFUNCTION(BlueprintCallable, Category = "Resource Gathering")
	bool BeginGatherFromInteractor(AActor* Interactor);

	/** Cancels this node's active gather without awarding or consuming yield. */
	UFUNCTION(BlueprintCallable, Category = "Resource Gathering")
	void CancelGather();

	/** Intended for the existing IA_Move and IA_Jump action paths on the interacting player. */
	UFUNCTION(BlueprintCallable, Category = "Resource Gathering", meta = (DefaultToSelf = "Interactor"))
	static bool CancelActiveGatherForInteractor(AActor* Interactor);

	/** Read-only lookup used by the player presentation component. */
	static AResourceNodeActor* FindActiveGatherNodeForInteractor(const AActor* Interactor);

	/** Presentation-only predicate: depleted resource nodes should not advertise an interaction prompt. */
	UFUNCTION(BlueprintPure, Category = "Resource Gathering")
	static bool ShouldShowInteractionPromptFor(const AActor* Interactable);

	UFUNCTION(BlueprintPure, Category = "Resource Gathering")
	EResourceNodeState GetGatherState() const { return GatherState; }

	UFUNCTION(BlueprintPure, Category = "Resource Gathering")
	bool IsGathering() const { return GatherState == EResourceNodeState::Gathering; }

	UFUNCTION(BlueprintPure, Category = "Resource Gathering")
	bool IsDepleted() const { return GatherState == EResourceNodeState::Depleted; }

	UFUNCTION(BlueprintPure, Category = "Resource Gathering")
	int32 GetRemainingYield() const { return RemainingYield; }

	UFUNCTION(BlueprintPure, Category = "Resource Gathering")
	int32 GetMaxYield() const { return MaxYield; }

	UFUNCTION(BlueprintPure, Category = "Resource Gathering")
	TSubclassOf<UItemDefinition> GetOutputItemDefinition() const { return OutputItemDefinition; }

	UFUNCTION(BlueprintPure, Category = "Resource Gathering")
	float GetGatherDuration() const { return GatherDuration; }

	UFUNCTION(BlueprintPure, Category = "Resource Gathering")
	EToolType GetRequiredToolType() const { return RequiredToolType; }

	/** Read-only progress for a future UI. This actor does not tick for progress. */
	UFUNCTION(BlueprintPure, Category = "Resource Gathering")
	float GetNormalizedGatherProgress() const;

protected:
	virtual UInventoryComponent* ResolveInventoryForInteractor(AActor* Interactor) const;
	virtual UToolLoadoutComponent* ResolveToolLoadoutForInteractor(AActor* Interactor) const;
	virtual bool ScheduleGatherCompletion();
	virtual void CompleteGather();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resource Gathering")
	TSubclassOf<UItemDefinition> OutputItemDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resource Gathering", meta = (ClampMin = "0"))
	int32 MaxYield = 1;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Resource Gathering")
	int32 RemainingYield = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resource Gathering", meta = (ClampMin = "0.01"))
	float GatherDuration = 2.0f;

	/** None preserves generic gathering; non-None requires the matching exact inventory tool assignment. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resource Gathering")
	EToolType RequiredToolType = EToolType::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Resource Gathering", meta = (AllowPrivateAccess = "true"))
	EResourceNodeState GatherState = EResourceNodeState::Idle;

private:
	bool IsOutputDefinitionUsable() const;
	void InitializeYield();
	void ClearGathering();

	TWeakObjectPtr<AActor> ActiveInteractor;
	FTimerHandle GatherTimerHandle;
	double GatherStartTimeSeconds = 0.0;
};
