// Copyright 2025, CRAFTCODE, All Rights Reserved.

#include "TDSPlayerController.h"

void ATDSPlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void ATDSPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateControlRotation();
}

void ATDSPlayerController::UpdateControlRotation()
{
    FVector WorldLocation, WorldDirection;

    // 1. ����������� ���� �� ������ � ���
    if (DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
    {
        FHitResult CursorHit;
        FVector Start = WorldLocation;
        FVector End = Start + (WorldDirection * 10000.0f); // ����� �����������

        FCollisionQueryParams Params;
        Params.AddIgnoredActor(GetPawn()); // ���������� ����

        FVector TargetLocation;

        if (GetWorld()->LineTraceSingleByChannel(CursorHit, Start, End, ECC_Visibility, Params))
        {
            TargetLocation = CursorHit.ImpactPoint; // ������ ����� � �����
        }
        else
        {
            return; // ���� ��� ������������ � ������ �� ������
        }

        APawn* ControlledPawn = GetPawn();
        if (ControlledPawn)
        {
            FVector CharacterLocation = ControlledPawn->GetActorLocation();

            // 2. ������ Trace: ��������� ����������� ����� ���������� � ��������
            FHitResult ObstacleHit;
            FCollisionQueryParams ObstacleParams;
            ObstacleParams.AddIgnoredActor(ControlledPawn); // ���������� ����

            bool bHitObstacle = GetWorld()->LineTraceSingleByChannel(
                ObstacleHit,
                CharacterLocation,
                TargetLocation,
                ECC_Visibility,
                ObstacleParams
            );

            if (bHitObstacle) // ���� ���� ����������� � ������ TargetLocation
            {
                TargetLocation = ObstacleHit.ImpactPoint;
            }

            FVector Direction = TargetLocation - CharacterLocation;

            FRotator NewControlRotation = Direction.Rotation();
            NewControlRotation.Roll = 0; // ������� Roll

            SetControlRotation(NewControlRotation);

            // Debug ������������ �����������
            DrawDebugLine(GetWorld(), CharacterLocation, TargetLocation, FColor::Red, false, 0.1f, 0, 2.0f);
            DrawDebugPoint(GetWorld(), TargetLocation, 10.0f, FColor::Green, false, 0.1f);
        }
    }
}

