#include "ChronosGraphicsSettings.h"

#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "Misc/ConfigCacheIni.h"
#include "RenderUtils.h"
#include "Widgets/SWindow.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

#define LOCTEXT_NAMESPACE "ChronosGraphicsSettings"

namespace ChronosGraphics
{
    namespace
    {
        const TCHAR* Section = TEXT("Chronos.Graphics");
        bool bLowMemory = false;
        bool bInitialized = false;
        FRHIMemoryStats MeasuredMemory;
        double NextSample = 0.0;
        TOptional<FValues> Recommendation;
        struct FMemorySample
        {
            double Average = 0.;
            int32 Count = 0;
        };
        TMap<FString, FMemorySample> MemorySamples;
        double SampleAfter = 0.;
        const TCHAR* RecommendedSection = TEXT("Chronos.Graphics.Recommended");
        const TPair<const TCHAR*, int32 Scalability::FQualityLevels::*> QualityGroups[] = {
            {TEXT("ViewDistance"), &Scalability::FQualityLevels::ViewDistanceQuality},
            {TEXT("AntiAliasing"), &Scalability::FQualityLevels::AntiAliasingQuality},
            {TEXT("Shadow"), &Scalability::FQualityLevels::ShadowQuality},
            {TEXT("GlobalIllumination"), &Scalability::FQualityLevels::GlobalIlluminationQuality},
            {TEXT("Reflection"), &Scalability::FQualityLevels::ReflectionQuality},
            {TEXT("PostProcess"), &Scalability::FQualityLevels::PostProcessQuality},
            {TEXT("Texture"), &Scalability::FQualityLevels::TextureQuality},
            {TEXT("Effects"), &Scalability::FQualityLevels::EffectsQuality},
            {TEXT("Foliage"), &Scalability::FQualityLevels::FoliageQuality},
            {TEXT("Shading"), &Scalability::FQualityLevels::ShadingQuality},
            {TEXT("Landscape"), &Scalability::FQualityLevels::LandscapeQuality},
        };

        FString SampleKey(const FValues& Values)
        {
            const UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport.Get() : nullptr;
            const auto* World = Viewport ? Viewport->GetWorld() : nullptr;
            const FIntPoint Size = Values.Mode == EWindowMode::WindowedFullscreen ? Hardware().Display : Values.Resolution;
            FString Key = FString::Printf(TEXT("%s|%dx%d|%.2f|%d"), World ? *World->GetMapName() : TEXT(""), Size.X, Size.Y,
                Values.Quality.ResolutionQuality, int32(Values.bRayTracing));
            for (const auto& Group : QualityGroups) Key += FString::Printf(TEXT("|%d"), Values.Quality.*Group.Value);
            return Key;
        }

        int32 FrameCap(const FValues& Values)
        {
            if (Values.FPS != AutomaticFPS) return Values.FPS;
            const int32 Refresh = Hardware().Refresh;
            return Refresh > 0 && Refresh < 60 ? 30 :
                (Refresh >= 120 && Values.Quality.GetSingleQualityLevel() >= 2 && Hardware().Dedicated >= 8ull * 1024 * 1024 * 1024 ? 120 : 60);
        }

        void SetRayTracing(bool bEnable)
        {
            if (!Hardware().bRayTracing) return;
            auto& Console = IConsoleManager::Get();
            if (auto* Switch = Console.FindConsoleVariable(TEXT("r.RayTracing.Enable")))
                Switch->Set(bEnable ? 1 : 0, ECVF_SetByGameOverride);
            if (auto* Lumen = Console.FindConsoleVariable(TEXT("r.Lumen.HardwareRayTracing")))
                Lumen->Set(bEnable ? 1 : 0, ECVF_SetByGameOverride);
        }

        FString HardwareKey()
        {
            const auto& Detected = Hardware();
            return FString::Printf(TEXT("%s|%s|%llu|%llu|%dx%d@%d|%s"), *Detected.GPU, *Detected.CPU, Detected.Dedicated,
                Detected.RAM, Detected.Display.X, Detected.Display.Y, Detected.Refresh, *Detected.RHI);
        }
    }

    const FHardware& Hardware()
    {
        static const FHardware Info = []
        {
            FHardware Result;
            if (GDynamicRHI && !GUsingNullRHI)
            {
                Result.GPU = GRHIAdapterName;
                Result.Vendor = GRHIVendorId;
                Result.Dedicated = GRHIGlobals.GpuInfo.DedicatedVideoMemory;
                Result.RHI = GDynamicRHI->GetName();
                Result.bRayTracing = GIsRHIInitialized && GetRayTracingMode() == ERayTracingMode::Dynamic;
            }
            Result.CPU = FPlatformMisc::GetCPUBrand();
            Result.RAM = FPlatformMemory::GetConstants().TotalPhysical;
            FDisplayMetrics Displays;
            FDisplayMetrics::RebuildDisplayMetrics(Displays);
            Result.Display = FIntPoint(Displays.PrimaryDisplayWidth, Displays.PrimaryDisplayHeight);
#if PLATFORM_WINDOWS
            DEVMODEW Mode{};
            Mode.dmSize = sizeof(Mode);
            wchar_t* DeviceName = nullptr;
            MONITORINFOEXW Monitor{};
            Monitor.cbSize = sizeof(Monitor);
            if (GEngine && GEngine->GameViewport)
                if (auto Window = GEngine->GameViewport->GetWindow(); Window && Window->GetNativeWindow())
                    if (GetMonitorInfoW(MonitorFromWindow(static_cast<HWND>(Window->GetNativeWindow()->GetOSWindowHandle()),
                            MONITOR_DEFAULTTOPRIMARY), &Monitor)) DeviceName = Monitor.szDevice;
            if (EnumDisplaySettingsW(DeviceName, ENUM_CURRENT_SETTINGS, &Mode))
            {
                Result.Display = FIntPoint(Mode.dmPelsWidth, Mode.dmPelsHeight);
                if (Mode.dmDisplayFrequency > 1) Result.Refresh = Mode.dmDisplayFrequency;
            }
#endif
            return Result;
        }();
        return Info;
    }

    const FRHIMemoryStats& VideoMemory()
    {
        const double Now = FPlatformTime::Seconds();
        if (Now >= NextSample)
        {
            NextSample = Now + 1.0;
            MeasuredMemory = {};
            if (GDynamicRHI && Hardware().Dedicated) RHIGetMemoryStats(MeasuredMemory);
            const double Ratio = MeasuredMemory.BudgetLocal ? double(MeasuredMemory.UsedLocal) / MeasuredMemory.BudgetLocal : 0.;
            bLowMemory = Ratio >= (bLowMemory ? .84 : .90);
            if (MeasuredMemory.BudgetLocal && Now >= SampleAfter && GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
            {
                if (const auto* Settings = GEngine->GetGameUserSettings())
                {
                    FValues Active;
                    Active.Read(*Settings);
                    Active.Quality = Scalability::GetQualityLevels();
                    const FIntPoint EffectiveSize = GEngine->GameViewport->Viewport->GetSizeXY();
                    if (Active.Mode == EWindowMode::WindowedFullscreen || EffectiveSize == Active.Resolution)
                    {
                        if (MemorySamples.Num() >= 64) MemorySamples.Reset();
                        auto& Sample = MemorySamples.FindOrAdd(SampleKey(Active));
                        const double Used = double(MeasuredMemory.UsedLocal);
                        Sample.Average = Sample.Count ? FMath::Lerp(Sample.Average, Used, .25) : Used;
                        ++Sample.Count;
                    }
                }
            }
        }
        return MeasuredMemory;
    }

    TOptional<uint64> EstimatedVideoMemory(const FValues& Values)
    {
        VideoMemory();
        const auto* Sample = MemorySamples.Find(SampleKey(Values));
        if (!Sample || Sample->Count < 4) return {};
        return uint64(Sample->Average);
    }

    bool LowVideoMemory()
    {
        VideoMemory();
        return bLowMemory;
    }

    FText MemoryAmount(uint64 Bytes)
    {
        return FText::FromString(FString::Printf(TEXT("%.1f GB"), double(Bytes) / (1024. * 1024. * 1024.)));
    }

    void FValues::Read(const UGameUserSettings& Settings)
    {
        Mode = Settings.GetFullscreenMode();
        Resolution = Settings.GetScreenResolution();
        if (Resolution.X <= 0 || Resolution.Y <= 0) Resolution = Settings.GetDesktopResolution();
        const auto* Viewport = GEngine && GEngine->GameViewport ? GEngine->GameViewport->Viewport : nullptr;
        if (Viewport && !Viewport->IsPlayInEditorViewport())
        {
            if (const auto Window = GEngine->GameViewport->GetWindow()) Mode = Window->GetWindowMode();
            if (Mode != EWindowMode::WindowedFullscreen && Viewport->GetSizeXY().X > 0) Resolution = Viewport->GetSizeXY();
        }
        bVSync = Settings.IsVSyncEnabled();
        Quality = Settings.ScalabilityQuality;
        FPS = FMath::RoundToInt(Settings.GetFrameRateLimit());
        const auto* Lumen = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.HardwareRayTracing"));
        bRayTracing = GDynamicRHI && GIsRHIInitialized && !GUsingNullRHI && IsRayTracingEnabled() && Lumen && Lumen->GetInt() != 0;
        GConfig->GetInt(Section, TEXT("FPSSelection"), FPS, GGameUserSettingsIni);
        GConfig->GetBool(Section, TEXT("Manual"), bManual, GGameUserSettingsIni);
    }

    void FValues::ApplyVideo(UGameUserSettings& Settings) const
    {
        Settings.SetFullscreenMode(Mode);
        if (Resolution.X > 0 && Resolution.Y > 0) Settings.SetScreenResolution(Resolution);
        Settings.ApplyResolutionSettings(false);
        SampleAfter = FPlatformTime::Seconds() + 16.;
    }

    void FValues::ApplyQuality(UGameUserSettings& Settings) const
    {
        Settings.SetVSyncEnabled(bVSync);
        Settings.SetFrameRateLimit(FrameCap(*this));
        Settings.ScalabilityQuality = Quality;
        Settings.ApplyNonResolutionSettings();
        SetRayTracing(bRayTracing);
        IConsoleManager::Get().CallAllConsoleVariableSinks();
        SampleAfter = FPlatformTime::Seconds() + 8.;
        const auto Actual = Scalability::GetQualityLevels();
        const auto* FrameLimit = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
        const auto* VSync = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
        UE_LOG(LogTemp, Display, TEXT("Chronos graphic settings applied: FPS %.0f VSync %d shadows %d textures %d effects %d post %d reflections %d distance %d RT %d"),
            FrameLimit ? FrameLimit->GetFloat() : -1.f, VSync ? VSync->GetInt() : -1, Actual.ShadowQuality, Actual.TextureQuality,
            Actual.EffectsQuality, Actual.PostProcessQuality, Actual.ReflectionQuality, Actual.ViewDistanceQuality,
            GIsRHIInitialized && !GUsingNullRHI && IsRayTracingEnabled());
    }

    FValues Recommended(bool bRecalculate)
    {
        if (Recommendation.IsSet() && !bRecalculate) return Recommendation.GetValue();
        auto* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
        FValues Result;
        if (!Settings) return Result;
        Result.Read(*Settings);
        const auto SavedQuality = Settings->ScalabilityQuality;
        if (bRecalculate && GDynamicRHI && !GUsingNullRHI) Settings->RunHardwareBenchmark(5);
        const auto& Detected = Hardware();
        const bool bMeasured = Settings->GetLastGPUBenchmarkResult() > 0.f && Settings->GetLastCPUBenchmarkResult() > 0.f;
        if (bMeasured && bRecalculate) Result.Quality = Settings->ScalabilityQuality;
        else
        {
            int32 Preset = 1;
            GConfig->GetInt(Section, TEXT("RecommendedQuality"), Preset, GGameUserSettingsIni);
            Result.Quality.SetFromSingleQualityLevel(FMath::Clamp(Preset, 0, 3));
            GConfig->GetFloat(RecommendedSection, TEXT("ResolutionScale"), Result.Quality.ResolutionQuality, GGameUserSettingsIni);
            for (const auto& Group : QualityGroups)
                GConfig->GetInt(RecommendedSection, Group.Key, Result.Quality.*Group.Value, GGameUserSettingsIni);
        }
        const uint64 GiB = 1024ull * 1024 * 1024;
        if (Detected.Dedicated && Detected.Dedicated <= 4 * GiB)
            Result.Quality.TextureQuality = FMath::Min(1, Result.Quality.TextureQuality);
        if (int64(Detected.Display.X) * Detected.Display.Y >= 3840ll * 2160)
            Result.Quality.ResolutionQuality = FMath::Min(Result.Quality.ResolutionQuality, 85.f);
        Result.bManual = false;
        Result.FPS = AutomaticFPS;
        Result.bRayTracing = bMeasured && Detected.bRayTracing && Detected.Dedicated >= 8 * GiB
            && Settings->GetLastGPUBenchmarkResult() >= 160.f;
        Settings->ScalabilityQuality = SavedQuality;
        Recommendation = Result;
        return Result;
    }

    void SaveChoices(const FValues& Values)
    {
        if (GIsEditor) Scalability::SaveState(GGameUserSettingsIni);
        GConfig->SetInt(Section, TEXT("FPSSelection"), Values.FPS, GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("Manual"), Values.bManual, GGameUserSettingsIni);
        GConfig->SetBool(Section, TEXT("HardwareRayTracing"), Values.bRayTracing, GGameUserSettingsIni);
        GConfig->SetString(Section, TEXT("Hardware"), *HardwareKey(), GGameUserSettingsIni);
        if (Recommendation.IsSet())
        {
            GConfig->SetInt(Section, TEXT("RecommendedQuality"), Recommendation->Quality.ViewDistanceQuality, GGameUserSettingsIni);
            GConfig->SetFloat(RecommendedSection, TEXT("ResolutionScale"), Recommendation->Quality.ResolutionQuality, GGameUserSettingsIni);
            for (const auto& Group : QualityGroups)
                GConfig->SetInt(RecommendedSection, Group.Key, Recommendation->Quality.*Group.Value, GGameUserSettingsIni);
        }
        GConfig->Flush(false, GGameUserSettingsIni);
    }

    void Initialize()
    {
        if (bInitialized || !GEngine || !GEngine->GetGameUserSettings() || !GDynamicRHI || GUsingNullRHI || GIsEditor) return;
        bInitialized = true;
        auto& Settings = *GEngine->GetGameUserSettings();
        FValues Saved;
        Saved.Read(Settings);
        FString PreviousHardware;
        const bool bHasProfile = GConfig->GetString(Section, TEXT("Hardware"), PreviousHardware, GGameUserSettingsIni);
        int32 ExistingResolution = 0;
        const bool bHasLegacyPreferences = GConfig->GetInt(TEXT("/Script/Engine.GameUserSettings"), TEXT("LastUserConfirmedResolutionSizeX"),
            ExistingResolution, GGameUserSettingsIni) && ExistingResolution > 0;
        if (!bHasProfile && bHasLegacyPreferences)
        {
            Saved.bManual = true;
            SaveChoices(Saved);
            return;
        }
        if (!bHasProfile || (!Saved.bManual && PreviousHardware != HardwareKey()))
        {
            const FValues Automatic = Recommended(true);
            Settings.ScalabilityQuality = Automatic.Quality;
            Settings.ApplyHardwareBenchmarkResults();
            Automatic.ApplyQuality(Settings);
            Settings.SaveSettings();
            SaveChoices(Automatic);
            return;
        }
        bool bRT = Saved.bRayTracing;
        if (GConfig->GetBool(Section, TEXT("HardwareRayTracing"), bRT, GGameUserSettingsIni)) SetRayTracing(bRT);
        if (Saved.FPS == AutomaticFPS) Saved.ApplyQuality(Settings);
    }
}

#undef LOCTEXT_NAMESPACE
