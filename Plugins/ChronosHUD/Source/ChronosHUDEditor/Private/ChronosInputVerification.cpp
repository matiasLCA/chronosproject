// Editor-only input driver. Not included in the game's runtime module.
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"

namespace
{
    void SendVerificationInput(const TArray<FString>& Args, UWorld* World)
    {
        auto* PC = World ? World->GetFirstPlayerController() : nullptr;
        if (IsEngineExitRequested() || !PC || !PC->PlayerInput || !PC->GetLocalPlayer() || Args.Num() < 2) return;
        const FKey Key(*Args[0]);
        const auto User = PC->GetLocalPlayer()->GetPlatformUserId();
        auto& Mapper = IPlatformInputDeviceMapper::Get();
        const auto Device = Key.IsGamepadKey() ? Mapper.GetPrimaryInputDeviceForUser(User) : Mapper.GetDefaultInputDevice();
        const bool bSlate = Args.Num() > 2 && Args[2] == TEXT("slate");
        const bool bPress = Args[1] == TEXT("down");
        if (Args[1] != TEXT("down") && Args[1] != TEXT("up"))
        {
            if (bSlate && Key.IsGamepadKey() && FSlateApplication::IsInitialized())
            {
                FSlateApplication::Get().OnControllerAnalog(Key.GetFName(), User, Device, FCString::Atod(*Args[1]));
                return;
            }
            PC->InputKey(FInputKeyParams(Key, FCString::Atod(*Args[1]), 1.f / 60, 1, Key.IsGamepadKey(), Device));
            return;
        }
        if (bSlate && FSlateApplication::IsInitialized())
        {
            auto& Slate = FSlateApplication::Get();
            if (Key.IsGamepadKey())
            {
                if (bPress) Slate.OnControllerButtonPressed(Key.GetFName(), User, Device, false);
                else Slate.OnControllerButtonReleased(Key.GetFName(), User, Device, false);
            }
            else
            {
                const FKeyEvent Event(Key, FModifierKeysState(), Device, false, 0, 0);
                if (bPress) Slate.ProcessKeyDownEvent(Event);
                else Slate.ProcessKeyUpEvent(Event);
            }
        }
        else PC->InputKey(FInputKeyParams(Key, bPress ? IE_Pressed : IE_Released, bPress ? 1. : 0., Key.IsGamepadKey(), Device));
        UE_LOG(LogTemp, Display, TEXT("CHRONOS_EDITOR_TEST_INPUT %s %s"), *Key.GetFName().ToString(), *Args[1]);
    }
    FAutoConsoleCommandWithWorldAndArgs VerificationInputCommand(TEXT("Chronos.TestPadInput"),
        TEXT("Send an input event for editor integration verification."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SendVerificationInput));
}
