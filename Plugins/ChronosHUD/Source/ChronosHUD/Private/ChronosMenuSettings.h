#pragma once

#include "Blueprint/UserWidget.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "ChronosMenuFocus.h"
#include "ChronosMenuSettings.generated.h"

class UAudioComponent;
class UButton;
class UChronosHUDLayer;
class FSlateApplication;
class UNativeWidgetHost;
class UWidget;

// YukoDev — HUD Y ACCESIBILIDAD
struct FChronosHUDScales
{
    int32 Life = 100;
    int32 Stamina = 100;
    int32 Lamp = 100;
    int32 Map = 100;
    int32 Calendar = 100;
    int32 MenuButtons = 100;

    bool operator==(const FChronosHUDScales&) const = default;
};

UCLASS()
class CHRONOSHUD_API UChronosOptionsSave : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(SaveGame)
    int32 HUDLife = 100;
    UPROPERTY(SaveGame)
    int32 HUDStamina = 100;
    UPROPERTY(SaveGame)
    int32 HUDLamp = 100;
    UPROPERTY(SaveGame)
    int32 HUDMap = 100;
    UPROPERTY(SaveGame)
    int32 HUDCalendar = 100;

    UPROPERTY(SaveGame)
    bool bAlternateGamepad = false;
    UPROPERTY(SaveGame)
    int32 GamepadGlyphs = 0;
    UPROPERTY(SaveGame)
    TMap<FName, FKey> GamepadStandard;
    UPROPERTY(SaveGame)
    TMap<FName, FKey> GamepadAlternate;
    UPROPERTY(SaveGame)
    int32 MenuMusicPercent = 35;
    UPROPERTY(SaveGame)
    int32 ControlsZoom = 100;
    UPROPERTY(SaveGame)
    int32 MenuButtonSize = 100;

    UFUNCTION(BlueprintCallable, Category="YukoDev|Opciones")
    static USaveGame* LoadOptionsForWrite(TSubclassOf<USaveGame> SaveClass);
};

UCLASS()
class UChronosMenuSettings : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    void Initialize(FSubsystemCollectionBase& Collection) override;
    void Deinitialize() override;
    void AttachHUD(UChronosHUDLayer* HUD);
    void Preview(const FChronosHUDScales& Scales);
    void Revert();
    bool Commit(const FChronosHUDScales& Scales);
    const FChronosHUDScales& Saved() const { return Confirmed; }
    float MenuScale() const { return VisibleMenuSize / 100.f; }
    bool HasLiveHUD() const { return LiveHUD.IsValid(); }
    UFUNCTION(BlueprintPure, Category="Chronos|Audio")
    bool IsMenuMusicRequested() const { return bWantsMusic; }
    static UChronosMenuSettings* Get(const UObject* WorldContext);
    FVector3f AudioVolumes() const;
    void PreviewAudioVolumes(const FVector3f& Volumes);
    bool SaveAudioVolumes(const FVector3f& Volumes);
    int32 BindingZoom() const { return ControlsZoom; }
    bool SetBindingZoom(int32 Percentage);

private:
    bool UpdateMenuMusic(float Seconds);
    void MusicFinished(UAudioComponent* Component);

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> MenuMusic;
    TWeakObjectPtr<UChronosHUDLayer> LiveHUD;
    FChronosHUDScales Confirmed;
    FTSTicker::FDelegateHandle MusicTicker;
    TWeakObjectPtr<UWorld> VolumeWorld;
    float PreviewMusicVolume = .35f;
    bool bWantsMusic = false;
    bool bFading = false;
    int32 ControlsZoom = 100;
    int32 MenuMusicPercent = 35;
    int32 VisibleMenuSize = 100;
};

UCLASS()
class CHRONOSHUD_API UChronosOptionsMenu : public UUserWidget
{
    GENERATED_BODY()

public:
    bool HandleHorizontalStick(FSlateApplication& Slate, const FAnalogInputEvent& Event);

protected:
    void NativeConstruct() override;
    void NativeDestruct() override;
    void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    FReply NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event) override;

    UPROPERTY(Transient)
    TObjectPtr<UNativeWidgetHost> SelectorHost;

private:
    UWidget* InitialFocus() const;
    UNativeWidgetHost* ActivePanel() const;
    void RestorePauseFocus();
    UFUNCTION()
    void OpenHUDSettings();
    void CloseHUDSettings();
    void AlignOptionsShell();
    void ThemeExistingPanel();
    UPROPERTY(Transient)
    TObjectPtr<UNativeWidgetHost> HUDPanel;
    UPROPERTY(Transient)
    TObjectPtr<UChronosHUDLayer> SampleHUD;
    UPROPERTY(Transient)
    TObjectPtr<class UImage> Backdrop;
    TArray<TPair<TWeakObjectPtr<UWidget>, ESlateVisibility>> SectionVisibility;
    TArray<TPair<TWeakObjectPtr<UButton>, FButtonStyle>> NavigationStyles;
    TArray<TWeakObjectPtr<UButton>> SidebarButtons;
    FChronosWidgetFocus SidebarFocus;
    float AppliedMenuScale = 1.f;
    bool bHorizontalStickHeld = false;
};
