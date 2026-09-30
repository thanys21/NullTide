#include "UI/GatherProgressWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "ResourceGathering/ResourceNodeActor.h"

void UGatherProgressWidget::UpdateFromGatherNode(const AResourceNodeActor* ResourceNode)
{
	if (!IsValid(ResourceNode) || !ResourceNode->IsGathering())
	{
		ResetGatherProgress();
		return;
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (GatherProgressBar)
	{
		GatherProgressBar->SetPercent(ResourceNode->GetNormalizedGatherProgress());
	}
	if (GatherStatusText)
	{
		GatherStatusText->SetText(FText::FromString(TEXT("Gathering...")));
	}
}

void UGatherProgressWidget::ResetGatherProgress()
{
	if (GatherProgressBar)
	{
		GatherProgressBar->SetPercent(0.0f);
	}
	SetVisibility(ESlateVisibility::Collapsed);
}
