#include "ChronosContext.h"
#include "ChronosControls.h"
#include "ChronosConversation.h"
#include "ChronosInventory.h"
#include "ChronosFridge.h"
#include "ChronosGamepad.h"
#include "ChronosHUDLayer.h"
#include "ChronosMenuArt.h"
#include "ChronosUrns.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
    const AActor* LakeEvent(const AActor* Player)
    {
        auto* Class = FindObject<UClass>(nullptr, TEXT("/Game/Interactivo/cadaver_mision/BP_EventoLago.BP_EventoLago_C"));
        return IsValid(Player) && Class ? UGameplayStatics::GetActorOfClass(Player, Class) : nullptr;
    }

    AActor* FocusLakeBody(AActor* Player, const FVector& Start, const FVector& Forward, const FCollisionQueryParams& Params)
    {
        const auto* Event = LakeEvent(Player);
        const auto* BagProperty = Event ? FindFProperty<FObjectPropertyBase>(Event->GetClass(), TEXT("bolsa")) : nullptr;
        auto* Body = BagProperty ? Cast<AChronosBody>(BagProperty->GetObjectPropertyValue_InContainer(Event)) : nullptr;
        if (!IsValid(Body) || Body->GetAttachParentActor() || !UChronosInteraction::IsAllowed(Player, Body)) return nullptr;

        const auto* Taken = FindFProperty<FBoolProperty>(Body->GetClass(), TEXT("decisionTomada"));
        const auto* Carry = FindFProperty<FBoolProperty>(Body->GetClass(), TEXT("decidido"));
        if (Taken && Taken->GetPropertyValue_InContainer(Body) && (!Carry || !Carry->GetPropertyValue_InContainer(Body))) return nullptr;

        const auto* MeshProperty = FindFProperty<FObjectPropertyBase>(Body->GetClass(), TEXT("CADAVER"));
        const auto* Mesh = MeshProperty ? Cast<UStaticMeshComponent>(MeshProperty->GetObjectPropertyValue_InContainer(Body)) : nullptr;
        if (!Mesh || !Mesh->IsVisible()) return nullptr;
        const FVector Point = Mesh->Bounds.Origin + FVector(0, 0, Mesh->Bounds.BoxExtent.Z * .5);
        if (FVector::DistSquared(Player->GetActorLocation(), Point) > FMath::Square(290.)
            || FVector::DotProduct((Point - Start).GetSafeNormal(), Forward) < .70) return nullptr;

        FCollisionQueryParams Visibility = Params;
        // The lake trigger surrounds the body and blocks Visibility with an initial overlap.
        if (const auto* ZoneProperty = FindFProperty<FObjectPropertyBase>(Event->GetClass(), TEXT("zona")))
            if (const auto* Zone = Cast<UPrimitiveComponent>(ZoneProperty->GetObjectPropertyValue_InContainer(Event)))
                Visibility.AddIgnoredComponent(Zone);
        FHitResult Obstacle;
        if (Player->GetWorld()->LineTraceSingleByChannel(Obstacle, Start, Point, ECC_Visibility, Visibility)
            && Obstacle.GetActor() != Body) return nullptr;
        return Body;
    }
}

FKey UChronosContext::ActionKey(APlayerController* Player, FName Action, FKey Fallback)
{
    auto* Pad = UChronosGamepad::Get(Player);
    if (Pad && Pad->IsGamepadActive()) return Pad->Key(Action, Pad->IsAlternate());
    return ChronosControls::Key(Player, Action, Fallback);
}

bool UChronosInteraction::CanPresentThought(AActor* Player)
{
    if (!IsValid(Player) || Player->GetWorld()->IsPaused() || UChronosConversation::IsActive(Player) || UChronosInventory::IsOpen(Player)) return false;
    const auto* HUD = Player->GetWorld()->GetSubsystem<UChronosHUDSubsystem>();
    if (HUD && HUD->IsReadingLetter()) return false;
    const auto* Intro = FindFProperty<FBoolProperty>(Player->GetClass(), TEXT("enDespertar"));
    if (Intro && Intro->GetPropertyValue_InContainer(Player)) return false;
    const auto* Pawn = Cast<APawn>(Player);
    const auto* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    return !Controller || !Controller->bCinematicMode;
}

bool UChronosInteraction::HasActiveLakeInvestigation(AActor* Player)
{
    const auto* Event = LakeEvent(Player);
    if (!IsValid(Event)) return false;
    const auto* Active = FindFProperty<FBoolProperty>(Event->GetClass(), TEXT("activo"));
    const auto* Fired = FindFProperty<FBoolProperty>(Event->GetClass(), TEXT("disparado"));
    return Active && Fired && Active->GetPropertyValue_InContainer(Event) && !Fired->GetPropertyValue_InContainer(Event);
}

AActor* UChronosInteraction::TraceLook(AActor* Player, FHitResult& Hit)
{
    const auto* Camera = IsValid(Player) ? Player->FindComponentByClass<UCameraComponent>() : nullptr;
    if (!Camera) return nullptr;
    const FVector Start = Camera->GetComponentLocation();
    const FCollisionQueryParams Params(SCENE_QUERY_STAT(ChronosFocus), false, Player);
    Player->GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Camera->GetForwardVector() * 400.f, ECC_Visibility, Params);
    return Hit.GetActor();
}

bool UChronosInteraction::IsAllowed(AActor* Player, AActor* Target)
{
    if (!IsValid(Player) || !IsValid(Target) || !CanPresentThought(Player)
        || FVector::DistSquared(Player->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(400.f)) return false;
    const auto* Session = Player->GetWorld()->GetSubsystem<UChronosFridgeSession>();
    if (Session && Session->GetHeld() && Session->GetHeld()->IsDrinking()) return false;
    if (const auto* Fridge = Cast<AChronosFridgeTarget>(Target)) return Fridge->CanUseFridgeTarget(Player);
    static auto* Interaction = LoadClass<UInterface>(nullptr, TEXT("/Game/Characters/interaction/bpi_interaction.bpi_interaction_C"));
    if (!Interaction || !Target->GetClass()->ImplementsInterface(Interaction)) return false;
    auto* Condition = Target->FindFunction(TEXT("puedeInteractuar"));
    if (!Condition) return true;
    FStructOnScope Arguments(Condition);
    if (const auto* Character = FindFProperty<FObjectPropertyBase>(Condition, TEXT("PlayerCharacter")))
        Character->SetObjectPropertyValue_InContainer(Arguments.GetStructMemory(), Player);
    Target->ProcessEvent(Condition, Arguments.GetStructMemory());
    const auto* Can = FindFProperty<FBoolProperty>(Condition, TEXT("puede"));
    return Can && Can->GetPropertyValue_InContainer(Arguments.GetStructMemory());
}

AActor* UChronosInteraction::ResolveTarget(AActor* Player)
{
    if (!CanPresentThought(Player)) return nullptr;
    FHitResult Look;
    auto* Focus = TraceLook(Player, Look);
    if (Cast<AChronosFridgeTarget>(Focus)) return IsAllowed(Player, Focus) ? Focus : nullptr;

    const auto* Camera = Player->FindComponentByClass<UCameraComponent>();
    if (!Camera) return nullptr;
    const FVector Start = Camera->GetComponentLocation();
    const FVector Forward = Camera->GetForwardVector();
    const FCollisionQueryParams Params(SCENE_QUERY_STAT(ChronosNearInteraction), false, Player);
    TArray<FHitResult> Hits;
    Player->GetWorld()->LineTraceMultiByChannel(Hits, Start, Start + Forward * 400.f, ECC_Visibility, Params);
    AActor* LookedPickup = nullptr;
    double BestAlignment = -1.;
    for (const auto& Hit : Hits)
    {
        auto* Candidate = Hit.GetActor();
        if (!IsAllowed(Player, Candidate)) continue;
        const double Alignment = FVector::DotProduct((Candidate->GetActorLocation() - Start).GetSafeNormal(), Forward);
        if (Alignment > BestAlignment)
        {
            BestAlignment = Alignment;
            LookedPickup = Candidate;
        }
    }
    if (LookedPickup) return LookedPickup;
    if (auto* LakeBody = FocusLakeBody(Player, Start, Forward, Params)) return LakeBody;

    const auto* InRange = FindFProperty<FArrayProperty>(Player->GetClass(), TEXT("InteractablesInRange"));
    const auto* Inner = InRange ? CastField<FObjectPropertyBase>(InRange->Inner) : nullptr;
    if (!Inner) return nullptr;
    const auto* Data = InRange->ContainerPtrToValuePtr<void>(Player);
    FScriptArrayHelper Candidates(InRange, Data);
    AActor* Best = nullptr;
    double BestScore = 0.;
    for (int32 Index = 0; Index < Candidates.Num(); ++Index)
    {
        auto* Candidate = Cast<AActor>(Inner->GetObjectPropertyValue(Candidates.GetRawPtr(Index)));
        if (!IsAllowed(Player, Candidate)) continue;
        FVector Center, Extent;
        Candidate->GetActorBounds(true, Center, Extent);
        const FVector Offset = Center - Start;
        const double Score = FVector::DotProduct(Offset.GetSafeNormal(), Camera->GetForwardVector());
        if (Score <= BestScore) continue;
        FHitResult Visibility;
        if (Player->GetWorld()->LineTraceSingleByChannel(Visibility, Start, Center, ECC_Visibility, Params)
            && Visibility.GetActor() != Candidate) continue;
        Best = Candidate;
        BestScore = Score;
    }
    return Best;
}

void UChronosInteraction::Interact(AActor* Player)
{
    const auto* Session = IsValid(Player) ? Player->GetWorld()->GetSubsystem<UChronosFridgeSession>() : nullptr;
    if (Session && Session->PickedUpThisFrame()) return;
    auto* Target = ResolveTarget(Player);
    if (!Target || Cast<AChronosBeer>(Target)) return;
    auto* Function = Target->FindFunction(TEXT("Interact"));
    const auto* Character = Function ? FindFProperty<FObjectPropertyBase>(Function, TEXT("PlayerCharacter")) : nullptr;
    if (!Character || !Player->IsA(Character->PropertyClass)) return;
    if (const auto* Active = FindFProperty<FObjectPropertyBase>(Player->GetClass(), TEXT("ActiveInteractable")))
        Active->SetObjectPropertyValue_InContainer(Player, Target);
    FStructOnScope Arguments(Function);
    Character->SetObjectPropertyValue_InContainer(Arguments.GetStructMemory(), Player);
    Target->ProcessEvent(Function, Arguments.GetStructMemory());
}

void UChronosContext::Update(AActor* Player, bool bBlocked)
{
    bHidden = bBlocked || !UChronosInteraction::CanPresentThought(Player);
    if (bHidden)
    {
        NextSample = 0.;
        return;
    }
    const double Now = FPlatformTime::Seconds();
    if (Now < NextSample) return;
    NextSample = Now + 1. / 15.;
    Action = FText::GetEmpty();
    bHoldingBeer = false;
    if (!Font) Font = LoadObject<UFont>(nullptr, TEXT("/Game/assets/Cementerio/Tipografia/TrajanPro3SemiBold_Font.TrajanPro3SemiBold_Font"));
    auto* PC = GetOwningPlayer();
    InteractKey = ActionKey(PC, TEXT("Chronos.Interact"), EKeys::E);
    if (const auto* Session = GetWorld()->GetSubsystem<UChronosFridgeSession>())
        bHoldingBeer = Session->GetHeld() && !Session->GetHeld()->IsDrinking();
    auto* Target = UChronosInteraction::ResolveTarget(Player);
    if (const auto* Property = FindFProperty<FObjectPropertyBase>(Player->GetClass(), TEXT("CurrentInteractable")))
        Property->SetObjectPropertyValue_InContainer(Player, Target);
    if (auto* Check = Player->FindFunction(TEXT("CheckInteractionLook"))) Player->ProcessEvent(Check, nullptr);
    if (!Target) return;
    if (auto* Fixture = Cast<AChronosFridge>(Target))
        Action = FText::FromString(Fixture->HasShelfAccess() ? TEXT("Cerrar refrigerador") : TEXT("Abrir refrigerador"));
    else if (Cast<AChronosBeer>(Target))
    {
        Action = FText::FromString(TEXT("Recoger cerveza"));
        InteractKey = ActionKey(PC, TEXT("Chronos.PickupBeer"), EKeys::T);
    }
    else
    {
        const FString Class = Target->GetClass()->GetName();
        const TCHAR* Label = TEXT("Interactuar");
        if (Class.StartsWith(TEXT("BP-Notas")) || Class.StartsWith(TEXT("BP_Nota"))) Label = TEXT("Leer carta");
        else if (Class.Contains(TEXT("Visitante"))) Label = TEXT("Hablar");
        else if (Class.Contains(TEXT("slotAnfora"))) Label = TEXT("Usar estante");
        else if (Class.Contains(TEXT("anfora"))) Label = TEXT("Recoger ánfora");
        else if (Class.Contains(TEXT("Trash_Bin")))
            Label = UChronosUrns::CanUseBin(Target, Player) ? TEXT("Desechar ánfora del lago") : TEXT("Usar basurero");
        else if (Class.Contains(TEXT("CadaverMision"))) Label = TEXT("Examinar cuerpo");
        Action = FText::FromString(Label);
        if (const auto* Body = Cast<AChronosBody>(Target); Body && !Body->DeceasedName.IsEmpty())
            Action = FText::Format(NSLOCTEXT("Chronos", "ExamineNamedBody", "Examinar: {0}"), Body->DeceasedName);
    }
}

int32 UChronosContext::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
    FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
    Layer = Super::NativePaint(Args, Geometry, Culling, Elements, Layer, Style, bEnabled);
    if (bHidden || (!bHoldingBeer && Action.IsEmpty())) return Layer;
    const FVector2D Size = Geometry.GetLocalSize();
    const float Scale = FMath::Min(Size.X / 1920.f, Size.Y / 1080.f);
    const auto* Pad = UChronosGamepad::Get(GetOwningPlayer());
    auto Row = [&](FKey Key, const FText& Text, int32 Index)
    {
        const auto* Glyph = Pad && Key.IsGamepadKey() ? ChronosControls::Glyph(Key, Pad->IsPlayStation()) : nullptr;
        FSlateFontInfo Face(Font.Get(), FMath::Max(1, FMath::RoundToInt(18 * Scale)));
        const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
        const float Dpi = Geometry.GetAccumulatedLayoutTransform().GetScale();
        const FText KeyLabel = Key.GetDisplayName();
        const auto Width = [&](const FText& Value) { return Measure->Measure(Value, Face, Dpi).X / Dpi; };
        const float MaxWidth = Size.X * .94f;
        const float EndPadding = 22.f * Scale;
        float TextOffset = 76.f * Scale;
        float FullWidth = 0.f;
        do
        {
            TextOffset = Glyph ? 76.f * Scale : FMath::Max(76.f * Scale, Width(KeyLabel) + 36.f * Scale);
            FullWidth = TextOffset + Width(Text) + EndPadding;
            if (FullWidth <= MaxWidth || Face.Size <= 1) break;
            Face.Size -= 1;
        } while (true);
        const FVector2f Dimensions(FMath::Min(MaxWidth, FMath::Max(440.f * Scale, FMath::CeilToFloat(FullWidth))), 51.f * Scale);
        const FVector2f Position((Size.X - Dimensions.X) * .5f, Size.Y * .78f + Index * 58 * Scale);
        const auto Geom = Geometry.ToPaintGeometry(Dimensions, FSlateLayoutTransform(Position));
        FSlateDrawElement::MakeBox(Elements, ++Layer, Geom, &ChronosMenuArt::OptionsPanel(), ESlateDrawEffect::None, FLinearColor::White);
        const float TextTop = (Dimensions.Y - Measure->Measure(Text, Face, Dpi).Y / Dpi) * .5f;
        const FLinearColor Gold(.88f, .76f, .59f);
        if (Glyph)
            FSlateDrawElement::MakeBox(Elements, ++Layer,
                Geometry.ToPaintGeometry(FVector2f(36 * Scale), FSlateLayoutTransform(Position + FVector2f(16, 7) * Scale)), Glyph);
        else
            FSlateDrawElement::MakeText(Elements, ++Layer,
                Geometry.ToPaintGeometry(Dimensions, FSlateLayoutTransform(Position + FVector2f(20.f * Scale, TextTop))),
                KeyLabel, Face, ESlateDrawEffect::None, Gold);
        FSlateDrawElement::MakeText(Elements, ++Layer,
            Geometry.ToPaintGeometry(Dimensions, FSlateLayoutTransform(Position + FVector2f(TextOffset, TextTop))),
            Text, Face, ESlateDrawEffect::None, Gold);
    };
    int32 Index = 0;
    if (!Action.IsEmpty()) Row(InteractKey, Action, Index++);
    if (bHoldingBeer)
    {
        Row(ActionKey(GetOwningPlayer(), TEXT("Chronos.DrinkItem"), EKeys::X),
            FText::FromString(TEXT("Beber")), Index++);
        Row(ActionKey(GetOwningPlayer(), TEXT("Chronos.PocketItem"), EKeys::R),
            FText::FromString(TEXT("Guardar en bolsillos")), Index++);
    }
    return Layer;
}
