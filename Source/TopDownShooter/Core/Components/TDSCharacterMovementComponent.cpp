// TDSCharacterMovementComponent.cpp

#include "TDSCharacterMovementComponent.h"
#include "TDSCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "KismetAnimationLibrary.h"
#include "Kismet/KismetMathLibrary.h"

//////////////////////////////////////////////////////////////////////////
// UTDSCharacterMovementComponent

UTDSCharacterMovementComponent::UTDSCharacterMovementComponent()
{
    // Инициализация состояний
    WalkState = 0;
    SprintState = 0;
    StrafeState = 0;
    AimState = 0;
    WallRunKeysDown = 0;
    SlideKeysDown = 0;
    ProneKeysDown = 0;
    bNetworkUpdateReceived = 0;

    // Инициализация Wall Running
    WallRunDirection = FVector::ZeroVector;
    WallRunSide = ETDSWallRunSide::Left;

    // Инициализация системы гейтов
    CurrentGait = EGait::Run;
    bUseGaitSystem = true;
    MovementStickMode = EAnalogStickBehavior::FixedSingleGait;
    AnalogWalkRunThreshold = 0.7f;

    // Значения скоростей по умолчанию
    WalkSpeeds = FVector(200.f, 180.f, 150.f);
    RunSpeeds = FVector(500.f, 350.f, 300.f);
    SprintSpeeds = FVector(700.f, 700.f, 700.f);
    CrouchSpeeds = FVector(225.f, 200.f, 180.f);

    // Кривая страфа по умолчанию не назначена
    StrafeSpeedMapCurve = nullptr;
}

void UTDSCharacterMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    // Сохраняем оригинальные размеры капсулы
    if (CharacterOwner && CharacterOwner->GetCapsuleComponent())
    {
        DefaultCapsuleHalfHeight = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
        DefaultCapsuleRadius = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
    }

    // Подписываемся на события коллизии только для Authority и AutonomousProxy
    if (GetPawnOwner()->GetLocalRole() > ROLE_SimulatedProxy)
    {
        GetPawnOwner()->OnActorHit.AddDynamic(this, &UTDSCharacterMovementComponent::OnActorHit);
    }
}

void UTDSCharacterMovementComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    if (GetPawnOwner() != nullptr && GetPawnOwner()->GetLocalRole() > ROLE_SimulatedProxy)
    {
        GetPawnOwner()->OnActorHit.RemoveDynamic(this, &UTDSCharacterMovementComponent::OnActorHit);
    }

    Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UTDSCharacterMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Существующие состояния
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, WalkState, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, SprintState, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, StrafeState, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, AimState, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, WallRunKeysDown, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, SlideKeysDown, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, ProneKeysDown, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, WallRunDirection, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, WallRunSide, COND_SkipOwner);
    
    // Новые переменные для системы гейтов
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, CurrentGait, COND_SkipOwner);
}

#pragma region Gait System Implementation

void UTDSCharacterMovementComponent::UpdateMovementWithGait()
{
    // Проверяем, активна ли система гейтов
    if (!bUseGaitSystem)
    {
        return;
    }

    // 1) Определяем текущий гейт
    EGait DesiredGait = GetDesiredGait();
    if (CurrentGait != DesiredGait)
    {
        EGait OldGait = CurrentGait;
        CurrentGait = DesiredGait;
        OnGaitChanged(OldGait, CurrentGait);
    }

    // 2) Устанавливаем максимальное ускорение
    MaxAcceleration = CalculateMaxAccelerationWithGait();

    // 3) Устанавливаем тормозное замедление
    BrakingDecelerationWalking = CalculateBrakingDecelerationWithGait();

    // 4) Устанавливаем трение по земле
    GroundFriction = CalculateGroundFrictionWithGait();

    // 5) Устанавливаем максимальную скорость без приседания
    if (!IsCrouching() && MovementMode != MOVE_Custom)
    {
        MaxWalkSpeed = CalculateMaxSpeedWithGait();
    }

    // 6) Устанавливаем максимальную скорость в режиме приседания
    if (IsCrouching())
    {
        MaxWalkSpeedCrouched = CalculateMaxCrouchSpeed();
    }

    // Вызываем событие для Blueprint
    OnMovementParametersUpdated();
}

EGait UTDSCharacterMovementComponent::GetDesiredGait() const
{
    // Если система гейтов отключена, возвращаем Run по умолчанию
    if (!bUseGaitSystem)
    {
        return EGait::Run;
    }

    // Получаем кэшированные значения ввода
    FVector2D MoveInput = GetMovementInput();
    FVector2D MoveWSInput = GetMovementWorldSpaceInput();

    // Вычисляем величины длины для каждого вектора
    float MoveMag = UKismetMathLibrary::VSize2D(MoveInput);
    float MoveWSMag = UKismetMathLibrary::VSize2D(MoveWSInput);

    // Определяем, есть ли «полный ввод» в зависимости от режима стика
    bool bFullMovementInput = false;
    switch (MovementStickMode)
    {
        case EAnalogStickBehavior::FixedSingleGait:
        case EAnalogStickBehavior::VariableSingleGait:
            bFullMovementInput = true;
            break;

        case EAnalogStickBehavior::FixedWalkRun:
        case EAnalogStickBehavior::VariableWalkRun:
        {
            float MaxMag = FMath::Max(MoveMag, MoveWSMag);
            bFullMovementInput = (MaxMag >= AnalogWalkRunThreshold);
        }
        break;

        default:
            bFullMovementInput = false;
            break;
    }

    // Используем существующие состояния для определения гейта
    if (bFullMovementInput)
    {
        if (SprintState && CanSprintWithGait())
        {
            return EGait::Sprint;
        }
        else
        {
            return EGait::Run;
        }
    }
    else
    {
        if (WalkState)
        {
            return EGait::Walk;
        }
        else
        {
            return EGait::Run;
        }
    }
}

void UTDSCharacterMovementComponent::SetGait(EGait NewGait)
{
    if (CurrentGait != NewGait)
    {
        EGait OldGait = CurrentGait;
        CurrentGait = NewGait;
        
        // Обновляем параметры движения
        UpdateMovementWithGait();
        
        // Уведомляем Blueprint
        OnGaitChanged(OldGait, CurrentGait);
    }
}

float UTDSCharacterMovementComponent::CalculateMaxSpeedWithGait() const
{
    if (!bUseGaitSystem)
    {
        // Используем оригинальную логику
        return MaxWalkSpeed;
    }

    // Получаем скорость и направление движения
    const FVector VelocityLocal = GetLastUpdateVelocity();
    const FRotator BaseRotation = GetCharacterOwner() ? GetCharacterOwner()->GetActorRotation() : FRotator::ZeroRotator;
    const float DirectionAngle = UKismetAnimationLibrary::CalculateDirection(VelocityLocal, BaseRotation);
    const float AbsDirectionAngle = FMath::Abs(DirectionAngle);

    // Получаем значение из кривой страфа
    float StrafeSpeedMap = 0.f;
    if (StrafeSpeedMapCurve && !bUseControllerDesiredRotation)
    {
        StrafeSpeedMap = StrafeSpeedMapCurve->GetFloatValue(AbsDirectionAngle);
    }

    // Выбираем скорости в зависимости от гейта
    FVector GaitSpeeds;
    switch (CurrentGait)
    {
        case EGait::Walk:
            GaitSpeeds = WalkSpeeds;
            break;
        case EGait::Run:
            GaitSpeeds = RunSpeeds;
            break;
        case EGait::Sprint:
            GaitSpeeds = SprintSpeeds;
            break;
        default:
            GaitSpeeds = RunSpeeds;
            break;
    }

    // Интерполируем между скоростями на основе направления
    float MappedSpeed = 0.f;
    if (StrafeSpeedMap < 1.f)
    {
        // Интерполяция между Forward и Strafe
        MappedSpeed = UKismetMathLibrary::MapRangeClamped(
            StrafeSpeedMap,
            0.f, 1.f,
            GaitSpeeds.X, GaitSpeeds.Y
        );
    }
    else
    {
        // Интерполяция между Strafe и Backwards
        MappedSpeed = UKismetMathLibrary::MapRangeClamped(
            StrafeSpeedMap,
            1.f, 2.f,
            GaitSpeeds.Y, GaitSpeeds.Z
        );
    }

    return MappedSpeed;
}

float UTDSCharacterMovementComponent::CalculateMaxCrouchSpeed() const
{
    if (!bUseGaitSystem)
    {
        return MaxWalkSpeedCrouched;
    }

    const FVector VelocityLocal = Velocity;
    const FRotator BaseRotation = GetCharacterOwner() ? GetCharacterOwner()->GetActorRotation() : FRotator::ZeroRotator;
    const float DirectionAngle = UKismetAnimationLibrary::CalculateDirection(VelocityLocal, BaseRotation);
    const float AbsDirectionAngle = FMath::Abs(DirectionAngle);

    float StrafeSpeedMap = 0.f;
    if (StrafeSpeedMapCurve && !bOrientRotationToMovement)
    {
        StrafeSpeedMap = StrafeSpeedMapCurve->GetFloatValue(AbsDirectionAngle);
    }

    const FVector Crouch = CrouchSpeeds;
    float MappedSpeed = 0.f;

    if (StrafeSpeedMap < 1.f)
    {
        MappedSpeed = UKismetMathLibrary::MapRangeClamped(
            StrafeSpeedMap,
            0.f, 1.f,
            Crouch.X, Crouch.Y
        );
    }
    else
    {
        MappedSpeed = UKismetMathLibrary::MapRangeClamped(
            StrafeSpeedMap,
            1.f, 2.f,
            Crouch.Y, Crouch.Z
        );
    }

    return MappedSpeed;
}

float UTDSCharacterMovementComponent::CalculateMaxAccelerationWithGait() const
{
    if (!bUseGaitSystem)
    {
        return MaxAcceleration;
    }

    switch (CurrentGait)
    {
        case EGait::Walk:
        case EGait::Run:
            return 800.f;

        case EGait::Sprint:
        {
            float SpeedXY = Velocity.Size2D();
            // Интерполяция ускорения для спринта
            return UKismetMathLibrary::MapRangeClamped(
                SpeedXY,
                300.f, 700.f,
                800.f, 300.f
            );
        }

        default:
            return 800.f;
    }
}

float UTDSCharacterMovementComponent::CalculateBrakingDecelerationWithGait() const
{
    if (!bUseGaitSystem)
    {
        return BrakingDecelerationWalking;
    }

    // Если нет входного вектора движения – тормозим "жёстко"
    if (!HasMovementInputVector())
    {
        return 2000.f;
    }
    
    // Иначе – стандартное замедление
    return 500.f;
}

float UTDSCharacterMovementComponent::CalculateGroundFrictionWithGait() const
{
    if (!bUseGaitSystem)
    {
        return GroundFriction;
    }

    switch (CurrentGait)
    {
        case EGait::Walk:
        case EGait::Run:
            return 5.f;

        case EGait::Sprint:
        {
            float Speed2D = Velocity.Size2D();
            return UKismetMathLibrary::MapRangeClamped(
                Speed2D,
                0.f, 500.f,
                5.f, 3.f
            );
        }

        default:
            return 5.f;
    }
}

bool UTDSCharacterMovementComponent::CanSprintWithGait() const
{
    if (!SprintState)
    {
        return false;
    }

    const ACharacter* Character = GetCharacterOwner();
    if (!Character)
    {
        return false;
    }

    const FVector CurrentAccel = GetCurrentAcceleration();
    if (CurrentAccel.IsNearlyZero())
    {
        return false;
    }

    // Проверяем направление движения
    const FRotator ActorRotation = Character->GetActorRotation();
    const FRotator AccelRotator = Acceleration.Rotation();
    const FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(ActorRotation, AccelRotator);
    float AbsYawDelta = FMath::Abs(DeltaRot.Yaw);

    // Спринт доступен только при движении вперед (±50 градусов)
    bool bFacingWithinThreshold = (AbsYawDelta < 50.f);
    bool bOrientRotToMovement = bOrientRotationToMovement;
    bool bDirectionCheck = (bOrientRotToMovement ? true : bFacingWithinThreshold);

    return bDirectionCheck;
}

bool UTDSCharacterMovementComponent::HasMovementInputVector() const
{
    const ACharacter* Character = GetCharacterOwner();
    if (!Character)
    {
        return false;
    }

    const FVector PendingInput = Character->GetPendingMovementInputVector();
    return (PendingInput != FVector::ZeroVector);
}

FVector2D UTDSCharacterMovementComponent::GetMovementInput() const
{
    // Обновляем кэш при необходимости
    UpdateCachedInput();
    return CachedMoveInput;
}

FVector2D UTDSCharacterMovementComponent::GetMovementWorldSpaceInput() const
{
    // Обновляем кэш при необходимости
    UpdateCachedInput();
    return CachedMoveWorldSpaceInput;
}

void UTDSCharacterMovementComponent::UpdateCachedInput() const
{
    // Проверяем, нужно ли обновить кэш (раз в кадр)
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (CurrentTime == LastInputUpdateTime)
    {
        return;
    }

    // Обновляем время последнего обновления
    LastInputUpdateTime = CurrentTime;
    
    // Сбрасываем значения
    CachedMoveInput = FVector2D::ZeroVector;
    CachedMoveWorldSpaceInput = FVector2D::ZeroVector;

    const ACharacter* CharOwner = GetCharacterOwner();
    if (!CharOwner)
    {
        return;
    }

    const APlayerController* PC = Cast<APlayerController>(CharOwner->GetController());
    if (!PC)
    {
        return;
    }

    // Получаем Enhanced Input Component
    const UEnhancedInputComponent* InputComp = Cast<UEnhancedInputComponent>(PC->InputComponent);
    if (!InputComp)
    {
        return;
    }

    // Получаем значения действий ввода
    // Примечание: В реальном проекте нужно получить ссылки на InputAction через UPROPERTY
    // Здесь используется поиск по имени для примера
    
    // 5) Достаем указатель на ATDSCharacter, чтобы взять оттуда UInputAction*
    const ATDSCharacter* TDSChar = Cast<ATDSCharacter>(CharOwner);
    if (!TDSChar)
    {
        return;
    }
    
    // 6) Если IA_Move задан в Blueprint, получаем его текущее значение
    if (const UInputAction* MoveAction = TDSChar->IA_Move)
    {
        FInputActionValue Val = InputComp->GetBoundActionValue(MoveAction);
        CachedMoveInput = Val.Get<FVector2D>();
    }

    // 7) Аналогично для IA_Move_WorldSpace
    if (const UInputAction* MoveWorldAction = TDSChar->IA_Move_WorldSpace)
    {
        FInputActionValue Val = InputComp->GetBoundActionValue(MoveWorldAction);
        CachedMoveWorldSpaceInput = Val.Get<FVector2D>();
    }
}

#pragma endregion

#pragma region State Management

void UTDSCharacterMovementComponent::SetWalking(bool NewWalk, bool bClientSimulation)
{
    WalkState = NewWalk;
    if (ATDSCharacter* TDSChar = Cast<ATDSCharacter>(CharacterOwner))
    {
        TDSChar->bIsWalkingState = NewWalk;
    }
}

void UTDSCharacterMovementComponent::SetSprinting(bool NewSprint, bool bClientSimulation)
{
    SprintState = NewSprint;
    if (ATDSCharacter* TDSChar = Cast<ATDSCharacter>(CharacterOwner))
    {
        TDSChar->bIsSprintingState = NewSprint;
    }
}

void UTDSCharacterMovementComponent::SetStrafing(bool NewStrafe, bool bClientSimulation)
{
    StrafeState = NewStrafe;
    if (ATDSCharacter* TDSChar = Cast<ATDSCharacter>(CharacterOwner))
    {
        TDSChar->bIsStrafingState = NewStrafe;
    }
}

void UTDSCharacterMovementComponent::SetAiming(bool NewAim, bool bClientSimulation)
{
    AimState = NewAim;
    if (ATDSCharacter* TDSChar = Cast<ATDSCharacter>(CharacterOwner))
    {
        TDSChar->bIsAimingState = NewAim;
    }
}

void UTDSCharacterMovementComponent::SetSlideInput(bool bSlidePressed)
{
    bSlideKeyDown = bSlidePressed;
    SlideKeysDown = bSlidePressed;
}

void UTDSCharacterMovementComponent::SetProneInput(bool bPronePressed)
{
    bProneKeyDown = bPronePressed;
    ProneKeysDown = bPronePressed;
}

void UTDSCharacterMovementComponent::SetWallRunInput(bool bWallRunPressed)
{
    WallRunKeysDown = bWallRunPressed;
}

bool UTDSCharacterMovementComponent::IsCustomMovementMode(ETDSCustomMovementMode CustomMode) const
{
    return MovementMode == EMovementMode::MOVE_Custom && 
           CustomMovementMode == static_cast<uint8>(CustomMode);
}

ETDSCustomMovementMode UTDSCharacterMovementComponent::GetCurrentCustomMovementMode() const
{
    if (MovementMode == EMovementMode::MOVE_Custom)
    {
        return static_cast<ETDSCustomMovementMode>(CustomMovementMode);
    }
    return ETDSCustomMovementMode::CMOVE_None;
}

#pragma endregion

#pragma region Custom Movement - Wall Running

bool UTDSCharacterMovementComponent::BeginWallRun()
{
    if (!WallRunKeysDown || !IsFalling())
    {
        return false;
    }

    SetMovementMode(EMovementMode::MOVE_Custom, static_cast<uint8>(ETDSCustomMovementMode::CMOVE_WallRunning));
    OnWallRunStarted(WallRunSide);
    return true;
}

void UTDSCharacterMovementComponent::EndWallRun()
{
    if (IsCustomMovementMode(ETDSCustomMovementMode::CMOVE_WallRunning))
    {
        SetMovementMode(EMovementMode::MOVE_Falling);
        OnWallRunEnded();
    }
}

bool UTDSCharacterMovementComponent::AreRequiredWallRunKeysDown() const
{
    if (!GetPawnOwner()->IsLocallyControlled())
    {
        return WallRunKeysDown;
    }

    // Проверяем, что нажаты Sprint + дополнительная клавиша wall run
    return SprintState && WallRunKeysDown;
}

bool UTDSCharacterMovementComponent::IsNextToWall(float VerticalTolerance)
{
    FVector CrossVector = WallRunSide == ETDSWallRunSide::Left ? 
        FVector(0.0f, 0.0f, -1.0f) : FVector(0.0f, 0.0f, 1.0f);
    
    FVector TraceStart = GetPawnOwner()->GetActorLocation() + (WallRunDirection * 20.0f);
    FVector TraceEnd = TraceStart + (FVector::CrossProduct(WallRunDirection, CrossVector) * 100.0f);
    FHitResult HitResult;

    auto LineTrace = [&](const FVector& Start, const FVector& End)
    {
        return GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility);
    };

    if (VerticalTolerance > FLT_EPSILON)
    {
        if (!LineTrace(FVector(TraceStart.X, TraceStart.Y, TraceStart.Z + VerticalTolerance / 2.0f),
                      FVector(TraceEnd.X, TraceEnd.Y, TraceEnd.Z + VerticalTolerance / 2.0f)) &&
            !LineTrace(FVector(TraceStart.X, TraceStart.Y, TraceStart.Z - VerticalTolerance / 2.0f),
                      FVector(TraceEnd.X, TraceEnd.Y, TraceEnd.Z - VerticalTolerance / 2.0f)))
        {
            return false;
        }
    }
    else
    {
        if (!LineTrace(TraceStart, TraceEnd))
        {
            return false;
        }
    }

    ETDSWallRunSide NewWallRunSide;
    FindWallRunDirectionAndSide(HitResult.ImpactNormal, WallRunDirection, NewWallRunSide);
    return NewWallRunSide == WallRunSide;
}

void UTDSCharacterMovementComponent::FindWallRunDirectionAndSide(const FVector& SurfaceNormal, FVector& Direction, ETDSWallRunSide& Side) const
{
    FVector CrossVector;
    
    if (FVector2D::DotProduct(FVector2D(SurfaceNormal), FVector2D(GetPawnOwner()->GetActorRightVector())) > 0.0f)
    {
        Side = ETDSWallRunSide::Right;
        CrossVector = FVector(0.0f, 0.0f, 1.0f);
    }
    else
    {
        Side = ETDSWallRunSide::Left;
        CrossVector = FVector(0.0f, 0.0f, -1.0f);
    }

    Direction = FVector::CrossProduct(SurfaceNormal, CrossVector);
}

bool UTDSCharacterMovementComponent::CanSurfaceBeWallRan(const FVector& SurfaceNormal) const
{
    if (SurfaceNormal.Z < -0.05f)
    {
        return false;
    }

    FVector NormalNoZ = FVector(SurfaceNormal.X, SurfaceNormal.Y, 0.0f);
    NormalNoZ.Normalize();

    float WallAngle = FMath::Acos(FVector::DotProduct(NormalNoZ, SurfaceNormal));
    return WallAngle < GetWalkableFloorAngle();
}

void UTDSCharacterMovementComponent::PhysWallRunning(float DeltaTime, int32 Iterations)
{
    if (!AreRequiredWallRunKeysDown())
    {
        EndWallRun();
        return;
    }

    if (!IsNextToWall(LineTraceVerticalTolerance))
    {
        EndWallRun();
        return;
    }

    FVector NewVelocity = WallRunDirection;
    NewVelocity.X *= WallRunSpeed;
    NewVelocity.Y *= WallRunSpeed;
    NewVelocity.Z = FMath::Max(Velocity.Z * WallRunGravityScale, -GetMaxSpeed() * 0.1f);
    Velocity = NewVelocity;

    const FVector Adjusted = Velocity * DeltaTime;
    FHitResult Hit(1.f);
    SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, Hit);
}

#pragma endregion

#pragma region Custom Movement - Sliding

bool UTDSCharacterMovementComponent::BeginSlide()
{
    if (!CanSlide())
    {
        return false;
    }

    SetMovementMode(EMovementMode::MOVE_Custom, static_cast<uint8>(ETDSCustomMovementMode::CMOVE_Sliding));
    SetCapsuleSize(SlideCapsuleHalfHeight);
    OnSlideStarted();
    return true;
}

void UTDSCharacterMovementComponent::EndSlide()
{
    if (IsCustomMovementMode(ETDSCustomMovementMode::CMOVE_Sliding))
    {
        RestoreCapsuleSize();
        SetMovementMode(EMovementMode::MOVE_Walking);
        OnSlideEnded();
    }
}

bool UTDSCharacterMovementComponent::CanSlide() const
{
    // Slide можно активировать только во время Sprint
    return SprintState && SlideKeysDown && IsMovingOnGround() && !IsCrouching();
}

void UTDSCharacterMovementComponent::PhysSliding(float DeltaTime, int32 Iterations)
{
    if (!SlideKeysDown || Velocity.Size() < MinSlideSpeed)
    {
        EndSlide();
        return;
    }

    // Применяем замедление
    FVector SlideDirection = Velocity.GetSafeNormal();
    float CurrentSpeed = Velocity.Size();
    float NewSpeed = FMath::Max(CurrentSpeed - SlideDeceleration * DeltaTime, MinSlideSpeed);
    
    Velocity = SlideDirection * NewSpeed;

    const FVector Adjusted = Velocity * DeltaTime;
    FHitResult Hit(1.f);
    SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, Hit);
    
    if (Hit.bBlockingHit)
    {
        EndSlide();
    }
}

#pragma endregion

#pragma region Custom Movement - Prone

bool UTDSCharacterMovementComponent::BeginProne()
{
    if (!CanProne())
    {
        return false;
    }

    SetMovementMode(EMovementMode::MOVE_Custom, static_cast<uint8>(ETDSCustomMovementMode::CMOVE_Prone));
    SetCapsuleSize(ProneCapsuleHalfHeight);
    OnProneStarted();
    return true;
}

void UTDSCharacterMovementComponent::EndProne()
{
    if (IsCustomMovementMode(ETDSCustomMovementMode::CMOVE_Prone))
    {
        RestoreCapsuleSize();
        SetMovementMode(EMovementMode::MOVE_Walking);
        OnProneEnded();
    }
}

bool UTDSCharacterMovementComponent::CanProne() const
{
    // Prone можно активировать только из Crouch
    return IsCrouching() && ProneKeysDown && IsMovingOnGround();
}

void UTDSCharacterMovementComponent::PhysProne(float DeltaTime, int32 Iterations)
{
    if (!ProneKeysDown)
    {
        EndProne();
        return;
    }

    // Ограничиваем скорость в Prone
    if (Velocity.Size() > ProneSpeed)
    {
        Velocity = Velocity.GetSafeNormal() * ProneSpeed;
    }

    const FVector Adjusted = Velocity * DeltaTime;
    FHitResult Hit(1.f);
    SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, Hit);
}

#pragma endregion

#pragma region Capsule Management

void UTDSCharacterMovementComponent::SetCapsuleSize(float NewHalfHeight, float NewRadius, bool bUpdateOverlaps)
{
    if (!CharacterOwner || !CharacterOwner->GetCapsuleComponent())
    {
        return;
    }

    const float RadiusToUse = (NewRadius > 0.0f) ? NewRadius : DefaultCapsuleRadius;
    
    CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(RadiusToUse, NewHalfHeight, bUpdateOverlaps);
    CharacterOwner->BaseEyeHeight = NewHalfHeight * 0.8f; // Настраиваем высоту глаз
}

void UTDSCharacterMovementComponent::RestoreCapsuleSize()
{
    SetCapsuleSize(DefaultCapsuleHalfHeight, DefaultCapsuleRadius);
}

#pragma endregion

#pragma region Event Handlers

void UTDSCharacterMovementComponent::OnActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
    // Wall Running logic
    if (IsCustomMovementMode(ETDSCustomMovementMode::CMOVE_WallRunning))
    {
        return;
    }

    if (!IsFalling())
    {
        return;
    }

    if (!CanSurfaceBeWallRan(Hit.ImpactNormal))
    {
        return;
    }

    FindWallRunDirectionAndSide(Hit.ImpactNormal, WallRunDirection, WallRunSide);

    if (!IsNextToWall())
    {
        return;
    }

    BeginWallRun();
}

#pragma endregion

#pragma region Engine Overrides

void UTDSCharacterMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    // Локальная логика управления
    if (GetPawnOwner()->IsLocallyControlled())
    {
        // Логика для слайда
        if (bSlideKeyDown && CanSlide() && !IsCustomMovementMode(ETDSCustomMovementMode::CMOVE_Sliding))
        {
            BeginSlide();
        }
        else if (!bSlideKeyDown && IsCustomMovementMode(ETDSCustomMovementMode::CMOVE_Sliding))
        {
            EndSlide();
        }

        // Логика для prone
        if (bProneKeyDown && CanProne() && !IsCustomMovementMode(ETDSCustomMovementMode::CMOVE_Prone))
        {
            BeginProne();
        }
        else if (!bProneKeyDown && IsCustomMovementMode(ETDSCustomMovementMode::CMOVE_Prone))
        {
            EndProne();
        }
    }

    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UTDSCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);

    // Извлекаем состояния из сжатых флагов
    const bool NewWalk = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
    const bool NewSprint = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
    const bool NewStrafe = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
    const bool NewAim = (Flags & FSavedMove_Character::FLAG_Custom_3) != 0;

    // Дополнительные флаги для кастомных движений (используем биты из основного байта)
    const bool NewWallRun = SprintState; // Wall run требует sprint
    const bool NewSlide = SlideKeysDown;
    const bool NewProne = ProneKeysDown;

    // Обновляем состояния
    WalkState = NewWalk;
    SprintState = NewSprint;
    StrafeState = NewStrafe;
    AimState = NewAim;
    WallRunKeysDown = NewWallRun;
    SlideKeysDown = NewSlide;
    ProneKeysDown = NewProne;

    // Синхронизируем с персонажем на сервере
    if (CharacterOwner->HasAuthority())
    {
        if (ATDSCharacter* TDSChar = Cast<ATDSCharacter>(CharacterOwner))
        {
            TDSChar->bIsWalkingState = NewWalk;
            TDSChar->bIsSprintingState = NewSprint;
            TDSChar->bIsStrafingState = NewStrafe;
            TDSChar->bIsAimingState = NewAim;
        }
    }
}

void UTDSCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
    Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
    
    // Обновляем параметры движения с учетом гейта
    if (bUseGaitSystem)
    {
        UpdateMovementWithGait();
    }
    
    // Вызываем Blueprint событие
    OnMovementCustomUpdated(DeltaSeconds);

    // Логика ротации для всех ролей
    const bool bShouldUseControllerRotation = (AimState || StrafeState);
    
    if (GetOwner()->GetLocalRole() == ROLE_SimulatedProxy)
    {
        // Принудительное обновление для SimulatedProxy
        bUseControllerDesiredRotation = bShouldUseControllerRotation;
        bOrientRotationToMovement = !bShouldUseControllerRotation;
    }
    else
    {
        // Стандартная логика для Authority/AutonomousProxy
        bUseControllerDesiredRotation = bShouldUseControllerRotation;
        bOrientRotationToMovement = !bShouldUseControllerRotation;
    }
    
    // Настройка скорости поворота
    RotationRate = IsFalling() ? FallingRotationRate : GroundRotationRate;
}

void UTDSCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
    // Обработка входа в кастомные режимы
    if (MovementMode == MOVE_Custom)
    {
        switch (static_cast<ETDSCustomMovementMode>(CustomMovementMode))
        {
        case ETDSCustomMovementMode::CMOVE_WallRunning:
            StopMovementImmediately();
            bConstrainToPlane = true;
            SetPlaneConstraintNormal(FVector(0.0f, 0.0f, 1.0f));
            break;
            
        case ETDSCustomMovementMode::CMOVE_Sliding:
            // Слайд не требует дополнительной настройки
            break;
            
        case ETDSCustomMovementMode::CMOVE_Prone:
            // Prone не требует дополнительной настройки
            break;
        }
    }

    // Обработка выхода из кастомных режимов
    if (PreviousMovementMode == MOVE_Custom)
    {
        switch (static_cast<ETDSCustomMovementMode>(PreviousCustomMode))
        {
        case ETDSCustomMovementMode::CMOVE_WallRunning:
            bConstrainToPlane = false;
            break;
            
        case ETDSCustomMovementMode::CMOVE_Sliding:
        case ETDSCustomMovementMode::CMOVE_Prone:
            // Капсула уже восстановлена в End функциях
            break;
        }
    }

    Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

void UTDSCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
    // Проверка роли для предотвращения выполнения на SimulatedProxy
    if (GetOwner()->GetLocalRole() == ROLE_SimulatedProxy)
    {
        return;
    }

    switch (static_cast<ETDSCustomMovementMode>(CustomMovementMode))
    {
    case ETDSCustomMovementMode::CMOVE_WallRunning:
        PhysWallRunning(DeltaTime, Iterations);
        break;
        
    case ETDSCustomMovementMode::CMOVE_Sliding:
        PhysSliding(DeltaTime, Iterations);
        break;
        
    case ETDSCustomMovementMode::CMOVE_Prone:
        PhysProne(DeltaTime, Iterations);
        break;
    }

    Super::PhysCustom(DeltaTime, Iterations);
}

float UTDSCharacterMovementComponent::GetMaxSpeed() const
{
    // Сначала проверяем кастомные режимы
    switch (MovementMode)
    {
    case MOVE_Custom:
        switch (static_cast<ETDSCustomMovementMode>(CustomMovementMode))
        {
        case ETDSCustomMovementMode::CMOVE_WallRunning:
            return WallRunSpeed;
        case ETDSCustomMovementMode::CMOVE_Sliding:
            return SlideSpeed;
        case ETDSCustomMovementMode::CMOVE_Prone:
            return ProneSpeed;
        }
        return MaxCustomMovementSpeed;
        
    case MOVE_Walking:
    case MOVE_NavWalking:
        if (IsCrouching())
        {
            return bUseGaitSystem ? CalculateMaxCrouchSpeed() : MaxWalkSpeedCrouched;
        }
        else if (bUseGaitSystem)
        {
            return CalculateMaxSpeedWithGait();
        }
        else
        {
            // Оригинальная логика
            if (SprintState)
            {
                return MaxWalkSpeed * 1.5f;
            }
            else if (WalkState)
            {
                return MaxWalkSpeed * 0.5f;
            }
        }
        return MaxWalkSpeed;
        
    case MOVE_Falling:
        return MaxWalkSpeed;
        
    case MOVE_Swimming:
        return MaxSwimSpeed;
        
    case MOVE_Flying:
        return MaxFlySpeed;
        
    case MOVE_None:
    default:
        return 0.f;
    }
}

float UTDSCharacterMovementComponent::GetMaxAcceleration() const
{
    if (bUseGaitSystem && IsMovingOnGround())
    {
        return CalculateMaxAccelerationWithGait();
    }

    // Оригинальная логика
    if (IsMovingOnGround())
    {
        if (SprintState)
        {
            return MaxAcceleration * 1.2f;
        }
        else if (WalkState)
        {
            return MaxAcceleration * 0.8f;
        }
    }

    return Super::GetMaxAcceleration();
}

float UTDSCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
    if (bUseGaitSystem)
    {
        return CalculateBrakingDecelerationWithGait();
    }

    return Super::GetMaxBrakingDeceleration();
}

void UTDSCharacterMovementComponent::ProcessLanded(const FHitResult& Hit, float RemainingTime, int32 Iterations)
{
    Super::ProcessLanded(Hit, RemainingTime, Iterations);

    // Завершаем Wall Running при приземлении
    if (IsCustomMovementMode(ETDSCustomMovementMode::CMOVE_WallRunning))
    {
        EndWallRun();
    }
}

void UTDSCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
    Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
}

FNetworkPredictionData_Client* UTDSCharacterMovementComponent::GetPredictionData_Client() const
{
    if (!ClientPredictionData)
    {
        UTDSCharacterMovementComponent* MutableThis = const_cast<UTDSCharacterMovementComponent*>(this);
        MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_TDS(*this);
    }
    return ClientPredictionData;
}

void UTDSCharacterMovementComponent::OnMovementCustomUpdated_Implementation(float DeltaSeconds)
{
    // Базовая реализация - может быть переопределена в Blueprint
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FSavedMove_TDS Implementation

FSavedMove_TDS::FSavedMove_TDS()
    : SavedWalkState(0)
    , SavedSprintState(0)
    , SavedStrafeState(0)
    , SavedAimState(0)
    , SavedWallRunKeysDown(0)
    , SavedSlideKeysDown(0)
    , SavedProneKeysDown(0)
    , SavedGait(EGait::Run)
    , SavedMoveInput(FVector2D::ZeroVector)
    , SavedMoveWSInput(FVector2D::ZeroVector)
{
}

void FSavedMove_TDS::Clear()
{
    Super::Clear();
    
    SavedWalkState = 0;
    SavedSprintState = 0;
    SavedStrafeState = 0;
    SavedAimState = 0;
    SavedWallRunKeysDown = 0;
    SavedSlideKeysDown = 0;
    SavedProneKeysDown = 0;
    SavedGait = EGait::Run;
    SavedMoveInput = FVector2D::ZeroVector;
    SavedMoveWSInput = FVector2D::ZeroVector;
}

void FSavedMove_TDS::SetMoveFor(ACharacter* Character, float InDeltaTime, const FVector& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
    Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);

    if (UTDSCharacterMovementComponent* MoveComp = Cast<UTDSCharacterMovementComponent>(Character->GetCharacterMovement()))
    {
        SavedWalkState = MoveComp->WalkState;
        SavedSprintState = MoveComp->SprintState;
        SavedStrafeState = MoveComp->StrafeState;
        SavedAimState = MoveComp->AimState;
        SavedWallRunKeysDown = MoveComp->WallRunKeysDown;
        SavedSlideKeysDown = MoveComp->SlideKeysDown;
        SavedProneKeysDown = MoveComp->ProneKeysDown;
        
        // Сохраняем данные гейта
        SavedGait = MoveComp->GetCurrentGait();
        SavedMoveInput = MoveComp->GetMovementInput();
        SavedMoveWSInput = MoveComp->GetMovementWorldSpaceInput();
    }
}

void FSavedMove_TDS::PrepMoveFor(ACharacter* Character)
{
    Super::PrepMoveFor(Character);

    if (UTDSCharacterMovementComponent* MoveComp = Cast<UTDSCharacterMovementComponent>(Character->GetCharacterMovement()))
    {
        MoveComp->WalkState = SavedWalkState;
        MoveComp->SprintState = SavedSprintState;
        MoveComp->StrafeState = SavedStrafeState;
        MoveComp->AimState = SavedAimState;
        MoveComp->WallRunKeysDown = SavedWallRunKeysDown;
        MoveComp->SlideKeysDown = SavedSlideKeysDown;
        MoveComp->ProneKeysDown = SavedProneKeysDown;
        
        // Восстанавливаем гейт
        MoveComp->SetGait(SavedGait);
    }
}

uint8 FSavedMove_TDS::GetCompressedFlags() const
{
    uint8 Flags = Super::GetCompressedFlags();
    
    // Используем доступные кастомные флаги
    if (SavedWalkState) Flags |= FLAG_Custom_0;
    if (SavedSprintState) Flags |= FLAG_Custom_1;
    if (SavedStrafeState) Flags |= FLAG_Custom_2;
    if (SavedAimState) Flags |= FLAG_Custom_3;
    
    // Для дополнительных состояний используем другие механизмы
    // или расширяем систему флагов
    
    return Flags;
}

bool FSavedMove_TDS::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
    const FSavedMove_TDS* Other = static_cast<const FSavedMove_TDS*>(NewMove.Get());
    
    // Проверяем, что все состояния совпадают
    if (SavedWalkState != Other->SavedWalkState ||
        SavedSprintState != Other->SavedSprintState ||
        SavedStrafeState != Other->SavedStrafeState ||
        SavedAimState != Other->SavedAimState ||
        SavedWallRunKeysDown != Other->SavedWallRunKeysDown ||
        SavedSlideKeysDown != Other->SavedSlideKeysDown ||
        SavedProneKeysDown != Other->SavedProneKeysDown ||
        SavedGait != Other->SavedGait)
    {
        return false;
    }
    
    return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

//////////////////////////////////////////////////////////////////////////
// FNetworkPredictionData_Client_TDS Implementation

FNetworkPredictionData_Client_TDS::FNetworkPredictionData_Client_TDS(const UCharacterMovementComponent& InMovement)
    : FNetworkPredictionData_Client_Character(InMovement)
{
}

FSavedMovePtr FNetworkPredictionData_Client_TDS::AllocateNewMove()
{
    return MakeShared<FSavedMove_TDS>();
}
