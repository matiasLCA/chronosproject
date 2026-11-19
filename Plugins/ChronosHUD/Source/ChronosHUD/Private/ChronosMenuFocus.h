#pragma once

#include "ChronosMenuArt.h"
#include "Components/Button.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"

namespace ChronosMenuArt
{
    inline FSlateFontInfo ScaleFont(FSlateFontInfo Font, float Scale)
    {
        Font.Size = FMath::RoundToInt(Font.Size * Scale);
        return Font;
    }

    inline int32 PaintFocusArrow(const SWidget& Widget, const FGeometry& Geometry,
        FSlateWindowElementList& Elements, int32 Layer, float X)
    {
        if (!Widget.HasAnyUserFocus().IsSet()) return Layer;
        const float Height = Geometry.GetLocalSize().Y;
        FSlateDrawElement::MakeText(Elements, ++Layer,
            Geometry.ToPaintGeometry(FVector2f(12.f, 18.f), FSlateLayoutTransform(FVector2f(X, (Height - 18.f) * .5f))),
            FString(TEXT("›")), FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16),
            ESlateDrawEffect::None, FLinearColor(FColor(255, 209, 137)));
        return Layer;
    }
}

class FChronosMenuStick
{
public:
    int32 Step(float Axis)
    {
        const float Magnitude = FMath::Abs(Axis);
        if (Magnitude <= .35f) bHeld = false;
        if (bHeld || Magnitude < .55f) return 0;
        bHeld = true;
        return Axis > 0.f ? 1 : -1;
    }

    void Reset() { bHeld = false; }

private:
    bool bHeld = false;
};

class SChronosMenuButton : public SButton
{
public:
    void Construct(const SButton::FArguments& Args)
    {
        SButton::Construct(Args);
        SetMenuStyle(*Args._ButtonStyle);
    }

    void SetMenuStyle(const FButtonStyle& InStyle)
    {
        Regular = InStyle;
        Focused = InStyle;
        Focused.SetNormal(InStyle.Hovered);
        SetButtonStyle(HasAnyUserFocus().IsSet() ? &Focused : &Regular);
    }

protected:
    int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
    {
        Layer = SButton::OnPaint(Args, Geometry, CullingRect, Elements, Layer, InWidgetStyle, bParentEnabled);
        return ChronosMenuArt::PaintFocusArrow(*this, Geometry, Elements, Layer, 7.f);
    }

    FReply OnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event) override
    {
        SetButtonStyle(&Focused);
        return SButton::OnFocusReceived(Geometry, Event);
    }

    void OnFocusLost(const FFocusEvent& Event) override
    {
        SetButtonStyle(&Regular);
        SButton::OnFocusLost(Event);
    }

private:
    FButtonStyle Regular;
    FButtonStyle Focused;
};

class SChronosMenuSizeButton : public SChronosMenuButton
{
public:
    DECLARE_DELEGATE_OneParam(FOnStep, int32);

    void SetOnStep(FOnStep InOnStep) { OnStep = MoveTemp(InOnStep); }

protected:
    FReply OnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override
    {
        if (Event.GetKey() != EKeys::Gamepad_RightX) return SButton::OnAnalogValueChanged(Geometry, Event);
        if (const int32 Direction = Stick.Step(Event.GetAnalogValue())) OnStep.ExecuteIfBound(Direction);
        return FReply::Handled();
    }

    FNavigationReply OnNavigation(const FGeometry& Geometry, const FNavigationEvent& Event) override
    {
        const EUINavigation Direction = Event.GetNavigationType();
        if (Direction != EUINavigation::Left && Direction != EUINavigation::Right)
            return SButton::OnNavigation(Geometry, Event);
        OnStep.ExecuteIfBound(Direction == EUINavigation::Right ? 1 : -1);
        return FNavigationReply::Stop();
    }

    void OnFocusLost(const FFocusEvent& Event) override
    {
        Stick.Reset();
        SChronosMenuButton::OnFocusLost(Event);
    }

private:
    FOnStep OnStep;
    FChronosMenuStick Stick;
};

class SChronosMenuSlider : public SSlider
{
public:
    void Construct(const SSlider::FArguments& Args)
    {
        SSlider::Construct(Args);
        Regular = *Args._Style;
        Focused = Regular;
        Focused.SetNormalBarImage(Regular.HoveredBarImage).SetNormalThumbImage(Regular.HoveredThumbImage);
        SetStyle(&Regular);
    }

protected:
    int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
    {
        Layer = SSlider::OnPaint(Args, Geometry, CullingRect, Elements, Layer, InWidgetStyle, bParentEnabled);
        return ChronosMenuArt::PaintFocusArrow(*this, Geometry, Elements, Layer, -17.f);
    }

    FReply OnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override
    {
        if (Event.GetKey() != EKeys::Gamepad_RightX) return SSlider::OnAnalogValueChanged(Geometry, Event);
        if (const int32 Direction = Stick.Step(Event.GetAnalogValue())) Adjust(Direction);
        return FReply::Handled();
    }

    FNavigationReply OnNavigation(const FGeometry& Geometry, const FNavigationEvent& Event) override
    {
        const EUINavigation Direction = Event.GetNavigationType();
        if (Direction != EUINavigation::Left && Direction != EUINavigation::Right)
            return SSlider::OnNavigation(Geometry, Event);
        Adjust(Direction == EUINavigation::Right ? 1 : -1);
        return FNavigationReply::Stop();
    }

    FReply OnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event) override
    {
        SetStyle(&Focused);
        return SSlider::OnFocusReceived(Geometry, Event);
    }

    void OnFocusLost(const FFocusEvent& Event) override
    {
        Stick.Reset();
        SetStyle(&Regular);
        SSlider::OnFocusLost(Event);
    }

private:
    void Adjust(int32 Direction)
    {
        const float Value = GetValue();
        const float Next = FMath::Clamp(Value + Direction * GetStepSize(), GetMinValue(), GetMaxValue());
        if (!FMath::IsNearlyEqual(Next, Value)) CommitValue(Next);
    }

    FChronosMenuStick Stick;
    FSliderStyle Regular;
    FSliderStyle Focused;
};

class FChronosWidgetFocus
{
public:
    void Set(UButton* Button)
    {
        if (Button == Focused.Get()) return;
        if (auto* Previous = Focused.Get())
        {
            FButtonStyle Style = Previous->GetStyle();
            Previous->SetStyle(Style.SetNormal(OriginalNormal));
        }
        Focused = Button;
        if (Button)
        {
            FButtonStyle Style = Button->GetStyle();
            OriginalNormal = Style.Normal;
            const FSlateBrush& Glow = Style.Hovered.DrawAs == ESlateBrushDrawType::NoDrawType
                ? ChronosMenuArt::ButtonStyle().Hovered : Style.Hovered;
            Button->SetStyle(Style.SetNormal(Glow));
        }
    }

    void Set(const TArray<TWeakObjectPtr<UButton>>& Buttons, APlayerController* Player)
    {
        for (const auto& Entry : Buttons)
            if (auto* Button = Entry.Get(); Button && Button->HasUserFocus(Player))
            {
                Set(Button);
                return;
            }
        Set(nullptr);
    }

private:
    TWeakObjectPtr<UButton> Focused;
    FSlateBrush OriginalNormal;
};
