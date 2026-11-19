#include "ChronosBeerAnimation.h"
#include "ChronosFridge.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

void UChronosBeerAnimation::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    DrinkArmAlpha = 0.f;
    const auto* Pawn = TryGetPawnOwner();
    const auto* Skel = GetSkelMeshComponent();
    const auto* Session = Pawn && GetWorld() ? GetWorld()->GetSubsystem<UChronosFridgeSession>() : nullptr;
    const auto* Can = Session ? Session->GetHeld() : nullptr;
    if (!Can || !Skel || Can->GetAttachParentActor() != Pawn) return;
    DrinkArmAlpha = Can->GetDrinkLift();
    if (DrinkArmAlpha <= 0.f) return;
    const auto& ToWorld = Skel->GetComponentTransform();
    const FVector Mouth = Skel->GetSocketLocation(TEXT("head")) + Pawn->GetActorForwardVector() * 17.f;
    const FVector Wrist = Mouth + Pawn->GetActorRightVector() * 7.f - FVector(0, 0, 6);
    const FVector Elbow = Skel->GetSocketLocation(TEXT("upperarm_r")) + Pawn->GetActorRightVector() * 50.f
        + Pawn->GetActorForwardVector() * 8.f - FVector(0, 0, 18);
    DrinkHandTarget = ToWorld.InverseTransformPosition(Wrist);
    DrinkElbowTarget = ToWorld.InverseTransformPosition(Elbow);
}
