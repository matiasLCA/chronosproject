#include "ChronosControls.h"
#include "ChronosGamepad.h"
#include "ChronosMenuFocus.h"
#include "Types/NavigationMetaData.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/NativeWidgetHost.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/InputSettings.h"
#include "InputMappingContext.h"
#include "PlayerMappableKeySettings.h"
#include "Styling/CoreStyle.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChronosControls"

namespace ChronosControls
{
    const UInputMappingContext* ItemContext()
    {
        static TWeakObjectPtr<UInputMappingContext> Cached;
        if (!Cached.IsValid()) Cached = LoadObject<UInputMappingContext>(nullptr,
            TEXT("/Game/Characters/input/interaction/IMC_ItemControls.IMC_ItemControls"), nullptr, LOAD_NoWarn);
        return Cached.Get();
    }

    bool CanShare(FName A, FName B)
    {
        const auto IsHeld = [](FName Name) { return Name == TEXT("Chronos.DrinkItem") || Name == TEXT("Chronos.PocketItem"); };
        const auto IsCombat = [](FName Name) { return Name == TEXT("Chronos.Attack") || Name == TEXT("Chronos.Kick") || Name == TEXT("Chronos.Block"); };
        return (IsHeld(A) && IsCombat(B)) || (IsHeld(B) && IsCombat(A))
            || (A == TEXT("Chronos.PickupBeer") && B == TEXT("Chronos.Interact"))
            || (B == TEXT("Chronos.PickupBeer") && A == TEXT("Chronos.Interact"));
    }

    const UInputMappingContext* Context()
    {
        static TWeakObjectPtr<UInputMappingContext> Cached;
        if (!Cached.IsValid())
            Cached = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Characters/input/imc_player.imc_player"));
        return Cached.Get();
    }

    UEnhancedInputUserSettings* Settings(APlayerController* Player)
    {
        auto* Subsystem = Player ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player->GetLocalPlayer()) : nullptr;
        auto* UserSettings = Subsystem ? Subsystem->GetUserSettings() : nullptr;
        const UInputMappingContext* MappingContext = Context();
        if (UserSettings && MappingContext && !UserSettings->IsMappingContextRegistered(MappingContext))
            UserSettings->RegisterInputMappingContext(MappingContext);
        const auto* Consumables = ItemContext();
        if (UserSettings && Consumables && !UserSettings->IsMappingContextRegistered(Consumables))
            UserSettings->RegisterInputMappingContext(Consumables);
        return UserSettings;
    }

    FKey Key(APlayerController* Player, FName Name, FKey Fallback)
    {
        const auto* UserSettings = Settings(Player);
        const auto* Profile = UserSettings ? UserSettings->GetActiveKeyProfile() : nullptr;
        TArray<FKey> Keys;
        if (Profile && Profile->GetMappedKeysInRow(Name, Keys) && Keys[0].IsValid()) return Keys[0];
        return Fallback;
    }
}

namespace
{
    const FSlateColor Gold(FLinearColor(FColor(202, 174, 132)));
    const FSlateColor Ivory(FLinearColor(FColor(239, 218, 181)));

    const FScrollBarStyle& ControlsScrollBar()
    {
        static const FScrollBarStyle Style = FScrollBarStyle()
            .SetVerticalBackgroundImage(FSlateRoundedBoxBrush(FLinearColor(.027f, .018f, .012f), 4.f,
                FLinearColor(.24f, .16f, .078f), 1.f))
            .SetVerticalTopSlotImage(FSlateNoResource())
            .SetVerticalBottomSlotImage(FSlateNoResource())
            .SetNormalThumbImage(FSlateRoundedBoxBrush(FLinearColor(.47f, .32f, .16f), 3.f, FLinearColor(.73f, .54f, .29f), 1.f))
            .SetHoveredThumbImage(FSlateRoundedBoxBrush(FLinearColor(.65f, .47f, .24f), 3.f, FLinearColor(.85f, .66f, .39f), 1.f))
            .SetDraggedThumbImage(FSlateRoundedBoxBrush(FLinearColor(.74f, .54f, .28f), 3.f, FLinearColor(.91f, .74f, .45f), 1.f))
            .SetThickness(10.f);
        return Style;
    }

    constexpr double CaptureSeconds = 12.;

    int32 DisplayPriority(FName Action)
    {
        if (Action == TEXT("Chronos.Inventory")) return 0;
        if (Action == TEXT("Chronos.PickupBeer")) return 1;
        if (Action == TEXT("Chronos.DrinkItem")) return 2;
        if (Action == TEXT("Chronos.PocketItem")) return 3;
        return 4;
    }

    // YukoDev — CONTROLES
    FSlateBrush ButtonBrush(const TCHAR* Directory, const FString& Name, FSlateColor Tint = FSlateColor(FLinearColor::White))
    {
        const FString Path = FString(TEXT("/Game/TheChronosCurse_HUD_Assets/MenuCrontroller/")) + Directory + TEXT("/") + Name + TEXT(".") + Name;
        FSlateBrush Brush;
        if (auto* Texture = LoadObject<UTexture2D>(nullptr, *Path))
        {
            Texture->AddToRoot();
            Brush.SetResourceObject(Texture);
            Brush.DrawAs = ESlateBrushDrawType::Image;
            Brush.TintColor = Tint;
        }
        return Brush;
    }

    FButtonStyle FooterStyle(const TCHAR* Name)
    {
        const FString Prefix = FString(TEXT("BTN_")) + Name + TEXT("_");
        const auto Brush = [&](const TCHAR* State) { return ButtonBrush(TEXT("Buttons_With_Text"), Prefix + State); };
        return FButtonStyle().SetNormal(Brush(TEXT("Normal"))).SetHovered(Brush(TEXT("Hover")))
            .SetPressed(Brush(TEXT("Seleccionado"))).SetDisabled(Brush(TEXT("Deshabilitado")))
            .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
    }

}

const FButtonStyle& ChronosMenuArt::ButtonStyle(bool bSelected)
{
        static const auto Normal = ButtonBrush(TEXT("Templates_Without_Text"), TEXT("BTN_Plantilla_Normal"));
        static const auto Hover = ButtonBrush(TEXT("Templates_Without_Text"), TEXT("BTN_Plantilla_Hover"));
        static const auto Pressed = ButtonBrush(TEXT("Templates_Without_Text"), TEXT("BTN_Plantilla_Presionado"));
        static const auto Disabled = ButtonBrush(TEXT("Templates_Without_Text"), TEXT("BTN_Plantilla_Normal"),
            FSlateColor(FLinearColor(.5f, .5f, .5f, .8f)));
        static const auto Make = [](bool Active)
        {
            return FButtonStyle().SetNormal(Active ? Pressed : Normal).SetHovered(Hover)
                .SetPressed(Pressed).SetDisabled(Active ? Pressed : Disabled)
                .SetNormalPadding(FMargin(6, 2)).SetPressedPadding(FMargin(6, 2));
        };
        static const auto Regular = Make(false);
        static const auto Active = Make(true);
        return bSelected ? Active : Regular;
}

const FButtonStyle& ChronosMenuArt::ConfirmStyle()
{
    static const auto Style = FooterStyle(TEXT("Confirmar_Cambios"));
    return Style;
}

const FSlateBrush& ChronosMenuArt::OptionsPanel()
{
    static const FSlateBrush Brush = []
    {
        FSlateBrush Art = FSlateRoundedBoxBrush(FLinearColor(.018f, .015f, .012f, .97f), 4.f);
        if (auto* Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/CARGOPARTIDA.CARGOPARTIDA")))
        {
            Texture->AddToRoot();
            Art = FSlateBrush();
            Art.SetResourceObject(Texture);
            Art.DrawAs = ESlateBrushDrawType::Image;
        }
        return Art;
    }();
    return Brush;
}

namespace
{

    FText DisplayKey(FKey Key)
    {
        if (Key == EKeys::LeftMouseButton) return LOCTEXT("LeftClick", "Clic izq.");
        if (Key == EKeys::RightMouseButton) return LOCTEXT("RightClick", "Clic der.");
        if (Key == EKeys::MiddleMouseButton) return LOCTEXT("MiddleClick", "Clic central");
        if (Key == EKeys::SpaceBar) return LOCTEXT("Space", "Espacio");
        if (Key == EKeys::LeftAlt) return LOCTEXT("AltLeft", "Alt izq.");
        if (Key == EKeys::LeftControl) return LOCTEXT("ControlLeft", "Ctrl izq.");
        if (Key == EKeys::LeftShift) return LOCTEXT("ShiftLeft", "Shift izq.");
        return Key.GetDisplayName();
    }

    struct FControlRow
    {
        FName Name;
        FText Label;
        FKey DefaultKey;
        FKey SavedKey;
        FKey PendingKey;
    };

    class SChronosControlList final : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SChronosControlList) {}
            SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Player)
            SLATE_ARGUMENT(FSlateFontInfo, RowFont)
            SLATE_ATTRIBUTE(float, MenuScale)
            SLATE_ATTRIBUTE(float, BindingScale)
        SLATE_END_ARGS()

        void Construct(const FArguments& Args)
        {
            Player = Args._Player;
            Font = Args._RowFont;
            MenuScale = Args._MenuScale;
            BindingScale = Args._BindingScale;
            Font.Size = FMath::Min(Font.Size, 22.f);
            KeyFont = Font;
            KeyFont.Size = 17;
            FooterFont = Font;
            FooterFont.Size = 16;

            TSharedPtr<SScrollBox> List;
            ChildSlot
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().FillHeight(1.f)
                [
                    SAssignNew(List, SScrollBox)
                    .AllowOverscroll(EAllowOverscroll::No)
                    .ScrollBarStyle(&ControlsScrollBar())
                    .ScrollBarThickness(FVector2D(10, 10))
                    .ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll)
                    .NavigationScrollPadding(10)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 8)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this] { return Status; })
                    .AutoWrapText(true).Font(FooterFont).ColorAndOpacity(Ivory)
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 8, 0)
                    [
                        MakeFooter(&ResetStyle, LOCTEXT("Reset", "Restablecer controles"), &SChronosControlList::RequestReset)
                    ]
                    + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 8, 0)
                    [
                        MakeFooter(&ConfirmStyle, LOCTEXT("Confirm", "Confirmar cambios"), &SChronosControlList::Confirm)
                    ]
                    + SHorizontalBox::Slot().FillWidth(1)
                    [
                        MakeFooter(&CancelStyle, LOCTEXT("Cancel", "Cancelar"), &SChronosControlList::Cancel)
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0, 6, 0, 0)
                [
                    SNew(SBox).WidthOverride(255).Visibility_Lambda([this] {
                        return Pending == EState::Conflict ? EVisibility::Visible : EVisibility::Collapsed;
                    })
                    [
                        MakeFooter(&SwapStyle, LOCTEXT("Swap", "Intercambiar teclas"), &SChronosControlList::ConfirmLocal)
                    ]
                ]
            ];

            const auto* Settings = ChronosControls::Settings(Player.Get());
            const auto* Context = ChronosControls::Context();
            if (!Settings || !Settings->GetActiveKeyProfile() || !Context)
            {
                Status = LOCTEXT("Unavailable", "No se pudieron cargar los controles.");
                return;
            }

            TArray<FEnhancedActionKeyMapping> Mappings = Context->GetMappings();
            if (const auto* Items = ChronosControls::ItemContext()) Mappings.Append(Items->GetMappings());
            Mappings.StableSort([](const auto& A, const auto& B)
                { return DisplayPriority(A.GetMappingName()) < DisplayPriority(B.GetMappingName()); });
            for (const FEnhancedActionKeyMapping& Mapping : Mappings)
            {
                const UPlayerMappableKeySettings* Info = Mapping.GetPlayerMappableKeySettings();
                const FName Name = Mapping.GetMappingName();
                if (!Info || Name.IsNone() || !Mapping.IsPlayerMappable()) continue;
                const FKey Saved = ChronosControls::Key(Player.Get(), Name, Mapping.Key);
                const int32 Index = Rows.Add({Name, Info->DisplayName, Mapping.Key, Saved, Saved});
                List->AddSlot().Padding(0, 2, 32, 2)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                    [SNew(STextBlock).Text(Info->DisplayName).Font_Lambda([this] { return ScaledFont(Font); }).ColorAndOpacity(Gold)]
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SBox).WidthOverride_Lambda([this] { return 215.f * BindingScale.Get(1.f); })
                        .HeightOverride_Lambda([this] { return 36.f * BindingScale.Get(1.f); })
                        [
                            SAssignNew(RowButtons.AddDefaulted_GetRef(), SChronosMenuButton).HAlign(HAlign_Center).VAlign(VAlign_Center)
                            .ButtonStyle(&ChronosMenuArt::ButtonStyle())
                            .IsEnabled_Lambda([this] { return Pending == EState::Idle; })
                            .OnClicked(this, &SChronosControlList::BeginCapture, Index)
                            [
                                SNew(STextBlock).Font_Lambda([this] { return ScaledFont(KeyFont); }).ColorAndOpacity(Ivory)
                                .Text_Lambda([this, Index] {
                                    return Selected == Index && Pending == EState::Capture
                                        ? LOCTEXT("WaitingKey", "Esperando...") : DisplayKey(CurrentKey(Index));
                                })
                            ]
                        ]
                    ]
                ];
            }
        }

        bool SupportsKeyboardFocus() const override { return true; }

        FReply OnPreviewKeyDown(const FGeometry&, const FKeyEvent& Event) override
        {
            if (Pending == EState::Idle) return FReply::Unhandled();
            if (Event.GetKey() == EKeys::Escape) { Finish(FText::GetEmpty()); return FReply::Handled(); }
            if (Pending == EState::Capture && !Event.IsRepeat()) return Capture(Event.GetKey());
            return Pending == EState::Capture ? FReply::Handled() : FReply::Unhandled();
        }

        FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent& Event) override
        {
            return Pending == EState::Capture ? Capture(Event.GetEffectingButton()) : FReply::Unhandled();
        }

        void OnFocusLost(const FFocusEvent& Event) override
        {
            if (Pending == EState::Capture && Event.GetCause() == EFocusCause::WindowActivate)
                Finish(FText::GetEmpty());
            SCompoundWidget::OnFocusLost(Event);
        }

        TSharedPtr<SWidget> FirstBinding() const { return RowButtons.IsEmpty() ? nullptr : RowButtons[0]; }

    private:
        enum class EState { Idle, Capture, Conflict, Reset };
        TWeakObjectPtr<APlayerController> Player;
        TArray<FControlRow> Rows;
        FSlateFontInfo Font;
        FSlateFontInfo FooterFont;
        TAttribute<float> MenuScale;
        TAttribute<float> BindingScale;
        FSlateFontInfo ScaledFont(FSlateFontInfo Base) const
        {
            Base.Size = FMath::RoundToInt(Base.Size * BindingScale.Get(1.f));
            return Base;
        }
        FSlateFontInfo KeyFont;
        FText Status;
        EState Pending = EState::Idle;
        int32 Selected = INDEX_NONE;
        int32 Conflicting = INDEX_NONE;
        FKey Candidate;
        double CaptureUntil = 0.;
        TWeakPtr<FActiveTimerHandle> Timer;

        bool NeedsConfirmation() const { return Pending == EState::Conflict || Pending == EState::Reset; }

        TArray<TSharedPtr<SChronosMenuButton>> RowButtons;
        const FButtonStyle ResetStyle = FooterStyle(TEXT("Restablecer_Controles"));
        const FButtonStyle& ConfirmStyle = ChronosMenuArt::ConfirmStyle();
        const FButtonStyle CancelStyle = FooterStyle(TEXT("Cancelar"));
        const FButtonStyle SwapStyle = FooterStyle(TEXT("Intercambiar_Teclas"));

        FKey CurrentKey(int32 Index) const { return Rows[Index].PendingKey; }

        TSharedRef<SWidget> MakeFooter(const FButtonStyle* Style, const FText& Label,
            FReply (SChronosControlList::*Action)())
        {
            return SNew(SBox).HeightOverride_Lambda([this] { return 49.f * MenuScale.Get(1.f); })
            [
                SNew(SChronosMenuButton).ButtonStyle(Style).ToolTipText(Label).OnClicked(this, Action)
                .IsEnabled_Lambda([this, Action] {
                    if (Action == &SChronosControlList::Cancel) return Pending != EState::Idle || HasChanges();
                    if (Action == &SChronosControlList::ConfirmLocal) return NeedsConfirmation();
                    if (Action == &SChronosControlList::Confirm)
                        return (Pending == EState::Idle && HasChanges()) || Pending == EState::Reset;
                    return Pending == EState::Idle && !Rows.IsEmpty();
                })
            ];
        }

        bool HasChanges() const
        {
            return Rows.ContainsByPredicate([](const FControlRow& Row) { return Row.PendingKey != Row.SavedKey; });
        }

        FReply BeginCapture(int32 Index)
        {
            Selected = Index;
            Pending = EState::Capture;
            RowButtons[Index]->SetMenuStyle(ChronosMenuArt::ButtonStyle(true));
            CaptureUntil = FPlatformTime::Seconds() + CaptureSeconds;
            Status = FText::Format(LOCTEXT("Waiting", "{0} · Presiona una tecla o haz clic en el espacio libre del panel. Esc para cancelar."), Rows[Index].Label);
            if (!Timer.IsValid())
                Timer = RegisterActiveTimer(.25f, FWidgetActiveTimerDelegate::CreateSP(this, &SChronosControlList::CheckCapture));
            return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::SetDirectly);
        }

        EActiveTimerReturnType CheckCapture(double Now, float Delta)
        {
            if (Pending == EState::Capture && FSlateApplication::Get().IsActive() && FPlatformTime::Seconds() < CaptureUntil)
                return EActiveTimerReturnType::Continue;
            if (Pending == EState::Capture)
                Finish(LOCTEXT("Expired", "Captura cancelada."));
            Timer.Reset();
            return EActiveTimerReturnType::Stop;
        }

        FReply Capture(FKey Key)
        {
            if (!Key.IsValid() || Key.IsAxis1D() || Key.IsAxis2D() || Key.IsAxis3D() || Key.IsTouch())
                return FReply::Handled();
            if (Key == CurrentKey(Selected)) { Finish(FText::GetEmpty()); return FReply::Handled(); }

            const auto* Input = GetDefault<UInputSettings>();
            if (Input->ConsoleKeys.Contains(Key) || (Key == EKeys::F11 && Input->bF11TogglesFullscreen))
            {
                Finish(LOCTEXT("Reserved", "Esa tecla está asignada a un atajo del sistema."));
                return FReply::Handled();
            }

            Candidate = Key;
            for (int32 Index = 0; Index < Rows.Num(); ++Index)
            {
                if (Index == Selected || CurrentKey(Index) != Key || ChronosControls::CanShare(Rows[Index].Name, Rows[Selected].Name)) continue;
                Pending = EState::Conflict;
                Conflicting = Index;
                Status = FText::Format(LOCTEXT("Conflict", "{0} ya se usa para {1}. Puedes intercambiar las dos teclas o cancelar."),
                    DisplayKey(Key), Rows[Index].Label);
                return FReply::Handled();
            }
            Rows[Selected].PendingKey = Candidate;
            Finish(LOCTEXT("Pending", "Cambios pendientes. Confirma para guardarlos."));
            return FReply::Handled();
        }

        FReply RequestReset()
        {
            Pending = EState::Reset;
            Status = LOCTEXT("ConfirmReset", "¿Preparar los controles predeterminados? Pulsa Confirmar cambios para continuar.");
            return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::SetDirectly);
        }

        FReply ConfirmLocal()
        {
            if (Pending == EState::Conflict)
            {
                Rows[Conflicting].PendingKey = Rows[Selected].PendingKey;
                Rows[Selected].PendingKey = Candidate;
            }
            else if (Pending == EState::Reset)
            {
                for (auto& Row : Rows) Row.PendingKey = Row.DefaultKey;
            }
            Finish(LOCTEXT("Pending", "Cambios pendientes. Confirma para guardarlos."));
            return FReply::Handled();
        }

        FReply Confirm()
        {
            if (Pending == EState::Reset) return ConfirmLocal();
            auto* Settings = ChronosControls::Settings(Player.Get());
            const auto* Profile = Settings ? Settings->GetActiveKeyProfile() : nullptr;
            TMultiMap<FKey, FName> Used;
            TArray<TPair<FMapPlayerKeyArgs, FKey>> Changes;
            for (const auto& Row : Rows)
            {
                TArray<FName> SameKey;
                Used.MultiFind(Row.PendingKey, SameKey);
                if (SameKey.ContainsByPredicate([&](FName Name) { return !ChronosControls::CanShare(Name, Row.Name); }))
                {
                    Finish(LOCTEXT("Duplicate", "Hay teclas duplicadas. Corrige las asignaciones antes de guardar."));
                    return FReply::Handled();
                }
                Used.Add(Row.PendingKey, Row.Name);
                if (Row.PendingKey == Row.SavedKey) continue;
                FMapPlayerKeyArgs Change;
                Change.MappingName = Row.Name;
                Change.Slot = EPlayerMappableKeySlot::First;
                Change.NewKey = Row.PendingKey;
                Change.bCreateMatchingSlotIfNeeded = false;
                if (!Profile || !Profile->FindKeyMapping(Change))
                {
                    Finish(LOCTEXT("Failure", "No se pudieron actualizar los controles."));
                    return FReply::Handled();
                }
                Changes.Emplace(Change, Row.SavedKey);
            }
            if (!Settings || Changes.IsEmpty()) return FReply::Handled();

            int32 Applied = 0;
            for (const auto& Change : Changes)
            {
                FGameplayTagContainer Failure;
                Settings->MapPlayerKey(Change.Key, Failure);
                if (Failure.IsEmpty())
                {
                    ++Applied;
                    continue;
                }
                while (Applied > 0)
                {
                    const auto& Old = Changes[--Applied];
                    auto Rollback = Old.Key;
                    Rollback.NewKey = Old.Value;
                    FGameplayTagContainer Ignored;
                    Settings->MapPlayerKey(Rollback, Ignored);
                }
                Finish(LOCTEXT("Failure", "No se pudieron actualizar los controles."));
                return FReply::Handled();
            }
            if (APlayerController* PC = Player.Get()) PC->FlushPressedKeys();
            Settings->ApplySettings();
            Settings->SaveSettings();
            for (auto& Row : Rows) Row.SavedKey = Row.PendingKey;
            Finish(LOCTEXT("Saved", "Controles actualizados."));
            return FReply::Handled();
        }

        FReply Cancel()
        {
            if (Pending == EState::Idle)
                for (auto& Row : Rows) Row.PendingKey = Row.SavedKey;
            Finish(FText::GetEmpty());
            return FReply::Handled();
        }

        void Finish(FText Message)
        {
            Status = MoveTemp(Message);
            Pending = EState::Idle;
            if (RowButtons.IsValidIndex(Selected)) RowButtons[Selected]->SetMenuStyle(ChronosMenuArt::ButtonStyle());
            Selected = Conflicting = INDEX_NONE;
            Candidate = FKey();
        }
    };

    const FSlateBrush* PadGlyph(FKey Key, bool bPlayStation)
    {
        static const TMap<FKey, FString> Paths = {
            {EKeys::Gamepad_Left2D, TEXT("01_Movimiento")},
            {EKeys::Gamepad_Right2D, TEXT("02_Camara")},
            {EKeys::Gamepad_FaceButton_Bottom, TEXT("03_Saltar")},
            {EKeys::Gamepad_FaceButton_Left, TEXT("04_Interactuar")},
            {EKeys::Gamepad_FaceButton_Right, TEXT("05_Agacharse")},
            {EKeys::Gamepad_LeftThumbstick, TEXT("06_Correr")},
            {EKeys::Gamepad_DPad_Up, TEXT("07_Linterna")},
            {EKeys::Gamepad_DPad_Down, TEXT("08_Incienso")},
            {EKeys::Gamepad_RightTrigger, TEXT("09_Atacar_Barrer")},
            {EKeys::Gamepad_LeftTrigger, TEXT("10_Bloquear")},
            {EKeys::Gamepad_RightShoulder, TEXT("11_Patada")},
            {EKeys::Gamepad_Special_Left, TEXT("12_Cuaderno")},
            {EKeys::Gamepad_DPad_Left, TEXT("13_Mapa")},
            {EKeys::Gamepad_DPad_Right, TEXT("14_Lista_de_tareas")},
            {EKeys::Gamepad_FaceButton_Top, TEXT("15_Soltar_objeto")},
            {EKeys::Gamepad_RightThumbstick, TEXT("16_Cambiar_HUD")},
            {EKeys::Gamepad_Special_Right, TEXT("17_Pausa")},
        };
        static const auto LoadBrushes = [](const TMap<FKey, FString>& Assets, const TCHAR* Platform)
        {
            TMap<FKey, FSlateBrush> Brushes;
            for (const auto& Entry : Assets)
            {
                const FString Path = FString::Printf(TEXT("/Game/TheChronosCurse_HUD_Assets/%s/Glifos_Solos/PNG/%s.%s"),
                    Platform, *Entry.Value, *Entry.Value);
                if (auto* Texture = LoadObject<UTexture2D>(nullptr, *Path))
                {
                    Texture->AddToRoot();
                    auto& Brush = Brushes.Add(Entry.Key);
                    Brush.SetResourceObject(Texture);
                    Brush.DrawAs = ESlateBrushDrawType::Image;
                    Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
                }
            }
            return Brushes;
        };
        static const auto PlayStation = LoadBrushes(Paths, TEXT("PlayStation"));
        static const auto Xbox = LoadBrushes(Paths, TEXT("Xbox"));
        return (bPlayStation ? PlayStation : Xbox).Find(Key);
    }

    class SChronosPadSettings final : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SChronosPadSettings) {}
            SLATE_ARGUMENT(TWeakObjectPtr<UChronosGamepad>, Input)
            SLATE_ARGUMENT(FSlateFontInfo, Font)
            SLATE_ARGUMENT(int32, Tab)
            SLATE_ATTRIBUTE(float, MenuScale)
            SLATE_ATTRIBUTE(float, BindingScale)
        SLATE_END_ARGS()

        void Construct(const FArguments& Args)
        {
            Input = Args._Input;
            Font = Args._Font;
            Font.Size = 19;
            MenuScale = Args._MenuScale;
            BindingScale = Args._BindingScale;
            Tab = Args._Tab;
            bAlternate = Tab == 3;
            Load();
            auto List = SNew(SScrollBox).AllowOverscroll(EAllowOverscroll::No)
                .ScrollBarStyle(&ControlsScrollBar()).ScrollBarThickness(FVector2D(10, 10))
                .ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll).NavigationScrollPadding(10);
            List->AddSlot()[Row(LOCTEXT("PadMove", "Movimiento"), BindingVisual(EKeys::Gamepad_Left2D, LOCTEXT("LeftStick", "Stick izquierdo")))];
            List->AddSlot()[Row(LOCTEXT("PadLook", "Cámara"), BindingVisual(EKeys::Gamepad_Right2D, LOCTEXT("RightStick", "Stick derecho")))];
            TArray<FChronosPadBinding> Bindings = UChronosGamepad::Defaults(bAlternate);
            Bindings.StableSort([](const auto& A, const auto& B)
                { return DisplayPriority(A.Name) < DisplayPriority(B.Name); });
            for (const auto& Binding : Bindings)
            {
                const FName Name = Binding.Name;
                auto BindingButton = SNew(SChronosMenuButton).ButtonStyle(&ChronosMenuArt::ButtonStyle())
                    .OnClicked(this, &SChronosPadSettings::Begin, Name)
                    .HAlign(HAlign_Center).VAlign(VAlign_Center)
                    .ToolTipText_Lambda([this, Name] { return UChronosGamepad::KeyLabel(Pending.FindRef(Name), UsePlayStationArt()); })
                    [BindingVisual(Name)];
                if (!FirstBindingButton) FirstBindingButton = BindingButton;
                List->AddSlot()[Row(Binding.Label,
                    SNew(SBox).MinDesiredHeight_Lambda([this] { return 47.f * BindingScale.Get(1.f); })[BindingButton])];
            }
            ChildSlot
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 1, 0, 6)
                [
                    SNew(STextBlock).Font(Font).ColorAndOpacity(Ivory)
                    .Text_Lambda([this]
                    {
                        if (Input.IsValid() && Input->IsAlternate() == bAlternate)
                            return bAlternate ? LOCTEXT("AltActive", "Alternativo en uso") : LOCTEXT("DefaultActive", "Estándar en uso");
                        return bAlternate ? LOCTEXT("AltInactive", "Confirma los cambios para usar Alternativo")
                            : LOCTEXT("DefaultInactive", "Confirma los cambios para usar Estándar");
                    })
                ]
                + SVerticalBox::Slot().FillHeight(1)[List]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 6)
                [SNew(STextBlock).Font(Font).ColorAndOpacity(Ivory).AutoWrapText(true).Text_Lambda([this] { return Status; })]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 5, 0)
                    [Button(LOCTEXT("PadReset", "Restablecer"), FOnClicked::CreateSP(this, &SChronosPadSettings::Reset))]
                    + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 5, 0)
                    [Button(TAttribute<FText>::CreateLambda([this]
                        { return Conflict.IsNone() ? LOCTEXT("PadConfirm", "Confirmar cambios") : LOCTEXT("PadSwap", "Intercambiar botones"); }),
                        FOnClicked::CreateSP(this, &SChronosPadSettings::Confirm))]
                    + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 5, 0)
                    [Button(TAttribute<FText>::CreateLambda([this] { return GlyphLabel(); }), FOnClicked::CreateSP(this, &SChronosPadSettings::CycleGlyphs))]
                    + SHorizontalBox::Slot().FillWidth(1)
                    [Button(LOCTEXT("PadCancel", "Cancelar"), FOnClicked::CreateSP(this, &SChronosPadSettings::Cancel))]
                ]
            ];
        }

        bool IsCapturing() const { return !Capture.IsNone(); }
        TSharedPtr<SWidget> FirstBinding() const { return FirstBindingButton; }

        FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override
        {
            if (Capture.IsNone()) return SCompoundWidget::OnPreviewKeyDown(Geometry, Event);
            const FKey Key = Event.GetKey();
            if (Key == EKeys::Escape) return Cancel();
            if (!Event.IsRepeat() && UChronosGamepad::CanRebind(Key))
            {
                FName Existing;
                for (const auto& Entry : Pending)
                    if (Entry.Value == Key && Entry.Key != Capture && !ChronosControls::CanShare(Entry.Key, Capture))
                    { Existing = Entry.Key; break; }
                if (!Existing.IsNone())
                {
                    Conflict = Existing;
                    ToSwap = Capture;
                    Candidate = Key;
                    Status = LOCTEXT("PadConflict", "Ya se usa ese botón. Intercambia las dos asignaciones o cancela.");
                }
                else
                {
                    Pending.Add(Capture, Key);
                    Status = FText();
                }
                Capture = NAME_None;
                return FReply::Handled();
            }
            return Key.IsGamepadKey() ? FReply::Handled() : FReply::Unhandled();
        }

    private:
        TSharedRef<SWidget> Caption(TAttribute<FText> Text) const
        {
            return SNew(STextBlock).Font_Lambda([this] { return ChronosMenuArt::ScaleFont(Font, MenuScale.Get(1.f)); })
                .ColorAndOpacity(Ivory).Text(MoveTemp(Text));
        }

        TSharedRef<SWidget> Button(TAttribute<FText> Text, FOnClicked Click) const
        {
            return SNew(SBox).MinDesiredHeight_Lambda([this] { return 33.f * MenuScale.Get(1.f); })
                [SNew(SChronosMenuButton).ButtonStyle(&ChronosMenuArt::ButtonStyle()).OnClicked(MoveTemp(Click))
                    .HAlign(HAlign_Center).VAlign(VAlign_Center)[Caption(MoveTemp(Text))]];
        }

        bool UsePlayStationArt() const
        {
            return Tab == 1 || (Tab == 3 && Input.IsValid() && Input->IsPlayStation());
        }

        TSharedRef<SWidget> BindingVisual(FName Action) const
        {
            return BindingVisual(
                TAttribute<FKey>::CreateLambda([this, Action] { return Capture == Action ? FKey() : Pending.FindRef(Action); }),
                TAttribute<FText>::CreateLambda([this, Action]
                {
                    return Capture == Action ? LOCTEXT("WaitingPad", "Pulsa un botón")
                        : UChronosGamepad::KeyLabel(Pending.FindRef(Action), UsePlayStationArt());
                }));
        }

        TSharedRef<SWidget> BindingVisual(TAttribute<FKey> Key, TAttribute<FText> Label) const
        {
            return SNew(SWidgetSwitcher)
                .WidgetIndex_Lambda([this, Key] { return PadGlyph(Key.Get(), UsePlayStationArt()) ? 1 : 0; })
                + SWidgetSwitcher::Slot()[Caption(MoveTemp(Label))]
                + SWidgetSwitcher::Slot()
                [
                    SNew(SBox).WidthOverride_Lambda([this] { return 132.f * BindingScale.Get(1.f); })
                        .HeightOverride_Lambda([this] { return 42.f * BindingScale.Get(1.f); })
                    [
                        SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                        [SNew(SImage).Image_Lambda([this, Key] { return PadGlyph(Key.Get(), UsePlayStationArt()); })]
                    ]
                ];
        }

        TSharedRef<SWidget> Row(const FText& Text, TSharedRef<SWidget> Control) const
        {
            return SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0, 1)
                [SNew(STextBlock).Font_Lambda([this]
                {
                    auto Scaled = Font;
                    Scaled.Size = FMath::RoundToInt(Scaled.Size * BindingScale.Get(1.f));
                    return Scaled;
                }).ColorAndOpacity(Gold).Text(Text)]
                + SHorizontalBox::Slot().FillWidth(.60f).VAlign(VAlign_Center).HAlign(HAlign_Left).Padding(10, 2, 22, 2)
                [SNew(SBox).WidthOverride_Lambda([this] { return 200.f * BindingScale.Get(1.f); })[Control]];
        }

        void Load()
        {
            Pending.Reset();
            for (const auto& Binding : UChronosGamepad::Defaults(bAlternate))
                Pending.Add(Binding.Name, Input.IsValid() ? Input->Key(Binding.Name, bAlternate) : Binding.Key);
            Capture = Conflict = ToSwap = NAME_None;
            Preferred = Input.IsValid() ? Input->Glyphs() : EChronosGamepadGlyphs::Automatic;
        }

        FReply Begin(FName Name)
        {
            if (Conflict.IsNone())
            {
                Capture = Name;
                Status = LOCTEXT("PadCaptureStatus", "Pulsa el nuevo botón del mando.");
            }
            return FReply::Handled();
        }

        FReply Reset()
        {
            Capture = Conflict = ToSwap = NAME_None;
            for (const auto& Binding : UChronosGamepad::Defaults(bAlternate)) Pending.Add(Binding.Name, Binding.Key);
            Status = LOCTEXT("PadResetPending", "Asignaciones predeterminadas. Confirma para aplicarlas.");
            return FReply::Handled();
        }

        FReply Confirm()
        {
            if (!Conflict.IsNone())
            {
                Pending.Add(Conflict, Pending.FindRef(ToSwap));
                Pending.Add(ToSwap, Candidate);
                Conflict = ToSwap = NAME_None;
                Status = LOCTEXT("PadSwapped", "Botones intercambiados. Confirma para aplicarlos.");
                return FReply::Handled();
            }
            if (!Capture.IsNone()) return FReply::Handled();
            Status = Input.IsValid() && Input->Commit(bAlternate, Preferred, Pending)
                ? LOCTEXT("PadSaved", "Controles del mando guardados.") : LOCTEXT("PadSaveError", "No se pudieron guardar los controles.");
            return FReply::Handled();
        }

        FReply Cancel()
        {
            if (Capture.IsNone() && Conflict.IsNone()) Load();
            Capture = Conflict = ToSwap = NAME_None;
            Status = FText();
            return FReply::Handled();
        }

        FText GlyphLabel() const
        {
            switch (Preferred)
            {
                case EChronosGamepadGlyphs::PlayStation: return LOCTEXT("GlyphPS", "Iconos PlayStation");
                case EChronosGamepadGlyphs::Xbox: return LOCTEXT("GlyphXbox", "Iconos Xbox");
                default: return LOCTEXT("GlyphAuto", "Iconos automáticos");
            }
        }

        FReply CycleGlyphs()
        {
            Preferred = static_cast<EChronosGamepadGlyphs>((static_cast<uint8>(Preferred) + 1) % 3);
            return FReply::Handled();
        }

        TWeakObjectPtr<UChronosGamepad> Input;
        TSharedPtr<SWidget> FirstBindingButton;
        TAttribute<float> MenuScale;
        TAttribute<float> BindingScale;
        TMap<FName, FKey> Pending;
        FSlateFontInfo Font;
        FText Status;
        FName Capture;
        FName Conflict;
        FName ToSwap;
        FKey Candidate;
        EChronosGamepadGlyphs Preferred = EChronosGamepadGlyphs::Automatic;
        int32 Tab = 0;
        bool bAlternate = false;
    };

    class SChronosControlTabs final : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SChronosControlTabs) {}
            SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Player)
            SLATE_ARGUMENT(FSlateFontInfo, Font)
        SLATE_END_ARGS()

        void Construct(const FArguments& Args)
        {
            Player = Args._Player;
            Settings = UChronosMenuSettings::Get(Player.Get());
            Zoom = Settings.IsValid() ? Settings->BindingZoom() : 100;
            Font = Args._Font;
            Font.Size = 15;
            auto Navigation = SNew(SHorizontalBox);
            const FText Labels[] = {LOCTEXT("KeyMouseTab", "Teclado y ratón"), LOCTEXT("PSTab", "PlayStation"),
                LOCTEXT("XboxTab", "Xbox"), LOCTEXT("AltTab", "Alternativo")};
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(Labels); ++Index)
            {
                Navigation->AddSlot().FillWidth(1).Padding(0, 0, 7, 0)
                    [SNew(SBox).MinDesiredHeight_Lambda([this] { return 40.f * MenuScale(); })
                        [SAssignNew(Tabs[Index], SChronosMenuButton).ButtonStyle(&ChronosMenuArt::ButtonStyle()).HAlign(HAlign_Center).VAlign(VAlign_Center)
                            .OnClicked(this, &SChronosControlTabs::Switch, Index)
                            [SNew(STextBlock).Text(Labels[Index]).Font_Lambda([this] { return ChronosMenuArt::ScaleFont(Font, MenuScale()); }).ColorAndOpacity(Ivory)]]];
            }
            ChildSlot
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1)
                    [
                        SNew(STextBlock).Font(Font).ColorAndOpacity(Gold).Text(LOCTEXT("ControlsHeading", "Controles"))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
                    [
                        ZoomButton(LOCTEXT("ZoomOut", "−"), LOCTEXT("ZoomOutLabel", "Reducir escala de controles"), -15)
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
                    [
                        SNew(STextBlock).Font(Font).ColorAndOpacity(Ivory)
                        .Text_Lambda([this] { return FText::AsPercent(Zoom / 100.); })
                    ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        ZoomButton(LOCTEXT("ZoomIn", "+"), LOCTEXT("ZoomInLabel", "Ampliar escala de controles"), 15)
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[Navigation]
                + SVerticalBox::Slot().FillHeight(1)
                [
                    SAssignNew(Panel, SBox)
                    [SAssignNew(Keyboard, SChronosControlList).Player(Player).RowFont(Args._Font).MenuScale(this, &SChronosControlTabs::MenuScale).BindingScale(this, &SChronosControlTabs::Scale)]
                ]
            ];
            UpdateTabs();
        }

        bool SupportsKeyboardFocus() const override { return true; }

        FReply OnFocusReceived(const FGeometry&, const FFocusEvent&) override
        {
            return FReply::Handled().SetUserFocus(Tabs[Active].ToSharedRef());
        }

        FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override
        {
            if (Pad && Pad->IsCapturing()) return FReply::Unhandled();
            const FKey Key = Event.GetKey();
            if (!Event.IsRepeat() && (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder))
            {
                const int32 Next = (Active + (Key == EKeys::Gamepad_LeftShoulder ? 3 : 1)) % 4;
                Switch(Next);
                return FReply::Handled().SetUserFocus(Tabs[Next].ToSharedRef());
            }
            return SCompoundWidget::OnPreviewKeyDown(Geometry, Event);
        }

    private:
        TSharedRef<SWidget> ZoomButton(FText Symbol, FText Tooltip, int32 Delta)
        {
            auto SymbolFont = Font;
            SymbolFont.Size = 21;
            return SNew(SBox).WidthOverride(50).HeightOverride_Lambda([this] { return 37.f * MenuScale(); })
            [
                SNew(SChronosMenuButton).ButtonStyle(&ChronosMenuArt::ButtonStyle())
                .HAlign(HAlign_Center).VAlign(VAlign_Center).ToolTipText(Tooltip)
                .OnClicked(this, &SChronosControlTabs::AdjustZoom, Delta)
                [SNew(STextBlock).Text(Symbol).Font(SymbolFont).ColorAndOpacity(Ivory)]
            ];
        }

        FReply AdjustZoom(int32 Delta)
        {
            const int32 Next = FMath::Clamp(Zoom + Delta, 75, 150);
            if (Next != Zoom && (!Settings.IsValid() || Settings->SetBindingZoom(Next))) Zoom = Next;
            return FReply::Handled();
        }

        float MenuScale() const { return Settings.IsValid() ? Settings->MenuScale() : 1.f; }
        float Scale() const { return Zoom / 100.f * MenuScale(); }

        FReply Switch(int32 Index)
        {
            if (Active == Index) return FReply::Handled();
            Active = Index;
            if (Active == 0)
            {
                Pad.Reset();
                Panel->SetContent(SAssignNew(Keyboard, SChronosControlList).Player(Player).RowFont(Font).MenuScale(this, &SChronosControlTabs::MenuScale).BindingScale(this, &SChronosControlTabs::Scale));
            }
            else
            {
                Keyboard.Reset();
                Panel->SetContent(SAssignNew(Pad, SChronosPadSettings).Input(UChronosGamepad::Get(Player.Get())).Font(Font)
                    .Tab(Index).MenuScale(this, &SChronosControlTabs::MenuScale).BindingScale(this, &SChronosControlTabs::Scale));
            }
            UpdateTabs();
            return FReply::Handled();
        }

        void UpdateTabs()
        {
            const auto First = Pad ? Pad->FirstBinding() : Keyboard->FirstBinding();
            for (int32 Index = 0; Index < 4; ++Index)
            {
                Tabs[Index]->SetMenuStyle(ChronosMenuArt::ButtonStyle(Index == Active));
                const auto Navigation = Tabs[Index]->FindOrAddMetaData<FNavigationMetaData>();
                if (First) Navigation->SetNavigationExplicit(EUINavigation::Down, First);
                else Navigation->SetNavigationEscape(EUINavigation::Down);
            }
            if (First) First->FindOrAddMetaData<FNavigationMetaData>()->SetNavigationExplicit(EUINavigation::Up, Tabs[Active]);
        }

        TWeakObjectPtr<APlayerController> Player;
        TSharedPtr<SChronosMenuButton> Tabs[4];
        TSharedPtr<SBox> Panel;
        TSharedPtr<SChronosPadSettings> Pad;
        TSharedPtr<SChronosControlList> Keyboard;
        FSlateFontInfo Font;
        TWeakObjectPtr<UChronosMenuSettings> Settings;
        int32 Zoom = 100;
        int32 Active = 0;
    };
}

void UChronosControls::NativeConstruct()
{
    Super::NativeConstruct();
    auto* Panel = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("panelControles")));
    auto* Columns = GetWidgetFromName(TEXT("columnas"));
    const auto* Labels = Cast<UTextBlock>(GetWidgetFromName(TEXT("acciones")));
    if (!Panel || !Columns || !Labels || SelectorHost) return;

    // Reuse the original canvas, frame, navigation and label font.
    const auto List = SNew(SChronosControlTabs).Player(GetOwningPlayer()).Font(Labels->GetFont());
    auto* Host = WidgetTree->ConstructWidget<UNativeWidgetHost>();
    Host->SetContent(List);
    SelectorHost = Host;
    UCanvasPanelSlot* ListSlot = Panel->AddChildToCanvas(Host);
    ListSlot->SetAnchors(FAnchors(0, 0, 1, 1));
    ListSlot->SetOffsets(FMargin(0));
    if (auto* PanelSlot = Cast<UCanvasPanelSlot>(Panel->Slot)) ChronosMenuArt::PlaceOptionsPanel(*PanelSlot, true);
    Columns->SetVisibility(ESlateVisibility::Collapsed);
}

const FSlateBrush* ChronosControls::Glyph(FKey Key, bool bPlayStation)
{
    return PadGlyph(Key, bPlayStation);
}

#undef LOCTEXT_NAMESPACE
