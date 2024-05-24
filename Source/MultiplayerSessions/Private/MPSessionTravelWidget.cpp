// Fill out your copyright notice in the Description page of Project Settings.


#include "MPSessionTravelWidget.h"

#include "BlueprintSessionResult.h"
#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSessionSettings.h"

DEFINE_LOG_CATEGORY(LogMPSessionTravelWidget);


void UMPSessionTravelWidget::MenuSetup(
	const int32 NumberPublicConnections
)
{
	NumPublicConnections = NumberPublicConnections;
	
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);

	if(!TryFocusWidgetAndShowMouse())
	{
		
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Menu: Could not focus widget and show mouse"));
	}

	if(!TrySetMultiplayerSessionsSubsystem())
	{
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Menu: Could not set MultiplayerSessionsSubsystem"));
	}
	
	if(!TryBindCallbacksToMultiplayerSessionsSubsystem())
	{
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Menu: Failed to bind callbacks to MultiplayerSessionsSubsystem"));
	}
}

void UMPSessionTravelWidget::CreateSession(
	const TSoftObjectPtr<UWorld> LobbyServerTravelMap,
	const TMap<FName, FString>& SessionSettings
	) 
{
	LobbyMapAsset = LobbyServerTravelMap;
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Failed to issue CreateSession, MultiplayerSessionsSubsystem is null"));
		return;
	}
	MultiplayerSessionsSubsystem->CreateSession(NumPublicConnections, SessionSettings);
}

void UMPSessionTravelWidget::FindSessions(const int32 MaxSearchResults) const 
{
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Failed to issue FindSessions, MultiplayerSessionsSubsystem is null"));
		return;
	}
	MultiplayerSessionsSubsystem->FindSessions(MaxSearchResults);
}

void UMPSessionTravelWidget::JoinSession(const FBPSessionResult& SearchResult)
{
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Failed to issue JoinSession, MultiplayerSessionsSubsystem is null"));
		return;
	}
	MultiplayerSessionsSubsystem->JoinSession(SearchResult.SearchResult);
}

bool UMPSessionTravelWidget::TryFocusWidgetAndShowMouse()
{
	bool bHasSuccessfullyFocusedWidgetAndShownMouse = false;
	if (const UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PlayerController->SetInputMode(InputMode);
			PlayerController->SetShowMouseCursor(true);
			bHasSuccessfullyFocusedWidgetAndShownMouse = true;
		}
		else
		{
			UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Menu: PlayerController is null"));
		}
	}
	else
	{
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Menu: World is null"));
	}
	return bHasSuccessfullyFocusedWidgetAndShownMouse;
}


bool UMPSessionTravelWidget::TrySetMultiplayerSessionsSubsystem()
{
	bool bHasSuccessfullySetMultiplayerSessionsSubsystem = false;
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		if(const UGameInstance* GameInstance = GetGameInstance())
		{
			MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
			if (MultiplayerSessionsSubsystem)
			{
				bHasSuccessfullySetMultiplayerSessionsSubsystem = true;
			}
			else
			{
				UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Menu: MultiplayerSessionsSubsystem is null"));
			}
		}
		else
		{
			UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Menu: GameInstance is null, could not get MultiplayerSessionsSubsystem"));
		}
	}

	return bHasSuccessfullySetMultiplayerSessionsSubsystem;
}

bool UMPSessionTravelWidget::TryBindCallbacksToMultiplayerSessionsSubsystem()
{
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		return false;
	}
	
	MultiplayerSessionsSubsystem->MultiplayerOnCreateSessionComplete.AddUObject(this, &ThisClass::OnCreateSessionComplete);
	MultiplayerSessionsSubsystem->MultiplayerOnFindSessionsComplete.AddUObject(this, &ThisClass::OnFindSessionsComplete);
	MultiplayerSessionsSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &ThisClass::OnJoinSessionComplete);
	// MultiplayerSessionsSubsystem->MultiplayerOnStartSessionComplete.AddUObject(this, &ThisClass::OnStartSessionComplete);
	// MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.AddUObject(this, &ThisClass::OnDestroySessionComplete);
	return true;
}

void UMPSessionTravelWidget::StartMultiplayerSession() const
{
	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->StartSession();
	}
	else
	{
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Menu: MultiplayerSessionsSubsystem is null"));
	}
}

void UMPSessionTravelWidget::OnCreateSessionComplete(bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Menu: Failed to create session"));
	}
	UE_LOG(LogMPSessionTravelWidget, Log, TEXT("Menu: Session Created successfully"));

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Menu: Failed to get World, cannot server travel to Lobby"));
		return;
	}
	
	const FString ServerTravelLobbyMapPath = GetServerTravelLobbyMapPath();
	UE_LOG(LogMPSessionTravelWidget, Log, TEXT("Menu: ServerTravelLobbyMapPath set to: %s"), *ServerTravelLobbyMapPath);
	
	if (World->ServerTravel(ServerTravelLobbyMapPath))
	{
		UE_LOG(LogMPSessionTravelWidget, Log, TEXT("Menu: Listen Server Travelled to LobbyMap"));
	}
	else
	{
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Menu: Listen Server Failed to travel to LobbyMap"));
	}
}

FString UMPSessionTravelWidget::GetServerTravelLobbyMapPath() const
{
	FString ServerTravelLobbyMapPath;
	if (!LobbyMapAsset.IsNull())
	{
		// CanonicalAssetPath is something like "/Game/ThirdPerson/Maps/LobbyMap.LobbyMap"
		const FString CanonicalAssetPath = LobbyMapAsset.ToString();
		// Remove the ".*" part and add "?listen" to the end
		if (
			const int32 DotPosition = CanonicalAssetPath.Find(".");
			DotPosition != INDEX_NONE
		)
		{
			ServerTravelLobbyMapPath = CanonicalAssetPath.Left(DotPosition) + "?listen";
		}
	}
	else
	{
		const FString DefaultLobbyMapPath { "/Game/ThirdPerson/Maps/LobbyMap?listen" };
		ServerTravelLobbyMapPath = DefaultLobbyMapPath;
	}
	return ServerTravelLobbyMapPath;
}

void UMPSessionTravelWidget::OnFindSessionsComplete(const TArray<FOnlineSessionSearchResult>& SearchResults, bool bWasSuccessful)
{
	TArray<FBPSessionResult> BlueprintSearchResults {};
	if (!bWasSuccessful)
	{
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("FindSessions Was unsuccesful"));
		OnSessionsFound(BlueprintSearchResults, bWasSuccessful);
		return;
	}
	
	UE_LOG(LogMPSessionTravelWidget, Warning, TEXT("%d Sessions Found"), SearchResults.Num());
	for (const FOnlineSessionSearchResult& SearchResult : SearchResults)
	{
		UE_LOG(LogMPSessionTravelWidget, Log, TEXT("Session Found:"));
		FString Id = SearchResult.GetSessionIdStr();
		UE_LOG(LogMPSessionTravelWidget, Log, TEXT("SessionId %s:"), *Id);
		FString Name = SearchResult.Session.OwningUserName;
		UE_LOG(LogMPSessionTravelWidget, Log, TEXT("OwningUserName %s:"), *Id);
		TMap<FName, FString> SessionSettings;
		UE_LOG(LogMPSessionTravelWidget, Log, TEXT("Session Settings:"));
		for (auto& Setting : SearchResult.Session.SessionSettings.Settings)
		{
			const FName SettingName = Setting.Key;
			FString SettingValue;
			Setting.Value.Data.GetValue(SettingValue);
			SessionSettings.Add(SettingName, SettingValue);
			UE_LOG(LogMPSessionTravelWidget, Log, TEXT("SettingName %s SettingValue %s"), *SettingName.ToString(), *SettingValue);
		}
		
		FBPSessionResult BPSearchResult;
		BPSearchResult.Id = Id;
		BPSearchResult.OwningUserName = Name;
		BPSearchResult.SessionSettings = SessionSettings;
		BPSearchResult.SearchResult = SearchResult;
		BlueprintSearchResults.Add(BPSearchResult);
	}
	OnSessionsFound(BlueprintSearchResults, bWasSuccessful);
}

void UMPSessionTravelWidget::OnJoinSessionComplete(const FName& SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("MultiplayerSessionsSubsystem is null"));
		return;
	}

	FString Address;
	if (!MultiplayerSessionsSubsystem->GetResolvedConnectString(SessionName, Address))
	{
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Failed to get resolved connect string"));
		return;
	}
	UE_LOG(LogMPSessionTravelWidget, Log, TEXT("Resolved connect string: %s"), *Address);
	

	if(!MultiplayerSessionsSubsystem->TryFirstLocalPlayerControllerClientTravel(Address))
	{
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Failed to travel to session"));
	}
	else
	{
		UE_LOG(LogMPSessionTravelWidget, Log, TEXT("Travel to session successful"));
	}
}

void UMPSessionTravelWidget::OnStartSessionComplete(bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Menu: Failed to start session"));
		return;
	}
	UE_LOG(LogMPSessionTravelWidget, Log, TEXT("Menu: Session started successfully"));

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Menu: Failed to get World, cannot server travel to session map"));
		return;
	}
	
	const FString ServerTravelLobbyMapPath = GetServerTravelSessionMapPath();
	UE_LOG(LogMPSessionTravelWidget, Log, TEXT("Menu: ServerTravelSessionMapPath set to: %s"), *ServerTravelLobbyMapPath);
	
	if (World->ServerTravel(ServerTravelLobbyMapPath))
	{
		UE_LOG(LogMPSessionTravelWidget, Log, TEXT("Menu: Listen Server Travelled to %s"), *ServerTravelLobbyMapPath);
	}
	else
	{
		UE_LOG(LogMPSessionTravelWidget, Error, TEXT("Menu: Listen Server Failed to travel to session map %s"), *ServerTravelLobbyMapPath);
	}
}

FString UMPSessionTravelWidget::GetServerTravelSessionMapPath() const
{
	FString ServerTravelSessionMapPath;
	if (!LobbyMapAsset.IsNull())
	{
		// CanonicalAssetPath is something like "/Game/ThirdPerson/Maps/LobbyMap.LobbyMap"
		const FString CanonicalAssetPath = LobbyMapAsset.ToString();
		// Remove the ".*" part and add "?listen" to the end
		if (
			const int32 DotPosition = CanonicalAssetPath.Find(".");
			DotPosition != INDEX_NONE
		)
		{
			ServerTravelSessionMapPath = CanonicalAssetPath.Left(DotPosition) + "?listen";
		}
	}
	else
	{
		const FString DefaultSessionsMapPath { "/MultiplayerSessions/Maps/LVL_MainLevelMultiplayerSessionsDemo?listen" };
		ServerTravelSessionMapPath = DefaultSessionsMapPath;
	}
	return ServerTravelSessionMapPath;
}

void UMPSessionTravelWidget::NativeDestruct()
{
	MenuTeardown();
	
	Super::NativeDestruct();
}

void UMPSessionTravelWidget::MenuTeardown()
{
	RemoveFromParent();
	if (const UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			PlayerController->SetInputMode( FInputModeGameOnly {} );
			PlayerController->SetShowMouseCursor(false);
		}
	}
}
