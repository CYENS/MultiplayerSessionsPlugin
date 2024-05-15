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
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnCreateSessionComplete, bool bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& SearchResults, bool bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnJoinSessionComplete, const FName& SessionName, EOnJoinSessionCompleteResult::Type Result);
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnStartSessionComplete, bool bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnDestroySessionComplete, bool bWasSuccessful);


struct FQuerySetting
{
	FString Value;
	EOnlineComparisonOp::Type ComparisonOp = EOnlineComparisonOp::Equals;
};

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
	void CreateSession(
		const int32 NumPublicConnections,
		const TMap<FName, FString>& SessionSettings = TMap<FName, FString> ()
	);
	void FindSessions(const int32 MaxSearchResults, const TMap<FName, FQuerySetting>& QuerySettings = TMap<FName, FQuerySetting> ());
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
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful) const ;

	bool IsSessionInterfaceInvalid() const;
	bool TryAsyncCreateSession(const TMap<FName, FString>& SessionSettings = TMap<FName, FString>());
	void SetupLastSessionSettings(const TMap<FName, FString>& ExtraSessionSettings);
	bool DestroyPreviousSessionIfExists(const int32 NumPublicConnections);
	bool TryAsyncFindSessions(int32 MaxSearchResults, const TMap<FName, FQuerySetting>& ExtraQuerySettings);
	void SetupLastSessionSearchOptions(const int32 MaxSearchResults, const TMap<FName, FQuerySetting>& ExtraQuerySettings);

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
};
