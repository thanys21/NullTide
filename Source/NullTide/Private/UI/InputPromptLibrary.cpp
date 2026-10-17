#include "UI/InputPromptLibrary.h"

#include "InputMappingContext.h"

FKey UInputPromptLibrary::GetKeyMappedToAction(const UInputMappingContext* Context, const UInputAction* Action)
{
	if (Context && Action)
	{
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (Mapping.Action == Action)
			{
				return Mapping.Key;
			}
		}
	}
	return EKeys::Invalid;
}
