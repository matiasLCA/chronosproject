#pragma once

#include "Blueprint/UserWidget.h"
#include "ChronosTutorial.generated.h"

UCLASS()
class CHRONOSHUD_API UChronosTutorial : public UUserWidget
{
    GENERATED_BODY()
protected:
    void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
private:
    float SampleIn = 0.f;
};
