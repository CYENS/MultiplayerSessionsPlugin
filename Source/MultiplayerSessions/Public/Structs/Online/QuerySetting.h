// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemTypes.h"
#include "Enums/Online/ComparisonOp.h"
#include "QuerySetting.generated.h"

USTRUCT(BlueprintType, Category="Multiplayer Sessions")
struct  MULTIPLAYERSESSIONS_API FQuerySetting
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString Value;
	
	UPROPERTY(EditAnywhere)
	EComparisonOp ComparisonOp;

	explicit FQuerySetting(const FString& InValue = "", const EComparisonOp InComparisonOp = EComparisonOp::Equals);
	explicit FQuerySetting(const TQuery);
 	
};


template<typename ValueType>
struct  MULTIPLAYERSESSIONS_API TQuerySetting
{
	ValueType Value;
	
	EOnlineComparisonOp::Type ComparisonOp;
};

FQuerySetting ConvertToFQuerySetting(TQuerySetting<FString> QuerySetting);
