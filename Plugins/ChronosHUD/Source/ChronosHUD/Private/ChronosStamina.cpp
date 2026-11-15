#include "ChronosStamina.h"

#include "EnhancedPlayerInput.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "UObject/UnrealType.h"

namespace
{
    // Retain the speed from the original imc_sprint release/cancel node.
    constexpr float WalkSpeed = 600.f;
    constexpr float MovementTolerance = 18.f;

    const FNumericProperty* NumberProperty(const UObject* Object, FName Name)
    {
        return IsValid(Object) ? FindFProperty<FNumericProperty>(Object->GetClass(), Name) : nullptr;
    }

    double Number(const UObject* Object, FName Name)
    {
        const auto* Property = NumberProperty(Object, Name);
        return Property ? Property->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Object)) : 0.;
    }

    void SetNumber(UObject* Object, FName Name, double Value)
    {
        if (const auto* Property = NumberProperty(Object, Name))
            Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Object), Value);
    }

    bool Flag(const UObject* Object, FName Name)
    {
        const auto* Property = IsValid(Object) ? FindFProperty<FBoolProperty>(Object->GetClass(), Name) : nullptr;
        return Property && Property->GetPropertyValue_InContainer(Object);
    }

    void SetFlag(UObject* Object, FName Name, bool bValue)
    {
        if (const auto* Property = IsValid(Object) ? FindFProperty<FBoolProperty>(Object->GetClass(), Name) : nullptr)
            Property->SetPropertyValue_InContainer(Object, bValue);
    }

    UObject* Member(const UObject* Object, FName Name)
    {
        const auto* Property = IsValid(Object) ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name) : nullptr;
        return Property ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
    }

    double Maximum(const ACharacter* John) { return FMath::Max(0., Number(John, TEXT("MaxStamina"))); }
    double Current(const ACharacter* John) { return FMath::Clamp(Number(John, TEXT("CurrentStamina")), 0., Maximum(John)); }
    double Time(const ACharacter* John) { return John->GetWorld()->GetTimeSeconds(); }

    bool IsSprintActionActive(const ACharacter* John)
    {
        const auto* Player = Cast<APlayerController>(John->GetController());
        const auto* Input = Player ? Cast<UEnhancedPlayerInput>(Player->PlayerInput) : nullptr;
        if (!Input) return false;
        static TWeakObjectPtr<UInputAction> Sprint;
        if (!Sprint.IsValid())
            Sprint = LoadObject<UInputAction>(nullptr, TEXT("/Game/Characters/input/movement/imc_sprint.imc_sprint"));
        return Sprint.IsValid() && Input->GetActionValue(Sprint.Get()).IsNonZero() &&
            (!FSlateApplication::IsInitialized() || FSlateApplication::Get().IsActive());
    }

    bool HasMovementControl(const ACharacter* John)
    {
        return John->GetController() && !John->GetController()->IsMoveInputIgnored() &&
            !Flag(John, TEXT("enDespertar")) && !IsValid(Member(John, TEXT("camaraMuerte")));
    }
}

bool UChronosStamina::HasStamina(const ACharacter* John)
{
    return NumberProperty(John, TEXT("CurrentStamina")) != nullptr;
}

double UChronosStamina::GetStaminaPercent(const ACharacter* John)
{
    const double Max = Maximum(John);
    return Max > 0. ? Current(John) / Max * 100. : 0.;
}

bool UChronosStamina::CanSprint(const ACharacter* John)
{
    return HasStamina(John) && HasMovementControl(John) && Current(John) > 0. &&
        !Flag(John, TEXT("bStaminaExhausted")) && !Flag(John, TEXT("bSprintNeedsRelease")) &&
        !IsValid(Member(John, TEXT("CadaverCargado")));
}

// HECHO POR YukoDev (STAMINA)
bool UChronosStamina::CanStaminaJump(const ACharacter* John)
{
    return HasStamina(John) && HasMovementControl(John) && John->CanJump() &&
        CanPerformStaminaAction(John, FMath::Max(0., Number(John, TEXT("JumpStaminaCost"))));
}

void UChronosStamina::ChargeSuccessfulJump(ACharacter* John)
{
    ConsumeStamina(John, FMath::Max(0., Number(John, TEXT("JumpStaminaCost"))));
}

void UChronosStamina::HoldSprintInput(ACharacter* John)
{
    SetFlag(John, TEXT("bSprintInputHeld"), true);
}

bool UChronosStamina::BeginSprint(ACharacter* John)
{
    if (!CanSprint(John))
    {
        if (Flag(John, TEXT("bStaminaExhausted"))) SetFlag(John, TEXT("bSprintNeedsRelease"), true);
        return false;
    }
    SetFlag(John, TEXT("bIsSprinting"), true);
    return true;
}

void UChronosStamina::StopSprinting(ACharacter* John)
{
    if (!IsValid(John) || !Flag(John, TEXT("bIsSprinting"))) return;
    SetFlag(John, TEXT("bIsSprinting"), false);
    John->GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void UChronosStamina::ReleaseSprint(ACharacter* John)
{
    if (Flag(John, TEXT("bSprintInputHeld")))
        SetNumber(John, TEXT("LastStaminaUseTime"), Time(John));
    SetFlag(John, TEXT("bSprintInputHeld"), false);
    StopSprinting(John);
    SetFlag(John, TEXT("bSprintNeedsRelease"), false);
}

void UChronosStamina::HandleStaminaExhaustion(ACharacter* John)
{
    SetFlag(John, TEXT("bStaminaExhausted"), true);
    if (Flag(John, TEXT("bIsSprinting"))) SetFlag(John, TEXT("bSprintNeedsRelease"), true);
    StopSprinting(John);
}

bool UChronosStamina::CanPerformStaminaAction(const ACharacter* John, double Cost)
{
    return HasStamina(John) && Cost >= 0. && Current(John) >= Cost;
}

void UChronosStamina::ConsumeStamina(ACharacter* John, double Amount)
{
    if (!HasStamina(John) || Amount <= 0.) return;
    const double Remaining = FMath::Clamp(Current(John) - Amount, 0., Maximum(John));
    SetNumber(John, TEXT("CurrentStamina"), Remaining > UE_SMALL_NUMBER ? Remaining : 0.);
    SetNumber(John, TEXT("LastStaminaUseTime"), Time(John));
    if (Remaining <= UE_SMALL_NUMBER) HandleStaminaExhaustion(John);
}

void UChronosStamina::RegenerateStamina(ACharacter* John, double Amount)
{
    if (!HasStamina(John) || Amount <= 0. || Flag(John, TEXT("bSprintInputHeld"))) return;
    const double NewValue = FMath::Clamp(Current(John) + Amount, 0., Maximum(John));
    SetNumber(John, TEXT("CurrentStamina"), NewValue);
    const double Threshold = FMath::Clamp(Number(John, TEXT("StaminaSprintThreshold")), 0., Maximum(John));
    if (NewValue > 0. && NewValue >= Threshold) SetFlag(John, TEXT("bStaminaExhausted"), false);
}

void UChronosStamina::UpdateStamina(ACharacter* John, double DeltaSeconds)
{
    if (!HasStamina(John) || DeltaSeconds <= 0.) return;

    const double Raw = Number(John, TEXT("CurrentStamina"));
    const double Stamina = Current(John);
    if (Raw != Stamina) SetNumber(John, TEXT("CurrentStamina"), Stamina);

    if (Flag(John, TEXT("bSprintInputHeld")) && !IsSprintActionActive(John)) ReleaseSprint(John);
    if (!HasMovementControl(John) || IsValid(Member(John, TEXT("CadaverCargado")))) StopSprinting(John);
    const UCharacterMovementComponent* Movement = John->GetCharacterMovement();
    const double Speed = John->GetVelocity().Size2D();
    const bool bAttacking = Time(John) < Number(Member(John, TEXT("MeleeCombat")), TEXT("NextAttackTime"));
    const bool bRunning = Flag(John, TEXT("bIsSprinting")) && Movement->IsMovingOnGround() && !Movement->IsCrouching()
        && Speed > WalkSpeed + MovementTolerance && Movement->GetCurrentAcceleration().SizeSquared2D() > 1.;
    if (bRunning && !bAttacking)
    {
        ConsumeStamina(John, FMath::Max(0., Number(John, TEXT("StaminaDrainRate"))) * DeltaSeconds);
        return;
    }

    if (Flag(John, TEXT("bSprintInputHeld")) || bAttacking || Stamina >= Maximum(John)) return;
    const double TimeSinceUse = Time(John) - Number(John, TEXT("LastStaminaUseTime"));
    const double ReadyTime = FMath::Max(0., Number(John, TEXT("StaminaRegenDelay")));
    const double RecoverSeconds = FMath::Clamp(TimeSinceUse - ReadyTime, 0., DeltaSeconds);
    const FName Rate = Speed > MovementTolerance ? TEXT("StaminaRegenRateWalking") : TEXT("StaminaRegenRateIdle");
    RegenerateStamina(John, FMath::Max(0., Number(John, Rate)) * RecoverSeconds);
}

void UChronosStamina::TryStaminaAttack(ACharacter* John, bool bKick)
{
    const FName CostName = bKick ? TEXT("KickStaminaCost") : TEXT("PunchStaminaCost");
    const double Cost = FMath::Max(0., Number(John, CostName));
    if (!CanPerformStaminaAction(John, Cost)) return;

    UObject* Combat = Member(John, TEXT("MeleeCombat"));
    if (!IsValid(Combat)) return;
    UFunction* Attack = Combat->FindFunction(bKick ? TEXT("TryKick") : TEXT("TryAttack"));
    if (!Attack || Attack->ParmsSize != 0) return;

    // The existing component advances this only after Montage_Play succeeds.
    const double PreviousAttackTime = Number(Combat, TEXT("NextAttackTime"));
    Combat->ProcessEvent(Attack, nullptr);
    if (Number(Combat, TEXT("NextAttackTime")) > PreviousAttackTime) ConsumeStamina(John, Cost);
}
