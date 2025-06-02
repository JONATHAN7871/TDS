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
     * Создать LAN-сессию с указанным уровнем.
     * Вызывается только на стороне хоста.
     */
    UFUNCTION(BlueprintCallable)
    void CreateLANSession(FString MapName);

    /**
     * Найти LAN-сессию и подключиться к ней.
     * Вызывается на стороне клиента.
     */
    UFUNCTION(BlueprintCallable)
    void FindAndJoinLANSession();

private:
    IOnlineSessionPtr SessionInterface;
    TSharedPtr<FOnlineSessionSearch> SessionSearch;

    // Колбэки сессии
    void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
    void OnFindSessionsComplete(bool bWasSuccessful);
    void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);

    // Внутренний метод для создания сессии
    void CreateLANSessionInternal(FString MapName);

    // Проверка наличия существующей сессии и её удаление перед созданием новой
    void DestroyExistingSessionAndCreateNew(FString MapName);

    // Сохранённое название карты для загрузки (должно задаваться на стороне хоста)
    FString SelectedMapName;
};
