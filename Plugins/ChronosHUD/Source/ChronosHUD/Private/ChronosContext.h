#pragma once

#include "Blueprint/UserWidget.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ChronosContext.generated.h"

class APlayerController;
class UFont;

UCLASS()
class CHRONOSHUD_API UChronosContext : public UUserWidget
{
    GENERATED_BODY()
public:
    void Update(AActor* Player, bool bBlocked);
    static FKey ActionKey(APlayerController* Player, FName Action, FKey Fallback);
protected:
    int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
private:
    UPROPERTY(Transient) TObjectPtr<UFont> Font;
    FText Action;
    FKey InteractKey;
    bool bHoldingBeer = false;
    bool bHidden = true;
    double NextSample = 0.;
};

UCLASS()
class CHRONOSHUD_API UChronosInteraction : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="Chronos|Interaction")
    static bool IsAllowed(AActor* Player, AActor* Target);
    UFUNCTION(BlueprintCallable, Category="Chronos|Interaction")
    static void Interact(AActor* Player);
    static AActor* ResolveTarget(AActor* Player);
    static AActor* TraceLook(AActor* Player, FHitResult& Hit);
    UFUNCTION(BlueprintPure, Category="Chronos|Interaction")
    static bool CanPresentThought(AActor* Player);
    UFUNCTION(BlueprintPure, Category="Chronos|Interaction")
    static bool HasActiveLakeInvestigation(AActor* Player);
};
