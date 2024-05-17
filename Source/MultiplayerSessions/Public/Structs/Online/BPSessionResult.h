// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSessionSettings.h"
#include "BPSessionResult.generated.h"

USTRUCT(BlueprintType, Category="Multiplayer Sessions")
struct  MULTIPLAYERSESSIONS_API FBPSessionResult
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	FString ConnectionString;

	UPROPERTY(BlueprintReadOnly)
	int PingInMs;

	UPROPERTY(BlueprintReadOnly)
	FString OwningUserName;

	UPROPERTY(BlueprintReadOnly)
	FString Id;
	
	UPROPERTY(BlueprintReadOnly)
	TMap<FName, FString> SessionSettings;

	FOnlineSessionSearchResult SearchResult;
	
};