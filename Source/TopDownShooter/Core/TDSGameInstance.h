// Copyright 2025, CRAFTCODE, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "TDSGameInstance.generated.h"

UCLASS()
class TOPDOWNSHOOTER_API UTDSGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    UTDSGameInstance();

    virtual void Init() override;

    /**
     * ������� LAN-������ � ��������� �������.
     * ���������� ������ �� ������� �����.
     */
    UFUNCTION(BlueprintCallable)
    void CreateLANSession(FString MapName);

    /**
     * ����� LAN-������ � ������������ � ���.
     * ���������� �� ������� �������.
     */
    UFUNCTION(BlueprintCallable)
    void FindAndJoinLANSession();

private:
    IOnlineSessionPtr SessionInterface;
    TSharedPtr<FOnlineSessionSearch> SessionSearch;

    // ������� ������
    void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
    void OnFindSessionsComplete(bool bWasSuccessful);
    void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);

    // ���������� ����� ��� �������� ������
    void CreateLANSessionInternal(FString MapName);

    // �������� ������� ������������ ������ � � �������� ����� ��������� �����
    void DestroyExistingSessionAndCreateNew(FString MapName);

    // ���������� �������� ����� ��� �������� (������ ���������� �� ������� �����)
    FString SelectedMapName;
};
