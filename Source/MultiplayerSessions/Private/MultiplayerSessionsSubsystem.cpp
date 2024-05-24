// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiplayerSessionsSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSubsystemTypes.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Online/OnlineSessionNames.h"

DEFINE_LOG_CATEGORY(LogMultiplayerSessionsSubsystem);

UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem():
	LoginCompleteDelegate(FOnLoginCompleteDelegate::CreateUObject(this, &ThisClass::OnLoginComplete)),
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete)),
	DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete)),
	StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete)),
	SessionInterface(nullptr),
	IdentityInterface(nullptr),
	IsLoggedIn(false)
{
	const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem == nullptr)
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("No Online Subsystem found"));
		return;
	}
	UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("Using Online Subsystem ( OSS ): '%s'"), *Subsystem->GetSubsystemName().ToString());

	SessionInterface = Subsystem->GetSessionInterface();
	IdentityInterface = Subsystem->GetIdentityInterface();
}

bool UMultiplayerSessionsSubsystem::TryAsyncLogin(const FPendingLoginAction& PendingLoginAction)
{
    /*
    Tutorial 2: This function will access the EOS OSS via the OSS identity interface to log first into Epic Account Services, and then into Epic Game Services.
    It will bind a delegate to handle the callback event once login call succeeeds or fails. 
    All functions that access the OSS will have this structure: 1-Get OSS interface, 2-Bind delegate for callback and 3-Call OSS interface function (which will call the correspongin EOS OSS function)
    */
    // If you're logged in, don't try to login again.
    // This can happen if your player travels to a dedicated server or different maps as BeginPlay() will be called each time.
	if (IsIdentityInterfaceInvalid())
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Warning, TEXT("Login failed. IdentityInterface is invalid."));
		return false;
	}
    
    if(const FUniqueNetIdPtr NetId = IdentityInterface->GetUniquePlayerId(0))
    {
		if(IdentityInterface->GetLoginStatus(0) == ELoginStatus::LoggedIn)
		{
			UE_LOG(LogMultiplayerSessionsSubsystem, Warning, TEXT("Login failed. Player already Logged In."));
			IsLoggedIn = true;
			PendingLoginAction.ExecuteIfBound();
			return false; 
		}
    }
    else
    {
    	UE_LOG(LogMultiplayerSessionsSubsystem, Warning, TEXT("Could not retrieve Logged In status. NetId is null."));
    }
	// These actions will be executed on successful Login
	PendingLoginActionsQueue.push(PendingLoginAction);
	
    
    /* This binds a delegate so we can run our function when the callback completes. 0 represents the player number.
    You should parametrize this Login function and pass the parameter here for splitscreen. 
    */
    LoginCompleteDelegateHandle = IdentityInterface->AddOnLoginCompleteDelegate_Handle(0, LoginCompleteDelegate);
 
    // Grab command line parameters. If empty call hardcoded login function - Hardcoded login function useful for Play In Editor. 
    FString AuthType; 
    FParse::Value(FCommandLine::Get(), TEXT("AUTH_TYPE="), AuthType);
 
    if (!AuthType.IsEmpty()) //If parameter is NOT empty we can autologin.
    {
        /* 
        In most situations you will want to automatically log a player in using the parameters passed via CLI.
        For example, using the exchange code for the Epic Games Store.
        */
        UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("Logging into EOS..."));
      
        if (!IdentityInterface->AutoLogin(0))
        {
            UE_LOG(LogMultiplayerSessionsSubsystem, Warning, TEXT("Failed to login. AutoLogin failed"));
			// Clear our handle and reset the delegate.
			IdentityInterface->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteDelegateHandle);
            LoginCompleteDelegateHandle.Reset();
        	return false;
        }
    }
    else
    {
        /* 
        Fallback if the CLI parameters are empty.Useful for PIE.
        The type here could be developer if using the DevAuthTool, ExchangeCode if the game is launched via the Epic Games Launcher, etc...
        */
        const FOnlineAccountCredentials Credentials("AccountPortal","", "");
 
        UE_LOG(LogTemp, Log, TEXT("Logging into EOS...")); // Log to the UE logs that we are trying to log in. 
        
        if (!IdentityInterface->Login(0, Credentials))
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to login. Login with Credentials failed "));
			// Clear our handle and reset the delegate. 
            IdentityInterface->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteDelegateHandle);
            LoginCompleteDelegateHandle.Reset();
        	return false;
        }        
    }
	return true;
}

void UMultiplayerSessionsSubsystem::CreateSession(
	const int32 NumPublicConnections,
	const TMap<FName, FString>& SessionSettings
)
{
	// if a session already exists, destroy it first, and return early, then it will be created on the OnDestroySessionComplete callback
	if (DestroyPreviousSessionIfExists(NumPublicConnections)) return;
	if (!IsLoggedIn)
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("User not logged in. Attempting to log in."));
		const bool bHasIssuedAsyncLogin =  
			TryAsyncLogin(FPendingLoginAction::CreateLambda(
				[this, NumPublicConnections, SessionSettings]()
					{
					CreateSession(NumPublicConnections, SessionSettings);
					}
				)
			);
		
		if(bHasIssuedAsyncLogin)
		{
			UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("Async login initiated. Will create session after login."));
			return;
		}
		
		if (!IsLoggedIn)
		{
			UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("Failed to issue session creation"));
			UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("Async login failed."));
			MultiplayerOnCreateSessionComplete.Broadcast(false);
			return;	
		}
		
	}

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

bool UMultiplayerSessionsSubsystem::IsIdentityInterfaceInvalid() const
{
	if(!IdentityInterface.IsValid())
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("IdentityInterface is not valid"));
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
	// if we're using the NULL subsystem, we're in a LAN match
	if (!LastSessionSettings.IsValid())
	{
		LastSessionSettings = MakeShareable(new FOnlineSessionSettings);
	}
	LastSessionSettings->bIsLANMatch =  IOnlineSubsystem::Get()->GetSubsystemName() == "NULL";
	LastSessionSettings->bIsDedicated = false;
	LastSessionSettings->NumPublicConnections = 4;
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowJoinViaPresence = true;
	LastSessionSettings->bShouldAdvertise = true;
	LastSessionSettings->bUsesPresence = true;
	LastSessionSettings->bUseLobbiesIfAvailable = false;
	 for (const auto& ExtraSessionSetting : ExtraSessionSettings)
	 {
	 	const FName SettingName = ExtraSessionSetting.Key;
		const FString SettingValue = ExtraSessionSetting.Value;
	 	LastSessionSettings->Set(SettingName, SettingValue, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	 }
}


bool UMultiplayerSessionsSubsystem::TryAsyncFindSessions(const int32 MaxSearchResults)
{
	bool bHasSuccessfullyIssuedAsyncFindSessions = false;
	if (const UWorld* World = GetWorld())
	{
		SetupLastSessionSearchOptions(MaxSearchResults);
		
		FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);
		if (LastSessionSearch->bIsLanQuery)
		{
			UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("Will perform Session Search in Lan"));
		}
		else
		{
			UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("Will perform Session Search in Web"));
		}
		
		UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("List of query search params (name, value):"));
		for (const auto& QuerySetting: LastSessionSearch->QuerySettings.SearchParams)
		{
			const FName SearchSettingName = QuerySetting.Key;
			const FOnlineSessionSearchParam SearchParam = QuerySetting.Value;
			UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("(%s: %s)"), *SearchSettingName.ToString(), *SearchParam.Data.ToString());
		}
		UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("****************************************"));
		
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

void UMultiplayerSessionsSubsystem::SetupLastSessionSearchOptions(const int32 MaxSearchResults)
{
	LastSessionSearch = MakeShareable(new FOnlineSessionSearch);
	LastSessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL";
	LastSessionSearch->MaxSearchResults = MaxSearchResults;
	LastSessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
}

void UMultiplayerSessionsSubsystem::ExecutePendingLoginActions()
{
	while (!PendingLoginActionsQueue.empty())
	{
		FPendingLoginAction PendingAction = PendingLoginActionsQueue.front();
		PendingLoginActionsQueue.pop();
		PendingAction.ExecuteIfBound();
	}
}

void UMultiplayerSessionsSubsystem::FindSessions(const int32 MaxSearchResults)
{
	if (IsSessionInterfaceInvalid()) return;
	
	if (!IsLoggedIn)
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("User not logged in. Attempting to log in."));
		const bool HasIssuedAsyncLogin =  
			TryAsyncLogin(FPendingLoginAction::CreateLambda(
				[this, MaxSearchResults]()
					{
						FindSessions(MaxSearchResults);
					}
				)
			);
		
		if(HasIssuedAsyncLogin)
		{
			UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("Async login initiated. Will find sessions after login."));
			return;
		}
		
		if (!HasIssuedAsyncLogin && !IsLoggedIn)
		{
			UE_LOG(LogMultiplayerSessionsSubsystem, Error, TEXT("Login Failed. Can't find sessions"));
			MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult> (),false);
			return;	
		}
	}
	

	if (!TryAsyncFindSessions(MaxSearchResults))
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

void UMultiplayerSessionsSubsystem::OnLoginComplete(
	int LocalUserNum,
	bool bWasSuccessful,
	const FUniqueNetId& UserId,
	const FString& Error
)
{
	/*
		This function handles the callback from logging in. You should not proceed with any EOS features until this function is called.
		This function will remove the delegate that was bound in the Login() function.
	*/
	IsLoggedIn = bWasSuccessful;
	if (bWasSuccessful)
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("Login success."));
		ExecutePendingLoginActions();
	}
	else
	{
		UE_LOG(LogMultiplayerSessionsSubsystem, Warning, TEXT("Login failed. Reason: '%s'"), *Error);
	}
	MultiplayerOnLoginComplete.Broadcast(LocalUserNum, bWasSuccessful, UserId, Error);

	if (!IsIdentityInterfaceInvalid())
	{
		IdentityInterface->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, LoginCompleteDelegateHandle);
		LoginCompleteDelegateHandle.Reset();
	}
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
	const auto NamedSession = SessionInterface->GetNamedSession(SessionName);
	UE_LOG(LogMultiplayerSessionsSubsystem, Log, TEXT("MultiplayerSessionSubsystem: Session ID %s"), *NamedSession->GetSessionIdStr());

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
