// Fill out your copyright notice in the Description page of Project Settings.


#include "NiagaraDestructionDriverSettings.h"

UNiagaraDestructionDriverSettings::UNiagaraDestructionDriverSettings()
{
	DefaultMaterialForNiagaraDestructibles = TSoftObjectPtr<UMaterial>(FSoftObjectPath(TEXT("/NiagaraDestructionDriver/M_VertexMeshSystem.M_VertexMeshSystem")));
	DebugMaterialForNiagaraDestructibles = TSoftObjectPtr<UMaterial>(FSoftObjectPath(TEXT("/NiagaraDestructionDriver/M_VertexMeshSystem.M_VertexMeshSystem")));
	DefaultNiagaraParticleSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/NiagaraDestructionDriver/PS_DestructibleRig.PS_DestructibleRig")));
}
