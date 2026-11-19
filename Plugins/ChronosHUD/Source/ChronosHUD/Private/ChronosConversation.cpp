#include "ChronosConversation.h"
#include "ChronosContext.h"
#include "ChronosInventory.h"
#include "TimerManager.h"

#include "ChronosMenuArt.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
    TArray<TWeakObjectPtr<UChronosConversation>> ActiveConversations;
    UFont* Serif()
    {
        return LoadObject<UFont>(nullptr, TEXT("/Game/assets/Cementerio/Tipografia/TrajanPro3SemiBold_Font.TrajanPro3SemiBold_Font"));
    }

    void FormatText(UTextBlock& Text, int32 Size)
    {
        Text.SetFont(FSlateFontInfo(Serif(), Size));
        Text.SetColorAndOpacity(FSlateColor(FLinearColor(.88f, .76f, .59f)));
        Text.SetShadowOffset(FVector2D(1.3, 1.3));
        Text.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, .9));
        Text.SetAutoWrapText(true);
        Text.SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

void UChronosConversation::BuildLegacyChoices()
{
    auto* Greeting = Cast<UTextBlock>(GetWidgetFromName(TEXT("GreetingOptionText")));
    auto* Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
    if (!Greeting || !Root) return;
    bLegacyChoices = true;
    if (GetWidgetFromName(TEXT("ChronosConversationFrame"))) return;
    const bool bRobert = GetWidgetFromName(TEXT("CemeteryOptionText")) != nullptr;
    const bool bViolet = GetWidgetFromName(TEXT("RitualOptionText")) != nullptr;
    LegacyReference = bRobert ? TEXT("RobertRef") : bViolet ? TEXT("VioletRef") : TEXT("VisitorRef");
    LegacyLastDays = {TEXT("LastGreetingDay"), bRobert ? FName(TEXT("LastCemeteryTalkDay")) : FName(TEXT("LastFamilyTalkDay")),
        bRobert ? FName(TEXT("LastRadioTalkDay")) : bViolet ? FName(TEXT("LastRitualTalkDay")) : FName(TEXT("LastFarewellDay"))};
    auto* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ChronosConversationFrame"));
    Frame->SetBrush(ChronosMenuArt::OptionsPanel());
    Frame->SetPadding(FMargin(30, 20));
    Frame->SetHorizontalAlignment(HAlign_Fill);
    auto* FrameSlot = Root->AddChildToCanvas(Frame);
    FrameSlot->SetAnchors(FAnchors(.12f, 1.f, .88f, 1.f));
    FrameSlot->SetAlignment(FVector2D(0, 1));
    FrameSlot->SetOffsets(FMargin(0, -38, 0, 360));
    auto* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
    Frame->SetContent(Layout);
    if (auto* Name = GetWidgetFromName(bRobert || bViolet ? TEXT("TextBlock_0") : TEXT("NameText")))
    {
        Name->RemoveFromParent();
        if (auto* NameText = Cast<UTextBlock>(Name))
        {
            NameText->SetText(FText::FromString(NameText->GetText().ToString().ToUpper()));
            NameText->SetJustification(ETextJustify::Center);
        }
        Layout->AddChildToVerticalBox(Name)->SetPadding(FMargin(0, 0, 0, 10));
    }
    DialogueScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ChronosDialogueScroll"));
    DialogueScroll->SetClipping(EWidgetClipping::ClipToBounds);
    DialogueScroll->SetAnimateWheelScrolling(true);
    DialogueScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
    auto* DialogueSlot = Layout->AddChildToVerticalBox(DialogueScroll);
    DialogueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    DialogueSlot->SetPadding(FMargin(8, 4, 8, 14));
    if (auto* Subtitle = GetWidgetFromName(TEXT("SubtitleText")))
    {
        Subtitle->RemoveFromParent();
        DialogueScroll->AddChild(Subtitle);
    }
    if (auto* OldSubtitle = GetWidgetFromName(TEXT("SubtitleBackdrop"))) OldSubtitle->SetVisibility(ESlateVisibility::Collapsed);
    auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    auto* ChoicesSize = WidgetTree->ConstructWidget<USizeBox>();
    ChoicesSize->SetHeightOverride(82.f);
    ChoicesSize->SetContent(Row);
    Layout->AddChildToVerticalBox(ChoicesSize)->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    const FName Names[] = {TEXT("GreetingOptionText"), bRobert ? FName(TEXT("CemeteryOptionText")) : FName(TEXT("FamilyOptionText")),
        bRobert ? FName(TEXT("RadioOptionText")) : bViolet ? FName(TEXT("RitualOptionText")) : FName(TEXT("FarewellOptionText")),
        bRobert || bViolet ? FName(TEXT("TextBlock_4")) : FName(TEXT("LeaveOptionText"))};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        auto* Text = Index == 1 && !bRobert && !bViolet
            ? Cast<UTextBlock>(GetWidgetFromName(TEXT("QuestionOptionText")))
            : Cast<UTextBlock>(GetWidgetFromName(Names[Index]));
        check(Text);
        Text->RemoveFromParent();
        FString Caption = Text->GetText().ToString();
        if (Caption.StartsWith(TEXT("[")))
        {
            const int32 End = Caption.Find(TEXT("]"));
            if (End != INDEX_NONE) Caption = Caption.Mid(End + 1).TrimStart();
        }
        Text->SetText(FText::FromString(Caption));
        auto* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
            *FString::Printf(TEXT("ChronosChoice%d"), Index + 1));
        Button->AddChild(Text);
        Button->OnClicked.AddUniqueDynamic(this, &UChronosConversation::ExecuteLegacyChoice);
        auto* ChoiceSlot = Row->AddChildToHorizontalBox(Button);
        ChoiceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        ChoiceSlot->SetVerticalAlignment(VAlign_Fill);
    }
    if (auto* Old = GetWidgetFromName(TEXT("OptionsBackdrop"))) Old->SetVisibility(ESlateVisibility::Collapsed);
    else if (auto* Column = GetWidgetFromName(TEXT("VerticalBox_61"))) Column->SetVisibility(ESlateVisibility::Collapsed);
}

void UChronosConversation::InvokeLegacyKey(FKey Key)
{
    const FKeyEvent Event(Key, FModifierKeysState(), 0, false, 0, 0);
    OnKeyDown(GetCachedGeometry(), Event);
    bConfirmed = false;
    RefreshLegacyChoiceStates();
}

void UChronosConversation::ExecuteLegacyChoice()
{
    if (!bConfirmed)
        for (int32 Index = 0; Index < Choices.Num(); ++Index)
            if (Choices[Index]->IsHovered()) { Selected = Index; break; }
    static const FKey Keys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Escape};
    if (Selected >= 0 && Selected < UE_ARRAY_COUNT(Keys)) InvokeLegacyKey(Keys[Selected]);
}

void UChronosConversation::RefreshLegacyChoiceStates()
{
    if (!bLegacyChoices) return;
    const auto* Ref = FindFProperty<FObjectPropertyBase>(GetClass(), LegacyReference);
    auto* Actor = Ref ? Ref->GetObjectPropertyValue_InContainer(this) : nullptr;
    if (!IsValid(Actor)) return;
    const auto* Day = FindFProperty<FIntProperty>(Actor->GetClass(), TEXT("CurrentDay"));
    TOptional<int32> Current;
    if (Day) Current = Day->GetPropertyValue_InContainer(Actor);
    else if (auto* Method = Actor->FindFunction(TEXT("GetCurrentGameDay")))
    {
        FStructOnScope Arguments(Method);
        Actor->ProcessEvent(Method, Arguments.GetStructMemory());
        if (const auto* Value = FindFProperty<FIntProperty>(Method, TEXT("CurrentDay")))
            Current = Value->GetPropertyValue_InContainer(Arguments.GetStructMemory());
    }
    if (!Current.IsSet()) return;
    for (int32 Index = 0; Index < LegacyLastDays.Num() && Index < Choices.Num(); ++Index)
        if (const auto* Last = FindFProperty<FIntProperty>(Actor->GetClass(), LegacyLastDays[Index]))
            Choices[Index]->SetIsEnabled(Last->GetPropertyValue_InContainer(Actor) != Current.GetValue());
    if (!SelectedButton()) MoveSelection(1);
}

bool UChronosConversation::IsActive(const UObject* Context)
{
    const auto* World = Context ? Context->GetWorld() : nullptr;
    return World && ActiveConversations.ContainsByPredicate([World](const auto& Entry)
    {
        const auto* Widget = Entry.Get();
        return Widget && Widget->GetWorld() == World && Widget->IsInViewport() && Widget->IsVisible();
    });
}

void UChronosConversation::NativeConstruct()
{
    UChronosInventory::CloseForModal(this);
    Super::NativeConstruct();
    BuildLegacyChoices();
    ActiveConversations.AddUnique(this);
    SetIsFocusable(true);
    Choices.Reset();
    Selected = 0;
    OpenElapsed = 0.f;
    AnalogRepeatAt = 0.f;
    bConfirmed = false;
    if (WidgetTree)
    {
        WidgetTree->ForEachWidget([this](UWidget* Widget)
        {
            if (auto* Text = Cast<UTextBlock>(Widget))
            {
                const bool bBody = Widget->GetFName() == TEXT("texto") || Widget->GetFName() == TEXT("SubtitleText");
                FormatText(*Text, bBody ? 22 : 17);
                Text->SetWrapTextAt(0.f);
                if (!bBody) Text->SetJustification(ETextJustify::Center);
                if (Widget->GetFName() == TEXT("NameText") || Widget->GetFName() == TEXT("TextBlock_0"))
                    FormatText(*Text, 24);
            }
            if (auto* Button = Cast<UButton>(Widget))
            {
                Button->SetStyle(ChronosMenuArt::ButtonStyle());
                if (auto* Content = Button->GetContent())
                    if (auto* ContentSlot = Cast<UButtonSlot>(Content->Slot))
                    {
                        ContentSlot->SetPadding(FMargin(24, 11));
                        ContentSlot->SetHorizontalAlignment(HAlign_Center);
                        ContentSlot->SetVerticalAlignment(VAlign_Center);
                    }
                if (auto* ButtonSlot = Cast<UHorizontalBoxSlot>(Button->Slot))
                {
                    ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
                    ButtonSlot->SetPadding(FMargin(8, 12, 8, 0));
                }
                Choices.Add(Button);
            }
        });
    }
    if (auto* Border = Cast<UBorder>(GetWidgetFromName(TEXT("fondo"))))
    {
        Border->SetBrush(ChronosMenuArt::OptionsPanel());
        Border->SetBrushColor(FLinearColor::White);
        Border->SetPadding(FMargin(34, 22));
        Border->SetHorizontalAlignment(HAlign_Fill);
        if (auto* BorderSlot = Cast<UCanvasPanelSlot>(Border->Slot))
        {
            BorderSlot->SetAnchors(FAnchors(.19f, 1.f, .81f, 1.f));
            BorderSlot->SetAlignment(FVector2D(0, 1));
            BorderSlot->SetOffsets(FMargin(0, -38, 0, Choices.Num() > 1 ? 175 : 156));
        }
    }
    if (auto* Row = GetWidgetFromName(TEXT("botones")))
        if (auto* RowSlot = Cast<UVerticalBoxSlot>(Row->Slot))
            RowSlot->SetHorizontalAlignment(Choices.Num() > 1 ? HAlign_Fill : HAlign_Right);
    if (auto* PC = GetOwningPlayer())
    {
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
        bLocksMovement = true;
    }
    MoveSelection(0);
    RefreshLegacyChoiceStates();
    if (auto* PC = GetOwningPlayer())
    {
        FInputModeUIOnly Input;
        Input.SetWidgetToFocus(SelectedButton() ? SelectedButton()->TakeWidget() : TakeWidget());
        Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(Input);
        PC->bShowMouseCursor = true;
    }
}

void UChronosConversation::NativeDestruct()
{
    ActiveConversations.Remove(this);
    if (bLocksMovement)
    {
        if (auto* PC = GetOwningPlayer())
        {
            PC->SetIgnoreMoveInput(false);
            PC->SetIgnoreLookInput(false);
            if (!IsActive(this))
            {
                FInputModeGameOnly Input;
                Input.SetConsumeCaptureMouseDown(false);
                PC->SetInputMode(Input);
                PC->bShowMouseCursor = false;
            }
        }
        bLocksMovement = false;
    }
    Super::NativeDestruct();
}

UButton* UChronosConversation::SelectedButton() const
{
    return Choices.IsValidIndex(Selected) && Choices[Selected]->IsVisible() && Choices[Selected]->GetIsEnabled()
        ? Choices[Selected].Get() : nullptr;
}

void UChronosConversation::MoveSelection(int32 Direction)
{
    if (Choices.IsEmpty()) return;
    for (int32 Attempt = 0; Attempt < Choices.Num(); ++Attempt)
    {
        Selected = (Selected + Choices.Num() + Direction) % Choices.Num();
        if (auto* Button = SelectedButton())
        {
            Button->SetUserFocus(GetOwningPlayer());
            return;
        }
        Direction = Direction ? Direction : 1;
    }
}

void UChronosConversation::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    OpenElapsed += DeltaTime;
    SetRenderOpacity(FMath::Clamp(OpenElapsed / .18f, 0.f, 1.f));
    RefreshLegacyChoiceStates();
    for (int32 Index = 0; Index < Choices.Num(); ++Index)
    {
        auto* Button = Choices[Index].Get();
        if (Button->HasUserFocus(GetOwningPlayer())) Selected = Index;
        const bool bHighlight = Button->HasUserFocus(GetOwningPlayer()) || Button->IsHovered();
        const auto& Style = ChronosMenuArt::ButtonStyle(bHighlight);
        if (Button->GetStyle().Normal.GetResourceObject() != Style.Normal.GetResourceObject()) Button->SetStyle(Style);
    }
}

FReply UChronosConversation::NativeOnFocusReceived(const FGeometry&, const FFocusEvent&)
{
    MoveSelection(0);
    return FReply::Handled();
}

FReply UChronosConversation::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    const FKey Key = Event.GetKey();
    if (bLegacyChoices && (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right))
    {
        if (!Event.IsRepeat()) InvokeLegacyKey(EKeys::Escape);
        return FReply::Handled();
    }
    if (Key == EKeys::Left || Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Up)
    {
        MoveSelection(-1);
        return FReply::Handled();
    }
    if (Key == EKeys::Right || Key == EKeys::Down || Key == EKeys::Tab || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_DPad_Down)
    {
        MoveSelection(1);
        return FReply::Handled();
    }
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        if (!bConfirmed && !Event.IsRepeat())
        {
            if (auto* Button = SelectedButton())
            {
                bConfirmed = true;
                Button->OnClicked.Broadcast();
            }
        }
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply UChronosConversation::NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event)
{
    if (DialogueScroll && Event.GetKey() == EKeys::Gamepad_RightY)
    {
        const float Value = Event.GetAnalogValue();
        if (FMath::Abs(Value) > .25f)
            DialogueScroll->SetScrollOffset(FMath::Clamp(DialogueScroll->GetScrollOffset() - Value * 360.f * GetWorld()->GetDeltaSeconds(),
                0.f, DialogueScroll->GetScrollOffsetOfEnd()));
        return FReply::Handled();
    }
    if (Event.GetKey() != EKeys::Gamepad_LeftX && Event.GetKey() != EKeys::Gamepad_LeftY)
        return Super::NativeOnAnalogValueChanged(Geometry, Event);
    if (FMath::Abs(Event.GetAnalogValue()) < .35f) AnalogRepeatAt = 0;
    else if (FMath::Abs(Event.GetAnalogValue()) > .6f && OpenElapsed >= AnalogRepeatAt)
    {
        const float Sign = Event.GetKey() == EKeys::Gamepad_LeftY ? -1.f : 1.f;
        MoveSelection(Event.GetAnalogValue() * Sign > 0 ? 1 : -1);
        AnalogRepeatAt = OpenElapsed + .28f;
    }
    return FReply::Handled();
}

void UChronosThought::NativeConstruct()
{
    Super::NativeConstruct();
    if (auto* Text = Cast<UTextBlock>(GetWidgetFromName(TEXT("texto"))))
    {
        FormatText(*Text, 22);
        Text->SetJustification(ETextJustify::Center);
        if (auto* TextSlot = Cast<UCanvasPanelSlot>(Text->Slot))
        {
            TextSlot->SetAnchors(FAnchors(.2, .68, .8, .68));
            TextSlot->SetOffsets(FMargin(0, 0, 0, 110));
            TextSlot->SetAlignment(FVector2D(0, 0));
        }
    }
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UChronosThought::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    const bool bBlocked = !UChronosInteraction::CanPresentThought(GetOwningPlayerPawn());
    FTimerDynamicDelegate Hide;
    Hide.BindUFunction(this, TEXT("ocultar"));
    auto& Timers = GetWorld()->GetTimerManager();
    const FTimerHandle Handle = Timers.K2_FindDynamicTimerHandle(Hide);
    if (bBlocked)
    {
        Timers.PauseTimer(Handle);
        bSuspended = true;
        auto* Player = GetOwningPlayerPawn();
        const auto* Leaving = FindFProperty<FBoolProperty>(GetClass(), TEXT("saliendo"));
        const auto* Deadline = Player ? FindFProperty<FDoubleProperty>(Player->GetClass(), TEXT("finPensamiento")) : nullptr;
        if (Deadline && Leaving && !Leaving->GetPropertyValue_InContainer(this))
            Deadline->SetPropertyValue_InContainer(Player, Deadline->GetPropertyValue_InContainer(Player) + DeltaTime);
    }
    else if (bSuspended)
    {
        Timers.UnPauseTimer(Handle);
        bSuspended = false;
    }
    if (auto* Text = GetWidgetFromName(TEXT("texto")))
        Text->SetVisibility(bBlocked ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
    if (!bBlocked) Super::NativeTick(Geometry, DeltaTime);
}
