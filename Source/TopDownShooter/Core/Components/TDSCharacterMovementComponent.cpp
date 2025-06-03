// TDSCharacterMovementComponent.cpp

#include "TDSCharacterMovementComponent.h"
#include "TDSCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/InputSettings.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

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

    // Реплицируем состояния всем, кроме владельца
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, WalkState, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, SprintState, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, StrafeState, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, AimState, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, WallRunKeysDown, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, SlideKeysDown, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, ProneKeysDown, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, WallRunDirection, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(UTDSCharacterMovementComponent, WallRunSide, COND_SkipOwner);
}

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
    switch (MovementMode)
    {
    case MOVE_Walking:
    case MOVE_NavWalking:
        if (IsCrouching())
        {
            return MaxWalkSpeedCrouched;
        }
        else if (SprintState)
        {
            return MaxWalkSpeed * 1.5f; // Sprint multiplier
        }
        else if (WalkState)
        {
            return MaxWalkSpeed * 0.5f; // Walk multiplier
        }
        return MaxWalkSpeed;
        
    case MOVE_Falling:
        return MaxWalkSpeed;
        
    case MOVE_Swimming:
        return MaxSwimSpeed;
        
    case MOVE_Flying:
        return MaxFlySpeed;
        
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
        
    case MOVE_None:
    default:
        return 0.f;
    }
}

float UTDSCharacterMovementComponent::GetMaxAcceleration() const
{
    if (IsMovingOnGround())
    {
        if (SprintState)
        {
            return MaxAcceleration * 1.2f; // Sprint acceleration boost
        }
        else if (WalkState)
        {
            return MaxAcceleration * 0.8f; // Walk acceleration reduction
        }
    }

    return Super::GetMaxAcceleration();
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
}

void FSavedMove_TDS::SetMoveFor(ACharacter* Character, float InDeltaTime, const FVector& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
    Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);

    if (UTDSCharacterMovementComponent* MoveComp = Character->GetCharacterMovement<UTDSCharacterMovementComponent>())
    {
        SavedWalkState = MoveComp->WalkState;
        SavedSprintState = MoveComp->SprintState;
        SavedStrafeState = MoveComp->StrafeState;
        SavedAimState = MoveComp->AimState;
        SavedWallRunKeysDown = MoveComp->WallRunKeysDown;
        SavedSlideKeysDown = MoveComp->SlideKeysDown;
        SavedProneKeysDown = MoveComp->ProneKeysDown;
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
        SavedProneKeysDown != Other->SavedProneKeysDown)
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
