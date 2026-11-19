#include "ChronosInventory.h"
#include "ChronosContext.h"
#include "ChronosControls.h"
#include "ChronosFridge.h"
#include "ChronosGamepad.h"
#include "ChronosMenuFocus.h"
#include "ChronosProgress.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    TArray<TWeakObjectPtr<UChronosInventory>> OpenInventories;
    const FLinearColor InventoryGold(.88f, .76f, .59f);
}

bool UChronosInventory::IsOpen(const UObject* Context)
{
    return Context && OpenInventories.ContainsByPredicate([&](const auto& Entry)
    { return Entry.IsValid() && Entry->GetWorld() == Context->GetWorld() && Entry->IsInViewport(); });
}

void UChronosInventory::Open()
{
    auto* PC = GetOwningPlayer();
    if (!PC || IsInViewport()) return;
    SetIsFocusable(true);
    if (const auto* Progress = UChronosProgressState::Get(this))
        for (const auto& Item : Progress->Data.Items) if (Item.Value > 0) Items.Add(Item.Key);
    Items.Sort(FNameLexicalLess());
    bWasCursorVisible = PC->bShowMouseCursor;
    PC->SetIgnoreMoveInput(true);
    PC->SetIgnoreLookInput(true);
    bLocksInput = true;
    OpenInventories.AddUnique(this);
    AddToViewport(100);
    FInputModeUIOnly Input;
    Input.SetWidgetToFocus(Items.IsEmpty() ? Footer.Last().ToSharedRef() : Cells[0].ToSharedRef());
    Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(Input);
    PC->bShowMouseCursor = true;
    Select(0);
}

void UChronosInventory::CloseForModal(const UObject* Context)
{
    if (!Context) return;
    const auto Existing = OpenInventories;
    for (const auto& Entry : Existing)
        if (auto* Widget = Entry.Get(); Widget && Widget->GetWorld() == Context->GetWorld()) Widget->Close();
}

FName UChronosInventory::GetSelectedItem() const
{
    return Items.IsValidIndex(Selected) ? Items[Selected] : NAME_None;
}

FText UChronosInventory::ItemName(FName Item) const
{
    return FText::FromString(Item.IsNone() ? TEXT("Inventario vacío") : Item == TEXT("BlackCrow") ? TEXT("Black Crow")
        : Item == TEXT("WhiteCrow") ? TEXT("White Crow") : Item.ToString());
}

FText UChronosInventory::Description() const
{
    const auto Item = GetSelectedItem();
    return FText::FromString(Item == TEXT("BlackCrow")
        ? TEXT("Cerveza artesanal lager.\n\n473 mL · 5.2 % alc. vol.\n\nProduce mareo y reduce temporalmente la velocidad. El temblor y la lentitud aumentan al beber varias Black Crow seguidas.")
        : Item == TEXT("WhiteCrow")
        ? TEXT("Cerveza artesanal lager sin alcohol.\n\n473 mL · 0.0 % alc. vol.\n\nRecupera por completo la resistencia del personaje.")
        : Item.IsNone() ? TEXT("Los objetos que guardes aparecerán aquí.") : TEXT("Objeto guardado en el inventario."));
}

const FSlateBrush* UChronosInventory::ItemImage(FName Item) const
{
    if (const auto* Image = Images.Find(Item)) return Image;
    return &Raven;
}

FKey UChronosInventory::ActionKey(FName Name, FKey Fallback) const
{
    return UChronosContext::ActionKey(GetOwningPlayer(), Name, Fallback);
}

TSharedRef<SWidget> UChronosInventory::ActionCaption(const TCHAR* Label, FName Action, FKey Keyboard, FKey Controller) const
{
    const auto Key = [this, Action, Keyboard, Controller]
    {
        const auto* Pad = UChronosGamepad::Get(GetOwningPlayer());
        if (Action.IsNone()) return Pad && Pad->IsGamepadActive() ? Controller : Keyboard;
        return ActionKey(Action, Keyboard);
    };
    auto* Serif = LoadObject<UFont>(nullptr, TEXT("/Game/assets/Cementerio/Tipografia/TrajanPro3SemiBold_Font.TrajanPro3SemiBold_Font"));
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
        [SNew(SBox).WidthOverride_Lambda([Key] { return Key().IsGamepadKey() ? 32.f
            : FMath::Clamp(Key().GetDisplayName().ToString().Len() * 13.f + 8.f, 32.f, 100.f); }).HeightOverride(32)
         [SNew(SOverlay)
          + SOverlay::Slot()[SNew(SImage)
            .Image_Lambda([this, Key] { const auto* Pad = UChronosGamepad::Get(GetOwningPlayer());
                const auto* Glyph = ChronosControls::Glyph(Key(), Pad && Pad->IsPlayStation()); return Glyph ? Glyph : &Keycap; })]
          + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
            [SNew(STextBlock).Font(FSlateFontInfo(Serif, 17)).ColorAndOpacity(InventoryGold)
             .Visibility_Lambda([this, Key] { const auto* Pad = UChronosGamepad::Get(GetOwningPlayer());
                 return ChronosControls::Glyph(Key(), Pad && Pad->IsPlayStation()) ? EVisibility::Collapsed : EVisibility::Visible; })
             .Text_Lambda([this, Key] { const auto* Pad = UChronosGamepad::Get(GetOwningPlayer()); return Pad ? Pad->Prompt(Key()) : Key().GetDisplayName(); })]]]]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [SNew(STextBlock).Text(FText::FromString(Label)).Font(FSlateFontInfo(Serif, 20)).ColorAndOpacity(InventoryGold)];
}

TSharedRef<SWidget> UChronosInventory::RebuildWidget()
{
    Cells.Reset();
    Footer.Reset();
    const auto LoadImage = [this](FSlateBrush& Brush, const TCHAR* Path)
    {
        if (auto* Texture = LoadObject<UTexture2D>(nullptr, Path))
        {
            Textures.Add(Texture);
            Brush.SetResourceObject(Texture);
            Brush.DrawAs = ESlateBrushDrawType::Image;
            Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
        }
    };
    LoadImage(Raven, TEXT("/Game/TheChronosCurse_HUD_Assets/Icons/cuervo_128_RGBA.cuervo_128_RGBA"));
    LoadImage(Keycap, TEXT("/Game/TheChronosCurse_HUD_Assets/Panels/T_UI_Control_Keycap_192.T_UI_Control_Keycap_192"));
    LoadImage(Images.FindOrAdd(TEXT("BlackCrow")), TEXT("/Game/Interactivo/bp_fridge/UI/T_BlackCrow_Inventory.T_BlackCrow_Inventory"));
    LoadImage(Images.FindOrAdd(TEXT("WhiteCrow")), TEXT("/Game/Interactivo/bp_fridge/UI/T_WhiteCrow_Inventory.T_WhiteCrow_Inventory"));
    auto* Serif = LoadObject<UFont>(nullptr, TEXT("/Game/assets/Cementerio/Tipografia/TrajanPro3SemiBold_Font.TrajanPro3SemiBold_Font"));
    const FSlateFontInfo TitleFont(Serif, 36), NameFont(Serif, 27), BodyFont(Serif, 19), SmallFont(Serif, 16);
    auto Grid = SNew(SVerticalBox);
    for (int32 Row = 0; Row < 3; ++Row)
    {
        auto Line = SNew(SHorizontalBox);
        for (int32 Column = 0; Column < 3; ++Column)
        {
            const int32 Cell = Row * 3 + Column;
            auto Button = SNew(SChronosMenuButton).ButtonStyle(&ChronosMenuArt::ButtonStyle()).ContentPadding(10).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
                .IsEnabled_Lambda([this, Cell] { return Items.IsValidIndex(Page * 9 + Cell); })
                .OnHovered_Lambda([this, Cell] { if (Items.IsValidIndex(Page * 9 + Cell)) Select(Page * 9 + Cell); })
                .OnClicked_Lambda([this, Cell] { Select(Page * 9 + Cell, true); return FReply::Handled(); })
                [SNew(SOverlay)
                 + SOverlay::Slot().Padding(5, 0, 5, 24)
                   [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                    [SNew(SImage).Image_Lambda([this, Cell] { const int32 Index = Page * 9 + Cell; return Items.IsValidIndex(Index) ? ItemImage(Items[Index]) : nullptr; })
                     .Visibility_Lambda([this, Cell] { return Items.IsValidIndex(Page * 9 + Cell) ? EVisibility::HitTestInvisible : EVisibility::Hidden; })]]
                 + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom)
                   [SNew(STextBlock).Font(BodyFont).ColorAndOpacity(InventoryGold).Text_Lambda([this, Cell]
                    { const int32 Index = Page * 9 + Cell; const auto* Progress = UChronosProgressState::Get(this);
                      return FText::FromString(Items.IsValidIndex(Index) && Progress ? FString::Printf(TEXT("× %d"), Progress->Count(Items[Index])) : FString()); })]];
            Cells.Add(Button);
            Line->AddSlot().FillWidth(1).Padding(5)[SNew(SBox).HeightOverride(158)[Button]];
        }
        Grid->AddSlot().AutoHeight()[Line];
    }
    const auto PageButton = [this, SmallFont](const TCHAR* Label, int32 Direction)
    {
        return SNew(SChronosMenuButton).ButtonStyle(&ChronosMenuArt::ButtonStyle()).HAlign(HAlign_Center)
            .OnClicked(FOnClicked::CreateUObject(this, &UChronosInventory::ChangePage, Direction))
            [SNew(STextBlock).Text(FText::FromString(Label)).Font(SmallFont).ColorAndOpacity(InventoryGold)];
    };
    auto Actions = SNew(SHorizontalBox);
    const auto FooterButton = [this, Actions](TSharedRef<SWidget> Caption, FOnClicked OnClick)
    {
        auto Button = SNew(SChronosMenuButton).ButtonStyle(&ChronosMenuArt::ButtonStyle()).HAlign(HAlign_Center).OnClicked(OnClick)[Caption];
        Footer.Add(Button);
        Actions->AddSlot().FillWidth(1).Padding(6)[SNew(SBox).HeightOverride(58)[Button]];
        return Button;
    };
    auto UseButton = FooterButton(ActionCaption(TEXT("Usar"), TEXT("Chronos.DrinkItem"), EKeys::X, EKeys::Gamepad_RightTrigger), FOnClicked::CreateUObject(this, &UChronosInventory::Use));
    UseButton->SetEnabled(TAttribute<bool>::CreateLambda([this]
    {
        const auto* Session = GetWorld()->GetSubsystem<UChronosFridgeSession>();
        return !GetSelectedItem().IsNone() && (GetSelectedItem() == TEXT("BlackCrow") || GetSelectedItem() == TEXT("WhiteCrow")) && Session && !Session->HasCan();
    }));
    FooterButton(ActionCaption(TEXT("Examinar"), NAME_None, EKeys::Enter, EKeys::Gamepad_FaceButton_Top), FOnClicked::CreateUObject(this, &UChronosInventory::Examine));
    FooterButton(ActionCaption(TEXT("Cerrar"), TEXT("Chronos.Inventory"), EKeys::I, EKeys::Gamepad_LeftShoulder), FOnClicked::CreateLambda([this] { Close(); return FReply::Handled(); }));
    return SNew(SOverlay)
        + SOverlay::Slot()[SNew(SBorder).BorderBackgroundColor(FLinearColor(0, 0, 0, .65f))]
        + SOverlay::Slot().Padding(64, 40)
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
         [SNew(SBox).WidthOverride(1320).HeightOverride(860)
          [SNew(SBorder).BorderImage(&ChronosMenuArt::OptionsPanel()).Padding(54, 36)
           [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18)
              [SNew(SHorizontalBox)
               + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(92).HeightOverride(90)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&Raven)]]]
               + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(20, 0)[SNew(STextBlock).Text(FText::FromString(TEXT("INVENTARIO"))).Font(TitleFont).ColorAndOpacity(InventoryGold)]
               + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                 [SNew(SVerticalBox)
                  + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[SNew(STextBlock).Text(FText::FromString(TEXT("Monedero"))).Font(SmallFont).ColorAndOpacity(InventoryGold)]
                  + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[SNew(STextBlock).Font(NameFont).ColorAndOpacity(InventoryGold).Text_Lambda([this]
                    { const auto* Progress = UChronosProgressState::Get(this); return FText::FromString(FString::Printf(TEXT("$%lld.00"), Progress ? Progress->GetDollars() : 0LL)); })]]]
            + SVerticalBox::Slot().FillHeight(1)
              [SNew(SHorizontalBox)
               + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 25, 0)
                 [SNew(SVerticalBox)
                  + SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this] { return bExamining ? EVisibility::Collapsed : EVisibility::Visible; })[Grid]]
                  + SVerticalBox::Slot().AutoHeight().Padding(0, 10)
                    [SNew(SBox).Visibility_Lambda([this] { return Items.Num() > 9 ? EVisibility::Visible : EVisibility::Collapsed; })
                    [SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1)[PageButton(TEXT("‹ Anterior"), -1)]
                     + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(15, 0)[SNew(STextBlock).Font(SmallFont).ColorAndOpacity(InventoryGold).Text_Lambda([this]
                       { return FText::FromString(FString::Printf(TEXT("%d / %d"), Page + 1, FMath::Max(1, FMath::DivideAndRoundUp(Items.Num(), 9)))); })]
                     + SHorizontalBox::Slot().FillWidth(1)[PageButton(TEXT("Siguiente ›"), 1)]]]
                  + SVerticalBox::Slot().FillHeight(1).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(10)
                    [SNew(STextBlock).Font(BodyFont).ColorAndOpacity(InventoryGold).AutoWrapText(true).Text_Lambda([this]
                     { return FText::FromString(Items.IsEmpty() ? TEXT("Bolsillos vacíos") : bExamining ? TEXT("Examinando objeto\n\nPulsa Examinar de nuevo para volver a la cuadrícula.") : TEXT("Selecciona un objeto para ver sus propiedades.")); })]]
               + SHorizontalBox::Slot().FillWidth(1).Padding(25, 0, 0, 0)
                 [SNew(SVerticalBox)
                  + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(STextBlock).Font(NameFont).ColorAndOpacity(InventoryGold).Text_Lambda([this] { return ItemName(GetSelectedItem()); })]
                  + SVerticalBox::Slot().AutoHeight().Padding(0, 14)
                    [SNew(SBox).HeightOverride_Lambda([this] { return bExamining ? 350.f : 260.f; })
                     [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image_Lambda([this] { return ItemImage(GetSelectedItem()); })]]]
                  + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox) + SScrollBox::Slot()[SNew(STextBlock).Font(BodyFont).ColorAndOpacity(InventoryGold).AutoWrapText(true).Text_Lambda([this] { return Description(); })]]]]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 0)[Actions]]]]];
}

void UChronosInventory::Select(int32 Index, bool bFocus)
{
    if (!Items.IsValidIndex(Index)) return;
    Selected = Index;
    Page = Selected / 9;
    for (int32 Cell = 0; Cell < Cells.Num(); ++Cell) Cells[Cell]->SetMenuStyle(ChronosMenuArt::ButtonStyle(Page * 9 + Cell == Selected));
    if (bFocus) FSlateApplication::Get().SetUserFocus(GetOwningPlayer()->GetLocalPlayer()->GetControllerId(), Cells[Selected % 9], EFocusCause::Navigation);
}

FReply UChronosInventory::ChangePage(int32 Direction)
{
    const int32 Pages = FMath::Max(1, FMath::DivideAndRoundUp(Items.Num(), 9));
    Select(FMath::Clamp(Page + Direction, 0, Pages - 1) * 9, true);
    return FReply::Handled();
}

void UChronosInventory::Navigate(int32 Step)
{
    auto& Slate = FSlateApplication::Get();
    const int32 User = GetOwningPlayer()->GetLocalPlayer()->GetControllerId();
    const int32 FocusedFooter = Footer.IndexOfByPredicate([](const auto& Button) { return Button->HasAnyUserFocus().IsSet(); });
    if (FocusedFooter != INDEX_NONE)
    {
        if (Step == -3 && !Items.IsEmpty()) Select(Selected, true);
        else if (FMath::Abs(Step) == 1)
        {
            int32 Next = FocusedFooter;
            for (int32 Attempt = 0; Attempt < Footer.Num(); ++Attempt)
            {
                Next = (Next + Footer.Num() + Step) % Footer.Num();
                if (Footer[Next]->IsEnabled()) { Slate.SetUserFocus(User, Footer[Next], EFocusCause::Navigation); break; }
            }
        }
    }
    else if (Step == 3 && Selected + 3 >= Items.Num())
        Slate.SetUserFocus(User, Footer[Footer[0]->IsEnabled() ? 0 : 2], EFocusCause::Navigation);
    else if (!Items.IsEmpty()) Select(FMath::Clamp(Selected + Step, 0, Items.Num() - 1), true);
}

FReply UChronosInventory::Examine()
{
    if (!Items.IsEmpty()) bExamining = !bExamining;
    return FReply::Handled();
}

FReply UChronosInventory::Use()
{
    if (auto* Session = GetWorld()->GetSubsystem<UChronosFridgeSession>(); Session && Session->EquipItem(GetSelectedItem()))
    {
        Close();
        if (auto* Beer = Session->GetHeld()) Beer->Drink();
    }
    return FReply::Handled();
}

FReply UChronosInventory::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    const auto Key = Event.GetKey();
    const FKey InventoryKey = ActionKey(TEXT("Chronos.Inventory"), EKeys::I);
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == InventoryKey)
    {
        if (!Event.IsRepeat()) { if (bExamining) bExamining = false; else Close(); }
        return FReply::Handled();
    }
    if (!Event.IsRepeat() && Key == ActionKey(TEXT("Chronos.DrinkItem"), EKeys::X)) return Use();
    const bool bGridFocus = Cells.ContainsByPredicate([](const auto& Cell) { return Cell->HasAnyUserFocus().IsSet(); });
    if (!Event.IsRepeat() && (Key == EKeys::Gamepad_FaceButton_Top || (Key == EKeys::Enter && bGridFocus))) return Examine();
    if (Key == EKeys::PageDown) return ChangePage(1);
    if (Key == EKeys::PageUp) return ChangePage(-1);
    int32 Step = 0;
    if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left) Step = -1;
    if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right) Step = 1;
    if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up) Step = -3;
    if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down) Step = 3;
    if (Step) { Navigate(Step); return FReply::Handled(); }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply UChronosInventory::NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event)
{
    const auto Key = Event.GetKey();
    if (Key != EKeys::Gamepad_LeftX && Key != EKeys::Gamepad_LeftY) return Super::NativeOnAnalogValueChanged(Geometry, Event);
    const float Now = GetWorld()->GetTimeSeconds();
    if (FMath::Abs(Event.GetAnalogValue()) < .35f) StickRepeatAt = 0.f;
    else if (FMath::Abs(Event.GetAnalogValue()) > .6f && Now >= StickRepeatAt && !Items.IsEmpty())
    {
        const int32 Step = Key == EKeys::Gamepad_LeftX ? (Event.GetAnalogValue() > 0 ? 1 : -1) : (Event.GetAnalogValue() > 0 ? -3 : 3);
        Navigate(Step);
        StickRepeatAt = Now + .25f;
    }
    return FReply::Handled();
}

void UChronosInventory::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    for (int32 Cell = 0; Cell < Cells.Num(); ++Cell)
        if (Cells[Cell]->HasAnyUserFocus().IsSet() && Items.IsValidIndex(Page * 9 + Cell) && Selected != Page * 9 + Cell) Select(Page * 9 + Cell);
}

void UChronosInventory::RestoreInput()
{
    OpenInventories.Remove(this);
    if (bLocksInput)
    {
        if (auto* PC = GetOwningPlayer())
        {
            PC->SetIgnoreMoveInput(false);
            PC->SetIgnoreLookInput(false);
            PC->bShowMouseCursor = bWasCursorVisible;
            FInputModeGameOnly Input;
            Input.SetConsumeCaptureMouseDown(false);
            PC->SetInputMode(Input);
        }
        bLocksInput = false;
    }
}

void UChronosInventory::Close() { RestoreInput(); RemoveFromParent(); }
void UChronosInventory::NativeDestruct() { RestoreInput(); Super::NativeDestruct(); }
