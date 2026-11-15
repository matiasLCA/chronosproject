#pragma once

#include "GameFramework/SaveGame.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ChronosPlayerSave.generated.h"

USTRUCT()
struct FChronosTaskSave
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    FSoftClassPath TaskClass;

    UPROPERTY(SaveGame)
    bool bCompleted = false;
};

UCLASS()
class CHRONOSHUD_API UChronosPlayerSave : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(SaveGame)
    FGuid Revision;

    UPROPERTY(SaveGame)
    FString MapPackage;

    UPROPERTY(SaveGame)
    FDateTime SavedUtc;

    UPROPERTY(SaveGame)
    TArray<uint8> PreviewPng;

    UPROPERTY(SaveGame)
    int32 CalendarDay = 0;

    UPROPERTY(SaveGame)
    int32 CalendarMinute = INDEX_NONE;

    UPROPERTY(SaveGame)
    double Health = -1.0;

    UPROPERTY(SaveGame)
    double Stamina = -1.0;

    UPROPERTY(SaveGame)
    bool bLampInHand = false;

    UPROPERTY(SaveGame)
    bool bHasLampState = false;

    UPROPERTY(SaveGame)
    TArray<uint8> JournalData;

    UPROPERTY(SaveGame)
    bool bHasDailyTasks = false;

    UPROPERTY(SaveGame)
    TArray<FChronosTaskSave> DailyTasks;

    bool bAppliedDailyTasks = false;
};

class UGameInstance;
class ACharacter;

namespace ChronosPlayerSave
{
    struct FSlot
    {
        int32 Number = 0;
        FString Label;
        FString Calendar;
        FString Level;
        FDateTime LocalTime;
        FDateTime SortTime;
        bool bReadable = false;
    };

    FString SlotName(int32 Number);
    UChronosPlayerSave* Read(int32 Number);
    CHRONOSHUD_API TArray<FSlot> List();
    FString Map(const UChronosPlayerSave& Save);
    bool CanLoad(const UChronosPlayerSave& Save);
    bool SaveSlot(UGameInstance& Instance, int32 Number);
    bool LoadSlot(UGameInstance& Instance, int32 Number);
    void ReturnToMainMenu(UGameInstance& Instance, UUserWidget& From);
    UChronosPlayerSave* LoadedFrom(const UObject* Context);
}

UCLASS()
class CHRONOSHUD_API UChronosSaveHooks : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject"), Category="YukoDev|Partidas")
    static bool WritePlayerSave(const UObject* WorldContextObject, USaveGame* SaveGameObject, const FString& SlotName, int32 UserIndex);

    UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject"), Category="YukoDev|Partidas")
    static void TravelToPlayerSave(const UObject* WorldContextObject, USaveGame* SaveGameObject);

    UFUNCTION(BlueprintCallable, Category="YukoDev|Partidas")
    static void RestorePlayerSave(ACharacter* John, USaveGame* SaveGameObject);

    UFUNCTION(BlueprintCallable, Category="YukoDev|Partidas")
    static bool RestoreDailyTasks(AActor* Manager);
};
