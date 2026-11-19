#pragma once

#include "Blueprint/UserWidget.h"
#include "ChronosMenuFocus.h"
#include "ChronosSaveMenus.generated.h"

class UChronosPauseMenu;
class UButton;

UCLASS()
class CHRONOSHUD_API UChronosMainMenu : public UUserWidget
{
    GENERATED_BODY()
protected:
    void NativeConstruct() override;
    void NativeDestruct() override;
    void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;

private:
    TArray<TWeakObjectPtr<UButton>> MenuButtons;
    FChronosWidgetFocus MenuFocus;
};

UCLASS()
class CHRONOSHUD_API UChronosSaveBrowser : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetPauseOrigin(UChronosPauseMenu& Pause, bool bSave);
    bool IsSaving() const { return bSaving; }
    bool IsFromPause() const { return Origin.IsValid(); }
    void GoBack();
    bool WriteSlot(int32 Slot);
    bool LoadSlot(int32 Slot);

protected:
    void NativeConstruct() override;
    void NativeDestruct() override;
    FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    FReply NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event) override;

private:
    TWeakObjectPtr<UChronosPauseMenu> Origin;

    UPROPERTY(Transient)
    TObjectPtr<UWidget> Host;

    TFunction<FReply()> CancelOrBack;
    TWeakPtr<SWidget> InitialFocus;
    bool bSaving = false;
};

UCLASS()
class CHRONOSHUD_API UChronosPauseMenu : public UUserWidget
{
    GENERATED_BODY()

public:
    static bool IsOpen(const UObject* Context);
    void FocusBrowserEntry(bool bSaving);
    void FocusOptionsEntry();

    UFUNCTION(BlueprintCallable, Category="YukoDev|Partidas")
    void OpenSaveBrowser();

protected:
    void NativeConstruct() override;
    void NativeDestruct() override;
    FReply NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event) override;
    FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;

private:
    UFUNCTION()
    void OpenLoadBrowser();

    UFUNCTION()
    void SoundLoadHover();

    void OpenBrowser(bool bSave);

    UPROPERTY(Transient)
    TObjectPtr<UButton> LoadButton;

    UPROPERTY(Transient)
    TObjectPtr<UChronosSaveBrowser> Browser;
    TArray<TPair<TWeakObjectPtr<UButton>, FButtonStyle>> PauseButtons;
    TWeakObjectPtr<UButton> PreviousHover;
    TWeakObjectPtr<UButton> PreviousFocus;
    TWeakObjectPtr<UButton> SelectedButton;
    bool bPointerHighlight = false;
};
