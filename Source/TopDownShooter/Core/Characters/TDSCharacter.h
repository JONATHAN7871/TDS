// Copyright 2025, CRAFTCODE, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TDSCharacter.generated.h"

class UTDSCharacterMovementComponent;

UCLASS()
class TOPDOWNSHOOTER_API ATDSCharacter : public ACharacter
{
    GENERATED_BODY()

#pragma region TDS Character Movement
public:
    /** Состояние бега, ходьбы, стрейфа, прицеливания */
    UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsWalkingState, Category=Character)
    uint8 bIsWalkingState   : 1;
    UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsSprintingState, Category=Character)
    uint8 bIsSprintingState : 1;
    UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsStrafingState, Category=Character)
    uint8 bIsStrafingState : 1;
    UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsAimingState, Category=Character)
    uint8 bIsAimingState    : 1;

    UFUNCTION()
    virtual void OnRep_IsWalkingState();
    UFUNCTION()
    virtual void OnRep_IsSprintingState();
    UFUNCTION()
    virtual void OnRep_IsStrafingState();
    UFUNCTION()
    virtual void OnRep_IsAimingState();

    UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
    void SetWalking(bool NewWalk, bool bClientSimulation = false);

    UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
    void SetSprinting(bool NewSprint, bool bClientSimulation = false);

    UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
    void SetStrafing(bool NewStrafe, bool bClientSimulation = false);

    UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
    void SetAiming(bool NewAim, bool bClientSimulation = false);
#pragma endregion

    
public:
    ATDSCharacter(const FObjectInitializer& ObjectInitializer);
    
    virtual void Tick(float DeltaTime) override;

    /** Access to custom movement component */
    UFUNCTION(BlueprintCallable, Category = "Movement")
    UTDSCharacterMovementComponent* GetTDSMovementComponent() const;
    
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;
    
};
