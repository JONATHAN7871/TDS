#include "TDSGameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

// Определяем локальную константу для имени сессии
static const FName SESSION_NAME(TEXT("GameSession"));

UTDSGameInstance::UTDSGameInstance()
{
}

void UTDSGameInstance::Init()
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (!Subsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("Error: OnlineSubsystem not found!"));
        return;
    }

    SessionInterface = Subsystem->GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Error: SessionInterface is not initialized!"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("SessionInterface successfully initialized."));

    SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UTDSGameInstance::OnCreateSessionComplete);
    SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &UTDSGameInstance::OnFindSessionsComplete);
    SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &UTDSGameInstance::OnJoinSessionComplete);
    SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &UTDSGameInstance::OnDestroySessionComplete);
}

void UTDSGameInstance::CreateLANSession(FString MapName)
{
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Error: SessionInterface is not valid!"));
        return;
    }

    if (MapName.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Error: MapName is empty, cannot create session!"));
        return;
    }

    // Сохраняем название карты для последующей загрузки
    SelectedMapName = MapName;

    // Если сессия уже существует, удаляем её перед созданием новой
    DestroyExistingSessionAndCreateNew(MapName);
}

void UTDSGameInstance::DestroyExistingSessionAndCreateNew(FString MapName)
{
    // Проверяем наличие сессии с именем SESSION_NAME
    FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(SESSION_NAME);
    if (ExistingSession != nullptr)
    {
        UE_LOG(LogTemp, Log, TEXT("Session %s already exists. Destroying existing session..."), *SESSION_NAME.ToString());
        if (!SessionInterface->DestroySession(SESSION_NAME))
        {
            UE_LOG(LogTemp, Error, TEXT("Error: Failed to start session destruction process."));
        }
    }
    else
    {
        // Если сессия отсутствует, создаём новую сразу
        CreateLANSessionInternal(MapName);
    }
}

void UTDSGameInstance::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (bWasSuccessful)
    {
        UE_LOG(LogTemp, Log, TEXT("Existing session %s destroyed successfully. Creating new session."), *SessionName.ToString());
        CreateLANSessionInternal(SelectedMapName);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Error: Failed to destroy existing session %s. Aborting session creation."), *SessionName.ToString());
    }
}

void UTDSGameInstance::CreateLANSessionInternal(FString MapName)
{
    FOnlineSessionSettings SessionSettings;
    SessionSettings.bIsLANMatch = true;
    SessionSettings.NumPublicConnections = 5;
    SessionSettings.bShouldAdvertise = true;
    SessionSettings.bUsesPresence = false;

    if (!SessionInterface->CreateSession(0, SESSION_NAME, SessionSettings))
    {
        UE_LOG(LogTemp, Error, TEXT("Error: Failed to start session creation process."));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("LAN session creation started for map: %s"), *MapName);
    }
}

void UTDSGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (bWasSuccessful)
    {
        UE_LOG(LogTemp, Log, TEXT("LAN session %s successfully created"), *SessionName.ToString());

        // После успешного создания сессии открываем уровень в режиме listen.
        // Этот вызов выполняется только на стороне хоста.
        if (!SelectedMapName.IsEmpty())
        {
            UE_LOG(LogTemp, Log, TEXT("Opening level: %s"), *SelectedMapName);
            UGameplayStatics::OpenLevel(GetWorld(), FName(*SelectedMapName), true, "listen");
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Error: Map name is not set! Aborting travel."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Error: Failed to create LAN session %s"), *SessionName.ToString());
    }
}

void UTDSGameInstance::FindAndJoinLANSession()
{
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Error: SessionInterface is not valid!"));
        return;
    }

    // Создаём новый объект поиска сессий
    SessionSearch = MakeShareable(new FOnlineSessionSearch());
    if (!SessionSearch.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Error: Failed to create SessionSearch!"));
        return;
    }

    // Для поиска LAN-сессий обязательно устанавливаем данный флаг
    SessionSearch->bIsLanQuery = true;
    SessionSearch->MaxSearchResults = 10;

    UE_LOG(LogTemp, Log, TEXT("Starting LAN session search..."));
    SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
}

void UTDSGameInstance::OnFindSessionsComplete(bool bWasSuccessful)
{
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Error: SessionInterface is invalid!"));
        return;
    }

    if (!SessionSearch.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Error: SessionSearch is invalid!"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("LAN session search completed. Found: %d"), SessionSearch->SearchResults.Num());

    // Если найдены результаты – пытаемся подключиться к первой найденной сессии
    if (bWasSuccessful && SessionSearch->SearchResults.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("Attempting to join the first found session..."));

        if (!SessionSearch->SearchResults[0].IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("Error: Search result is invalid!"));
            return;
        }

        SessionInterface->JoinSession(0, SESSION_NAME, SessionSearch->SearchResults[0]);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No LAN sessions found."));
    }
}

void UTDSGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    if (Result == EOnJoinSessionCompleteResult::Success)
    {
        UE_LOG(LogTemp, Log, TEXT("Successfully joined session: %s"), *SessionName.ToString());

        FString ConnectInfo;
        if (SessionInterface->GetResolvedConnectString(SessionName, ConnectInfo))
        {
            UE_LOG(LogTemp, Log, TEXT("Connecting to server at: %s"), *ConnectInfo);

            APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
            if (!PlayerController)
            {
                UE_LOG(LogTemp, Error, TEXT("Error: PlayerController is nullptr!"));
                return;
            }

            // Клиент автоматически перемещается на сервер с использованием полученной строки подключения
            PlayerController->ClientTravel(ConnectInfo, ETravelType::TRAVEL_Absolute);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Error: Failed to retrieve connection string!"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Error: Failed to join session %s"), *SessionName.ToString());
    }
}
