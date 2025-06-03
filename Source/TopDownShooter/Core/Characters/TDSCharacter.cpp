// TDSCharacter.cpp - Обновленная реализация

#include "TDSCharacter.h"
#include "TDSCharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"

ATDSCharacter::ATDSCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UTDSCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    // Инициализация состояний
    bIsWalkingState = false;
    bIsSprintingState = false;
    bIsStrafingState = false;
    bIsAimingState = false;

    // Настройки для мультиплеера
    bReplicates = true;
    SetReplicateMovement(true);
}

void ATDSCharacter::BeginPlay()
{
    Super::BeginPlay();
}

void ATDSCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ATDSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // Привязка основных действий движения
    PlayerInputComponent->BindAction("Walk", IE_Pressed, this, &ATDSCharacter::OnWalkPressed);
    PlayerInputComponent->BindAction("Walk", IE_Released, this, &ATDSCharacter::OnWalkReleased);
    
    PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &ATDSCharacter::OnSprintPressed);
    PlayerInputComponent->BindAction("Sprint", IE_Released, this, &ATDSCharacter::OnSprintReleased);
    
    PlayerInputComponent->BindAction("Strafe", IE_Pressed, this, &ATDSCharacter::OnStrafePressed);
    PlayerInputComponent->BindAction("Strafe", IE_Released, this, &ATDSCharacter::OnStrafeReleased);
    
    PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &ATDSCharacter::OnAimPressed);
    PlayerInputComponent->BindAction("Aim", IE_Released, this, &ATDSCharacter::OnAimReleased);

    // Привязка кастомных движений
    PlayerInputComponent->BindAction("WallRun", IE_Pressed, this, &ATDSCharacter::OnWallRunPressed);
    PlayerInputComponent->BindAction("WallRun", IE_Released, this, &ATDSCharacter::OnWallRunReleased);
    
    PlayerInputComponent->BindAction("Slide", IE_Pressed, this, &ATDSCharacter::OnSlidePressed);
    PlayerInputComponent->BindAction("Slide", IE_Released, this, &ATDSCharacter::OnSlideReleased);
    
    PlayerInputComponent->BindAction("Prone", IE_Pressed, this, &ATDSCharacter::OnPronePressed);
    PlayerInputComponent->BindAction("Prone", IE_Released, this, &ATDSCharacter::OnProneReleased);
}

UTDSCharacterMovementComponent* ATDSCharacter::GetTDSMovementComponent() const
{
    return Cast<UTDSCharacterMovementComponent>(GetCharacterMovement());
}

#pragma region Basic Movement States

void ATDSCharacter::SetWalking(bool NewWalk, bool bClientSimulation)
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        TDSMovement->SetWalking(NewWalk, bClientSimulation);
    }
    bIsWalkingState = NewWalk;
    
    // Уведомляем Blueprint о изменении состояния
    OnMovementStateChanged(bIsWalkingState, bIsSprintingState, bIsStrafingState, bIsAimingState);
}

void ATDSCharacter::SetSprinting(bool NewSprint, bool bClientSimulation)
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        TDSMovement->SetSprinting(NewSprint, bClientSimulation);
    }
    bIsSprintingState = NewSprint;
    
    OnMovementStateChanged(bIsWalkingState, bIsSprintingState, bIsStrafingState, bIsAimingState);
}

void ATDSCharacter::SetStrafing(bool NewStrafe, bool bClientSimulation)
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        TDSMovement->SetStrafing(NewStrafe, bClientSimulation);
    }
    bIsStrafingState = NewStrafe;
    
    OnMovementStateChanged(bIsWalkingState, bIsSprintingState, bIsStrafingState, bIsAimingState);
}

void ATDSCharacter::SetAiming(bool NewAim, bool bClientSimulation)
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        TDSMovement->SetAiming(NewAim, bClientSimulation);
    }
    bIsAimingState = NewAim;
    
    OnMovementStateChanged(bIsWalkingState, bIsSprintingState, bIsStrafingState, bIsAimingState);
}

#pragma endregion

#pragma region RepNotify Functions

void ATDSCharacter::OnRep_IsWalkingState()
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        TDSMovement->SetWalking(bIsWalkingState, true);
        TDSMovement->bNetworkUpdateReceived = true;
    }
    OnMovementStateChanged(bIsWalkingState, bIsSprintingState, bIsStrafingState, bIsAimingState);
}

void ATDSCharacter::OnRep_IsSprintingState()
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        TDSMovement->SetSprinting(bIsSprintingState, true);
        TDSMovement->bNetworkUpdateReceived = true;
    }
    OnMovementStateChanged(bIsWalkingState, bIsSprintingState, bIsStrafingState, bIsAimingState);
}

void ATDSCharacter::OnRep_IsStrafingState()
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        TDSMovement->SetStrafing(bIsStrafingState, true);
        TDSMovement->bNetworkUpdateReceived = true;
    }
    OnMovementStateChanged(bIsWalkingState, bIsSprintingState, bIsStrafingState, bIsAimingState);
}

void ATDSCharacter::OnRep_IsAimingState()
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        TDSMovement->SetAiming(bIsAimingState, true);
        TDSMovement->bNetworkUpdateReceived = true;
    }
    OnMovementStateChanged(bIsWalkingState, bIsSprintingState, bIsStrafingState, bIsAimingState);
}

#pragma endregion

#pragma region Custom Movement Controls

void ATDSCharacter::StartWallRun()
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        if (TDSMovement->BeginWallRun())
        {
            OnCustomMovementStarted(ETDSCustomMovementMode::CMOVE_WallRunning);
        }
    }
}

void ATDSCharacter::StopWallRun()
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        if (TDSMovement->IsCustomMovementMode(ETDSCustomMovementMode::CMOVE_WallRunning))
        {
            TDSMovement->EndWallRun();
            OnCustomMovementEnded(ETDSCustomMovementMode::CMOVE_WallRunning);
        }
    }
}

void ATDSCharacter::StartSlide()
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        if (TDSMovement->BeginSlide())
        {
            OnCustomMovementStarted(ETDSCustomMovementMode::CMOVE_Sliding);
        }
    }
}

void ATDSCharacter::StopSlide()
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        if (TDSMovement->IsCustomMovementMode(ETDSCustomMovementMode::CMOVE_Sliding))
        {
            TDSMovement->EndSlide();
            OnCustomMovementEnded(ETDSCustomMovementMode::CMOVE_Sliding);
        }
    }
}

void ATDSCharacter::StartProne()
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        if (TDSMovement->BeginProne())
        {
            OnCustomMovementStarted(ETDSCustomMovementMode::CMOVE_Prone);
        }
    }
}

void ATDSCharacter::StopProne()
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        if (TDSMovement->IsCustomMovementMode(ETDSCustomMovementMode::CMOVE_Prone))
        {
            TDSMovement->EndProne();
            OnCustomMovementEnded(ETDSCustomMovementMode::CMOVE_Prone);
        }
    }
}

bool ATDSCharacter::IsWallRunning() const
{
    if (const UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        return TDSMovement->IsCustomMovementMode(ETDSCustomMovementMode::CMOVE_WallRunning);
    }
    return false;
}

bool ATDSCharacter::IsSliding() const
{
    if (const UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        return TDSMovement->IsCustomMovementMode(ETDSCustomMovementMode::CMOVE_Sliding);
    }
    return false;
}

bool ATDSCharacter::IsInProne() const
{
    if (const UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        return TDSMovement->IsCustomMovementMode(ETDSCustomMovementMode::CMOVE_Prone);
    }
    return false;
}

ETDSCustomMovementMode ATDSCharacter::GetCurrentCustomMovementMode() const
{
    if (const UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        return TDSMovement->GetCurrentCustomMovementMode();
    }
    return ETDSCustomMovementMode::CMOVE_None;
}

#pragma endregion

#pragma region Input Handling

// Основные движения
void ATDSCharacter::OnWalkPressed()
{
    SetWalking(true);
}

void ATDSCharacter::OnWalkReleased()
{
    SetWalking(false);
}

void ATDSCharacter::OnSprintPressed()
{
    SetSprinting(true);
}

void ATDSCharacter::OnSprintReleased()
{
    SetSprinting(false);
}

void ATDSCharacter::OnStrafePressed()
{
    SetStrafing(true);
}

void ATDSCharacter::OnStrafeReleased()
{
    SetStrafing(false);
}

void ATDSCharacter::OnAimPressed()
{
    SetAiming(true);
}

void ATDSCharacter::OnAimReleased()
{
    SetAiming(false);
}

// Кастомные движения
void ATDSCharacter::OnWallRunPressed()
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        TDSMovement->SetWallRunInput(true);
    }
}

void ATDSCharacter::OnWallRunReleased()
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        TDSMovement->SetWallRunInput(false);
    }
}

void ATDSCharacter::OnSlidePressed()
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        TDSMovement->SetSlideInput(true);
    }
}

void ATDSCharacter::OnSlideReleased()
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        TDSMovement->SetSlideInput(false);
    }
}

void ATDSCharacter::OnPronePressed()
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        TDSMovement->SetProneInput(true);
    }
}

void ATDSCharacter::OnProneReleased()
{
    if (UTDSCharacterMovementComponent* TDSMovement = GetTDSMovementComponent())
    {
        TDSMovement->SetProneInput(false);
    }
}

#pragma endregion

void ATDSCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Реплицируем состояния всем, кроме владельца
    DOREPLIFETIME_CONDITION(ATDSCharacter, bIsWalkingState, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(ATDSCharacter, bIsSprintingState, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(ATDSCharacter, bIsStrafingState, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(ATDSCharacter, bIsAimingState, COND_SkipOwner);
}
