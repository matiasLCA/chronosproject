#pragma once

#include "ChronosProgress.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ChronosUrns.generated.h"

class UWidgetComponent;
class STextBlock;

UCLASS()
class CHRONOSHUD_API AChronosBody : public AActor
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Identidad") FGuid PersistentBodyId;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Identidad") FDataTableRowHandle DeceasedRecord;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Identidad") FText DeceasedName;
    void BeginPlay() override;
    bool Identify();
};

UCLASS()
class CHRONOSHUD_API AChronosUrn : public AActor
{
    GENERATED_BODY()
public:
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Identidad") FGuid PersistentId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Identidad") FDataTableRowHandle DeceasedRecord;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identidad") EChronosUrnOrigin Origin = EChronosUrnOrigin::Unverified;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identidad") FName SourceMission;
    void BeginPlay() override;
    FChronosUrnRecord* Record();
    void RefreshLabel();
private:
    UPROPERTY(Transient) TObjectPtr<UWidgetComponent> Plaque;
    TSharedPtr<STextBlock> PlaqueText;
};

UCLASS()
class CHRONOSHUD_API UChronosUrns : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Chronos|Urnas") static void InitializeUrn(AActor* Urn);
    UFUNCTION(BlueprintPure, Category="Chronos|Urnas") static FText Label(AActor* Urn);
    UFUNCTION(BlueprintPure, Category="Chronos|Urnas") static bool CanDispose(AActor* Urn);
    UFUNCTION(BlueprintCallable, Category="Chronos|Urnas") static bool Dispose(AActor* Bin, AActor* Player);
    UFUNCTION(BlueprintPure, Category="Chronos|Urnas") static bool CanUseBin(AActor* Bin, AActor* Player);
    UFUNCTION(BlueprintCallable, Category="Chronos|Urnas") static bool AssignDeceased(AActor* Urn, FDataTableRowHandle Deceased);
    UFUNCTION(BlueprintCallable, Category="Chronos|Urnas") static bool PrepareCremation(AActor* Furnace, AActor* Body);
    UFUNCTION(BlueprintCallable, Category="Chronos|Urnas") static void CompleteCremation(AActor* Urn, AActor* LowerDoor);
    UFUNCTION(BlueprintCallable, Category="Chronos|Urnas") static void TrackCarried(AActor* Urn, AActor* Player);
    UFUNCTION(BlueprintCallable, Category="Chronos|Urnas") static void TrackStored(AActor* Urn, AActor* Shelf);
    UFUNCTION(BlueprintCallable, Category="Chronos|Urnas") static bool Deliver(AActor* Visitor, AActor* Player);
    UFUNCTION(BlueprintPure, Category="Chronos|Urnas") static FString RequestedName(AActor* Visitor);
    UFUNCTION(BlueprintCallable, Category="Chronos|Urnas") static bool AssignRequest(AActor* Visitor);
    UFUNCTION(BlueprintPure, Category="Chronos|Urnas") static FString RequestText(AActor* Visitor, bool bWrongUrn);
    UFUNCTION(BlueprintPure, Category="Chronos|Urnas") static FText BodyName(AActor* Body);
    UFUNCTION(BlueprintPure, Category="Chronos|Urnas") static bool CanHideFamilyAtNight(AActor* Visitor);
    UFUNCTION(BlueprintPure, Category="Chronos|Urnas") static bool HasSavedUrns(AActor* Context);
    static void Capture(UWorld* World);
    static void Restore(UWorld* World);
};
