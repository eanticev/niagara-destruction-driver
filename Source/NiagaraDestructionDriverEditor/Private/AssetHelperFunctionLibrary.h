// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AssetHelperFunctionLibrary.generated.h"

USTRUCT()
struct FUniqueAssetPackageAndName
{
	GENERATED_BODY()
	UPROPERTY() FString UniqueAssetName;
	UPROPERTY() FString UniquePackageName;
};

/**
 * 
 */
UCLASS()
class NIAGARADESTRUCTIONDRIVEREDITOR_API UAssetHelperFunctionLibrary : public UObject
{
	GENERATED_BODY()
public:
	template<typename TDataAssetType>
	TDataAssetType* CreateAndSaveAsset(
		const FString& PackageFolderPath,
		const FString& BaseName,
		TFunction<void(TDataAssetType*)> InitializeAsset = nullptr,
		const FString& Prefix = TEXT(""),
		const FString& Suffix = TEXT("_NDD"),
		EObjectFlags Flags = RF_Public | RF_Standalone | RF_MarkAsNative);
	static void MoveTransientAssetToPackage(UObject* Asset, const FUniqueAssetPackageAndName AssetName);
	static FString GetAssetFolderPath(const UObject* Asset);
	static FUniqueAssetPackageAndName GetNewAssetUniqueName(const FString FolderPath, const FString Name);
	static bool MarkAssetDirty(UObject* Asset);
	static void SaveAsset(UObject* Asset, const EObjectFlags Flags = RF_Public | RF_Standalone);
};
