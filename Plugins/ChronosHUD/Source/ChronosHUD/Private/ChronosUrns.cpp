#include "ChronosUrns.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SceneComponent.h"
#include "Components/TextBlock.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Misc/Crc.h"
#include "TimerManager.h"
#include "GameFramework/GameStateBase.h"

namespace
{
    TMap<TWeakObjectPtr<AActor>, FChronosUrnRecord> PendingCremations;

    UObject* Object(const UObject* Owner, FName Name)
    {
        const auto* P = IsValid(Owner) ? FindFProperty<FObjectPropertyBase>(Owner->GetClass(), Name) : nullptr;
        return P ? P->GetObjectPropertyValue_InContainer(Owner) : nullptr;
    }

    void SetObject(UObject* Owner, FName Name, UObject* Value)
    {
        if (auto* P = IsValid(Owner) ? FindFProperty<FObjectPropertyBase>(Owner->GetClass(), Name) : nullptr)
            P->SetObjectPropertyValue_InContainer(Owner, Value);
    }

    int32 Number(const UObject* Owner, FName Name)
    {
        const auto* P = IsValid(Owner) ? FindFProperty<FIntProperty>(Owner->GetClass(), Name) : nullptr;
        return P ? P->GetPropertyValue_InContainer(Owner) : 0;
    }

    void SetNumber(UObject* Owner, FName Name, int32 Value)
    {
        if (auto* P = IsValid(Owner) ? FindFProperty<FIntProperty>(Owner->GetClass(), Name) : nullptr)
            P->SetPropertyValue_InContainer(Owner, Value);
    }

    FString RowName(const FDataTableRowHandle& Row)
    {
        const auto* Table = Row.DataTable.Get();
        if (!Table || !Table->GetPathName().StartsWith(TEXT("/Game/Interactivo/Tombstone/DT_Tombstones"))) return {};
        const auto* Data = Table->FindRowUnchecked(Row.RowName);
        if (!Data) return {};
        for (TFieldIterator<FProperty> Field(Table->GetRowStruct()); Field; ++Field)
        {
            if (!Field->GetName().StartsWith(TEXT("TombstoneName"))) continue;
            if (const auto* Text = CastField<FTextProperty>(*Field)) return Text->GetPropertyValue_InContainer(Data).ToString();
            if (const auto* String = CastField<FStrProperty>(*Field)) return String->GetPropertyValue_InContainer(Data);
        }
        return {};
    }

    void Call(UObject* Target, FName Method)
    {
        auto* Function = IsValid(Target) ? Target->FindFunction(Method) : nullptr;
        if (Function && Function->NumParms == 0) Target->ProcessEvent(Function, nullptr);
    }

    FString Map(UWorld* World) { return UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()); }

    const UDataTable* RegistryTable()
    {
        const auto* Config = LoadObject<UDataTable>(nullptr, TEXT("/Game/NPC_System/GraveRegistry/DT_GraveRegistryConfig.DT_GraveRegistryConfig"));
        if (!Config) return nullptr;
        const auto* Default = Config->FindRowUnchecked(TEXT("Default"));
        if (!Default) return nullptr;
        for (TFieldIterator<FObjectPropertyBase> Field(Config->GetRowStruct()); Field; ++Field)
            if (Field->GetName().StartsWith(TEXT("GraveDataTable")))
                return Cast<UDataTable>(Field->GetObjectPropertyValue_InContainer(Default));
        return nullptr;
    }

    FString IdentityKey(FString Name)
    {
        Name.TrimStartAndEndInline();
        Name.ToLowerInline();
        while (Name.ReplaceInline(TEXT("  "), TEXT(" ")) > 0) {}
        return Name;
    }

    bool SelectUnusedIdentity(AActor* Context, const FGuid& Excluded, FDataTableRowHandle& Result)
    {
        const UDataTable* Table = RegistryTable();
        const auto* Progress = UChronosProgressState::Get(Context);
        if (!Table || !Progress || !Context->GetWorld()) return false;
        TSet<FString> Used;
        const auto Reserve = [&](const FDataTableRowHandle& Row, const FString& Name)
        {
            const FString Value = RowName(Row);
            if (!Value.IsEmpty()) Used.Add(IdentityKey(Value));
            if (!Name.IsEmpty()) Used.Add(IdentityKey(Name));
        };
        for (const auto& Pair : Progress->Data.Urns)
            if (Pair.Key != Excluded) Reserve(Pair.Value.Deceased, Pair.Value.DisplayName);
        for (const auto& Pair : Progress->Data.Bodies)
            if (Pair.Key != Excluded) Reserve(Pair.Value.Deceased, Pair.Value.DisplayName);
        // Read the actual grave actors, including child actors of niches and rows assigned during play.
        for (TActorIterator<AActor> It(Context->GetWorld()); It; ++It)
        {
            const auto* Row = FindFProperty<FNameProperty>(It->GetClass(), TEXT("TombstoneRow"));
            if (!Row) continue;
            FDataTableRowHandle Handle; Handle.DataTable = Table; Handle.RowName = Row->GetPropertyValue_InContainer(*It);
            FString Name;
            if (const auto* Field = FindFProperty<FStrProperty>(It->GetClass(), TEXT("TombstoneName")))
                Name = Field->GetPropertyValue_InContainer(*It);
            Reserve(Handle, Name);
            if (const auto* Plaque = Cast<UTextRenderComponent>(Object(*It, TEXT("Name"))))
                if (!Plaque->Text.IsEmpty()) Used.Add(IdentityKey(Plaque->Text.ToString()));
        }
        auto Rows = Table->GetRowNames(); Rows.Sort(FNameLexicalLess());
        if (!Rows.IsEmpty())
        {
            const int32 Start = FCrc::StrCrc32(*Excluded.ToString()) % Rows.Num();
            for (int32 Offset = 0; Offset < Rows.Num(); ++Offset)
            {
                FDataTableRowHandle Candidate; Candidate.DataTable = Table; Candidate.RowName = Rows[(Start + Offset) % Rows.Num()];
                const FString Name = RowName(Candidate);
                if (!Name.IsEmpty() && !Used.Contains(IdentityKey(Name))) { Result = Candidate; return true; }
            }
        }
        UE_LOG(LogTemp, Error, TEXT("ChronosIdentity: no unused deceased names in %s; assignment blocked for %s."), *Table->GetPathName(), *Context->GetPathName());
        return false;
    }

    bool AddToArray(AActor* Owner, FName Name, UObject* Item)
    {
        const auto* Property = FindFProperty<FArrayProperty>(Owner->GetClass(), Name);
        const auto* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
        if (!Inner || !Item->IsA(Inner->PropertyClass)) return false;
        FScriptArrayHelper Array(Property, Property->ContainerPtrToValuePtr<void>(Owner));
        for (int32 Index = 0; Index < Array.Num(); ++Index)
            if (Inner->GetObjectPropertyValue(Array.GetRawPtr(Index)) == Item) return true;
        Inner->SetObjectPropertyValue(Array.GetRawPtr(Array.AddValue()), Item);
        return true;
    }

    FString BodySourceKey(AActor* Body, bool bLake)
    {
        if(bLake)return Map(Body->GetWorld())+TEXT("|LakeBody");
        auto* Coffin=Object(Body,TEXT("ataudpadre"));
        auto* Manager=Object(Body->GetWorld()->GetGameState(),TEXT("gestorTareas"));
        const auto* Tasks=Manager?FindFProperty<FArrayProperty>(Manager->GetClass(),TEXT("tareas")):nullptr;
        const auto* Item=Tasks?CastField<FObjectPropertyBase>(Tasks->Inner):nullptr;
        if(!Coffin || !Item)return {};
        int32 Day=1;
        for(TActorIterator<AActor> It(Body->GetWorld());It;++It)
            if(It->GetClass()->GetName()==TEXT("BP_cycle_C")){Day=Number(*It,TEXT("CurrentDay"));break;}
        FScriptArrayHelper Values(Tasks,Tasks->ContainerPtrToValuePtr<void>(Manager));
        for(int32 Index=0;Index<Values.Num();++Index)
        {
            auto* Task=Item->GetObjectPropertyValue(Values.GetRawPtr(Index));
            if(Object(Task,TEXT("ataud"))==Coffin)
                return FString::Printf(TEXT("%s|Day%d|Task%d|%s"),*Map(Body->GetWorld()),Day,Index,*Task->GetClass()->GetPathName());
        }
        return {};
    }
}

void AChronosBody::BeginPlay()
{
    Super::BeginPlay();
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this]{Identify();}));
}

bool AChronosBody::Identify()
{
    auto* Progress = UChronosProgressState::Get(this);
    if (!Progress) return false;
    bool bLake=false;
    for(const UClass* Class=GetClass();Class;Class=Class->GetSuperClass())bLake|=Class->GetName()==TEXT("BP_CadaverMision_C");
    const FString SourceKey=BodySourceKey(this,bLake);
    FGuid Matched;
    if(!SourceKey.IsEmpty())
        for(const auto& Pair:Progress->Data.Bodies)
            if(Pair.Value.SourceKey==SourceKey && !Pair.Value.bCremated)
            {
                Matched=Pair.Key;break;
            }
    if(Matched.IsValid() && Matched!=PersistentBodyId)
    {
        const auto* Temporary=Progress->Data.Bodies.Find(PersistentBodyId);
        if(Temporary && Temporary->SourceKey.IsEmpty() && !Progress->Data.Urns.Contains(PersistentBodyId))
            Progress->Data.Bodies.Remove(PersistentBodyId);
        PersistentBodyId=Matched;
    }
    if (!PersistentBodyId.IsValid()) PersistentBodyId = FGuid::NewGuid();
    auto& Record = Progress->Data.Bodies.FindOrAdd(PersistentBodyId);
    if (!Record.Id.IsValid())
    {
        Record.Id = PersistentBodyId; Record.Map = Map(GetWorld()); Record.ActorName = GetName();
        Record.bLakeBody=bLake;
        if (Record.bLakeBody) Record.DisplayName = TEXT("Desconocido");
    }
    if(!SourceKey.IsEmpty())Record.SourceKey=SourceKey;
    if (!Record.bLakeBody && Record.Deceased.IsNull())
    {
        if (!SelectUnusedIdentity(this, PersistentBodyId, Record.Deceased)) return false;
        Record.DisplayName = RowName(Record.Deceased);
    }
    DeceasedRecord = Record.Deceased;
    DeceasedName = FText::FromString(Record.DisplayName);
    UE_LOG(LogTemp, Display, TEXT("ChronosIdentity: body=%s id=%s row=%s name=%s lake=%d"), *GetName(), *PersistentBodyId.ToString(), *Record.Deceased.RowName.ToString(), *Record.DisplayName, Record.bLakeBody);
    return !DeceasedName.IsEmpty();
}

FText UChronosUrns::BodyName(AActor* Actor)
{
    const auto* Body = Cast<AChronosBody>(Actor);
    return Body ? Body->DeceasedName : FText::GetEmpty();
}

FChronosUrnRecord* AChronosUrn::Record()
{
    auto* Progress = UChronosProgressState::Get(this);
    return Progress ? Progress->Data.Urns.Find(PersistentId) : nullptr;
}

void AChronosUrn::BeginPlay()
{
    Super::BeginPlay();
    UChronosUrns::InitializeUrn(this);
}

void AChronosUrn::RefreshLabel()
{
    const auto* Identity = Record();
    if (!Identity || Identity->DisplayName.IsEmpty() || !GetRootComponent() || !FSlateApplication::IsInitialized()) return;
    auto* Font = LoadObject<UFont>(nullptr, TEXT("/Game/assets/Cementerio/Tipografia/TrajanPro3SemiBold_Font.TrajanPro3SemiBold_Font"));
    const FLinearColor Gold(FColor(237, 207, 157));
    const FText Name = FText::FromString(Identity->DisplayName);
    TInlineComponentArray<UWidgetComponent*> Labels(this);
    bool bHasWidgetLabel = false;
    for (auto* LabelWidget : Labels)
    {
        if (LabelWidget == Plaque) continue;
        LabelWidget->InitWidget();
        if (auto* Widget = LabelWidget->GetUserWidgetObject(); Widget && Widget->WidgetTree)
            Widget->WidgetTree->ForEachWidget([&](UWidget* Child)
            {
                if (auto* Text = Cast<UTextBlock>(Child))
                {
                    Text->SetFont(FSlateFontInfo(Font, 20));
                    Text->SetColorAndOpacity(FSlateColor(Gold));
                    Text->SetAutoWrapText(true);
                    Text->SetJustification(ETextJustify::Center);
                    bHasWidgetLabel = true;
                }
            });
    }
    if (bHasWidgetLabel) return;
    if (!Plaque)
    {
        FVector Center, Extent;
        GetActorBounds(false, Center, Extent);
        Plaque = NewObject<UWidgetComponent>(this, TEXT("EtiquetaAnfora"));
        Plaque->SetupAttachment(GetRootComponent());
        Plaque->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Plaque->SetDrawAtDesiredSize(true);
        Plaque->SetBlendMode(EWidgetBlendMode::Transparent);
        Plaque->SetTwoSided(true);
        Plaque->SetSlateWidget(SAssignNew(PlaqueText, STextBlock).Text(Name).Font(FSlateFontInfo(Font, 22))
            .ColorAndOpacity(Gold).ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(FLinearColor::Black));
        Plaque->RegisterComponent();
        Plaque->SetWorldScale3D(FVector(FMath::Clamp(Extent.Z * .01, .075, .16)));
        Plaque->SetWorldLocation(Center + GetActorForwardVector() * (FMath::Max(Extent.X, Extent.Y) + .4) + FVector(0, 0, Extent.Z * .18));
    }
    if (PlaqueText) PlaqueText->SetText(Name);
}

void UChronosUrns::InitializeUrn(AActor* Actor)
{
    auto* Urn = Cast<AChronosUrn>(Actor);
    auto* Progress = Urn ? UChronosProgressState::Get(Urn) : nullptr;
    if (!Progress) return;
    const bool bPlaced = Urn->HasAnyFlags(RF_WasLoaded) || Urn->IsNetStartupActor();
    if (!Urn->PersistentId.IsValid())
    {
        for (const auto& Pair : Progress->Data.Urns)
            if (Pair.Value.bPlacedInLevel && bPlaced
                && Pair.Value.ActorName == Urn->GetName() && Pair.Value.Map == Map(Urn->GetWorld()))
            {
                Urn->PersistentId = Pair.Key;
                break;
            }
        if (!Urn->PersistentId.IsValid()) Urn->PersistentId = FGuid::NewGuid();
    }
    auto& Record = Progress->Data.Urns.FindOrAdd(Urn->PersistentId);
    if (!Record.Id.IsValid())
    {
        Record.Id = Urn->PersistentId;
        Record.Map = Map(Urn->GetWorld());
        Record.ActorName = Urn->GetName();
        Record.Transform = Urn->GetActorTransform();
        Record.bPlacedInLevel = bPlaced;
        const int32 Existing = Number(Urn, TEXT("numeroAnfora"));
        bool bCollision = false;
        for (const auto& Pair : Progress->Data.Urns)
            bCollision |= Pair.Key != Urn->PersistentId && Pair.Value.LegacyNumber == Existing;
        Record.LegacyNumber = Existing > 0 && !bCollision ? Existing : Progress->Data.NextUrnNumber++;
        Progress->Data.NextUrnNumber = FMath::Max(Progress->Data.NextUrnNumber, Record.LegacyNumber + 1);
    }
    SetNumber(Urn, TEXT("numeroAnfora"), Record.LegacyNumber);
    if (!Urn->DeceasedRecord.IsNull()) AssignDeceased(Urn, Urn->DeceasedRecord);
    if (Record.bPlacedInLevel && Record.Origin == EChronosUrnOrigin::Unverified)
    {
        FDataTableRowHandle Candidate;
        if (SelectUnusedIdentity(Urn, Urn->PersistentId, Candidate)) AssignDeceased(Urn, Candidate);
    }
    Urn->RefreshLabel();
    if (Record.State == EChronosUrnState::Available)
        if (auto* Shelf = Urn->GetAttachParentActor(); Shelf &&
            Shelf->GetClass()->GetPathName() == TEXT("/Game/Interactivo/bp_muebleAnfora/bp_slotAnfora.bp_slotAnfora_C"))
            TrackStored(Urn, Shelf);
    if (Record.State == EChronosUrnState::Disposed || Record.State == EChronosUrnState::Delivered) Urn->Destroy();
}

bool UChronosUrns::AssignDeceased(AActor* Actor, FDataTableRowHandle Deceased)
{
    auto* Urn = Cast<AChronosUrn>(Actor);
    auto* Progress = Urn ? UChronosProgressState::Get(Urn) : nullptr;
    auto* Record = Urn ? Urn->Record() : nullptr;
    const FString Name = RowName(Deceased);
    if (!Progress || !Record || Name.IsEmpty() || Record->Origin == EChronosUrnOrigin::UnknownLakeBody) return false;
    if (!Record->Deceased.IsNull() && Record->Deceased != Deceased) return false;
    for (const auto& Pair : Progress->Data.Urns)
        if (Pair.Key != Urn->PersistentId && Pair.Value.Deceased == Deceased) return false;
    Record->Origin = EChronosUrnOrigin::IdentifiedDeceased;
    Record->Deceased = Deceased;
    Record->DisplayName = Name;
    Urn->DeceasedRecord = Deceased;
    Urn->Origin = Record->Origin;
    Urn->RefreshLabel();
    return true;
}

FText UChronosUrns::Label(AActor* Actor)
{
    const auto* Urn = Cast<AChronosUrn>(Actor);
    const auto* Progress = Urn ? UChronosProgressState::Get(Urn) : nullptr;
    const auto* Record = Progress ? Progress->Data.Urns.Find(Urn->PersistentId) : nullptr;
    if (Record && Record->Origin == EChronosUrnOrigin::UnknownLakeBody) return FText::FromString(TEXT("Desconocido"));
    const FString Name = Record ? RowName(Record->Deceased) : FString();
    return FText::FromString(Name.IsEmpty() ? TEXT("Identificación pendiente") : Name);
}

bool UChronosUrns::CanDispose(AActor* Actor)
{
    const auto* Urn = Cast<AChronosUrn>(Actor);
    const auto* Progress = Urn ? UChronosProgressState::Get(Urn) : nullptr;
    const auto* Record = Progress ? Progress->Data.Urns.Find(Urn->PersistentId) : nullptr;
    return Record && Record->Origin == EChronosUrnOrigin::UnknownLakeBody && Record->bCremated
        && Record->Mission == TEXT("UnknownLakeBody") && Record->Deceased.IsNull()
        && Record->State == EChronosUrnState::Carried;
}

bool UChronosUrns::Dispose(AActor* Bin, AActor* Player)
{
    if (!CanUseBin(Bin, Player)) return false;
    auto* Urn = Cast<AChronosUrn>(Object(Player, TEXT("anforaCargada")));
    if (!CanDispose(Urn) || Urn->GetAttachParentActor() != Player) return false;
    Urn->Record()->State = EChronosUrnState::Disposed;
    SetObject(Player, TEXT("anforaCargada"), nullptr);
    Call(Bin, TEXT("abrirTapa"));
    Urn->Destroy();
    return true;
}

bool UChronosUrns::CanUseBin(AActor* Bin, AActor* Player)
{
    if (!IsValid(Bin) || !IsValid(Player) || Bin->GetClass()->GetPathName() != TEXT("/Game/Interactivo/bp_basura/Plastic_Trash_Bin.Plastic_Trash_Bin_C")
        || FVector::DistSquared(Bin->GetActorLocation(), Player->GetActorLocation()) > FMath::Square(400.f)) return false;
    auto* Urn = Cast<AChronosUrn>(Object(Player, TEXT("anforaCargada")));
    return CanDispose(Urn) && Urn->GetAttachParentActor() == Player;
}

bool UChronosUrns::PrepareCremation(AActor* Furnace, AActor* Body)
{
    auto* Lower = Cast<AActor>(Object(Furnace, TEXT("puertaInferior")));
    if (!IsValid(Lower) || !IsValid(Body)) return false;
    for (auto It = PendingCremations.CreateIterator(); It; ++It)
        if (!It.Key().IsValid()) It.RemoveCurrent();
    FChronosUrnRecord Record;
    auto* IdentifiedBody = Cast<AChronosBody>(Body);
    if (IdentifiedBody && !IdentifiedBody->Identify()) return false;
    Record.Id = IdentifiedBody ? IdentifiedBody->PersistentBodyId : FGuid::NewGuid();
    Record.bCremated = true;
    const auto* LakeClass = LoadClass<AActor>(nullptr, TEXT("/Game/Interactivo/cadaver_mision/BP_CadaverMision.BP_CadaverMision_C"));
    if (LakeClass && Body->IsA(LakeClass))
    {
        const auto* Decided = FindFProperty<FBoolProperty>(Body->GetClass(), TEXT("decidido"));
        if (!Decided || !Decided->GetPropertyValue_InContainer(Body)) return false;
        Record.Origin = EChronosUrnOrigin::UnknownLakeBody;
        Record.Mission = TEXT("UnknownLakeBody");
        Record.DisplayName = TEXT("Desconocido");
    }
    else if (const auto* Row = FindFProperty<FStructProperty>(Body->GetClass(), TEXT("DeceasedRecord"));
             Row && Row->Struct == FDataTableRowHandle::StaticStruct())
    {
        Record.Deceased = *Row->ContainerPtrToValuePtr<FDataTableRowHandle>(Body);
        Record.DisplayName = RowName(Record.Deceased);
        auto* Progress = UChronosProgressState::Get(Body);
        bool bInUse = false;
        if (Progress)
            for (const auto& Pair : Progress->Data.Urns)
                bInUse |= Pair.Key != Record.Id && Pair.Value.Deceased == Record.Deceased;
        if (bInUse || Record.DisplayName.IsEmpty())
        {
            Record.Deceased = FDataTableRowHandle();
            Record.DisplayName.Reset();
        }
        else Record.Origin = EChronosUrnOrigin::IdentifiedDeceased;
    }
    if (Record.Origin == EChronosUrnOrigin::Unverified) return false;
    PendingCremations.Add(Lower, MoveTemp(Record));
    return true;
}

void UChronosUrns::CompleteCremation(AActor* Actor, AActor* LowerDoor)
{
    auto* Urn = Cast<AChronosUrn>(Actor);
    auto* Progress = Urn ? UChronosProgressState::Get(Urn) : nullptr;
    FChronosUrnRecord Record;
    if (!Progress || !PendingCremations.RemoveAndCopyValue(LowerDoor, Record)) return;
    const int32 Legacy = Urn->Record() ? Urn->Record()->LegacyNumber : Progress->Data.NextUrnNumber++;
    Progress->Data.Urns.Remove(Urn->PersistentId);
    Urn->PersistentId = Record.Id;
    Urn->Origin = Record.Origin;
    Urn->SourceMission = Record.Mission;
    Urn->DeceasedRecord = Record.Deceased;
    Record.LegacyNumber = Legacy;
    Record.Map = Map(Urn->GetWorld());
    Record.ActorName = Urn->GetName();
    Record.Transform = Urn->GetActorTransform();
    Progress->Data.Urns.Add(Record.Id, Record);
    if (auto* Body = Progress->Data.Bodies.Find(Record.Id)) Body->bCremated = true;
    Urn->RefreshLabel();
    if (Record.Origin == EChronosUrnOrigin::IdentifiedDeceased)
        for (TActorIterator<AActor> It(Urn->GetWorld()); It; ++It)
        {
            if (It->GetClass()->GetName() != TEXT("BP_VisitanteEspecial_C") || Number(*It, TEXT("estado")) != 0) continue;
            const FString Key = Map(Urn->GetWorld()) + TEXT("|") + It->GetName();
            const auto Existing = Progress->Data.Requests.FindRef(Key);
            if (Existing.IsValid() && Existing != Record.Id) continue;
            Progress->Data.Requests.Add(Key, Record.Id);
            if (AssignRequest(*It))
            {
                SetNumber(*It, TEXT("estado"), 1);
                It->SetActorHiddenInGame(false);
                It->SetActorEnableCollision(true);
                UE_LOG(LogTemp, Display, TEXT("ChronosMission: original family %s arrived for cremated urn %s."), *It->GetName(), *Record.Id.ToString());
            }
            break;
        }
}

void UChronosUrns::TrackCarried(AActor* Actor, AActor* Player)
{
    auto* Urn = Cast<AChronosUrn>(Actor);
    if (Urn && Urn->Record() && IsValid(Player))
    {
        Urn->Record()->State = EChronosUrnState::Carried;
        Urn->Record()->Shelf.Reset();
        Urn->Record()->Slot.Reset();
    }
}

void UChronosUrns::TrackStored(AActor* Actor, AActor* Shelf)
{
    auto* Urn = Cast<AChronosUrn>(Actor);
    if (Urn && Urn->Record() && IsValid(Shelf))
    {
        auto& Record = *Urn->Record();
        Record.State = EChronosUrnState::Stored;
        Record.Shelf = Shelf->GetName();
        Record.Transform = Urn->GetActorTransform();
        if (const auto* Parent = Urn->GetRootComponent()->GetAttachParent()) Record.Slot = Parent->GetName();
    }
}

FString UChronosUrns::RequestedName(AActor* Visitor)
{
    auto* Urn = Cast<AChronosUrn>(Object(Visitor, TEXT("anforaPedida")));
    const auto* Record = Urn ? Urn->Record() : nullptr;
    const FString Name = Record && Record->Origin == EChronosUrnOrigin::IdentifiedDeceased ? RowName(Record->Deceased) : FString();
    return Name;
}

bool UChronosUrns::AssignRequest(AActor* Visitor)
{
    auto* Progress = UChronosProgressState::Get(Visitor);
    if (!Progress || !IsValid(Visitor)) return false;
    const FString RequestKey = Map(Visitor->GetWorld()) + TEXT("|") + Visitor->GetName();
    const auto Previous = Progress->Data.Requests.FindRef(RequestKey);
    for (TActorIterator<AChronosUrn> It(Visitor->GetWorld()); It; ++It)
    {
        const auto* Record = It->Record();
        if (!Record || Record->Origin != EChronosUrnOrigin::IdentifiedDeceased
            || (Record->State != EChronosUrnState::Stored
                && !(Record->bCremated && Record->State == EChronosUrnState::Available && Previous == Record->Id)
                && !(Record->State == EChronosUrnState::Carried && Previous == Record->Id))
            || RowName(Record->Deceased).IsEmpty() || (Previous.IsValid() && Previous != Record->Id)) continue;
        const bool bLegacyRequest=Number(Visitor,TEXT("estado"))>0
            && Number(Visitor,TEXT("numeroPedido"))==Record->LegacyNumber;
        if(!Record->bCremated && Previous!=Record->Id && !bLegacyRequest)continue;
        bool bReserved = false;
        for (const auto& Request : Progress->Data.Requests)
            bReserved |= Request.Key != RequestKey && Request.Value == Record->Id;
        if (bReserved) continue;
        Progress->Data.Requests.Add(RequestKey, Record->Id);
        SetObject(Visitor, TEXT("anforaPedida"), *It);
        SetNumber(Visitor, TEXT("numeroPedido"), Record->LegacyNumber);
        return true;
    }
    return false;
}

bool UChronosUrns::CanHideFamilyAtNight(AActor* Visitor)
{
    const auto* Progress = UChronosProgressState::Get(Visitor);
    if (!Progress || !IsValid(Visitor)) return true;
    const auto Id = Progress->Data.Requests.FindRef(Map(Visitor->GetWorld()) + TEXT("|") + Visitor->GetName());
    const auto* Record = Progress->Data.Urns.Find(Id);
    return !Record || !Record->bCremated || Record->Origin != EChronosUrnOrigin::IdentifiedDeceased
        || Record->State == EChronosUrnState::Delivered || Record->State == EChronosUrnState::Disposed;
}

bool UChronosUrns::HasSavedUrns(AActor* Context)
{
    const auto* Progress=UChronosProgressState::Get(Context);
    return Progress && Progress->bRestoredFromSave;
}

FString UChronosUrns::RequestText(AActor* Visitor, bool bWrongUrn)
{
    const FString Name = RequestedName(Visitor);
    if (Name.IsEmpty()) return TEXT("No encuentro un registro válido de estas cenizas.");
    return bWrongUrn ? FString::Printf(TEXT("Esa no corresponde. Vengo a retirar las cenizas de %s."), *Name)
        : FString::Printf(TEXT("Disculpe, vengo a retirar las cenizas de %s. ¿Podría ayudarme?"), *Name);
}

bool UChronosUrns::Deliver(AActor* Visitor, AActor* Player)
{
    auto* Requested = Cast<AChronosUrn>(Object(Visitor, TEXT("anforaPedida")));
    auto* Held = Cast<AChronosUrn>(Object(Player, TEXT("anforaCargada")));
    auto* Progress = UChronosProgressState::Get(Player);
    auto* Record = Held ? Held->Record() : nullptr;
    if (!IsValid(Visitor) || !Progress || !Record || Held != Requested || RequestedName(Visitor).IsEmpty()
        || Record->Origin != EChronosUrnOrigin::IdentifiedDeceased || Record->State != EChronosUrnState::Carried
        || Held->GetAttachParentActor() != Player
        || Progress->Data.Requests.FindRef(Map(Visitor->GetWorld()) + TEXT("|") + Visitor->GetName()) != Record->Id
        || Record->LegacyNumber != Number(Visitor, TEXT("numeroPedido"))) return false;
    Record->State = EChronosUrnState::Delivered;
    if (!Progress->PayDelivery(Record->Id))
    {
        Record->State = EChronosUrnState::Carried;
        return false;
    }
    SetObject(Player, TEXT("anforaCargada"), nullptr);
    Held->Destroy();
    return true;
}

void UChronosUrns::Capture(UWorld* World)
{
    for(TActorIterator<AChronosBody> It(World);It;++It)It->Identify();
    for (TActorIterator<AChronosUrn> It(World); It; ++It)
        if (auto* Record = It->Record()) Record->Transform = It->GetActorTransform();
}

void UChronosUrns::Restore(UWorld* World)
{
    auto* Progress = UChronosProgressState::Get(World);
    auto* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
    auto* Class = LoadClass<AChronosUrn>(nullptr, TEXT("/Game/Interactivo/bp_anfora/bp_anfora.bp_anfora_C"));
    if (!Progress || !Player || !Class) return;
    TArray<FChronosUrnRecord> Records;
    Progress->Data.Urns.GenerateValueArray(Records);
    for (const auto& Record : Records)
    {
        if (Record.Map != Map(World) || Record.State == EChronosUrnState::Disposed || Record.State == EChronosUrnState::Delivered) continue;
        AChronosUrn* Urn = nullptr;
        for (TActorIterator<AChronosUrn> It(World); It; ++It)
            if (It->PersistentId == Record.Id) { Urn = *It; break; }
        if (!Urn && Record.bPlacedInLevel) continue;
        if (!Urn)
        {
            Urn = World->SpawnActorDeferred<AChronosUrn>(Class, Record.Transform);
            if (!Urn) continue;
            Urn->PersistentId = Record.Id;
            Urn->DeceasedRecord = Record.Deceased;
            Urn->Origin = Record.Origin;
            Urn->FinishSpawning(Record.Transform);
        }
        if (Record.State == EChronosUrnState::Carried && !Object(Player, TEXT("anforaCargada")))
        {
            Urn->SetActorEnableCollision(false);
            Urn->AttachToComponent(Player->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("socket_anfora"));
            SetObject(Player, TEXT("anforaCargada"), Urn);
        }
        if (Record.State == EChronosUrnState::Stored && !Record.Shelf.IsEmpty() && !Record.Slot.IsEmpty())
        {
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                if (It->GetName() != Record.Shelf) continue;
                TInlineComponentArray<USceneComponent*> Points(*It);
                for (auto* Point : Points)
                {
                    if (Point->GetName() != Record.Slot) continue;
                    TArray<AActor*> Occupants;
                    It->GetAttachedActors(Occupants);
                    const bool Taken = Occupants.ContainsByPredicate([Point, Urn](const AActor* Occupant)
                    { return Occupant != Urn && Occupant->GetRootComponent() && Occupant->GetRootComponent()->GetAttachParent() == Point; });
                    if (Taken) break;
                    Urn->AttachToComponent(Point, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                    AddToArray(*It, TEXT("anforasColocadas"), Urn);
                    AddToArray(*It, TEXT("puntosOcupados"), Point);
                    break;
                }
                break;
            }
        }
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetClass()->GetName() != TEXT("BP_VisitanteEspecial_C")) continue;
        const int32 State = Number(*It, TEXT("estado"));
        if (State == 1 || State == 2) AssignRequest(*It);
    }
}
