// Fill out your copyright notice in the Description page of Project Settings.

#include "AssetToolsModule.h"
#include "NiagaraDestructionDriverGeometryCollectionFunctions.h"
#include "GeometryCollection/GeometryCollectionObject.h"

#define LOCTEXT_NAMESPACE "FNiagaraDestructionDriverEditorModule"

namespace NiagaraDestructionDriver::MenuExtension_GeometryCollection
{
	static void ExecuteCreateNiagaraDestructionAssets(const FToolMenuContext& InContext)
	{
		if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext))
		{
			TArray<UGeometryCollection*> GeometryCollections = Context->LoadSelectedObjects<UGeometryCollection>();
			if (!GeometryCollections.IsEmpty())
			{
				for (UGeometryCollection* GeometryCollection : GeometryCollections)
				{
					if (GeometryCollection)
					{
						UNiagaraDestructionDriverGeometryCollectionFunctions::GeometryCollectionToNiagaraDestructible(GeometryCollection);
					}
				}
			}
		}
	}

	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, [] {
		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
				{
					FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
					UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UGeometryCollection::StaticClass());
					FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
					{
						const TAttribute<FText> Label = LOCTEXT("CreateNiagaraDestructionAssets_Label", "Create Niagara Destruction Driver Assets");
						const TAttribute<FText> ToolTip = LOCTEXT("CreateNiagaraDestructionAssets_Tooltip", "Creates a data asset pointing to a new static mesh and initial bone locations texture for the selected geometry collection.");
						const TAttribute<FSlateIcon> Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.ParticleSystemComponent");

						FToolUIAction UIAction;
						UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteCreateNiagaraDestructionAssets);
						Section.AddMenuEntry("GC_CreateNiagaraDestructionAssets", Label, ToolTip, Icon, UIAction);
					}
				}));
		});
}
#undef LOCTEXT_NAMESPACE
