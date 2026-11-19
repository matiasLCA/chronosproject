#pragma once

#include "Containers/Ticker.h"
#include "InputModifiers.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ChronosGamepad.generated.h"

class APlayerController;
class IInputProcessor;
class UInputMappingContext;
class UChronosOptionsMenu;

enum class EChronosGamepadGlyphs : uint8 { Automatic, PlayStation, Xbox };

struct FChronosPadBinding
{
    FName Name;
    FText Label;
    FKey Key;
};

UCLASS()
class UChronosStickDirection : public UInputModifier
{
    GENERATED_BODY()

public:
    UPROPERTY()
    float Direction = 1.f;

    virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* Input,
        FInputActionValue Value, float DeltaSeconds) override;
};

UCLASS()
class CHRONOSHUD_API UChronosGamepad : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    void Initialize(FSubsystemCollectionBase& Collection) override;
    void Deinitialize() override;
    static UChronosGamepad* Get(const APlayerController* Player);
    static const TArray<FChronosPadBinding>& Defaults(bool bAlternate);
    static FText KeyLabel(FKey Key, bool bPlayStation);
    static bool CanRebind(FKey Key);

    FKey Key(FName Name, bool bAlternate) const;
    FText Prompt(FKey Key) const;
    bool IsGamepadActive() const { return bUsingGamepad; }
    bool IsPlayStation() const;
    bool IsAlternate() const { return bAlternateActive; }
    EChronosGamepadGlyphs Glyphs() const { return PreferredGlyphs; }
    bool Commit(bool bAlternate, EChronosGamepadGlyphs Glyphs, const TMap<FName, FKey>& Bindings);
    void OnInput(FKey Key, FInputDeviceId Device);
    TWeakObjectPtr<UChronosOptionsMenu> OptionsMenu;
    FName MainMenuFocus = TEXT("BTN-NuevaPartida");

private:
    bool Tick(float DeltaSeconds);
    void Rebuild();
    void DeviceChanged(EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId Device);

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> Context;
    FTSTicker::FDelegateHandle TickHandle;
    FDelegateHandle DeviceHandle;
    TSharedPtr<IInputProcessor> Observer;
    TMap<FName, FKey> Standard;
    TMap<FName, FKey> Alternate;
    FInputDeviceId LastDevice;
    EChronosGamepadGlyphs PreferredGlyphs = EChronosGamepadGlyphs::Automatic;
    bool bAlternateActive = false;
    bool bUsingGamepad = false;
    bool bDetectedPlayStation = false;
};
