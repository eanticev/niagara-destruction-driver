// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NiagaraDestructionDriverGeometryCollectionFunctions.generated.h"


class UTexture2D;
class UStaticMesh;
class UNiagaraDestructionDriverDataAsset;
class UGeometryCollection;
struct FMeshDescription;
class UMaterialInterface;
class FGeometryCollection;

/**
 * Static function library to process Geometry Collections into Niagara Destructible assets.
 * For Niagara Destructibles, in principle we need to output at least:
 * - A Static Mesh with a custom UV channel where the geometry collection "bone index" that a vertex is bound to is written.
 * - A texture where RGB is the XYZ coordinate in local space of the initial bone positions of a geometry collection.
 */
UCLASS()
class NIAGARADESTRUCTIONDRIVEREDITOR_API UNiagaraDestructionDriverGeometryCollectionFunctions : public UObject
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable, Category = "Geometry Collection Processing")
	static UNiagaraDestructionDriverDataAsset* StaticMeshToNiagaraDestructible(UStaticMesh* StaticMesh);
	
	/**
	 * This does the heavy lifting of taking a Niagara destructible and generating all the assets nescessary for
	 * the Niagara Destruction Driver to work:
	 *   - A static mesh where the bone index of each geometry collection fragment is written to a custom UV per vertex
	 *   - A texture with BoneCount width that holds the initial bone location vector in RGB.
	 *   - Duplicated Material Instances for each of the static mesh materials with the "NDD_Enabled" static switch parameter set to TRUE.
	 *   - A Data Asset that has references to all of the created assets.
	 *   - A Blueprint subclassing `NiagaraDestructionDriverActor` that uses the created Data Asset.
	 *
	 *   Drop the generated blueprint into your level.
	 *   
	 * @brief Generates niagara destructible assets for a given geometry collection.
	 * @note This creates and save new uassets in the editor.
	 * @param GeometryCollectionIn the geometry collection we want to process into a niagara driven destructible
	 * @return the data asset that holds references to all the generated assets
	 */
	UFUNCTION(BlueprintCallable, Category = "Geometry Collection Processing")
	static UNiagaraDestructionDriverDataAsset* GeometryCollectionToNiagaraDestructible(UGeometryCollection* GeometryCollectionIn);

	
	/**
	 * @brief Generates static mesh where the bone index of each geometry collection fragment is written to a custom UV per vertex  for a given geometry collection.
	 * @note This DOES NOT create or save any assets in the editor.
	 * @param GeometryCollectionIn the geometry collection we want to process.
	 * @return the static mesh where the bone index of each geometry collection fragment is written to a custom UV per vertex 
	 */
	UFUNCTION(BlueprintCallable, Category = "Geometry Collection Processing")
	static UStaticMesh* GeometryCollectionToStaticMesh(UGeometryCollection* GeometryCollectionIn);

	/**
	 * Generates a texture of size [BONECOUNT,1] that holds the initial bone location vectors of
	 * the provided Geometry Collection in RGB of each pixel.
	 * 
	 * Since RGB can only be positive, this texture has a bitmask in the Alpha channel (A) that provides the "sign" (+/-)
	 * for each of the RGB components respectively.
	 * 
	 * @brief Generates a texture that holds the initial bone location vectors of the provided Geometry Collection.
	 * @note This DOES NOT create or save any assets in the editor.
	 * @param GeometryCollectionIn the geometry collection we want to process.
	 * @return the texture that holds the initial bone location vectors of the provided Geometry Collection in RGB of each pixel.
	 */
	UFUNCTION(BlueprintCallable, Category = "Geometry Collection Processing")
	static UTexture2D* CreateInitialBoneLocationsToTexture(UGeometryCollection* GeometryCollectionIn);

private:

	static TArray<FVector3f> GenerateGeometryCollectionFragmentCentroids(const FGeometryCollection* GeometryCollection);
	static TArray<UMaterialInterface*> BuildGeometryCollectionMaterials(UGeometryCollection* GeometryCollectionIn, bool bOddMaterialsAreInternal);
	static TArray<UMaterialInterface*> CreateNewInstancesOfMeshMaterials(UStaticMesh* StaticMesh);
	static void GeometryCollectionToMeshDescription(UGeometryCollection* GeometryCollectionIn, FMeshDescription& MeshOut, TFunction<int32(int32, bool)> RemapMaterialIDs);
};
