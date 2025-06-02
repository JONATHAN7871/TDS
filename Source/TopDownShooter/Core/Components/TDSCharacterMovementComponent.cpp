// TDSCharacterMovementComponent.cpp

#include "TDSCharacterMovementComponent.h"

#include "TDSCharacter.h"
#include "GameFramework/Character.h"

UTDSCharacterMovementComponent::UTDSCharacterMovementComponent()
{
    WalkState   = 0;
    SprintState = 0;
    StrafeState = 0;
    AimState    = 0;
}

void UTDSCharacterMovementComponent::SetWalking(bool NewWalk, bool bClientSimulation)
{
    
    WalkState = NewWalk;
    Cast<ATDSCharacter>(CharacterOwner)->bIsWalkingState = NewWalk;
    
}

void UTDSCharacterMovementComponent::SetSprinting(bool NewSprint, bool bClientSimulation)
{

    SprintState = NewSprint;
    Cast<ATDSCharacter>(CharacterOwner)->bIsSprintingState = NewSprint;
}
void UTDSCharacterMovementComponent::SetStrafing(bool NewStrafe, bool bClientSimulation)
{

    StrafeState = NewStrafe;
    Cast<ATDSCharacter>(CharacterOwner)->bIsStrafingState = NewStrafe;
}
void UTDSCharacterMovementComponent::SetAiming(bool NewAim, bool bClientSimulation)
{
   // if (!HasValidData())
   // {
   //     return;
   // }

    AimState = NewAim;
    Cast<ATDSCharacter>(CharacterOwner)->bIsAimingState = NewAim;
}

void UTDSCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);

    const bool NewWalk   = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
    const bool NewSprint = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
    const bool NewStrafe = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
    const bool NewAim    = (Flags & FSavedMove_Character::FLAG_Custom_3) != 0;

    WalkState   = NewWalk;
    SprintState = NewSprint;
    StrafeState = NewStrafe;
    AimState    = NewAim;

    // Вот эта часть – обновляем актёра на сервере:
    if (CharacterOwner->HasAuthority())
    {
        if (auto* Char = Cast<ATDSCharacter>(CharacterOwner))
        {
            Char->bIsWalkingState   = NewWalk;
            Char->bIsSprintingState = NewSprint;
            Char->bIsStrafingState  = NewStrafe;
            Char->bIsAimingState    = NewAim;
        }
    }
}

void UTDSCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
    Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
    // OnMovementCustomUpdated(DeltaSeconds);
}


void UTDSCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
    Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
    
    // Blueprint-коллбэк
    OnMovementCustomUpdated(DeltaSeconds);

    if (AimState || StrafeState)
    {
        bUseControllerDesiredRotation = true;
        bOrientRotationToMovement     = false;
    }
    else
    {
        bUseControllerDesiredRotation = false;
        bOrientRotationToMovement     = true;
    }
    
    RotationRate = IsFalling()
    ? FallingRotationRate
    : GroundRotationRate;
}

void UTDSCharacterMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UTDSCharacterMovementComponent::OnMovementCustomUpdated_Implementation(float DeltaSeconds)
{
    // Базовая C++-логика.
    // Если Blueprint-перевоплощение НЕ задано, то выполнится именно этот код.
}

FNetworkPredictionData_Client* UTDSCharacterMovementComponent::GetPredictionData_Client() const
{
    if (!ClientPredictionData)
    {
        auto* MutableThis = const_cast<UTDSCharacterMovementComponent*>(this);
        MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_TDS(*this);
        // MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
        // MutableThis->ClientPredictionData->NoSmoothNetUpdateDist   = 140.f;
        // MutableThis->ClientPredictionData->SmoothNetUpdateTime     = 0.10f;
    }
    return ClientPredictionData;
}

//////////////////////////////////////////////////////////////////////////
// FSavedMove_TDS

void FSavedMove_TDS::Clear()
{
    Super::Clear();
    SavedWalkState   = 0;
    SavedSprintState = 0;
    SavedStrafeState = 0;
    SavedAimState    = 0;
}

void FSavedMove_TDS::SetMoveFor(ACharacter* Character, float InDeltaTime, const FVector& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
    Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);

    if (auto* MoveComp = Character->GetCharacterMovement<UTDSCharacterMovementComponent>())
    {
        SavedWalkState   = MoveComp->WalkState;
        SavedSprintState = MoveComp->SprintState;
        SavedStrafeState = MoveComp->StrafeState;
        SavedAimState    = MoveComp->AimState;
    }
}

void FSavedMove_TDS::PrepMoveFor(ACharacter* Character)
{
    Super::PrepMoveFor(Character);

    if (auto* MoveComp = Cast<UTDSCharacterMovementComponent>(Character->GetCharacterMovement()))
    {
        MoveComp->WalkState   = SavedWalkState;
        MoveComp->SprintState = SavedSprintState;
        MoveComp->StrafeState = SavedStrafeState;
        MoveComp->AimState    = SavedAimState;
    }
}

uint8 FSavedMove_TDS::GetCompressedFlags() const
{
    uint8 Flags = Super::GetCompressedFlags();
    if (SavedWalkState)   Flags |= FLAG_Custom_0;
    if (SavedSprintState) Flags |= FLAG_Custom_1;
    if (SavedStrafeState) Flags |= FLAG_Custom_2;
    if (SavedAimState)    Flags |= FLAG_Custom_3;
    return Flags;
}

bool FSavedMove_TDS::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
    const auto* Other = static_cast<const FSavedMove_TDS*>(NewMove.Get());
    if (SavedWalkState   != Other->SavedWalkState ||
        SavedSprintState != Other->SavedSprintState ||
        SavedStrafeState != Other->SavedStrafeState ||
        SavedAimState    != Other->SavedAimState)
    {
        return false;
    }
    return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

//////////////////////////////////////////////////////////////////////////
// FNetworkPredictionData_Client_TDS

FNetworkPredictionData_Client_TDS::FNetworkPredictionData_Client_TDS(const UCharacterMovementComponent& InMovement)
    : FNetworkPredictionData_Client_Character(InMovement)
{}

FSavedMovePtr FNetworkPredictionData_Client_TDS::AllocateNewMove()
{
    return MakeShared<FSavedMove_TDS>();
}