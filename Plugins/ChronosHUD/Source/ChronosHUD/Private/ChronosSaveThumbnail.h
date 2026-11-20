#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UTexture2D;

namespace ChronosSaveThumbnail
{
    constexpr int32 Width = 320;
    constexpr int32 Height = 180;

    bool Capture(APlayerController& Controller, TArray<uint8>& Encoded);
    UTexture2D* Decode(const TArray<uint8>& Encoded);
}
