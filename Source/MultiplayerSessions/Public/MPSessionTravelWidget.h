// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Containers/Union.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "MPSessionTravelWidget.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMPSessionTravelWidget, Log, All);

class UMultiplayerSessionsSubsystem;

struct FBPSessionResult;

UENUM(BlueprintType)
enum class EComparisonOp: uint8
{
	Equals,
	NotEquals,
	GreaterThan,
	GreaterThanEquals,
	LessThan,
	LessThanEquals,
	Near,
	In,
	NotIn
};

USTRUCT(BlueprintType)
struct FBPQuerySetting
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SettingValue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EComparisonOp ComparisonOp;
	
};

UCLASS()
class MULTIPLAYERSESSIONS_API UMPSessionTravelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void MenuSetup(
		const int32 NumberPublicConnections = 4
	);

	TSoftObjectPtr<UWorld> LobbyMapAsset;
	TSoftObjectPtr<UWorld> SessionMapAsset;
	
	UFUNCTION(BlueprintCallable, Category="Multiplayer Sessions")
	void CreateSession(
		const TSoftObjectPtr<UWorld> LobbyServerTravelMap,
		const TMap<FName, FString>& SessionSettings
	);

	UFUNCTION(BlueprintCallable, Category="Μultiplayer Sessions")
	void FindSessions(
		const int32 MaxSearchResults,
		const TMap<FName, FBPQuerySetting> QuerySettings
	) const;
	UFUNCTION(BlueprintCallable, Category="Μultiplayer Sessions")
	void JoinSession(const FBPSessionResult& SearchResult);
	
	UFUNCTION(BlueprintImplementableEvent, Category="Multiplayer Sessions")
	void OnSessionsFound(const TArray<FBPSessionResult>& SearchResults, const bool bWasSuccessful);

protected:
	virtual void NativeDestruct() override;

	void OnCreateSessionComplete(bool bWasSuccessful);
	void OnFindSessionsComplete(const TArray<FOnlineSessionSearchResult>& SearchResults, bool bWasSuccessful);
	void OnJoinSessionComplete(const FName& SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnStartSessionComplete(bool bWasSuccessful);
	
	FString GetServerTravelLobbyMapPath() const;
	FString GetServerTravelSessionMapPath() const;
	
	// menu setup functions
	bool TryBindCallbacksToMultiplayerSessionsSubsystem();
	bool TrySetMultiplayerSessionsSubsystem();
	bool TryFocusWidgetAndShowMouse();

private:
	UPROPERTY()
	UMultiplayerSessionsSubsystem* MultiplayerSessionsSubsystem;

	UFUNCTION(BlueprintCallable, Category="MultiplayerSessions")
	void StartMultiplayerSession() const;

	static EOnlineComparisonOp::Type ConvertEComparisonOpToEOnlineComparisonOp(EComparisonOp ComparisonOp);
	void MenuTeardown();

	int32 NumPublicConnections { 4 };
};
