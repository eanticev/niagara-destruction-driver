// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FCreateNiagaraDestructionDriverAssetAction;

DECLARE_LOG_CATEGORY_EXTERN(LogNiagaraDestructionDriverEditor, Log, All);

class FNiagaraDestructionDriverEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
    
	// Store a reference to created asset type actions for cleanup
	TArray<TSharedPtr<FCreateNiagaraDestructionDriverAssetAction>> CreatedAssetTypeActions;
	// We'll store additional references for the actions we're extending
	TSharedPtr<class FCreateNiagaraDestructionDriverAssetAction> CustomActions;
};
