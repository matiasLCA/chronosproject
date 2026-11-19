#include "ChronosOptionPanels.h"

#include "ChronosGraphicsSettings.h"
#include "ChronosMenuArt.h"
#include "ChronosMenuFocus.h"
#include "ChronosMenuSettings.h"
#include "Algo/IndexOf.h"
#include "Algo/AnyOf.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Input/NavigationReply.h"
#include "Layout/WidgetPath.h"
#include "Styling/CoreStyle.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChronosGraphicsPanel"

const FSlateBrush& ChronosMenuArt::Premium(const FString& Name)
{
    struct FAsset
    {
        TStrongObjectPtr<UTexture2D> Texture;
        FSlateBrush Brush;
    };
    static TMap<FString, TUniquePtr<FAsset>> Assets;
    if (auto* Found = Assets.Find(Name)) return (*Found)->Brush;
    auto Asset = MakeUnique<FAsset>();
    const FString Path = FString::Printf(TEXT("/Game/TheChronosCurse_HUD_Assets/PREMIUM/%s.%s"), *Name, *Name);
    Asset->Texture.Reset(LoadObject<UTexture2D>(nullptr, *Path));
    Asset->Brush.SetResourceObject(Asset->Texture.Get());
    Asset->Brush.DrawAs = Asset->Texture ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
    if (Asset->Texture)
        Asset->Brush.ImageSize = FVector2D(Asset->Texture->GetSizeX(), Asset->Texture->GetSizeY());
    return Assets.Add(Name, MoveTemp(Asset))->Brush;
}

const FButtonStyle& ChronosMenuArt::PremiumButton(const FString& Prefix, bool bSelected)
{
    static TMap<FString, TUniquePtr<FButtonStyle>> Styles;
    const FString Key = Prefix + (bSelected ? TEXT("_selected") : TEXT("_normal"));
    if (auto* Found = Styles.Find(Key)) return **Found;
    auto Style = MakeUnique<FButtonStyle>();
    Style->SetNormal(Premium(Key)).SetHovered(Premium(Prefix + TEXT("_hover")))
        .SetPressed(Premium(Prefix + TEXT("_selected"))).SetDisabled(Premium(Prefix + TEXT("_disabled")))
        .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
    return *Styles.Add(Key, MoveTemp(Style));
}

namespace
{
    const FLinearColor Gold(FColor(218, 188, 131));
    const FLinearColor Ivory(FColor(239, 226, 199));
    const FLinearColor Muted(FColor(169, 154, 132));
    const FLinearColor Green(FColor(167, 216, 117));
    const FLinearColor Red(FColor(248, 123, 91));

    enum class ESetting : uint8 { Preset, Mode, Resolution, VSync, FPS, Quality, Textures, Shadows, Effects, Post, Reflections, RayTracing, Distance };
    struct FRow
    {
        ESetting Setting;
        const TCHAR* Icon;
        const TCHAR* Name;
        const TCHAR* Description;
    };
    const FRow Rows[] = {
        {ESetting::Preset, TEXT("graficos"), TEXT("Preajuste de calidad"), TEXT("Ajusta todos los grupos de calidad a la vez.")},
        {ESetting::Mode, TEXT("pantalla"), TEXT("Modo de pantalla"), TEXT("Pantalla completa, sin bordes o ventana.")},
        {ESetting::Resolution, TEXT("resolucion"), TEXT("Resolución"), TEXT("Sin bordes utiliza la resolución del escritorio.")},
        {ESetting::VSync, TEXT("vsync"), TEXT("VSync"), TEXT("Sincroniza los fotogramas con la pantalla para reducir el desgarro de imagen.")},
        {ESetting::FPS, TEXT("fps"), TEXT("Límite de FPS"), TEXT("Fija un máximo de fotogramas. El rendimiento efectivo depende del equipo.")},
        {ESetting::Quality, TEXT("graficos"), TEXT("Calidad de gráficos"), TEXT("Ajusta la calidad de la iluminación global.")},
        {ESetting::Textures, TEXT("texturas"), TEXT("Calidad de texturas"), TEXT("Controla los niveles de detalle y el presupuesto de streaming.")},
        {ESetting::Shadows, TEXT("sombras"), TEXT("Calidad de sombras"), TEXT("Controla el detalle y la distancia de las sombras.")},
        {ESetting::Effects, TEXT("efectos"), TEXT("Calidad de efectos"), TEXT("Controla los grupos escalables de partículas y efectos ambientales.")},
        {ESetting::Post, TEXT("postprocesado"), TEXT("Calidad de postprocesado"), TEXT("Controla la calidad de los efectos aplicados a la imagen final.")},
        {ESetting::Reflections, TEXT("reflejos"), TEXT("Reflejos"), TEXT("Ajusta la calidad de los reflejos del método activo, incluido Lumen cuando está disponible.")},
        {ESetting::RayTracing, TEXT("rayos"), TEXT("Trazado de rayos"), TEXT("Habilita hardware Ray Tracing cuando la GPU, el RHI y los shaders lo permiten.")},
        {ESetting::Distance, TEXT("graficos"), TEXT("Distancia de dibujado"), TEXT("Ajusta la distancia de visualización de los objetos escalables.")},
    };
    constexpr int32 FrameLimits[] = {30, 60, 120, 240, 360, 0, ChronosGraphics::AutomaticFPS};
    const TCHAR* Selector = TEXT("selector_355x48");

    FText QualityName(int32 Level)
    {
        switch (Level)
        {
            case 0: return LOCTEXT("Low", "Bajo");
            case 1: return LOCTEXT("Medium", "Medio");
            case 2: return LOCTEXT("High", "Alto");
            case 3: return LOCTEXT("Epic", "Épico");
            case 4: return LOCTEXT("Ultra", "Ultra");
            default: return LOCTEXT("Custom", "Personalizado");
        }
    }

    TSharedRef<SWidget> Icon(const TCHAR* Name, float Size)
    {
        return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
        [
            SNew(SImage).Image(&ChronosMenuArt::Premium(FString::Printf(TEXT("icono_%s_48x48"), Name)))
        ];
    }

    class SChronosGraphicsPanel final : public SCompoundWidget
    {
        struct FSelector
        {
            const FRow* Row;
            TSharedPtr<SWidget> Previous;
            TSharedPtr<SWidget> Next;
        };

    public:
        SLATE_BEGIN_ARGS(SChronosGraphicsPanel) {}
            SLATE_ARGUMENT(FSlateFontInfo, Font)
            SLATE_ARGUMENT(TWeakObjectPtr<UChronosMenuSettings>, MenuSettings)
        SLATE_END_ARGS()

        void Construct(const FArguments& Args)
        {
            Font = Args._Font;
            MenuSettings = Args._MenuSettings;
            if (const auto* Settings = GEngine->GetGameUserSettings()) Saved.Read(*Settings);
            Pending = Saved;
            for (const TCHAR* State : {TEXT("verde"), TEXT("dorado"), TEXT("rojo")})
                ChronosMenuArt::Premium(FString::Printf(TEXT("vram_relleno_%s_334x18"), State));

            static const FScrollBarStyle ScrollStyle = FScrollBarStyle(FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>(TEXT("Scrollbar")))
                .SetVerticalBackgroundImage(ChronosMenuArt::Premium(TEXT("scrollbar_canal_16x530")))
                .SetNormalThumbImage(ChronosMenuArt::Premium(TEXT("scrollbar_pulgar_16x200")))
                .SetHoveredThumbImage(ChronosMenuArt::Premium(TEXT("scrollbar_pulgar_16x200")))
                .SetDraggedThumbImage(ChronosMenuArt::Premium(TEXT("scrollbar_pulgar_16x200")));
            auto Scroll = SNew(SScrollBox).ScrollBarStyle(&ScrollStyle).ScrollBarThickness(FVector2D(9, 9))
                .ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);
            const auto PresetRow = MakeRow(Rows[0]);
            for (int32 Index = 1; Index < UE_ARRAY_COUNT(Rows); ++Index) Scroll->AddSlot().Padding(0, 0, 7, 3)[MakeRow(Rows[Index])];

            auto Main = SNew(SVerticalBox)
                + SVerticalBox::Slot().FillHeight(1)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(.685f).Padding(0, 0, 15, 0)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 9)[Banner()]
                        + SVerticalBox::Slot().FillHeight(1)
                        [
                            SNew(SBorder).BorderImage(&ChronosMenuArt::Premium(TEXT("panel_ajustes_central_900x652"))).Padding(23, 15)
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 7, 11)[PresetRow]
                                + SVerticalBox::Slot().FillHeight(1)[Scroll]
                            ]
                        ]
                    ]
                    + SHorizontalBox::Slot().FillWidth(.315f)[RightColumn()]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(33, 22, 35, 0)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 15, 0)
                    [Button(LOCTEXT("Reset", "Restablecer gráficos"), TEXT("restaurar"), TEXT("boton_principal_388x59"), FOnClicked::CreateSP(this, &SChronosGraphicsPanel::Reset), &FooterActions[0])]
                    + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 15, 0)
                    [Button(LOCTEXT("Confirm", "Confirmar cambios"), TEXT("confirmar"), TEXT("boton_principal_388x59"), FOnClicked::CreateSP(this, &SChronosGraphicsPanel::Confirm), &FooterActions[1], true)]
                    + SHorizontalBox::Slot().FillWidth(.92f)
                    [Button(LOCTEXT("Cancel", "Cancelar"), TEXT("cancelar"), TEXT("boton_principal_388x59"), FOnClicked::CreateSP(this, &SChronosGraphicsPanel::Cancel), &FooterActions[2])]
                ];

            ChildSlot
            [
                SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                [
                    SNew(SBox).WidthOverride(1515).HeightOverride(944)
                    [
                        SAssignNew(Screens, SWidgetSwitcher)
                        + SWidgetSwitcher::Slot()[Main]
                        + SWidgetSwitcher::Slot()[VideoConfirmation()]
                    ]
                ]
            ];
        }

        ~SChronosGraphicsPanel() override { RevertVideo(); }
        bool SupportsKeyboardFocus() const override { return true; }

        FReply OnFocusReceived(const FGeometry&, const FFocusEvent&) override
        {
            const auto& Target = Deadline > 0. ? RevertButton : Initial;
            return Target ? FReply::Handled().SetUserFocus(Target.ToSharedRef()) : FReply::Handled();
        }

        FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override
        {
            const FKey Key = Event.GetKey();
            if (Deadline > 0. && (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right))
                return RevertClick();
            if (Deadline <= 0. && (Key == EKeys::Up || Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Down))
                if (auto Target = VerticalFocus(Event.GetUserIndex(), Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up ? -1 : 1))
                    return FReply::Handled().SetUserFocus(Target.ToSharedRef(), EFocusCause::Navigation);
            if (Key == EKeys::Left || Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Right)
                if (const FRow* Row = FocusedRow(Event.GetUserIndex()))
                    return Change(Row->Setting, Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left ? -1 : 1);
            return SCompoundWidget::OnPreviewKeyDown(Geometry, Event);
        }

        FNavigationReply OnNavigation(const FGeometry& Geometry, const FNavigationEvent& Event) override
        {
            const EUINavigation Direction = Event.GetNavigationType();
            if (Deadline <= 0. && (Direction == EUINavigation::Up || Direction == EUINavigation::Down))
                if (auto Target = VerticalFocus(Event.GetUserIndex(), Direction == EUINavigation::Up ? -1 : 1))
                    return FNavigationReply::Explicit(Target);
            return SCompoundWidget::OnNavigation(Geometry, Event);
        }

        FReply OnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override
        {
            if (Event.GetKey() != EKeys::Gamepad_RightX)
                return SCompoundWidget::OnAnalogValueChanged(Geometry, Event);
            const float Axis = Event.GetAnalogValue();
            if (FMath::Abs(Axis) <= .35f) bRightStickHeld = false;
            if (const FRow* Row = FocusedRow(Event.GetUserIndex()))
            {
                if (!bRightStickHeld && FMath::Abs(Axis) >= .55f)
                {
                    bRightStickHeld = true;
                    Change(Row->Setting, Axis > 0.f ? 1 : -1);
                }
                return FReply::Handled();
            }
            return FReply::Unhandled();
        }

    private:
        TSharedRef<STextBlock> Text(TAttribute<FText> Value, int32 Size, FLinearColor Color = Ivory, float Wrap = 0.f)
        {
            return SNew(STextBlock).Text(MoveTemp(Value)).ColorAndOpacity(Color).WrapTextAt(Wrap)
                .Font_Lambda([this, Size]
                {
                    auto Scaled = Font;
                    Scaled.Size = Size;
                    return ChronosMenuArt::ScaleFont(Scaled, MenuSettings.IsValid() ? MenuSettings->MenuScale() : 1.f);
                });
        }

        TSharedRef<SWidget> Button(TAttribute<FText> Label, const TCHAR* Symbol, const TCHAR* Art, FOnClicked Click,
                                  TSharedPtr<SWidget>* Focus = nullptr, bool bPrimary = false)
        {
            auto Control = SNew(SChronosMenuButton).ButtonStyle(&ChronosMenuArt::PremiumButton(Art, bPrimary))
                .OnClicked(MoveTemp(Click)).HAlign(HAlign_Fill).VAlign(VAlign_Center)
                [
                    SNew(SBox).Padding(32, 0)
                    [
                        SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)[Icon(Symbol, 25)]
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Text(MoveTemp(Label), 19)]
                        ]
                    ]
                ];
            if (Focus) *Focus = Control;
            return SNew(SBox).HeightOverride(56)[Control];
        }

        TSharedRef<SWidget> Banner()
        {
            return SNew(SBox).HeightOverride(93)
            [
                SNew(SBorder).BorderImage(&ChronosMenuArt::Premium(TEXT("panel_banner_auto_900x93"))).Padding(24, 12)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 14, 0)[Icon(TEXT("pantalla"), 40)]
                    + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("Automatic", "Configuración automática"), 24, Gold)]
                        + SVerticalBox::Slot().AutoHeight()[Text(TAttribute<FText>::CreateLambda([this]
                        {
                            return Pending.bManual ? LOCTEXT("AutomaticOff", "Desactivada · Ajustes manuales")
                                : LOCTEXT("AutomaticOn", "Activada · Editar la calidad activa el modo manual");
                        }), 15)]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5, 0, 0, 0)
                    [
                        SNew(SBox).WidthOverride(321)
                        [
                            Button(TAttribute<FText>::CreateLambda([this]
                            {
                                return Pending.bManual ? LOCTEXT("UseAutomatic", "Volver al ajuste automático")
                                    : LOCTEXT("Unlock", "Desbloquear ajuste manual");
                            }), TEXT("candado_abierto"), TEXT("boton_recomendacion_332x60"),
                                FOnClicked::CreateSP(this, &SChronosGraphicsPanel::ToggleManual), &Initial, true)
                        ]
                    ]
                ]
            ];
        }

        TSharedRef<SWidget> MakeRow(const FRow& Row)
        {
            const ESetting Setting = Row.Setting;
            const auto Arrow = [this, Setting](int32 Delta)
            {
                return SNew(SButton)
                    .ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("NoBorder")))
                    .IsEnabled_Lambda([this, Setting] { return Editable(Setting); })
                    .OnClicked(this, &SChronosGraphicsPanel::Change, Setting, Delta)
                    .HAlign(HAlign_Center).VAlign(VAlign_Center).ContentPadding(0)
                    [Icon(Delta < 0 ? TEXT("flecha_izquierda") : TEXT("flecha_derecha"), 19)];
            };
            auto Previous = Arrow(-1);
            auto Next = Arrow(1);
            Selectors.Add({&Row, Previous, Next});
            const auto Highlight = [Previous, Next]
            {
                return Previous->HasAnyUserFocus().IsSet() || Next->HasAnyUserFocus().IsSet() || Previous->IsHovered() || Next->IsHovered();
            };
            return SNew(SBox).HeightOverride(Setting == ESetting::Preset ? 61 : 48)
            [
                SNew(SBorder).Padding(12, 0)
                .BorderImage_Lambda([Highlight]
                {
                    return &ChronosMenuArt::Premium(Highlight() ? TEXT("fila_config_seleccionada") : TEXT("fila_config_normal"));
                })
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 18, 0)[Icon(Row.Icon, 30)]
                    + SHorizontalBox::Slot().FillWidth(.55f).VAlign(VAlign_Center)
                    [Text(FText::FromString(Row.Name), Setting == ESetting::Preset ? 24 : 20, Setting == ESetting::Preset ? Gold : Ivory)]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3, 0, 23, 0)
                    [Icon(TEXT("informacion"), 22)]
                    + SHorizontalBox::Slot().FillWidth(.45f).VAlign(VAlign_Center)
                    [
                        SNew(SBox).HeightOverride(39)
                        [
                            SNew(SBorder).Padding(12, 0)
                            .BorderImage_Lambda([this, Setting, Highlight]
                            {
                                const auto& Style = ChronosMenuArt::PremiumButton(Selector, Setting == ESetting::Preset && !Pending.bManual);
                                return !Editable(Setting) ? &Style.Disabled : Highlight() ? &Style.Hovered : &Style.Normal;
                            })
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(38)[Previous]]
                                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).HAlign(HAlign_Center)
                                [
                                    SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                                    [Text(TAttribute<FText>::CreateSP(this, &SChronosGraphicsPanel::Value, Setting), 18)]
                                ]
                                + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(38)[Next]]
                            ]
                        ]
                    ]
                ]
            ];
        }

        TSharedRef<SWidget> HardwareRow(const TCHAR* Symbol, const FText& Label, const FText& Value)
        {
            return SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)[Icon(Symbol, 24)]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 14, 0)[Text(Label, 16, Gold)]
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(Value, 16, Ivory, 304)];
        }

        TSharedRef<SWidget> RightColumn()
        {
            const auto& Hardware = ChronosGraphics::Hardware();
            const auto Available = [](const FString& Value) { return Value.IsEmpty() ? LOCTEXT("NA", "No disponible") : FText::FromString(Value); };
            const FString Display = Hardware.Display.X > 0 ? FString::Printf(TEXT("%d × %d"), Hardware.Display.X, Hardware.Display.Y) : FString();
            const FString Refresh = Hardware.Refresh > 0 ? FString::Printf(TEXT(" (%d Hz)"), Hardware.Refresh) : FString();
            const FString VRAM = Hardware.Dedicated ? FString::Printf(TEXT(" · %s"), *ChronosGraphics::MemoryAmount(Hardware.Dedicated).ToString()) : FString();
            return SNew(SVerticalBox)
                + SVerticalBox::Slot().FillHeight(.055f)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 13)
                [
                    SNew(SBorder).BorderImage(&ChronosMenuArt::Premium(TEXT("panel_hardware_424x207"))).Padding(23, 17)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[Text(LOCTEXT("Hardware", "Tu hardware"), 25, Gold)]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)[HardwareRow(TEXT("gpu"), INVTEXT("GPU"), Available(Hardware.GPU + VRAM))]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)[HardwareRow(TEXT("cpu"), INVTEXT("CPU"), Available(Hardware.CPU))]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)[HardwareRow(TEXT("ram"), INVTEXT("RAM"), ChronosGraphics::MemoryAmount(Hardware.RAM))]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)[HardwareRow(TEXT("pantalla"), LOCTEXT("Display", "Pantalla"), Available(Display + Refresh))]
                        + SVerticalBox::Slot().AutoHeight()[Text(Available(Hardware.RHI), 15, Muted)]
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 13)[MemoryPanel()]
                + SVerticalBox::Slot().FillHeight(1)
                [
                    SNew(SBorder).BorderImage(&ChronosMenuArt::Premium(TEXT("panel_recomendacion_424x272"))).Padding(23, 17)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 9)
                        [
                            StatusHeading()
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 9)
                        [
                            Text(TAttribute<FText>::CreateSP(this, &SChronosGraphicsPanel::StatusHelp), 17, Ivory, 395)
                        ]
                        + SVerticalBox::Slot().FillHeight(1)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            Button(LOCTEXT("Benchmark", "Recalcular recomendación"), TEXT("sistema"), TEXT("boton_recomendacion_332x60"),
                                FOnClicked::CreateSP(this, &SChronosGraphicsPanel::Recalculate))
                        ]
                    ]
                ];
        }

        TSharedRef<SWidget> StatusHeading()
        {
            auto Heading = Text(TAttribute<FText>::CreateSP(this, &SChronosGraphicsPanel::StatusTitle), 21, Gold, 395);
            Heading->SetColorAndOpacity(TAttribute<FSlateColor>::CreateLambda([this]
            {
                return FSlateColor(MemoryRatio() >= .90 || ChronosGraphics::LowVideoMemory() ? Red : Pending.bManual ? Gold : Green);
            }));
            return Heading;
        }

        double MemoryRatio() const
        {
            const auto& Memory = ChronosGraphics::VideoMemory();
            const auto Estimate = ChronosGraphics::EstimatedVideoMemory(Pending);
            return Memory.BudgetLocal ? double(Estimate.Get(Memory.UsedLocal)) / Memory.BudgetLocal : 0.0;
        }

        TSharedRef<SWidget> MemoryPanel()
        {
            return SNew(SBorder).BorderImage(&ChronosMenuArt::Premium(TEXT("panel_vram_424x194"))).Padding(23, 16)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[Text(LOCTEXT("Memory", "Uso de VRAM"), 24, Gold)]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
                [
                    Text(TAttribute<FText>::CreateLambda([]
                    {
                        const auto& Memory = ChronosGraphics::VideoMemory();
                        return Memory.BudgetLocal ? FText::Format(LOCTEXT("BudgetRatio", "Medido   {0} / {1}"),
                            ChronosGraphics::MemoryAmount(Memory.UsedLocal), ChronosGraphics::MemoryAmount(Memory.BudgetLocal)) : LOCTEXT("NoMemory", "Telemetría no disponible");
                    }), 18)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 7)
                [
                    SNew(SBox).WidthOverride(372).HeightOverride(32).HAlign(HAlign_Fill)
                    [
                        SNew(SOverlay)
                        + SOverlay::Slot()[SNew(SImage).Image(&ChronosMenuArt::Premium(TEXT("vram_track_372x32")))]
                        + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(16, 0, 0, 0)
                        [
                            SNew(SBox).WidthOverride_Lambda([this] { return float(334. * FMath::Clamp(MemoryRatio(), 0.0, 1.0)); }).HeightOverride(18)
                            .Visibility_Lambda([this] { return MemoryRatio() > 0. ? EVisibility::Visible : EVisibility::Collapsed; })
                            [
                                SNew(SImage).Image_Lambda([this]
                                {
                                    const double Ratio = MemoryRatio();
                                    return &ChronosMenuArt::Premium(Ratio >= .9 ? TEXT("vram_relleno_rojo_334x18")
                                        : Ratio >= .75 ? TEXT("vram_relleno_dorado_334x18") : TEXT("vram_relleno_verde_334x18"));
                                })
                            ]
                        ]
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    Text(TAttribute<FText>::CreateLambda([]
                    {
                        const auto& Memory = ChronosGraphics::VideoMemory();
                        return Memory.BudgetLocal ? FText::Format(LOCTEXT("FreeVRAM", "Disponible   {0}"),
                            ChronosGraphics::MemoryAmount(Memory.BudgetLocal > Memory.UsedLocal ? Memory.BudgetLocal - Memory.UsedLocal : 0))
                            : LOCTEXT("EstimateNA", "Estimación no disponible");
                    }), 16)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
                [
                    Text(TAttribute<FText>::CreateLambda([this]
                    {
                        const auto Estimate = ChronosGraphics::EstimatedVideoMemory(Pending);
                        return Estimate.IsSet() ? FText::Format(LOCTEXT("PendingEstimate", "Estimación de la configuración   {0}"),
                            ChronosGraphics::MemoryAmount(Estimate.GetValue()))
                            : LOCTEXT("PendingEstimateNA", "Estimación de la configuración: sin mediciones.");
                    }), 14, Muted, 374)
                ]
            ];
        }

        TSharedRef<SWidget> VideoConfirmation()
        {
            return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(FLinearColor(0, 0, 0, .8f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
                [
                    SNew(SBox).WidthOverride(620)
                    [
                        SNew(SBorder).BorderImage(&ChronosMenuArt::Premium(TEXT("panel_confirmacion_606x212"))).Padding(31, 24)
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Text(LOCTEXT("KeepVideo", "¿Conservar la pantalla?"), 23, Gold)]
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 12, 0, 20)
                            [
                                Text(TAttribute<FText>::CreateLambda([this]
                                {
                                    return FText::Format(LOCTEXT("Timer", "La pantalla se restaurará en {0} s."),
                                        FText::AsNumber(FMath::Max(0, FMath::CeilToInt(Deadline - FPlatformTime::Seconds()))));
                                }), 19)
                            ]
                            + SVerticalBox::Slot().AutoHeight()
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 10, 0)
                                [Button(LOCTEXT("Keep", "Mantener"), TEXT("confirmar"), TEXT("boton_secundario_276x62"), FOnClicked::CreateSP(this, &SChronosGraphicsPanel::Keep))]
                                + SHorizontalBox::Slot().FillWidth(1)
                                [Button(LOCTEXT("Revert", "Revertir"), TEXT("cancelar"), TEXT("boton_secundario_276x62"), FOnClicked::CreateSP(this, &SChronosGraphicsPanel::RevertClick), &RevertButton)]
                            ]
                        ]
                    ]
                ];
        }

        const FRow* FocusedRow(int32 User) const
        {
            const int32 Index = FocusedSelector(User);
            return Selectors.IsValidIndex(Index) ? Selectors[Index].Row : nullptr;
        }

        int32 FocusedSelector(int32 User) const
        {
            return Selectors.IndexOfByPredicate([User](const FSelector& Item)
            {
                return Item.Previous->HasUserFocus(User).IsSet() || Item.Next->HasUserFocus(User).IsSet();
            });
        }

        TSharedPtr<SWidget> VerticalFocus(int32 User, int32 Delta) const
        {
            const int32 Index = FocusedSelector(User);
            const bool bOnFooter = Algo::AnyOf(FooterActions, [User](const auto& Action)
            {
                return Action && Action->HasUserFocus(User).IsSet();
            });
            const bool bOnBanner = Initial && Initial->HasUserFocus(User).IsSet();
            if (Index == INDEX_NONE && !bOnFooter && !bOnBanner) return {};
            const int32 Start = bOnBanner ? -1 : bOnFooter ? Selectors.Num() : Index;
            for (int32 Next = Start + Delta; Selectors.IsValidIndex(Next); Next += Delta)
                if (Editable(Selectors[Next].Row->Setting)) return Selectors[Next].Previous;
            return Delta > 0 ? FooterActions[1] : Initial;
        }

        bool Editable(ESetting Setting) const
        {
            if (Setting == ESetting::RayTracing) return ChronosGraphics::Hardware().bRayTracing;
            if (Setting == ESetting::Mode || Setting == ESetting::Resolution)
                if (const auto* Viewport = GEngine->GameViewport ? GEngine->GameViewport->Viewport : nullptr;
                    Viewport && Viewport->IsPlayInEditorViewport()) return false;
            if (Setting == ESetting::Resolution) return Pending.Mode != EWindowMode::WindowedFullscreen;
            return true;
        }

        FText StatusTitle() const
        {
            if (MemoryRatio() >= .90 || ChronosGraphics::LowVideoMemory()) return LOCTEXT("LowMemoryTitle", "Memoria gráfica insuficiente");
            return Pending.bManual ? LOCTEXT("ManualTitle", "Configuración personalizada") : LOCTEXT("RecommendedTitle", "Configuración recomendada");
        }

        FText StatusHelp() const
        {
            if (!Notice.IsEmpty()) return Notice;
            if (MemoryRatio() >= .90 || ChronosGraphics::LowVideoMemory())
                return LOCTEXT("LowMemoryHelp", "VRAM BAJA. El uso de vídeo se acerca al presupuesto. Pueden aparecer tirones y retrasos al cargar texturas.");
            if (FSlateApplication::IsInitialized())
            {
                for (const auto& Item : Selectors)
                    if (Item.Previous->IsHovered() || Item.Next->IsHovered()) return FText::FromString(Item.Row->Description);
                for (int32 User = 0; User < 4; ++User)
                    if (const FRow* Row = FocusedRow(User)) return FText::FromString(Row->Description);
            }
            if (!ChronosGraphics::Hardware().bRayTracing)
                return LOCTEXT("UnsupportedHelp", "Trazado de rayos por hardware no compatible con el renderizador activo.");
            return Pending.bManual ? LOCTEXT("ManualHelp", "Los ajustes personalizados pueden aumentar el consumo de recursos.")
                : LOCTEXT("RecommendedHelp", "El perfil automático se adapta a la resolución y al rendimiento del equipo.");
        }

        FText Value(ESetting Setting) const
        {
            const auto& Q = Pending.Quality;
            switch (Setting)
            {
                case ESetting::Preset:
                    return Pending.bManual ? QualityName(Q.GetSingleQualityLevel())
                        : FText::Format(LOCTEXT("RecommendedPreset", "{0} (recomendado)"), QualityName(Q.ViewDistanceQuality));
                case ESetting::Mode:
                    if (Pending.Mode == EWindowMode::Fullscreen) return LOCTEXT("Fullscreen", "Pantalla completa");
                    return Pending.Mode == EWindowMode::WindowedFullscreen ? LOCTEXT("Borderless", "Sin bordes") : LOCTEXT("Window", "Ventana");
                case ESetting::Resolution:
                {
                    const FIntPoint Size = Pending.Mode == EWindowMode::WindowedFullscreen && ChronosGraphics::Hardware().Display.X > 0
                        ? ChronosGraphics::Hardware().Display : Pending.Resolution;
                    return FText::FromString(FString::Printf(TEXT("%d × %d"), Size.X, Size.Y));
                }
                case ESetting::VSync: return Pending.bVSync ? LOCTEXT("On", "Activado") : LOCTEXT("Off", "Desactivado");
                case ESetting::FPS:
                    if (Pending.FPS == ChronosGraphics::AutomaticFPS) return LOCTEXT("AutoFPS", "Automático");
                    return Pending.FPS > 0 ? FText::AsNumber(Pending.FPS) : LOCTEXT("Unlimited", "Sin límites");
                case ESetting::Quality: return QualityName(Q.GlobalIlluminationQuality);
                case ESetting::Textures: return QualityName(Q.TextureQuality);
                case ESetting::Shadows: return QualityName(Q.ShadowQuality);
                case ESetting::Effects: return QualityName(Q.EffectsQuality);
                case ESetting::Post: return QualityName(Q.PostProcessQuality);
                case ESetting::Reflections: return QualityName(Q.ReflectionQuality);
                case ESetting::RayTracing:
                    if (!ChronosGraphics::Hardware().bRayTracing) return LOCTEXT("Unsupported", "No compatible");
                    return Pending.bRayTracing ? LOCTEXT("On", "Activado") : LOCTEXT("Off", "Desactivado");
                case ESetting::Distance: return QualityName(Q.ViewDistanceQuality);
            }
            return {};
        }

        void StepResolution(int32 Delta)
        {
            TArray<FIntPoint> Resolutions;
            if (Pending.Mode == EWindowMode::Windowed) UKismetSystemLibrary::GetConvenientWindowedResolutions(Resolutions);
            else UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
            Resolutions.AddUnique(Pending.Resolution);
            Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
                { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
            Pending.Resolution = Resolutions[FMath::Clamp(Resolutions.Find(Pending.Resolution) + Delta, 0, Resolutions.Num() - 1)];
        }

        FReply Change(ESetting Setting, int32 Delta)
        {
            if (!Editable(Setting)) return FReply::Handled();
            Notice = {};
            auto& Q = Pending.Quality;
            if (Setting == ESetting::Preset || Setting > ESetting::FPS) Pending.bManual = true;
            const auto Step = [Delta](int32 Level) { return FMath::Clamp(FMath::Max(0, Level) + Delta, 0, 4); };
            switch (Setting)
            {
                case ESetting::Preset: Q.SetFromSingleQualityLevel(Step(Q.GetSingleQualityLevel() < 0 ? Q.ViewDistanceQuality : Q.GetSingleQualityLevel())); break;
                case ESetting::Mode: Pending.Mode = static_cast<EWindowMode::Type>(FMath::Clamp(int32(Pending.Mode) + Delta, 0, 2)); break;
                case ESetting::Resolution: StepResolution(Delta); break;
                case ESetting::VSync: Pending.bVSync = !Pending.bVSync; break;
                case ESetting::FPS:
                {
                    const int32 Index = FMath::Max(0, int32(Algo::IndexOf(FrameLimits, Pending.FPS)));
                    Pending.FPS = FrameLimits[FMath::Clamp(Index + Delta, 0, int32(UE_ARRAY_COUNT(FrameLimits)) - 1)];
                    break;
                }
                case ESetting::Quality: Q.GlobalIlluminationQuality = Step(Q.GlobalIlluminationQuality); break;
                case ESetting::Textures: Q.TextureQuality = Step(Q.TextureQuality); break;
                case ESetting::Shadows: Q.ShadowQuality = Step(Q.ShadowQuality); break;
                case ESetting::Effects: Q.EffectsQuality = Step(Q.EffectsQuality); break;
                case ESetting::Post: Q.PostProcessQuality = Step(Q.PostProcessQuality); break;
                case ESetting::Reflections: Q.ReflectionQuality = Step(Q.ReflectionQuality); break;
                case ESetting::RayTracing: Pending.bRayTracing = !Pending.bRayTracing; break;
                case ESetting::Distance: Q.ViewDistanceQuality = Step(Q.ViewDistanceQuality); break;
            }
            UE_LOG(LogTemp, Verbose, TEXT("Chronos graphics pending %d: %s"), int32(Setting), *Value(Setting).ToString());
            return FReply::Handled();
        }

        FReply ToggleManual()
        {
            Notice = {};
            if (Pending.bManual) StageRecommendation(false);
            else Pending.bManual = true;
            return FReply::Handled();
        }

        void StageRecommendation(bool bRecalculate)
        {
            const auto Profile = ChronosGraphics::Recommended(bRecalculate);
            Pending.Quality = Profile.Quality;
            Pending.FPS = Profile.FPS;
            Pending.bRayTracing = Profile.bRayTracing;
            Pending.bManual = false;
        }

        FReply Reset()
        {
            Pending = Saved;
            StageRecommendation(false);
            Notice = LOCTEXT("ResetPending", "Perfil recomendado listo. Confirma los cambios para guardarlo.");
            return FReply::Handled().SetUserFocus(Initial.ToSharedRef());
        }

        FReply Recalculate()
        {
            StageRecommendation(true);
            Notice = LOCTEXT("BenchmarkDone", "Perfil recalculado. Confirma los cambios para aplicarlo.");
            return FReply::Handled();
        }

        FReply Confirm()
        {
            auto* Settings = GEngine->GetGameUserSettings();
            if (!Settings) return FReply::Handled();
            if (Pending.Mode != Saved.Mode || Pending.Resolution != Saved.Resolution)
            {
                const auto Window = GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
                if (!Window || !GEngine->GameViewport->Viewport || GEngine->GameViewport->Viewport->IsPlayInEditorViewport())
                {
                    Pending.Mode = Saved.Mode;
                    Pending.Resolution = Saved.Resolution;
                    Commit(*Settings);
                    Notice = LOCTEXT("PlayInEditor", "Calidad guardada. Cambia el modo de pantalla desde el juego independiente.");
                    return FReply::Handled();
                }
                Pending.ApplyVideo(*Settings);
                Deadline = FPlatformTime::Seconds() + 15.;
                Screens->SetActiveWidgetIndex(1);
                RegisterActiveTimer(.1f, FWidgetActiveTimerDelegate::CreateSP(this, &SChronosGraphicsPanel::CheckDeadline));
                return FReply::Handled().SetUserFocus(RevertButton.ToSharedRef());
            }
            Commit(*Settings);
            return FReply::Handled();
        }

        void Commit(UGameUserSettings& Settings)
        {
            const bool bRequestedRayTracing = Pending.bRayTracing;
            Pending.ApplyQuality(Settings);
            ChronosGraphics::FValues Effective;
            Effective.Read(Settings);
            Pending.bRayTracing = Effective.bRayTracing;
            Settings.ConfirmVideoMode();
            Settings.SaveSettings();
            ChronosGraphics::SaveChoices(Pending);
            Saved.Read(Settings);
            Pending = Saved;
            Notice = ChronosGraphics::Hardware().bRayTracing && bRequestedRayTracing != Saved.bRayTracing
                ? LOCTEXT("RTFailed", "El renderizador no confirmó el cambio de trazado de rayos.")
                : LOCTEXT("Applied", "Cambios aplicados y guardados.");
        }

        FReply Keep()
        {
            if (Deadline <= 0.) return FReply::Handled();
            auto* Settings = GEngine->GetGameUserSettings();
            const auto Window = GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
            const auto Viewport = GEngine->GameViewport ? GEngine->GameViewport->Viewport : nullptr;
            if (!Settings || !Window || !Viewport || Window->GetWindowMode() != Pending.Mode
                || (Pending.Mode != EWindowMode::WindowedFullscreen && Viewport->GetSizeXY() != Pending.Resolution))
            {
                RevertVideo();
                Notice = LOCTEXT("VideoFailed", "La pantalla no aceptó el modo o la resolución elegida.");
            }
            else
            {
                Deadline = 0.;
                Commit(*Settings);
            }
            Screens->SetActiveWidgetIndex(0);
            return FReply::Handled().SetUserFocus(Initial.ToSharedRef());
        }

        void RevertVideo()
        {
            if (Deadline <= 0.) return;
            Deadline = 0.;
            if (auto* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
            {
                Settings->RevertVideoMode();
                Saved.ApplyVideo(*Settings);
            }
            Pending.Mode = Saved.Mode;
            Pending.Resolution = Saved.Resolution;
        }

        FReply RevertClick()
        {
            RevertVideo();
            Screens->SetActiveWidgetIndex(0);
            Notice = LOCTEXT("VideoRestored", "Modo de pantalla anterior restaurado.");
            return FReply::Handled().SetUserFocus(Initial.ToSharedRef());
        }

        EActiveTimerReturnType CheckDeadline(double, float)
        {
            if (Deadline <= 0.) return EActiveTimerReturnType::Stop;
            if (FPlatformTime::Seconds() < Deadline) return EActiveTimerReturnType::Continue;
            const bool bHadFocus = HasFocusedDescendants();
            RevertClick();
            if (FSlateApplication::IsInitialized() && Initial && bHadFocus)
                FSlateApplication::Get().SetAllUserFocus(Initial, EFocusCause::SetDirectly);
            return EActiveTimerReturnType::Stop;
        }

        FReply Cancel()
        {
            Pending = Saved;
            Notice = {};
            return FReply::Handled();
        }

        FSlateFontInfo Font;
        TWeakObjectPtr<UChronosMenuSettings> MenuSettings;
        TArray<FSelector> Selectors;
        TSharedPtr<SWidget> Initial;
        TSharedPtr<SWidget> FooterActions[3];
        TSharedPtr<SWidget> RevertButton;
        TSharedPtr<SWidgetSwitcher> Screens;
        ChronosGraphics::FValues Saved;
        ChronosGraphics::FValues Pending;
        FText Notice;
        double Deadline = 0.;
        bool bRightStickHeld = false;
    };
}

TSharedRef<SWidget> MakeChronosGraphicsPanel(const FSlateFontInfo& Font, UChronosMenuSettings* Settings)
{
    return SNew(SChronosGraphicsPanel).Font(Font).MenuSettings(Settings);
}

#undef LOCTEXT_NAMESPACE

