// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "NiagaraDestructionDriverSettings.generated.h"

/*
 * Project settings for Niagara Chaos Destruction Driver Plugin
 */
UCLASS(Config=Game, defaultconfig, meta = (DisplayName="Niagara Chaos Destruction Driver"))
class NIAGARADESTRUCTIONDRIVER_API UNiagaraDestructionDriverSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:

	UNiagaraDestructionDriverSettings();

	/** The default material that has the relevant shader for niagara driven destructibles. */
	UPROPERTY(Config, EditDefaultsOnly, Category=Config, meta=(Categories="Niagara Destructible"))
	TSoftObjectPtr<UMaterial> DefaultMaterialForNiagaraDestructibles;

	/** The default material that has the relevant shader for niagara driven destructibles. */
	UPROPERTY(Config, EditDefaultsOnly, Category=Config, meta=(Categories="Niagara Destructible"))
	TSoftObjectPtr<UMaterial> DebugMaterialForNiagaraDestructibles;

	/** The default particle system for niagara driven destructibles. */
	UPROPERTY(Config, EditDefaultsOnly, Category=Config, meta=(Categories="Niagara Destructible"))
	TSoftObjectPtr<UNiagaraSystem> DefaultNiagaraParticleSystem;
};
