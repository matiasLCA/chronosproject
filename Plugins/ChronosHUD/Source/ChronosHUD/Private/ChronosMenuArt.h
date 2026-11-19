#pragma once

#include "Containers/UnrealString.h"

struct FButtonStyle;
struct FSlateBrush;
class UCanvasPanelSlot;

namespace ChronosMenuArt
{
    const FButtonStyle& ButtonStyle(bool bSelected = false);
    const FButtonStyle& ConfirmStyle();
    const FSlateBrush& OptionsPanel();
    const FSlateBrush& Premium(const FString& Name);
    const FButtonStyle& PremiumButton(const FString& Prefix, bool bSelected = false);
    void PlaceOptionsPanel(UCanvasPanelSlot& PanelSlot, bool bInset = false);
}
