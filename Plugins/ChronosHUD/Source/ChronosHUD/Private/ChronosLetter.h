#pragma once

#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "ChronosLetter.generated.h"

UCLASS()
class CHRONOSHUD_API UChronosLetter : public UUserWidget
{
    GENERATED_BODY()

public:
    bool IsReading() const { return bReading; }
    FKey CloseKey() const;
    int32 ZoomPercent() const;

protected:
    void NativeConstruct() override;
    void NativeDestruct() override;
    void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    FReply NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override;
    FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
    FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
    void AdjustZoom(double Delta);
    void Pan(FVector2D Delta);
    void Close();
    void RestoreHints();

    TWeakObjectPtr<AActor> Source;
    TMap<TWeakObjectPtr<UUserWidget>, ESlateVisibility> ObscuredHints;
    FVector2D PadAxis = FVector2D::ZeroVector;
    bool bReading = false;
    bool bFocusPending = false;
    bool bShowCursorBefore = false;
};

