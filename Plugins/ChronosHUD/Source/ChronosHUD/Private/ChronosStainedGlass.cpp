#include "ChronosStainedGlass.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h"

namespace
{
    UObject* EnvironmentObject(UObject* Owner, FName Name)
    {
        const auto* Property = Owner ? FindFProperty<FObjectPropertyBase>(Owner->GetClass(), Name) : nullptr;
        return Property ? Property->GetObjectPropertyValue_InContainer(Owner) : nullptr;
    }

    float EnvironmentNumber(UObject* Owner, FName Name, float Fallback = 0.f)
    {
        const auto* Property = Owner ? FindFProperty<FNumericProperty>(Owner->GetClass(), Name) : nullptr;
        if (!Property || !Property->IsFloatingPoint()) return Fallback;
        return Property->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Owner));
    }
}

AChronosStainedGlass::AChronosStainedGlass()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Ventana"));
    Projection = CreateDefaultSubobject<USpotLightComponent>(TEXT("ProyeccionRGB"));
    Projection->SetupAttachment(RootComponent);
    Projection->SetMobility(EComponentMobility::Movable);
    Projection->SetIntensityUnits(ELightUnits::Lumens);
    Projection->SetIntensity(0);
    Projection->SetInnerConeAngle(6.f);
    Projection->SetOuterConeAngle(8.f);
    Projection->SetAttenuationRadius(MaximumDistance);
    Projection->SetLightFunctionScale(FVector(.5f, 1.f, 1.f));
    Projection->SetCastShadows(true);
    Projection->SetVolumetricScatteringIntensity(.08f);
    Projection->ShadowResolutionScale = .5f;

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .1f;
}

void AChronosStainedGlass::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateProjection();
}

void AChronosStainedGlass::Tick(float Seconds)
{
    Super::Tick(Seconds);
    UpdateProjection();
}

void AChronosStainedGlass::FindEnvironment()
{
    UWorld* World = GetWorld();
    if (!World || World->GetTimeSeconds() < NextEnvironmentSearch) return;
    NextEnvironmentSearch = World->GetTimeSeconds() + 2.;
    if (!ExistingWeather.IsValid()) ExistingWeather = EnvironmentObject(World->GetGameState(), TEXT("gestorClima"));
    if (!ExistingClouds.IsValid())
        for (TActorIterator<AVolumetricCloud> It(World); It; ++It)
            if (auto* Component = It->FindComponentByClass<UVolumetricCloudComponent>())
            { ExistingClouds = Component; break; }
}

void AChronosStainedGlass::UpdateProjection()
{
    Projection->SetLightFunctionMaterial(Pattern);
    Projection->SetAttenuationRadius(MaximumDistance);
    const auto* SunLight = Sun ? Sun->GetComponentByClass<UDirectionalLightComponent>() : nullptr;
    if (!SunLight)
    {
        Projection->SetIntensity(0.f);
        return;
    }
    const FVector SunRay = SunLight->GetForwardVector();
    const bool bNight = SunRay.Z > -.025f;
    if (bFollowExistingWeather) FindEnvironment();
    UObject* Weather = bFollowExistingWeather ? ExistingWeather.Get() : nullptr;
    auto* ExistingNightLight = Cast<UDirectionalLightComponent>(EnvironmentObject(Weather, TEXT("luzNoche")));
    const auto* MoonLight = Moon ? Moon->GetComponentByClass<UDirectionalLightComponent>() : nullptr;
    const FVector Ray = bNight ? (MoonLight ? MoonLight->GetForwardVector() : -SunRay) : SunRay;
    // A wide cone turns the whole church into a slide projector. Keep the rays
    // close to the solar/lunar direction and preserve brightness as it narrows.
    const float RayElevation = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(-Ray.Z, 0., 1.)));
    const float BeamSpread = FMath::Clamp(RayElevation * .22f, 2.f, MaximumBeamSpread);
    Projection->SetInnerConeAngle(BeamSpread * .75f);
    Projection->SetOuterConeAngle(BeamSpread);
    const float ConeEnergy = (1.f - FMath::Cos(FMath::DegreesToRadians(BeamSpread)))
        / (1.f - FMath::Cos(FMath::DegreesToRadians(36.f)));
    const FVector Normal = InwardNormal.GetSafeNormal();
    const float Incidence = FMath::Max(0.f, FVector::DotProduct(Ray, Normal));
    const float Elevation = FMath::SmoothStep(0., .1, -Ray.Z);
    const float DayBrightness = FMath::Clamp(SunLight->Intensity / FMath::Max(.01f, SolarReferenceLux), 0.f, 2.f);
    const float NightBrightness = MoonLight ? FMath::Clamp(MoonLight->Intensity / .15f, 0.f, 2.f)
        : ExistingNightLight ? FMath::Clamp(ExistingNightLight->Intensity / FMath::Max(.001f, LunarReferenceLux), 0.f, 2.f) : 1.f;
    const float Brightness = bNight ? NightBrightness : DayBrightness;
    float Cover = FMath::Clamp(CloudCover, 0.f, 1.f);
    if (Weather)
        Cover = FMath::Max(Cover, FMath::Clamp(EnvironmentNumber(Weather, TEXT("alphaLluvia")), 0.f, 1.f));
    if (bFollowExistingWeather && ExistingClouds.IsValid())
        if (const UMaterialInterface* CloudMaterial = ExistingClouds->Material.Get())
        {
            float Coverage = -.2f;
            if (CloudMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Cloud_GlobalCoverage")), Coverage))
                Cover = FMath::Max(Cover, FMath::Clamp((Coverage + .2f) / 1.2f, 0.f, 1.f));
        }
    const float Fog = Weather ? FMath::Clamp(EnvironmentNumber(Weather, TEXT("alphaNiebla")), 0.f, 1.f) : 0.f;
    WeatherTransmission = (1.f - .85f * Cover) * (1.f - .55f * Fog);
    float Power = (bNight ? NightLumens : DayLumens) * Brightness * Incidence * Elevation
        * WeatherTransmission * ConeEnergy;
    if (const auto* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr; PC && PC->PlayerCameraManager)
    {
        const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
        const float Distance = FVector::DistSquared(Eye, GetActorLocation());
        int32 Nearer = 0;
        for (TActorIterator<AChronosStainedGlass> It(GetWorld()); It; ++It)
            if (&**It != this && FVector::DotProduct(It->InwardNormal.GetSafeNormal(), Ray) > .01f
                && (FVector::DistSquared(Eye, It->GetActorLocation()) < Distance
                    || (FVector::DistSquared(Eye, It->GetActorLocation()) == Distance && It->GetUniqueID() < GetUniqueID()))) ++Nearer;
        const float Fade = 1.f - FMath::SmoothStep(2200.f, 3000.f, FMath::Sqrt(Distance));
        Power *= Nearer < 4 ? Fade : 0.f;
    }
    FRotator Orientation = Ray.Rotation();
    Orientation.Roll = PatternRoll;
    Projection->SetWorldRotation(Orientation);
    Projection->SetWorldLocation(GetActorLocation() + Normal * InteriorOffset);
    const FLinearColor MoonTint(.48f, .62f, 1.f);
    const FLinearColor NightColor = MoonLight ? MoonTint * MoonLight->GetLightColor()
        : ExistingNightLight ? MoonTint * ExistingNightLight->GetLightColor() : MoonTint;
    Projection->SetLightColor(bNight ? NightColor : SunLight->GetLightColor());
    Projection->SetIntensity(Power);
}

