#include "ChronosSaveThumbnail.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "RHI.h"
#include "TextureResource.h"

namespace ChronosSaveThumbnail
{
    // YukoDev — CAPTURAS DE GUARDADO
    bool Capture(APlayerController& Controller, TArray<uint8>& Encoded)
    {
        UWorld* World = Controller.GetWorld();
        const APlayerCameraManager* Camera = Controller.PlayerCameraManager;
        if (!World || !World->Scene || !Camera || !FApp::CanEverRender()) return false;

        const FMinimalViewInfo& View = Camera->GetCameraCacheView();
        FActorSpawnParameters Spawn;
        Spawn.ObjectFlags = RF_Transient;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ASceneCapture2D* Actor = World->SpawnActor<ASceneCapture2D>(View.Location, View.Rotation, Spawn);
        if (!Actor) return false;

        auto* Target = NewObject<UTextureRenderTarget2D>(Actor);
        Target->ClearColor = FLinearColor::Black;
        Target->InitCustomFormat(Width, Height, PF_B8G8R8A8, false);
        Target->UpdateResourceImmediate(true);

        USceneCaptureComponent2D* Capture = Actor->GetCaptureComponent2D();
        Capture->bCaptureEveryFrame = false;
        Capture->bCaptureOnMovement = false;
        Capture->SetCameraView(View);
        Capture->PostProcessSettings = View.PostProcessSettings;
        Capture->PostProcessBlendWeight = View.PostProcessBlendWeight;
        Capture->ShowFlags.SetMotionBlur(false);
        Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
        Capture->TextureTarget = Target;
        Capture->CaptureScene();

        TArray<FColor> Pixels;
        const bool bRead = Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, FReadSurfaceDataFlags(RCM_UNorm));
        Actor->Destroy();
        if (!bRead || Pixels.Num() != Width * Height) return false;
        for (FColor& Pixel : Pixels) Pixel.A = 255;

        TArray64<uint8> Png;
        if (!FImageUtils::CompressImage(Png, TEXT("png"), FImageView(Pixels.GetData(), Width, Height))) return false;
        Encoded = TArray<uint8>(Png.GetData(), Png.Num());
        return true;
    }

    UTexture2D* Decode(const TArray<uint8>& Encoded)
    {
        if (Encoded.Num() < 24 || Encoded.Num() > 4 * Width * Height + 4096) return nullptr;
        constexpr uint8 Header[] = {
            137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 'I', 'H', 'D', 'R',
            0, 0, Width >> 8, Width & 255, 0, 0, Height >> 8, Height & 255
        };
        if (FMemory::Memcmp(Encoded.GetData(), Header, UE_ARRAY_COUNT(Header)) != 0) return nullptr;
        FImage Image;
        if (!FImageUtils::DecompressImage(Encoded.GetData(), Encoded.Num(), Image)) return nullptr;
        if (Image.SizeX != Width || Image.SizeY != Height || Image.NumSlices != 1) return nullptr;
        return UTexture2D::CreateTransientFromImage(&Image);
    }
}
