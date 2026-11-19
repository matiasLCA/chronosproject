#pragma once

#include "Camera/CameraModifier.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "ChronosFridge.generated.h"

class ACharacter;
class AChronosBeer;
class UBoxComponent;
class UChildActorComponent;
class UCurveFloat;
class UStaticMeshComponent;
class UMaterialInterface;
class UTextBlock;
class UTimelineComponent;
class UUserWidget;
class UAudioComponent;
class USoundBase;

UENUM(BlueprintType)
enum class EChronosBeer : uint8
{
    BlackCrow UMETA(DisplayName="Black Crow"),
    White UMETA(DisplayName="Cerveza blanca")
};

UCLASS(Abstract)
class CHRONOSHUD_API AChronosFridgeTarget : public AActor
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Interacción")
    virtual void UseFridgeTarget(AActor* Player) {}

    UFUNCTION(BlueprintPure, Category="Interacción")
    virtual bool CanUseFridgeTarget(AActor* Player) const;

    virtual FText InteractionPrompt() const { return FText::GetEmpty(); }
};

UCLASS()
class CHRONOSHUD_API AChronosFridge : public AChronosFridgeTarget
{
    GENERATED_BODY()

public:
    AChronosFridge();
    void OnConstruction(const FTransform& Transform) override;
    bool CanUseFridgeTarget(AActor* Player) const override;
    void UseFridgeTarget(AActor* Player) override;
    FText InteractionPrompt() const override;

    UFUNCTION(BlueprintPure, Category="Refrigerador")
    bool HasShelfAccess() const { return FMath::Abs(DoorAngle) >= 48.f; }

    UFUNCTION(BlueprintPure, Category="Refrigerador")
    float GetDoorAngle() const { return DoorAngle; }

    bool TakeSlot(int32 Index);
    UPROPERTY(EditAnywhere, Category="Refrigerador|Existencias")
    TArray<FVector> StockPositions;

protected:
    void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    UFUNCTION()
    void MoveDoor(float Progress);
    UFUNCTION()
    void RepeatOpenSound();
    void UpdateOpenSound();

    bool IsDoorBlocked(float CandidateAngle) const;
    void ConfigureCan(UChildActorComponent& Slot, EChronosBeer Variant);
    void RefreshStock();
    FString StockId() const;
    UPROPERTY(Transient) TArray<TObjectPtr<AChronosBeer>> Stocked;
    FTimerHandle StockTimer;
    TWeakObjectPtr<AActor> Calendar;
    UPROPERTY(VisibleAnywhere, Category="Refrigerador|Audio") TObjectPtr<UAudioComponent> DoorAudio;
    UPROPERTY(VisibleAnywhere, Category="Refrigerador|Audio") TObjectPtr<UAudioComponent> OpenAudio;
    UPROPERTY(EditDefaultsOnly, Category="Refrigerador|Audio") TObjectPtr<USoundBase> DoorSound;
    UPROPERTY(EditDefaultsOnly, Category="Refrigerador|Audio") TObjectPtr<USoundBase> OpenSound;
    bool bOpenSoundActive = false;

    UPROPERTY(VisibleAnywhere, Category="Refrigerador")
    TObjectPtr<UStaticMeshComponent> Cabinet;
    UPROPERTY(VisibleAnywhere, Category="Refrigerador")
    TObjectPtr<USceneComponent> Hinge;
    UPROPERTY(VisibleAnywhere, Category="Refrigerador")
    TObjectPtr<UStaticMeshComponent> LowerDoor;
    UPROPERTY(VisibleAnywhere, Category="Refrigerador")
    TObjectPtr<UBoxComponent> DoorCollision;
    UPROPERTY(VisibleAnywhere, Category="Refrigerador")
    TObjectPtr<UTimelineComponent> DoorTimeline;
    UPROPERTY()
    TObjectPtr<UCurveFloat> DoorCurve;
    UPROPERTY(VisibleAnywhere, Category="Refrigerador")
    TObjectPtr<UChildActorComponent> BlackCan;
    UPROPERTY(VisibleAnywhere, Category="Refrigerador")
    TObjectPtr<UChildActorComponent> WhiteCan;
    UPROPERTY(EditDefaultsOnly, Category="Refrigerador")
    TSubclassOf<AChronosBeer> BeerClass;

    float DoorAngle = 0.f;
    float DoorPosition = 0.f;
    bool bOpening = false;
    bool bUpdatingDoor = false;
};

UCLASS()
class CHRONOSHUD_API AChronosBeer : public AChronosFridgeTarget
{
    GENERATED_BODY()

public:
    AChronosBeer();
    void OnConstruction(const FTransform& Transform) override;
    void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    bool CanUseFridgeTarget(AActor* Player) const override;
    void UseFridgeTarget(AActor* Player) override;
    FText InteractionPrompt() const override;

    void Stock(AChronosFridge* InFridge, EChronosBeer InVariant, int32 Index = INDEX_NONE);
    bool Equip(ACharacter* Character);
    UFUNCTION(BlueprintCallable, Category="Cerveza") bool Pocket();
    FName ItemId() const;
    float GetDrinkLift() const;
    bool IsDrinking() const;
    void CancelDrink();

    UFUNCTION(BlueprintPure, Category="Cerveza")
    EChronosBeer GetVariant() const { return Variant; }

    UFUNCTION(BlueprintPure, Category="Cerveza")
    bool IsHeld() const { return Holder.IsValid(); }

    UFUNCTION(BlueprintCallable, Category="Cerveza")
    void Drink();

protected:
    void BeginPlay() override;

private:
    UFUNCTION()
    void AnimateDrink(float Progress);
    UFUNCTION()
    void FinishDrink();

    void SetAppearance();
    UPROPERTY(VisibleAnywhere, Category="Cerveza|Audio") TObjectPtr<UAudioComponent> DrinkAudio;
    UPROPERTY(EditDefaultsOnly, Category="Cerveza|Audio") TObjectPtr<USoundBase> DrinkSound;
    float ConsumptionDuration = 1.75f;

    UPROPERTY(EditAnywhere, Category="Cerveza")
    EChronosBeer Variant = EChronosBeer::BlackCrow;
    UPROPERTY(EditAnywhere, Category="Cerveza|Mareo", meta=(ClampMin="1.0"))
    float DuracionMareo = 12.f;
    UPROPERTY(EditAnywhere, Category="Cerveza|Mareo", meta=(ClampMin="0.0", ClampMax="1.0"))
    float IntensidadMareo = .35f;
    UPROPERTY(VisibleAnywhere, Category="Cerveza")
    TObjectPtr<UStaticMeshComponent> LabelMesh;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> BlackLabel;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> WhiteLabel;
    UPROPERTY(VisibleAnywhere, Category="Cerveza")
    TObjectPtr<UTimelineComponent> DrinkTimeline;
    UPROPERTY()
    TObjectPtr<UCurveFloat> DrinkCurve;

    TWeakObjectPtr<AChronosFridge> Fridge;
    TWeakObjectPtr<ACharacter> Holder;
    TWeakObjectPtr<APlayerController> Controller;
    FTransform Grip;
    int32 StockIndex = INDEX_NONE;
    bool bConsumed = false;
};

UCLASS()
class UChronosBeerCamera : public UCameraModifier
{
    GENERATED_BODY()

public:
    void StartEffect(float Duration, float Strength);
    bool ModifyCamera(float DeltaTime, FMinimalViewInfo& View) override;
    UFUNCTION(BlueprintPure, Category="Cerveza|Mareo") bool IsActive() const { return Intensity > KINDA_SMALL_NUMBER; }
    UFUNCTION(BlueprintPure, Category="Cerveza|Mareo") float GetIntoxication() const { return Intensity; }
    UFUNCTION(BlueprintPure, Category="Cerveza|Mareo") float GetRecoveryTime() const { return Intensity / FMath::Max(RecoveryRate, .0001f); }
    float SpeedMultiplier() const;

private:
    UPROPERTY(EditDefaultsOnly, Category="Cerveza|Mareo", meta=(ClampMin="0.35", ClampMax="3.0"))
    float MaximumIntensity = 1.75f;
    UPROPERTY(EditDefaultsOnly, Category="Cerveza|Mareo", meta=(ClampMin="0.1", ClampMax="1.0"))
    float MinimumSpeedMultiplier = .3f;
    UPROPERTY(EditDefaultsOnly, Category="Cerveza|Mareo", meta=(ClampMin="0.0", ClampMax="2.0"))
    float CameraTremor = .35f;
    float Elapsed = 0.f;
    float Intensity = 0.f;
    float RecoveryRate = .35f / 12.f;
};

UCLASS()
class UChronosFridgeSession : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    void Tick(float DeltaTime) override;
    TStatId GetStatId() const override;
    bool DoesSupportWorldType(EWorldType::Type Type) const override;
    void Deinitialize() override;
    bool HasCan() const { return Held.IsValid(); }
    void Hold(AChronosBeer* Beer);
    bool EquipItem(FName Item);
    AChronosBeer* GetHeld() const { return Held.Get(); }
    bool PickedUpThisFrame() const { return LastPickupFrame == GFrameCounter; }

private:
    TWeakObjectPtr<AChronosBeer> Held;
    void UpdateItemInput(APlayerController* PC);
    void ConsumeHeld();
    void PocketHeld();
    void PickupLookedBeer();
    UPROPERTY(Transient) TObjectPtr<class UInputMappingContext> ItemContext;
    UPROPERTY(Transient) TObjectPtr<class UInputAction> DrinkAction;
    UPROPERTY(Transient) TObjectPtr<class UInputAction> PocketAction;
    UPROPERTY(Transient) TObjectPtr<class UInputAction> PickupAction;
    UPROPERTY(Transient) TObjectPtr<class UEnhancedInputComponent> ItemInput;
    TWeakObjectPtr<APlayerController> InputPlayer;
    bool bItemInputActive = false;
    bool bRestoredEquipment = false;
    TArray<FKey> AppliedItemKeys;
    uint64 LastPickupFrame = MAX_uint64;
};
