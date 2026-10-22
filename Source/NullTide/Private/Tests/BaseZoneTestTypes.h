#pragma once

#include "CoreMinimal.h"
#include "Base/BaseZonePresenceComponent.h"
#include "Blueprint/UserWidget.h"
#include "UI/BaseZoneTagWidget.h"
#include "BaseZoneTestTypes.generated.h"

class UBaseZoneComponent;

/** Base zone presence with the local-player gate forced and a viewport-free tag (S3): a test world has no local player. */
UCLASS(Transient, NotBlueprintable)
class UBaseZonePresenceTestComponent : public UBaseZonePresenceComponent
{
	GENERATED_BODY()

public:
	bool bTreatAsLocal = true;
	TSubclassOf<UBaseZoneTagWidget> TestTagClass;
	int32 TagCreateCount = 0;

protected:
	virtual bool IsLocalPlayerController(const APlayerController& PlayerController) const override { return bTreatAsLocal; }

	virtual UBaseZoneTagWidget* CreateTagWidget(APlayerController& PlayerController) override
	{
		++TagCreateCount;
		return TestTagClass ? CreateWidget<UBaseZoneTagWidget>(GetWorld(), TestTagClass) : nullptr;
	}
};

/** Records OnBaseZoneChanged. */
UCLASS(Transient, NotBlueprintable)
class UBaseZoneTestListener : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleZoneChanged(UBaseZoneComponent* NewZone) { Changes.Add(NewZone); }

	TArray<TWeakObjectPtr<UBaseZoneComponent>> Changes;
};
