#include "ChronosSaveMenus.h"
#include "ChronosGamepad.h"
#include "ChronosMenuArt.h"
#include "ChronosPlayerSave.h"
#include "ChronosSaveThumbnail.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/NativeWidgetHost.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "ChronosSaveMenus"

namespace
{
    const FSlateColor TextColor(FLinearColor(FColor(239, 218, 181)));
    using FSlot = ChronosPlayerSave::FSlot;
    using FEntry = TSharedPtr<FSlot>;

    class SSavePreview final : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SSavePreview) {}
            SLATE_ARGUMENT(int32, Slot)
            SLATE_ARGUMENT(FSlateFontInfo, Font)
        SLATE_END_ARGS()

        void Construct(const FArguments& Args)
        {
            if (const auto* Save = ChronosPlayerSave::Read(Args._Slot))
                Texture.Reset(ChronosSaveThumbnail::Decode(Save->PreviewPng));
            TSharedRef<SWidget> Content = SNew(STextBlock)
                .Text(LOCTEXT("NoPreview", "Sin miniatura")).Font(Args._Font).ColorAndOpacity(TextColor);
            if (Texture)
            {
                Image.SetResourceObject(Texture.Get());
                Image.DrawAs = ESlateBrushDrawType::Image;
                Content = SNew(SImage).Image(&Image);
            }
            static const FSlateRoundedBoxBrush Frame(FLinearColor(.008f, .006f, .004f), 2.f, FLinearColor(.38f, .28f, .15f), 1.f);
            ChildSlot
            [
                SNew(SBox).WidthOverride(160).HeightOverride(90)
                [SNew(SBorder).BorderImage(&Frame).Padding(2)
                    .HAlign(Texture ? HAlign_Fill : HAlign_Center).VAlign(Texture ? VAlign_Fill : VAlign_Center)
                    [Content]]
            ];
        }

    private:
        TStrongObjectPtr<UTexture2D> Texture;
        FSlateBrush Image;
    };

    class SChronosSaveList final : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SChronosSaveList) {}
            SLATE_ARGUMENT(TWeakObjectPtr<UChronosSaveBrowser>, Owner)
            SLATE_ARGUMENT(FSlateFontInfo, Font)
            SLATE_ARGUMENT(bool, ShowTitle)
        SLATE_END_ARGS()

        void Construct(const FArguments& Args)
        {
            Owner = Args._Owner;
            Font = Args._Font;
            Font.Size = 18;
            Small = Font;
            Small.Size = 14;
            bSaving = Owner.IsValid() && Owner->IsSaving();
            static const FTableViewStyle ListStyle = FTableViewStyle().SetBackgroundBrush(FSlateNoResource());

            auto Layout = SNew(SVerticalBox);
            if (Args._ShowTitle)
                Layout->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 2, 0, 12)
                [SNew(STextBlock).Text(bSaving ? LOCTEXT("Save", "Guardar partida") : LOCTEXT("Load", "Cargar partida"))
                    .Font(Font).ColorAndOpacity(TextColor)];

            Layout->AddSlot().FillHeight(1)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [SAssignNew(List, SListView<FEntry>).ListViewStyle(&ListStyle).ListItemsSource(&Entries).SelectionMode(ESelectionMode::Single)
                    .IsEnabled_Lambda([this] { return Pending == INDEX_NONE; })
                    .OnGenerateRow(this, &SChronosSaveList::Generate).AllowOverscroll(EAllowOverscroll::No)]
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                [SNew(STextBlock).Text(LOCTEXT("Empty", "No hay partidas guardadas")).Font(Font).ColorAndOpacity(TextColor)
                    .Visibility_Lambda([this] { return Entries.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })]
            ];
            Layout->AddSlot().AutoHeight().Padding(0, 8, 0, 6)
            [SNew(STextBlock).Text_Lambda([this] { return Status; }).Font(Small).ColorAndOpacity(TextColor).AutoWrapText(true)];

            auto Actions = SNew(SHorizontalBox);
            const auto BackButton = Button(LOCTEXT("Back", "Atrás"), FOnClicked::CreateSP(this, &SChronosSaveList::Back));
            const auto ConfirmButton = Button(LOCTEXT("Confirm", "Confirmar"), FOnClicked::CreateSP(this, &SChronosSaveList::Confirm));
            BackFocus = BackButton->GetChildren()->GetChildAt(0);
            ConfirmFocus = ConfirmButton->GetChildren()->GetChildAt(0);
            if (bSaving)
                Actions->AddSlot().FillWidth(1).Padding(0, 0, 8, 0)
                [Button(LOCTEXT("New", "Nuevo guardado"), FOnClicked::CreateSP(this, &SChronosSaveList::SaveNew))];
            Actions->AddSlot().FillWidth(1).Padding(0, 0, 8, 0)
            [Button(bSaving ? LOCTEXT("Overwrite", "Sobrescribir") : LOCTEXT("LoadSelected", "Cargar partida"),
                FOnClicked::CreateSP(this, &SChronosSaveList::UseSelected), TAttribute<bool>::CreateLambda([this]
                {
                    const auto Entry = List->GetSelectedItems();
                    return !Entry.IsEmpty() && (bSaving || Entry[0]->bReadable);
                }))];
            Actions->AddSlot().FillWidth(1)
            [BackButton];

            Layout->AddSlot().AutoHeight()
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [SNew(SBox).Visibility_Lambda([this] { return Pending == INDEX_NONE ? EVisibility::Visible : EVisibility::Collapsed; })[Actions]]
                + SOverlay::Slot()
                [
                    SNew(SHorizontalBox)
                    .Visibility_Lambda([this] { return Pending != INDEX_NONE ? EVisibility::Visible : EVisibility::Collapsed; })
                    + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 8, 0)
                    [ConfirmButton]
                    + SHorizontalBox::Slot().FillWidth(1)
                    [Button(LOCTEXT("Cancel", "Cancelar"), FOnClicked::CreateSP(this, &SChronosSaveList::Back))]
                ]
            ];
            ChildSlot[Layout];
            Refresh();
        }

        FReply Back()
        {
            if (Pending != INDEX_NONE)
            {
                Pending = INDEX_NONE;
                Status = FText();
                return FReply::Handled().SetUserFocus(List.ToSharedRef(), EFocusCause::SetDirectly);
            }
            if (Owner.IsValid()) Owner->GoBack();
            return FReply::Handled();
        }

        TSharedRef<SWidget> FirstFocus() const
        {
            return Entries.IsEmpty() ? BackFocus.ToSharedRef() : TSharedRef<SWidget>(List.ToSharedRef());
        }

        FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override
        {
            if (Event.GetKey() == EKeys::Gamepad_FaceButton_Bottom && !Event.IsRepeat()
                && Pending == INDEX_NONE && List->HasKeyboardFocus())
                return UseSelected();
            return SCompoundWidget::OnPreviewKeyDown(Geometry, Event);
        }

    private:
        TSharedRef<SBox> Button(const FText& Label, FOnClicked Click, TAttribute<bool> Enabled = true) const
        {
            return SNew(SBox).MinDesiredHeight(42)
                [SNew(SButton).ButtonStyle(&ChronosMenuArt::ButtonStyle()).HAlign(HAlign_Center).VAlign(VAlign_Center)
                    .OnClicked(MoveTemp(Click)).IsEnabled(MoveTemp(Enabled))
                    [SNew(STextBlock).Text(Label).Font(Small).ColorAndOpacity(TextColor)]];
        }

        TSharedRef<ITableRow> Generate(FEntry Entry, const TSharedRef<STableViewBase>& OwnerTable)
        {
            static const FTableRowStyle Style = []
            {
                auto Value = FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("TableView.Row"));
                const FSlateRoundedBoxBrush Regular(FLinearColor(.014f, .011f, .008f, .82f), 2.f, FLinearColor(.13f, .09f, .046f), 1.f);
                const FSlateRoundedBoxBrush Hover(FLinearColor(.061f, .04f, .021f, .95f), 2.f, FLinearColor(.34f, .24f, .13f), 1.f);
                const FSlateRoundedBoxBrush Selected(FLinearColor(.10f, .064f, .030f, .95f), 2.f, FLinearColor(.85f, .62f, .31f), 2.f);
                return Value.SetEvenRowBackgroundBrush(Regular).SetOddRowBackgroundBrush(Regular)
                    .SetEvenRowBackgroundHoveredBrush(Hover).SetOddRowBackgroundHoveredBrush(Hover)
                    .SetActiveBrush(Selected).SetInactiveBrush(Selected).SetActiveHoveredBrush(Selected).SetInactiveHoveredBrush(Selected);
            }();
            const FText Date = Entry->LocalTime.GetTicks() > 0
                ? FText::FromString(Entry->LocalTime.ToString(TEXT("%d/%m/%Y — %H:%M"))) : FText();
            const FString Details = Entry->bReadable
                ? Entry->Calendar.IsEmpty() ? Entry->Level : Entry->Calendar + TEXT("  •  ") + Entry->Level
                : LOCTEXT("Invalid", "Partida dañada o nivel no disponible").ToString();
            return SNew(STableRow<FEntry>, OwnerTable).Style(&Style).Padding(FMargin(8, 6))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 14, 0)
                [SNew(SSavePreview).Slot(Entry->Number).Font(Small)]
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [SNew(STextBlock).Text(FText::FromString(Entry->Label)).Font(Font).ColorAndOpacity(TextColor)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
                    [SNew(STextBlock).Text(Date).Font(Small).ColorAndOpacity(TextColor)]
                    + SVerticalBox::Slot().AutoHeight()
                    [SNew(STextBlock).Text(FText::FromString(Details)).Font(Small).ColorAndOpacity(TextColor).AutoWrapText(true)]
                ]
            ];
        }

        void Refresh(int32 Select = 0)
        {
            List->ClearSelection();
            Entries.Reset();
            for (FSlot& Slot : ChronosPlayerSave::List())
            {
                auto Item = MakeShared<FSlot>(MoveTemp(Slot));
                Entries.Add(Item);
                if (Item->Number == Select) SelectAfterRefresh = Item;
            }
            List->RequestListRefresh();
            if (SelectAfterRefresh)
            {
                List->SetSelection(SelectAfterRefresh);
                List->RequestScrollIntoView(SelectAfterRefresh);
                SelectAfterRefresh.Reset();
            }
        }

        FReply SaveNew()
        {
            TSet<int32> Used;
            for (const FSlot& Slot : ChronosPlayerSave::List()) Used.Add(Slot.Number);
            int32 Free = 1;
            while (Used.Contains(Free) && Free < MAX_int32) ++Free;
            if (!Used.Contains(Free)) Save(Free);
            return FReply::Handled();
        }

        FReply UseSelected()
        {
            const TArray<FEntry> Selected = List->GetSelectedItems();
            if (Selected.IsEmpty() || !Owner.IsValid() || (!bSaving && !Selected[0]->bReadable)) return FReply::Handled();
            const int32 Number = Selected[0]->Number;
            if (!bSaving && !Owner->IsFromPause())
            {
                Load(Number);
                return FReply::Handled();
            }
            Pending = Number;
            Status = bSaving ? LOCTEXT("OverwriteQuestion", "¿Quieres sobrescribir esta partida?")
                : LOCTEXT("LoadQuestion", "¿Cargar esta partida? Se perderá el progreso sin guardar.");
            return FReply::Handled().SetUserFocus(ConfirmFocus.ToSharedRef());
        }

        FReply Confirm()
        {
            const int32 Number = Pending;
            Pending = INDEX_NONE;
            if (bSaving) Save(Number);
            else if (Load(Number)) return FReply::Handled();
            return FReply::Handled().SetUserFocus(FirstFocus());
        }

        void Save(int32 Number)
        {
            if (Owner.IsValid() && Owner->WriteSlot(Number))
            {
                Refresh(Number);
                Status = LOCTEXT("Saved", "Partida guardada.");
            }
            else Status = LOCTEXT("SaveFailed", "No se pudo guardar la partida o capturar la miniatura.");
        }

        bool Load(int32 Number)
        {
            if (Owner.IsValid() && Owner->LoadSlot(Number)) return true;
            Refresh();
            Status = LOCTEXT("LoadFailed", "La partida ya no está disponible o está dañada.");
            return false;
        }

        TWeakObjectPtr<UChronosSaveBrowser> Owner;
        TArray<FEntry> Entries;
        FEntry SelectAfterRefresh;
        TSharedPtr<SListView<FEntry>> List;
        TSharedPtr<SWidget> ConfirmFocus;
        TSharedPtr<SWidget> BackFocus;
        FSlateFontInfo Font;
        FSlateFontInfo Small;
        FText Status;
        int32 Pending = INDEX_NONE;
        bool bSaving = false;
    };
}

void UChronosMainMenu::NativeConstruct()
{
    SetIsFocusable(true);
    Super::NativeConstruct();
    auto* Player = GetOwningPlayer();
    const auto* Pad = UChronosGamepad::Get(Player);
    if (auto* First = GetWidgetFromName(Pad ? Pad->MainMenuFocus : FName(TEXT("BTN-NuevaPartida"))))
        First->SetUserFocus(Player);
}

void UChronosMainMenu::NativeDestruct()
{
    auto* Player = GetOwningPlayer();
    if (auto* Pad = UChronosGamepad::Get(Player))
        for (FName Name : {FName(TEXT("BTN-NuevaPartida")), FName(TEXT("BTN-Cargarpartida")), FName(TEXT("BTN-Opciones")), FName(TEXT("BTN-Cuaderno"))})
            if (auto* Button = GetWidgetFromName(Name); Button && Button->HasUserFocus(Player))
            {
                Pad->MainMenuFocus = Name;
                break;
            }
    Super::NativeDestruct();
}

// YukoDev — SISTEMA DE CARGA
void UChronosSaveBrowser::NativeConstruct()
{
    Super::NativeConstruct();
    auto* Canvas = Cast<UCanvasPanel>(GetRootWidget());
    const auto* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("TextBlock_143")));
    if (!Canvas || !Label || Host) return;
    const UWidget* Frame = GetWidgetFromName(TEXT("Image_186"));
    const auto* FrameSlot = Frame ? Cast<UCanvasPanelSlot>(Frame->Slot) : nullptr;
    SetIsFocusable(true);
    FSlateFontInfo Font = Label->GetFont();
    for (FName Name : {FName(TEXT("ListaPartidas")), FName(TEXT("TextBlock_143"))})
        if (auto* Widget = GetWidgetFromName(Name)) Widget->SetVisibility(ESlateVisibility::Collapsed);
    if (IsFromPause())
    {
        for (auto* Widget : Canvas->GetAllChildren()) Widget->SetVisibility(ESlateVisibility::Collapsed);
    }

    auto* WidgetHost = WidgetTree->ConstructWidget<UNativeWidgetHost>();
    Host = WidgetHost;
    const auto Contents = SNew(SChronosSaveList).Owner(this).Font(Font).ShowTitle(IsFromPause());
    InitialFocus = Contents->FirstFocus();
    const TWeakPtr<SChronosSaveList> WeakList = Contents;
    CancelOrBack = [WeakList]
    {
        if (auto List = WeakList.Pin()) return List->Back();
        return FReply::Handled();
    };
    if (IsFromPause())
    {
        static const FSlateRoundedBoxBrush Shade(FLinearColor(0, 0, 0, .66f), 0.f);
        WidgetHost->SetContent(SNew(SBorder).BorderImage(&Shade).HAlign(HAlign_Center).VAlign(VAlign_Fill).Padding(FMargin(32, 50))
            [SNew(SBox).WidthOverride(790)
                [SNew(SBorder).BorderImage(&ChronosMenuArt::OptionsPanel()).Padding(FMargin(42, 38))[Contents]]]);
    }
    else WidgetHost->SetContent(Contents);
    auto* HostSlot = Canvas->AddChildToCanvas(WidgetHost);
    if (!IsFromPause() && FrameSlot)
    {
        FAnchorData Layout = FrameSlot->GetLayout();
        Layout.Offsets.Left += 48;
        Layout.Offsets.Top += 48;
        Layout.Offsets.Right -= 96;
        Layout.Offsets.Bottom -= 96;
        HostSlot->SetLayout(Layout);
    }
    else
    {
        HostSlot->SetAnchors(IsFromPause() ? FAnchors(0, 0, 1, 1) : FAnchors(.35f, .18f, .9f, .9f));
        HostSlot->SetOffsets(FMargin(0));
    }
    HostSlot->SetZOrder(5);
    SetUserFocus(GetOwningPlayer());
}

void UChronosSaveBrowser::NativeDestruct()
{
    CancelOrBack = nullptr;
    InitialFocus.Reset();
    if (Host) Host->RemoveFromParent();
    Host = nullptr;
    Super::NativeDestruct();
}

FReply UChronosSaveBrowser::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() != EKeys::Escape && Event.GetKey() != EKeys::Gamepad_FaceButton_Right)
        return Super::NativeOnPreviewKeyDown(Geometry, Event);
    return !Event.IsRepeat() && CancelOrBack ? CancelOrBack() : FReply::Handled();
}

FReply UChronosSaveBrowser::NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event)
{
    if (const auto Target = InitialFocus.Pin()) return FReply::Handled().SetUserFocus(Target.ToSharedRef());
    return Super::NativeOnFocusReceived(Geometry, Event);
}

void UChronosSaveBrowser::SetPauseOrigin(UChronosPauseMenu& Pause, bool bSave)
{
    Origin = &Pause;
    bSaving = bSave;
}

void UChronosSaveBrowser::GoBack()
{
    if (auto* Pause = Origin.Get())
    {
        RemoveFromParent();
        Pause->SetVisibility(ESlateVisibility::Visible);
        Pause->FocusBrowserEntry(bSaving);
    }
    else if (auto* Instance = GetGameInstance())
        ChronosPlayerSave::ReturnToMainMenu(*Instance, *this);
}

bool UChronosSaveBrowser::WriteSlot(int32 Number)
{
    auto* Instance = GetGameInstance();
    return bSaving && Instance && ChronosPlayerSave::SaveSlot(*Instance, Number);
}

bool UChronosSaveBrowser::LoadSlot(int32 Number)
{
    auto* Instance = GetGameInstance();
    if (bSaving || !Instance || !ChronosPlayerSave::LoadSlot(*Instance, Number)) return false;
    if (auto* Pause = Origin.Get()) Pause->RemoveFromParent();
    RemoveFromParent();
    return true;
}

// YukoDev — REDISEÑO DE MENÚS
void UChronosPauseMenu::NativeConstruct()
{
    SetIsFocusable(true);
    Super::NativeConstruct();
    auto* Resume = Cast<UButton>(GetWidgetFromName(TEXT("RESUME")));
    auto* Save = Cast<UButton>(GetWidgetFromName(TEXT("BtnGuardar")));
    auto* Options = Cast<UButton>(GetWidgetFromName(TEXT("opciones")));
    auto* Quit = Cast<UButton>(GetWidgetFromName(TEXT("quitMenu")));
    auto* Column = Resume ? Cast<UVerticalBox>(Resume->GetParent()) : nullptr;
    if (!Column || !Save || !Options || !Quit) return;
    if (!LoadButton)
    {
        LoadButton = WidgetTree->ConstructWidget<UButton>();
        LoadButton->SetStyle(Save->GetStyle());
        if (auto* Label = Cast<UTextBlock>(Save->GetContent()))
        {
            auto* Text = DuplicateObject<UTextBlock>(Label, WidgetTree);
            Text->SetText(LOCTEXT("Load", "Cargar partida"));
            LoadButton->AddChild(Text);
        }
        LoadButton->OnClicked.AddUniqueDynamic(this, &UChronosPauseMenu::OpenLoadBrowser);
        LoadButton->OnHovered.AddUniqueDynamic(this, &UChronosPauseMenu::SoundLoadHover);
        auto* Notice = GetWidgetFromName(TEXT("avisoGuardado"));
        if (Notice) Column->RemoveChild(Notice);
        for (auto* Button : {Resume, Save, Options, Quit}) Column->RemoveChild(Button);
        if (auto* ColumnSlot = Cast<UCanvasPanelSlot>(Column->Slot)) ColumnSlot->SetSize(FVector2D(780, 685));
        for (auto* Button : {Resume, Save, LoadButton.Get(), Options, Quit})
        {
            auto* ButtonSlot = Column->AddChildToVerticalBox(Button);
            ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
            ButtonSlot->SetVerticalAlignment(VAlign_Fill);
            PauseButtons.Emplace(Button, Button->GetStyle());
            FButtonStyle Regular = Button->GetStyle();
            Regular.SetHovered(Regular.Normal).SetPressed(Regular.Normal);
            Button->SetStyle(Regular);
        }
        if (Notice) Notice->SetVisibility(ESlateVisibility::Collapsed);
        if (auto* Text = Cast<UTextBlock>(Quit->GetContent()))
            Text->SetText(LOCTEXT("Exit", "Salir al menú principal"));
    }
    Resume->SetUserFocus(GetOwningPlayer());
}

void UChronosPauseMenu::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    UButton* Hovered = nullptr;
    UButton* Focused = nullptr;
    for (const auto& Entry : PauseButtons)
        if (auto* Button = Entry.Key.Get())
        {
            if (Button->IsHovered()) Hovered = Button;
            if (Button->HasUserFocus(GetOwningPlayer())) Focused = Button;
        }
    if (Hovered != PreviousHover.Get()) bPointerHighlight = true;
    if (Focused && Focused != PreviousFocus.Get()) bPointerHighlight = Hovered == Focused;
    PreviousHover = Hovered;
    PreviousFocus = Focused;
    UButton* Next = bPointerHighlight ? Hovered : Focused;
    if (Next == SelectedButton.Get()) return;
    SelectedButton = Next;
    for (const auto& Entry : PauseButtons)
        if (auto* Button = Entry.Key.Get())
        {
            FButtonStyle Style = Entry.Value;
            const FSlateBrush Brush = Button == Next ? Style.Hovered : Style.Normal;
            Style.SetNormal(Brush).SetHovered(Brush).SetPressed(Button == Next ? Entry.Value.Pressed : Brush);
            Button->SetStyle(Style);
        }
}

FReply UChronosPauseMenu::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    const FKey Key = Event.GetKey();
    const auto* Pad = UChronosGamepad::Get(GetOwningPlayer());
    const FKey Pause = Pad ? Pad->Key(TEXT("Chronos.Pause"), Pad->IsAlternate()) : EKeys::Gamepad_Special_Right;
    if (Key != EKeys::Escape && Key != EKeys::Gamepad_FaceButton_Right && Key != Pause)
        return Super::NativeOnPreviewKeyDown(Geometry, Event);
    if (!Event.IsRepeat())
        if (auto* Resume = FindFunction(TEXT("reanudar"))) ProcessEvent(Resume, nullptr);
    return FReply::Handled();
}

void UChronosPauseMenu::NativeDestruct()
{
    if (Browser) Browser->RemoveFromParent();
    Browser = nullptr;
    Super::NativeDestruct();
}

FReply UChronosPauseMenu::NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event)
{
    if (auto* Resume = GetWidgetFromName(TEXT("RESUME")))
        return FReply::Handled().SetUserFocus(Resume->TakeWidget(), EFocusCause::SetDirectly);
    return Super::NativeOnFocusReceived(Geometry, Event);
}

void UChronosPauseMenu::FocusBrowserEntry(bool bSaving)
{
    UWidget* Button = bSaving ? GetWidgetFromName(TEXT("BtnGuardar")) : LoadButton.Get();
    if (Button) Button->SetUserFocus(GetOwningPlayer());
}

void UChronosPauseMenu::FocusOptionsEntry()
{
    if (auto* Button = GetWidgetFromName(TEXT("opciones"))) Button->SetUserFocus(GetOwningPlayer());
}

void UChronosPauseMenu::OpenSaveBrowser() { OpenBrowser(true); }
void UChronosPauseMenu::OpenLoadBrowser()
{
    if (auto* Sound = FindFunction(TEXT("sonarClick")); Sound && Sound->NumParms == 0) ProcessEvent(Sound, nullptr);
    OpenBrowser(false);
}

void UChronosPauseMenu::SoundLoadHover()
{
    if (auto* Sound = FindFunction(TEXT("sonarHover")); Sound && Sound->NumParms == 0) ProcessEvent(Sound, nullptr);
}

void UChronosPauseMenu::OpenBrowser(bool bSave)
{
    auto* Player = GetOwningPlayer();
    auto* WidgetClass = LoadClass<UChronosSaveBrowser>(nullptr, TEXT("/Game/UI/cargarPartida.cargarPartida_C"));
    if (!Player || !WidgetClass) return;
    Browser = CreateWidget<UChronosSaveBrowser>(Player, WidgetClass);
    if (!Browser) return;
    Browser->SetPauseOrigin(*this, bSave);
    SetVisibility(ESlateVisibility::Collapsed);
    Browser->AddToViewport(100);
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(Browser->TakeWidget());
    Player->SetInputMode(Mode);
}

#undef LOCTEXT_NAMESPACE
