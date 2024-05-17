// Fill out your copyright notice in the Description page of Project Settings.


#include "Enums/Online/ComparisonOp.h"

EOnlineComparisonOp::Type ConvertToOnlineComparisonOp(const EComparisonOp ComparisonOp)
{
 	switch (ComparisonOp)
 	{
 	case EComparisonOp::Equals:
 		return EOnlineComparisonOp::Equals;
 	case EComparisonOp::NotEquals:
 		return EOnlineComparisonOp::NotEquals;
 	case EComparisonOp::GreaterThan:
 		return EOnlineComparisonOp::GreaterThan;
 	case EComparisonOp::GreaterThanEquals:
 		return EOnlineComparisonOp::GreaterThanEquals;
 	case EComparisonOp::LessThan:
 		return EOnlineComparisonOp::LessThan;
 	case EComparisonOp::LessThanEquals:
 		return EOnlineComparisonOp::LessThanEquals;
 	case EComparisonOp::Near:
 		return EOnlineComparisonOp::Near;
 	case EComparisonOp::In:
 		return EOnlineComparisonOp::In;
 	case EComparisonOp::NotIn:
 		return EOnlineComparisonOp::NotIn;
 	}
 	return EOnlineComparisonOp::Equals;
}

EComparisonOp ConvertToComparisonOp(const EOnlineComparisonOp::Type ComparisonOp)
{
	switch (ComparisonOp)
	{
	case EOnlineComparisonOp::Equals:
		return EComparisonOp::Equals;
	case EOnlineComparisonOp::NotEquals:
		return EComparisonOp::NotEquals;
	case EOnlineComparisonOp::GreaterThan:
		return EComparisonOp::GreaterThan;
	case EOnlineComparisonOp::GreaterThanEquals:
		return EComparisonOp::GreaterThanEquals;
	case EOnlineComparisonOp::LessThan:
		return EComparisonOp::LessThan;
	case EOnlineComparisonOp::LessThanEquals:
		return EComparisonOp::LessThanEquals;
	case EOnlineComparisonOp::Near:
		return EComparisonOp::Near;
	case EOnlineComparisonOp::In:
		return EComparisonOp::In;
	case EOnlineComparisonOp::NotIn:
		return EComparisonOp::NotIn;
	}
	return EComparisonOp::Equals; // Default case
}
