// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiplayerSessionsSubsystem.h"

#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Online/OnlineSessionNames.h"

DEFINE_LOG_CATEGORY(LogMultiplayerSessionsSubsystem);

UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem():
	SessionInterface(nullptr),
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete)),
	DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete)),
	StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete))
{
	const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem == nullptr)
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("No Online Subsystem found"));
		return;
	}
	
	SessionInterface = Subsystem->GetSessionInterface();
}

void UMultiplayerSessionsSubsystem::CreateSession(
	const int32 NumPublicConnections,
	const TMap<FName, FString>& SessionSettings
)
{
	// if a session already exists, destroy it first, and return early, then it will be created on the OnDestroySessionComplete callback
	if (DestroyPreviousSessionIfExists(NumPublicConnections)) return;

	const bool bHasSuccessfullyIssuedAsyncCreateSession = TryAsyncCreateSession(SessionSettings);
	if(!bHasSuccessfullyIssuedAsyncCreateSession)
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("Failed to issue session creation"));
		MultiplayerOnCreateSessionComplete.Broadcast(false);
	}
}

/**
 * @return  True if a session was destroyed, false if no session was destroyed
 */
bool UMultiplayerSessionsSubsystem::DestroyPreviousSessionIfExists(const int32 NumPublicConnections)
{
	if (IsSessionInterfaceInvalid()) return false;
	
	if (
		const FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
		ExistingSession != nullptr
	)
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Warning, TEXT("Session already exists"));
		bCreateSessionOnDestroy = true;
		LastNumPublicConnections = NumPublicConnections;
		DestroySession();
		return true;
	}
	return false;
}

bool UMultiplayerSessionsSubsystem::IsSessionInterfaceInvalid() const
{
	if(!SessionInterface.IsValid())
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("SessionInterface is not valid"));
		return true;
	}
	return false;
}

bool UMultiplayerSessionsSubsystem::TryAsyncCreateSession(const TMap<FName, FString>& SessionSettings)
{
	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);
	
	SetupLastSessionSettings(SessionSettings);
	
	bool bHasSuccessfullyIssuedAsyncCreateSession = false;
	if (
		const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
		SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings)
	)
	{
		bHasSuccessfullyIssuedAsyncCreateSession = true;
	}
	else
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}
	return bHasSuccessfullyIssuedAsyncCreateSession;
}

void UMultiplayerSessionsSubsystem::SetupLastSessionSettings(const TMap<FName, FString>& ExtraSessionSettings)
{
	if (!LastSessionSettings.IsValid())
	{
		LastSessionSettings = MakeShareable(new FOnlineSessionSettings);
	}
	// if we're using the NULL subsystem, we're in a LAN match
	LastSessionSettings->bIsLANMatch =  IOnlineSubsystem::Get()->GetSubsystemName() == "NULL";
	LastSessionSettings->NumPublicConnections = 4;
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowJoinViaPresence = true;
	LastSessionSettings->bShouldAdvertise = true;
	LastSessionSettings->bUsesPresence = true;
	LastSessionSettings->bUseLobbiesIfAvailable = true;
	for (const auto& ExtraSessionSetting : ExtraSessionSettings)
	{
		const FName SettingName = ExtraSessionSetting.Key;
		const FString SettingValue = ExtraSessionSetting.Value;
		LastSessionSettings->Set(SettingName, SettingValue, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	}
}


bool UMultiplayerSessionsSubsystem::TryAsyncFindSessions(
	const int32 MaxSearchResults,
	const TMap<FName, FQuerySetting>& ExtraQuerySettings
)
{
	bool bHasSuccessfullyIssuedAsyncFindSessions = false;
	if (const UWorld* World = GetWorld())
	{
		SetupLastSessionSearchOptions(MaxSearchResults, ExtraQuerySettings);
		
		FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);
		
		if(
			const ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController();
			!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef())
		)
		{
			SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);

			// broadcast that we failed to find sessions
			const TArray<FOnlineSessionSearchResult> EmptySearchResults;
			MultiplayerOnFindSessionsComplete.Broadcast(EmptySearchResults, false);
			
			UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("SessionInterface->FindSessions failed"));
		}
		else
		{
			bHasSuccessfullyIssuedAsyncFindSessions = true;
		}
	}
	else
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("World is null"));
	}
	return bHasSuccessfullyIssuedAsyncFindSessions;
}

void UMultiplayerSessionsSubsystem::SetupLastSessionSearchOptions(
	const int32 MaxSearchResults,
	const TMap<FName, FQuerySetting>& ExtraQuerySettings
)
{
	LastSessionSearch = MakeShareable(new FOnlineSessionSearch);
	LastSessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL";
	LastSessionSearch->MaxSearchResults = MaxSearchResults;
	LastSessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
	for (const auto& QuerySetting : ExtraQuerySettings)
	{
		const FName QuerySettingName = QuerySetting.Key;
		const auto [Value, ComparisonOp] = QuerySetting.Value;
		LastSessionSearch->QuerySettings.Set(QuerySettingName, Value, ComparisonOp);
		UE_LOG(LogMultiplayerSessionsSubsystem, Warning, TEXT("%s %s %s"), *QuerySettingName.ToString(), *Value, ComparisonOp);
	}
}

void UMultiplayerSessionsSubsystem::FindSessions(
	const int32 MaxSearchResults,
	const TMap<FName, FQuerySetting>& QuerySettings
)
{
	if (IsSessionInterfaceInvalid()) return;

	if (!TryAsyncFindSessions(MaxSearchResults, QuerySettings))
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("FindSessions failed to issue"));
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
	else
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("FindSessions issued successfully"));
	}
}

void UMultiplayerSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SearchResult)
{
	if(!SessionInterface.IsValid())
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("SessionInterface is not valid"));
		MultiplayerOnJoinSessionComplete.Broadcast(NAME_GameSession, EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	bool bJoinSuccess; 
	if (const UWorld* World = GetWorld())
	{
		if (const ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController())
		{
			bJoinSuccess = SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SearchResult);
			if (!bJoinSuccess)
			{
				UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("MultiplayerSessionSubsystem: Failed to join session"));
			}
			else
			{
				UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("MultiplayerSessionSubsystem: Joining session %s"), *SearchResult.GetSessionIdStr());
			}
		}
		else
		{
			bJoinSuccess = false;
			UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("LocalPlayer is null"));
		}
	}
	else
	{
		bJoinSuccess = false;
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("World is null"));
	}
	if (!bJoinSuccess)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		MultiplayerOnJoinSessionComplete.Broadcast(NAME_GameSession, EOnJoinSessionCompleteResult::UnknownError);
	}
}

void UMultiplayerSessionsSubsystem::DestroySession()
{
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("During Destroy Session: SessionInterface is not valid"));
		return;
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);
	
	if(!SessionInterface->DestroySession(NAME_GameSession))
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("Failed to destroy session"));
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		MultiplayerOnDestroySessionComplete.Broadcast(false);
	}
	else
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("DestroySession issued successfully"));
	}
}

bool UMultiplayerSessionsSubsystem::StartSession()
{
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("SessionInterface is not valid"));
		return false;
	}

	StartSessionCompleteDelegateHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegate);
	
	const bool bSuccess = SessionInterface->StartSession(NAME_GameSession);
	if (bSuccess)
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("StartSession issued successfully"));
	}
	else
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("Failed to start session"));
	}
	if (!bSuccess)
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		MultiplayerOnStartSessionComplete.Broadcast(false);
	}
	return bSuccess;
}

void UMultiplayerSessionsSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("SessionInterface is not valid"));
		return;
	}
	if (bWasSuccessful)
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("Session %s has been created!"), *SessionName.ToString());
	}
	else
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("MultiplayerSessionSubsystem: Failed to create session"));
	}

	SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	MultiplayerOnCreateSessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("SessionInterface is not valid"));
		return;
	}

	SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);

	if (!bWasSuccessful)
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("Failed to find sessions"));
	}
	else
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("Found %d sessions"), LastSessionSearch->SearchResults.Num());
		for (const FOnlineSessionSearchResult& SearchResult : LastSessionSearch->SearchResults)
		{
			UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("Session found: %s"), *SearchResult.GetSessionIdStr());
		}
	}
	
	MultiplayerOnFindSessionsComplete.Broadcast(LastSessionSearch->SearchResults, bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("SessionInterface is not valid"));
		return;
	}

	SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	MultiplayerOnJoinSessionComplete.Broadcast(SessionName, Result);
}

void UMultiplayerSessionsSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("Failed to Destroy Session %s"), *SessionName.ToString());
		return;
	}
	UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("Successfuly destroyed Session %s"), *SessionName.ToString());
	
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("SessionInterface is not valid"));
		return;
	}

	SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	if (bWasSuccessful && bCreateSessionOnDestroy)
	{
		bCreateSessionOnDestroy = false;
		CreateSession(LastNumPublicConnections);
	}
	MultiplayerOnStartSessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful) const 
{
	if (bWasSuccessful)
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("Session %s has started"), *SessionName.ToString());
	}
	else
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("Failed to start session"));
	}

	MultiplayerOnStartSessionComplete.Broadcast(bWasSuccessful);
}


bool UMultiplayerSessionsSubsystem::GetResolvedConnectString(const FName& SessionName, FString& ConnectInfo) const
{
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("SessionInterface is not valid"));
		return false;
	}

	return SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectInfo);
}

bool UMultiplayerSessionsSubsystem::TryFirstLocalPlayerControllerClientTravel(const FString& Address)
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (APlayerController* PlayerController = GameInstance->GetFirstLocalPlayerController())
		{
			PlayerController->ClientTravel(Address, TRAVEL_Absolute);
			return true;
		}
	}
	return false;
}

bool UMultiplayerSessionsSubsystem::TryFirstLocalPlayerControllerClientTravel(const FName& SessionName)
{
	FString Address;
	if (GetResolvedConnectString(NAME_GameSession, Address))
	{
		return TryFirstLocalPlayerControllerClientTravel(Address);
	}
	return false;
}
