// Copyright 2025, CRAFTCODE, All Rights Reserved.

#include "TDSCharacter.h"
#include "TDSCharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"


ATDSCharacter::ATDSCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bIsWalkingState = false;
    bIsSprintingState = false;
    bIsStrafingState = false;
    bIsAimingState = false;
}

void ATDSCharacter::BeginPlay()
{
    Super::BeginPlay();
}

void ATDSCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

UTDSCharacterMovementComponent* ATDSCharacter::GetTDSMovementComponent() const
{
    return Cast<UTDSCharacterMovementComponent>(GetCharacterMovement());
}

void ATDSCharacter::SetWalking(bool NewWalk, bool bClientSimulation)
{
    if (GetTDSMovementComponent())
    {
        GetTDSMovementComponent()->WalkState = NewWalk;
    }
    bIsWalkingState = NewWalk;
}
void ATDSCharacter::SetSprinting(bool NewSprint, bool bClientSimulation)
{
    if (GetTDSMovementComponent())
    {
        GetTDSMovementComponent()->SprintState = NewSprint;
    }
  bIsSprintingState = NewSprint;   
}
void ATDSCharacter::SetStrafing(bool NewStrafe, bool bClientSimulation)
{
    if (GetTDSMovementComponent())
    {
        GetTDSMovementComponent()->StrafeState = NewStrafe;
    }
    bIsStrafingState = NewStrafe;
}
void ATDSCharacter::SetAiming(bool NewAim, bool bClientSimulation)
{
    if (GetTDSMovementComponent())
    {
        GetTDSMovementComponent()->AimState = NewAim;
    }
    bIsAimingState = NewAim;
}

void ATDSCharacter::OnRep_IsWalkingState()
{
    if (GetTDSMovementComponent())
    {
        GetTDSMovementComponent()->SetWalking(bIsWalkingState, true);
        GetTDSMovementComponent()->bNetworkUpdateReceived = true;
    }
}

void ATDSCharacter::OnRep_IsSprintingState()
{
    if (GetTDSMovementComponent())
    {
        GetTDSMovementComponent()->SetSprinting(bIsSprintingState, true);
        GetTDSMovementComponent()->bNetworkUpdateReceived = true;
    }
}

void ATDSCharacter::OnRep_IsStrafingState()
{
    if (GetTDSMovementComponent())
    {
        GetTDSMovementComponent()->SetStrafing(bIsStrafingState, true);
        GetTDSMovementComponent()->bNetworkUpdateReceived = true;
    }
}

void ATDSCharacter::OnRep_IsAimingState()
{
    if (GetTDSMovementComponent())
    {
        GetTDSMovementComponent()->SetAiming(bIsAimingState, true);
        GetTDSMovementComponent()->bNetworkUpdateReceived = true;
    }
}

void ATDSCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Посылаем всем SimulatedProxy, но не отправляем обратно владелецу:
    DOREPLIFETIME_CONDITION(ATDSCharacter, bIsWalkingState,   COND_SkipOwner);
    DOREPLIFETIME_CONDITION(ATDSCharacter, bIsSprintingState, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(ATDSCharacter, bIsStrafingState,  COND_SkipOwner);
    DOREPLIFETIME_CONDITION(ATDSCharacter, bIsAimingState,    COND_SkipOwner);
}