#pragma once

#include "Blueprint/UserWidget.h"
#include "ChronosConversation.generated.h"

class UButton;
class UScrollBox;

UCLASS()
class CHRONOSHUD_API UChronosConversation : public UUserWidget
{
    GENERATED_BODY()

public:
    static bool IsActive(const UObject* WorldContext);

protected:
    void NativeConstruct() override;
    void NativeDestruct() override;
    void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    FReply NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event) override;
    FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    FReply NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override;

private:
    void BuildLegacyChoices();
    void RefreshLegacyChoiceStates();
    void InvokeLegacyKey(FKey Key);
    UFUNCTION()
    void ExecuteLegacyChoice();
    void MoveSelection(int32 Direction);
    UButton* SelectedButton() const;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UButton>> Choices;
    UPROPERTY(Transient) TObjectPtr<UScrollBox> DialogueScroll;
    int32 Selected = 0;
    float AnalogRepeatAt = 0.f;
    float OpenElapsed = 0.f;
    bool bLocksMovement = false;
    bool bConfirmed = false;
    bool bLegacyChoices = false;
    FName LegacyReference;
    TArray<FName> LegacyLastDays;
};

UCLASS()
class CHRONOSHUD_API UChronosThought : public UUserWidget
{
    GENERATED_BODY()

protected:
    void NativeConstruct() override;
    void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
    bool bSuspended = false;
};
