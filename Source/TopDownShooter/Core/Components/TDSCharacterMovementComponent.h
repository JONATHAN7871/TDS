// TDSCharacterMovementComponent.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CharacterMovementComponentAsync.h"
#include "TDSCharacterMovementComponent.generated.h"

class ATDSCharacter;

UCLASS

(BlueprintType, Blueprintable)
class TOPDOWNSHOOTER_API UTDSCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

    UTDSCharacterMovementComponent();
    
    friend class FSavedMove_TDS;
    friend class FNetworkPredictionData_Client_TDS;

#pragma region TDS Defaults
private:
    /** Ротация в воздухе */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TDS Movement|Rotation", Meta=(AllowPrivateAccess="true"))
    FRotator FallingRotationRate = FRotator(0.f, 200.f, 0.f);

    /** Ротация на земле */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TDS Movement|Rotation", Meta=(AllowPrivateAccess="true"))
    FRotator GroundRotationRate  = FRotator(0.f, -1.f, 0.f);
#pragma endregion

#pragma region TDS Compressed Flags
public:
    /** Состояние бега, ходьбы, стрейфа, прицеливания */
    uint8 WalkState   : 1;
    uint8 SprintState : 1;
    uint8 StrafeState : 1;
    uint8 AimState    : 1;
#pragma endregion

#pragma region TDS State Accessors
public:
    UFUNCTION(BlueprintCallable, Category="States") bool IsWalkingState()  const { return WalkState; }
    UFUNCTION(BlueprintCallable, Category="States") bool IsSprintingState()const { return SprintState; }
    UFUNCTION(BlueprintCallable, Category="States") bool IsStrafingState() const { return StrafeState; }
    UFUNCTION(BlueprintCallable, Category="States") bool IsAimingState()   const { return AimState; }
#pragma endregion

#pragma region TDS State Mutators
public:
    void SetWalking(bool NewWalk, bool bClientSimulation = false);
    void SetSprinting(bool NewSprint, bool bClientSimulation = false);
    void SetStrafing(bool NewStrafe, bool bClientSimulation = false);
    void SetAiming(bool NewAim, bool bClientSimulation = false);
#pragma endregion

#pragma region TDS Overrides
protected:
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
#pragma endregion
    
#pragma region TDS Blueprint Events
public:
    /** Для дополнительной логики в Blueprint */
    UFUNCTION(BlueprintNativeEvent, Category="Movement Update")
    void OnMovementCustomUpdated(float DeltaSeconds);
    virtual void OnMovementCustomUpdated_Implementation(float DeltaSeconds);
#pragma endregion

#pragma region Overrides
public:
    virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
protected:
    virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds);
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;
    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
    
#pragma endregion
};

// Сохранённый ход
class FSavedMove_TDS : public FSavedMove_Character
{
public:
    typedef FSavedMove_Character Super;

    FSavedMove_TDS()
        : SavedWalkState(0)
        , SavedSprintState(0)
        , SavedStrafeState(0)
        , SavedAimState(0)
    {}
    
    virtual void Clear() override;
    virtual uint8 GetCompressedFlags() const override;
    virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;
    virtual void SetMoveFor(ACharacter* Character, float DeltaTime, const FVector& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
    virtual void PrepMoveFor(ACharacter* Character) override;
private:
    uint8 SavedWalkState   : 1;
    uint8 SavedSprintState : 1;
    uint8 SavedStrafeState : 1;
    uint8 SavedAimState    : 1;
};

// Данные сетевого предсказания клиента
class FNetworkPredictionData_Client_TDS : public FNetworkPredictionData_Client_Character
{
public:
    typedef FNetworkPredictionData_Client_Character Super;

    FNetworkPredictionData_Client_TDS(const UCharacterMovementComponent& InMovementComponent);
    virtual FSavedMovePtr AllocateNewMove() override;
};
