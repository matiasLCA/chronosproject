#pragma once

#include "ChronosMenuSettings.h"
#include "InputCoreTypes.h"
#include "ChronosControls.generated.h"

class APlayerController;
class UEnhancedInputUserSettings;
class UInputMappingContext;
class UWidget;

// YukoDev — CONFIGURACIÓN DE CONTROLES
namespace ChronosControls
{
    CHRONOSHUD_API const UInputMappingContext* Context();
    UEnhancedInputUserSettings* Settings(APlayerController* Player);
    FKey Key(APlayerController* Player, FName Name, FKey Fallback);
}

UCLASS()
class CHRONOSHUD_API UChronosControls : public UChronosOptionsMenu
{
    GENERATED_BODY()

protected:
    void NativeConstruct() override;
    void NativeDestruct() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UWidget> ControlList;
};
