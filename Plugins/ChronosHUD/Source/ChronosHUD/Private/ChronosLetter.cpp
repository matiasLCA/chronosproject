#include "ChronosLetter.h"

#include "ChronosControls.h"
#include "ChronosGamepad.h"
#include "ChronosHUDLayer.h"
#include "ChronosTutorial.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
    constexpr double ZoomStep = .4;

    double GetValue(const UObject& Widget, FName Name)
    {
        const auto* Property = FindFProperty<FDoubleProperty>(Widget.GetClass(), Name);
        return Property ? Property->GetPropertyValue_InContainer(&Widget) : 0.;
    }

    void AddValue(UObject& Widget, FName Name, double Delta)
    {
        if (const auto* Property = FindFProperty<FDoubleProperty>(Widget.GetClass(), Name))
            Property->SetPropertyValue_InContainer(&Widget, Property->GetPropertyValue_InContainer(&Widget) + Delta);
    }
}

void UChronosLetter::NativeConstruct()
{
    SetIsFocusable(true);
    Super::NativeConstruct();
    auto* PC = GetOwningPlayer();
    auto* World = GetWorld();
    auto* NoteClass = FindObject<UClass>(nullptr, TEXT("/Game/Letter/BP-Notas.BP-Notas_C"));
    if (!PC || !World || !NoteClass) return;
    const auto* WidgetProperty = FindFProperty<FObjectPropertyBase>(NoteClass, TEXT("widgetNota"));
    if (!WidgetProperty) return;
    for (TActorIterator<AActor> Note(World, NoteClass); Note; ++Note)
    {
        if (WidgetProperty->GetObjectPropertyValue_InContainer(*Note) != this) continue;
        Source = *Note;
        break;
    }
    if (!Source.IsValid()) return;

    bReading = true;
    bFocusPending = true;
    bShowCursorBefore = PC->bShowMouseCursor;
    SetVisibility(ESlateVisibility::Visible);
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockInFullscreen);
    PC->SetInputMode(Mode);
    if (auto* HUD = World->GetSubsystem<UChronosHUDSubsystem>()) HUD->SetLetterReader(this);

    const auto HideHints = [this, PC](TSubclassOf<UUserWidget> Class)
    {
        if (!Class) return;
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, Class, true);
        for (auto* Widget : Widgets)
        {
            if (Widget->GetOwningPlayer() && Widget->GetOwningPlayer() != PC) continue;
            ObscuredHints.Add(Widget, Widget->GetVisibility());
            Widget->SetVisibility(ESlateVisibility::Hidden);
        }
    };
    HideHints(UChronosTutorial::StaticClass());
    HideHints(FindObject<UClass>(nullptr, TEXT("/Game/UI/WBP_Objetivo.WBP_Objetivo_C")));
}

void UChronosLetter::NativeDestruct()
{
    const auto* PC = GetOwningPlayer();
    if (bReading && GetWorld() && !GetWorld()->bIsTearingDown && IsValid(PC) && PC->GetPawn()
        && GetWorld()->GetFirstPlayerController() == PC) Close();
    RestoreHints();
    Super::NativeDestruct();
}

void UChronosLetter::RestoreHints()
{
    for (const auto& Pair : ObscuredHints)
        if (auto* Hint = Pair.Key.Get()) Hint->SetVisibility(Pair.Value);
    ObscuredHints.Reset();
}

FKey UChronosLetter::CloseKey() const
{
    return ChronosControls::Key(GetOwningPlayer(), TEXT("Chronos.Interact"), EKeys::E);
}

int32 UChronosLetter::ZoomPercent() const
{
    return FMath::Max(100, FMath::RoundToInt(GetValue(*this, TEXT("zoomObjetivo")) * 100.));
}

void UChronosLetter::Close()
{
    if (!bReading) return;
    bReading = false;
    bFocusPending = false;
    PadAxis = FVector2D::ZeroVector;
    RestoreHints();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* PC = GetOwningPlayer();
    auto* Note = Source.Get();
    auto* CloseNote = Note ? Note->FindFunction(TEXT("cerrarNota")) : nullptr;
    if (CloseNote) Note->ProcessEvent(CloseNote, nullptr);
    if (!PC) return;
    if (!CloseNote)
    {
        PC->SetIgnoreMoveInput(false);
        PC->SetIgnoreLookInput(false);
    }
    FInputModeGameOnly Mode;
    Mode.SetConsumeCaptureMouseDown(false);
    PC->SetInputMode(Mode);
    PC->bShowMouseCursor = bShowCursorBefore;
}

void UChronosLetter::AdjustZoom(double Delta)
{
    auto* Function = FindFunction(TEXT("cambiarZoom"));
    if (!Function) return;
    const auto* Amount = FindFProperty<FDoubleProperty>(Function, TEXT("delta"));
    if (!Amount) return;
    FStructOnScope Arguments(Function);
    Amount->SetPropertyValue_InContainer(Arguments.GetStructMemory(), Delta);
    ProcessEvent(Function, Arguments.GetStructMemory());
}

void UChronosLetter::Pan(FVector2D Delta)
{
    if (GetValue(*this, TEXT("zoomActual")) <= 1.01) return;
    AddValue(*this, TEXT("panX"), Delta.X);
    AddValue(*this, TEXT("panY"), Delta.Y);
}

void UChronosLetter::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    if (bReading && bFocusPending)
    {
        bFocusPending = false;
        if (auto* PC = GetOwningPlayer(); PC && !HasUserFocus(PC)) SetUserFocus(PC);
    }
    if (bReading && !PadAxis.IsNearlyZero())
        Pan(FVector2D(-PadAxis.X, PadAxis.Y) * FMath::Min(DeltaTime, .05f) * 560.);
    Super::NativeTick(Geometry, DeltaTime);
}

FReply UChronosLetter::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (!bReading) return Super::NativeOnKeyDown(Geometry, Event);
    const FKey Key = Event.GetKey();
    if (Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Escape || Key == CloseKey())
    {
        if (!Event.IsRepeat()) Close();
    }
    else if (Key == EKeys::Gamepad_RightShoulder || Key == EKeys::Add || Key == EKeys::Equals)
        AdjustZoom(ZoomStep);
    else if (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Subtract || Key == EKeys::Hyphen)
        AdjustZoom(-ZoomStep);
    else if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left) Pan(FVector2D(55, 0));
    else if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right) Pan(FVector2D(-55, 0));
    else if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up) Pan(FVector2D(0, 55));
    else if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down) Pan(FVector2D(0, -55));
    return FReply::Handled();
}

FReply UChronosLetter::NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event)
{
    if (!bReading) return Super::NativeOnAnalogValueChanged(Geometry, Event);
    const float Value = Event.GetAnalogValue();
    const float Axis = FMath::Abs(Value) >= .22f ? Value : 0.f;
    if (Event.GetKey() == EKeys::Gamepad_RightX) PadAxis.X = Axis;
    if (Event.GetKey() == EKeys::Gamepad_RightY) PadAxis.Y = Axis;
    return FReply::Handled();
}

FReply UChronosLetter::NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (!bReading) return Super::NativeOnMouseWheel(Geometry, Event);
    if (auto* Pad = UChronosGamepad::Get(GetOwningPlayer())) Pad->OnInput(EKeys::MouseWheelAxis, Event.GetInputDeviceId());
    AdjustZoom(Event.GetWheelDelta() * ZoomStep);
    return FReply::Handled();
}

FReply UChronosLetter::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (!bReading) return Super::NativeOnMouseMove(Geometry, Event);
    Pan(Event.GetCursorDelta());
    return FReply::Handled();
}

