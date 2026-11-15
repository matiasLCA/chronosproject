#include "ChronosTutorial.h"
#include "ChronosControls.h"
#include "ChronosGamepad.h"

#include "Components/TextBlock.h"

// YukoDev — INDICACIONES DE ENTRADA
void UChronosTutorial::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    SampleIn -= DeltaSeconds;
    auto* Player = GetOwningPlayer();
    if (SampleIn > 0.f || !Player) return;
    SampleIn = .1f;
    const auto* Pad = UChronosGamepad::Get(Player);
    const auto Label = [&](FName Action)
    {
        if (Pad && Pad->IsGamepadActive()) return Pad->Prompt(Pad->Key(Action, Pad->IsAlternate())).ToString();
        const FKey Key = ChronosControls::Key(Player, Action, FKey());
        if (Key == EKeys::LeftMouseButton) return FString(TEXT("Clic izq."));
        if (Key == EKeys::RightMouseButton) return FString(TEXT("Clic der."));
        return Key.IsValid() ? Key.GetDisplayName().ToString() : FString();
    };
    const auto Update = [&](FName Description, FName Prompt)
    {
        const auto* Desc = Cast<UTextBlock>(GetWidgetFromName(Description));
        auto* Hint = Cast<UTextBlock>(GetWidgetFromName(Prompt));
        if (!Desc || !Hint) return;
        const FString Text = Desc->GetText().ToString();
        FString NewPrompt;
        if (Text.Contains(TEXT("Moverse")))
        {
            NewPrompt = Pad && Pad->IsGamepadActive() ? TEXT("Stick izquierdo")
                : Label(TEXT("Chronos.Forward")) + TEXT(" ") + Label(TEXT("Chronos.Left")) + TEXT(" ")
                    + Label(TEXT("Chronos.Backward")) + TEXT(" ") + Label(TEXT("Chronos.Right"));
        }
        else if (Text.Contains(TEXT("Correr"))) NewPrompt = Label(TEXT("Chronos.Sprint"));
        else if (Text.Contains(TEXT("Saltar"))) NewPrompt = Label(TEXT("Chronos.Jump"));
        else if (Text.Contains(TEXT("Agacharse"))) NewPrompt = Label(TEXT("Chronos.Crouch"));
        else if (Text.Contains(TEXT("Golpear")) || Text.Contains(TEXT("Patada")))
            NewPrompt = Label(TEXT("Chronos.Attack")) + TEXT(" / ") + Label(TEXT("Chronos.Kick"));
        else if (Text.Contains(TEXT("Bloquear"))) NewPrompt = Label(TEXT("Chronos.Block"));
        else if (Text.Contains(TEXT("Interactuar"))) NewPrompt = Label(TEXT("Chronos.Interact"));
        else if (Text.Contains(TEXT("Linterna"))) NewPrompt = Label(TEXT("Chronos.Flashlight"));
        else if (Text.Contains(TEXT("Incienso"))) NewPrompt = Label(TEXT("Chronos.Incense"));
        if (!NewPrompt.IsEmpty() && Hint->GetText().ToString() != NewPrompt) Hint->SetText(FText::FromString(NewPrompt));
    };
    Update(TEXT("descripcionIzq"), TEXT("teclaIzq"));
    Update(TEXT("descripcionDer"), TEXT("teclaDer"));
}
