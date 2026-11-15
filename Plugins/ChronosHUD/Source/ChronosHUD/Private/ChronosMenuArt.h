#pragma once

struct FButtonStyle;
struct FSlateBrush;

namespace ChronosMenuArt
{
    const FButtonStyle& ButtonStyle(bool bSelected = false);
    const FButtonStyle& ConfirmStyle();
    const FSlateBrush& OptionsPanel();
}
