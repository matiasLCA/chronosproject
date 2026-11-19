#pragma once
#include "Styling/SlateWidgetStyle.h"
#include "Templates/SharedPointer.h"

class SWidget;
class UChronosMenuSettings;
struct FSlateFontInfo;

TSharedRef<SWidget> MakeChronosGraphicsPanel(const FSlateFontInfo& Font, UChronosMenuSettings* Settings);
TSharedRef<SWidget> MakeChronosVolumePanel(const FSlateFontInfo& Font, UChronosMenuSettings* Settings);
