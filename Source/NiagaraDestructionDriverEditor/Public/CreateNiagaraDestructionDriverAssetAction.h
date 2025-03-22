// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

/**
 * Edotr context menu action to process a Geometry Collection into Niagara Destruction Driver assets.
 * (visible when you right click an asset in the editor)
 */
class FCreateNiagaraDestructionDriverAssetAction : public FAssetTypeActions_Base
{
public:
	// Constructor
	FCreateNiagaraDestructionDriverAssetAction();

	// FAssetTypeActions_Base overrides
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return true; }
    
	// This is where we'll add our custom actions
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
};
