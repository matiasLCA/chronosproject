#include "Modules/ModuleManager.h"
#include "ChronosGraphicsSettings.h"
#include "Misc/CoreDelegates.h"

#if PLATFORM_WINDOWS
#include "ChronosDualSenseInput.h"
#include "IInputDeviceModule.h"
#include "Misc/App.h"

class FChronosHUDModule final : public IInputDeviceModule
{
    void StartupModule() override
    {
        IInputDeviceModule::StartupModule();
        StartupHandle = FCoreDelegates::OnFEngineLoopInitComplete.AddStatic(&ChronosGraphics::Initialize);
    }

    void ShutdownModule() override
    {
        FCoreDelegates::OnFEngineLoopInitComplete.Remove(StartupHandle);
        IInputDeviceModule::ShutdownModule();
    }

    FDelegateHandle StartupHandle;

    TSharedPtr<IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& Handler) override
    {
        return !IsRunningCommandlet() && !FApp::IsUnattended() ? CreateChronosDualSenseInput(Handler) : nullptr;
    }
};

IMPLEMENT_MODULE(FChronosHUDModule, ChronosHUD)
#else
IMPLEMENT_MODULE(FDefaultModuleImpl, ChronosHUD)
#endif
