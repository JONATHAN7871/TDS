// TDSCharacter.h - Обновленная версия

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TDSCharacterMovementComponent.h"
#include "TDSCharacter.generated.h"

class UTDSCharacterMovementComponent;

UCLASS()
class TOPDOWNSHOOTER_API ATDSCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ATDSCharacter(const FObjectInitializer& ObjectInitializer);

#pragma region TDS Character Movement States
public:
    /** Основные состояния движения (реплицируются) */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_IsWalkingState, Category="TDS Character")
    uint8 bIsWalkingState : 1;
    
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_IsSprintingState, Category="TDS Character")
    uint8 bIsSprintingState : 1;
    
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_IsStrafingState, Category="TDS Character")
    uint8 bIsStrafingState : 1;
    
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_IsAimingState, Category="TDS Character")
    uint8 bIsAimingState : 1;

    /** RepNotify функции для состояний */
    UFUNCTION()
    virtual void OnRep_IsWalkingState();
    
    UFUNCTION()
    virtual void OnRep_IsSprintingState();
    
    UFUNCTION()
    virtual void OnRep_IsStrafingState();
    
    UFUNCTION()
    virtual void OnRep_IsAimingState();

    /** Методы управления состояниями */
    UFUNCTION(BlueprintCallable, Category="TDS Character", meta=(HidePin="bClientSimulation"))
    void SetWalking(bool NewWalk, bool bClientSimulation = false);

    UFUNCTION(BlueprintCallable, Category="TDS Character", meta=(HidePin="bClientSimulation"))
    void SetSprinting(bool NewSprint, bool bClientSimulation = false);

    UFUNCTION(BlueprintCallable, Category="TDS Character", meta=(HidePin="bClientSimulation"))
    void SetStrafing(bool NewStrafe, bool bClientSimulation = false);

    UFUNCTION(BlueprintCallable, Category="TDS Character", meta=(HidePin="bClientSimulation"))
    void SetAiming(bool NewAim, bool bClientSimulation = false);
#pragma endregion

#pragma region Custom Movement Controls
public:
    /** Методы для кастомных движений */
    UFUNCTION(BlueprintCallable, Category="TDS Movement")
    void StartWallRun();

    UFUNCTION(BlueprintCallable, Category="TDS Movement")
    void StopWallRun();

    UFUNCTION(BlueprintCallable, Category="TDS Movement")
    void StartSlide();

    UFUNCTION(BlueprintCallable, Category="TDS Movement")
    void StopSlide();

    UFUNCTION(BlueprintCallable, Category="TDS Movement")
    void StartProne();

    UFUNCTION(BlueprintCallable, Category="TDS Movement")
    void StopProne();

    /** Проверки состояний кастомных движений */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="TDS Movement")
    bool IsWallRunning() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="TDS Movement")
    bool IsSliding() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="TDS Movement")
    bool IsInProne() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="TDS Movement")
    ETDSCustomMovementMode GetCurrentCustomMovementMode() const;
#pragma endregion

#pragma region Engine Overrides
public:
    virtual void Tick(float DeltaTime) override;

    /** Доступ к кастомному компоненту движения */
    UFUNCTION(BlueprintCallable, Category="TDS Movement")
    UTDSCharacterMovementComponent* GetTDSMovementComponent() const;
    
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
#pragma endregion

#pragma region Input Handling
protected:
    /** Input Actions для различных типов движения */
    
    // Основные движения
    UFUNCTION(BlueprintCallable, Category="TDS Input")
    void OnWalkPressed();
    
    UFUNCTION(BlueprintCallable, Category="TDS Input")
    void OnWalkReleased();
    
    UFUNCTION(BlueprintCallable, Category="TDS Input")
    void OnSprintPressed();
    
    UFUNCTION(BlueprintCallable, Category="TDS Input")
    void OnSprintReleased();
    
    UFUNCTION(BlueprintCallable, Category="TDS Input")
    void OnStrafePressed();
    
    UFUNCTION(BlueprintCallable, Category="TDS Input")
    void OnStrafeReleased();
    
    UFUNCTION(BlueprintCallable, Category="TDS Input")
    void OnAimPressed();
    
    UFUNCTION(BlueprintCallable, Category="TDS Input")
    void OnAimReleased();

    // Кастомные движения
    UFUNCTION(BlueprintCallable, Category="TDS Input")
    void OnWallRunPressed();
    
    UFUNCTION(BlueprintCallable, Category="TDS Input")
    void OnWallRunReleased();
    
    UFUNCTION(BlueprintCallable, Category="TDS Input")
    void OnSlidePressed();
    
    UFUNCTION(BlueprintCallable, Category="TDS Input")
    void OnSlideReleased();
    
    UFUNCTION(BlueprintCallable, Category="TDS Input")
    void OnPronePressed();
    
    UFUNCTION(BlueprintCallable, Category="TDS Input")
    void OnProneReleased();
#pragma endregion

#pragma region Blueprint Events
protected:
    /** События для Blueprint */
    UFUNCTION(BlueprintImplementableEvent, Category="TDS Character")
    void OnMovementStateChanged(bool bWalk, bool bSprint, bool bStrafe, bool bAim);

    UFUNCTION(BlueprintImplementableEvent, Category="TDS Character")
    void OnCustomMovementStarted(ETDSCustomMovementMode MovementMode);

    UFUNCTION(BlueprintImplementableEvent, Category="TDS Character")
    void OnCustomMovementEnded(ETDSCustomMovementMode MovementMode);
#pragma endregion
};
