#include "ChronosGamepad.h"

#include "ChronosControls.h"
#include "ChronosMenuSettings.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GenericPlatform/InputDeviceRegistry.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerMappableKeySettings.h"

#define LOCTEXT_NAMESPACE "ChronosGamepad"

namespace
{
    constexpr TCHAR SaveSlot[] = TEXT("Chronos_Opciones");
    constexpr TCHAR SaveClass[] = TEXT("/Game/Guardado/SG_Opciones.SG_Opciones_C");
    constexpr int32 Priority = 1;

    class FChronosInputObserver final : public IInputProcessor
    {
    public:
        explicit FChronosInputObserver(UChronosGamepad& InOwner) : Owner(&InOwner) {}
        void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
        bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
        {
            if (!Event.IsRepeat() && Owner.IsValid()) Owner->OnInput(Event.GetKey(), Event.GetInputDeviceId());
            return false;
        }
        bool HandleAnalogInputEvent(FSlateApplication&, const FAnalogInputEvent& Event) override
        {
            if (FMath::Abs(Event.GetAnalogValue()) >= .32f && Owner.IsValid())
                Owner->OnInput(Event.GetKey(), Event.GetInputDeviceId());
            return false;
        }
        bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent& Event) override
        {
            if (Event.GetCursorDelta().SizeSquared() >= 16. && Owner.IsValid())
                Owner->OnInput(EKeys::MouseX, Event.GetInputDeviceId());
            return false;
        }
        bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& Event) override
        {
            if (Owner.IsValid()) Owner->OnInput(Event.GetEffectingButton(), Event.GetInputDeviceId());
            return false;
        }
    private:
        TWeakObjectPtr<UChronosGamepad> Owner;
    };

    const UInputAction* FindAction(const UInputMappingContext& Existing, FName Mapping, FName Action = NAME_None)
    {
        for (const auto& Entry : Existing.GetMappings())
            if (Entry.Action && (Mapping.IsNone() ? Entry.Action->GetFName() == Action : Entry.GetMappingName() == Mapping))
                return Entry.Action;
        return nullptr;
    }
}

FInputActionValue UChronosStickDirection::ModifyRaw_Implementation(const UEnhancedPlayerInput*, FInputActionValue Value, float)
{
    return FInputActionValue(FMath::Max(0.f, Value.Get<float>() * Direction));
}

const TArray<FChronosPadBinding>& UChronosGamepad::Defaults(bool bAlternateProfile)
{
    const auto Make = [](bool bAlternate) -> TArray<FChronosPadBinding>
    {
        return {
        {TEXT("Chronos.Jump"), LOCTEXT("Jump", "Saltar"), EKeys::Gamepad_FaceButton_Bottom},
        {TEXT("Chronos.Interact"), LOCTEXT("Interact", "Interactuar"), EKeys::Gamepad_FaceButton_Left},
        {TEXT("Chronos.Crouch"), LOCTEXT("Crouch", "Agacharse"), bAlternate ? EKeys::Gamepad_RightThumbstick : EKeys::Gamepad_FaceButton_Right},
        {TEXT("Chronos.Sprint"), LOCTEXT("Sprint", "Correr"), bAlternate ? EKeys::Gamepad_LeftShoulder : EKeys::Gamepad_LeftThumbstick},
        {TEXT("Chronos.Flashlight"), LOCTEXT("Flashlight", "Linterna"), EKeys::Gamepad_DPad_Up},
        {TEXT("Chronos.Incense"), LOCTEXT("Incense", "Incienso"), EKeys::Gamepad_DPad_Down},
        {TEXT("Chronos.Attack"), LOCTEXT("Attack", "Atacar / barrer"), bAlternate ? EKeys::Gamepad_RightShoulder : EKeys::Gamepad_RightTrigger},
        {TEXT("Chronos.Block"), LOCTEXT("Block", "Bloquear"), EKeys::Gamepad_LeftTrigger},
        {TEXT("Chronos.Kick"), LOCTEXT("Kick", "Patada"), bAlternate ? EKeys::Gamepad_RightTrigger : EKeys::Gamepad_RightShoulder},
        {TEXT("Chronos.Journal"), LOCTEXT("Journal", "Cuaderno"), EKeys::Gamepad_Special_Left},
        {TEXT("Chronos.Map"), LOCTEXT("Map", "Mapa"), EKeys::Gamepad_DPad_Left},
        {TEXT("Chronos.Tasks"), LOCTEXT("Tasks", "Lista de tareas"), EKeys::Gamepad_DPad_Right},
        {TEXT("Chronos.Drop"), LOCTEXT("Drop", "Soltar objeto"), EKeys::Gamepad_FaceButton_Top},
        {TEXT("Chronos.HUDMode"), LOCTEXT("HUDMode", "Cambiar modo del HUD"), bAlternate ? EKeys::Gamepad_FaceButton_Right : EKeys::Gamepad_RightThumbstick},
        {TEXT("Chronos.Pause"), LOCTEXT("Pause", "Pausa"), EKeys::Gamepad_Special_Right},
        };
    };
    static const auto StandardBindings = Make(false);
    static const auto AlternateBindings = Make(true);
    return bAlternateProfile ? AlternateBindings : StandardBindings;
}

bool UChronosGamepad::CanRebind(FKey Key)
{
    return Defaults(false).ContainsByPredicate([Key](const auto& Binding) { return Binding.Key == Key; })
        || Key == EKeys::Gamepad_LeftShoulder;
}

FText UChronosGamepad::KeyLabel(FKey Key, bool bPlayStation)
{
    const struct { FKey Key; const TCHAR* PlayStation; const TCHAR* Xbox; } Labels[] = {
        {EKeys::Gamepad_FaceButton_Bottom, TEXT("Cruz"), TEXT("A")},
        {EKeys::Gamepad_FaceButton_Right, TEXT("Círculo"), TEXT("B")},
        {EKeys::Gamepad_FaceButton_Left, TEXT("Cuadrado"), TEXT("X")},
        {EKeys::Gamepad_FaceButton_Top, TEXT("Triángulo"), TEXT("Y")},
        {EKeys::Gamepad_LeftShoulder, TEXT("L1"), TEXT("LB")},
        {EKeys::Gamepad_RightShoulder, TEXT("R1"), TEXT("RB")},
        {EKeys::Gamepad_LeftTrigger, TEXT("L2"), TEXT("LT")},
        {EKeys::Gamepad_RightTrigger, TEXT("R2"), TEXT("RT")},
        {EKeys::Gamepad_LeftThumbstick, TEXT("L3"), TEXT("LS")},
        {EKeys::Gamepad_RightThumbstick, TEXT("R3"), TEXT("RS")},
        {EKeys::Gamepad_Special_Left, TEXT("Crear / Share"), TEXT("Vista")},
        {EKeys::Gamepad_Special_Right, TEXT("Options"), TEXT("Menú")},
        {EKeys::Gamepad_DPad_Up, TEXT("Cruceta arriba"), TEXT("D-Pad arriba")},
        {EKeys::Gamepad_DPad_Down, TEXT("Cruceta abajo"), TEXT("D-Pad abajo")},
        {EKeys::Gamepad_DPad_Left, TEXT("Cruceta izquierda"), TEXT("D-Pad izquierda")},
        {EKeys::Gamepad_DPad_Right, TEXT("Cruceta derecha"), TEXT("D-Pad derecha")},
    };
    for (const auto& Item : Labels)
        if (Key == Item.Key) return FText::FromString(bPlayStation ? Item.PlayStation : Item.Xbox);
    return Key.GetDisplayName();
}

UChronosGamepad* UChronosGamepad::Get(const APlayerController* Player)
{
    return Player ? ULocalPlayer::GetSubsystem<UChronosGamepad>(Player->GetLocalPlayer()) : nullptr;
}

void UChronosGamepad::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UEnhancedInputLocalPlayerSubsystem>();
    if (const auto* Save = Cast<UChronosOptionsSave>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0)))
    {
        bAlternateActive = Save->bAlternateGamepad;
        PreferredGlyphs = static_cast<EChronosGamepadGlyphs>(FMath::Clamp(Save->GamepadGlyphs, 0, 2));
        Standard = Save->GamepadStandard;
        Alternate = Save->GamepadAlternate;
    }
    if (FSlateApplication::IsInitialized())
    {
        Observer = MakeShared<FChronosInputObserver>(*this);
        FSlateApplication::Get().RegisterInputPreProcessor(Observer);
    }
    DeviceHandle = IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddUObject(this, &UChronosGamepad::DeviceChanged);
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UChronosGamepad::Tick), .05f);
}

void UChronosGamepad::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(DeviceHandle);
    if (Observer && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(Observer);
    Observer.Reset();
    if (auto* Input = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(); Context && Input)
        Input->RemoveMappingContext(Context);
    Super::Deinitialize();
}

void UChronosGamepad::OnInput(FKey Key, FInputDeviceId Device)
{
    if (!Key.IsValid()) return;
    const auto User = IPlatformInputDeviceMapper::Get().GetUserForInputDevice(Device);
    if (User.IsValid() && User != GetLocalPlayer()->GetPlatformUserId()) return;
    bUsingGamepad = Key.IsGamepadKey();
    if (!bUsingGamepad) return;
    LastDevice = Device;
    const auto Descriptor = FInputDeviceRegistry::FindDescriptor(Device);
    const FString Hardware = Descriptor.IsSet() ? Descriptor->HardwareDeviceIdentifier.ToString() : FString();
    bDetectedPlayStation = Hardware.Contains(TEXT("DualSense")) || Hardware.Contains(TEXT("DualShock"))
        || Hardware.Contains(TEXT("PlayStation")) || Hardware.Contains(TEXT("Sony"));
}

void UChronosGamepad::DeviceChanged(EInputDeviceConnectionState State, FPlatformUserId, FInputDeviceId Device)
{
    if (State == EInputDeviceConnectionState::Disconnected && Device == LastDevice)
    {
        bUsingGamepad = false;
        bDetectedPlayStation = false;
        if (auto* PC = GetLocalPlayer()->GetPlayerController(GetWorld())) PC->FlushPressedKeys();
    }
}

bool UChronosGamepad::IsPlayStation() const
{
    return PreferredGlyphs == EChronosGamepadGlyphs::PlayStation
        || (PreferredGlyphs == EChronosGamepadGlyphs::Automatic && bDetectedPlayStation);
}

FText UChronosGamepad::Prompt(FKey Key) const
{
    return Key.IsGamepadKey() ? KeyLabel(Key, IsPlayStation()) : Key.GetDisplayName();
}

FKey UChronosGamepad::Key(FName Name, bool bAlternate) const
{
    const auto& Overrides = bAlternate ? Alternate : Standard;
    if (const auto* Override = Overrides.Find(Name); Override && CanRebind(*Override)) return *Override;
    for (const auto& Item : Defaults(bAlternate)) if (Item.Name == Name) return Item.Key;
    return {};
}

bool UChronosGamepad::Tick(float)
{
    auto* Input = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    const auto* Keyboard = ChronosControls::Context();
    if (Input && Keyboard && Input->HasMappingContext(Keyboard))
    {
        if (!Context) Rebuild();
        if (Context && !Input->HasMappingContext(Context))
        {
            FModifyContextOptions Options;
            Options.bIgnoreAllPressedKeysUntilRelease = true;
            Input->AddMappingContext(Context, Priority, Options);
        }
    }
    else if (Input && Context && Input->HasMappingContext(Context)) Input->RemoveMappingContext(Context);
    return true;
}

void UChronosGamepad::Rebuild()
{
    auto* Input = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    const auto* Keyboard = ChronosControls::Context();
    if (!Input || !Keyboard) return;
    if (Context) Input->RemoveMappingContext(Context);
    Context = NewObject<UInputMappingContext>(this);
    for (const auto& Item : Defaults(bAlternateActive))
        if (const auto* Action = FindAction(*Keyboard, Item.Name))
            Context->MapKey(Action, Key(Item.Name, bAlternateActive));

    const auto Analog = [&](const UInputAction* Action, FKey Axis, float Sign, bool bCamera)
    {
        if (!Action) return;
        auto& Mapping = Context->MapKey(Action, Axis);
        auto* DeadZone = NewObject<UInputModifierDeadZone>(Context);
        DeadZone->LowerThreshold = bCamera ? .22f : .19f;
        DeadZone->Type = EDeadZoneType::Axial;
        Mapping.Modifiers.Add(DeadZone);
        if (bCamera)
        {
            auto* Scalar = NewObject<UInputModifierScalar>(Context);
            Scalar->Scalar = FVector(105.f * Sign);
            Mapping.Modifiers.Add(Scalar);
            Mapping.Modifiers.Add(NewObject<UInputModifierScaleByDeltaTime>(Context));
        }
        else
        {
            auto* Direction = NewObject<UChronosStickDirection>(Context);
            Direction->Direction = Sign;
            Mapping.Modifiers.Add(Direction);
        }
    };
    Analog(FindAction(*Keyboard, TEXT("Chronos.Forward")), EKeys::Gamepad_LeftY, 1, false);
    Analog(FindAction(*Keyboard, TEXT("Chronos.Backward")), EKeys::Gamepad_LeftY, -1, false);
    Analog(FindAction(*Keyboard, TEXT("Chronos.Right")), EKeys::Gamepad_LeftX, 1, false);
    Analog(FindAction(*Keyboard, TEXT("Chronos.Left")), EKeys::Gamepad_LeftX, -1, false);
    Analog(FindAction(*Keyboard, NAME_None, TEXT("mousex")), EKeys::Gamepad_RightX, 1, true);
    Analog(FindAction(*Keyboard, NAME_None, TEXT("mousey")), EKeys::Gamepad_RightY, -1, true);
    Tick(0);
}

bool UChronosGamepad::Commit(bool bAlternate, EChronosGamepadGlyphs Glyphs, const TMap<FName, FKey>& Bindings)
{
    TSet<FKey> Used;
    for (const auto& Item : Defaults(bAlternate))
    {
        const auto* Override = Bindings.Find(Item.Name);
        const FKey Key = Override ? *Override : Item.Key;
        if (!CanRebind(Key) || Used.Contains(Key)) return false;
        Used.Add(Key);
    }
    const auto Class = LoadClass<UChronosOptionsSave>(nullptr, SaveClass);
    auto* Save = Cast<UChronosOptionsSave>(UChronosOptionsSave::LoadOptionsForWrite(Class));
    if (!Save) return false;
    Save->bAlternateGamepad = bAlternate;
    Save->GamepadGlyphs = static_cast<int32>(Glyphs);
    (bAlternate ? Save->GamepadAlternate : Save->GamepadStandard) = Bindings;
    if (!UGameplayStatics::SaveGameToSlot(Save, SaveSlot, 0)) return false;
    (bAlternate ? Alternate : Standard) = Bindings;
    bAlternateActive = bAlternate;
    PreferredGlyphs = Glyphs;
    Rebuild();
    return true;
}

#undef LOCTEXT_NAMESPACE
