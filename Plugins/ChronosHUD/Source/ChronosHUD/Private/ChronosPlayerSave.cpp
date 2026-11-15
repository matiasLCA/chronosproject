#include "ChronosPlayerSave.h"
#include "ChronosJournal.h"
#include "ChronosSaveThumbnail.h"

#include "Blueprint/UserWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/FileManager.h"
#include "PlatformFeatures.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "SaveGameSystem.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

#define LOCTEXT_NAMESPACE "ChronosPlayerSave"

namespace
{
    constexpr int32 User = 0;

    bool SlotNumber(const FString& Name, int32& Number)
    {
        return Name.StartsWith(TEXT("Partida_"), ESearchCase::CaseSensitive)
            && LexTryParseString(Number, *Name.Mid(8))
            && Number > 0 && Name == ChronosPlayerSave::SlotName(Number);
    }

    UObject* ObjectField(const UObject* Owner, FName Field)
    {
        const auto* Property = Owner ? FindFProperty<FObjectPropertyBase>(Owner->GetClass(), Field) : nullptr;
        return Property ? Property->GetObjectPropertyValue_InContainer(Owner) : nullptr;
    }

    double NumberField(const UObject* Owner, FName Field, double Missing = -1.0)
    {
        const auto* Property = Owner ? FindFProperty<FNumericProperty>(Owner->GetClass(), Field) : nullptr;
        if (!Property) return Missing;
        const auto* Value = Property->ContainerPtrToValuePtr<void>(Owner);
        return Property->IsFloatingPoint() ? Property->GetFloatingPointPropertyValue(Value) : Property->GetSignedIntPropertyValue(Value);
    }

    void SetNumber(UObject* Owner, FName Field, double Value)
    {
        auto* Property = Owner ? FindFProperty<FNumericProperty>(Owner->GetClass(), Field) : nullptr;
        if (!Property) return;
        auto* Address = Property->ContainerPtrToValuePtr<void>(Owner);
        if (Property->IsFloatingPoint()) Property->SetFloatingPointPropertyValue(Address, Value);
        else Property->SetIntPropertyValue(Address, static_cast<int64>(Value));
    }

    void InvokeEvent(UObject& Object, FName Name)
    {
        if (auto* Function = Object.FindFunction(Name); Function && Function->NumParms == 0)
            Object.ProcessEvent(Function, nullptr);
    }

    const FArrayProperty* TaskArray(const UObject* Manager)
    {
        const auto* Array = IsValid(Manager) ? FindFProperty<FArrayProperty>(Manager->GetClass(), TEXT("tareas")) : nullptr;
        return Array && CastField<FObjectPropertyBase>(Array->Inner) ? Array : nullptr;
    }

    void CaptureTasks(UWorld& World, UChronosPlayerSave& Save)
    {
        const auto* Manager = ObjectField(World.GetGameState(), TEXT("gestorTareas"));
        const auto* Array = TaskArray(Manager);
        if (!Array) return;
        Save.bHasDailyTasks = true;
        FScriptArrayHelper Tasks(Array, Array->ContainerPtrToValuePtr<void>(Manager));
        const auto* Item = CastFieldChecked<FObjectPropertyBase>(Array->Inner);
        for (int32 Index = 0; Index < Tasks.Num(); ++Index)
        {
            const auto* Task = Item->GetObjectPropertyValue(Tasks.GetRawPtr(Index));
            if (!IsValid(Task)) continue;
            const auto* Done = FindFProperty<FBoolProperty>(Task->GetClass(), TEXT("completada"));
            FChronosTaskSave& State = Save.DailyTasks.AddDefaulted_GetRef();
            State.TaskClass = FSoftClassPath(Task->GetClass());
            State.bCompleted = Done && Done->GetPropertyValue_InContainer(Task);
        }
    }

    bool InvokeSlot(UGameInstance& Instance, FName Function, int32 Number)
    {
        UFunction* Call = Instance.FindFunction(Function);
        const auto* Slot = Call ? FindFProperty<FIntProperty>(Call, TEXT("slot")) : nullptr;
        if (!Slot || Call->NumParms != 1) return false;
        FStructOnScope Args(Call);
        Slot->SetPropertyValue_InContainer(Args.GetStructMemory(), Number);
        Instance.ProcessEvent(Call, Args.GetStructMemory());
        return true;
    }

    FString ResolveMap(const UChronosPlayerSave& Save)
    {
        const FString Name = ChronosPlayerSave::Map(Save);
        if (Name.IsEmpty() || Name == TEXT("None")) return {};
        if (FPackageName::IsShortPackageName(Name))
        {
            // NombreNivel guardaba el nombre corto de este mapa en las partidas anteriores.
            if (Name.Equals(TEXT("CHRONOSALPHA"), ESearchCase::IgnoreCase)) return TEXT("/Game/CHRONOSALPHA");
            FString Resolved;
            if (FPackageName::SearchForPackageOnDisk(Name, &Resolved)) return Resolved;
            return {};
        }
        return FPackageName::ObjectPathToPackageName(Name);
    }

    FDateTime LegacyDate(const UChronosPlayerSave& Save)
    {
        const auto* Field = FindFProperty<FStructProperty>(Save.GetClass(), TEXT("FechaGuardado"));
        return Field && Field->Struct == TBaseStructure<FDateTime>::Get()
            ? *Field->ContainerPtrToValuePtr<FDateTime>(&Save) : FDateTime();
    }

    AActor* CemeteryClock(UWorld& World)
    {
        UClass* Class = LoadClass<AActor>(nullptr, TEXT("/Game/NPC/Visitors/LivingCemetery/BP_CemeteryDirector.BP_CemeteryDirector_C"));
        if (Class)
            for (TActorIterator<AActor> Actor(&World, Class); Actor; ++Actor) return *Actor;
        return nullptr;
    }

    bool WriteSaveBytes(const FString& Slot, const TArray<uint8>& Data, int32 UserIndex)
    {
        ISaveGameSystem* System = IPlatformFeaturesModule::Get().GetSaveGameSystem();
        if (!System) return false;
#if PLATFORM_WINDOWS
        const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("SaveGames"));
        const FString Final = FPaths::ConvertRelativePathToFull(Folder / (Slot + TEXT(".sav")));
        const FString Temp = Final + TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
        if (!IFileManager::Get().MakeDirectory(*Folder, true) && !IFileManager::Get().DirectoryExists(*Folder)) return false;
        const bool bWritten = FFileHelper::SaveArrayToFile(Data, *Temp);
        if (bWritten && ::MoveFileExW(*Temp, *Final, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
        IFileManager::Get().Delete(*Temp);
        return false;
#else
        return System->SaveGame(false, *Slot, UserIndex, Data);
#endif
    }
}

FString ChronosPlayerSave::SlotName(int32 Number)
{
    return FString::Printf(TEXT("Partida_%d"), Number);
}

UChronosPlayerSave* ChronosPlayerSave::Read(int32 Number)
{
    if (Number <= 0) return nullptr;
    TArray<uint8> Data;
    if (!UGameplayStatics::LoadDataFromSlot(Data, SlotName(Number), User) || Data.Num() < 32
        || FMemory::Memcmp(Data.GetData(), "GVAS", 4) != 0) return nullptr;
    return Cast<UChronosPlayerSave>(UGameplayStatics::LoadGameFromMemory(Data));
}

FString ChronosPlayerSave::Map(const UChronosPlayerSave& Save)
{
    if (!Save.MapPackage.IsEmpty()) return Save.MapPackage;
    const auto* Field = FindFProperty<FNameProperty>(Save.GetClass(), TEXT("NombreNivel"));
    return Field ? Field->GetPropertyValue_InContainer(&Save).ToString() : FString();
}

bool ChronosPlayerSave::CanLoad(const UChronosPlayerSave& Save)
{
    const FString Name = ResolveMap(Save);
    return FPackageName::IsValidLongPackageName(Name) && FPackageName::DoesPackageExist(Name);
}

TArray<ChronosPlayerSave::FSlot> ChronosPlayerSave::List()
{
    TArray<FSlot> Slots;
    TArray<FString> Found;
    if (auto* System = IPlatformFeaturesModule::Get().GetSaveGameSystem()) System->GetSaveGameNames(Found, User);
    for (const FString& Name : Found)
    {
        int32 Number = 0;
        if (!SlotNumber(Name, Number)) continue;
        FSlot& Slot = Slots.AddDefaulted_GetRef();
        Slot.Number = Number;
        Slot.Label = FString::Printf(TEXT("Partida %03d"), Number);
        const auto* Save = Read(Number);
        if (!Save) continue;
        Slot.bReadable = CanLoad(*Save);
        Slot.LocalTime = LegacyDate(*Save);
        Slot.SortTime = Slot.LocalTime;
        Slot.Level = FPackageName::GetShortName(Map(*Save));
        if (Slot.Level.Equals(TEXT("CHRONOSALPHA"), ESearchCase::IgnoreCase))
            Slot.Level = LOCTEXT("Cemetery", "Cementerio del Silencio Profundo").ToString();
        const int32 Day = Save->CalendarDay > 0 ? Save->CalendarDay : static_cast<int32>(NumberField(Save, TEXT("DiaActual"), 0.0));
        if (Day > 0)
            Slot.Calendar = Save->CalendarMinute >= 0
                ? FString::Printf(TEXT("Día %d · %02d:%02d"), Day, Save->CalendarMinute / 60, Save->CalendarMinute % 60)
                : FString::Printf(TEXT("Día %d"), Day);
    }
    Slots.Sort([](const FSlot& A, const FSlot& B)
    {
        return A.SortTime == B.SortTime ? A.Number > B.Number : A.SortTime > B.SortTime;
    });
    return Slots;
}

bool ChronosPlayerSave::SaveSlot(UGameInstance& Instance, int32 Number)
{
    const auto* Previous = Read(Number);
    const FGuid Revision = Previous ? Previous->Revision : FGuid();
    if (!InvokeSlot(Instance, TEXT("GuardarPartida"), Number)) return false;
    const auto* Written = Read(Number);
    if (!Written || !Written->Revision.IsValid() || Written->Revision == Revision) return false;
    SetNumber(&Instance, TEXT("slotActual"), Number);
    return true;
}

bool ChronosPlayerSave::LoadSlot(UGameInstance& Instance, int32 Number)
{
    const auto* Save = Read(Number);
    return Save && CanLoad(*Save) && InvokeSlot(Instance, TEXT("cargarPartida"), Number)
        && Cast<UChronosPlayerSave>(ObjectField(&Instance, TEXT("partidaPendiente"))) != nullptr;
}

void ChronosPlayerSave::ReturnToMainMenu(UGameInstance& Instance, UUserWidget& From)
{
    UFunction* Call = Instance.FindFunction(TEXT("cerrarOpciones"));
    const auto* Widget = Call ? FindFProperty<FObjectPropertyBase>(Call, TEXT("desde")) : nullptr;
    if (!Widget || Call->NumParms != 1) return;
    FStructOnScope Args(Call);
    Widget->SetObjectPropertyValue_InContainer(Args.GetStructMemory(), &From);
    Instance.ProcessEvent(Call, Args.GetStructMemory());
}

UChronosPlayerSave* ChronosPlayerSave::LoadedFrom(const UObject* Context)
{
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(Context, 0);
    return Cast<UChronosPlayerSave>(ObjectField(Pawn, TEXT("partidaCargada")));
}

// YukoDev — SISTEMA DE GUARDADO
bool UChronosSaveHooks::WritePlayerSave(const UObject* WorldContextObject, USaveGame* SaveGameObject, const FString& SlotName, int32 UserIndex)
{
    auto* Save = Cast<UChronosPlayerSave>(SaveGameObject);
    auto* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
    auto* Player = World ? World->GetFirstPlayerController() : nullptr;
    int32 Number = 0;
    if (!Save || !Player || !Player->GetPawn() || !SlotNumber(SlotName, Number)) return false;

    Save->Revision = FGuid::NewGuid();
    Save->SavedUtc = FDateTime::UtcNow();
    const auto* DateField = FindFProperty<FStructProperty>(Save->GetClass(), TEXT("FechaGuardado"));
    if (DateField && DateField->Struct == TBaseStructure<FDateTime>::Get())
        *DateField->ContainerPtrToValuePtr<FDateTime>(Save) = FDateTime::Now();
    Save->MapPackage = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
    Save->Health = NumberField(ObjectField(Player->GetPawn(), TEXT("Health")), TEXT("CurrentHealth"));
    Save->Stamina = NumberField(Player->GetPawn(), TEXT("CurrentStamina"));
    if (const auto* Lamp = ObjectField(Player->GetPawn(), TEXT("flashlight")))
        if (const auto* InHand = FindFProperty<FBoolProperty>(Lamp->GetClass(), TEXT("enMano")))
        {
            Save->bHasLampState = true;
            Save->bLampInHand = InHand->GetPropertyValue_InContainer(Lamp);
        }
    UChronosJournal::Snapshot(World, Save->JournalData);
    CaptureTasks(*World, *Save);
    if (const AActor* Clock = CemeteryClock(*World))
    {
        Save->CalendarDay = NumberField(Clock, TEXT("CalendarDay"), 0.0);
        const int32 Hour = NumberField(Clock, TEXT("HourOfDay"));
        const int32 Minute = NumberField(Clock, TEXT("MinuteOfHour"));
        if (Hour >= 0 && Hour < 24 && Minute >= 0 && Minute < 60) Save->CalendarMinute = Hour * 60 + Minute;
    }
    if (!ChronosSaveThumbnail::Capture(*Player, Save->PreviewPng)) return false;
    TArray<uint8> Bytes;
    return UGameplayStatics::SaveGameToMemory(Save, Bytes) && WriteSaveBytes(SlotName, Bytes, UserIndex);
}

// YukoDev — SISTEMA DE CARGA
void UChronosSaveHooks::TravelToPlayerSave(const UObject* WorldContextObject, USaveGame* SaveGameObject)
{
    auto* Instance = UGameplayStatics::GetGameInstance(WorldContextObject);
    auto* Save = Cast<UChronosPlayerSave>(SaveGameObject);
    if (!Instance) return;
    if (!Save || !ChronosPlayerSave::CanLoad(*Save))
    {
        if (auto* Pending = FindFProperty<FObjectPropertyBase>(Instance->GetClass(), TEXT("partidaPendiente")))
            Pending->SetObjectPropertyValue_InContainer(Instance, nullptr);
        return;
    }

    if (IsValid(ObjectField(Instance, TEXT("reproductorMenu"))))
        if (auto* CloseVideo = Instance->FindFunction(TEXT("cerrarVideoMenu")); CloseVideo && CloseVideo->NumParms == 0)
            Instance->ProcessEvent(CloseVideo, nullptr);
    if (auto* Pause = FindFProperty<FObjectPropertyBase>(Instance->GetClass(), TEXT("menuPausaActivo")))
    {
        if (auto* Menu = Cast<UUserWidget>(Pause->GetObjectPropertyValue_InContainer(Instance))) Menu->RemoveFromParent();
        Pause->SetObjectPropertyValue_InContainer(Instance, nullptr);
    }
    if (auto* PC = Instance->GetFirstLocalPlayerController())
    {
        PC->SetPause(false);
        PC->SetInputMode(FInputModeGameOnly());
        PC->FlushPressedKeys();
        PC->SetShowMouseCursor(false);
    }
    UGameplayStatics::OpenLevel(WorldContextObject, FName(ResolveMap(*Save)));
}

void UChronosSaveHooks::RestorePlayerSave(ACharacter* John, USaveGame* SaveGameObject)
{
    const auto* Save = Cast<UChronosPlayerSave>(SaveGameObject);
    if (!John || !Save || !Save->Revision.IsValid()) return;
    if (Save->Health >= 0 && FMath::IsFinite(Save->Health))
    {
        auto* Health = ObjectField(John, TEXT("Health"));
        SetNumber(Health, TEXT("CurrentHealth"), FMath::Clamp(Save->Health, 0.0, NumberField(Health, TEXT("MaxHealth"), Save->Health)));
    }
    if (Save->Stamina >= 0 && FMath::IsFinite(Save->Stamina))
        SetNumber(John, TEXT("CurrentStamina"), FMath::Clamp(Save->Stamina, 0.0, NumberField(John, TEXT("MaxStamina"), Save->Stamina)));
    if (Save->bHasLampState)
        if (auto* Lamp = Cast<AActor>(ObjectField(John, TEXT("flashlight"))); Lamp && John->GetMesh())
            if (const auto* InHand = FindFProperty<FBoolProperty>(Lamp->GetClass(), TEXT("enMano")))
            {
                InHand->SetPropertyValue_InContainer(Lamp, Save->bLampInHand);
                Lamp->AttachToComponent(John->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale,
                    Save->bLampInHand ? TEXT("socket_linterna") : TEXT("cintura_flashlight"));
                InvokeEvent(*Lamp, TEXT("actualizarLuz"));
            }
}

bool UChronosSaveHooks::RestoreDailyTasks(AActor* Manager)
{
    const auto* Array = TaskArray(Manager);
    auto* World = Array ? Manager->GetWorld() : nullptr;
    if (!World) return false;
    auto* Save = Cast<UChronosPlayerSave>(ObjectField(World->GetGameInstance(), TEXT("partidaPendiente")));
    if (!Save) Save = ChronosPlayerSave::LoadedFrom(Manager);
    if (!Save || !Save->bHasDailyTasks || Save->bAppliedDailyTasks) return false;

    const auto* Item = CastFieldChecked<FObjectPropertyBase>(Array->Inner);
    FScriptArrayHelper Tasks(Array, Array->ContainerPtrToValuePtr<void>(Manager));
    if (Tasks.Num() != 0) return false;
    TArray<UClass*> Classes;
    for (const auto& Record : Save->DailyTasks)
    {
        auto* Class = Record.TaskClass.TryLoadClass<AActor>();
        if (!Class || !Class->IsChildOf(Item->PropertyClass) || Class->HasAnyClassFlags(CLASS_Abstract)) return false;
        Classes.Add(Class);
    }

    TArray<AActor*> Spawned;
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (auto* Class : Classes)
    {
        auto* Task = World->SpawnActor<AActor>(Class, FTransform::Identity, Spawn);
        if (!Task)
        {
            for (auto* Created : Spawned) Created->Destroy();
            return false;
        }
        Spawned.Add(Task);
    }
    Save->bAppliedDailyTasks = true;
    for (int32 Index = 0; Index < Spawned.Num(); ++Index)
    {
        AActor* Task = Spawned[Index];
        if (Save->DailyTasks[Index].bCompleted)
        {
            if (const auto* Done = FindFProperty<FBoolProperty>(Task->GetClass(), TEXT("completada")))
                Done->SetPropertyValue_InContainer(Task, true);
        }
        else InvokeEvent(*Task, TEXT("iniciar"));
        Item->SetObjectPropertyValue(Tasks.GetRawPtr(Tasks.AddValue()), Task);
    }
    InvokeEvent(*Manager, TEXT("tareasActualizadas"));
    return true;
}

#undef LOCTEXT_NAMESPACE
