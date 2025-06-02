// Copyright 2025, CRAFTCODE, All Rights Reserved.

#include "TDSCameraControlComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h" // Для получения капсульного компонента

UTDSCameraControlComponent::UTDSCameraControlComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UTDSCameraControlComponent::BeginPlay()
{
    Super::BeginPlay();
    PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    CharacterOwner = Cast<ACharacter>(GetOwner());
    UpdateScreenSize();
}

void UTDSCameraControlComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateScreenSize();
    UpdateCameraOffset();
    UpdateCameraLocation();
}

void UTDSCameraControlComponent::UpdateScreenSize()
{
    if (!PlayerController) return;

    int32 ScreenWidth, ScreenHeight;
    PlayerController->GetViewportSize(ScreenWidth, ScreenHeight);

    if (ScreenWidth != LastScreenWidth || ScreenHeight != LastScreenHeight)
    {
        LastScreenWidth = ScreenWidth;
        LastScreenHeight = ScreenHeight;
    }
}

void UTDSCameraControlComponent::UpdateCameraOffset()
{
    if (!PlayerController || !CharacterOwner) return;

    // Если персонаж не локально контролируется и его CharacterMovement настроен на ориентацию по движению,
    // то камера должна оставаться в центре экрана.
    if (!CharacterOwner->IsLocallyControlled() &&
        CharacterOwner->GetCharacterMovement() &&
        CharacterOwner->GetCharacterMovement()->bOrientRotationToMovement)
    {
        CameraOffset = FVector2D::ZeroVector;
        return;
    }

    // Определяем, находится ли персонаж в воздухе.
    bIsCharacterInAir = CharacterOwner->GetCharacterMovement()->IsFalling();
    if (bIsCharacterInAir)
    {
        // При прыжке смещение будет вычислено отдельно в UpdateCameraLocation.
        return;
    }

    int32 ScreenWidth, ScreenHeight;
    PlayerController->GetViewportSize(ScreenWidth, ScreenHeight);
    ScreenCenter = FVector2D(ScreenWidth * 0.5f, ScreenHeight * 0.5f);
    float ScaledCircleRadius = CircleRadius * GetScreenScaleFactor();

    FVector2D SimulatedCursorPos;
    bool bGotCursorPos = false;

    if (CharacterOwner->IsLocallyControlled())
    {
        // Логика для локального игрока – вычисляем смещение по позиции курсора мыши.
        float MouseX, MouseY;
        if (!PlayerController->GetMousePosition(MouseX, MouseY)) return;
        // Инвертируем Y, чтобы координаты совпадали с системой экрана.
        FVector2D CursorPosition(MouseX, ScreenHeight - MouseY);
        float DistanceToCenter = FVector2D::Distance(CursorPosition, ScreenCenter);
        bIsCursorInCircle = DistanceToCenter <= ScaledCircleRadius;

        if (!bIsCursorInCircle)
        {
            FVector2D Direction = (CursorPosition - ScreenCenter).GetSafeNormal();
            float RawOffsetX = (DistanceToCenter - ScaledCircleRadius) * Direction.X;
            float RawOffsetY = (DistanceToCenter - ScaledCircleRadius) * Direction.Y;

            float ClampedOffsetX = FMath::Clamp(RawOffsetX, -CameraOffsetLeft.MaxOffset, CameraOffsetRight.MaxOffset);
            float ClampedOffsetY = FMath::Clamp(RawOffsetY, -CameraOffsetDown.MaxOffset, CameraOffsetUp.MaxOffset);

            CameraOffset = FVector2D(
                ClampedOffsetX * (ClampedOffsetX > 0 ? CameraOffsetRight.OffsetMultiplier : CameraOffsetLeft.OffsetMultiplier),
                ClampedOffsetY * (ClampedOffsetY > 0 ? CameraOffsetUp.OffsetMultiplier : CameraOffsetDown.OffsetMultiplier)
            );
        }
        else
        {
            CameraOffset = FVector2D::ZeroVector;
        }
        return;
    }
    else
    {
        // --- Логика для не локальных (remote) игроков – имитируем позицию курсора через GetBaseAimRotation ---
        // Используем центр Pawn как стартовую точку.
        FVector PawnLocation = CharacterOwner->GetActorLocation();

        // Получаем угол взгляда: если контроллер есть – используем его, иначе GetBaseAimRotation.
        FRotator AimRotation;
        if (CharacterOwner->GetController())
        {
            AimRotation = CharacterOwner->GetController()->GetControlRotation();
        }
        else
        {
            AimRotation = CharacterOwner->GetBaseAimRotation();
        }
        FVector AimDirection = AimRotation.Vector();

        // (При необходимости можно скорректировать Z, но в данном случае оставляем направление как есть.)
        float TraceDistance = 1000.0f;
        FVector TraceStart = PawnLocation;
        FVector TraceEnd = PawnLocation + (AimDirection * TraceDistance);

        // Выполняем трассировку, чтобы учесть препятствия.
        FHitResult Hit;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(CharacterOwner);
        if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
        {
            TraceEnd = Hit.ImpactPoint;
        }

        // Проецируем полученную точку на экран.
        if (!PlayerController->ProjectWorldLocationToScreen(TraceEnd, SimulatedCursorPos))
        {
            CameraOffset = FVector2D::ZeroVector;
            return;
        }
        // Приводим координаты к системе экрана (инвертируем Y).
        SimulatedCursorPos.Y = ScreenHeight - SimulatedCursorPos.Y;
        bGotCursorPos = true;
    }

    if (!bGotCursorPos)
    {
        CameraOffset = FVector2D::ZeroVector;
        return;
    }

    // Вычисляем расстояние от полученной (или имитированной) позиции до центра экрана.
    float DistanceToCenter = FVector2D::Distance(SimulatedCursorPos, ScreenCenter);
    bool bCursorWithinCircle = DistanceToCenter <= ScaledCircleRadius;

    if (!bCursorWithinCircle)
    {
        FVector2D Direction = (SimulatedCursorPos - ScreenCenter).GetSafeNormal();
        float RawOffsetX = (DistanceToCenter - ScaledCircleRadius) * Direction.X;
        float RawOffsetY = (DistanceToCenter - ScaledCircleRadius) * Direction.Y;

        float ClampedOffsetX = FMath::Clamp(RawOffsetX, -CameraOffsetLeft.MaxOffset, CameraOffsetRight.MaxOffset);
        float ClampedOffsetY = FMath::Clamp(RawOffsetY, -CameraOffsetDown.MaxOffset, CameraOffsetUp.MaxOffset);

        CameraOffset = FVector2D(
            ClampedOffsetX * (ClampedOffsetX > 0 ? CameraOffsetRight.OffsetMultiplier : CameraOffsetLeft.OffsetMultiplier),
            ClampedOffsetY * (ClampedOffsetY > 0 ? CameraOffsetUp.OffsetMultiplier : CameraOffsetDown.OffsetMultiplier)
        );
    }
    else
    {
        CameraOffset = FVector2D::ZeroVector;
    }
}






void UTDSCameraControlComponent::UpdateCameraLocation()
{
    if (!PlayerController || !CharacterOwner) return;

    if (bIsCharacterInAir)
    {
        CameraOffset = FVector2D::ZeroVector;

        // Обрабатываем смещение камеры при прыжке
        FRotator CameraRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
        FRotator YawRotation(0, CameraRotation.Yaw, 0); // Используем только Yaw

        FVector AdjustedVelocity = CharacterOwner->GetVelocity();
        AdjustedVelocity.Z = 0; // Исключаем Z

        FVector ForwardOffset = YawRotation.RotateVector(AdjustedVelocity.GetSafeNormal()) * -CameraJumpLookAhead;

        if (-ForwardOffset.Y < 0)
        {
            CameraLocation = FVector(
                -ForwardOffset.Y, // X <- Y (инверсия)
                ForwardOffset.X,  // Y <- X
                -ForwardOffset.Y  // Z <- Y (инверсия)
            );
        }
        else
        {
            CameraLocation = FVector::ZeroVector;
        }
    }
    else
    {
        // При нахождении персонажа на земле используем вычисленное CameraOffset
        CameraLocation = FVector(
            FMath::Clamp(CameraOffset.Y, -CameraOffsetLeft.MaxOffset, CameraOffsetRight.MaxOffset), // X
            FMath::Clamp(CameraOffset.X, -CameraOffsetDown.MaxOffset, CameraOffsetUp.MaxOffset),   // Y
            FMath::Clamp(CameraOffset.Y, -CameraOffsetDown.MaxOffset, CameraOffsetUp.MaxOffset)    // Z
        );
    }
}

float UTDSCameraControlComponent::GetScreenScaleFactor()
{
    if (!PlayerController) return 1.0f;

    int32 ScreenWidth, ScreenHeight;
    PlayerController->GetViewportSize(ScreenWidth, ScreenHeight);

    return FMath::Min(float(ScreenWidth) / 1920.0f, float(ScreenHeight) / 1080.0f);
}