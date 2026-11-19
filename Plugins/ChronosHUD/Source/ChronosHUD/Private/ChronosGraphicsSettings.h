#pragma once

#include "GameFramework/GameUserSettings.h"
#include "RHIStats.h"

namespace ChronosGraphics
{
    constexpr int32 AutomaticFPS = -1;

    struct FHardware
    {
        FString GPU;
        FString CPU;
        FString RHI;
        uint32 Vendor = 0;
        uint64 Dedicated = 0;
        uint64 RAM = 0;
        FIntPoint Display = FIntPoint::ZeroValue;
        int32 Refresh = 0;
        bool bRayTracing = false;
    };

    struct FValues
    {
        void Read(const UGameUserSettings& Settings);
        void ApplyVideo(UGameUserSettings& Settings) const;
        void ApplyQuality(UGameUserSettings& Settings) const;

        EWindowMode::Type Mode = EWindowMode::WindowedFullscreen;
        FIntPoint Resolution = FIntPoint::ZeroValue;
        Scalability::FQualityLevels Quality;
        int32 FPS = AutomaticFPS;
        bool bVSync = false;
        bool bRayTracing = false;
        bool bManual = false;
    };

    const FHardware& Hardware();
    const FRHIMemoryStats& VideoMemory();
    TOptional<uint64> EstimatedVideoMemory(const FValues& Values);
    FText MemoryAmount(uint64 Bytes);
    void Initialize();
    FValues Recommended(bool bRecalculate);
    void SaveChoices(const FValues& Values);
    bool LowVideoMemory();
}
