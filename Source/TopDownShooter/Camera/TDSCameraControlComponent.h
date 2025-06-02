// Copyright 2025, CRAFTCODE, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TDSCameraControlComponent.generated.h"

USTRUCT(BlueprintType)
struct FCameraOffsetSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MaxOffset = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float OffsetMultiplier = 1.0f;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TOPDOWNSHOOTER_API UTDSCameraControlComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTDSCameraControlComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Радиус круга, в котором не двигается камера */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float CircleRadius = 150.0f;

    /** Параметры смещения камеры */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    FCameraOffsetSettings CameraOffsetUp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    FCameraOffsetSettings CameraOffsetDown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    FCameraOffsetSettings CameraOffsetLeft;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    FCameraOffsetSettings CameraOffsetRight;

    /** Смещение камеры при прыжке */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float CameraJumpLookAhead = 300.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Camera")
    FVector2D CameraOffset;

    /** Смещение камеры */
    UPROPERTY(BlueprintReadOnly, Category = "Camera")
    FVector CameraLocation;

    /** Флаг, находится ли курсор внутри круга */
    UPROPERTY(BlueprintReadOnly, Category = "Camera")
    bool bIsCursorInCircle = true;

    /** Флаг, находится ли персонаж в воздухе */
    UPROPERTY(BlueprintReadOnly, Category = "Camera")
    bool bIsCharacterInAir = false;

private:
    APlayerController* PlayerController;
    ACharacter* CharacterOwner;
    FVector2D ScreenCenter;
    int32 LastScreenWidth = 0;
    int32 LastScreenHeight = 0;

    void UpdateCameraOffset();
    void UpdateCameraLocation();
    void UpdateScreenSize();
    float GetScreenScaleFactor();
};