#include "ChronosMenuSettings.h"

#include "ChronosHUDLayer.h"
#include "ChronosMenuArt.h"
#include "ChronosSaveMenus.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/AudioComponent.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/NativeWidgetHost.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChronosMenuSettings"

namespace
{
    constexpr TCHAR OptionsSlot[] = TEXT("Chronos_Opciones");
    constexpr TCHAR OptionsClass[] = TEXT("/Game/Guardado/SG_Opciones.SG_Opciones_C");
    constexpr TCHAR SongPath[] = TEXT("/Game/TheChronosCurse_HUD_Assets/PrincipalSong.PrincipalSong");
    const FLinearColor Gold(.73f, .50f, .26f);
    const FLinearColor Ivory(.88f, .76f, .59f);

    int32 ClampScale(int32 Percentage)
    {
        return FMath::Clamp(FMath::RoundToInt(Percentage / 10.f) * 10, 70, 150);
    }

    // YukoDev — HUD Y ACCESIBILIDAD
    class SChronosHUDSettings final : public SCompoundWidget
    {
        using FScaleField = int32 FChronosHUDScales::*;

    public:
        SLATE_BEGIN_ARGS(SChronosHUDSettings) {}
            SLATE_ARGUMENT(TWeakObjectPtr<UChronosMenuSettings>, Settings)
            SLATE_ARGUMENT(TWeakObjectPtr<UChronosHUDLayer>, Sample)
            SLATE_ARGUMENT(FSlateFontInfo, Font)
            SLATE_EVENT(FSimpleDelegate, Close)
        SLATE_END_ARGS()

        void Construct(const FArguments& Args)
        {
            Settings = Args._Settings;
            Sample = Args._Sample;
            Close = Args._Close;
            Font = Args._Font;
            Font.Size = 19;
            if (Settings.IsValid()) Pending = Settings->Saved();
            TSharedRef<SVerticalBox> Form = SNew(SVerticalBox);
            ChildSlot
            [
                SNew(SBorder).BorderImage(&ChronosMenuArt::OptionsPanel()).Padding(FMargin(28, 24))
                [Form]
            ];

            Form->AddSlot().AutoHeight().Padding(0, 0, 0, 12)
            [SNew(STextBlock).Text(LOCTEXT("Title", "Interfaz y HUD")).Font(Font).ColorAndOpacity(Ivory)];
            Form->AddSlot().AutoHeight().Padding(0, 0, 0, 10)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
                [Preset(LOCTEXT("Compact", "Compacto"), 80)]
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
                [Preset(LOCTEXT("Normal", "Normal"), 100)]
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
                [Preset(LOCTEXT("Large", "Grande"), 130)]
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                [SNew(STextBlock).Text(this, &SChronosHUDSettings::PresetName).Font(Font).ColorAndOpacity(Gold)]
            ];
            for (const auto& Entry : {
                TPair<FText, FScaleField>{LOCTEXT("Life", "Vida"), &FChronosHUDScales::Life},
                {LOCTEXT("Stamina", "Resistencia"), &FChronosHUDScales::Stamina},
                {LOCTEXT("Lamp", "Linterna"), &FChronosHUDScales::Lamp},
                {LOCTEXT("Map", "Minimapa"), &FChronosHUDScales::Map},
                {LOCTEXT("Calendar", "Calendario"), &FChronosHUDScales::Calendar}})
            {
                const FScaleField Field = Entry.Value;
                Form->AddSlot().AutoHeight().Padding(0, 4)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(.37f).VAlign(VAlign_Center)
                    [SNew(STextBlock).Text(Entry.Key).Font(Font).ColorAndOpacity(Ivory)]
                    + SHorizontalBox::Slot().FillWidth(.48f).VAlign(VAlign_Center).Padding(0, 0, 14, 0)
                    [
                        SNew(SSlider).MinValue(70).MaxValue(150).StepSize(10).MouseUsesStep(true)
                        .Style(&SliderStyle())
                        .Value_Lambda([this, Field] { return static_cast<float>(Pending.*Field); })
                        .OnValueChanged_Lambda([this, Field](float Value)
                        {
                            Pending.*Field = ClampScale(FMath::RoundToInt(Value));
                            ApplyPreview();
                        })
                    ]
                    + SHorizontalBox::Slot().FillWidth(.15f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock).Font(Font).ColorAndOpacity(Ivory)
                        .Text_Lambda([this, Field] { return FText::AsPercent((Pending.*Field) / 100.); })
                    ]
                ];
            }
            Form->AddSlot().AutoHeight().Padding(0, 12, 0, 8)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 8, 0)
                [Action(LOCTEXT("Reset", "Restablecer interfaz"), FOnClicked::CreateSP(this, &SChronosHUDSettings::SetPreset, 100))]
                + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 8, 0)
                [Action(LOCTEXT("Confirm", "Confirmar cambios"), FOnClicked::CreateSP(this, &SChronosHUDSettings::Confirm))]
                + SHorizontalBox::Slot().FillWidth(1)
                [Action(LOCTEXT("Cancel", "Cancelar"), FOnClicked::CreateSP(this, &SChronosHUDSettings::Cancel))]
            ];
            Form->AddSlot().AutoHeight()
            [SNew(STextBlock).Text_Lambda([this] { return Status; }).Font(Font).ColorAndOpacity(Ivory)];
            ApplyPreview();
        }

    private:
        TSharedRef<SWidget> Action(const FText& Label, FOnClicked Click)
        {
            FSlateFontInfo Small = Font;
            Small.Size = 15;
            return SNew(SBox).MinDesiredHeight(38)
                [SNew(SButton).ButtonStyle(&ChronosMenuArt::ButtonStyle()).HAlign(HAlign_Center).VAlign(VAlign_Center)
                    .OnClicked(MoveTemp(Click))
                    [SNew(STextBlock).Text(Label).Font(Small).ColorAndOpacity(Ivory)]];
        }

        TSharedRef<SWidget> Preset(const FText& Label, int32 Size)
        {
            TSharedPtr<SButton> Button;
            auto Widget = SNew(SBox).MinDesiredWidth(116).MinDesiredHeight(36)
                [SAssignNew(Button, SButton).HAlign(HAlign_Center).VAlign(VAlign_Center)
                    .OnClicked(this, &SChronosHUDSettings::SetPreset, Size)
                    [SNew(STextBlock).Text(Label).Font(Font).ColorAndOpacity(Ivory)]];
            PresetButtons.Emplace(Size, Button.ToSharedRef());
            return Widget;
        }

        static const FSliderStyle& SliderStyle()
        {
            static const FSliderStyle Style = FSliderStyle()
                .SetNormalBarImage(FSlateRoundedBoxBrush(FLinearColor(.47f, .31f, .16f), 2.f, FVector2f(5, 5)))
                .SetNormalThumbImage(FSlateRoundedBoxBrush(Ivory, 3.f, FVector2f(13, 18)))
                .SetHoveredThumbImage(FSlateRoundedBoxBrush(FLinearColor(1.f, .82f, .51f), 3.f, FVector2f(13, 18)))
                .SetBarThickness(5.f);
            return Style;
        }

        void ApplyPreview()
        {
            Status = FText::GetEmpty();
            if (Settings.IsValid()) Settings->Preview(Pending);
            if (Sample.IsValid()) Sample->SetScales(Pending);
            for (const auto& Entry : PresetButtons)
            {
                const int32 Size = Entry.Key;
                Entry.Value->SetButtonStyle(&ChronosMenuArt::ButtonStyle(Pending == FChronosHUDScales{Size, Size, Size, Size, Size}));
            }
        }

        FReply SetPreset(int32 Size)
        {
            Pending = {Size, Size, Size, Size, Size};
            ApplyPreview();
            return FReply::Handled();
        }

        FText PresetName() const
        {
            if (Pending == FChronosHUDScales{80, 80, 80, 80, 80}) return LOCTEXT("ActiveCompact", "Compacto");
            if (Pending == FChronosHUDScales{}) return LOCTEXT("ActiveNormal", "Normal");
            if (Pending == FChronosHUDScales{130, 130, 130, 130, 130}) return LOCTEXT("ActiveLarge", "Grande");
            return LOCTEXT("Custom", "Personalizado");
        }

        FReply Confirm()
        {
            Status = Settings.IsValid() && Settings->Commit(Pending)
                ? LOCTEXT("Saved", "Interfaz guardada.") : LOCTEXT("SaveError", "No se pudo guardar la interfaz.");
            return FReply::Handled();
        }

        FReply Cancel()
        {
            Close.ExecuteIfBound();
            return FReply::Handled();
        }

        TWeakObjectPtr<UChronosMenuSettings> Settings;
        TWeakObjectPtr<UChronosHUDLayer> Sample;
        FSimpleDelegate Close;
        FChronosHUDScales Pending;
        FSlateFontInfo Font;
        FText Status;
        TArray<TPair<int32, TSharedRef<SButton>>> PresetButtons;
    };
}

USaveGame* UChronosOptionsSave::LoadOptionsForWrite(TSubclassOf<USaveGame> SaveClass)
{
    if (!SaveClass) return nullptr;
    if (auto* Existing = UGameplayStatics::LoadGameFromSlot(OptionsSlot, 0); Existing && Existing->IsA(SaveClass))
        return Existing;
    return UGameplayStatics::CreateSaveGameObject(SaveClass);
}

void UChronosMenuSettings::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (const auto* Save = Cast<UChronosOptionsSave>(UGameplayStatics::LoadGameFromSlot(OptionsSlot, 0)))
    {
        Confirmed = {ClampScale(Save->HUDLife), ClampScale(Save->HUDStamina), ClampScale(Save->HUDLamp),
            ClampScale(Save->HUDMap), ClampScale(Save->HUDCalendar)};
        MenuMusicPercent = FMath::Clamp(Save->MenuMusicPercent, 0, 100);
    }
    MusicTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UChronosMenuSettings::UpdateMenuMusic), .1f);
}

void UChronosMenuSettings::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(MusicTicker);
    if (MenuMusic)
    {
        MenuMusic->OnAudioFinishedNative.RemoveAll(this);
        MenuMusic->DestroyComponent();
        MenuMusic = nullptr;
    }
    Super::Deinitialize();
}

UChronosMenuSettings* UChronosMenuSettings::Get(const UObject* WorldContext)
{
    UGameInstance* Instance = UGameplayStatics::GetGameInstance(WorldContext);
    return Instance ? Instance->GetSubsystem<UChronosMenuSettings>() : nullptr;
}

void UChronosMenuSettings::AttachHUD(UChronosHUDLayer* HUD)
{
    LiveHUD = HUD;
    Revert();
}

void UChronosMenuSettings::Preview(const FChronosHUDScales& Scales)
{
    if (LiveHUD.IsValid()) LiveHUD->SetScales(Scales);
}

void UChronosMenuSettings::Revert()
{
    Preview(Confirmed);
}

bool UChronosMenuSettings::Commit(const FChronosHUDScales& Scales)
{
    const auto Class = LoadClass<UChronosOptionsSave>(nullptr, OptionsClass);
    auto* Save = Cast<UChronosOptionsSave>(UChronosOptionsSave::LoadOptionsForWrite(Class));
    if (!Save) return false;
    Save->HUDLife = Scales.Life;
    Save->HUDStamina = Scales.Stamina;
    Save->HUDLamp = Scales.Lamp;
    Save->HUDMap = Scales.Map;
    Save->HUDCalendar = Scales.Calendar;
    if (!UGameplayStatics::SaveGameToSlot(Save, OptionsSlot, 0)) return false;
    Confirmed = Scales;
    Preview(Confirmed);
    return true;
}

bool UChronosMenuSettings::SetMusicVolume(int32 Percentage)
{
    const auto Class = LoadClass<UChronosOptionsSave>(nullptr, OptionsClass);
    auto* Save = Cast<UChronosOptionsSave>(UChronosOptionsSave::LoadOptionsForWrite(Class));
    if (!Save) return false;
    Save->MenuMusicPercent = FMath::Clamp(Percentage, 0, 100);
    if (!UGameplayStatics::SaveGameToSlot(Save, OptionsSlot, 0)) return false;
    MenuMusicPercent = Save->MenuMusicPercent;
    if (MenuMusic) MenuMusic->SetVolumeMultiplier(MenuMusicPercent / 100.f);
    return true;
}

// YukoDev — AUDIO DE MENÚS
bool UChronosMenuSettings::UpdateMenuMusic(float)
{
    UWorld* World = GetGameInstance()->GetWorld();
    if (!World || !World->IsGameWorld() || !World->HasBegunPlay() || !World->GetFirstPlayerController()) return true;
    const bool bMainMenu = World->GetMapName().EndsWith(TEXT("MainMenu"), ESearchCase::IgnoreCase);
    bWantsMusic = bMainMenu || (World->IsPaused() && World->GetFirstPlayerController()->GetPawn());

    if (!bWantsMusic)
    {
        if (MenuMusic && !bFading && MenuMusic->IsPlaying())
        {
            bFading = true;
            MenuMusic->FadeOut(.55f, 0);
        }
        return true;
    }
    if (MenuMusic && !bFading && MenuMusic->IsPlaying()) return true;
    if (MenuMusic && bFading)
    {
        bFading = false;
        MenuMusic->AdjustVolume(.25f, 1.f);
        return true;
    }
    if (!MenuMusic)
    {
        auto* Song = LoadObject<USoundWave>(nullptr, SongPath);
        if (!Song) return true;
        MenuMusic = UGameplayStatics::CreateSound2D(World, Song, MenuMusicPercent / 100.f, 1.f, 0.f, nullptr, true, false);
        if (!MenuMusic) return true;
        MenuMusic->bIsUISound = true;
        MenuMusic->SoundClassOverride = LoadObject<USoundClass>(nullptr, TEXT("/Engine/EngineSounds/Master.Master"));
        MenuMusic->OnAudioFinishedNative.AddUObject(this, &UChronosMenuSettings::MusicFinished);
    }
    MenuMusic->FadeIn(.45f, 1.f);
    return true;
}

void UChronosMenuSettings::MusicFinished(UAudioComponent*)
{
    bFading = false;
}

void UChronosOptionsMenu::NativeConstruct()
{
    SetIsFocusable(true);
    Super::NativeConstruct();
    // YukoDev — NAVEGACIÓN DE OPCIONES
    auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("BTN-IDIOMA")));
    auto* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("TextBlock_2")));
    if (Button && Label)
    {
        Button->SetVisibility(ESlateVisibility::Visible);
        Label->SetText(LOCTEXT("HUDNavigation", "Interfaz y HUD"));
        Button->OnClicked.AddUniqueDynamic(this, &UChronosOptionsMenu::OpenHUDSettings);
    }
    AddMenuMusicRow();
    ThemeExistingPanel();
    if (auto* First = InitialFocus(); First && GetOwningPlayer()) First->SetUserFocus(GetOwningPlayer());
}

UWidget* UChronosOptionsMenu::InitialFocus() const
{
    const FName Preferred[] = {TEXT("BTN-GRAFICOS"), TEXT("BTN-volumen"), TEXT("BTN-Controles")};
    const int32 Section = GetWidgetFromName(TEXT("panelVolumen")) ? 1 : GetWidgetFromName(TEXT("panelControles")) ? 2 : 0;
    return GetWidgetFromName(Preferred[Section]);
}

FReply UChronosOptionsMenu::NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event)
{
    if (auto* First = InitialFocus())
        return FReply::Handled().SetUserFocus(First->TakeWidget(), EFocusCause::SetDirectly);
    return Super::NativeOnFocusReceived(Geometry, Event);
}

void UChronosOptionsMenu::NativeDestruct()
{
    CloseHUDSettings();
    RestorePauseFocus();
    Super::NativeDestruct();
}

void UChronosOptionsMenu::RestorePauseFocus()
{
    if (auto* Instance = GetGameInstance())
        if (const auto* Property = FindFProperty<FObjectPropertyBase>(Instance->GetClass(), TEXT("menuPausaActivo")))
            if (auto* Pause = Cast<UChronosPauseMenu>(Property->GetObjectPropertyValue_InContainer(Instance)); Pause && Pause->IsVisible())
            {
                if (!Pause->IsInViewport()) Pause->AddToViewport(90);
                Pause->FocusOptionsEntry();
            }
}

FReply UChronosOptionsMenu::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() != EKeys::Escape && Event.GetKey() != EKeys::Gamepad_FaceButton_Right)
        return Super::NativeOnKeyDown(Geometry, Event);
    if (!Event.IsRepeat())
    {
        if (HUDPanel)
        {
            CloseHUDSettings();
        }
        else if (auto* Back = FindFunction(TEXT("volver")))
        {
            ProcessEvent(Back, nullptr);
            RestorePauseFocus();
        }
    }
    return FReply::Handled();
}

void UChronosOptionsMenu::ThemeExistingPanel()
{
    const bool bGraphics = GetWidgetFromName(TEXT("BTN-apply")) != nullptr;
    const bool bVolume = GetWidgetFromName(TEXT("volGenMenos")) != nullptr;
    if (!bGraphics && !bVolume) return;

    if (auto* Frame = Cast<UImage>(GetWidgetFromName(bGraphics ? TEXT("Image_80") : TEXT("marcoPanel"))))
        Frame->SetBrush(ChronosMenuArt::OptionsPanel());

    const FName GraphicsRows[] = {TEXT("win-"), TEXT("res-"), TEXT("sync-"), TEXT("fps-"),
        TEXT("graf-"), TEXT("texture-"), TEXT("som-"), TEXT("efect-")};
    const FName VolumeRows[] = {TEXT("volGenMenos"), TEXT("volAmbMenos")};
    const TArrayView<const FName> Rows = bGraphics ? MakeArrayView(GraphicsRows) : MakeArrayView(VolumeRows);
    SelectorButtons.Reset();
    TArray<TPair<UButton*, UButton*>> Arrows;
    for (int32 Index = 0; Index < Rows.Num(); ++Index)
    {
        auto* Decrease = GetWidgetFromName(Rows[Index]);
        auto* Row = Decrease ? Cast<UHorizontalBox>(Decrease->GetParent()) : nullptr;
        if (Row) FormatSelector(*Row, Index, Arrows);
    }
    if (bVolume && MusicValue)
        if (auto* Row = Cast<UHorizontalBox>(MusicValue->GetParent()))
            FormatSelector(*Row, Rows.Num(), Arrows);

    auto* Section = InitialFocus();
    auto* Apply = Cast<UButton>(GetWidgetFromName(TEXT("BTN-apply")));
    if (Section && !Arrows.IsEmpty())
        Section->SetNavigationRuleExplicit(EUINavigation::Right, Arrows[0].Key);
    for (int32 Index = 0; Index < Arrows.Num(); ++Index)
    {
        auto [Decrease, Increase] = Arrows[Index];
        Decrease->SetNavigationRuleExplicit(EUINavigation::Right, Increase);
        Increase->SetNavigationRuleExplicit(EUINavigation::Left, Decrease);
        if (Section) Decrease->SetNavigationRuleExplicit(EUINavigation::Left, Section);
        if (Index > 0)
        {
            Decrease->SetNavigationRuleExplicit(EUINavigation::Up, Arrows[Index - 1].Key);
            Increase->SetNavigationRuleExplicit(EUINavigation::Up, Arrows[Index - 1].Value);
        }
        if (Index + 1 < Arrows.Num())
        {
            Decrease->SetNavigationRuleExplicit(EUINavigation::Down, Arrows[Index + 1].Key);
            Increase->SetNavigationRuleExplicit(EUINavigation::Down, Arrows[Index + 1].Value);
        }
        else if (Apply)
        {
            Decrease->SetNavigationRuleExplicit(EUINavigation::Down, Apply);
            Increase->SetNavigationRuleExplicit(EUINavigation::Down, Apply);
        }
    }
    if (Apply)
    {
        Apply->SetStyle(ChronosMenuArt::ConfirmStyle());
        Apply->SetToolTipText(LOCTEXT("ApplyGraphics", "Aplicar cambios gráficos"));
        if (auto* Label = Apply->GetContent()) Label->SetRenderOpacity(0.f);
        if (auto* LayoutSlot = Cast<UCanvasPanelSlot>(Apply->Slot))
        {
            LayoutSlot->SetAnchors(FAnchors(.5f, 0));
            LayoutSlot->SetAlignment(FVector2D(.5f, 0));
            LayoutSlot->SetPosition(FVector2D(0, 875));
            LayoutSlot->SetSize(FVector2D(350, 65));
        }
        if (!Arrows.IsEmpty()) Apply->SetNavigationRuleExplicit(EUINavigation::Up, Arrows.Last().Key);
    }
}

void UChronosOptionsMenu::FormatSelector(UHorizontalBox& Row, int32 Index, TArray<TPair<UButton*, UButton*>>& Arrows)
{
    auto* CanvasSlot = Cast<UCanvasPanelSlot>(Row.Slot);
    auto* Decrease = Cast<UButton>(Row.GetChildAt(1));
    auto* Increase = Cast<UButton>(Row.GetChildAt(3));
    if (!CanvasSlot || !Decrease || !Increase) return;

    CanvasSlot->SetAnchors(FAnchors(0, 0, 1, 0));
    CanvasSlot->SetOffsets(FMargin(32, 28 + Index * 100, 56, 66));
    const float ColumnWidths[] = {.46f, .075f, .39f, .075f};
    for (int32 Column = 0; Column < 4; ++Column)
    {
        auto* Child = Row.GetChildAt(Column);
        if (!Child) continue;
        if (auto* LayoutSlot = Cast<UHorizontalBoxSlot>(Child->Slot))
        {
            FSlateChildSize Width(ESlateSizeRule::Fill);
            Width.Value = ColumnWidths[Column];
            LayoutSlot->SetSize(Width);
            LayoutSlot->SetPadding(FMargin(0, 0, 0, 0));
            LayoutSlot->SetVerticalAlignment(Column % 2 ? VAlign_Fill : VAlign_Center);
            LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
        }
        if (auto* Label = Cast<UTextBlock>(Child); Label && Column == 2)
            Label->SetJustification(ETextJustify::Center);
    }
    for (UButton* Control : {Decrease, Increase})
    {
        Control->SetStyle(ChronosMenuArt::ButtonStyle());
        SelectorButtons.Add(Control);
        if (auto* Image = Cast<UImage>(Control->GetContent()))
        {
            Image->SetDesiredSizeOverride(FVector2D(32, 20));
            Image->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        if (auto* LayoutSlot = Cast<UButtonSlot>(Control->GetContent() ? Control->GetContent()->Slot : nullptr))
        {
            LayoutSlot->SetHorizontalAlignment(HAlign_Center);
            LayoutSlot->SetVerticalAlignment(VAlign_Center);
        }
    }
    Arrows.Emplace(Decrease, Increase);
}

void UChronosOptionsMenu::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    for (const auto& Entry : SelectorButtons)
        if (auto* Button = Entry.Get())
        {
            const auto& Style = ChronosMenuArt::ButtonStyle(Button->HasUserFocus(GetOwningPlayer()));
            if (Button->GetStyle().Normal.GetResourceObject() != Style.Normal.GetResourceObject())
                Button->SetStyle(Style);
        }
}

void UChronosOptionsMenu::AddMenuMusicRow()
{
    auto* Panel = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("panelVolumen")));
    auto* Model = UChronosMenuSettings::Get(this);
    const auto* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("txtAmbiente")));
    auto* LeftTemplate = Cast<UButton>(GetWidgetFromName(TEXT("volAmbMenos")));
    auto* RightTemplate = Cast<UButton>(GetWidgetFromName(TEXT("volAmbMas")));
    if (MusicValue || !Panel || !Label || !Model || !LeftTemplate || !RightTemplate) return;

    auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    const auto AddText = [&](const FText& Text)
    {
        auto* Value = WidgetTree->ConstructWidget<UTextBlock>();
        Value->SetFont(Label->GetFont());
        Value->SetColorAndOpacity(FSlateColor(Ivory));
        Value->SetText(Text);
        Row->AddChildToHorizontalBox(Value);
        return Value;
    };
    const auto AddArrow = [&](UButton& Template, const FText& Tooltip)
    {
        auto* Control = WidgetTree->ConstructWidget<UButton>();
        Control->SetToolTipText(Tooltip);
        if (auto* Image = Cast<UImage>(Template.GetContent()))
            Control->AddChild(DuplicateObject<UImage>(Image, Control));
        Row->AddChildToHorizontalBox(Control);
        return Control;
    };
    AddText(LOCTEXT("MenuMusic", "Música del menú"));
    AddArrow(*LeftTemplate, LOCTEXT("LowerMusic", "Bajar música del menú"))->OnClicked.AddDynamic(this, &UChronosOptionsMenu::LowerMenuMusic);
    MusicValue = AddText(FText::AsPercent(Model->MusicVolume() / 100.));
    AddArrow(*RightTemplate, LOCTEXT("RaiseMusic", "Subir música del menú"))->OnClicked.AddDynamic(this, &UChronosOptionsMenu::RaiseMenuMusic);
    Panel->AddChildToCanvas(Row);
}

void UChronosOptionsMenu::AdjustMenuMusic(int32 Delta)
{
    if (auto* Model = UChronosMenuSettings::Get(this); Model && MusicValue && Model->SetMusicVolume(Model->MusicVolume() + Delta))
        MusicValue->SetText(FText::AsPercent(Model->MusicVolume() / 100.));
}

void UChronosOptionsMenu::LowerMenuMusic() { AdjustMenuMusic(-5); }
void UChronosOptionsMenu::RaiseMenuMusic() { AdjustMenuMusic(5); }

void UChronosOptionsMenu::OpenHUDSettings()
{
    if (HUDPanel) return;
    auto* Settings = UChronosMenuSettings::Get(this);
    auto* Canvas = Cast<UCanvasPanel>(GetRootWidget());
    auto* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("TextBlock_2")));
    auto* HUDTab = Cast<UButton>(GetWidgetFromName(TEXT("BTN-IDIOMA")));
    if (!Canvas || !Label || !HUDTab || !Settings) return;

    auto* Column = HUDTab->GetParent();
    for (UWidget* Child : Canvas->GetAllChildren())
    {
        if (Child == Column || Child->GetFName() == TEXT("Image_54") || Child->GetFName() == TEXT("fondoPausa")
            || Child->GetFName() == TEXT("Image_86")) continue;
        SectionVisibility.Emplace(Child, Child->GetVisibility());
        Child->SetVisibility(ESlateVisibility::Collapsed);
    }
    for (FName Name : {FName(TEXT("BTN-GRAFICOS")), FName(TEXT("BTN-volumen")), FName(TEXT("BTN-Controles")), FName(TEXT("BTN-IDIOMA"))})
    {
        auto* Tab = Cast<UButton>(GetWidgetFromName(Name));
        if (!Tab) continue;
        FButtonStyle Style = Tab->GetStyle();
        NavigationStyles.Emplace(Tab, Style);
        Style.SetNormal(Tab == HUDTab ? Style.Hovered : FSlateNoResource());
        Tab->SetStyle(Style);
    }

    const bool bShowSample = !Settings->HasLiveHUD();
    if (bShowSample)
    {
        SampleHUD = CreateWidget<UChronosHUDLayer>(GetWorld(), UChronosHUDLayer::StaticClass());
        SampleHUD->InitializeArt();
        SampleHUD->SetVisibility(ESlateVisibility::HitTestInvisible);
        SampleHUD->Health = SampleHUD->Stamina = SampleHUD->Battery = SampleHUD->ShownHealth = 1;
        SampleHUD->HealthOpacity = SampleHUD->StaminaOpacity = SampleHUD->BatteryOpacity = 1;
        SampleHUD->Clock = TEXT("Lunes  15:45");
        SampleHUD->Day = TEXT("Día 1");
        auto* SampleSlot = Canvas->AddChildToCanvas(SampleHUD);
        SampleSlot->SetAnchors(FAnchors(.33f, .07f, .94f, .48f));
        SampleSlot->SetOffsets(FMargin(0));
        SampleSlot->SetZOrder(40);
    }
    auto* Host = WidgetTree->ConstructWidget<UNativeWidgetHost>();
    HUDPanel = Host;
    Host->SetContent(SNew(SChronosHUDSettings).Settings(Settings).Sample(SampleHUD.Get()).Font(Label->GetFont())
        .Close(FSimpleDelegate::CreateUObject(this, &UChronosOptionsMenu::CloseHUDSettings)));
    auto* PanelSlot = Canvas->AddChildToCanvas(Host);
    PanelSlot->SetAnchors(FAnchors(.35f, bShowSample ? .49f : .06f, .97f, .98f));
    PanelSlot->SetOffsets(FMargin(0));
    PanelSlot->SetZOrder(41);
}

void UChronosOptionsMenu::CloseHUDSettings()
{
    if (!HUDPanel) return;
    if (auto* Settings = UChronosMenuSettings::Get(this)) Settings->Revert();
    if (SampleHUD) SampleHUD->RemoveFromParent();
    if (HUDPanel) HUDPanel->RemoveFromParent();
    SampleHUD = nullptr;
    HUDPanel = nullptr;
    for (const auto& Entry : SectionVisibility)
        if (Entry.Key.IsValid()) Entry.Key->SetVisibility(Entry.Value);
    SectionVisibility.Reset();
    for (const auto& Entry : NavigationStyles)
        if (Entry.Key.IsValid()) Entry.Key->SetStyle(Entry.Value);
    NavigationStyles.Reset();
    if (auto* Tab = GetWidgetFromName(TEXT("BTN-IDIOMA")); Tab && GetOwningPlayer() && IsInViewport())
        Tab->SetUserFocus(GetOwningPlayer());
}

#undef LOCTEXT_NAMESPACE
