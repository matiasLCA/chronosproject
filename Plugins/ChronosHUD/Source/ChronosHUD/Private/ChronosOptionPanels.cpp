#include "ChronosOptionPanels.h"

#include "ChronosMenuFocus.h"
#include "ChronosMenuSettings.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Engine.h"
#include "Layout/WidgetPath.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChronosOptions"


namespace
{
    const FLinearColor Gold(FColor(216, 180, 129));
    const FLinearColor Ivory(FColor(239, 217, 182));

    TSharedRef<SWidget> FooterButton(FSlateFontInfo Font, FText Label, FOnClicked Click, TAttribute<float> Scale)
    {
        Font.Size = 19;
        return SNew(SBox).HeightOverride_Lambda([Scale] { return 49.f * Scale.Get(1.f); })
        [
            SNew(SChronosMenuButton).ButtonStyle(&ChronosMenuArt::ButtonStyle())
            .HAlign(HAlign_Center).VAlign(VAlign_Center).OnClicked(MoveTemp(Click))
            [SNew(STextBlock).Font_Lambda([Font, Scale] { return ChronosMenuArt::ScaleFont(Font, Scale.Get(1.f)); }).Text(Label).ColorAndOpacity(Ivory)]
        ];
    }

    TSharedRef<SWidget> Footer(const FSlateFontInfo& Font, const FText& Reset, FOnClicked ResetClick,
        FOnClicked ConfirmClick, FOnClicked CancelClick, TAttribute<float> Scale)
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 12, 0)
            [FooterButton(Font, Reset, MoveTemp(ResetClick), Scale)]
            + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 12, 0)
            [FooterButton(Font, LOCTEXT("Confirm", "Confirmar cambios"), MoveTemp(ConfirmClick), Scale)]
            + SHorizontalBox::Slot().FillWidth(1)
            [FooterButton(Font, LOCTEXT("Cancel", "Cancelar"), MoveTemp(CancelClick), Scale)];
    }

    const FSliderStyle& VolumeSliderStyle()
    {
        static const FSliderStyle Style = FSliderStyle()
            .SetNormalBarImage(FSlateRoundedBoxBrush(FLinearColor(.41f, .28f, .14f), 3.f, Gold, 1.f))
            .SetHoveredBarImage(FSlateRoundedBoxBrush(FLinearColor(.59f, .42f, .23f), 3.f, Gold, 1.f))
            .SetNormalThumbImage(FSlateRoundedBoxBrush(Gold, 3.f, Ivory, 1.f, FVector2f(20, 29)))
            .SetHoveredThumbImage(FSlateRoundedBoxBrush(Ivory, 3.f, Gold, 1.f, FVector2f(20, 29)))
            .SetBarThickness(7.f);
        return Style;
    }

    class SChronosVolumePanel final : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SChronosVolumePanel) {}
            SLATE_ARGUMENT(FSlateFontInfo, Font)
            SLATE_ARGUMENT(TWeakObjectPtr<UChronosMenuSettings>, Settings)
        SLATE_END_ARGS()

        void Construct(const FArguments& Args)
        {
            Settings = Args._Settings;
            if (Settings.IsValid()) Saved = Settings->AudioVolumes();
            Pending = Saved;
            auto Font = Args._Font;
            Font.Size = 25;
            auto Rows = SNew(SVerticalBox);
            const FText Labels[] = {LOCTEXT("Master", "Volumen general"), LOCTEXT("Ambience", "Ambiente"),
                LOCTEXT("MenuMusic", "Música del menú")};
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(Labels); ++Index)
            {
                auto Slider = SNew(SChronosMenuSlider).Style(&VolumeSliderStyle()).StepSize(.05f).RequiresControllerLock(false)
                    .Value_Lambda([this, Index] { return Pending[Index]; })
                    .OnValueChanged(this, &SChronosVolumePanel::Change, Index);
                if (!Initial) Initial = Slider;
                Rows->AddSlot().AutoHeight().Padding(0, 8, 0, 18)
                [
                    SNew(SBox).HeightOverride_Lambda([this] { return 74.f * MenuScale(); })
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(.40f).VAlign(VAlign_Center)
                        [SNew(STextBlock).Font(Font).ColorAndOpacity(Gold).Text(Labels[Index])]
                        + SHorizontalBox::Slot().FillWidth(.47f).VAlign(VAlign_Center)
                        [SNew(SBox).HeightOverride_Lambda([this] { return 29.f * MenuScale(); })[Slider]]
                        + SHorizontalBox::Slot().FillWidth(.13f).VAlign(VAlign_Center).HAlign(HAlign_Right)
                        [SNew(STextBlock).Font_Lambda([this, Font] { return ChronosMenuArt::ScaleFont(Font, MenuScale()); }).ColorAndOpacity(Ivory)
                            .Text_Lambda([this, Index] { return FText::AsPercent(Pending[Index]); })]
                    ]
                ];
            }
            ChildSlot
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().FillHeight(1)[Rows]
                + SVerticalBox::Slot().AutoHeight()
                [
                    Footer(Font, LOCTEXT("ResetVolume", "Restablecer volumen"),
                        FOnClicked::CreateSP(this, &SChronosVolumePanel::Reset),
                        FOnClicked::CreateSP(this, &SChronosVolumePanel::Confirm),
                        FOnClicked::CreateSP(this, &SChronosVolumePanel::Cancel),
                        TAttribute<float>::CreateSP(this, &SChronosVolumePanel::MenuScale))
                ]
            ];
        }

        ~SChronosVolumePanel() override
        {
            if (Settings.IsValid() && Pending != Saved) Settings->PreviewAudioVolumes(Saved);
        }

        bool SupportsKeyboardFocus() const override { return true; }
        FReply OnFocusReceived(const FGeometry&, const FFocusEvent&) override
        {
            return Initial.IsValid() ? FReply::Handled().SetUserFocus(Initial.ToSharedRef()) : FReply::Handled();
        }

    private:
        float MenuScale() const { return Settings.IsValid() ? Settings->MenuScale() : 1.f; }

        void Change(float Value, int32 Index)
        {
            Pending[Index] = FMath::RoundToInt(FMath::Clamp(Value, 0.f, 1.f) * 100.f) / 100.f;
            if (Settings.IsValid()) Settings->PreviewAudioVolumes(Pending);
        }

        FReply Reset()
        {
            Pending = FVector3f(1.f, 1.f, .35f);
            if (Settings.IsValid()) Settings->PreviewAudioVolumes(Pending);
            return FReply::Handled();
        }

        FReply Confirm()
        {
            if (Settings.IsValid() && Settings->SaveAudioVolumes(Pending)) Saved = Pending;
            return FReply::Handled();
        }

        FReply Cancel()
        {
            Pending = Saved;
            if (Settings.IsValid()) Settings->PreviewAudioVolumes(Saved);
            return FReply::Handled();
        }

        TWeakObjectPtr<UChronosMenuSettings> Settings;
        TSharedPtr<SSlider> Initial;
        FVector3f Saved = FVector3f(1.f, 1.f, .35f);
        FVector3f Pending = Saved;
    };
}

TSharedRef<SWidget> MakeChronosVolumePanel(const FSlateFontInfo& Font, UChronosMenuSettings* Settings)
{
    return SNew(SChronosVolumePanel).Font(Font).Settings(Settings);
}

#undef LOCTEXT_NAMESPACE
