#pragma once

#include "Blueprint/UserWidget.h"
#include "ChronosMenuFocus.h"
#include "ChronosDefeatMenu.generated.h"

class UButton;

UCLASS()
class CHRONOSHUD_API UChronosDefeatMenu : public UUserWidget
{
    GENERATED_BODY()

protected:
    void NativeConstruct() override;
    void NativeDestruct() override;
    void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    FReply NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event) override;
    FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    FReply NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override;

private:
    void Select(int32 Index);

    UPROPERTY(Transient) TObjectPtr<UButton> MainMenuButton;
    UPROPERTY(Transient) TObjectPtr<UButton> ReplayButton;
    FChronosMenuStick NavigationStick;
    FChronosWidgetFocus FocusGlow;
    TMap<TWeakObjectPtr<UUserWidget>, ESlateVisibility> HiddenObjectives;
    int32 Selection = 1;
    bool bTravelRequested = false;
    bool bNeedsInitialFocus = false;
};
