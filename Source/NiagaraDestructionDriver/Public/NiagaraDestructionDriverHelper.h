// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NiagaraDestructionDriverHelper.generated.h"

/**
 * 
 */
UCLASS()
class NIAGARADESTRUCTIONDRIVER_API UNiagaraDestructionDriverHelper : public UObject
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void InitiateDestructionForce(const UObject* WorldContextObject, const FVector Location, const float Radius, const float Force);
};
