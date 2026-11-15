#include "ChronosHUDLayer.h"
#include "ChronosJournal.h"
#include "ChronosStamina.h"
#include "ChronosTutorial.h"
#include "ChronosGamepad.h"
#include "ChronosControls.h"
#include "GameFramework/Character.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "UObject/UnrealType.h"
#include "Widgets/SWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogChronosHUD, Log, All);

namespace
{
    constexpr float DiscoveryRadius = 4400.f;
    constexpr float QuickSamplePeriod = .12f;
    constexpr float TaskSamplePeriod = .85f;
    // Shared approaches in front of both doors, beyond the divider between the paths.
    const FVector WestRestroom(27220, -18100, 22160);
    const FVector EastRestroom(53854, -25778, 22160);

    UObject* ReadObject(const UObject* Source, FName Name)
    {
        if (!IsValid(Source)) return nullptr;
        const auto* Property = FindFProperty<FObjectPropertyBase>(Source->GetClass(), Name);
        return Property ? Property->GetObjectPropertyValue_InContainer(Source) : nullptr;
    }

    double ReadNumber(const UObject* Source, FName Name, double Default = 0)
    {
        if (!IsValid(Source)) return Default;
        const auto* Property = FindFProperty<FNumericProperty>(Source->GetClass(), Name);
        if (!Property) return Default;
        const void* Value = Property->ContainerPtrToValuePtr<void>(Source);
        return Property->IsFloatingPoint() ? Property->GetFloatingPointPropertyValue(Value)
            : static_cast<double>(Property->GetSignedIntPropertyValue(Value));
    }

    bool ReadBool(const UObject* Source, FName Name)
    {
        if (!IsValid(Source)) return false;
        const auto* Property = FindFProperty<FBoolProperty>(Source->GetClass(), Name);
        return Property && Property->GetPropertyValue_InContainer(Source);
    }

    FString ReadString(const UObject* Source, FName Name)
    {
        if (!IsValid(Source)) return {};
        if (const auto* Property = FindFProperty<FStrProperty>(Source->GetClass(), Name))
            return Property->GetPropertyValue_InContainer(Source);
        if (const auto* Property = FindFProperty<FTextProperty>(Source->GetClass(), Name))
            return Property->GetPropertyValue_InContainer(Source).ToString();
        return {};
    }

    void BindEvent(UObject* Source, FName Event, UObject* Target, FName Handler, bool bBind)
    {
        if (!IsValid(Source)) return;
        const auto* Property = FindFProperty<FMulticastDelegateProperty>(Source->GetClass(), Event);
        UFunction* Function = Target->FindFunction(Handler);
        if (!Property || !Function || !Property->SignatureFunction || !Property->SignatureFunction->IsSignatureCompatibleWith(Function))
        {
            return;
        }
        FScriptDelegate Delegate;
        Delegate.BindUFunction(Target, Handler);
        void* Value = Property->ContainerPtrToValuePtr<void>(Source);
        if (bBind) Property->AddDelegate(Delegate, Source, Value);
        else Property->RemoveDelegate(Delegate, Source, Value);
    }

    const FChronosTask* PrimaryTask(const TArray<FChronosTask>& Tasks)
    {
        return Tasks.FindByPredicate([](const FChronosTask& Task) { return !Task.bCompleted && Task.Position.IsSet(); });
    }

    void FadeToward(float& Value, float Target, float Delta)
    {
        Value = FMath::FInterpConstantTo(Value, Target, Delta, 3.5f);
    }
}

bool UChronosHUDSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UChronosHUDSubsystem::IsTickable() const
{
    return Super::IsTickable() && GetWorld() && GetWorld()->GetMapName().EndsWith(TEXT("CHRONOSALPHA"), ESearchCase::IgnoreCase)
        && GetWorld()->GetNetMode() != NM_DedicatedServer;
}

TStatId UChronosHUDSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UChronosHUDSubsystem, STATGROUP_Tickables);
}

void UChronosHUDSubsystem::SetHUDMode(EChronosHUDMode NewMode)
{
    DefaultMode = NewMode;
    if (HUD) HUD->Mode = NewMode;
}

void UChronosHUDSubsystem::SetNarrativeMarkersHidden(bool bHidden)
{
    bHideMarkers = bHidden;
    if (HUD) HUD->bNarrativeHidden = bHidden;
    if (HUD && bHidden)
    {
        HUD->bMapOpen = false;
        SetJournalOpen(HUD->GetOwningPlayer(), false);
    }
}

bool UChronosHUDSubsystem::KeyAvailable(APlayerController* PC, FKey Key) const
{
    if (!Key.IsValid()) return false;
    if (const auto* Input = Cast<UEnhancedPlayerInput>(PC->PlayerInput))
    {
        for (const FEnhancedActionKeyMapping& Mapping : Input->GetEnhancedActionMappingsView())
            if (Mapping.Key == Key) return false;
    }
    for (const FInputActionKeyMapping& Mapping : GetDefault<UInputSettings>()->GetActionMappings())
        if (Mapping.Key == Key) return false;
    for (const FInputAxisKeyMapping& Mapping : GetDefault<UInputSettings>()->GetAxisMappings())
        if (Mapping.Key == Key) return false;
    return true;
}

bool UChronosHUDSubsystem::Attach(APlayerController* PC, APawn* Pawn)
{
    if (!PC->IsLocalController() || !PC->GetLocalPlayer() || !ReadObject(Pawn, TEXT("Health"))) return false;
    HUD = CreateWidget<UChronosHUDLayer>(PC, UChronosHUDLayer::StaticClass());
    if (!HUD) return false;

    HUD->SetVisibility(ESlateVisibility::HitTestInvisible);
    HUD->InitializeArt();
    if (auto* Settings = UChronosMenuSettings::Get(PC)) Settings->AttachHUD(HUD);
    HUD->Mode = DefaultMode;
    HUD->Places = {
        {TEXT("Casa"), TEXT("Home"), FVector(88601, -26068, 22376)},
        {TEXT("Morgue y crematorio"), TEXT("Crematorium"), FVector(66365, -27490, 22170)},
        {TEXT("Iglesia"), TEXT("Church"), FVector(49330, -18350, 22205)},
        {TEXT("Baños"), TEXT("Restroom"), EastRestroom},
        {TEXT("Baños"), TEXT("Restroom"), WestRestroom},
        {TEXT("Nichos"), TEXT("Graves"), FVector(36300, -26300, 22200)},
        {TEXT("Zona de juegos"), TEXT("Playground"), FVector(42850, -16050, 22165)},
        {TEXT("Florería"), TEXT("Flowers"), FVector(27550, -22920, 22250)},
        {TEXT("Altar de la Virgen"), TEXT("Altar"), FVector(48910, -25600, 22160)},
        {TEXT("Entrada principal"), TEXT("Entrance"), FVector(74340, -21260, 22170)},
        {TEXT("Estacionamiento"), TEXT("Parking"), FVector(64000, -27000, 22165)}
    };
    HUD->AddToViewport(80);
    if (!HUD->IsInViewport())
    {
        HUD = nullptr;
        return false;
    }
    Player = Pawn;
    AtlasAt = GetWorld()->GetTimeSeconds() + 1.5f;
    if (UTexture2D* Baked = LoadObject<UTexture2D>(nullptr, TEXT("/Game/TheChronosCurse_HUD_Assets/Compass/T_Chronos_WorldAtlas.T_Chronos_WorldAtlas"), nullptr, LOAD_NoWarn))
    {
        HUD->SetAtlas(Baked);
        HUD->bAtlasReady = true;
    }
    UE_LOG(LogChronosHUD, Display, TEXT("HUD connected to %s."), *Pawn->GetClass()->GetName());
    return true;
}

void UChronosHUDSubsystem::HideOld(UObject* Candidate)
{
    UUserWidget* Widget = Cast<UUserWidget>(Candidate);
    if (!IsValid(Widget)) return;
    OldWidgets.AddUnique(Widget);
    if (Widget->GetVisibility() != ESlateVisibility::Collapsed) Widget->SetVisibility(ESlateVisibility::Collapsed);
}

void UChronosHUDSubsystem::SampleHealth(float Now)
{
    if (!HUD) return;
    const double Max = ReadNumber(HealthModel.Get(), TEXT("MaxHealth"));
    if (Max <= 0) return;
    const float Value = FMath::Clamp(ReadNumber(HealthModel.Get(), TEXT("CurrentHealth")) / Max, 0.0, 1.0);
    if (HUD->Health < 0) HUD->ShownHealth = Value;
    else if (Value < HUD->Health - KINDA_SMALL_NUMBER) HealthUntil = Now + 4.f;
    HUD->Health = Value;
}

void UChronosHUDSubsystem::OnHealthChanged(double CurrentHealth, double MaxHealth)
{
    SampleHealth(GetWorld()->GetTimeSeconds());
}

void UChronosHUDSubsystem::OnTasksChanged()
{
    SampleTasks();
}

void UChronosHUDSubsystem::SampleTasks()
{
    if (!HUD) return;
    UObject* Manager = ReadObject(GetWorld()->GetGameState(), TEXT("gestorTareas"));
    if (TaskManager.Get() != Manager)
    {
        BindEvent(TaskManager.Get(), TEXT("OnTareasCambiadas"), this, GET_FUNCTION_NAME_CHECKED(UChronosHUDSubsystem, OnTasksChanged), false);
        TaskManager = Manager;
        BindEvent(Manager, TEXT("OnTareasCambiadas"), this, GET_FUNCTION_NAME_CHECKED(UChronosHUDSubsystem, OnTasksChanged), true);
    }
    HUD->Tasks.Reset();
    if (!IsValid(Manager)) return;
    const auto* ArrayProperty = FindFProperty<FArrayProperty>(Manager->GetClass(), TEXT("tareas"));
    const auto* Inner = ArrayProperty ? CastField<FObjectPropertyBase>(ArrayProperty->Inner) : nullptr;
    if (!Inner) return;
    FScriptArrayHelper Values(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Manager));
    for (int32 Index = 0; Index < Values.Num(); ++Index)
    {
        UObject* Entry = Inner->GetObjectPropertyValue(Values.GetRawPtr(Index));
        const FString Description = ReadString(Entry, TEXT("descripcion"));
        if (Description.IsEmpty()) continue;
        FChronosTask& Task = HUD->Tasks.AddDefaulted_GetRef();
        Task.Description = Description;
        Task.bCompleted = ReadBool(Entry, TEXT("completada"));
        AActor* Destination = Cast<AActor>(ReadObject(Entry, TEXT("punto")));
        if (!IsValid(Destination)) Destination = Cast<AActor>(ReadObject(Entry, TEXT("ataud")));
        if (!IsValid(Destination)) Destination = Cast<AActor>(ReadObject(Entry, TEXT("bolsa")));
        if (IsValid(Destination))
        {
            Task.Position = Destination->GetActorLocation();
            float Distance = FMath::Square(2800.f);
            for (int32 Place = 0; Place < HUD->Places.Num(); ++Place)
            {
                const float Candidate = FVector::DistSquared2D(HUD->Places[Place].Position, Task.Position.GetValue());
                if (Candidate < Distance)
                {
                    Task.PlaceIndex = Place;
                    Distance = Candidate;
                }
            }
            if (HUD->Places.IsValidIndex(Task.PlaceIndex) && HUD->Places[Task.PlaceIndex].Icon == TEXT("Restroom"))
            {
                const FVector RestroomFacade = Task.PlaceIndex == 3
                    ? FVector(53854, -26850, 22160) : FVector(27220, -17028, 22160);
                if (FVector::DistSquared2D(Task.Position.GetValue(), RestroomFacade) <= FMath::Square(850.f))
                    Task.Position = HUD->Places[Task.PlaceIndex].Position;
                else
                    Task.PlaceIndex = INDEX_NONE;
            }
        }
        const int32 Total = ReadNumber(Entry, TEXT("cantidadBasuras"));
        if (Total > 0)
        {
            const int32 Remaining = FMath::Clamp(static_cast<int32>(ReadNumber(Entry, TEXT("restantes"))), 0, Total);
            Task.ProgressText = FString::Printf(TEXT("%d/%d"), Task.bCompleted ? Total : Total - Remaining, Total);
        }
    }
}

void UChronosHUDSubsystem::Sample(APawn* Pawn, float Now)
{
    UObject* Health = ReadObject(Pawn, TEXT("Health"));
    if (HealthModel.Get() != Health)
    {
        BindEvent(HealthModel.Get(), TEXT("OnHealthChanged"), this, GET_FUNCTION_NAME_CHECKED(UChronosHUDSubsystem, OnHealthChanged), false);
        HealthModel = Health;
        BindEvent(Health, TEXT("OnHealthChanged"), this, GET_FUNCTION_NAME_CHECKED(UChronosHUDSubsystem, OnHealthChanged), true);
        HUD->Health = -1.f;
    }
    SampleHealth(Now);
    if (HUD->Health >= 0) HideOld(ReadObject(Pawn, TEXT("PlayerHealthBar")));

    UObject* Lamp = ReadObject(Pawn, TEXT("flashlight"));
    const double Duration = ReadNumber(Lamp, TEXT("duracionBateria"));
    HUD->Battery = Duration > 0 ? FMath::Clamp(ReadNumber(Lamp, TEXT("bateriaRestante")) / Duration, 0.0, 1.0) : -1.f;
    const bool bWasActive = bBatteryActive;
    bBatteryActive = IsValid(Lamp) && ReadBool(Lamp, TEXT("enMano")) && HUD->Battery > 0;
    if (bBatteryActive || bWasActive || (IsValid(Lamp) && HUD->Battery == 0 && ReadBool(Lamp, TEXT("enMano")) && BatteryUntil == 0))
        BatteryUntil = Now + 2.5f;
    if (!IsValid(Lamp)) BatteryUntil = 0.f;
    if (HUD->Battery >= 0) HideOld(ReadObject(Lamp, TEXT("widgetBateria")));

    if (!Director.IsValid() && CacheIn <= 0)
    {
        UClass* Type = LoadClass<AActor>(nullptr, TEXT("/Game/NPC/Visitors/LivingCemetery/BP_CemeteryDirector.BP_CemeteryDirector_C"));
        if (Type)
            for (TActorIterator<AActor> Actor(GetWorld(), Type); Actor; ++Actor)
            {
                Director = *Actor;
                break;
            }
    }
    if (AActor* Clock = Director.Get())
    {
        const FString Weekday = ReadString(Clock, TEXT("WeekdayName"));
        if (!Weekday.IsEmpty())
        {
            HUD->Clock = FString::Printf(TEXT("%s · %02d:%02d"), *Weekday,
                static_cast<int32>(ReadNumber(Clock, TEXT("HourOfDay"))), static_cast<int32>(ReadNumber(Clock, TEXT("MinuteOfHour"))));
            HUD->Day = FString::Printf(TEXT("Día %d"), static_cast<int32>(ReadNumber(Clock, TEXT("CalendarDay"))));
            HideOld(ReadObject(Clock, TEXT("ClockWidget")));
        }
    }

    if (CacheIn <= 0)
    {
        SampleTasks();
        CacheIn = TaskSamplePeriod;
    }
    for (FChronosPlace& Place : HUD->Places)
        if (!Place.bDiscovered && FVector::DistSquared2D(Place.Position, Pawn->GetActorLocation()) <= FMath::Square(DiscoveryRadius))
            Place.bDiscovered = true;
}

void UChronosHUDSubsystem::CaptureAtlas()
{
    UWorld* World = GetWorld();
    AtlasAt = TNumericLimits<float>::Max();
    FActorSpawnParameters Params;
    Params.ObjectFlags = RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ASceneCapture2D* Actor = World->SpawnActor<ASceneCapture2D>(ChronosAtlas::Center, FRotator(-90, 0, 0), Params);
    if (!Actor) return;
    Atlas = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
    Atlas->RenderTargetFormat = RTF_RGBA8;
    Atlas->ClearColor = FLinearColor(.028f, .033f, .035f, 1);
    Atlas->bAutoGenerateMips = false;
    Atlas->InitAutoFormat(2048, 2048);
    USceneCaptureComponent2D* Capture = Actor->GetCaptureComponent2D();
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->ProjectionType = ECameraProjectionMode::Orthographic;
    Capture->OrthoWidth = ChronosAtlas::Width;
    Capture->CaptureSource = SCS_FinalColorLDR;
    Capture->TextureTarget = Atlas;
    Capture->ShowFlags.SetAtmosphere(false);
    Capture->ShowFlags.SetFog(false);
    Capture->ShowFlags.SetVolumetricFog(false);
    Capture->ShowFlags.SetMotionBlur(false);
    Capture->ShowFlags.SetDynamicShadows(false);
    Capture->bUseRayTracingIfEnabled = false;
    Capture->CaptureScene();
    HUD->SetAtlas(Atlas);
    HUD->bAtlasReady = true;
    Actor->Destroy();
    UE_LOG(LogChronosHUD, Display, TEXT("Captured one orthographic atlas of %s."), *World->GetMapName());
}

void UChronosHUDSubsystem::SetMapInput(APlayerController* PC, bool bEnabled)
{
    if (bMapInput == bEnabled || !PC) return;
    bMapInput = bEnabled;
    if (bEnabled)
    {
        bSavedShowCursor = PC->bShowMouseCursor;
        PC->bShowMouseCursor = true;
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
        FInputModeUIOnly Input;
        if (HUD)
        {
            HUD->SetIsFocusable(true);
            Input.SetWidgetToFocus(HUD->TakeWidget());
        }
        Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(Input);
    }
    else
    {
        PC->SetIgnoreMoveInput(false);
        PC->SetIgnoreLookInput(false);
        const bool bOtherUI = PC->IsPaused() || ReadBool(PC->GetPawn(), TEXT("enPausa"))
            || ReadBool(ReadObject(PC->GetPawn(), TEXT("lista")), TEXT("abierta"));
        if (!bOtherUI)
        {
            PC->bShowMouseCursor = bSavedShowCursor;
            if (!bSavedShowCursor) PC->SetInputMode(FInputModeGameOnly());
        }
    }
}

void UChronosHUDSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UWorld* World = GetWorld();
    if (World->IsPaused())
    {
        if (!bGameplayHUDHiddenForPause)
        {
            bGameplayHUDHiddenForPause = true;
            const auto Hide = [this](TSubclassOf<UUserWidget> Class)
            {
                if (!Class) return;
                TArray<UUserWidget*> Widgets;
                UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, Class, true);
                for (auto* Widget : Widgets)
                {
                    PausedWidgetOpacity.Add(Widget, Widget->GetRenderOpacity());
                    Widget->SetRenderOpacity(0.f);
                }
            };
            Hide(UChronosTutorial::StaticClass());
            Hide(LoadClass<UUserWidget>(nullptr, TEXT("/Game/UI/WBP_Objetivo.WBP_Objetivo_C")));
        }
        if (HUD && HUD->GetVisibility() != ESlateVisibility::Collapsed)
            HUD->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    if (bGameplayHUDHiddenForPause)
    {
        for (const auto& Entry : PausedWidgetOpacity)
            if (auto* Widget = Entry.Key.Get()) Widget->SetRenderOpacity(Entry.Value);
        PausedWidgetOpacity.Reset();
        bGameplayHUDHiddenForPause = false;
    }
    APlayerController* PC = World->GetFirstPlayerController();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!IsValid(Pawn))
    {
        SetMapInput(PC, false);
        if (HUD) HUD->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    if (HUD && HUD->GetOwningPlayer() != PC)
    {
        SetMapInput(HUD->GetOwningPlayer(), false);
        SetJournalOpen(HUD->GetOwningPlayer(), false);
        if (Journal) Journal->RemoveFromParent();
        Journal = nullptr;
        HUD->RemoveFromParent();
        HUD = nullptr;
    }
    if (!HUD && !Attach(PC, Pawn)) return;
    if (Player.Get() != Pawn)
    {
        Player = Pawn;
        SampleIn = 0.f;
    }
    const float Now = World->GetTimeSeconds();
    HUD->Location = Pawn->GetActorLocation();
    HUD->Heading = PC->GetControlRotation().Yaw;
    HUD->bNarrativeHidden = bHideMarkers || ReadBool(Pawn, TEXT("enDespertar")) || IsValid(ReadObject(Pawn, TEXT("camaraMuerte")));
    UObject* List = ReadObject(Pawn, TEXT("lista"));
    bool bLegacyListOpen = ReadBool(List, TEXT("abierta"));
    const bool bExternalOverlay = ReadBool(Pawn, TEXT("enPausa")) || PC->IsPaused();
    const FKey ConfiguredMap = ChronosControls::Key(PC, TEXT("Chronos.Map"), FKey());
    const FKey ConfiguredJournal = ChronosControls::Key(PC, TEXT("Chronos.Journal"), FKey());
    const FKey ConfiguredMode = ChronosControls::Key(PC, TEXT("Chronos.HUDMode"), FKey());
    const FKey ActiveMapKey = ConfiguredMap.IsValid() ? ConfiguredMap : MapKey;
    const FKey ActiveJournalKey = ConfiguredJournal.IsValid() ? ConfiguredJournal : JournalKey;
    const FKey ActiveModeKey = ConfiguredMode.IsValid() ? ConfiguredMode : HUDModeKey;
    auto* Gamepad = UChronosGamepad::Get(PC);
    const bool bPad = Gamepad && Gamepad->IsGamepadActive();
    const auto PadKey = [Gamepad](FName Action)
    {
        return Gamepad ? Gamepad->Key(Action, Gamepad->IsAlternate()) : FKey();
    };
    const FKey PadMap = PadKey(TEXT("Chronos.Map"));
    const FKey PadJournal = PadKey(TEXT("Chronos.Journal"));
    const FKey PadMode = PadKey(TEXT("Chronos.HUDMode"));
    const auto Pressed = [PC](FKey Keyboard, FKey Controller)
    {
        return (Keyboard.IsValid() && PC->WasInputKeyJustPressed(Keyboard))
            || (Controller.IsValid() && PC->WasInputKeyJustPressed(Controller));
    };
    HUD->MapCloseKey = ActiveMapKey;
    HUD->PadMapCloseKey = PadMap;

    SampleIn -= DeltaTime;
    CacheIn -= DeltaTime;
    RouteIn -= DeltaTime;
    if (SampleIn <= 0.f)
    {
        SampleIn = QuickSamplePeriod;
        Sample(Pawn, Now);
        const auto Prompt = [&](bool bAvailable, FKey Keyboard, FKey Controller)
        {
            if (bPad && Controller.IsValid()) return Gamepad->Prompt(Controller).ToString();
            return bAvailable ? Keyboard.GetDisplayName().ToString() : FString();
        };
        HUD->MapKeyText = Prompt(ConfiguredMap.IsValid() || KeyAvailable(PC, MapKey), ActiveMapKey, PadMap);
        HUD->JournalKeyText = Prompt(ConfiguredJournal.IsValid() || KeyAvailable(PC, JournalKey), ActiveJournalKey, PadJournal);
        HUD->ModeKeyText = Prompt(ConfiguredMode.IsValid() || KeyAvailable(PC, HUDModeKey), ActiveModeKey, PadMode);
    }
    if (!HUD->bAtlasReady && Now >= AtlasAt) CaptureAtlas();
    if (Journal && Journal->ConsumeCloseRequest())
    {
        if (bLegacyListOpen && List == OpenList.Get())
        {
            if (UFunction* Toggle = List->FindFunction(TEXT("alternar"))) List->ProcessEvent(Toggle, nullptr);
            bLegacyListOpen = ReadBool(List, TEXT("abierta"));
        }
        SetJournalOpen(PC, false);
    }
    if (HUD->bNarrativeHidden || bExternalOverlay)
    {
        HUD->bMapOpen = false;
        SetJournalOpen(PC, false);
    }
    else if (bLegacyListOpen)
    {
        if (CreateJournal(PC))
        {
            HUD->bMapOpen = false;
            SetMapInput(PC, false);
            HideOld(ReadObject(List, TEXT("widgetLista")));
            OpenList = List;
            SetJournalOpen(PC, true, bPad ? PadKey(TEXT("Chronos.Tasks")) : ChronosControls::Key(PC, TEXT("Chronos.Tasks"), EKeys::G));
        }
    }
    else
    {
        if (OpenList.IsValid()) SetJournalOpen(PC, false);
        const bool bJournalVisible = Journal && Journal->IsVisible();
        if (!bJournalVisible && !HUD->MapKeyText.IsEmpty() && Pressed(ActiveMapKey, PadMap))
            HUD->bMapOpen = !HUD->bMapOpen;
        if (!bJournalVisible && !HUD->JournalKeyText.IsEmpty() && Pressed(ActiveJournalKey, PadJournal) && CreateJournal(PC))
        {
            HUD->bMapOpen = false;
            SetMapInput(PC, false);
            SetJournalOpen(PC, true, bPad ? PadJournal : ActiveJournalKey);
        }
        if (!HUD->bMapOpen && !HUD->ModeKeyText.IsEmpty() && Pressed(ActiveModeKey, PadMode))
            SetHUDMode(static_cast<EChronosHUDMode>((static_cast<uint8>(HUD->Mode) + 1) % 3));
    }
    const bool bJournalVisible = Journal && Journal->IsVisible();
    SetMapInput(PC, HUD->bMapOpen);
    HUD->SetVisibility((bExternalOverlay || bJournalVisible) ? ESlateVisibility::Collapsed :
        HUD->bMapOpen ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
    if (bJournalVisible) Journal->RefreshTasks(HUD->Tasks, HUD->Day);

    const bool bGuided = HUD->Mode == EChronosHUDMode::Guided;
    const bool bShowGauges = HUD->Mode != EChronosHUDMode::Immersive;
    FadeToward(HUD->HealthOpacity, !HUD->bNarrativeHidden && HUD->Health >= 0
        && (bShowGauges || Now < HealthUntil) ? 1.f : 0.f, DeltaTime);
    FadeToward(HUD->BatteryOpacity, !HUD->bNarrativeHidden && HUD->Battery >= 0
        && (bShowGauges || bBatteryActive || Now < BatteryUntil) ? 1.f : 0.f, DeltaTime);
    const ACharacter* John = Cast<ACharacter>(Pawn);
    HUD->Stamina = UChronosStamina::HasStamina(John) ? UChronosStamina::GetStaminaPercent(John) * .01 : -1.f;
    FadeToward(HUD->StaminaOpacity, !HUD->bNarrativeHidden && HUD->Stamina >= 0.f
        && (bShowGauges || HUD->Stamina < .999f) ? 1.f : 0.f, DeltaTime);
    HUD->ShownHealth = FMath::FInterpTo(HUD->ShownHealth, FMath::Max(0.f, HUD->Health), DeltaTime, 9.f);

    const FChronosTask* Primary = HUD->bNarrativeHidden ? nullptr : PrimaryTask(HUD->Tasks);
    TOptional<FVector> Destination;
    if (!HUD->bNarrativeHidden)
    {
        if (HUD->Waypoint.IsSet()) Destination = HUD->Waypoint;
        else if (Primary) Destination = Primary->Position;
    }
    HUD->WorldMarker.Reset();
    if (Destination.IsSet() && (bGuided || HUD->Waypoint.IsSet()) && PC->GetLocalPlayer() && PC->GetLocalPlayer()->ViewportClient)
    {
        FVector2D Projected;
        FVector2D Size;
        PC->GetLocalPlayer()->ViewportClient->GetViewportSize(Size);
        if (Size.X > 0 && Size.Y > 0 && PC->ProjectWorldLocationToScreen(Destination.GetValue() + FVector(0, 0, 140), Projected))
        {
            const FVector2D Normalized = Projected / Size;
            if (Normalized.X > .09 && Normalized.X < .91 && Normalized.Y > .14 && Normalized.Y < .84)
                HUD->WorldMarker = Normalized;
        }
    }
    if (!Destination.IsSet() || (!bGuided && !HUD->bMapOpen && !HUD->Waypoint.IsSet()))
    {
        HUD->Route.Reset();
        RoutedDestination.Reset();
    }
    else if (RouteIn <= 0 || !RoutedDestination.IsSet() || !RoutedDestination.GetValue().Equals(Destination.GetValue(), 10))
    {
        RouteIn = 2.25f;
        HUD->Route.Reset();
        RoutedDestination = Destination;
        FNavLocation NavEnd;
        if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
            Nav && Nav->ProjectPointToNavigation(Destination.GetValue(), NavEnd, FVector(250, 250, 500)))
        {
            UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, HUD->Location, NavEnd.Location, Pawn);
            if (IsValid(Path) && Path->IsValid() && !Path->IsPartial()) HUD->Route = Path->PathPoints;
        }
    }
    if (const TSharedPtr<SWidget> Slate = HUD->GetCachedWidget()) Slate->Invalidate(EInvalidateWidgetReason::Paint);
}

void UChronosHUDSubsystem::Deinitialize()
{
    if (PoemSpawnHandle.IsValid()) GetWorld()->RemoveOnActorSpawnedHandler(PoemSpawnHandle);
    if (HUD)
    {
        SetJournalOpen(HUD->GetOwningPlayer(), false);
        SetMapInput(HUD->GetOwningPlayer(), false);
    }
    if (Journal) Journal->RemoveFromParent();
    Journal = nullptr;
    BindEvent(HealthModel.Get(), TEXT("OnHealthChanged"), this, GET_FUNCTION_NAME_CHECKED(UChronosHUDSubsystem, OnHealthChanged), false);
    BindEvent(TaskManager.Get(), TEXT("OnTareasCambiadas"), this, GET_FUNCTION_NAME_CHECKED(UChronosHUDSubsystem, OnTasksChanged), false);
    if (HUD) HUD->RemoveFromParent();
    OldWidgets.Reset();
    HUD = nullptr;
    Atlas = nullptr;
    Super::Deinitialize();
}

void UChronosHUDSubsystem::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);
    if (!World.GetMapName().EndsWith(TEXT("CHRONOSALPHA"), ESearchCase::IgnoreCase)) return;
    UClass* PoemClass = LoadClass<AActor>(nullptr, TEXT("/Game/Letter/BP-Notas.BP-Notas_C"));
    if (!PoemClass) return;
    for (TActorIterator<AActor> It(&World, PoemClass); It; ++It) AssignPoemId(*It);
    PoemSpawnHandle = World.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UChronosHUDSubsystem::AssignPoemId));
}

void UChronosHUDSubsystem::AssignPoemId(AActor* Actor)
{
    if (!IsValid(Actor) || !Actor->GetClass()->GetPathName().StartsWith(TEXT("/Game/Letter/BP-Notas."))) return;
    const auto* Id = FindFProperty<FStrProperty>(Actor->GetClass(), TEXT("IdPoema"));
    const UObject* Art = ReadObject(Actor, TEXT("TexturaPoema"));
    if (!Id || !Art || Id->GetPropertyValue_InContainer(Actor) != TEXT("01")) return;
    const FName Texture = Art->GetFName();
    const FName Images[] = {TEXT("Poema"), TEXT("poema2"), TEXT("poema3"), TEXT("poema4"), TEXT("poema5"), TEXT("poema_6"), TEXT("poema7")};
    for (int32 Index = 1; Index < UE_ARRAY_COUNT(Images); ++Index)
    {
        if (Images[Index] != Texture) continue;
        Id->SetPropertyValue_InContainer(Actor, FString::Printf(TEXT("%02d"), Index + 1));
        break;
    }
}

bool UChronosHUDSubsystem::CreateJournal(APlayerController* PC)
{
    if (Journal) return true;
    if (!PC || !HUD || !HUD->GetSerif()) return false;
    UClass* BlueprintClass = LoadClass<UChronosJournal>(nullptr,
        TEXT("/Game/TheChronosCurse_HUD_Assets/Widgets/WBP_Cuaderno_Main.WBP_Cuaderno_Main_C"), nullptr, LOAD_NoWarn);
    Journal = CreateWidget<UChronosJournal>(PC, BlueprintClass ? BlueprintClass : UChronosJournal::StaticClass());
    if (!Journal || !Journal->Prepare(HUD->GetSerif()))
    {
        Journal = nullptr;
        return false;
    }
    Journal->SetVisibility(ESlateVisibility::Collapsed);
    Journal->AddToViewport(90);
    return true;
}

void UChronosHUDSubsystem::SetJournalOpen(APlayerController* PC, bool bEnabled, FKey CloseKey)
{
    if (!Journal || Journal->IsVisible() == bEnabled || !PC) return;
    if (bEnabled)
    {
        Journal->SetVisibility(ESlateVisibility::Visible);
        Journal->RefreshTasks(HUD->Tasks, HUD->Day);
        Journal->ShowJournal(CloseKey);
        bSavedShowCursor = PC->bShowMouseCursor;
        PC->bShowMouseCursor = true;
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
        FInputModeUIOnly Input;
        Input.SetWidgetToFocus(Journal->TakeWidget());
        Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(Input);
    }
    else
    {
        Journal->SetVisibility(ESlateVisibility::Collapsed);
        OpenList.Reset();
        PC->SetIgnoreMoveInput(false);
        PC->SetIgnoreLookInput(false);
        if (!PC->IsPaused() && !ReadBool(PC->GetPawn(), TEXT("enPausa")))
        {
            PC->bShowMouseCursor = bSavedShowCursor;
            if (!bSavedShowCursor) PC->SetInputMode(FInputModeGameOnly());
            else PC->SetInputMode(FInputModeGameAndUI());
        }
    }
}

bool UChronosHUDSubsystem::RecordJournalDiscovery(FName Entry, const FText& Observation, bool bAlteredPerception)
{
    return CreateJournal(GetWorld()->GetFirstPlayerController()) && Journal->RecordDiscovery(Entry, Observation, bAlteredPerception);
}
