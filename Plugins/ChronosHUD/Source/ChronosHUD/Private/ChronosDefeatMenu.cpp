#include "ChronosDefeatMenu.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "ChronosDefeatMenu"

void UChronosDefeatMenu::NativeConstruct()
{
    Super::NativeConstruct();
    auto* Canvas = Cast<UCanvasPanel>(GetRootWidget());
    auto* Title = Cast<UTextBlock>(GetWidgetFromName(TEXT("titulo")));
    auto* Reason = Cast<UTextBlock>(GetWidgetFromName(TEXT("subtitulo")));
    auto* Actions = Cast<UHorizontalBox>(GetWidgetFromName(TEXT("botones")));
    MainMenuButton = Cast<UButton>(GetWidgetFromName(TEXT("botonMenu")));
    ReplayButton = Cast<UButton>(GetWidgetFromName(TEXT("botonReintentar")));
    if (!WidgetTree || !Canvas || !Title || !Reason || !Actions || !MainMenuButton || !ReplayButton)
    {
        UE_LOG(LogTemp, Warning, TEXT("Chronos defeat layout incomplete: canvas=%d title=%d reason=%d actions=%d menu=%d replay=%d"),
            Canvas != nullptr, Title != nullptr, Reason != nullptr, Actions != nullptr, MainMenuButton != nullptr, ReplayButton != nullptr);
        return;
    }

    Title->RemoveFromParent();
    Reason->RemoveFromParent();
    Actions->RemoveFromParent();
    Canvas->ClearChildren();

    auto* Dim = WidgetTree->ConstructWidget<UBorder>();
    Dim->SetBrush(FSlateColorBrush(FLinearColor(.012f, .010f, .008f, .76f)));
    Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* DimSlot = Canvas->AddChildToCanvas(Dim);
    DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    DimSlot->SetOffsets(FMargin(0.f));

    auto* Scaler = WidgetTree->ConstructWidget<UScaleBox>();
    Scaler->SetStretch(EStretch::ScaleToFit);
    Scaler->SetStretchDirection(EStretchDirection::DownOnly);
    auto* ScaleSlot = Canvas->AddChildToCanvas(Scaler);
    ScaleSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    ScaleSlot->SetOffsets(FMargin(32.f));

    auto* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(800.f);
    auto* SizeSlot = CastChecked<UScaleBoxSlot>(Scaler->SetContent(Size));
    SizeSlot->SetHorizontalAlignment(HAlign_Center);
    SizeSlot->SetVerticalAlignment(VAlign_Center);
    auto* Frame = WidgetTree->ConstructWidget<UBorder>();
    Frame->SetBrush(ChronosMenuArt::OptionsPanel());
    Frame->SetPadding(FMargin(52.f, 47.f, 52.f, 42.f));
    Size->SetContent(Frame);
    auto* Column = WidgetTree->ConstructWidget<UVerticalBox>();
    Frame->SetContent(Column);

    auto* Font = LoadObject<UFont>(nullptr,
        TEXT("/Game/assets/Cementerio/Tipografia/TrajanPro3SemiBold_Font.TrajanPro3SemiBold_Font"));
    const auto Format = [Font](UTextBlock& Text, int32 Size, FColor Color)
    {
        if (Font) Text.SetFont(FSlateFontInfo(Font, Size));
        Text.SetColorAndOpacity(FSlateColor(FLinearColor(Color)));
        Text.SetJustification(ETextJustify::Center);
        Text.SetAutoWrapText(true);
    };
    Title->SetText(LOCTEXT("DefeatTitle", "Has perdido"));
    Format(*Title, 45, FColor(220, 182, 121));
    Column->AddChildToVerticalBox(Title);
    if (Reason->GetText().ToString().Equals(TEXT("Tu reputacion llego a 0"), ESearchCase::IgnoreCase))
        Reason->SetText(LOCTEXT("ReputationLost", "Tu reputación llegó a 0."));
    Format(*Reason, 21, FColor(244, 223, 190));
    Column->AddChildToVerticalBox(Reason)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));

    Actions->ClearChildren();
    Column->AddChildToVerticalBox(Actions)->SetPadding(FMargin(0.f, 29.f, 0.f, 0.f));
    for (auto* Button : {MainMenuButton.Get(), ReplayButton.Get()})
    {
        Button->SetStyle(ChronosMenuArt::ButtonStyle());
        if (auto* Label = Cast<UTextBlock>(Button->GetContent())) Format(*Label, 20, FColor(249, 226, 183));
        if (auto* ContentSlot = Cast<UButtonSlot>(Button->GetContentSlot()))
        {
            ContentSlot->SetPadding(FMargin(26.f, 14.f));
            ContentSlot->SetHorizontalAlignment(HAlign_Center);
            ContentSlot->SetVerticalAlignment(VAlign_Center);
        }
        auto* ActionSlot = Actions->AddChildToHorizontalBox(Button);
        ActionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        ActionSlot->SetPadding(FMargin(7.f, 0.f));
    }

    bTravelRequested = false;
    bNeedsInitialFocus = true;
    NavigationStick.Reset();
    SetIsFocusable(true);
}

void UChronosDefeatMenu::NativeDestruct()
{
    FocusGlow.Set(nullptr);
    for (const auto& Entry : HiddenObjectives)
        if (auto* Objective = Entry.Key.Get()) Objective->SetVisibility(Entry.Value);
    HiddenObjectives.Reset();
    Super::NativeDestruct();
}

void UChronosDefeatMenu::Select(int32 Index)
{
    Selection = Index;
    auto* Button = Selection == 0 ? MainMenuButton.Get() : ReplayButton.Get();
    if (Button) Button->SetUserFocus(GetOwningPlayer());
    FocusGlow.Set(Button);
}

void UChronosDefeatMenu::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (auto* Player = GetOwningPlayer(); bNeedsInitialFocus && Player)
    {
        bNeedsInitialFocus = false;
        FInputModeUIOnly Input;
        Input.SetWidgetToFocus(ReplayButton->TakeWidget());
        Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Player->SetInputMode(Input);
        Select(1);
        const auto ObjectiveClass = FindObject<UClass>(nullptr, TEXT("/Game/UI/WBP_Objetivo.WBP_Objetivo_C"));
        if (ObjectiveClass)
        {
            TArray<UUserWidget*> Objectives;
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Objectives, ObjectiveClass, true);
            for (auto* Objective : Objectives)
            {
                HiddenObjectives.Add(Objective, Objective->GetVisibility());
                Objective->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
    }
    if (MainMenuButton && MainMenuButton->HasUserFocus(GetOwningPlayer())) Selection = 0;
    else if (ReplayButton && ReplayButton->HasUserFocus(GetOwningPlayer())) Selection = 1;
    FocusGlow.Set(Selection == 0 ? MainMenuButton.Get() : ReplayButton.Get());
}

FReply UChronosDefeatMenu::NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event)
{
    if (auto* Button = Selection == 0 ? MainMenuButton.Get() : ReplayButton.Get())
        return FReply::Handled().SetUserFocus(Button->TakeWidget(), EFocusCause::SetDirectly);
    return Super::NativeOnFocusReceived(Geometry, Event);
}

FReply UChronosDefeatMenu::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    const FKey Key = Event.GetKey();
    if (Key == EKeys::Left || Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Up)
    {
        Select(0);
        return FReply::Handled();
    }
    if (Key == EKeys::Right || Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_DPad_Down)
    {
        Select(1);
        return FReply::Handled();
    }
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        if (auto* Button = Selection == 0 ? MainMenuButton.Get() : ReplayButton.Get();
            Button && !bTravelRequested && !Event.IsRepeat() && Button->GetIsEnabled())
        {
            bTravelRequested = true;
            Button->OnClicked.Broadcast();
        }
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply UChronosDefeatMenu::NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event)
{
    if (Event.GetKey() != EKeys::Gamepad_LeftX) return Super::NativeOnAnalogValueChanged(Geometry, Event);
    if (const int32 Direction = NavigationStick.Step(Event.GetAnalogValue())) Select(Direction < 0 ? 0 : 1);
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
