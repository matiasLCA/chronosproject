#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/DataTable.h"
#include "GameFramework/SaveGame.h"
#include "ChronosJournal.generated.h"

class UButton;
class UCanvasPanel;
class UFont;
class UImage;
class UScrollBox;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class USoundBase;
struct FChronosTask;

UENUM(BlueprintType)
enum class EChronosJournalSection : uint8
{
    Tasks,
    Bestiary,
    Appearances,
    Herbarium,
    Poems,
    Notes
};

USTRUCT(BlueprintType)
struct FChronosJournalEntry : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cuaderno")
    EChronosJournalSection Section = EChronosJournalSection::Notes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cuaderno")
    FText Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cuaderno", meta=(MultiLine=true))
    FText Body;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cuaderno")
    FText Location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cuaderno")
    TSoftObjectPtr<UTexture2D> Sketch;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cuaderno")
    TSoftObjectPtr<UTexture2D> BlurredSketch;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cuaderno")
    FString CollectionSlot;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cuaderno")
    int32 Order = 0;
};

USTRUCT()
struct FChronosJournalDiscovery
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    FText Observation;

    UPROPERTY(SaveGame)
    bool bAlteredPerception = false;
};

UCLASS()
class UChronosJournalSave : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(SaveGame)
    TMap<FName, FChronosJournalDiscovery> Discoveries;

    UPROPERTY(SaveGame)
    TSet<FName> ReadEntries;

    UPROPERTY(SaveGame)
    int32 ZoomStep = 0;

    UPROPERTY(SaveGame)
    bool bHighContrast = false;
};

UCLASS(Blueprintable)
class UChronosJournal : public UUserWidget
{
    GENERATED_BODY()

public:
    bool Prepare(UFont* InFont);
    static void Snapshot(UWorld* World, TArray<uint8>& Out);
    void ShowJournal(FKey InCloseKey);
    void RefreshTasks(const TArray<FChronosTask>& InTasks, const FString& InDay);
    bool RecordDiscovery(FName Entry, const FText& Observation, bool bAlteredPerception);
    bool IsReady() const { return Catalog != nullptr; }
    bool ConsumeCloseRequest();

protected:
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event) override;
    virtual FReply NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UDataTable> Catalog;

    UPROPERTY(Transient)
    TObjectPtr<UChronosJournalSave> Progress;

    UPROPERTY(Transient)
    TObjectPtr<UFont> Serif;

    UPROPERTY(Transient)
    TObjectPtr<UCanvasPanel> Canvas;

    UPROPERTY(Transient)
    TObjectPtr<UImage> Book;

    UPROPERTY(Transient)
    TObjectPtr<UImage> Cover;

    UPROPERTY(Transient)
    TObjectPtr<USoundBase> PaperSound;

    UPROPERTY(Transient)
    TObjectPtr<USoundBase> PageTurnSound;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UButton>> Tabs;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> TabLabels;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UScrollBox>> Readers;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UVerticalBox>> Pages;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> PageCounter;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ZoomCounter;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> InputHints;
    int32 LastHintDevice = INDEX_NONE;

    UPROPERTY(Transient)
    TObjectPtr<UButton> Prev;

    UPROPERTY(Transient)
    TObjectPtr<UButton> Next;

    UPROPERTY(Transient)
    TObjectPtr<UButton> ZoomIn;

    UPROPERTY(Transient)
    TObjectPtr<UButton> ZoomOut;

    TMap<FName, bool> CollectedPoems;
    TArray<FString> TaskText;
    TArray<bool> TaskDone;
    int32 ActiveTask = INDEX_NONE;
    FString Day;
    int32 Section = 0;
    int32 Page = 0;
    int32 PageCount = 1;
    float Opening = 0.f;
    float Turning = 0.f;
    FKey CloseKey;
    bool bCloseRequested = false;

    UButton* AddIcon(const TCHAR* Asset, const FText& Hint, FVector2D Center, FVector2D Size);
    UButton* AddPageArrow(const TCHAR* Asset, const FText& Hint, FVector2D Center);
    void DrawPages(bool bResetScroll);
    void RefreshPoems();
    void SaveProgress() const;
    void SelectSection(int32 Value);
    void StepPage(int32 Direction);
    void SetZoomStep(int32 Value);
    void ReadEntry(FName Id);
    void Turn();

    UFUNCTION()
    void ClickTasks();
    UFUNCTION()
    void ClickBestiary();
    UFUNCTION()
    void ClickAppearances();
    UFUNCTION()
    void ClickHerbarium();
    UFUNCTION()
    void ClickPoems();
    UFUNCTION()
    void ClickNotes();
    UFUNCTION()
    void ClickPrev();
    UFUNCTION()
    void ClickNext();
    UFUNCTION()
    void ClickZoomIn();
    UFUNCTION()
    void ClickZoomOut();
    UFUNCTION()
    void ClickReset();
    UFUNCTION()
    void ClickContrast();
    UFUNCTION()
    void ClickClose();
};
