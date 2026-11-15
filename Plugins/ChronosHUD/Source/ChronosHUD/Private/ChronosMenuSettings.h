#pragma once

#include "Blueprint/UserWidget.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Styling/SlateTypes.h"
#include "ChronosMenuSettings.generated.h"

class UAudioComponent;
class UButton;
class UChronosHUDLayer;
class UWidget;

// YukoDev — HUD Y ACCESIBILIDAD
struct FChronosHUDScales
{
    int32 Life = 100;
    int32 Stamina = 100;
    int32 Lamp = 100;
    int32 Map = 100;
    int32 Calendar = 100;

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
    bool HasLiveHUD() const { return LiveHUD.IsValid(); }
    static UChronosMenuSettings* Get(const UObject* WorldContext);
    int32 MusicVolume() const { return MenuMusicPercent; }
    bool SetMusicVolume(int32 Percentage);

private:
    bool UpdateMenuMusic(float Seconds);
    void MusicFinished(UAudioComponent* Component);

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> MenuMusic;
    TWeakObjectPtr<UChronosHUDLayer> LiveHUD;
    FChronosHUDScales Confirmed;
    FTSTicker::FDelegateHandle MusicTicker;
    bool bWantsMusic = false;
    bool bFading = false;
    int32 MenuMusicPercent = 35;
};

UCLASS()
class CHRONOSHUD_API UChronosOptionsMenu : public UUserWidget
{
    GENERATED_BODY()

protected:
    void NativeConstruct() override;
    void NativeDestruct() override;
    void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    FReply NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event) override;

private:
    UWidget* InitialFocus() const;
    void RestorePauseFocus();
    UFUNCTION()
    void OpenHUDSettings();
    void CloseHUDSettings();
    void ThemeExistingPanel();
    void FormatSelector(class UHorizontalBox& Row, int32 Index, TArray<TPair<UButton*, UButton*>>& Arrows);
    void AddMenuMusicRow();
    void AdjustMenuMusic(int32 Delta);
    UFUNCTION()
    void LowerMenuMusic();
    UFUNCTION()
    void RaiseMenuMusic();

    UPROPERTY(Transient)
    TObjectPtr<UWidget> HUDPanel;
    UPROPERTY(Transient)
    TObjectPtr<UChronosHUDLayer> SampleHUD;
    UPROPERTY(Transient)
    TObjectPtr<class UTextBlock> MusicValue;
    TArray<TWeakObjectPtr<UButton>> SelectorButtons;
    TArray<TPair<TWeakObjectPtr<UWidget>, ESlateVisibility>> SectionVisibility;
    TArray<TPair<TWeakObjectPtr<UButton>, FButtonStyle>> NavigationStyles;
};
