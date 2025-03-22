// Fill out your copyright notice in the Description page of Project Settings.


#include "AssetHelperFunctionLibrary.h"

#include "AssetToolsModule.h"
#include "UObject/SavePackage.h"


template <typename TDataAssetType>
TDataAssetType* UAssetHelperFunctionLibrary::CreateAndSaveAsset(const FString& PackageFolderPath, const FString& BaseName,
	TFunction<void(TDataAssetType*)> InitializeAsset, const FString& Prefix, const FString& Suffix, EObjectFlags Flags)
{
	const auto AssetName = Prefix + BaseName + Suffix;
	const auto UniqueName = GetNewAssetUniqueName(PackageFolderPath, AssetName);

	UPackage* NewPackage = CreatePackage(*UniqueName.UniquePackageName);
	TDataAssetType* Asset = NewObject<TDataAssetType>(NewPackage, 
		*UniqueName.UniqueAssetName,
		Flags);

	// call the intialization function
	InitializeAsset(Asset);

	MoveTransientAssetToPackage(Asset, UniqueName);

	return Asset;
}

void UAssetHelperFunctionLibrary::MoveTransientAssetToPackage(UObject* Asset, const FUniqueAssetPackageAndName AssetName)
{
    if (Asset->GetPackage() == GetTransientPackage())
    {
    	UPackage* NewPackage = CreatePackage(*AssetName.UniquePackageName);
    	// move and don't create redirectors
    	Asset->Rename(*AssetName.UniqueAssetName, NewPackage, REN_DontCreateRedirectors);
    	NewPackage->MarkPackageDirty();
    	SaveAsset(Asset);
    }
}

FString UAssetHelperFunctionLibrary::GetAssetFolderPath(const UObject* Asset)
{
	const UPackage* AssetOuterPackage = CastChecked<UPackage>(Asset->GetOuter());
	const FString AssetPackageName = AssetOuterPackage->GetName();
	const FString PackageFolderPath = FPackageName::GetLongPackagePath(AssetPackageName);
	return PackageFolderPath;
}

FUniqueAssetPackageAndName UAssetHelperFunctionLibrary::GetNewAssetUniqueName(const FString FolderPath, const FString Name)
{
	FString UniquePackageName;
	FString UniqueAssetNameOut;
	FUniqueAssetPackageAndName NameOut;
	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(
		FolderPath + TEXT("/") + Name, 
		TEXT(""), 
		NameOut.UniquePackageName,
		NameOut.UniqueAssetName);
	return NameOut;
}

bool UAssetHelperFunctionLibrary::MarkAssetDirty(UObject* Asset)
{
	const auto IsMarkedDirty = Asset->MarkPackageDirty();
	Asset->PostEditChange();
	return IsMarkedDirty;
}

void UAssetHelperFunctionLibrary::SaveAsset(UObject* Asset, const EObjectFlags Flags)
{
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = Flags;
	UPackage* SavePackage = Asset->GetPackage();
	if (!Asset->HasAllFlags(Flags))
	{
		Asset->SetFlags(Asset->GetFlags() | Flags);
	}
	const FString PackageFileName = FPackageName::LongPackageNameToFilename(SavePackage->GetPathName(), FPackageName::GetAssetPackageExtension());
	UPackage::SavePackage(
		SavePackage,
		Asset,
		*PackageFileName,
		SaveArgs);
}
