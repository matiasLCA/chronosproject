#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "ChronosMenuSettings.h"
#include "Styling/SlateBrush.h"
#include "Subsystems/WorldSubsystem.h"
#include "ChronosHUDLayer.generated.h"

class APlayerController;
class UFont;
class UChronosJournal;
class UChronosContext;
class UChronosInventory;
class UChronosLetter;
class UTexture;
class UTextureRenderTarget2D;

namespace ChronosAtlas
{
    inline const FVector Center(47800, 0, 70000);
    inline constexpr float Width = 96000.f;
    inline const FVector2D MapMin(.201, .035);
    inline const FVector2D MapMax(.666, .828);
}

UENUM(BlueprintType)
enum class EChronosHUDMode : uint8
{
    Immersive,
    Normal,
    Guided
};

struct FChronosPlace
{
    FString Label;
    FString Icon;
    FVector Position;
    bool bDiscovered = false;
};

struct FChronosTask
{
    FString Description;
    FString ProgressText;
    TOptional<FVector> Position;
    bool bCompleted = false;
    int32 PlaceIndex = INDEX_NONE;
};

UCLASS()
class UChronosHUDLayer : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeArt();
    void SetScales(const FChronosHUDScales& Value);
    void SetAtlas(UTexture* Texture);
    UFont* GetSerif() const { return Serif.Get(); }

    TArray<FChronosPlace> Places;
    TArray<FChronosTask> Tasks;
    TArray<FVector> Route;
    FVector Location = FVector::ZeroVector;
    float Heading = 0;
    float Health = -1;
    float Battery = -1;
    float ShownHealth = 0;
    float Stamina = -1;
    float ShownStamina = -1;
    float StaminaOpacity = 0;
    float HealthOpacity = 0;
    float BatteryOpacity = 0;
    FString Clock;
    FString Day;
    FString MapKeyText;
    FString JournalKeyText;
    FString ModeKeyText;
    FString InventoryKeyText;
    FKey MapHintKey;
    FKey JournalHintKey;
    FKey ModeHintKey;
    FKey InventoryHintKey;
    TOptional<FVector2D> WorldMarker;
    TOptional<FVector> Waypoint;
    FString WaypointLabel;
    bool bAtlasReady = false;
    bool bMapOpen = false;
    FKey MapCloseKey;
    FKey PadMapCloseKey;
    bool bNarrativeHidden = false;
    TWeakObjectPtr<UChronosLetter> LetterReader;
    EChronosHUDMode Mode = EChronosHUDMode::Normal;

protected:
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& Event) override;
    virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;

private:
    struct FMapView
    {
        FVector2D Position;
        FVector2D Size;
    };
    static FMapView MapView(FVector2D LayoutSize);
    FVector2D UVAt(FVector2D Fraction) const;
    void SetMapZoomBounds();
    void PlaceMapPin(const FMapView& View, FVector2D Fraction);
    FVector2D PointerStart = FVector2D::ZeroVector;
    FVector2D CenterAtPress = FVector2D::ZeroVector;
    bool bMapPressed = false;
    bool bMapDragged = false;
    FVector2D MapCenter = (ChronosAtlas::MapMin + ChronosAtlas::MapMax) * .5;
    double MapZoom = 1.0;
    FVector2D PadMapCursor = FVector2D(.5);
    FVector2D PadCursorAxis = FVector2D::ZeroVector;
    FVector2D PadPanAxis = FVector2D::ZeroVector;

    UPROPERTY(Transient)
    TMap<FString, TObjectPtr<UTexture>> Textures;

    UPROPERTY(Transient)
    TObjectPtr<UFont> Serif;

    TMap<FString, FSlateBrush> Brushes;
    FChronosHUDScales Scales;
};

UCLASS(Config=Game)
class UChronosHUDSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
    virtual bool IsTickable() const override;
    virtual bool IsTickableWhenPaused() const override { return true; }
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& World) override;
    void SetLetterReader(UChronosLetter* Reader);
    bool IsReadingLetter() const;

    UFUNCTION(BlueprintCallable, Category="Chronos|HUD")
    void SetHUDMode(EChronosHUDMode NewMode);

    UFUNCTION(BlueprintCallable, Category="Chronos|HUD")
    void SetNarrativeMarkersHidden(bool bHidden);

    UFUNCTION(BlueprintCallable, Category="Chronos|Cuaderno")
    bool RecordJournalDiscovery(FName Entry, const FText& Observation, bool bAlteredPerception);

private:
    UPROPERTY(Config)
    FKey MapKey = EKeys::M;

    UPROPERTY(Config)
    FKey JournalKey = EKeys::J;

    UPROPERTY(Config)
    FKey HUDModeKey = EKeys::F8;

    UPROPERTY(Config)
    EChronosHUDMode DefaultMode = EChronosHUDMode::Normal;

    UPROPERTY(Transient)
    TObjectPtr<UChronosHUDLayer> HUD;

    UPROPERTY(Transient)
    TObjectPtr<UChronosContext> Context;

    UPROPERTY(Transient)
    TObjectPtr<UChronosJournal> Journal;
    UPROPERTY(Transient) TObjectPtr<UChronosInventory> Inventory;
    UPROPERTY(Transient) TObjectPtr<class UEnhancedInputComponent> InventoryInput;
    TWeakObjectPtr<APlayerController> InventoryPlayer;
    void BindInventoryInput(APlayerController* PC);
    void ToggleInventory();

    TWeakObjectPtr<UObject> OpenList;
    FDelegateHandle PoemSpawnHandle;

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> Atlas;

    TWeakObjectPtr<APawn> Player;
    TWeakObjectPtr<AActor> Director;
    TWeakObjectPtr<UObject> HealthModel;
    TWeakObjectPtr<UObject> TaskManager;
    TArray<TWeakObjectPtr<UUserWidget>> OldWidgets;
    TMap<TWeakObjectPtr<UUserWidget>, float> PausedWidgetOpacity;
    bool bGameplayHUDHiddenForPause = false;
    float SampleIn = 0;
    float CacheIn = 0;
    float RouteIn = 0;
    float HealthUntil = 0;
    float BatteryUntil = 0;
    float AtlasAt = 0;
    bool bBatteryActive = false;
    bool bHideMarkers = false;
    bool bUrnsRestored = false;
    bool bMapInput = false;
    bool bSavedShowCursor = false;
    TOptional<FVector> RoutedDestination;

    bool Attach(APlayerController* PC, APawn* Pawn);
    void Sample(APawn* Pawn, float Now);
    void SampleHealth(float Now);
    void SampleTasks();
    void CaptureAtlas();
    void HideOld(UObject* Widget);
    bool KeyAvailable(APlayerController* PC, FKey Key) const;
    void SetMapInput(APlayerController* PC, bool bEnabled);
    void SetJournalOpen(APlayerController* PC, bool bEnabled, FKey CloseKey = FKey());
    bool CreateJournal(APlayerController* PC);
    void AssignPoemId(AActor* Actor);

    UFUNCTION()
    void OnHealthChanged(double CurrentHealth, double MaxHealth);

    UFUNCTION()
    void OnTasksChanged();
};
