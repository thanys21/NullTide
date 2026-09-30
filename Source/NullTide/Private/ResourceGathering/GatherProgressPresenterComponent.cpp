#include "ResourceGathering/GatherProgressPresenterComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "ResourceGathering/ResourceNodeActor.h"
#include "UI/GatherProgressWidget.h"
#include "UObject/ConstructorHelpers.h"

UGatherProgressPresenterComponent::UGatherProgressPresenterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	static ConstructorHelpers::FClassFinder<UGatherProgressWidget> GatherProgressWidgetClass(
		TEXT("/Game/LevelPrototyping/ResourceGathering/W_GatherProgress"));
	if (GatherProgressWidgetClass.Succeeded())
	{
		ProgressWidgetClass = GatherProgressWidgetClass.Class;
	}
}

void UGatherProgressPresenterComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshGatherProgress();
}

void UGatherProgressPresenterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ProgressWidget)
	{
		ProgressWidget->RemoveFromParent();
		ProgressWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UGatherProgressPresenterComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshGatherProgress();
}

void UGatherProgressPresenterComponent::RefreshGatherProgress()
{
	AResourceNodeActor* ResourceNode = AResourceNodeActor::FindActiveGatherNodeForInteractor(GetOwner());
	if (!IsValid(ResourceNode) || !ResourceNode->IsGathering())
	{
		if (ProgressWidget)
		{
			ProgressWidget->ResetGatherProgress();
		}
		return;
	}

	if (UGatherProgressWidget* Widget = EnsureProgressWidget())
	{
		Widget->UpdateFromGatherNode(ResourceNode);
	}
}

UGatherProgressWidget* UGatherProgressPresenterComponent::EnsureProgressWidget()
{
	if (ProgressWidget)
	{
		return ProgressWidget;
	}

	const APawn* OwningPawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = OwningPawn ? Cast<APlayerController>(OwningPawn->GetController()) : nullptr;
	if (!PlayerController || !ProgressWidgetClass)
	{
		return nullptr;
	}

	ProgressWidget = CreateWidget<UGatherProgressWidget>(PlayerController, ProgressWidgetClass);
	if (ProgressWidget)
	{
		ProgressWidget->AddToViewport(20);
		ProgressWidget->ResetGatherProgress();
	}
	return ProgressWidget;
}
