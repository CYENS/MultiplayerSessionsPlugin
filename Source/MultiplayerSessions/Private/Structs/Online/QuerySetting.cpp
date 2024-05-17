// Fill out your copyright notice in the Description page of Project Settings.


#include "Structs/Online/QuerySetting.h"

FQuerySetting::FQuerySetting(const FString& InValue, const EComparisonOp InComparisonOp)
	: Value(InValue),
	ComparisonOp(InComparisonOp)
{}

FQuerySetting::FQuerySetting(const TQuery<FString> QuerySetting)
	: FQuerySetting(ConvertToFQuerySetting(QuerySetting))
{
};

FQuerySetting ConvertToFQuerySetting(const TQuerySetting<FString>& QuerySetting)
{
	return FQuerySetting(QuerySetting.Value, ConvertToComparisonOp(QuerySetting.ComparisonOp));
}
