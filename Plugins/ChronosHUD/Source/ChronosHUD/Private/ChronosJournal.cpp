#include "ChronosJournal.h"
#include "ChronosPlayerSave.h"
#include "ChronosGamepad.h"
#include "ChronosHUDLayer.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"

namespace
{
    constexpr TCHAR Root[] = TEXT("/Game/TheChronosCurse_HUD_Assets/");
    constexpr TCHAR SaveSlot[] = TEXT("Chronos_Cuaderno");
    constexpr TCHAR CatalogPath[] = TEXT("Data/DT_Cuaderno");
    constexpr int32 TasksPerSpread = 8;
    const TCHAR* Sections[] = {TEXT("Tareas"), TEXT("Bestiario"), TEXT("Apariciones"), TEXT("Herbario"), TEXT("Poemas de Emily"), TEXT("Notas")};
    const TCHAR* BaseImages[] = {TEXT("Tareas"), TEXT("Bestiario"), TEXT("Apariciones"), TEXT("Herbario"), TEXT("Poemas"), TEXT("Notas")};
    const FLinearColor Ink(.105f, .061f, .033f);
    const FLinearColor Muted(.20f, .125f, .08f);
    const FLinearColor Ivory(.93f, .85f, .66f);

    template <typename AssetType>
    AssetType* LoadArt(const FString& Relative)
    {
        const FString Path = Root + Relative;
        return LoadObject<AssetType>(nullptr, *(Path + TEXT(".") + FPaths::GetCleanFilename(Path)));
    }

    void Place(UCanvasPanel* Canvas, UWidget* Widget, FVector2D Position, FVector2D Size)
    {
        UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
        Slot->SetPosition(Position);
        Slot->SetSize(Size);
    }

    FSlateBrush BrushFor(UTexture2D* Texture, const FLinearColor& Color = FLinearColor::White)
    {
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::Image;
        Brush.SetResourceObject(Texture);
        Brush.TintColor = FSlateColor(Color);
        return Brush;
    }

    FButtonStyle PaperButton(UTexture2D* Texture)
    {
        FButtonStyle Style;
        Style.SetNormal(BrushFor(Texture));
        Style.SetHovered(BrushFor(Texture, FLinearColor(.75f, .69f, .55f)));
        Style.SetPressed(BrushFor(Texture, FLinearColor(.47f, .39f, .30f)));
        Style.SetDisabled(BrushFor(Texture, FLinearColor(1, 1, 1, .24f)));
        Style.SetNormalPadding(FMargin(0));
        Style.SetPressedPadding(FMargin(0));
        return Style;
    }

    FText JournalText(const TCHAR* Value) { return FText::FromString(Value); }
}

void UChronosJournal::Snapshot(UWorld* World, TArray<uint8>& Out)
{
    if (!World) return;
    if (const auto* Loaded = ChronosPlayerSave::LoadedFrom(World); Loaded && !Loaded->JournalData.IsEmpty())
    {
        Out = Loaded->JournalData;
        return;
    }
    const FString Name = World->WorldType == EWorldType::PIE ? FString(SaveSlot) + TEXT("_PIE") : SaveSlot;
    auto* Existing = Cast<UChronosJournalSave>(UGameplayStatics::LoadGameFromSlot(Name, 0));
    if (!Existing) Existing = NewObject<UChronosJournalSave>();
    UGameplayStatics::SaveGameToMemory(Existing, Out);
}

bool UChronosJournal::Prepare(UFont* InFont)
{
    Catalog = LoadArt<UDataTable>(CatalogPath);
    if (!Catalog || Catalog->GetRowStruct() != FChronosJournalEntry::StaticStruct() || !InFont) return false;
    Serif = InFont;
    const FString SaveName = GetWorld()->WorldType == EWorldType::PIE ? FString(SaveSlot) + TEXT("_PIE") : SaveSlot;
    if (const auto* Loaded = ChronosPlayerSave::LoadedFrom(this); Loaded && !Loaded->JournalData.IsEmpty())
        Progress = Cast<UChronosJournalSave>(UGameplayStatics::LoadGameFromMemory(Loaded->JournalData));
    if (!Progress) Progress = Cast<UChronosJournalSave>(UGameplayStatics::LoadGameFromSlot(SaveName, 0));
    if (!Progress) Progress = NewObject<UChronosJournalSave>(this);
    Progress->ZoomStep = FMath::Clamp(Progress->ZoomStep, 0, 4);

    SetIsFocusable(true);
    UCanvasPanel* Background = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Background;
    UImage* Shade = WidgetTree->ConstructWidget<UImage>();
    FSlateBrush ShadeBrush = *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    ShadeBrush.TintColor = FSlateColor(FLinearColor(0, 0, 0, .88f));
    Shade->SetBrush(ShadeBrush);
    UCanvasPanelSlot* ShadeSlot = Background->AddChildToCanvas(Shade);
    ShadeSlot->SetAnchors(FAnchors(0, 0, 1, 1));
    ShadeSlot->SetOffsets(FMargin(0));
    Shade->SetVisibility(ESlateVisibility::HitTestInvisible);
    UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
    Scale->SetStretch(EStretch::ScaleToFit);
    UCanvasPanelSlot* ScaleSlot = Background->AddChildToCanvas(Scale);
    ScaleSlot->SetAnchors(FAnchors(0, 0, 1, 1));
    ScaleSlot->SetOffsets(FMargin(12));
    USizeBox* Fixed = WidgetTree->ConstructWidget<USizeBox>();
    Fixed->SetWidthOverride(1600.f);
    Fixed->SetHeightOverride(960.f);
    Scale->AddChild(Fixed);
    Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    Fixed->AddChild(Canvas);

    Book = WidgetTree->ConstructWidget<UImage>();
    Place(Canvas, Book, FVector2D(0, 111), FVector2D(1600, 800));
    Book->SetVisibility(ESlateVisibility::HitTestInvisible);

    UTexture2D* NormalTab = LoadArt<UTexture2D>(TEXT("PapersAndTabs/Pestana_fisica_normal"));
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Sections); ++Index)
    {
        UButton* Tab = WidgetTree->ConstructWidget<UButton>();
        Tab->SetStyle(PaperButton(NormalTab));
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
        Label->SetText(JournalText(Sections[Index]));
        Label->SetFont(FSlateFontInfo(Serif.Get(), Index == 4 ? 15 : 18));
        Label->SetJustification(ETextJustify::Center);
        Label->SetVisibility(ESlateVisibility::HitTestInvisible);
        Tab->AddChild(Label);
        Place(Canvas, Tab, FVector2D(130 + 221 * Index, 66), FVector2D(210, 53));
        Tabs.Add(Tab);
        TabLabels.Add(Label);
    }
    Tabs[0]->OnClicked.AddDynamic(this, &UChronosJournal::ClickTasks);
    Tabs[1]->OnClicked.AddDynamic(this, &UChronosJournal::ClickBestiary);
    Tabs[2]->OnClicked.AddDynamic(this, &UChronosJournal::ClickAppearances);
    Tabs[3]->OnClicked.AddDynamic(this, &UChronosJournal::ClickHerbarium);
    Tabs[4]->OnClicked.AddDynamic(this, &UChronosJournal::ClickPoems);
    Tabs[5]->OnClicked.AddDynamic(this, &UChronosJournal::ClickNotes);
    for (int32 Index = 0; Index < 2; ++Index)
    {
        UScrollBox* Reader = WidgetTree->ConstructWidget<UScrollBox>();
        Reader->SetAllowOverscroll(false);
        Reader->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
        Reader->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);
        Reader->SetClipping(EWidgetClipping::ClipToBounds);
        Reader->SetScrollbarThickness(FVector2D(7.f, 7.f));
        Place(Canvas, Reader, FVector2D(Index == 0 ? 162. : 890., 186), FVector2D(548, 592));
        UVerticalBox* PageBox = WidgetTree->ConstructWidget<UVerticalBox>();
        Reader->AddChild(PageBox);
        Readers.Add(Reader);
        Pages.Add(PageBox);
    }

    Prev = AddPageArrow(TEXT("2f9ccc04-39d2-4003-844e-cdd3ba7886d3"), JournalText(TEXT("Página anterior · A")), FVector2D(180, 824));
    Next = AddPageArrow(TEXT("4208292d-3b23-471d-ab18-0a5a26c1e7ed"), JournalText(TEXT("Página siguiente · D")), FVector2D(1420, 824));
    ZoomOut = AddIcon(TEXT("zoom_menos_128_RGBA"), JournalText(TEXT("Reducir texto · -")), FVector2D(552, 824), FVector2D(49));
    AddIcon(TEXT("lupa_128_RGBA"), JournalText(TEXT("Restaurar zoom · R")), FVector2D(600, 824), FVector2D(44))->OnClicked.AddDynamic(this, &UChronosJournal::ClickReset);
    ZoomIn = AddIcon(TEXT("zoom_mas_128_RGBA"), JournalText(TEXT("Ampliar texto · +")), FVector2D(722, 824), FVector2D(49));
    Prev->OnClicked.AddDynamic(this, &UChronosJournal::ClickPrev);
    Next->OnClicked.AddDynamic(this, &UChronosJournal::ClickNext);
    ZoomOut->OnClicked.AddDynamic(this, &UChronosJournal::ClickZoomOut);
    ZoomIn->OnClicked.AddDynamic(this, &UChronosJournal::ClickZoomIn);

    auto Footer = [&](FVector2D Pos, FVector2D Size, const FText& Label, float FontSize, const FLinearColor& Color)
    {
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Text->SetText(Label);
        Text->SetFont(FSlateFontInfo(Serif.Get(), FontSize));
        Text->SetColorAndOpacity(FSlateColor(Color));
        Text->SetJustification(ETextJustify::Center);
        Text->SetVisibility(ESlateVisibility::HitTestInvisible);
        Place(Canvas, Text, Pos, Size);
        return Text;
    };
    ZoomCounter = Footer(FVector2D(621, 809), FVector2D(79, 34), FText::GetEmpty(), 20.f, Ink);
    PageCounter = Footer(FVector2D(1157, 811), FVector2D(206, 34), FText::GetEmpty(), 18.f, Ink);
    InputHints = Footer(FVector2D(220, 912), FVector2D(1080, 34),
        JournalText(TEXT("Q / E   Sección     ·     A / D   Página     ·     Rueda   Desplazar     ·     + / -   Zoom")), 17.f, Ivory);
    auto TextButton = [&](const FText& Caption, FVector2D Pos, FVector2D Size)
    {
        UButton* Button = WidgetTree->ConstructWidget<UButton>();
        Button->SetStyle(PaperButton(NormalTab));
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
        Label->SetText(Caption);
        Label->SetFont(FSlateFontInfo(Serif.Get(), 16));
        Label->SetColorAndOpacity(FSlateColor(Ink));
        Label->SetJustification(ETextJustify::Center);
        Label->SetVisibility(ESlateVisibility::HitTestInvisible);
        Button->AddChild(Label);
        Place(Canvas, Button, Pos, Size);
        return Button;
    };
    TextButton(JournalText(TEXT("Contraste")), FVector2D(905, 801), FVector2D(170, 47))->OnClicked.AddDynamic(this, &UChronosJournal::ClickContrast);
    TextButton(JournalText(TEXT("Esc · Cerrar")), FVector2D(1380, 907), FVector2D(196, 43))->OnClicked.AddDynamic(this, &UChronosJournal::ClickClose);

    Cover = WidgetTree->ConstructWidget<UImage>();
    Cover->SetBrushFromTexture(LoadArt<UTexture2D>(TEXT("HomePage/T_Cover_Raven_Transparent")));
    Cover->SetVisibility(ESlateVisibility::Collapsed);
    Place(Canvas, Cover, FVector2D(470, 61), FVector2D(660, 824));
    PaperSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Letter/S-PegarNota.S-PegarNota"));
    PageTurnSound = LoadArt<USoundBase>(TEXT("Book_Pages"));
    JournalButtons.Reset();
    WidgetTree->ForEachWidget([this](UWidget* Widget)
    {
        if (auto* Button = Cast<UButton>(Widget)) JournalButtons.Add(Button);
    });
    return true;
}

UButton* UChronosJournal::AddIcon(const TCHAR* Asset, const FText& Hint, FVector2D Center, FVector2D Size)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>();
    Button->SetStyle(PaperButton(LoadArt<UTexture2D>(FString(TEXT("Icons/")) + Asset)));
    Button->SetToolTipText(Hint);
    Place(Canvas, Button, Center - Size * .5, Size);
    return Button;
}

UButton* UChronosJournal::AddPageArrow(const TCHAR* Asset, const FText& Hint, FVector2D Center)
{
    const FVector2D Size(94., 61.);
    UButton* Button = AddIcon(Asset, Hint, Center, Size);
    FButtonStyle Style = Button->GetStyle();
    // The supplied square textures have generous transparent margins around the arrow.
    const FBox2f Crop(FVector2f(.037f, .205f), FVector2f(.950f, .800f));
    Style.Normal.SetUVRegion(Crop);
    Style.Hovered.SetUVRegion(Crop);
    Style.Pressed.SetUVRegion(Crop);
    Style.Disabled.SetUVRegion(Crop);
    Button->SetStyle(Style);
    return Button;
}
 
void UChronosJournal::ShowJournal(FKey InCloseKey)
{
    CloseKey = InCloseKey;
    bCloseRequested = false;
    RefreshPoems();
    DrawPages(true);
    Opening = .22f;
    Cover->SetRenderOpacity(1.f);
    Cover->SetVisibility(ESlateVisibility::HitTestInvisible);
    Canvas->SetRenderOpacity(.65f);
    if (PaperSound) UGameplayStatics::PlaySound2D(this, PaperSound, .28f);
}

bool UChronosJournal::ConsumeCloseRequest()
{
    const bool bResult = bCloseRequested;
    bCloseRequested = false;
    return bResult;
}

void UChronosJournal::RefreshPoems()
{
    CollectedPoems.Reset();
    for (const auto& Pair : Catalog->GetRowMap())
    {
        const auto* Entry = reinterpret_cast<const FChronosJournalEntry*>(Pair.Value);
        if (!Entry->CollectionSlot.IsEmpty())
            CollectedPoems.Add(Pair.Key, UGameplayStatics::DoesSaveGameExist(Entry->CollectionSlot, 0));
    }
}

void UChronosJournal::RefreshTasks(const TArray<FChronosTask>& InTasks, const FString& InDay)
{
    TArray<FString> Lines;
    TArray<bool> Done;
    int32 Active = INDEX_NONE;
    for (const FChronosTask& Task : InTasks)
    {
        if (Active == INDEX_NONE && !Task.bCompleted && Task.Position.IsSet()) Active = Lines.Num();
        Lines.Add(Task.ProgressText.IsEmpty() ? Task.Description : Task.Description + TEXT("  (") + Task.ProgressText + TEXT(")"));
        Done.Add(Task.bCompleted);
    }
    if (Lines == TaskText && Done == TaskDone && Day == InDay && Active == ActiveTask) return;
    TaskText = MoveTemp(Lines);
    TaskDone = MoveTemp(Done);
    ActiveTask = Active;
    Day = InDay;
    if (Section == 0 && IsVisible()) DrawPages(false);
}

void UChronosJournal::SaveProgress() const
{
    if (auto* Loaded = ChronosPlayerSave::LoadedFrom(this); Loaded && !Loaded->JournalData.IsEmpty())
    {
        UGameplayStatics::SaveGameToMemory(Progress, Loaded->JournalData);
        return;
    }
    const FString SaveName = GetWorld()->WorldType == EWorldType::PIE ? FString(SaveSlot) + TEXT("_PIE") : SaveSlot;
    if (!UGameplayStatics::SaveGameToSlot(Progress, SaveName, 0))
        UE_LOG(LogTemp, Warning, TEXT("Chronos: no se pudo guardar el cuaderno."));
}

bool UChronosJournal::RecordDiscovery(FName Entry, const FText& Observation, bool bAlteredPerception)
{
    const auto* Record = Catalog->FindRow<FChronosJournalEntry>(Entry, TEXT("ChronosJournal"), false);
    if (!Record || Record->Section == EChronosJournalSection::Tasks || Record->Section == EChronosJournalSection::Poems)
        return false;
    auto& Discovery = Progress->Discoveries.FindOrAdd(Entry);
    if (!Observation.IsEmpty()) Discovery.Observation = Observation;
    Discovery.bAlteredPerception = bAlteredPerception;
    SaveProgress();
    if (Section == static_cast<int32>(Record->Section) && IsVisible())
    {
        DrawPages(false);
        Turning = .13f;
    }
    return true;
}

void UChronosJournal::ReadEntry(FName Id)
{
    if (Progress->ReadEntries.Contains(Id)) return;
    Progress->ReadEntries.Add(Id);
    SaveProgress();
}

void UChronosJournal::DrawPages(bool bResetScroll)
{
    JournalFocus.Set(nullptr);
    const int32 Zoom = Progress->ZoomStep;
    const double Magnification = 1. + .25 * Zoom;
    const FLinearColor BodyInk = Progress->bHighContrast ? FLinearColor::Black : Ink;
    const FLinearColor SubInk = Progress->bHighContrast ? FLinearColor::Black : Muted;
    const double Offsets[] = { Readers[0]->GetScrollOffset(), Readers[1]->GetScrollOffset() };
    for (UVerticalBox* Box : Pages) Box->ClearChildren();

    FSlateBrush BookBrush = BrushFor(LoadArt<UTexture2D>(FString::Printf(TEXT("BookStates/T_Base_%s"), BaseImages[Section])));
    BookBrush.SetUVRegion(FBox2f(FVector2f(.104f, .116f), FVector2f(.900f, .862f)));
    Book->SetBrush(BookBrush);
    for (int32 Index = 0; Index < Tabs.Num(); ++Index)
    {
        const bool bCurrent = Index == Section;
        Tabs[Index]->SetStyle(PaperButton(LoadArt<UTexture2D>(bCurrent
            ? TEXT("PapersAndTabs/Pestana_fisica_seleccionada") : TEXT("PapersAndTabs/Pestana_fisica_normal"))));
        TabLabels[Index]->SetColorAndOpacity(FSlateColor(bCurrent ? Ivory : Ink));
    }
    auto Label = [&](const FText& Value, double BaseSize, FLinearColor Color)
    {
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Text->SetText(Value);
        Text->SetFont(FSlateFontInfo(Serif.Get(), BaseSize * Magnification));
        Text->SetColorAndOpacity(FSlateColor(Color));
        Text->SetAutoWrapText(true);
        Text->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
        return Text;
    };
    auto Write = [&](int32 Side, const FText& Value, double BaseSize, double Bottom, FLinearColor Color)
    {
        if (Value.IsEmpty()) return;
        Pages[Side]->AddChildToVerticalBox(Label(Value, BaseSize, Color))->SetPadding(FMargin(0, 0, 16, Bottom));
    };

    if (Section == 0)
    {
        PageCount = FMath::Max(1, FMath::DivideAndRoundUp(TaskText.Num(), TasksPerSpread));
        Page = FMath::Clamp(Page, 0, PageCount - 1);
        int32 Completed = 0;
        for (bool bDone : TaskDone) Completed += bDone;
        Write(0, JournalText(TEXT("Tareas del turno")), 31, 5, BodyInk);
        Write(0, FText::FromString(Day), 18, 20, SubInk);
        Write(1, FText::FromString(FString::Printf(TEXT("%d de %d completadas"), Completed, TaskText.Num())), 23, 27, BodyInk);
        if (TaskText.IsEmpty()) Write(0, JournalText(TEXT("Todavía no hay tareas anotadas.")), 24, 8, SubInk);
        for (int32 Index = Page * TasksPerSpread; Index < FMath::Min(TaskText.Num(), (Page + 1) * TasksPerSpread); ++Index)
        {
            const int32 Side = Index % TasksPerSpread >= TasksPerSpread / 2 ? 1 : 0;
            UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
            Pages[Side]->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 0, 16, 18));
            UImage* Check = WidgetTree->ConstructWidget<UImage>();
            FSlateBrush Mark = BrushFor(LoadArt<UTexture2D>(TaskDone[Index] ? TEXT("Icons/casilla_marcada_128_RGBA") : TEXT("Icons/casilla_128_RGBA")));
            Mark.ImageSize = FVector2D(29 * Magnification);
            Check->SetBrush(Mark);
            Row->AddChildToHorizontalBox(Check)->SetVerticalAlignment(VAlign_Top);
            const FString Prefix = Index == ActiveTask ? TEXT("En curso · ") : TEXT("");
            UTextBlock* Text = Label(FText::FromString(Prefix + TaskText[Index]), 22, BodyInk);
            if (TaskDone[Index])
            {
                FSlateBrush Stroke = *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
                Stroke.ImageSize.Y = 1.6f;
                Stroke.TintColor = FSlateColor(Ink);
                Text->SetStrikeBrush(Stroke);
            }
            UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Text);
            TextSlot->SetPadding(FMargin(11, 0, 0, 0));
            TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        }
    }
    else
    {
        TArray<TPair<FName, const FChronosJournalEntry*>> Entries;
        for (const auto& Pair : Catalog->GetRowMap())
        {
            const auto* Entry = reinterpret_cast<const FChronosJournalEntry*>(Pair.Value);
            if (static_cast<int32>(Entry->Section) == Section) Entries.Emplace(Pair.Key, Entry);
        }
        Entries.Sort([](const auto& A, const auto& B)
        {
            return A.Value->Order == B.Value->Order ? A.Key.LexicalLess(B.Key) : A.Value->Order < B.Value->Order;
        });
        auto Unlocked = [&](FName Id)
        {
            return Section == static_cast<int32>(EChronosJournalSection::Poems)
                ? CollectedPoems.FindRef(Id) : Progress->Discoveries.Contains(Id);
        };
        PageCount = FMath::Max(1, Entries.Num());
        Page = FMath::Clamp(Page, 0, PageCount - 1);
        Write(0, JournalText(Sections[Section]), 30, 18, BodyInk);
        if (Entries.IsEmpty()) Write(0, JournalText(TEXT("Aún no hay entradas.")), 23, 0, SubInk);
        if (Entries.IsValidIndex(Page))
        {
            const auto& Item = Entries[Page];
            const bool bFound = Unlocked(Item.Key);
            const bool bUnread = Section == static_cast<int32>(EChronosJournalSection::Poems)
                && bFound && !Progress->ReadEntries.Contains(Item.Key);
            if (!bFound)
            {
                Write(0, JournalText(Section == static_cast<int32>(EChronosJournalSection::Poems)
                    ? TEXT("Hoja todavía sin encontrar.") : TEXT("Entrada sin descubrir.")), 23, 8, SubInk);
            }
            else
            {
                Write(0, Item.Value->Title, 25, 10, BodyInk);
                if (!Item.Value->Location.IsEmpty()) Write(0, Item.Value->Location, 17, 13, SubInk);
                const FChronosJournalDiscovery* Discovery = Progress->Discoveries.Find(Item.Key);
                const bool bVariant = Discovery && Discovery->bAlteredPerception && !Item.Value->BlurredSketch.IsNull();
                UTexture2D* Sketch = (bVariant ? Item.Value->BlurredSketch : Item.Value->Sketch).LoadSynchronous();
                if (Sketch)
                {
                    USizeBox* Frame = WidgetTree->ConstructWidget<USizeBox>();
                    Frame->SetHeightOverride(395.f);
                    UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>();
                    Fit->SetStretch(EStretch::ScaleToFit);
                    UImage* Image = WidgetTree->ConstructWidget<UImage>();
                    Image->SetBrushFromTexture(Sketch, true);
                    Fit->AddChild(Image);
                    Frame->AddChild(Fit);
                    Pages[0]->AddChildToVerticalBox(Frame)->SetPadding(FMargin(4, 0, 45, 8));
                }
                if (bUnread) Write(0, JournalText(TEXT("Primera lectura")), 16, 8, SubInk);
                TArray<FString> Verses;
                const bool bPoem = Section == static_cast<int32>(EChronosJournalSection::Poems);
                if (bPoem) Item.Value->Body.ToString().ParseIntoArray(Verses, TEXT("\n\n"), true);
                if (Verses.Num() >= 2)
                {
                    const int32 SplitAt = FMath::Max(1, Verses.Num() / 2 - (Verses.Num() >= 7 ? 1 : 0));
                    for (int32 Index = 0; Index < Verses.Num(); ++Index)
                        Write(Index < SplitAt ? 0 : 1, FText::FromString(Verses[Index]), 20, 14, BodyInk);
                }
                else Write(1, Item.Value->Body, 23, 12, BodyInk);
                if (Discovery) Write(1, Discovery->Observation, 23, 12, BodyInk);
                if (bPoem) ReadEntry(Item.Key);
            }
        }
    }
    PageCounter->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Page + 1, PageCount)));
    ZoomCounter->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), 100 + Zoom * 25)));
    Prev->SetIsEnabled(Page > 0);
    Next->SetIsEnabled(Page + 1 < PageCount);
    ZoomOut->SetIsEnabled(Zoom > 0);
    ZoomIn->SetIsEnabled(Zoom < 4);
    for (int32 Index = 0; Index < Readers.Num(); ++Index)
        Readers[Index]->SetScrollOffset(bResetScroll ? 0.f : Offsets[Index]);
}

void UChronosJournal::SelectSection(int32 Value)
{
    if (Section == Value) return;
    Section = (Value + UE_ARRAY_COUNT(Sections)) % UE_ARRAY_COUNT(Sections);
    Page = 0;
    if (Section == static_cast<int32>(EChronosJournalSection::Poems)) RefreshPoems();
    DrawPages(true);
    Turn();
}

void UChronosJournal::StepPage(int32 Direction)
{
    const int32 NewPage = FMath::Clamp(Page + Direction, 0, PageCount - 1);
    if (NewPage == Page) return;
    Page = NewPage;
    DrawPages(true);
    Turn();
    if (PageTurnSound) UGameplayStatics::PlaySound2D(this, PageTurnSound);
}

void UChronosJournal::SetZoomStep(int32 Value)
{
    Value = FMath::Clamp(Value, 0, 4);
    if (Value == Progress->ZoomStep) return;
    Progress->ZoomStep = Value;
    DrawPages(false);
    SaveProgress();
}

void UChronosJournal::Turn()
{
    Turning = .13f;
}

void UChronosJournal::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    JournalFocus.Set(JournalButtons, GetOwningPlayer());
    if (InputHints)
    {
        const auto* Pad = UChronosGamepad::Get(GetOwningPlayer());
        const int32 Device = Pad && Pad->IsGamepadActive() ? (Pad->IsPlayStation() ? 1 : 2) : 0;
        if (Device != LastHintDevice)
        {
            LastHintDevice = Device;
            const TCHAR* Hint[] = {
                TEXT("Q / E   Sección     ·     A / D   Página     ·     Rueda   Desplazar     ·     + / -   Zoom"),
                TEXT("L1 / R1   Sección     ·     L2 / R2   Página     ·     Stick derecho   Desplazar     ·     Cuadrado / Triángulo   Zoom     ·     Círculo   Cerrar"),
                TEXT("LB / RB   Sección     ·     LT / RT   Página     ·     Stick derecho   Desplazar     ·     X / Y   Zoom     ·     B   Cerrar")
            };
            InputHints->SetText(JournalText(Hint[Device]));
        }
    }
    if (Opening > 0.f)
    {
        Opening = FMath::Max(0.f, Opening - DeltaTime);
        Cover->SetRenderOpacity(Opening / .22f);
        Canvas->SetRenderOpacity(1.f - .35f * Opening / .22f);
        if (Opening <= 0.f) Cover->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (Turning > 0.f)
    {
        Turning = FMath::Max(0.f, Turning - DeltaTime);
        for (UScrollBox* Reader : Readers) Reader->SetRenderOpacity(1.f - .27f * Turning / .13f);
    }
}

FReply UChronosJournal::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    const FKey Key = Event.GetKey();
    if (Event.IsRepeat() && Key.IsGamepadKey()) return FReply::Unhandled();
    if (Key == CloseKey || Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
        ClickClose();
    else if (Key == EKeys::Q || Key == EKeys::Gamepad_LeftShoulder) SelectSection(Section - 1);
    else if (Key == EKeys::E || Key == EKeys::Gamepad_RightShoulder) SelectSection(Section + 1);
    else if (Key == EKeys::A || Key == EKeys::Gamepad_LeftTrigger) StepPage(-1);
    else if (Key == EKeys::D || Key == EKeys::Gamepad_RightTrigger) StepPage(1);
    else if (Key == EKeys::Equals || Key == EKeys::Add || Key == EKeys::Gamepad_FaceButton_Top) ClickZoomIn();
    else if (Key == EKeys::Hyphen || Key == EKeys::Subtract || Key == EKeys::Gamepad_FaceButton_Left) ClickZoomOut();
    else if (Key == EKeys::R) ClickReset();
    else if (Key == EKeys::PageUp || Key == EKeys::PageDown)
    {
        const float Delta = Key == EKeys::PageUp ? -230.f : 230.f;
        for (UScrollBox* Reader : Readers) Reader->SetScrollOffset(Reader->GetScrollOffset() + Delta);
    }
    else return Super::NativeOnPreviewKeyDown(Geometry, Event);
    return FReply::Handled();
}

FReply UChronosJournal::NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event)
{
    if (const auto* Pad = UChronosGamepad::Get(GetOwningPlayer()); Pad && Pad->IsGamepadActive() && Tabs.IsValidIndex(Section))
        return FReply::Handled().SetUserFocus(Tabs[Section]->TakeWidget());
    return Super::NativeOnFocusReceived(Geometry, Event);
}

FReply UChronosJournal::NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event)
{
    if (Event.GetKey() != EKeys::Gamepad_RightY || FMath::Abs(Event.GetAnalogValue()) < .32f)
        return Super::NativeOnAnalogValueChanged(Geometry, Event);
    const float Delta = -Event.GetAnalogValue() * 580.f * GetWorld()->GetDeltaSeconds();
    for (UScrollBox* Reader : Readers)
        Reader->SetScrollOffset(FMath::Clamp(Reader->GetScrollOffset() + Delta, 0.f, Reader->GetScrollOffsetOfEnd()));
    return FReply::Handled();
}

FReply UChronosJournal::NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
    SetZoomStep(Progress->ZoomStep + (Event.GetWheelDelta() > 0 ? 1 : -1));
    return FReply::Handled();
}

void UChronosJournal::ClickTasks() { SelectSection(0); }
void UChronosJournal::ClickBestiary() { SelectSection(1); }
void UChronosJournal::ClickAppearances() { SelectSection(2); }
void UChronosJournal::ClickHerbarium() { SelectSection(3); }
void UChronosJournal::ClickPoems() { SelectSection(4); }
void UChronosJournal::ClickNotes() { SelectSection(5); }
void UChronosJournal::ClickPrev() { StepPage(-1); }
void UChronosJournal::ClickNext() { StepPage(1); }
void UChronosJournal::ClickZoomIn() { SetZoomStep(Progress->ZoomStep + 1); }
void UChronosJournal::ClickZoomOut() { SetZoomStep(Progress->ZoomStep - 1); }
void UChronosJournal::ClickReset() { SetZoomStep(0); }
void UChronosJournal::ClickContrast()
{
    Progress->bHighContrast = !Progress->bHighContrast;
    DrawPages(false);
    SaveProgress();
}
void UChronosJournal::ClickClose() { bCloseRequested = true; }
