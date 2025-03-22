// Fill out your copyright notice in the Description page of Project Settings.


#include "NiagaraDestructionDriverGeometryCollectionFunctions.h"

#include "AssetHelperFunctionLibrary.h"
#include "NiagaraDestructionDriverEditor.h"

#include "BoxTypes.h"
#include "DynamicMeshEditor.h"
#include "DynamicMeshToMeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshAttributes.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollectionConversion.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMeshActor.h"
#include "PhysicsEngine/BodySetup.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "FileHelpers.h"
#include "FractureModeSettings.h"
#include "IContentBrowserSingleton.h"
#include "NiagaraDestructionDriverActor.h"
#include "NiagaraDestructionDriverDataAsset.h"
#include "PlanarCut.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Factories/BlueprintFactory.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "GeometryCollection/GeometryCollectionFactory.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/SavePackage.h"
#include "Voronoi/Voronoi.h"

#define LOCTEXT_NAMESPACE "FNiagaraDestructionDriverEditorModule"

using namespace UE::Geometry;
using namespace Ck::GeometryCollectionConversion;

// <utility_functions>
#pragma region Utility Functions

void QuickSaveAssetRelativeTo(UObject* ObjectToSave, const UObject* RelativeToThis, const FString& Name, const FString Subfolder)
{
	const FString TargetSubfolder = Subfolder.IsEmpty() ? "" : TEXT("/") + Subfolder;
	const FString FolderPath = UAssetHelperFunctionLibrary::GetAssetFolderPath(RelativeToThis) + TargetSubfolder;
	const FUniqueAssetPackageAndName PackageAndName = UAssetHelperFunctionLibrary::GetNewAssetUniqueName(FolderPath, Name);
	UAssetHelperFunctionLibrary::MoveTransientAssetToPackage(ObjectToSave,PackageAndName);
	UAssetHelperFunctionLibrary::SaveAsset(ObjectToSave);
}

int32 NearestBiggerPowerOfTwo(int32 Reference)
{
	// If Reference is 0 or negative, return 2 as the smallest power of 2
	if (Reference <= 0)
	{
		return 2;
	}
    
	// Subtract 1 if Reference is already a power of 2
	Reference = (Reference & (Reference - 1)) ? Reference : Reference - 1;
    
	// Find the next power of 2
	Reference--;
	Reference |= Reference >> 1;
	Reference |= Reference >> 2;
	Reference |= Reference >> 4;
	Reference |= Reference >> 8;
	Reference |= Reference >> 16;
	Reference++;
    
	return Reference;
}

#pragma endregion 
// </utility_functions>

TArray<UMaterialInterface*> UNiagaraDestructionDriverGeometryCollectionFunctions::BuildGeometryCollectionMaterials(UGeometryCollection* GeometryCollectionIn, bool bOddMaterialsAreInternal)
{
	TArray<TObjectPtr<class UMaterialInterface>>& MainMaterials = GeometryCollectionIn->Materials;
	const auto Collection = GeometryCollectionIn->GetGeometryCollection();

	const int32 LastMaterialIdx = MainMaterials.Num() - 1;
	// if possible, we'd like to skip the last material -- it should be the selection material, which should be unused
	int32 SkipLastMaterialOffset = 1;
	if (LastMaterialIdx >= 0)
	{
		for (const int32 UsedMaterialID : Collection->MaterialID)
		{
			if (UsedMaterialID == LastMaterialIdx)
			{
				// last material is used in the mesh; we can't skip it
				SkipLastMaterialOffset = 0;
				break;
			}
		}
	}

	TArray<UMaterialInterface*> Materials;
	Materials.Reserve(MainMaterials.Num());
	for (int32 MatIdx = 0; MatIdx + SkipLastMaterialOffset < MainMaterials.Num(); MatIdx++)
	{
		Materials.Add(MainMaterials[MatIdx].Get());
	}

	// If we need odd materials as internal, naively duplicate materials
	// Note: We could do this in a smarter pattern based to minimize material slots, but at the cost of having a less predictable / more confusing result
	if (bOddMaterialsAreInternal)
	{
		const int32 RealMaterialNum = Materials.Num();
		Materials.SetNum(RealMaterialNum * 2);
		for (int32 MaterialIndex = RealMaterialNum - 1; MaterialIndex >= 0; --MaterialIndex)
		{
			Materials[MaterialIndex * 2 + 1] = Materials[MaterialIndex];
			Materials[MaterialIndex * 2] = Materials[MaterialIndex];
		}
	}

	return Materials;
}

TArray<UMaterialInterface*> UNiagaraDestructionDriverGeometryCollectionFunctions::CreateNewInstancesOfMeshMaterials(UStaticMesh* StaticMesh)
{
	const int32 NumMaterials = StaticMesh->GetStaticMaterials().Num();
	TArray<UMaterialInterface*> Materials;
	Materials.Reserve(NumMaterials);
	for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; MaterialIndex++)
	{
		if (UMaterialInterface* Material = StaticMesh->GetMaterial(MaterialIndex))
		{
			Materials.Add(Material);
		}
	}
	TMap<FString, UMaterialInstanceConstant*> CreatedMaterialInstanceAssets;
	TArray<UMaterialInterface*> NewMaterialInstances;
	NewMaterialInstances.Reserve(Materials.Num());
	for (const auto Material : Materials)
	{
		FString MaterialName = Material->GetName() + TEXT("_NDD");
		if (CreatedMaterialInstanceAssets.Contains(Material->GetFullName()))
		{
			UMaterialInterface* ExistingMaterial = CreatedMaterialInstanceAssets[Material->GetFullName()];
			NewMaterialInstances.Add(ExistingMaterial);
		} else
		{
			auto NewMaterialInstance = NewObject<UMaterialInstanceConstant>(
				GetTransientPackage(),
				UMaterialInstanceConstant::StaticClass(),
				FName(MaterialName),
				RF_Standalone | RF_Public | RF_Transactional);
			NewMaterialInstance->SetParentEditorOnly(Material);
			NewMaterialInstance->SetStaticSwitchParameterValueEditorOnly(FName("NiagaraDestructionDriver_Enabled"), true);
			CreatedMaterialInstanceAssets.Add(Material->GetFullName(), NewMaterialInstance);
			NewMaterialInstances.Add(NewMaterialInstance);
		}
	}

	return NewMaterialInstances;
}

UStaticMesh* UNiagaraDestructionDriverGeometryCollectionFunctions::GeometryCollectionToStaticMesh(UGeometryCollection* GeometryCollectionIn)
{
	check(GeometryCollectionIn);

	bool bPlaceInWorld = false;
	bool bSelectNewActors = false;
	bool bOddMaterialsAreInternal = true;

	auto CollectionToWorld = FTransform::Identity; 

	FScopedSlowTask ConvertTask(1, INVTEXT("Converting geometry to static mesh"));
	ConvertTask.MakeDialog();
	if (bSelectNewActors)
	{
		GEditor->SelectNone(false, true, false);
	}
	
	UStaticMesh* NewStaticMesh = NewObject<UStaticMesh>(
		GetTransientPackage(),
		UStaticMesh::StaticClass(),
		FName(TEXT("SM_") + GeometryCollectionIn->GetName() + TEXT("_CentroidUV_NDD")),
		RF_Standalone | RF_Public | RF_Transactional);

	// initialize the LOD 0 MeshDescription
	NewStaticMesh->SetNumSourceModels(1);
	// normals and tangents should carry over from the geometry collection
	NewStaticMesh->GetSourceModel(0).BuildSettings.bRecomputeNormals = false;
	NewStaticMesh->GetSourceModel(0).BuildSettings.bRecomputeTangents = false;
	NewStaticMesh->GetSourceModel(0).BuildSettings.bGenerateLightmapUVs = false;

	NewStaticMesh->ClearMeshDescriptions();
	UStaticMesh::RemoveUnusedMaterialSlots(NewStaticMesh);
	FMeshDescription* OutputMeshDescription = NewStaticMesh->CreateMeshDescription(0);

	NewStaticMesh->CreateBodySetup();
	NewStaticMesh->GetBodySetup()->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;

	TFunction<int32(int32, bool)> RemapMaterialIDs = nullptr;
	if (bOddMaterialsAreInternal)
	{
		RemapMaterialIDs = [](int32 InMatID, bool bIsInternal)
		{
			return InMatID * 2 + (int32)bIsInternal;
		};
	}

	FTransform BonesToCollection;
	GeometryCollectionToMeshDescription(GeometryCollectionIn, *OutputMeshDescription, RemapMaterialIDs);

	// take materials from origin geo collection to assign to new static mesh
	auto Materials = UNiagaraDestructionDriverGeometryCollectionFunctions::BuildGeometryCollectionMaterials(GeometryCollectionIn, bOddMaterialsAreInternal);

	// add a material slot. Must always have one material slot.
	int AddMaterialCount = FMath::Max(1, Materials.Num());
	for (int MatIdx = 0; MatIdx < AddMaterialCount; MatIdx++)
	{
		NewStaticMesh->GetStaticMaterials().Add(FStaticMaterial());
	}
	for (int MatIdx = 0; MatIdx < Materials.Num(); MatIdx++)
	{
		NewStaticMesh->SetMaterial(MatIdx, Materials[MatIdx]);
	}
	
	NewStaticMesh->CommitMeshDescription(0);

	NewStaticMesh->GetBodySetup()->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;


	if (bPlaceInWorld)
	{
		UWorld* TargetWorld = GEditor->GetEditorWorldContext().World();
		check(TargetWorld);

		// create new actor
		FRotator Rotation(0.0f, 0.0f, 0.0f);
		FActorSpawnParameters SpawnInfo;
		AStaticMeshActor* NewActor = TargetWorld->SpawnActor<AStaticMeshActor>(FVector::ZeroVector, Rotation, SpawnInfo);
		NewActor->SetActorLabel(*NewStaticMesh->GetName());
		NewActor->GetStaticMeshComponent()->SetStaticMesh(NewStaticMesh);

		// if we don't do this, world traces don't hit the mesh
		NewActor->GetStaticMeshComponent()->RecreatePhysicsState();

		NewActor->GetStaticMeshComponent()->SetWorldTransform(CollectionToWorld * BonesToCollection);

		for (int MatIdx = 0; MatIdx < Materials.Num(); MatIdx++)
		{
			NewActor->GetStaticMeshComponent()->SetMaterial(MatIdx, Materials[MatIdx]);
		}

		NewActor->MarkComponentsRenderStateDirty();

		if (bSelectNewActors)
		{
			GEditor->SelectActor(NewActor, true, false, true, false);
		}
	}

	if (bSelectNewActors)
	{
		GEditor->NoteSelectionChange(true);
	}

	return NewStaticMesh;
}

void UNiagaraDestructionDriverGeometryCollectionFunctions::GeometryCollectionToMeshDescription(UGeometryCollection* GeometryCollectionIn, FMeshDescription& MeshOut, TFunction<int32(int32, bool)> RemapMaterialIDs)
{
	bool bCenterPivot = false;
	bool bClearCustomAttributes = false;
	bool bWeldEdges = true;
	bool bComponentSpaceTransforms = false;
	bool bAllowInvisible = true;
	bool bSetPolygroupPerBone = false;

	const auto GeometryCollection = GeometryCollectionIn->GetGeometryCollection().Get();

	// how many geometries to skip in the collection (1 since L0 is )
	const int StartOffset = 1;
	
	// <Step1> 
	// code from GeometryCollectionMeshNodes.cpp:175
	const TManagedArray<FTransform3f>& BoneTransforms = GeometryCollection->GetAttribute<FTransform3f>("Transform", FGeometryCollection::TransformGroup);
	TArrayView<const FTransform3f> BoneTransformsAllArray = BoneTransforms.GetConstArray();
	TArrayView<const FTransform3f> BoneTransformsArray = BoneTransformsAllArray; // BoneTransformsAllArray.RightChop(StartOffset);

	TArray<int32> TransformIndices;
	TransformIndices.AddUninitialized(BoneTransformsArray.Num() - StartOffset);

	int32 Idx = StartOffset;
	for (int32& TransformIdx : TransformIndices)
	{
		TransformIdx = Idx++;
	}

	FStaticMeshAttributes Attributes(MeshOut);
	Attributes.Register();

	FTransform TransformOut;

	{ // ConvertToMeshDescription(MeshDescription, TransformOut, bCenterPivot, *GeometryCollection, BoneTransforms, TransformIndices);
		FDynamicMesh3 CombinedMesh;
		{ // ConvertToDynamicMeshTemplate<TransformType>(CombinedMesh, TransformOut, bCenterPivot, Collection, BoneTransforms.GetConstArray(), TransformIndices, RemapMaterialIDs, false);
			FTransform CellsToWorld = FTransform::Identity;
			TransformOut = FTransform::Identity;

			FDynamicMeshCollection MeshCollection;
			MeshCollection.bSkipInvisible = !bAllowInvisible;
			MeshCollection.bComponentSpaceTransforms = bComponentSpaceTransforms && !BoneTransformsArray.IsEmpty();

			// MAJOR STEP: initialize the mesh collection with the geometry collection and TransformIndicies corresponding to geometry we want to bake out into a mesh
			MeshCollection.Init(GeometryCollection, TransformIndices, CellsToWorld);

			// we add one UV layer that records the bone index in the U value as a normalized 0-1
			// this was already set in the FDynamicMeshCollection in InitTemplate
			const auto NumUVLayers = GeometryCollection->NumUVLayers()+1;
			SetGeometryCollectionAttributes(CombinedMesh, NumUVLayers);
			CombinedMesh.Attributes()->EnableTangents();
			if (bSetPolygroupPerBone)
			{
				CombinedMesh.EnableTriangleGroups();
			}

			int32 NumMeshes = MeshCollection.Meshes.Num();
			for (int32 MeshIdx = 0; MeshIdx < NumMeshes; MeshIdx++)
			{
				FDynamicMesh3& Mesh = MeshCollection.Meshes[MeshIdx].AugMesh;
				if (bSetPolygroupPerBone)
				{
					Mesh.EnableTriangleGroups();
				}
				const FTransform& FromCollection = MeshCollection.Meshes[MeshIdx].FromCollection;

				FMeshNormals::InitializeOverlayToPerVertexNormals(Mesh.Attributes()->PrimaryNormals(), true);
				AugmentedDynamicMesh::InitializeOverlayToPerVertexUVs(Mesh, NumUVLayers);
				AugmentedDynamicMesh::InitializeOverlayToPerVertexTangents(Mesh);

				if (RemapMaterialIDs)
				{
					FDynamicMeshMaterialAttribute* MaterialIDs = Mesh.Attributes()->GetMaterialID();
					for (int32 TID : Mesh.TriangleIndicesItr())
					{
						int32 MatID = MaterialIDs->GetValue(TID);
						bool bIsInternal = AugmentedDynamicMesh::GetInternal(Mesh, TID);
						int32 NewMatID = RemapMaterialIDs(MatID, bIsInternal);
						MaterialIDs->SetValue(TID, NewMatID);
					}
				}

				if (bWeldEdges && Mesh.TriangleCount() > 0)
				{
					FMergeCoincidentMeshEdges EdgeMerge(&Mesh);
					EdgeMerge.Apply();
				}

				if (MeshIdx > 0)
				{
					FDynamicMeshEditor MeshAppender(&CombinedMesh);
					FMeshIndexMappings IndexMaps_Unused;
					MeshAppender.AppendMesh(&Mesh, IndexMaps_Unused);
				}
				else
				{
					CombinedMesh = Mesh;
				}
			}

			if (bCenterPivot)
			{
				FAxisAlignedBox3d Bounds = CombinedMesh.GetBounds(true);
				FVector3d Translate = -Bounds.Center();
				MeshTransforms::Translate(CombinedMesh, Translate);
				TransformOut = FTransform((FVector)-Translate);
			}

			if (bClearCustomAttributes)
			{
				// <ClearCustomGeometryCollectionAttributes>
				// expanded from: ClearCustomGeometryCollectionAttributes(CombinedMesh);
				// copied from GeometryMeshConversion.cpp:1582
				
				CombinedMesh.DiscardVertexNormals();
				
				CombinedMesh.Attributes()->RemoveAttribute("ColorAttrib");
				CombinedMesh.Attributes()->RemoveAttribute("TangentUAttrib");
				CombinedMesh.Attributes()->RemoveAttribute("TangentVAttrib");
				CombinedMesh.Attributes()->RemoveAttribute("VisibleAttrib");
				CombinedMesh.Attributes()->RemoveAttribute("InternalAttrib");
				
				// copied from: GeometryMeshConversion.cpp:93
				// expanded from: `AugmentedDynamicMesh::EnableUVChannels(CombinedMesh, 0, false, true); // set 0 UV channels to remove UV attributes`
				enum
				{
					MAX_NUM_UV_CHANNELS = 8,
				};
				
				FName UVChannelNames[MAX_NUM_UV_CHANNELS] = {
					"UVAttrib0",
					"UVAttrib1",
					"UVAttrib2",
					"UVAttrib3",
					"UVAttrib4",
					"UVAttrib5",
					"UVAttrib6",
					"UVAttrib7"
				};
				
				for (int32 UVIdx = 0; UVIdx < MAX_NUM_UV_CHANNELS; UVIdx++)
				{
					CombinedMesh.Attributes()->RemoveAttribute(UVChannelNames[UVIdx]);
				}

				// </ClearCustomGeometryCollectionAttributes>
			}
		}

		FDynamicMeshToMeshDescription Converter;
		Converter.Convert(&CombinedMesh, MeshOut, true);	
	}
	
	// <Step1>
}

UNiagaraDestructionDriverDataAsset* UNiagaraDestructionDriverGeometryCollectionFunctions::StaticMeshToNiagaraDestructible(UStaticMesh* StaticMesh)
{
	int Seed = 999;
	int NumberVoronoiSitesMin = 5;
	int NumberVoronoiSitesMax = 10;
	
	//
	// // Create a new Geometry Collection asset
	// UGeometryCollection* GeometryCollectionObj = NewObject<UGeometryCollection>();
	// FGeometryCollection* GeometryCollection = GeometryCollectionObj->GetGeometryCollection().Get();
	//
	// TArray<TObjectPtr<UMaterialInterface>> Materials;
	// Algo::Transform(StaticMesh->GetStaticMaterials(), Materials, [](const FStaticMaterial& Mat)
	// {
	// 	return Mat.MaterialInterface;
	// });
	//
	// FGeometryCollectionSource StaticMeshSource;
	// StaticMeshSource.SourceGeometryObject = FSoftObjectPath(StaticMesh);
	// StaticMeshSource.LocalTransform = FTransform::Identity;
	// StaticMeshSource.SourceMaterial = Materials;
	// TArray<UMaterial*> MaterialsOut;
	// FGeometryCollectionEngineConversion::AppendGeometryCollectionSource(StaticMeshSource,*GeometryCollection,MaterialsOut);
	//
	// // 
	// QuickSaveAssetRelativeTo(GeometryCollectionObj, StaticMesh, "GC_" + StaticMesh->GetName(), "");






	
	// FTransform LastTransform = FTransform::Identity;
	// UGeometryCollection* NewGeometryCollection = static_cast<UGeometryCollection*>(NewObject<UGeometryCollection>(GetTransientPackage(), UGeometryCollection::StaticClass(), NAME_Name, RF_Transactional | RF_Public | RF_Standalone));
	// if (!NewGeometryCollection->SizeSpecificData.Num()) NewGeometryCollection->SizeSpecificData.Add(FGeometryCollectionSizeSpecificData());
	// FGeometryCollectionEngineConversion::AppendStaticMesh(StaticMesh, nullptr, LastTransform, NewGeometryCollection, false);
	// // De-duplicate materials and add selection material
	// NewGeometryCollection->InitializeMaterials(false);
	// GeometryCollectionAlgo::PrepareForSimulation(NewGeometryCollection->GetGeometryCollection().Get());
	// // Initial pivot : 
	// // Offset everything from the last selected element so the transform will align with the null space. 
	// if (TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Collection = NewGeometryCollection->GetGeometryCollection())
	// {
	// 	TManagedArray<int32> & Parent = Collection->Parent;
	// 	TManagedArray<FTransform3f>& Transform = Collection->Transform;
	//
	// 	for(int TransformGroupIndex =0; TransformGroupIndex<Collection->Transform.Num(); TransformGroupIndex++)
	// 	{
	// 		if (Parent[TransformGroupIndex] == FGeometryCollection::Invalid)
	// 		{
	// 			Transform[TransformGroupIndex] = Transform[TransformGroupIndex].GetRelativeTransform(FTransform3f(LastTransform));
	// 		}
	// 	}
	// }
	// NewGeometryCollection->Modify();
	// QuickSaveAssetRelativeTo(NewGeometryCollection, StaticMesh, "GC_" + StaticMesh->GetName(), "");
	// return nullptr;



	// <STEP_1> @see AGeometryCollectionActor* UFractureToolGenerateAsset::ConvertActorsToGeometryCollection
	//
	//
	FScopedSlowTask SlowTask(StaticMesh->GetNumTriangles(0), LOCTEXT("NDD_CreatingGeoCollection", "Creating new Geometry Collection"));
	SlowTask.MakeDialog();
	const bool bAddInternalMaterials = true;
	UGeometryCollection* FracturedGeometryCollection = static_cast<UGeometryCollection*>(NewObject<UGeometryCollection>(GetTransientPackage(), UGeometryCollection::StaticClass(), NAME_Name, RF_Transactional | RF_Public | RF_Standalone));;
	check(FracturedGeometryCollection);

	// If any of the static meshes have Nanite enabled, also enable on the new geometry collection asset for convenience.
	FracturedGeometryCollection->EnableNanite |= StaticMesh->IsNaniteEnabled();
	SlowTask.EnterProgressFrame(StaticMesh->GetNumTriangles(0));

	// Record the contributing source on the asset.
	FSoftObjectPath SourceSoftObjectPath(StaticMesh);
	TArray<TObjectPtr<UMaterialInterface>> SourceMaterials;
	Algo::Transform(StaticMesh->GetStaticMaterials(), SourceMaterials, [](const FStaticMaterial& Mat)
	{
		return Mat.MaterialInterface;
	});
	FracturedGeometryCollection->GeometrySource.Emplace(SourceSoftObjectPath, FTransform::Identity, SourceMaterials, false, false);
	FGeometryCollectionEngineConversion::AppendStaticMesh(StaticMesh, SourceMaterials, FTransform::Identity, FracturedGeometryCollection, false/*bReindexMaterials*/, bAddInternalMaterials, false, false);

	// init materials
	FracturedGeometryCollection->InitializeMaterials(bAddInternalMaterials);

	// add new root if one doesn't exist @see: UFractureToolGenerateAsset::AddSingleRootNodeIfRequired
	FGeometryCollection* Collection = FracturedGeometryCollection->GetGeometryCollection().Get();
	if (FGeometryCollectionClusteringUtility::ContainsMultipleRootBones(Collection))
	{
		FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(Collection);
	}

	FracturedGeometryCollection->InvalidateCollection();
	FracturedGeometryCollection->RebuildRenderData();

	// Add and initialize guids
	::GeometryCollection::GenerateTemporaryGuids(FracturedGeometryCollection->GetGeometryCollection().Get(), 0 , true);

	//
	const UFractureModeSettings* ModeSettings = GetDefault<UFractureModeSettings>();
	ModeSettings->ApplyDefaultSettings(*FracturedGeometryCollection->GetGeometryCollection());

	//
	FracturedGeometryCollection->CacheMaterialDensity();
	//
	//
	// </STEP_1>
	
	// <STEP_2> @see FVoronoiFractureOp
	//
	//
	// Make a selection of ALL bones
	const auto& TransformIndices = Collection->GetAttribute<int32>("TransformIndex", FGeometryCollection::GeometryGroup);
	auto AllSelectedTransformIndices = TransformIndices.GetConstArray();
	
	// GenerateVoronoiSites(FractureContext, Sites);
	TArray<FVector> Sites;
	FRandomStream RandStream(Seed);
	FBox Bounds = StaticMesh->GetBounds().GetBox();
	const FVector Extent(Bounds.Max - Bounds.Min);
	const int32 SiteCount = RandStream.RandRange(NumberVoronoiSitesMin, NumberVoronoiSitesMax);
	Sites.Reserve(Sites.Num() + SiteCount);
	for (int32 ii = 0; ii < SiteCount; ++ii)
	{
		Sites.Emplace(Bounds.Min + FVector(RandStream.FRand(), RandStream.FRand(), RandStream.FRand()) * Extent );
	}
	// end
	
	FBox VoronoiBounds = StaticMesh->GetBounds().GetBox(); 
	if (Sites.Num() > 0)
	{
		VoronoiBounds += FBox(Sites);
	}

	
	FTransform Transform = FTransform::Identity;
	FVector Origin = Transform.GetTranslation();
	// for (FVector& Site : Sites)
	// {
	// 	Site -= Origin;
	// }
	// Bounds.Min -= Origin;
	// Bounds.Max -= Origin;
	FVoronoiDiagram Voronoi(Sites, VoronoiBounds, .1f);
	FPlanarCells VoronoiPlanarCells = FPlanarCells(Sites, Voronoi);
	FNoiseSettings NoiseSettings;
	VoronoiPlanarCells.InternalSurfaceMaterials.NoiseSettings = NoiseSettings;
	FProgressCancel* Progress = nullptr;
	auto ResultGeometryIndex = CutMultipleWithPlanarCells(VoronoiPlanarCells, *Collection, AllSelectedTransformIndices, 0.f, 0.f, Seed, Transform, true, true, Progress, Origin);

	UE_LOG(LogNiagaraDestructionDriverEditor, Log, TEXT("New Starting Transform Index [%d]"), ResultGeometryIndex);
	
	// // Apply Internal Material ID to new fracture geometry.
	// // adapted from UFractureToolCutterBase::PostFractureProcess
	// int32 InternalMaterialID = CutterSettings->GetInternalMaterialID();
	// if (InternalMaterialID > INDEX_NONE)
	// {
	// 	FFractureEngineMaterials::SetMaterialOnGeometryAfter(Collection, FirstNewGeometryIndex, FFractureEngineMaterials::ETargetFaces::InternalFaces, InternalMaterialID);
	// 	Collection->ReindexMaterials();
	// }

	// @see: UFractureActionTool::Refresh
	FGeometryCollectionClusteringUtility::UpdateHierarchyLevelOfChildren(Collection, -1);
	FracturedGeometryCollection->RebuildRenderData();
	
	//
	//
	// </STEP_2>
	
	QuickSaveAssetRelativeTo(FracturedGeometryCollection, StaticMesh, "GC_" + StaticMesh->GetName(), "");
	return nullptr;
}

UNiagaraDestructionDriverDataAsset* UNiagaraDestructionDriverGeometryCollectionFunctions::GeometryCollectionToNiagaraDestructible(UGeometryCollection* GeometryCollectionIn)
{
	check(GeometryCollectionIn);
	
	const auto StaticMesh = GeometryCollectionToStaticMesh(GeometryCollectionIn);
	const auto InitialBoneLocationsTexture = CreateInitialBoneLocationsToTexture(GeometryCollectionIn);
	const auto BoneCount = GeometryCollectionIn->GetGeometryCollection().Get()->TransformIndex.Num();

	check(StaticMesh);
	check(InitialBoneLocationsTexture);

	// save the generated initial bone locations texture
	QuickSaveAssetRelativeTo(InitialBoneLocationsTexture, GeometryCollectionIn, "T_" + GeometryCollectionIn->GetName() + "_NDD", TEXT(""));

	// since we need to enable NDD material function params on the new materials instances, we want to create and save new ones for this generated static mesh
	// yes this is done after we've already done the material assignment once, but the benefit is that this block of code can be easily commented out and things still work
	// with the original materials
	auto NewMaterialInstances = UNiagaraDestructionDriverGeometryCollectionFunctions::CreateNewInstancesOfMeshMaterials(StaticMesh);
	for (int Idx = 0; Idx< NewMaterialInstances.Num(); Idx++)
	{
		StaticMesh->SetMaterial(Idx, NewMaterialInstances[Idx]);
		// Force material to invalidate its cache
		NewMaterialInstances[Idx]->PreEditChange(nullptr);
		// Trigger full recompilation
		NewMaterialInstances[Idx]->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
		// Notify the editor of changes
		NewMaterialInstances[Idx]->PostEditChange();
		// Optional: Refresh content browser
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets({ FAssetData(NewMaterialInstances[Idx]) });
		// If it's a material instance, also update the parent material
		if (const UMaterialInstanceConstant* MaterialInstance = Cast<UMaterialInstanceConstant>(NewMaterialInstances[Idx]))
		{
			if (UMaterialInterface* ParentMaterial = MaterialInstance->Parent)
			{
				// ParentMaterial->ForceRecompileForRendering();
			}
		}
		QuickSaveAssetRelativeTo(NewMaterialInstances[Idx], GeometryCollectionIn, NewMaterialInstances[Idx]->GetName(), TEXT(""));
	}
	// Additional steps to ensure editor updates
	FEditorDelegates::MapChange.Broadcast(0);
	// Save the generated Static Mesh in the editor
	QuickSaveAssetRelativeTo(StaticMesh, GeometryCollectionIn, StaticMesh->GetName(), TEXT(""));

	// create the NDD data asset
	UNiagaraDestructionDriverDataAsset* DataAsset = NewObject<UNiagaraDestructionDriverDataAsset>(
		GetTransientPackage(),
		UNiagaraDestructionDriverDataAsset::StaticClass(),
		FName("DA_" + GeometryCollectionIn->GetName() + "_NDD"));
	const int32 RenderTargetTextureSize = NearestBiggerPowerOfTwo(FMath::RoundToInt(FMath::Sqrt(static_cast<float>(BoneCount))));
	DataAsset->GeometryCollection = GeometryCollectionIn;
	DataAsset->StaticMesh = StaticMesh;
	DataAsset->InitialBoneLocationsTexture = InitialBoneLocationsTexture;
	DataAsset->CustomUVChannelIndex = 1;
	DataAsset->RenderTargetTextureSize = RenderTargetTextureSize;
	// Save the DataAsset in the editor
	QuickSaveAssetRelativeTo(DataAsset, GeometryCollectionIn, DataAsset->GetName(), TEXT(""));
	
	// create new package for Destruction Actor
	FString UniqueActorPackageName;
	FString UniqueActorAssetNameOut;
	const FString FolderPath = UAssetHelperFunctionLibrary::GetAssetFolderPath(GeometryCollectionIn);
	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(FolderPath + TEXT("/BP_") + GeometryCollectionIn->GetName() + TEXT("_NDD"), TEXT(""), UniqueActorPackageName, UniqueActorAssetNameOut);
	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = ANiagaraDestructionDriverActor::StaticClass();
	UObject* CreatedAsset = AssetToolsModule.Get().CreateAsset(UniqueActorAssetNameOut, FolderPath, Factory->SupportedClass, Factory);
	UBlueprint* Blueprint = Cast<UBlueprint>(CreatedAsset);
	if (Blueprint)
	{
		// Get the Blueprint's generated class
		UBlueprintGeneratedClass* BPGeneratedClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
		if (BPGeneratedClass)
		{
			// Get the class default object (CDO)
			ANiagaraDestructionDriverActor* CDO = Cast<ANiagaraDestructionDriverActor>(BPGeneratedClass->GetDefaultObject());
			if (CDO)
			{
				// Set the NDD data asset property directly on the CDO
				CDO->NiagaraDestructionDriverParams = DataAsset;

				// for each piece of the source geometry in the collection
				for (FGeometryCollectionSource& SourceGeometry : GeometryCollectionIn->GeometrySource)
				{
					// grab the static mesh and attach it as a component to the new actor
					// we do this so we can have a hot-swappable simple mesh that changes into the destructible mesh
					// at runtime when destruction forces happen
					if (UStaticMesh* SourceMesh = Cast<UStaticMesh>(SourceGeometry.SourceGeometryObject.TryLoad()))
					{
						/*
						UStaticMeshComponent* SourceMeshComp = NewObject<UStaticMeshComponent>(CDO, FName(SourceMesh->GetName()));
						SourceMeshComp->bAutoRegister = true;
						SourceMeshComp->SetStaticMesh(SourceMesh);
						SourceMeshComp->AttachToComponent(CDO->SourceGeometryContainer, FAttachmentTransformRules::KeepRelativeTransform);
						SourceMeshComp->SetRelativeTransform(SourceGeometry.LocalTransform);
						SourceMeshComp->CreationMethod = EComponentCreationMethod::SimpleConstructionScript;
						// CDO->FinishAndRegisterComponent(SourceMeshComp);
						// SourceMeshComp->RegisterComponent();
						// CDO->AddInstanceComponent(SourceMeshComp);
						*/
						
						USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
						USCS_Node* SCSNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), FName(SourceMesh->GetName()));
						UStaticMeshComponent* SourceMeshComp = Cast<UStaticMeshComponent>(SCSNode->ComponentTemplate);
						SourceMeshComp->CreationMethod = EComponentCreationMethod::Native;
						SCSNode->ParentComponentOrVariableName = CDO->SourceGeometryContainer->GetFName();
						SCSNode->bIsParentComponentNative = true;
						SCS->AddNode(SCSNode);
						
						SourceMeshComp->SetStaticMesh(SourceMesh);
						SourceMeshComp->AttachToComponent(CDO->SourceGeometryContainer, FAttachmentTransformRules::KeepRelativeTransform);
						SourceMeshComp->SetRelativeTransform(SourceGeometry.LocalTransform);
					}
				}
            
				// Force blueprint to be recompiled with new defaults
				FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
				FKismetEditorUtilities::CompileBlueprint(Blueprint);
			}
		}
    
		// Update and mark it as dirty
		Blueprint->MarkPackageDirty();
		Blueprint->PostEditChange();
	}
	// Save the package for Destruction Actor
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	const FString PackageFileName = FPackageName::LongPackageNameToFilename(Blueprint->GetPackage()->GetPathName(), FPackageName::GetAssetPackageExtension());
	UPackage::SavePackage(
		Blueprint->GetPackage(),
		Blueprint,
		*PackageFileName,
		SaveArgs);

	return DataAsset;
}

/*
// Create a new texture
UTexture2D* CreateRuntimeTexture(int32 Width, int32 Height)
{
    // Create the texture object
    UTexture2D* NewTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
    
    // Setup important properties
    NewTexture->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
    NewTexture->SRGB = false;
    NewTexture->AddToRoot(); // Prevent garbage collection
    NewTexture->Filter = TextureFilter::TF_Nearest;
    NewTexture->UpdateResource(); // Apply settings
    
    return NewTexture;
}
*/

TArray<FVector3f> UNiagaraDestructionDriverGeometryCollectionFunctions::GenerateGeometryCollectionFragmentCentroids(const FGeometryCollection* GeometryCollection)
{
	// Since the underlying geometry collection does not actually provide an accurate initial location of a bone
	// we will generate centroids based on the vertices of each of the fragments bound to a bone.

	// <unused>
	const TManagedArray<FTransform3f>& BoneTransforms = GeometryCollection->GetAttribute<FTransform3f>("Transform", FGeometryCollection::TransformGroup);
	const TManagedArray<FBox>& BoundingBoxes = GeometryCollection->GetAttribute<FBox>("BoundingBox", FGeometryCollection::GeometryGroup);
	// </unused>
	
	// <centroids>
	// calculate geometry centroids using the triangle center average methods
	const int32 NumGeometries = GeometryCollection->NumElements(FGeometryCollection::GeometryGroup);
	const TManagedArray<FVector3f>& Vertices = GeometryCollection->GetAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup);
	const TManagedArray<FIntVector>& TriangleVertexIndices = GeometryCollection->GetAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup);
	const TManagedArray<int32>& FaceStart = GeometryCollection->GetAttribute<int32>("FaceStart", FGeometryCollection::GeometryGroup);
	const TManagedArray<int32>& FaceCount = GeometryCollection->GetAttribute<int32>("FaceCount", FGeometryCollection::GeometryGroup);
	TArray<FVector3f> GeometryCentroids;
	const int StartOffset = 0; // 1 if skip L0 mesh
	GeometryCentroids.Reserve(NumGeometries-StartOffset);
	// we start at geometry[1] since geometry[0] is the whole mesh at destruction level 0
	// TODO: we need to handle levels here somehow
	for (int GeometryIndex=StartOffset; GeometryIndex<NumGeometries; GeometryIndex++) {
		const auto GeomFaceStartIndex = FaceStart[GeometryIndex];
		const auto GeomFaceCount = FaceCount[GeometryIndex];
		FVector3f Centroid = FVector3f::ZeroVector;
		float TotalArea = 0.0f;
		for (int32 Offset = 0; Offset < GeomFaceCount; ++Offset)
		{
			// face index
			const int32 i = GeomFaceStartIndex + Offset;
			
			// Get the triangle vertices
			const FVector3f& V1 = Vertices[TriangleVertexIndices[i][0]];
			const FVector3f& V2 = Vertices[TriangleVertexIndices[i][1]];
			const FVector3f& V3 = Vertices[TriangleVertexIndices[i][2]];

			// Calculate triangle centroid
			FVector3f TriangleCentroid = (V1 + V2 + V3) / 3.0f;
        
			// Calculate triangle area using cross product
			float Area = 0.5f * FVector3f::CrossProduct(V2 - V1, V3 - V1).Size();
        
			// Add weighted contribution
			Centroid += TriangleCentroid * Area;
			TotalArea += Area;
		}
		
		for (int i=GeomFaceStartIndex; i<GeomFaceStartIndex+GeomFaceCount; i++)
		{
			// Get the triangle vertices
			const FVector3f& V1 = Vertices[TriangleVertexIndices[i][0]];
			const FVector3f& V2 = Vertices[TriangleVertexIndices[i][1]];
			const FVector3f& V3 = Vertices[TriangleVertexIndices[i][2]];

			// Calculate triangle centroid
			FVector3f TriangleCentroid = (V1 + V2 + V3) / 3.0f;
        
			// Calculate triangle area using cross product
			float Area = 0.5f * FVector3f::CrossProduct(V2 - V1, V3 - V1).Size();
        
			// Add weighted contribution
			Centroid += TriangleCentroid * Area;
			TotalArea += Area;
		}
    
		// If we have valid area, calculate final centroid
		if (TotalArea > SMALL_NUMBER)
		{
			Centroid /= TotalArea;
		}
		GeometryCentroids.Add(Centroid);
	};
	// </centroids>
	
	// <normalize>
	// normalize geometry centroids (bone positions) to the bounds of object
	const auto DestructibleBounds = GeometryCollection->GetBoundingBox();
	const auto DestructibleBox = DestructibleBounds.GetBox();
	const auto Extents = DestructibleBox.GetExtent();
	const FVector BoundsMin = DestructibleBox.Min;
	const FVector BoundsMax = DestructibleBox.Max;
	const FVector BoundsSize = BoundsMax - BoundsMin;
	const FVector SafeBoundsSize(
		FMath::IsNearlyZero(BoundsSize.X) ? 1.0f : BoundsSize.X,
		FMath::IsNearlyZero(BoundsSize.Y) ? 1.0f : BoundsSize.Y,
		FMath::IsNearlyZero(BoundsSize.Z) ? 1.0f : BoundsSize.Z
	);
	ParallelFor(GeometryCentroids.Num(), [&](int32 Index)
	{
		// Convert FVector3f to FVector for calculations
		FVector Position(GeometryCentroids[Index].X, GeometryCentroids[Index].Y, GeometryCentroids[Index].Z);

		// Normalize to 0-1 range
		const FVector NormalizedPosZeroToOne = (Position - BoundsMin) / SafeBoundsSize;
		const FVector NormalizedPosWithNegatives = Position / Extents;

		const auto NormalizedPos = NormalizedPosWithNegatives;

		// Convert back to FVector3f and update the array
		GeometryCentroids[Index] = FVector3f(NormalizedPos.X, NormalizedPos.Y, NormalizedPos.Z);
	});
	// </normalize>

	return GeometryCentroids;
}

UTexture2D* UNiagaraDestructionDriverGeometryCollectionFunctions::CreateInitialBoneLocationsToTexture(UGeometryCollection* GeometryCollectionIn)
{
	const auto GeometryCollection = GeometryCollectionIn->GetGeometryCollection().Get();

	TArray<FVector3f> GeometryCentroids = GenerateGeometryCollectionFragmentCentroids(GeometryCollection);
	
    // Create a texture as wide as bone count
    const int32 TextureWidth = GeometryCentroids.Num();
	const int32 TextureHeight = 1;
	
	UTexture2D* NewTexture = NewObject<UTexture2D>();
	
	// Initialize the texture source data
	NewTexture->Source.Init(TextureWidth, TextureHeight, 1, 1, TSF_BGRA8);
    
	// Setup important properties
	NewTexture->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
	NewTexture->SRGB = false;
	NewTexture->Filter = TextureFilter::TF_Nearest;
	NewTexture->LODGroup = TEXTUREGROUP_World;
	NewTexture->MipGenSettings = TMGS_NoMipmaps;
	
	// Lock the source texture data for modification
	uint8* MipData = NewTexture->Source.LockMip(0);
    
	// Fill in pixel data
	FMemory::Memzero(MipData, TextureWidth * TextureHeight * 4);
	// ... (fill with your pixel data) ...
    
	// Example to fill with a pattern
	for (int32 y = 0; y < TextureHeight; y++)
	{
		for (int32 x = 0; x < TextureWidth; x++)
		{
			const int32 Index = (y * TextureWidth + x) * 4;
			const auto BonePosition = GeometryCentroids[x];
			
			const uint8 IsBonePositionXNegative = BonePosition.X < 0 ? 1 : 0; // 0x00000001
			const uint8 IsBonePositionYNegative = BonePosition.Y < 0 ? 2 : 0; // 0x00000010
			const uint8 IsBonePositionZNegative = BonePosition.Z < 0 ? 4 : 0; // 0x00000100
			const uint8 SignFlags = IsBonePositionXNegative | IsBonePositionYNegative | IsBonePositionZNegative;
			
			MipData[Index + 0] = FMath::Abs(BonePosition.Z) * 255;                        // B
			MipData[Index + 1] = FMath::Abs(BonePosition.Y) * 255;                        // G
			MipData[Index + 2] = FMath::Abs(BonePosition.X) * 255;                        // R
			MipData[Index + 3] = SignFlags;												  // A

			UE_LOG(LogNiagaraDestructionDriverEditor, Verbose, TEXT("Bone[%d] Position Vector: X=%f, Y=%f, Z=%f - Color:  R=%u, G=%u, B=%u, A=%u, Negatives:[%u, %u, %u]"),
					Index,
					BonePosition.X,
					BonePosition.Y,
					BonePosition.Z ,
					MipData[Index + 2],
					MipData[Index + 1],
					MipData[Index + 0],
					SignFlags,
					IsBonePositionXNegative,
					IsBonePositionYNegative,
					IsBonePositionZNegative
					);
		}
	}
    
	// Unlock the texture data
	NewTexture->Source.UnlockMip(0);

	// Update the texture and mark it as dirty
	NewTexture->UpdateResource();

	return NewTexture;
}
