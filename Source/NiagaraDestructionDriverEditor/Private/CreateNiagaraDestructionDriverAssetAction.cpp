// Fill out your copyright notice in the Description page of Project Settings.


#include "CreateNiagaraDestructionDriverAssetAction.h"

// StaticMeshActions.cpp
#include "AssetToolsModule.h"
#include "NiagaraDestructionDriverGeometryCollectionFunctions.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Widgets/Notifications/SNotificationList.h"

FCreateNiagaraDestructionDriverAssetAction::FCreateNiagaraDestructionDriverAssetAction()
{
}

FText FCreateNiagaraDestructionDriverAssetAction::GetName() const
{
    // We're not replacing the built-in Static Mesh actions, just adding to them
    // This name won't actually be used since we're not registering a new asset type
    return FText::FromString("Geometry Collection");
}

FColor FCreateNiagaraDestructionDriverAssetAction::GetTypeColor() const
{
    // Use the same color as the built-in Static Mesh
    return FColor(192, 128, 128);
}

UClass* FCreateNiagaraDestructionDriverAssetAction::GetSupportedClass() const
{
    return UGeometryCollection::StaticClass();
}

uint32 FCreateNiagaraDestructionDriverAssetAction::GetCategories()
{
    return EAssetTypeCategories::Basic;
}

void FCreateNiagaraDestructionDriverAssetAction::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
    // Call parent implementation to keep default actions
    FAssetTypeActions_Base::GetActions(InObjects, MenuBuilder);

    TArray<TWeakObjectPtr<UGeometryCollection>> SelectedGeometryCollections;
    
    // We'll create a collection from the selection
    TArray<TWeakObjectPtr<UGeometryCollection>> UGeometryCollections;
    for (const auto Object : InObjects)
    {
        if (UGeometryCollection* GC = Cast<UGeometryCollection>(Object))
        {
            SelectedGeometryCollections.Add(TWeakObjectPtr<UGeometryCollection>(GC));
        }
    }

    // Add a custom menu section
    MenuBuilder.AddMenuSeparator();
    MenuBuilder.AddMenuEntry(
        FText::FromString("Create Niagara Destruction Driver Assets"),
        FText::FromString("Creates a data asset pointing to a new static mesh and initial bone locations texture for the selected geometry collection."),
        FSlateIcon(),
        FUIAction(
        FExecuteAction::CreateLambda([SelectedGeometryCollections]()
        {
            for (auto GC : SelectedGeometryCollections)
            {
                if (UGeometryCollection* GeometryCollection = GC.Get())
                {
                    UNiagaraDestructionDriverGeometryCollectionFunctions::GeometryCollectionToNiagaraDestructible(GeometryCollection);
                }
            }
            // Display a notification
            FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("Finished Generating Niagara Destruction Driver Assets"))));
            Info.ExpireDuration = 5.0f;
            FSlateNotificationManager::Get().AddNotification(Info);
        }),
        FCanExecuteAction::CreateLambda([SelectedGeometryCollections]() {
            return SelectedGeometryCollections.Num() > 0;
        })
        )
    );
}