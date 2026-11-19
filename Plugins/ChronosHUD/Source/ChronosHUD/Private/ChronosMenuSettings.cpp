#include "ChronosMenuSettings.h"
#include "ChronosSaveMenus.h"

#include "ChronosHUDLayer.h"
#include "ChronosGamepad.h"
#include "Framework/Application/SlateApplication.h"
#include "ChronosMenuArt.h"
#include "ChronosOptionPanels.h"
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
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Image.h"
#include "Components/NativeWidgetHost.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/UnrealType.h"
#include "Sound/SoundWave.h"
#include "Types/NavigationMetaData.h"
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

    constexpr int32 DefaultMenuSize = 100;
    constexpr int32 MaxMenuSize = 130;
    constexpr int32 MenuSizeStep = 5;

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
            TArray<TSharedRef<SWidget>> Sliders;
            for (const auto& Entry : {
                TPair<FText, FScaleField>{LOCTEXT("Life", "Vida"), &FChronosHUDScales::Life},
                {LOCTEXT("Stamina", "Resistencia"), &FChronosHUDScales::Stamina},
                {LOCTEXT("Lamp", "Linterna"), &FChronosHUDScales::Lamp},
                {LOCTEXT("Map", "Minimapa"), &FChronosHUDScales::Map},
                {LOCTEXT("Calendar", "Calendario"), &FChronosHUDScales::Calendar}})
            {
                const FScaleField Field = Entry.Value;
                auto Slider = SNew(SChronosMenuSlider).MinValue(70).MaxValue(150).StepSize(10).MouseUsesStep(true)
                    .RequiresControllerLock(false).Style(&SliderStyle())
                    .Value_Lambda([this, Field] { return static_cast<float>(Pending.*Field); })
                    .OnValueChanged_Lambda([this, Field](float Value)
                    {
                        Pending.*Field = ClampScale(FMath::RoundToInt(Value));
                        ApplyPreview();
                    });
                Sliders.Add(Slider);
                Form->AddSlot().AutoHeight().Padding(0, 4)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(.37f).VAlign(VAlign_Center)
                    [SNew(STextBlock).Text(Entry.Key).Font(Font).ColorAndOpacity(Ivory)]
                    + SHorizontalBox::Slot().FillWidth(.48f).VAlign(VAlign_Center).Padding(0, 0, 14, 0)
                    [Slider]
                    + SHorizontalBox::Slot().FillWidth(.15f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock).Font(Font).ColorAndOpacity(Ivory)
                        .Text_Lambda([this, Field] { return FText::AsPercent((Pending.*Field) / 100.); })
                    ]
                ];
            }
            auto SizeButton = SNew(SChronosMenuSizeButton)
                .ButtonStyle(&ChronosMenuArt::ButtonStyle()).HAlign(HAlign_Center).VAlign(VAlign_Center)
                .OnClicked(this, &SChronosHUDSettings::CycleMenuSize)
                [
                    SNew(STextBlock).Font_Lambda([this] { return ChronosMenuArt::ScaleFont(Font, MenuScale()); })
                    .ColorAndOpacity(Ivory)
                    .Text_Lambda([this] { return FText::AsPercent(Pending.MenuButtons / 100.); })
                ];
            SizeButton->SetOnStep(SChronosMenuSizeButton::FOnStep::CreateSP(this, &SChronosHUDSettings::StepMenuSize));
            Sliders.Add(SizeButton);
            Form->AddSlot().AutoHeight().Padding(0, 4)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(.37f).VAlign(VAlign_Center)
                [SNew(STextBlock).Font(Font).Text(LOCTEXT("MenuButtonSize", "Tamaño de botones")).ColorAndOpacity(Ivory)]
                + SHorizontalBox::Slot().FillWidth(.48f).Padding(0, 0, 14, 0)
                [
                    SNew(SBox).MinDesiredHeight_Lambda([this] { return 38.f * MenuScale(); })
                    [SizeButton]
                ]
                + SHorizontalBox::Slot().FillWidth(.15f)
                [SNullWidget::NullWidget]
            ];
            TSharedPtr<SChronosMenuButton> ResetButton, ConfirmButton, CancelButton;
            Form->AddSlot().AutoHeight().Padding(0, 12, 0, 8)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 8, 0)
                [Action(ResetButton, LOCTEXT("Reset", "Restablecer interfaz"), FOnClicked::CreateSP(this, &SChronosHUDSettings::Reset))]
                + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 8, 0)
                [Action(ConfirmButton, LOCTEXT("Confirm", "Confirmar cambios"), FOnClicked::CreateSP(this, &SChronosHUDSettings::Confirm))]
                + SHorizontalBox::Slot().FillWidth(1)
                [Action(CancelButton, LOCTEXT("Cancel", "Cancelar"), FOnClicked::CreateSP(this, &SChronosHUDSettings::Cancel))]
            ];
            Form->AddSlot().AutoHeight()
            [SNew(STextBlock).Text_Lambda([this] { return Status; }).Font(Font).ColorAndOpacity(Ivory)];
            for (const auto& Entry : PresetButtons)
                Entry.Value->FindOrAddMetaData<FNavigationMetaData>()->SetNavigationExplicit(EUINavigation::Down, Sliders[0]);
            for (int32 Index = 0; Index < Sliders.Num(); ++Index)
            {
                const auto Navigation = Sliders[Index]->FindOrAddMetaData<FNavigationMetaData>();
                const TSharedRef<SWidget> Up = Index == 0 ? TSharedRef<SWidget>(PresetButtons[0].Value) : Sliders[Index - 1];
                const TSharedRef<SWidget> Down = Index + 1 < Sliders.Num() ? Sliders[Index + 1] : TSharedRef<SWidget>(ConfirmButton.ToSharedRef());
                Navigation->SetNavigationExplicit(EUINavigation::Up, Up);
                Navigation->SetNavigationExplicit(EUINavigation::Down, Down);
            }
            for (const auto& Button : {ResetButton, ConfirmButton, CancelButton})
                Button->FindOrAddMetaData<FNavigationMetaData>()->SetNavigationExplicit(EUINavigation::Up, Sliders.Last());
            ApplyPreview();
        }

        bool SupportsKeyboardFocus() const override { return true; }

        FReply OnFocusReceived(const FGeometry&, const FFocusEvent&) override
        {
            return FReply::Handled().SetUserFocus(PresetButtons[0].Value);
        }

    private:
        TSharedRef<SWidget> Action(TSharedPtr<SChronosMenuButton>& Button, const FText& Label, FOnClicked Click)
        {
            FSlateFontInfo Small = Font;
            Small.Size = 15;
            return SNew(SBox).MinDesiredHeight_Lambda([this] { return 38.f * MenuScale(); })
                [SAssignNew(Button, SChronosMenuButton).ButtonStyle(&ChronosMenuArt::ButtonStyle()).HAlign(HAlign_Center).VAlign(VAlign_Center)
                    .OnClicked(MoveTemp(Click))
                    [SNew(STextBlock).Text(Label).Font_Lambda([this, Small] { return ChronosMenuArt::ScaleFont(Small, MenuScale()); }).ColorAndOpacity(Ivory)]];
        }

        TSharedRef<SWidget> Preset(const FText& Label, int32 Size)
        {
            TSharedPtr<SChronosMenuButton> Button;
            auto Widget = SNew(SBox)
                .MinDesiredWidth_Lambda([this] { return 116.f * MenuScale(); })
                .MinDesiredHeight_Lambda([this] { return 36.f * MenuScale(); })
                [SAssignNew(Button, SChronosMenuButton).ButtonStyle(&ChronosMenuArt::ButtonStyle()).HAlign(HAlign_Center).VAlign(VAlign_Center)
                    .OnClicked(this, &SChronosHUDSettings::SetPreset, Size)
                    [SNew(STextBlock).Text(Label).Font_Lambda([this] { return ChronosMenuArt::ScaleFont(Font, MenuScale()); }).ColorAndOpacity(Ivory)]];
            PresetButtons.Emplace(Size, Button.ToSharedRef());
            return Widget;
        }

        static const FSliderStyle& SliderStyle()
        {
            static const FSliderStyle Style = FSliderStyle()
                .SetNormalBarImage(FSlateRoundedBoxBrush(FLinearColor(.47f, .31f, .16f), 2.f, FVector2f(5, 5)))
                .SetHoveredBarImage(FSlateRoundedBoxBrush(FLinearColor(.76f, .55f, .32f), 2.f, FVector2f(5, 5)))
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
                Entry.Value->SetMenuStyle(ChronosMenuArt::ButtonStyle(IsPreset(Size)));
            }
        }

        float MenuScale() const { return Pending.MenuButtons / 100.f; }

        bool IsPreset(int32 Size) const
        {
            return Pending == FChronosHUDScales{Size, Size, Size, Size, Size, Pending.MenuButtons};
        }

        void StepMenuSize(int32 Direction)
        {
            Pending.MenuButtons = FMath::Clamp(Pending.MenuButtons + Direction * MenuSizeStep, DefaultMenuSize, MaxMenuSize);
            ApplyPreview();
        }

        FReply CycleMenuSize()
        {
            if (Pending.MenuButtons >= MaxMenuSize)
            {
                Pending.MenuButtons = DefaultMenuSize;
                ApplyPreview();
            }
            else StepMenuSize(1);
            return FReply::Handled();
        }

        FReply Reset()
        {
            Pending = {};
            ApplyPreview();
            return FReply::Handled();
        }

        FReply SetPreset(int32 Size)
        {
            Pending = {Size, Size, Size, Size, Size, Pending.MenuButtons};
            ApplyPreview();
            return FReply::Handled();
        }

        FText PresetName() const
        {
            if (IsPreset(80)) return LOCTEXT("ActiveCompact", "Compacto");
            if (IsPreset(100)) return LOCTEXT("ActiveNormal", "Normal");
            if (IsPreset(130)) return LOCTEXT("ActiveLarge", "Grande");
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
        TArray<TPair<int32, TSharedRef<SChronosMenuButton>>> PresetButtons;
    };
}

void ChronosMenuArt::PlaceOptionsPanel(UCanvasPanelSlot& PanelSlot, bool bInset)
{
    PanelSlot.SetAnchors(FAnchors(.304f, .045f, .955f, .955f));
    PanelSlot.SetAlignment(FVector2D::ZeroVector);
    PanelSlot.SetAutoSize(false);
    PanelSlot.SetOffsets(bInset ? FMargin(48, 46, 48, 58) : FMargin(0));
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
            ClampScale(Save->HUDMap), ClampScale(Save->HUDCalendar),
            FMath::Clamp(Save->MenuButtonSize, DefaultMenuSize, MaxMenuSize)};
        VisibleMenuSize = Confirmed.MenuButtons;
        MenuMusicPercent = FMath::Clamp(Save->MenuMusicPercent, 0, 100);
        ControlsZoom = FMath::Clamp(Save->ControlsZoom, 75, 150);
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
    VisibleMenuSize = Scales.MenuButtons;
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
    Save->MenuButtonSize = Scales.MenuButtons;
    if (!UGameplayStatics::SaveGameToSlot(Save, OptionsSlot, 0)) return false;
    Confirmed = Scales;
    Preview(Confirmed);
    return true;
}

FVector3f UChronosMenuSettings::AudioVolumes() const
{
    const auto* Instance = GetGameInstance();
    const auto Read = [Instance](FName Name)
    {
        const auto* Property = FindFProperty<FDoubleProperty>(Instance->GetClass(), Name);
        return Property ? static_cast<float>(FMath::Clamp(Property->GetPropertyValue_InContainer(Instance), 0., 1.)) : 1.f;
    };
    return FVector3f(Read(TEXT("volumenGeneral")), Read(TEXT("volumenAmbiente")), MenuMusicPercent / 100.f);
}

void UChronosMenuSettings::PreviewAudioVolumes(const FVector3f& Volumes)
{
    UWorld* World = GetGameInstance()->GetWorld();
    if (!World || !World->IsGameWorld()) return;
    static const auto Mix = TSoftObjectPtr<USoundMix>(FSoftObjectPath(TEXT("/Game/Sonido/SMix_Opciones.SMix_Opciones")));
    static const auto Master = TSoftObjectPtr<USoundClass>(FSoftObjectPath(TEXT("/Engine/EngineSounds/Master.Master")));
    static const auto Atmosphere = TSoftObjectPtr<USoundClass>(FSoftObjectPath(TEXT("/Game/Sonido/SC_Ambiente.SC_Ambiente")));
    if (auto* Modifier = Mix.LoadSynchronous())
    {
        UGameplayStatics::SetSoundMixClassOverride(World, Modifier, Master.LoadSynchronous(), Volumes.X, 1.f, 0.f, true);
        UGameplayStatics::SetSoundMixClassOverride(World, Modifier, Atmosphere.LoadSynchronous(), Volumes.X * Volumes.Y, 1.f, 0.f, true);
        if (VolumeWorld.Get() != World)
        {
            UGameplayStatics::PushSoundMixModifier(World, Modifier);
            VolumeWorld = World;
        }
    }
    PreviewMusicVolume = Volumes.Z;
    if (MenuMusic) MenuMusic->SetVolumeMultiplier(PreviewMusicVolume);
}

bool UChronosMenuSettings::SaveAudioVolumes(const FVector3f& Volumes)
{
    auto* Instance = GetGameInstance();
    const auto SaveClass = LoadClass<UChronosOptionsSave>(nullptr, OptionsClass);
    auto* Save = Cast<UChronosOptionsSave>(UChronosOptionsSave::LoadOptionsForWrite(SaveClass));
    if (!Save) return false;
    const auto* General = FindFProperty<FDoubleProperty>(Save->GetClass(), TEXT("volumenGeneral"));
    const auto* Ambient = FindFProperty<FDoubleProperty>(Save->GetClass(), TEXT("volumenAmbiente"));
    const auto* LiveGeneral = FindFProperty<FDoubleProperty>(Instance->GetClass(), TEXT("volumenGeneral"));
    const auto* LiveAmbient = FindFProperty<FDoubleProperty>(Instance->GetClass(), TEXT("volumenAmbiente"));
    if (!General || !Ambient || !LiveGeneral || !LiveAmbient) return false;
    General->SetPropertyValue_InContainer(Save, Volumes.X);
    Ambient->SetPropertyValue_InContainer(Save, Volumes.Y);
    Save->MenuMusicPercent = FMath::RoundToInt(Volumes.Z * 100.f);
    if (!UGameplayStatics::SaveGameToSlot(Save, OptionsSlot, 0)) return false;
    LiveGeneral->SetPropertyValue_InContainer(Instance, Volumes.X);
    LiveAmbient->SetPropertyValue_InContainer(Instance, Volumes.Y);
    MenuMusicPercent = Save->MenuMusicPercent;
    PreviewAudioVolumes(Volumes);
    return true;
}

bool UChronosMenuSettings::SetBindingZoom(int32 Percentage)
{
    const auto Class = LoadClass<UChronosOptionsSave>(nullptr, OptionsClass);
    auto* Save = Cast<UChronosOptionsSave>(UChronosOptionsSave::LoadOptionsForWrite(Class));
    if (!Save) return false;
    Save->ControlsZoom = FMath::Clamp(Percentage, 75, 150);
    if (!UGameplayStatics::SaveGameToSlot(Save, OptionsSlot, 0)) return false;
    ControlsZoom = Save->ControlsZoom;
    return true;
}


// YukoDev — AUDIO DE MENÚS
bool UChronosMenuSettings::UpdateMenuMusic(float)
{
    UWorld* World = GetGameInstance()->GetWorld();
    if (!World || !World->IsGameWorld() || !World->HasBegunPlay() || !World->GetFirstPlayerController()) return true;
    const bool bMainMenu = World->GetMapName().EndsWith(TEXT("MainMenu"), ESearchCase::IgnoreCase);
    if (VolumeWorld.Get() != World) PreviewAudioVolumes(AudioVolumes());
    bWantsMusic = bMainMenu || UChronosPauseMenu::IsOpen(World);

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
        MenuMusic = UGameplayStatics::CreateSound2D(World, Song, PreviewMusicVolume, 1.f, 0.f, nullptr, true, false);
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
    for (FName Name : {FName(TEXT("BTN-GRAFICOS")), FName(TEXT("BTN-volumen")),
        FName(TEXT("BTN-Controles")), FName(TEXT("BTN-IDIOMA")), FName(TEXT("BTN-BACK"))})
        if (auto* Tab = Cast<UButton>(GetWidgetFromName(Name))) SidebarButtons.Add(Tab);
    AlignOptionsShell();
    ThemeExistingPanel();
    if (auto* Pad = UChronosGamepad::Get(GetOwningPlayer())) Pad->OptionsMenu = this;
    if (auto* First = InitialFocus(); First && GetOwningPlayer()) First->SetUserFocus(GetOwningPlayer());
}

UWidget* UChronosOptionsMenu::InitialFocus() const
{
    if (HUDPanel) return GetWidgetFromName(TEXT("BTN-IDIOMA"));
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
    if (auto* Pad = UChronosGamepad::Get(GetOwningPlayer()); Pad && Pad->OptionsMenu.Get() == this)
        Pad->OptionsMenu.Reset();
    CloseHUDSettings();
    SidebarFocus.Set(nullptr);
    SidebarButtons.Reset();
    bHorizontalStickHeld = false;
    AppliedMenuScale = 0.f;
    if (SelectorHost) SelectorHost->RemoveFromParent();
    SelectorHost = nullptr;
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

UNativeWidgetHost* UChronosOptionsMenu::ActivePanel() const
{
    return HUDPanel ? HUDPanel.Get() : SelectorHost.Get();
}

void UChronosOptionsMenu::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    SidebarFocus.Set(SidebarButtons, GetOwningPlayer());
    if (const auto* Settings = UChronosMenuSettings::Get(this); Settings && AppliedMenuScale != Settings->MenuScale())
    {
        AppliedMenuScale = Settings->MenuScale();
        const FVector2D Scale(FMath::Lerp(1.f, AppliedMenuScale, .15f), AppliedMenuScale);
        for (const auto& Entry : SidebarButtons)
            if (auto* Button = Entry.Get()) Button->SetRenderScale(Scale);
    }
}

bool UChronosOptionsMenu::HandleHorizontalStick(FSlateApplication& Slate, const FAnalogInputEvent& Event)
{
    const float Axis = Event.GetAnalogValue();
    if (FMath::Abs(Axis) <= .35f)
    {
        bHorizontalStickHeld = false;
        return false;
    }
    if (!IsInViewport() || !IsVisible() || !HasUserFocusedDescendants(GetOwningPlayer())) return false;
    if (bHorizontalStickHeld) return true;

    const auto* Panel = ActivePanel();
    auto* Tab = InitialFocus();
    if (!Panel || !Tab) return false;

    auto* Player = GetOwningPlayer();
    if (Panel->HasUserFocusedDescendants(Player))
    {
        if (Axis <= -.55f)
            bHorizontalStickHeld = Slate.SetUserFocus(Event.GetUserIndex(), Tab->TakeWidget(), EFocusCause::Navigation);
        return Axis < 0.f || HUDPanel || GetWidgetFromName(TEXT("panelVolumen"));
    }
    if (Axis > 0.f && Tab->HasUserFocus(Player))
    {
        if (Axis >= .55f)
            if (const auto Content = Panel->GetContent())
                bHorizontalStickHeld = Slate.SetUserFocus(Event.GetUserIndex(), Content, EFocusCause::Navigation);
        return true;
    }
    return false;
}

FReply UChronosOptionsMenu::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Right || Event.GetKey() == EKeys::Gamepad_DPad_Right)
    {
        const auto* Panel = ActivePanel();
        const auto* Tab = InitialFocus();
        if (Panel && Tab && Tab->HasUserFocus(GetOwningPlayer()))
            if (const auto Content = Panel->GetContent())
                return FReply::Handled().SetUserFocus(Content.ToSharedRef(), EFocusCause::Navigation);
    }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
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

void UChronosOptionsMenu::AlignOptionsShell()
{
    const bool bGraphics = GetWidgetFromName(TEXT("BTN-apply")) != nullptr;
    const bool bBuildPremiumSidebar = bGraphics && !Backdrop;
    if (const auto* Sidebar = GetWidgetFromName(TEXT("VerticalBox_72")))
        if (auto* SidebarSlot = Cast<UCanvasPanelSlot>(Sidebar->Slot))
        {
            SidebarSlot->SetAnchors(FAnchors(bGraphics ? .017f : 0.f, .035f, bGraphics ? .188f : .293f, .96f));
            SidebarSlot->SetOffsets(FMargin(0));
        }
    if (const auto* Crest = GetWidgetFromName(TEXT("Image_86")))
        if (auto* CrestSlot = Cast<UCanvasPanelSlot>(Crest->Slot))
        {
            CrestSlot->SetAnchors(FAnchors(0.f, 0.f, bGraphics ? .195f : .293f, .4f));
            CrestSlot->SetOffsets(FMargin(0));
        }
    UWidget* Frame = GetWidgetFromName(TEXT("marcoPanel"));
    if (!Frame) Frame = GetWidgetFromName(TEXT("Image_80"));
    if (auto* Image = Cast<UImage>(Frame))
    {
        Image->SetBrush(ChronosMenuArt::OptionsPanel());
        Image->SetVisibility(bGraphics ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
        if (auto* FrameSlot = Cast<UCanvasPanelSlot>(Image->Slot))
            ChronosMenuArt::PlaceOptionsPanel(*FrameSlot);
    }
    if (auto* Canvas = Cast<UCanvasPanel>(GetRootWidget()); Canvas && !Backdrop)
    {
        Backdrop = WidgetTree->ConstructWidget<UImage>();
        if (bGraphics) Backdrop->SetBrush(ChronosMenuArt::Premium(TEXT("fondo_iglesia_desenfocado_1920x1080")));
        else Backdrop->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.010f, .008f, .006f, .49f), 4.f));
        Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
        auto* BackdropSlot = Canvas->AddChildToCanvas(Backdrop);
        BackdropSlot->SetAnchors(FAnchors(0, 0, 1, 1));
        BackdropSlot->SetOffsets(FMargin(0));
        BackdropSlot->SetZOrder(1);
        if (Frame)
            if (auto* FrameSlot = Cast<UCanvasPanelSlot>(Frame->Slot)) FrameSlot->SetZOrder(2);
        for (FName Name : {FName(TEXT("VerticalBox_72")), FName(TEXT("Image_86")),
            FName(TEXT("CanvasPanel_153")), FName(TEXT("panelVolumen")), FName(TEXT("panelControles"))})
            if (const auto* Child = GetWidgetFromName(Name))
                if (auto* ChildSlot = Cast<UCanvasPanelSlot>(Child->Slot)) ChildSlot->SetZOrder(3);
    }
    if (!bBuildPremiumSidebar) return;
    const TCHAR* Icons[] = {TEXT("pantalla"), TEXT("audio"), TEXT("mando"), TEXT("hud"), TEXT("flecha_atras")};
    for (int32 Index = 0; Index < SidebarButtons.Num() && Index < UE_ARRAY_COUNT(Icons); ++Index)
    {
        auto* Tab = SidebarButtons[Index].Get();
        if (!Tab || !Tab->GetContent()) continue;
        auto* Caption = Tab->GetContent();
        UWidgetTree::ForWidgetAndChildren(Caption, [](UWidget* Widget)
        {
            if (auto* Label = Cast<UTextBlock>(Widget))
            {
                auto Font = Label->GetFont();
                Font.Size = FMath::Min(Font.Size, 20.f);
                Label->SetFont(Font);
            }
        });
        Caption->RemoveFromParent();
        auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        auto* Symbol = WidgetTree->ConstructWidget<UImage>();
        FSlateBrush Brush = ChronosMenuArt::Premium(FString::Printf(TEXT("icono_%s_48x48"), Icons[Index]));
        Brush.ImageSize = FVector2D(31, 31);
        Symbol->SetBrush(Brush);
        auto* IconSlot = Row->AddChildToHorizontalBox(Symbol);
        IconSlot->SetPadding(FMargin(8, 0, 13, 0));
        IconSlot->SetVerticalAlignment(VAlign_Center);
        Row->AddChildToHorizontalBox(Caption)->SetVerticalAlignment(VAlign_Center);
        auto* Fit = WidgetTree->ConstructWidget<UScaleBox>();
        Fit->SetStretch(EStretch::ScaleToFit);
        Fit->SetStretchDirection(EStretchDirection::DownOnly);
        Fit->SetContent(Row);
        auto* TabContent = CastChecked<UButtonSlot>(Tab->SetContent(Fit));
        TabContent->SetHorizontalAlignment(HAlign_Fill);
        if (auto* TabSlot = Cast<UVerticalBoxSlot>(Tab->Slot)) TabSlot->SetHorizontalAlignment(HAlign_Fill);
        if (Index < 4)
        {
            auto Artwork = ChronosMenuArt::PremiumButton(TEXT("navegacion_pestana"), Index == 0);
            if (Index != 0) Artwork.SetNormal(FSlateNoResource());
            Tab->SetStyle(Artwork);
        }
    }
}


void UChronosOptionsMenu::ThemeExistingPanel()
{
    const bool bGraphics = GetWidgetFromName(TEXT("BTN-apply")) != nullptr;
    auto* Legacy = GetWidgetFromName(bGraphics ? TEXT("CanvasPanel_153") : TEXT("panelVolumen"));
    auto* Root = Cast<UCanvasPanel>(GetRootWidget());
    const auto* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("TextBlock_161")));
    if (!Legacy || !Root || !Label || SelectorHost) return;

    Legacy->SetVisibility(ESlateVisibility::Collapsed);
    auto* Host = WidgetTree->ConstructWidget<UNativeWidgetHost>();
    SelectorHost = Host;
    Host->SetContent(bGraphics ? MakeChronosGraphicsPanel(Label->GetFont(), UChronosMenuSettings::Get(this))
        : MakeChronosVolumePanel(Label->GetFont(), UChronosMenuSettings::Get(this)));
    auto* Layout = Root->AddChildToCanvas(Host);
    if (bGraphics)
    {
        Layout->SetAnchors(FAnchors(.197f, .018f, .995f, .945f));
        Layout->SetOffsets(FMargin(0));
    }
    else ChronosMenuArt::PlaceOptionsPanel(*Layout, true);
    Layout->SetZOrder(4);
    UE_LOG(LogTemp, Display, TEXT("Chronos options panel active: %s."), *GetClass()->GetPathName());
}

void UChronosOptionsMenu::OpenHUDSettings()
{
    if (HUDPanel) return;
    auto* Settings = UChronosMenuSettings::Get(this);
    auto* Canvas = Cast<UCanvasPanel>(GetRootWidget());
    auto* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("TextBlock_2")));
    auto* HUDTab = Cast<UButton>(GetWidgetFromName(TEXT("BTN-IDIOMA")));
    if (!Canvas || !Label || !HUDTab || !Settings) return;

    SidebarFocus.Set(nullptr);
    auto* Column = HUDTab->GetParent();
    for (UWidget* Child : Canvas->GetAllChildren())
    {
        if (Child == Column || Child->GetFName() == TEXT("Image_54") || Child->GetFName() == TEXT("fondoPausa")
            || Child->GetFName() == TEXT("Image_86") || Child == Backdrop) continue;
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
        SampleSlot->SetAnchors(FAnchors(.375f, .65f, .945f, .925f));
        SampleSlot->SetOffsets(FMargin(0));
        SampleSlot->SetZOrder(42);
    }
    auto* Host = WidgetTree->ConstructWidget<UNativeWidgetHost>();
    HUDPanel = Host;
    Host->SetContent(SNew(SChronosHUDSettings).Settings(Settings).Sample(SampleHUD.Get()).Font(Label->GetFont())
        .Close(FSimpleDelegate::CreateUObject(this, &UChronosOptionsMenu::CloseHUDSettings)));
    auto* PanelSlot = Canvas->AddChildToCanvas(Host);
    ChronosMenuArt::PlaceOptionsPanel(*PanelSlot);
    PanelSlot->SetZOrder(41);
}

void UChronosOptionsMenu::CloseHUDSettings()
{
    if (!HUDPanel) return;
    SidebarFocus.Set(nullptr);
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
