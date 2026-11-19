#pragma once

#include "Animation/AnimInstance.h"
#include "ChronosBeerAnimation.generated.h"

UCLASS()
class CHRONOSHUD_API UChronosBeerAnimation : public UAnimInstance
{
    GENERATED_BODY()
public:
    void NativeUpdateAnimation(float DeltaSeconds) override;
    UPROPERTY(BlueprintReadOnly, Category="Cerveza") float DrinkArmAlpha = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="Cerveza") FVector DrinkHandTarget = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="Cerveza") FVector DrinkElbowTarget = FVector::ZeroVector;
};
