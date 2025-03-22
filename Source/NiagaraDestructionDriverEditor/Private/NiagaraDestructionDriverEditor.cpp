// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDestructionDriverEditor.h"

#include "AssetToolsModule.h"
#include "CreateNiagaraDestructionDriverAssetAction.h"
#include "IAssetTools.h"

#define LOCTEXT_NAMESPACE "FNiagaraDestructionDriverEditorModule"
DEFINE_LOG_CATEGORY(LogNiagaraDestructionDriverEditor);

void FNiagaraDestructionDriverEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	// Get the asset tools module
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    
	// Create and register additional actions for Static Mesh assets
	CustomActions = MakeShareable(new FCreateNiagaraDestructionDriverAssetAction());
	AssetTools.RegisterAssetTypeActions(CustomActions.ToSharedRef());
	CreatedAssetTypeActions.Add(CustomActions);
}

void FNiagaraDestructionDriverEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	
	// Unregister asset type actions
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
        
		for (TSharedPtr<FCreateNiagaraDestructionDriverAssetAction>& Action : CreatedAssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
		}
	}
    
	CreatedAssetTypeActions.Empty();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FNiagaraDestructionDriverEditorModule, NiagaraDestructionDriverEditor)