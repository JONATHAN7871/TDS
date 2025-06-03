// TDSCharacterMovementComponent.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CharacterMovementComponentAsync.h"
#include "Curves/CurveFloat.h"
#include "Net/UnrealNetwork.h"
#include "TDSCharacterMovementComponent.generated.h"

class ATDSCharacter;
class UEnhancedInputComponent;

/** Гейт персонажа: ходьба, бег или спринт */
UENUM(BlueprintType)
enum class EGait : uint8
{
    Walk    UMETA(DisplayName = "Walk"),
    Run     UMETA(DisplayName = "Run"),
    Sprint  UMETA(DisplayName = "Sprint")
};

/** Поведение аналогового стика для выбора скорости/гейта */
UENUM(BlueprintType)
enum class EAnalogStickBehavior : uint8
{
    FixedSingleGait      UMETA(DisplayName = "Fixed Speed - Single Gait"),
    FixedWalkRun         UMETA(DisplayName = "Fixed Speed - Walk / Run"),
    VariableSingleGait   UMETA(DisplayName = "Variable Speed - Single Gait"),
    VariableWalkRun      UMETA(DisplayName = "Variable Speed - Walk / Run")
};

/** Кастомные режимы движения для TDS */
UENUM(BlueprintType)
enum class ETDSCustomMovementMode : uint8
{
    CMOVE_None          UMETA(DisplayName = "None"),
    CMOVE_WallRunning   UMETA(DisplayName = "Wall Running"),
    CMOVE_Sliding       UMETA(DisplayName = "Sliding"),
    CMOVE_Prone         UMETA(DisplayName = "Prone"),
    CMOVE_MAX           UMETA(Hidden),
};

/** Сторона стены для Wall Running */
UENUM(BlueprintType)
enum class ETDSWallRunSide : uint8
{
    Left    UMETA(DisplayName = "Left"),
    Right   UMETA(DisplayName = "Right"),
};

UCLASS(BlueprintType, Blueprintable)
class TOPDOWNSHOOTER_API UTDSCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UTDSCharacterMovementComponent();
    
    friend class FSavedMove_TDS;
    friend class FNetworkPredictionData_Client_TDS;

#pragma region Gait System Properties
private:
    /** Текущий гейт персонажа (реплицируется) */
    UPROPERTY(Replicated, BlueprintReadOnly, Category="TDS Gait", Meta=(AllowPrivateAccess="true"))
    EGait CurrentGait = EGait::Run;

    /** Кривая для коррекции скорости при движении вбок и назад */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="TDS Movement|Speeds", Meta=(AllowPrivateAccess="true"))
    TObjectPtr<UCurveFloat> StrafeSpeedMapCurve;

    /** Скорости для Walk (X=Forward, Y=Strafe, Z=Backwards) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TDS Movement|Speeds", Meta=(AllowPrivateAccess="true"))
    FVector WalkSpeeds = FVector(200.f, 180.f, 150.f);

    /** Скорости для Run (X=Forward, Y=Strafe, Z=Backwards) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TDS Movement|Speeds", Meta=(AllowPrivateAccess="true"))
    FVector RunSpeeds = FVector(500.f, 350.f, 300.f);

    /** Скорости для Sprint (X=Forward, Y=Strafe, Z=Backwards) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TDS Movement|Speeds", Meta=(AllowPrivateAccess="true"))
    FVector SprintSpeeds = FVector(700.f, 700.f, 700.f);

    /** Скорости для Crouch (X=Forward, Y=Strafe, Z=Backwards) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TDS Movement|Speeds", Meta=(AllowPrivateAccess="true"))
    FVector CrouchSpeeds = FVector(225.f, 200.f, 180.f);

    /** Режим поведения аналогового стика */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="TDS Movement|Input", Meta=(AllowPrivateAccess="true"))
    EAnalogStickBehavior MovementStickMode = EAnalogStickBehavior::FixedSingleGait;

    /** Порог для перехода между Walk/Run */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="TDS Movement|Input", Meta=(AllowPrivateAccess="true"))
    float AnalogWalkRunThreshold = 0.7f;

    /** Использовать ли систему гейтов */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="TDS Movement|Gait", Meta=(AllowPrivateAccess="true"))
    bool bUseGaitSystem = true;

    /** Кэшированные значения ввода */
    mutable FVector2D CachedMoveInput;
    mutable FVector2D CachedMoveWorldSpaceInput;
    mutable float LastInputUpdateTime = 0.0f;
#pragma endregion

#pragma region Custom Movement Properties
private:
    /** Настройки ротации */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TDS Movement|Rotation", Meta=(AllowPrivateAccess="true"))
    FRotator FallingRotationRate = FRotator(0.f, 200.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TDS Movement|Rotation", Meta=(AllowPrivateAccess="true"))
    FRotator GroundRotationRate = FRotator(0.f, -1.f, 0.f);

    /** Настройки Wall Running */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TDS Movement|Wall Running", Meta=(AllowPrivateAccess="true"))
    float WallRunSpeed = 625.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TDS Movement|Wall Running", Meta=(AllowPrivateAccess="true"))
    float LineTraceVerticalTolerance = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TDS Movement|Wall Running", Meta=(AllowPrivateAccess="true"))
    float WallRunGravityScale = 0.1f;

    /** Настройки Sliding */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TDS Movement|Sliding", Meta=(AllowPrivateAccess="true"))
    float SlideSpeed = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TDS Movement|Sliding", Meta=(AllowPrivateAccess="true"))
    float SlideDeceleration = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TDS Movement|Sliding", Meta=(AllowPrivateAccess="true"))
    float MinSlideSpeed = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TDS Movement|Sliding", Meta=(AllowPrivateAccess="true"))
    float SlideCapsuleHalfHeight = 40.0f;

    /** Настройки Prone */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TDS Movement|Prone", Meta=(AllowPrivateAccess="true"))
    float ProneSpeed = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TDS Movement|Prone", Meta=(AllowPrivateAccess="true"))
    float ProneCapsuleHalfHeight = 30.0f;

    /** Сохраненные размеры капсулы */
    float DefaultCapsuleHalfHeight = 0.0f;
    float DefaultCapsuleRadius = 0.0f;
#pragma endregion

#pragma region Movement States
public:
    /** Основные состояния движения (реплицируются) */
    UPROPERTY(Replicated, BlueprintReadOnly, Category="TDS States")
    uint8 WalkState : 1;
    
    UPROPERTY(Replicated, BlueprintReadOnly, Category="TDS States")
    uint8 SprintState : 1;
    
    UPROPERTY(Replicated, BlueprintReadOnly, Category="TDS States")
    uint8 StrafeState : 1;
    
    UPROPERTY(Replicated, BlueprintReadOnly, Category="TDS States")
    uint8 AimState : 1;

    /** Кастомные состояния движения */
    UPROPERTY(Replicated, BlueprintReadOnly, Category="TDS States")
    uint8 WallRunKeysDown : 1;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="TDS States")
    uint8 SlideKeysDown : 1;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="TDS States")
    uint8 ProneKeysDown : 1;

    /** Направление и сторона Wall Running */
    UPROPERTY(Replicated, BlueprintReadOnly, Category="TDS States")
    FVector WallRunDirection;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="TDS States")
    ETDSWallRunSide WallRunSide;

    /** Флаг для обновлений по сети */
    uint8 bNetworkUpdateReceived : 1;

private:
    /** Локальные состояния ввода (не реплицируются) */
    bool bSprintKeyDown = false;
    bool bSlideKeyDown = false;
    bool bProneKeyDown = false;
#pragma endregion

#pragma region Public Methods - Gait System
public:
    /** Обновляет все показатели движения (скорость, трение, ускорение) */
    UFUNCTION(BlueprintCallable, Category="TDS Gait")
    void UpdateMovementWithGait();

    /** Определяет желаемый гейт в зависимости от ввода */
    UFUNCTION(BlueprintPure, Category="TDS Gait")
    EGait GetDesiredGait() const;

    /** Получить текущий гейт */
    UFUNCTION(BlueprintPure, Category="TDS Gait")
    EGait GetCurrentGait() const { return CurrentGait; }

    /** Установить гейт (для Blueprint) */
    UFUNCTION(BlueprintCallable, Category="TDS Gait")
    void SetGait(EGait NewGait);

    /** Расчёт максимальной скорости с учетом гейта и направления */
    UFUNCTION(BlueprintPure, Category="TDS Gait")
    float CalculateMaxSpeedWithGait() const;

    /** Расчёт максимальной скорости в приседании */
    UFUNCTION(BlueprintPure, Category="TDS Gait")
    float CalculateMaxCrouchSpeed() const;

    /** Расчёт максимального ускорения в зависимости от гейта */
    UFUNCTION(BlueprintPure, Category="TDS Gait")
    float CalculateMaxAccelerationWithGait() const;

    /** Расчёт тормозного замедления */
    UFUNCTION(BlueprintPure, Category="TDS Gait")
    float CalculateBrakingDecelerationWithGait() const;

    /** Расчёт трения при движении по земле */
    UFUNCTION(BlueprintPure, Category="TDS Gait")
    float CalculateGroundFrictionWithGait() const;

    /** Проверяет, можно ли в текущей ситуации спринтовать */
    UFUNCTION(BlueprintPure, Category="TDS Gait")
    bool CanSprintWithGait() const;

    /** Получить ввод движения из Enhanced Input */
    UFUNCTION(BlueprintPure, Category="TDS Gait")
    FVector2D GetMovementInput() const;

    /** Получить ввод движения в мировом пространстве */
    UFUNCTION(BlueprintPure, Category="TDS Gait")
    FVector2D GetMovementWorldSpaceInput() const;

private:
    /** Обновить кэшированные значения ввода */
    void UpdateCachedInput() const;

    /** Проверяет, есть ли входной вектор движения */
    bool HasMovementInputVector() const;
#pragma endregion

#pragma region Public Methods - State Accessors
public:
    UFUNCTION(BlueprintCallable, Category="TDS States") 
    bool IsWalkingState() const { return WalkState; }
    
    UFUNCTION(BlueprintCallable, Category="TDS States") 
    bool IsSprintingState() const { return SprintState; }
    
    UFUNCTION(BlueprintCallable, Category="TDS States") 
    bool IsStrafingState() const { return StrafeState; }
    
    UFUNCTION(BlueprintCallable, Category="TDS States") 
    bool IsAimingState() const { return AimState; }

    UFUNCTION(BlueprintCallable, Category="TDS States")
    bool IsCustomMovementMode(ETDSCustomMovementMode CustomMode) const;

    UFUNCTION(BlueprintCallable, Category="TDS States")
    ETDSCustomMovementMode GetCurrentCustomMovementMode() const;
#pragma endregion

#pragma region Public Methods - State Mutators
public:
    UFUNCTION(BlueprintCallable, Category="TDS Movement")
    void SetWalking(bool NewWalk, bool bClientSimulation = false);

    UFUNCTION(BlueprintCallable, Category="TDS Movement")
    void SetSprinting(bool NewSprint, bool bClientSimulation = false);

    UFUNCTION(BlueprintCallable, Category="TDS Movement")
    void SetStrafing(bool NewStrafe, bool bClientSimulation = false);

    UFUNCTION(BlueprintCallable, Category="TDS Movement")
    void SetAiming(bool NewAim, bool bClientSimulation = false);

    /** Методы для кастомных движений */
    UFUNCTION(BlueprintCallable, Category="TDS Movement")
    void SetSlideInput(bool bSlidePressed);

    UFUNCTION(BlueprintCallable, Category="TDS Movement")
    void SetProneInput(bool bPronePressed);

    UFUNCTION(BlueprintCallable, Category="TDS Movement")
    void SetWallRunInput(bool bWallRunPressed);
#pragma endregion

#pragma region Public Methods - Custom Movement
public:
    /** Wall Running */
    UFUNCTION(BlueprintCallable, Category="TDS Custom Movement")
    bool BeginWallRun();

    UFUNCTION(BlueprintCallable, Category="TDS Custom Movement")
    void EndWallRun();

    /** Sliding */
    UFUNCTION(BlueprintCallable, Category="TDS Custom Movement")
    bool BeginSlide();

    UFUNCTION(BlueprintCallable, Category="TDS Custom Movement")
    void EndSlide();

    /** Prone */
    UFUNCTION(BlueprintCallable, Category="TDS Custom Movement")
    bool BeginProne();

    UFUNCTION(BlueprintCallable, Category="TDS Custom Movement")
    void EndProne();
#pragma endregion

#pragma region Blueprint Events
public:
    /** Событие для дополнительной логики в Blueprint */
    UFUNCTION(BlueprintNativeEvent, Category = "TDS Movement")
    void OnMovementCustomUpdated(float DeltaSeconds);
    virtual void OnMovementCustomUpdated_Implementation(float DeltaSeconds);

    /** События для кастомных движений */
    UFUNCTION(BlueprintImplementableEvent, Category="TDS Movement")
    void OnWallRunStarted(ETDSWallRunSide Side);

    UFUNCTION(BlueprintImplementableEvent, Category="TDS Movement")
    void OnWallRunEnded();

    UFUNCTION(BlueprintImplementableEvent, Category="TDS Movement")
    void OnSlideStarted();

    UFUNCTION(BlueprintImplementableEvent, Category="TDS Movement")
    void OnSlideEnded();

    UFUNCTION(BlueprintImplementableEvent, Category="TDS Movement")
    void OnProneStarted();

    UFUNCTION(BlueprintImplementableEvent, Category="TDS Movement")
    void OnProneEnded();

    /** События для системы гейтов */
    UFUNCTION(BlueprintImplementableEvent, Category="TDS Gait")
    void OnGaitChanged(EGait OldGait, EGait NewGait);

    UFUNCTION(BlueprintImplementableEvent, Category="TDS Gait")
    void OnMovementParametersUpdated();
#pragma endregion

#pragma region Protected Methods - Engine Overrides
protected:
    virtual void BeginPlay() override;
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;
    virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
    virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
    virtual float GetMaxSpeed() const override;
    virtual float GetMaxAcceleration() const override;
    virtual float GetMaxBrakingDeceleration() const override;
    virtual void ProcessLanded(const FHitResult& Hit, float RemainingTime, int32 Iterations) override;
    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
    virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
    virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
#pragma endregion

#pragma region Private Methods - Custom Movement Helpers
private:
    /** Wall Running Helper Functions */
    bool AreRequiredWallRunKeysDown() const;
    bool IsNextToWall(float VerticalTolerance = 0.0f);
    void FindWallRunDirectionAndSide(const FVector& SurfaceNormal, FVector& Direction, ETDSWallRunSide& Side) const;
    bool CanSurfaceBeWallRan(const FVector& SurfaceNormal) const;

    /** Slide Helper Functions */
    bool CanSlide() const;

    /** Prone Helper Functions */
    bool CanProne() const;

    /** Capsule Management */
    void SetCapsuleSize(float NewHalfHeight, float NewRadius = -1.0f, bool bUpdateOverlaps = true);
    void RestoreCapsuleSize();

    /** Custom Physics Functions */
    void PhysWallRunning(float DeltaTime, int32 Iterations);
    void PhysSliding(float DeltaTime, int32 Iterations);
    void PhysProne(float DeltaTime, int32 Iterations);

    /** Collision Events */
    UFUNCTION()
    void OnActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);
#pragma endregion
};

//////////////////////////////////////////////////////////////////////////
// FSavedMove_TDS - Расширенная версия для всех состояний

class FSavedMove_TDS : public FSavedMove_Character
{
public:
    typedef FSavedMove_Character Super;

    FSavedMove_TDS();
    
    virtual void Clear() override;
    virtual uint8 GetCompressedFlags() const override;
    virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;
    virtual void SetMoveFor(ACharacter* Character, float DeltaTime, const FVector& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
    virtual void PrepMoveFor(ACharacter* Character) override;

private:
    // Основные состояния
    uint8 SavedWalkState : 1;
    uint8 SavedSprintState : 1;
    uint8 SavedStrafeState : 1;
    uint8 SavedAimState : 1;
    
    // Кастомные состояния
    uint8 SavedWallRunKeysDown : 1;
    uint8 SavedSlideKeysDown : 1;
    uint8 SavedProneKeysDown : 1;
    
    // Система гейтов
    EGait SavedGait;
    FVector2D SavedMoveInput;
    FVector2D SavedMoveWSInput;
};

//////////////////////////////////////////////////////////////////////////
// FNetworkPredictionData_Client_TDS

class FNetworkPredictionData_Client_TDS : public FNetworkPredictionData_Client_Character
{
public:
    typedef FNetworkPredictionData_Client_Character Super;

    FNetworkPredictionData_Client_TDS(const UCharacterMovementComponent& InMovementComponent);
    virtual FSavedMovePtr AllocateNewMove() override;
};
