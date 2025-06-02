// Copyright 2025, CRAFTCODE, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TDSPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class TOPDOWNSHOOTER_API ATDSPlayerController : public APlayerController
{
	GENERATED_BODY()
	
private:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	void UpdateControlRotation();
};
