#include "ChronosProgress.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

UChronosProgressState* UChronosProgressState::Get(const UObject* Context)
{
    auto* Instance = UGameplayStatics::GetGameInstance(Context);
    return Instance ? Instance->GetSubsystem<UChronosProgressState>() : nullptr;
}

int32 UChronosProgressState::Count(FName Item) const
{
    return FMath::Max(0, Data.Items.FindRef(Item));
}

bool UChronosProgressState::Add(FName Item, int32 Quantity)
{
    if (Item.IsNone() || Quantity <= 0 || Count(Item) > MAX_int32 - Quantity) return false;
    Data.Items.Add(Item, Count(Item) + Quantity);
    return true;
}

bool UChronosProgressState::Remove(FName Item, int32 Quantity)
{
    if (Item.IsNone() || Quantity <= 0 || Count(Item) < Quantity) return false;
    const int32 Remaining = Count(Item) - Quantity;
    if (Remaining) Data.Items.Add(Item, Remaining);
    else Data.Items.Remove(Item);
    return true;
}

bool UChronosProgressState::PayDelivery(FGuid Urn)
{
    const auto* Record = Data.Urns.Find(Urn);
    if (!Record || Record->Origin != EChronosUrnOrigin::IdentifiedDeceased || Record->Deceased.IsNull()
        || Record->State != EChronosUrnState::Delivered || Data.PaidDeliveries.Contains(Urn)
        || Data.Dollars > MAX_int64 - 5) return false;
    Data.PaidDeliveries.Add(Urn);
    Data.Dollars += 5;
    return true;
}
