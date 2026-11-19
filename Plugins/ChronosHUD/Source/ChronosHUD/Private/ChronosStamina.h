#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ChronosStamina.generated.h"

class ACharacter;

UCLASS()
class CHRONOSHUD_API UChronosStamina : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Chronos|Stamina")
    static void RestoreStamina(ACharacter* John);
    UFUNCTION(BlueprintPure, Category="Chronos|Stamina")
    static double ApplyConsumableSpeed(ACharacter* John, double BaseFactor);
    UFUNCTION(BlueprintCallable, Category="YukoDev|Stamina")
    static void UpdateStamina(ACharacter* John, double DeltaSeconds);

    UFUNCTION(BlueprintCallable, Category="YukoDev|Stamina")
    static void HoldSprintInput(ACharacter* John);

    UFUNCTION(BlueprintPure, Category="YukoDev|Stamina")
    static bool CanStaminaJump(const ACharacter* John);

    UFUNCTION(BlueprintCallable, Category="YukoDev|Stamina")
    static void ChargeSuccessfulJump(ACharacter* John);

    UFUNCTION(BlueprintCallable, Category="YukoDev|Stamina")
    static bool BeginSprint(ACharacter* John);

    UFUNCTION(BlueprintCallable, Category="YukoDev|Stamina")
    static void ReleaseSprint(ACharacter* John);

    UFUNCTION(BlueprintCallable, Category="YukoDev|Stamina")
    static void TryStaminaAttack(ACharacter* John, bool bKick);

    UFUNCTION(BlueprintPure, Category="YukoDev|Stamina")
    static double GetStaminaPercent(const ACharacter* John);

    UFUNCTION(BlueprintPure, Category="YukoDev|Stamina")
    static bool CanSprint(const ACharacter* John);

    UFUNCTION(BlueprintPure, Category="YukoDev|Stamina")
    static bool CanPerformStaminaAction(const ACharacter* John, double Cost);

    UFUNCTION(BlueprintCallable, Category="YukoDev|Stamina")
    static void ConsumeStamina(ACharacter* John, double Amount);

    UFUNCTION(BlueprintCallable, Category="YukoDev|Stamina")
    static void RegenerateStamina(ACharacter* John, double Amount);

    UFUNCTION(BlueprintCallable, Category="YukoDev|Stamina")
    static void StopSprinting(ACharacter* John);

    UFUNCTION(BlueprintCallable, Category="YukoDev|Stamina")
    static void HandleStaminaExhaustion(ACharacter* John);

    static bool HasStamina(const ACharacter* John);
};
