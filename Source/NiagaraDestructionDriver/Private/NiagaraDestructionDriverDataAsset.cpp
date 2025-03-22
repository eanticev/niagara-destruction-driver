// Fill out your copyright notice in the Description page of Project Settings.

#include "NiagaraDestructionDriverDataAsset.h"
#include "NiagaraDestructionDriverSettings.h"

UNiagaraDestructionDriverDataAsset::UNiagaraDestructionDriverDataAsset()
{
	if (const UNiagaraDestructionDriverSettings* Settings = GetDefault<UNiagaraDestructionDriverSettings>())
	{
		ParticleSystemDriver = Settings->DefaultNiagaraParticleSystem;
	}
}
