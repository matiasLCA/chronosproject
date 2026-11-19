#pragma once

#include "Templates/SharedPointer.h"

class FGenericApplicationMessageHandler;
class IInputDevice;

TSharedPtr<IInputDevice> CreateChronosDualSenseInput(const TSharedRef<FGenericApplicationMessageHandler>& Handler);
