#pragma once

#include "ChronosMenuSettings.h"
#include "InputCoreTypes.h"
#include "ChronosControls.generated.h"

class APlayerController;
class UEnhancedInputUserSettings;
class UInputMappingContext;
class UWidget;
struct FSlateBrush;

// YukoDev — CONFIGURACIÓN DE CONTROLES
namespace ChronosControls
{
    CHRONOSHUD_API const UInputMappingContext* Context();
    const UInputMappingContext* ItemContext();
    bool CanShare(FName A, FName B);
    UEnhancedInputUserSettings* Settings(APlayerController* Player);
    FKey Key(APlayerController* Player, FName Name, FKey Fallback);
    const FSlateBrush* Glyph(FKey Key, bool bPlayStation);
}

UCLASS()
class CHRONOSHUD_API UChronosControls : public UChronosOptionsMenu
{
    GENERATED_BODY()

protected:
    void NativeConstruct() override;
};
