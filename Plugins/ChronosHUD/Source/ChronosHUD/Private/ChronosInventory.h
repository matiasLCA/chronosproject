#pragma once

#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "ChronosInventory.generated.h"

class SChronosMenuButton;
class UTexture2D;

UCLASS()
class UChronosInventory : public UUserWidget
{
    GENERATED_BODY()
public:
    void Open();
    void Close();
    static bool IsOpen(const UObject* Context);
    static void CloseForModal(const UObject* Context);
    UFUNCTION(BlueprintPure, Category="Chronos|Inventory") FName GetSelectedItem() const;
    UFUNCTION(BlueprintPure, Category="Chronos|Inventory") int32 GetPage() const { return Page; }
    UFUNCTION(BlueprintPure, Category="Chronos|Inventory") bool IsExamining() const { return bExamining; }
protected:
    TSharedRef<SWidget> RebuildWidget() override;
    void NativeDestruct() override;
    void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    FReply NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override;
private:
    void Select(int32 Index, bool bFocus = false);
    void Navigate(int32 Step);
    FReply Use();
    FReply Examine();
    FReply ChangePage(int32 Direction);
    void RestoreInput();
    FText ItemName(FName Item) const;
    FText Description() const;
    const FSlateBrush* ItemImage(FName Item) const;
    FKey ActionKey(FName Name, FKey Fallback) const;
    TSharedRef<SWidget> ActionCaption(const TCHAR* Label, FName Action, FKey Keyboard, FKey Controller) const;
    TArray<FName> Items;
    int32 Selected = 0;
    int32 Page = 0;
    bool bExamining = false;
    bool bLocksInput = false;
    bool bWasCursorVisible = false;
    float StickRepeatAt = 0.f;
    TArray<TSharedPtr<SChronosMenuButton>> Cells;
    TArray<TSharedPtr<SChronosMenuButton>> Footer;
    TMap<FName, FSlateBrush> Images;
    FSlateBrush Raven;
    FSlateBrush Keycap;
    UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> Textures;
};
