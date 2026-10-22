#include "UI/NpcTalkWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Npc/NpcCharacter.h"
#include "UI/GameInputMode.h"

void UNpcTalkWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (ByeButton)
	{
		ByeButton->OnClicked.AddUniqueDynamic(this, &UNpcTalkWidget::HandleByeClicked);
	}
}

void UNpcTalkWidget::OpenForNpc(ANpcCharacter* InNpc, const FText& NpcName, const FText& Greeting)
{
	Npc = InNpc;
	bTalkOpen = true;
	if (NpcNameText)
	{
		NpcNameText->SetText(NpcName);
	}
	if (GreetingText)
	{
		GreetingText->SetText(Greeting);
	}
}

void UNpcTalkWidget::CloseTalk()
{
	if (!bTalkOpen)
	{
		return;
	}
	bTalkOpen = false;
	RemoveFromParent();
	FinishTalk();
}

void UNpcTalkWidget::FinishTalk()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		NullTideInput::RestoreGameplayCursor(*PlayerController);
		NullTideInput::RestoreGameOnlyInput(*PlayerController);
	}
	ANpcCharacter* TalkingNpc = Npc.Get();
	Npc.Reset();
	if (TalkingNpc)
	{
		TalkingNpc->EndTalk();
	}
}

void UNpcTalkWidget::NativeDestruct()
{
	// Removed some other way (travel, RemoveAllViewportWidgets): the NPC must not stay in its talk.
	if (bTalkOpen)
	{
		bTalkOpen = false;
		FinishTalk();
	}
	Super::NativeDestruct();
}

FText UNpcTalkWidget::GetNpcNameText() const
{
	return NpcNameText ? NpcNameText->GetText() : FText::GetEmpty();
}

FText UNpcTalkWidget::GetGreetingText() const
{
	return GreetingText ? GreetingText->GetText() : FText::GetEmpty();
}

bool UNpcTalkWidget::IsCloseKey(const FKey& Key)
{
	return Key == EKeys::I || Key == EKeys::Tab || Key == EKeys::Escape;
}

FReply UNpcTalkWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// The preview pass, so a focused button cannot turn Tab into focus navigation first (as on the other screens).
	if (IsCloseKey(InKeyEvent.GetKey()))
	{
		CloseTalk();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UNpcTalkWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (IsCloseKey(InKeyEvent.GetKey()))
	{
		CloseTalk();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UNpcTalkWidget::HandleByeClicked()
{
	CloseTalk();
}
