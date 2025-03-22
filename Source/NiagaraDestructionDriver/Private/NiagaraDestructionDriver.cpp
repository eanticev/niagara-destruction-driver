// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDestructionDriver.h"

#define LOCTEXT_NAMESPACE "FNiagaraDestructionDriverModule"

DEFINE_LOG_CATEGORY(LogNiagaraDestructionDriver);

void FNiagaraDestructionDriverModule::StartupModule()
{
}

void FNiagaraDestructionDriverModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FNiagaraDestructionDriverModule, NiagaraDestructionDriver)