#include "ChronosFridge.h"
#include "ChronosBeerAnimation.h"
#include "ChronosProgress.h"
#include "ChronosStamina.h"
#include "ChronosGamepad.h"
#include "ChronosContext.h"
#include "ChronosControls.h"
#include "ChronosConversation.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "ChronosFridge"
DEFINE_LOG_CATEGORY_STATIC(LogChronosFridge, Log, All);

namespace
{
    constexpr float DoorDuration = .5f;
    constexpr float OpenAngle = -100.f;
    constexpr float UseRange = 255.f;
    constexpr float DrinkDuration = 1.75f;
    const FName HandSocket(TEXT("hand_r"));
    const FName HeadSocket(TEXT("head"));

    UBoxComponent* AddCollision(AActor& Owner, FName Name, USceneComponent& Parent, FVector Center,
        FVector Extent, FName Profile = UCollisionProfile::BlockAll_ProfileName)
    {
        auto* Box = Owner.CreateDefaultSubobject<UBoxComponent>(Name);
        Box->SetupAttachment(&Parent);
        Box->SetRelativeLocation(Center);
        Box->InitBoxExtent(Extent);
        Box->SetCollisionProfileName(Profile);
        Box->SetGenerateOverlapEvents(false);
        return Box;
    }

    void AddClock(UTimelineComponent& Timeline, UCurveFloat& Curve, float Length, FOnTimelineFloat Update)
    {
        Timeline.AddInterpFloat(&Curve, Update);
        Timeline.SetTimelineLengthMode(ETimelineLengthMode::TL_TimelineLength);
        Timeline.SetTimelineLength(Length);
    }

    void ConfigureAudio(UAudioComponent& Audio, float Volume)
    {
        Audio.bAutoActivate = false;
        Audio.bOverrideAttenuation = true;
        Audio.AttenuationOverrides.bAttenuate = true;
        Audio.AttenuationOverrides.bSpatialize = true;
        Audio.AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
        Audio.AttenuationOverrides.AttenuationShapeExtents = FVector(160.f);
        Audio.AttenuationOverrides.FalloffDistance = 1100.f;
        Audio.SetVolumeMultiplier(Volume);
    }
}

bool AChronosFridgeTarget::CanUseFridgeTarget(AActor* Player) const
{
    FHitResult Hit;
    return IsValid(Player) && FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) <= FMath::Square(UseRange)
        && UChronosInteraction::TraceLook(Player, Hit) == this;
}

AChronosFridge::AChronosFridge()
{
    for (int32 Variant = 0; Variant < 2; ++Variant)
        for (int32 Row = 0; Row < 2; ++Row)
            for (int32 Column = 0; Column < 3; ++Column)
                StockPositions.Add(FVector((Variant ? 8.f : -26.f) + Column * 9.f, 6.f + Row * 12.f, 84.47f));
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Refrigerador"));
    Cabinet = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CuerpoRepisasYCongelador"));
    Cabinet->SetupAttachment(RootComponent);
    Cabinet->SetRelativeScale3D(FVector(1.34, 1.44849, 1.102495));
    Cabinet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyAsset(TEXT("/Game/TENCENT3D/SM_SM_Fridge.SM_SM_Fridge"));
    Cabinet->SetStaticMesh(BodyAsset.Object);

    Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("BisagraInferior"));
    Hinge->SetupAttachment(RootComponent);
    Hinge->SetRelativeLocation(FVector(43.7, 39.2, 73.6));
    LowerDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PuertaInferior"));
    LowerDoor->SetupAttachment(Hinge);
    LowerDoor->SetRelativeLocation(FVector(-43.7, 0, 0));
    LowerDoor->SetRelativeRotation(FRotator(0, 90, 0));
    LowerDoor->SetRelativeScale3D(FVector(1, 1.335, 1.1075));
    LowerDoor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> DoorAsset(TEXT("/Game/TENCENT3D/SM_Door_Fridge.SM_Door_Fridge"));
    LowerDoor->SetStaticMesh(DoorAsset.Object);

    DoorCollision = AddCollision(*this, TEXT("ColisionPuerta"), *Hinge, FVector(-43.7, 1.4, 0),
        FVector(43.3, 1.35, 69.8), UCollisionProfile::BlockAllDynamic_ProfileName);
    AddCollision(*this, TEXT("Lateral_Izquierdo"), *RootComponent, FVector(-42, 0, 101), FVector(2, 39, 100));
    AddCollision(*this, TEXT("Lateral_Derecho"), *RootComponent, FVector(42, 0, 101), FVector(2, 39, 100));
    AddCollision(*this, TEXT("Fondo"), *RootComponent, FVector(0, -36, 101), FVector(42, 3, 100));
    AddCollision(*this, TEXT("ColisionCongelador"), *RootComponent, FVector(0, 36.75, 177.05), FVector(43.7, 2, 30.31));
    AddCollision(*this, TEXT("Techo"), *RootComponent, FVector(0, 0, 204), FVector(43.7, 39, 2));
    AddCollision(*this, TEXT("Base"), *RootComponent, FVector(0, 0, 8), FVector(43.7, 39, 8));
    AddCollision(*this, TEXT("RepisaInferior"), *RootComponent, FVector(0, 0, 44.98), FVector(40.37, 34.76, .55));
    AddCollision(*this, TEXT("RepisaMedia"), *RootComponent, FVector(0, 0, 77.62), FVector(40.37, 34.76, .55));
    AddCollision(*this, TEXT("RepisaSuperior"), *RootComponent, FVector(0, 0, 110.69), FVector(40.37, 34.76, .55));
    AddCollision(*this, TEXT("SeparacionCongelador"), *RootComponent, FVector(0, 0, 145.5), FVector(40.37, 35, 4.82));

    auto* Range = AddCollision(*this, TEXT("AlcanceInteraccion"), *RootComponent, FVector(0, 95, 77),
        FVector(104, 111, 87), TEXT("OverlapAllDynamic"));
    Range->SetCollisionResponseToAllChannels(ECR_Ignore);
    Range->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Range->SetGenerateOverlapEvents(true);

    BlackCan = CreateDefaultSubobject<UChildActorComponent>(TEXT("LataBlackCrow"));
    BlackCan->SetupAttachment(RootComponent);
    BlackCan->SetRelativeLocation(FVector(-13, 19, 84.47));
    WhiteCan = CreateDefaultSubobject<UChildActorComponent>(TEXT("LataBlanca"));
    WhiteCan->SetupAttachment(RootComponent);
    WhiteCan->SetRelativeLocation(FVector(12, 19, 84.47));

    DoorTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("TimelinePuerta"));
    DoorCurve = CreateDefaultSubobject<UCurveFloat>(TEXT("ProgresoPuerta"));
    DoorCurve->FloatCurve.AddKey(0.f, 0.f);
    DoorCurve->FloatCurve.AddKey(DoorDuration, 1.f);
    DoorAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioPuerta"));
    DoorAudio->SetupAttachment(Hinge);
    ConfigureAudio(*DoorAudio, .8f);
    OpenAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioRefrigeradorAbierto"));
    OpenAudio->SetupAttachment(RootComponent);
    OpenAudio->SetRelativeLocation(FVector(0, 10, 100));
    ConfigureAudio(*OpenAudio, .35f);
    static ConstructorHelpers::FObjectFinder<USoundBase> DoorAudioAsset(TEXT("/Game/TENCENT3D/puerta_refri.puerta_refri"));
    static ConstructorHelpers::FObjectFinder<USoundBase> OpenAsset(TEXT("/Game/TENCENT3D/refri_abierto.refri_abierto"));
    DoorSound = DoorAudioAsset.Object;
    OpenSound = OpenAsset.Object;
}

void AChronosFridge::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ConfigureCan(*BlackCan, EChronosBeer::BlackCrow);
    ConfigureCan(*WhiteCan, EChronosBeer::White);
}

void AChronosFridge::ConfigureCan(UChildActorComponent& Slot, EChronosBeer Variant)
{
    Slot.SetChildActorClass(BeerClass);
    if (auto* Can = Cast<AChronosBeer>(Slot.GetChildActor())) Can->Stock(this, Variant);
}

void AChronosFridge::BeginPlay()
{
    Super::BeginPlay();
    BlackCan->SetChildActorClass(nullptr);
    WhiteCan->SetChildActorClass(nullptr);
    Stocked.SetNum(12);
    RefreshStock();
    GetWorld()->GetTimerManager().SetTimer(StockTimer, this, &AChronosFridge::RefreshStock, 1.f, true);
    FOnTimelineFloat Update;
    Update.BindDynamic(this, &AChronosFridge::MoveDoor);
    AddClock(*DoorTimeline, *DoorCurve, DoorDuration, Update);
    DoorAudio->SetSound(DoorSound);
    OpenAudio->SetSound(OpenSound);
    OpenAudio->OnAudioFinished.AddUniqueDynamic(this, &AChronosFridge::RepeatOpenSound);
}

void AChronosFridge::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorld()->GetTimerManager().ClearTimer(StockTimer);
    bOpenSoundActive = false;
    DoorAudio->Stop();
    OpenAudio->Stop();
    if (Reason == EEndPlayReason::RemovedFromWorld || Reason == EEndPlayReason::Destroyed)
        for (const auto& Can : Stocked)
            if (IsValid(Can)) Can->Destroy();
    Super::EndPlay(Reason);
}

FString AChronosFridge::StockId() const
{
    return UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()) + TEXT("|") + GetName();
}

void AChronosFridge::RefreshStock()
{
    auto* Progress = UChronosProgressState::Get(this);
    if (!Progress || !BeerClass || StockPositions.Num() != 12) return;
    if (!Calendar.IsValid())
    {
        auto* Class = LoadClass<AActor>(nullptr, TEXT("/Game/NPC/Visitors/LivingCemetery/BP_CemeteryDirector.BP_CemeteryDirector_C"));
        if (Class) for (TActorIterator<AActor> It(GetWorld(), Class); It; ++It) { Calendar = *It; break; }
    }
    auto& Record = Progress->Data.Refrigerators.FindOrAdd(StockId());
    const auto* DayProperty = Calendar.IsValid() ? FindFProperty<FIntProperty>(Calendar->GetClass(), TEXT("CalendarDay")) : nullptr;
    if (DayProperty)
    {
        const int32 Day = DayProperty->GetPropertyValue_InContainer(Calendar.Get());
        if (Day > Record.Day)
        {
            Record.Day = Day;
            Record.Remaining = 4095;
        }
    }
    for (int32 Index = 0; Index < 12; ++Index)
    {
        if (!(Record.Remaining & (1 << Index)) || IsValid(Stocked[Index])) continue;
        FActorSpawnParameters Parameters;
        Parameters.Owner = this;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Can = GetWorld()->SpawnActor<AChronosBeer>(BeerClass,
            GetActorTransform().TransformPosition(StockPositions[Index]), GetActorRotation(), Parameters);
        if (!Can) continue;
        Can->Stock(this, Index < 6 ? EChronosBeer::BlackCrow : EChronosBeer::White, Index);
        Stocked[Index] = Can;
    }
}

bool AChronosFridge::TakeSlot(int32 Index)
{
    auto* Progress = UChronosProgressState::Get(this);
    if (!Progress || Index < 0 || Index >= 12) return false;
    auto* Record = Progress->Data.Refrigerators.Find(StockId());
    if (!Record || !(Record->Remaining & (1 << Index))) return false;
    Record->Remaining &= ~(1 << Index);
    Stocked[Index] = nullptr;
    return true;
}

FText AChronosFridge::InteractionPrompt() const
{
    return bOpening ? LOCTEXT("Close", "Presiona E para cerrar el refrigerador")
        : LOCTEXT("Open", "Presiona E para abrir el refrigerador");
}

bool AChronosFridge::CanUseFridgeTarget(AActor* Player) const
{
    FHitResult Hit;
    return IsValid(Player) && FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) <= FMath::Square(UseRange)
        && UChronosInteraction::TraceLook(Player, Hit) == this
        && GetActorTransform().InverseTransformPosition(Hit.ImpactPoint).Z < 145.5;
}

void AChronosFridge::UseFridgeTarget(AActor* Player)
{
    if (!CanUseFridgeTarget(Player) || DoorTimeline->IsPlaying()) return;
    bOpening = !bOpening;
    UE_LOG(LogChronosFridge, Verbose, TEXT("Door input opening=%d position=%.3f"), bOpening, DoorPosition);
    if (bOpening) DoorTimeline->Play();
    else DoorTimeline->Reverse();
    DoorAudio->Play();
}

void AChronosFridge::RepeatOpenSound()
{
    if (bOpenSoundActive && OpenSound) OpenAudio->Play();
}

void AChronosFridge::UpdateOpenSound()
{
    const bool bAudible = FMath::Abs(DoorAngle) > 5.f;
    if (bAudible == bOpenSoundActive) return;
    bOpenSoundActive = bAudible;
    if (bAudible && OpenSound) OpenAudio->FadeIn(.18f, 1.f);
    else OpenAudio->FadeOut(.18f, 0.f);
}

bool AChronosFridge::IsDoorBlocked(float CandidateAngle) const
{
    const FQuat Turn(FRotator(0, CandidateAngle, 0));
    const FTransform Parent = Hinge->GetAttachParent()->GetComponentTransform();
    const FVector Center = Parent.TransformPosition(Hinge->GetRelativeLocation() + Turn.RotateVector(DoorCollision->GetRelativeLocation()));
    const FQuat Rotation = Parent.GetRotation() * Turn;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ChronosFridgeDoor), false, this);
    if (const auto* Actor = BlackCan->GetChildActor()) Params.AddIgnoredActor(Actor);
    if (const auto* Actor = WhiteCan->GetChildActor()) Params.AddIgnoredActor(Actor);
    TArray<FOverlapResult> Results;
    const bool bBlocked = GetWorld()->OverlapMultiByProfile(Results, Center, Rotation,
        UCollisionProfile::BlockAllDynamic_ProfileName, FCollisionShape::MakeBox(DoorCollision->GetScaledBoxExtent()), Params);
    if (bBlocked)
        for (const auto& Result : Results)
            if (Result.bBlockingHit)
                UE_LOG(LogChronosFridge, Verbose, TEXT("Door blocked by %s.%s at %.1f"),
                    *GetNameSafe(Result.GetActor()), *GetNameSafe(Result.GetComponent()), CandidateAngle);
    return bBlocked;
}

void AChronosFridge::MoveDoor(float Progress)
{
    if (bUpdatingDoor) return;
    TGuardValue<bool> Updating(bUpdatingDoor, true);
    const float TargetAngle = OpenAngle * FMath::SmoothStep(0.f, 1.f, Progress);
    const int32 Substeps = FMath::Max(1, FMath::CeilToInt(FMath::Abs(TargetAngle - DoorAngle) / 3.f));
    const float PreviousAngle = DoorAngle;
    for (int32 Step = 1; Step <= Substeps; ++Step)
    {
        const float Candidate = FMath::Lerp(PreviousAngle, TargetAngle, Step / static_cast<float>(Substeps));
        if (IsDoorBlocked(Candidate))
        {
            DoorTimeline->Stop();
            DoorTimeline->SetPlaybackPosition(DoorPosition, false, false);
            return;
        }
    }
    DoorPosition = DoorTimeline->GetPlaybackPosition();
    DoorAngle = TargetAngle;
    Hinge->SetRelativeRotation(FRotator(0, DoorAngle, 0));
    UpdateOpenSound();
}

AChronosBeer::AChronosBeer()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Lata"));
    LabelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Etiqueta"));
    LabelMesh->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    LabelMesh->SetStaticMesh(Cylinder.Object);
    LabelMesh->SetRelativeScale3D(FVector(.066, .066, .122));
    LabelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Black(TEXT("/Game/TENCENT3D/cerv.cerv"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> White(TEXT("/Game/TENCENT3D/cervv.cervv"));
    BlackLabel = Black.Object;
    WhiteLabel = White.Object;
    SetAppearance();
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Aluminum(TEXT("/Game/TENCENT3D/M_Cerveza_Aluminio.M_Cerveza_Aluminio"));
    for (const float Height : {-6.0f, 6.0f})
    {
        auto* Rim = CreateDefaultSubobject<UStaticMeshComponent>(Height < 0 ? TEXT("BaseAluminio") : TEXT("TapaAluminio"));
        Rim->SetupAttachment(RootComponent);
        Rim->SetStaticMesh(Cylinder.Object);
        Rim->SetMaterial(0, Aluminum.Object);
        Rim->SetRelativeLocation(FVector(0, 0, Height));
        Rim->SetRelativeScale3D(FVector(.067, .067, .006));
        Rim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    auto* Target = AddCollision(*this, TEXT("ColisionLata"), *RootComponent, FVector::ZeroVector,
        FVector(3.4, 3.4, 6.3), UCollisionProfile::BlockAllDynamic_ProfileName);
    Target->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Target->SetCollisionResponseToAllChannels(ECR_Ignore);
    Target->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    DrinkTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("TimelineBeber"));
    DrinkCurve = CreateDefaultSubobject<UCurveFloat>(TEXT("ProgresoBeber"));
    DrinkCurve->FloatCurve.AddKey(0.f, 0.f);
    DrinkCurve->FloatCurve.AddKey(DrinkDuration, 1.f);
    DrinkAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioAbrirYBeber"));
    DrinkAudio->SetupAttachment(RootComponent);
    ConfigureAudio(*DrinkAudio, .8f);
    static ConstructorHelpers::FObjectFinder<USoundBase> DrinkAsset(TEXT("/Game/TENCENT3D/beber_cerveza.beber_cerveza"));
    DrinkSound = DrinkAsset.Object;
}

void AChronosBeer::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    SetAppearance();
}

void AChronosBeer::Stock(AChronosFridge* InFridge, EChronosBeer InVariant, int32 Index)
{
    Fridge = InFridge;
    StockIndex = Index;
    Variant = InVariant;
    SetAppearance();
}

void AChronosBeer::SetAppearance()
{
    LabelMesh->SetMaterial(0, Variant == EChronosBeer::BlackCrow ? BlackLabel.Get() : WhiteLabel.Get());
}

void AChronosBeer::BeginPlay()
{
    Super::BeginPlay();
    DrinkAudio->SetSound(DrinkSound);
    ConsumptionDuration = DrinkSound && FMath::IsFinite(DrinkSound->GetDuration())
        ? FMath::Max(DrinkDuration, DrinkSound->GetDuration()) : DrinkDuration;
    DrinkCurve->FloatCurve.Reset();
    DrinkCurve->FloatCurve.AddKey(0.f, 0.f);
    DrinkCurve->FloatCurve.AddKey(ConsumptionDuration, 1.f);
    FOnTimelineFloat Update;
    Update.BindDynamic(this, &AChronosBeer::AnimateDrink);
    AddClock(*DrinkTimeline, *DrinkCurve, ConsumptionDuration, Update);
    FOnTimelineEvent Done;
    Done.BindDynamic(this, &AChronosBeer::FinishDrink);
    DrinkTimeline->SetTimelineFinishedFunc(Done);
}

void AChronosBeer::EndPlay(const EEndPlayReason::Type Reason)
{
    DrinkAudio->Stop();
    if (Controller.IsValid()) DisableInput(Controller.Get());
    if (auto* Session = GetWorld()->GetSubsystem<UChronosFridgeSession>(); Session && Holder.IsValid())
        Session->Hold(nullptr);
    Super::EndPlay(Reason);
}

bool AChronosBeer::CanUseFridgeTarget(AActor* Player) const
{
    const auto* Session = GetWorld()->GetSubsystem<UChronosFridgeSession>();
    return Session && !Session->HasCan() && !Holder.IsValid()
        && (!Fridge.IsValid() || Fridge->HasShelfAccess()) && Super::CanUseFridgeTarget(Player);
}

FText AChronosBeer::InteractionPrompt() const
{
    return LOCTEXT("Pick", "Recoger cerveza");
}

void AChronosBeer::UseFridgeTarget(AActor* Player)
{
    if (!CanUseFridgeTarget(Player)) return;
    Equip(Cast<ACharacter>(Player));
}

FName AChronosBeer::ItemId() const
{
    return Variant == EChronosBeer::BlackCrow ? TEXT("BlackCrow") : TEXT("WhiteCrow");
}

bool AChronosBeer::Equip(ACharacter* Character)
{
    auto* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
    auto* Skin = Character ? Character->GetMesh() : nullptr;
    auto* Session = GetWorld()->GetSubsystem<UChronosFridgeSession>();
    if (!PC || !Skin || !Skin->DoesSocketExist(HandSocket) || !Session || Session->HasCan()) return false;
    if (Fridge.IsValid() && !Fridge->TakeSlot(StockIndex)) return false;

    Holder = Character;
    Controller = PC;
    SetActorEnableCollision(false);
    AttachToComponent(Skin, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandSocket);
    Grip = FTransform(FRotator(0, 0, 90), FVector(5, 0, 0));
    SetActorRelativeTransform(Grip);
    Session->Hold(this);
    if (auto* Progress = UChronosProgressState::Get(this)) Progress->Data.EquippedItem = ItemId();
    return true;
}

void AChronosBeer::Drink()
{
    if (Holder.IsValid() && !IsDrinking() && !bConsumed && UChronosInteraction::CanPresentThought(Holder.Get()))
    {
        DrinkTimeline->PlayFromStart();
        DrinkAudio->Play();
    }
}

bool AChronosBeer::IsDrinking() const { return DrinkTimeline->IsPlaying(); }

void AChronosBeer::CancelDrink()
{
    if (!IsDrinking() || bConsumed) return;
    DrinkTimeline->Stop();
    DrinkAudio->FadeOut(.08f, 0.f);
    DrinkTimeline->SetPlaybackPosition(0, false);
    SetActorRelativeTransform(Grip);
}

float AChronosBeer::GetDrinkLift() const
{
    const float Progress = DrinkTimeline->GetPlaybackPosition() / ConsumptionDuration;
    return IsDrinking() ? FMath::SmoothStep(0.f, .35f, Progress) * (1.f - FMath::SmoothStep(.84f, 1.f, Progress)) : 0.f;
}

bool AChronosBeer::Pocket()
{
    auto* Progress = UChronosProgressState::Get(this);
    if (!Progress || !Holder.IsValid() || IsDrinking() || bConsumed || !Progress->Add(ItemId(), 1)) return false;
    Progress->Data.EquippedItem = NAME_None;
    GetWorld()->GetSubsystem<UChronosFridgeSession>()->Hold(nullptr);
    Destroy();
    return true;
}

void AChronosBeer::AnimateDrink(float Progress)
{
    auto* Character = Holder.Get();
    if (!Character || !Character->GetMesh()) return;
    const auto* Skin = Character->GetMesh();
    if (Skin->GetAnimInstance() && Skin->GetAnimInstance()->IsA<UChronosBeerAnimation>())
    {
        auto Pose = Grip;
        Pose.SetRotation(Grip.GetRotation() * FRotator(-55.f * GetDrinkLift(), 0, 0).Quaternion());
        SetActorRelativeTransform(Pose);
        return;
    }
    const FTransform Hand = Grip * Skin->GetSocketTransform(HandSocket);
    const FVector Mouth = Skin->GetSocketLocation(HeadSocket)
        + 12.f * Character->GetActorForwardVector() + FVector(0, 0, -2);
    const float Lift = FMath::SmoothStep(0.f, .35f, Progress) * (1.f - FMath::SmoothStep(.84f, 1.f, Progress));
    const FRotator Tilt(-70.f * Lift, Character->GetActorRotation().Yaw + 90.f, 0.f);
    SetActorLocationAndRotation(FMath::Lerp(Hand.GetLocation(), Mouth, Lift),
        FQuat::Slerp(Hand.GetRotation(), Tilt.Quaternion(), Lift));
}

void AChronosBeer::FinishDrink()
{
    if (bConsumed || !Holder.IsValid()) return;
    bConsumed = true;
    if (Variant == EChronosBeer::BlackCrow && Controller.IsValid() && Controller->PlayerCameraManager)
    {
        auto* Camera = Controller->PlayerCameraManager.Get();
        auto* Modifier = Cast<UChronosBeerCamera>(Camera->FindCameraModifierByClass(UChronosBeerCamera::StaticClass()));
        if (!Modifier) Modifier = Cast<UChronosBeerCamera>(Camera->AddNewCameraModifier(UChronosBeerCamera::StaticClass()));
        if (Modifier) Modifier->StartEffect(DuracionMareo, IntensidadMareo);
    }
    if (Variant == EChronosBeer::White) UChronosStamina::RestoreStamina(Holder.Get());
    if (auto* Progress = UChronosProgressState::Get(this)) Progress->Data.EquippedItem = NAME_None;
    GetWorld()->GetSubsystem<UChronosFridgeSession>()->Hold(nullptr);
    Destroy();
}

float UChronosBeerCamera::SpeedMultiplier() const
{
    return FMath::Clamp(1.f - Intensity * .6f, MinimumSpeedMultiplier, 1.f);
}

void UChronosBeerCamera::StartEffect(float Duration, float Strength)
{
    const float Dose = FMath::Max(0.f, Strength);
    if (Dose <= 0.f) return;
    const float DoseRecovery = FMath::Max(.0001f, Dose / FMath::Max(1.f, Duration));
    if (!IsActive()) { Elapsed = 0.f; Intensity = 0.f; RecoveryRate = DoseRecovery; }
    else RecoveryRate = FMath::Min(RecoveryRate, DoseRecovery);
    Intensity = FMath::Clamp(Intensity + Dose, 0.f, MaximumIntensity);
    UE_LOG(LogChronosFridge, Display, TEXT("Black Crow acumulada: intensidad=%.3f velocidad=%.3f recuperacion=%.1fs"),
        Intensity, SpeedMultiplier(), GetRecoveryTime());
}

bool UChronosBeerCamera::ModifyCamera(float DeltaTime, FMinimalViewInfo& View)
{
    if (!IsActive()) return false;
    Elapsed += DeltaTime;
    Intensity = FMath::Max(0.f, Intensity - RecoveryRate * DeltaTime);
    const float Weight = Intensity * FMath::SmoothStep(0.f, 1.1f, Elapsed);
    const float Tremor = CameraTremor * Weight * Weight;
    View.Rotation.Roll += FMath::Sin(Elapsed * 2.1f) * 3.f * Weight + FMath::Sin(Elapsed * 27.f) * Tremor;
    View.Rotation.Pitch += FMath::Sin(Elapsed * 1.5f) * 1.4f * Weight + FMath::Sin(Elapsed * 31.f) * Tremor * .55f;
    View.Rotation.Yaw += FMath::Sin(Elapsed * 1.7f) * .55f * Weight;
    FPostProcessSettings Effect;
    Effect.bOverride_ColorSaturation = true;
    Effect.ColorSaturation = FVector4(.76f, .83f, .82f, 1.f);
    Effect.bOverride_VignetteIntensity = true;
    Effect.VignetteIntensity = .58f;
    Effect.bOverride_MotionBlurAmount = true;
    Effect.MotionBlurAmount = .55f;
    Effect.bOverride_SceneFringeIntensity = true;
    Effect.SceneFringeIntensity = 1.45f;
    CameraOwner->AddCachedPPBlend(Effect, FMath::Clamp(Weight, 0.f, 1.f), VTBlendOrder_Override);
    return false;
}

bool UChronosFridgeSession::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type == EWorldType::Game || Type == EWorldType::PIE;
}

TStatId UChronosFridgeSession::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UChronosFridgeSession, STATGROUP_Tickables);
}

void UChronosFridgeSession::Hold(AChronosBeer* Beer)
{
    Held = Beer;
    UpdateItemInput(GetWorld()->GetFirstPlayerController());
}

void UChronosFridgeSession::ConsumeHeld() { if (Held.IsValid()) Held->Drink(); }
void UChronosFridgeSession::PocketHeld() { if (Held.IsValid() && UChronosInteraction::CanPresentThought(GetWorld()->GetFirstPlayerController()->GetPawn())) Held->Pocket(); }

void UChronosFridgeSession::PickupLookedBeer()
{
    auto* Player = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn().Get() : nullptr;
    if (!Held.IsValid())
        if (auto* Beer = Cast<AChronosBeer>(UChronosInteraction::ResolveTarget(Player)))
        {
            LastPickupFrame = GFrameCounter;
            Beer->UseFridgeTarget(Player);
        }
}

void UChronosFridgeSession::UpdateItemInput(APlayerController* PC)
{
    if (!PC || !PC->GetLocalPlayer()) return;
    auto* Enhanced = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!Enhanced) return;
    if (!ItemContext)
    {
        ItemContext = NewObject<UInputMappingContext>(this, TEXT("EquippedConsumable"));
        DrinkAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Characters/input/interaction/IA_DrinkItem.IA_DrinkItem"));
        PocketAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Characters/input/interaction/IA_PocketItem.IA_PocketItem"));
        PickupAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Characters/input/interaction/IA_PickupBeer.IA_PickupBeer"));
    }
    if (!DrinkAction || !PocketAction || !PickupAction) return;
    if (!ItemInput || InputPlayer.Get() != PC)
    {
        if (InputPlayer.IsValid() && ItemInput) InputPlayer->PopInputComponent(ItemInput);
        ItemInput = NewObject<UEnhancedInputComponent>(PC);
        ItemInput->Priority = 10;
        ItemInput->RegisterComponent();
        ItemInput->BindAction(DrinkAction, ETriggerEvent::Started, this, &UChronosFridgeSession::ConsumeHeld);
        ItemInput->BindAction(PocketAction, ETriggerEvent::Started, this, &UChronosFridgeSession::PocketHeld);
        ItemInput->BindAction(PickupAction, ETriggerEvent::Started, this, &UChronosFridgeSession::PickupLookedBeer);
        InputPlayer = PC;
        PC->PushInputComponent(ItemInput);
    }
    const bool bEnable = Held.IsValid();
    const auto* Pad = UChronosGamepad::Get(PC);
    const TArray<FKey> Keys = {
        ChronosControls::Key(PC, TEXT("Chronos.DrinkItem"), EKeys::X),
        ChronosControls::Key(PC, TEXT("Chronos.PocketItem"), EKeys::R),
        Pad ? Pad->Key(TEXT("Chronos.DrinkItem"), Pad->IsAlternate()) : EKeys::Gamepad_RightTrigger,
        Pad ? Pad->Key(TEXT("Chronos.PocketItem"), Pad->IsAlternate()) : EKeys::Gamepad_RightShoulder
    };
    if (bEnable == bItemInputActive && (!bEnable || Keys == AppliedItemKeys)) return;
    bItemInputActive = bEnable;
    if (bEnable)
    {
        ItemContext->UnmapAll();
        AppliedItemKeys = Keys;
        ItemContext->MapKey(DrinkAction, Keys[0]);
        ItemContext->MapKey(PocketAction, Keys[1]);
        ItemContext->MapKey(DrinkAction, Keys[2]);
        ItemContext->MapKey(PocketAction, Keys[3]);
        FModifyContextOptions Options;
        Options.bIgnoreAllPressedKeysUntilRelease = true;
        Enhanced->AddMappingContext(ItemContext, 200, Options);
    }
    else
    {
        Enhanced->RemoveMappingContext(ItemContext);
    }
}

void UChronosFridgeSession::Deinitialize()
{
    Held.Reset();
    UpdateItemInput(InputPlayer.Get());
    if (InputPlayer.IsValid() && ItemInput) InputPlayer->PopInputComponent(ItemInput);
    if (ItemInput) ItemInput->DestroyComponent();
    ItemInput = nullptr;
    Super::Deinitialize();
}

bool UChronosFridgeSession::EquipItem(FName Item)
{
    if (Held.IsValid() || (Item != TEXT("BlackCrow") && Item != TEXT("WhiteCrow"))) return false;
    auto* Progress = UChronosProgressState::Get(this);
    auto* Character = Cast<ACharacter>(GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn().Get() : nullptr);
    if (!Progress || Progress->Count(Item) <= 0 || !Character) return false;
    auto* Class = LoadClass<AChronosBeer>(nullptr, TEXT("/Game/Interactivo/bp_fridge/Blueprints/BP_Cerveza.BP_Cerveza_C"));
    auto* Can = Class ? GetWorld()->SpawnActor<AChronosBeer>(Class, Character->GetActorLocation(), FRotator::ZeroRotator) : nullptr;
    if (!Can) return false;
    Can->Stock(nullptr, Item == TEXT("BlackCrow") ? EChronosBeer::BlackCrow : EChronosBeer::White);
    if (!Can->Equip(Character)) { Can->Destroy(); return false; }
    return Progress->Remove(Item, 1);
}

void UChronosFridgeSession::Tick(float DeltaTime)
{
    auto* PC = GetWorld()->GetFirstPlayerController();
    APawn* Player = PC ? PC->GetPawn().Get() : nullptr;
    UpdateItemInput(PC);
    if (!bRestoredEquipment && IsValid(Player) && GetWorld()->GetTimeSeconds() > 3.f)
    {
        const auto* Intro = FindFProperty<FBoolProperty>(Player->GetClass(), TEXT("enDespertar"));
        if (!Intro || !Intro->GetPropertyValue_InContainer(Player))
        {
            bRestoredEquipment = true;
            if (auto* Progress = UChronosProgressState::Get(this); Progress && !Progress->Data.EquippedItem.IsNone() && !Held.IsValid())
            {
                const FName Item = Progress->Data.EquippedItem;
                if (Progress->Add(Item, 1) && !EquipItem(Item)) Progress->Remove(Item, 1);
            }
        }
    }
    if (Held.IsValid() && UChronosConversation::IsActive(this)) Held->CancelDrink();
}

#undef LOCTEXT_NAMESPACE


