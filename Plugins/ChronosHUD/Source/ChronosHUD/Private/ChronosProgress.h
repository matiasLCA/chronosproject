#pragma once

#include "Engine/DataTable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ChronosProgress.generated.h"

UENUM(BlueprintType)
enum class EChronosUrnOrigin : uint8 { Unverified, IdentifiedDeceased, UnknownLakeBody };

UENUM(BlueprintType)
enum class EChronosUrnState : uint8 { Available, Carried, Stored, Delivered, Disposed };

USTRUCT(BlueprintType)
struct FChronosUrnRecord
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) FGuid Id;
    UPROPERTY(SaveGame, BlueprintReadOnly) EChronosUrnOrigin Origin = EChronosUrnOrigin::Unverified;
    UPROPERTY(SaveGame, BlueprintReadOnly) EChronosUrnState State = EChronosUrnState::Available;
    UPROPERTY(SaveGame, BlueprintReadOnly) FDataTableRowHandle Deceased;
    UPROPERTY(SaveGame, BlueprintReadOnly) FString DisplayName;
    UPROPERTY(SaveGame, BlueprintReadOnly) FName Mission;
    UPROPERTY(SaveGame) FString Map;
    UPROPERTY(SaveGame) FString ActorName;
    UPROPERTY(SaveGame) FTransform Transform;
    UPROPERTY(SaveGame) FString Shelf;
    UPROPERTY(SaveGame) FString Slot;
    UPROPERTY(SaveGame) int32 LegacyNumber = 0;
    UPROPERTY(SaveGame) bool bCremated = false;
    UPROPERTY(SaveGame) bool bPlacedInLevel = false;
};

USTRUCT()
struct FChronosFridgeStock
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) int32 Day = 0;
    UPROPERTY(SaveGame) int32 Remaining = 4095;
};

USTRUCT()
struct FChronosBodyRecord
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FGuid Id;
    UPROPERTY(SaveGame) FDataTableRowHandle Deceased;
    UPROPERTY(SaveGame) FString DisplayName;
    UPROPERTY(SaveGame) FString Map;
    UPROPERTY(SaveGame) FString ActorName;
    UPROPERTY(SaveGame) bool bLakeBody = false;
    UPROPERTY(SaveGame) bool bCremated = false;
    UPROPERTY(SaveGame) FString SourceKey;
};

USTRUCT()
struct FChronosProgress
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) int32 Version = 1;
    UPROPERTY(SaveGame) TMap<FName, int32> Items;
    UPROPERTY(SaveGame) TMap<FString, FChronosFridgeStock> Refrigerators;
    UPROPERTY(SaveGame) TMap<FGuid, FChronosUrnRecord> Urns;
    UPROPERTY(SaveGame) TMap<FGuid, FChronosBodyRecord> Bodies;
    UPROPERTY(SaveGame) TMap<FString, FGuid> Requests;
    UPROPERTY(SaveGame) TSet<FGuid> PaidDeliveries;
    UPROPERTY(SaveGame) int64 Dollars = 0;
    UPROPERTY(SaveGame) FName EquippedItem;
    UPROPERTY(SaveGame) int32 NextUrnNumber = 10001;
};

UCLASS()
class CHRONOSHUD_API UChronosProgressState : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(Transient) FChronosProgress Data;
    bool bRestoredFromSave = false;
    static UChronosProgressState* Get(const UObject* Context);
    UFUNCTION(BlueprintPure, Category="Chronos|Inventory") int32 Count(FName Item) const;
    UFUNCTION(BlueprintCallable, Category="Chronos|Inventory") bool Add(FName Item, int32 Quantity);
    UFUNCTION(BlueprintCallable, Category="Chronos|Inventory") bool Remove(FName Item, int32 Quantity);
    UFUNCTION(BlueprintPure, Category="Chronos|Economy") int64 GetDollars() const { return Data.Dollars; }
    UFUNCTION(BlueprintCallable, Category="Chronos|Economy") bool PayDelivery(FGuid Urn);
    void ResetProgress() { Data = FChronosProgress(); bRestoredFromSave = false; }
};
