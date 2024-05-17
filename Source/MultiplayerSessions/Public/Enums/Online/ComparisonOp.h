#pragma once

#include "CoreMinimal.h"
#include "ComparisonOp.generated.h"

UENUM(BlueprintType, Category="Multiplayer Sessions")
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
