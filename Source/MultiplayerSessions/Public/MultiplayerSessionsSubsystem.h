// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"

#include "MultiplayerSessionsSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMultiplayerSessionsSubsystem, Log, All);

/**
 * Declaring our own custom delegates for the Menu class to bind callbacks to.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnCreateSessionComplete, bool, bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& SearchResults, bool bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnJoinSessionComplete, const FName& SessionName, EOnJoinSessionCompleteResult::Type Result);
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnStartSessionComplete, bool bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnDestroySessionComplete, bool bWasSuccessful);

/**
 * 
 */
UCLASS()
class MULTIPLAYERSESSIONS_API UMultiplayerSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UMultiplayerSessionsSubsystem();

	/**
	 * To handle session functionality
	 * The Menu class will call these.
	 */
	UFUNCTION(BlueprintCallable)
	void CreateSession(const int32 NumPublicConnections, FString MatchType);
	void FindSessions(const int32 MaxSearchResults);
	void JoinSession(const FOnlineSessionSearchResult& SearchResult);
	void DestroySession();
	bool StartSession();
	
	/**
	 * Our own custom delegates for the Menu class to bind callbacks to.
	 */
	FMultiplayerOnCreateSessionComplete MultiplayerOnCreateSessionComplete;
	FMultiplayerOnFindSessionsComplete MultiplayerOnFindSessionsComplete;
	FMultiplayerOnJoinSessionComplete MultiplayerOnJoinSessionComplete;
	FMultiplayerOnStartSessionComplete MultiplayerOnStartSessionComplete;
	FMultiplayerOnDestroySessionComplete MultiplayerOnDestroySessionComplete;

	/**
	 * Utility functions for the Menu class to use.
	 */
	bool GetResolvedConnectString(const FName& SessionName, FString& ConnectInfo) const;
	bool TryFirstLocalPlayerControllerClientTravel(const FString& Address);
	bool TryFirstLocalPlayerControllerClientTravel(const FName& SessionName);

protected:
	// Internal callbacks we'll bind to the Online Session Interface delegates
	// These don't need to be called outside of this class.
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);

	bool IsSessionInterfaceInvalid() const;
	bool TryAsyncCreateSession();
	void SetupLastSessionSettings();
	bool DestroyPreviousSessionIfExists(int32 NumPublicConnections, FString MatchType);
	bool TryAsyncFindSessions(int32 MaxSearchResults);
	void SetupLastSessionSearchOptions(int32 MaxSearchResults);

private:
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;

	/**
	 * To add to the Online Session Interface delegate list.
	 * We'll bind the MultiplayerSessionsSubsystem internal callback functions to these delegates.
	 */
	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate;
	FDelegateHandle DestroySessionCompleteDelegateHandle;
	FOnStartSessionCompleteDelegate StartSessionCompleteDelegate;
	FDelegateHandle StartSessionCompleteDelegateHandle;

	bool bCreateSessionOnDestroy { false };
	int32 LastNumPublicConnections { 4 };
	FString LastMatchType { "FreeForAll" };
};
