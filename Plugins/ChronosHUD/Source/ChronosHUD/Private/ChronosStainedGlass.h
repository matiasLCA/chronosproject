#pragma once

#include "GameFramework/Actor.h"
#include "ChronosStainedGlass.generated.h"

class ADirectionalLight;
class UMaterialInterface;
class USpotLightComponent;
class UVolumetricCloudComponent;

UCLASS()
class CHRONOSHUD_API AChronosStainedGlass : public AActor
{
    GENERATED_BODY()

public:
    AChronosStainedGlass();
    void Tick(float Seconds) override;
    bool ShouldTickIfViewportsOnly() const override { return true; }
    void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitral")
    TObjectPtr<ADirectionalLight> Sun;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitral")
    TObjectPtr<ADirectionalLight> Moon;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitral")
    TObjectPtr<UMaterialInterface> Pattern;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitral")
    FVector InwardNormal = FVector(1, 0, 0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitral", meta=(ClampMin="0"))
    float DayLumens = 600.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitral", meta=(ClampMin="0.01"))
    float SolarReferenceLux = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitral", meta=(ClampMin="0"))
    float NightLumens = 12.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitral", meta=(ClampMin="2", ClampMax="12"))
    float MaximumBeamSpread = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitral", meta=(ClampMin="0", ClampMax="1"))
    float CloudCover = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitral|Clima")
    bool bFollowExistingWeather = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitral|Clima", meta=(ClampMin="0.001"))
    float LunarReferenceLux = .018375f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitral", meta=(ClampMin="0", ClampMax="150"))
    float InteriorOffset = 55.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitral")
    float PatternRoll = 90.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Vitral|Clima")
    float WeatherTransmission = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitral", meta=(ClampMin="100"))
    float MaximumDistance = 1750.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vitral")
    TObjectPtr<USpotLightComponent> Projection;

private:
    void UpdateProjection();
    void FindEnvironment();
    TWeakObjectPtr<UObject> ExistingWeather;
    TWeakObjectPtr<UVolumetricCloudComponent> ExistingClouds;
    double NextEnvironmentSearch = 0.;
};
