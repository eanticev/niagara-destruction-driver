#pragma once

#include "DynamicMesh/DynamicMesh3.h"
#include "GeometryCollection/GeometryCollection.h"
#include "Spatial/PointHashGrid3.h"

namespace Ck
{
namespace GeometryCollectionConversion
{

/**
 * Add attributes necessary for a dynamic mesh to represent geometry from an FGeometryCollection
 */
void SetGeometryCollectionAttributes(UE::Geometry::FDynamicMesh3& Mesh, int32 NumUVLayers);
	
// functions for DynamicMesh3 meshes that have FGeometryCollection attributes set
namespace AugmentedDynamicMesh
{
	void Augment(UE::Geometry::FDynamicMesh3& Mesh, int32 NumUVChannels);
	void SetVisibility(UE::Geometry::FDynamicMesh3& Mesh, int TID, bool bIsVisible);
	bool GetVisibility(const UE::Geometry::FDynamicMesh3& Mesh, int TID);
	void SetInternal(UE::Geometry::FDynamicMesh3& Mesh, int TID, bool bIsInternal);
	bool GetInternal(const UE::Geometry::FDynamicMesh3& Mesh, int TID);
	void SetUV(UE::Geometry::FDynamicMesh3& Mesh, int VID, FVector2f UV, int UVLayer);
	void GetUV(const UE::Geometry::FDynamicMesh3& Mesh, int VID, FVector2f& UV, int UVLayer);
	void SetTangent(UE::Geometry::FDynamicMesh3& Mesh, int VID, FVector3f Normal, FVector3f TangentU, FVector3f TangentV);
	void GetTangent(const UE::Geometry::FDynamicMesh3& Mesh, int VID, FVector3f& U, FVector3f& V);
	/// Initialize UV overlays based on the custom AugmentedDynamicMesh per-vertex UV attributes.  Optionally use FirstUVLayer to skip layers
	void InitializeOverlayToPerVertexUVs(UE::Geometry::FDynamicMesh3& Mesh, int32 NumUVLayers, int32 FirstUVLayer = 0);
	void InitializeOverlayToPerVertexTangents(UE::Geometry::FDynamicMesh3& Mesh);
	void ComputeTangents(UE::Geometry::FDynamicMesh3& Mesh, bool bOnlyInternalSurfaces,
		bool bRecomputeNormals = true, bool bMakeSharpEdges = false, float SharpAngleDegrees = 60);
	void AddCollisionSamplesPerComponent(UE::Geometry::FDynamicMesh3& Mesh, double Spacing);
	void SplitOverlayAttributesToPerVertex(UE::Geometry::FDynamicMesh3& Mesh, bool bSplitUVs = true, bool bSplitTangents = true);
}

// Holds Geometry from an FGeometryCollection in an FDynamicMesh3 representation, and convert both directions
// Also supports cutting geometry with FCellMeshes
struct FDynamicMeshCollection
{
	struct FMeshData
	{
		UE::Geometry::FDynamicMesh3 AugMesh;

		// FDynamicMeshAABBTree3 Spatial; // TODO: maybe refactor mesh booleans to allow version where caller provides spatial data; it's computed every boolean now
		// FTransform3d Transform; // TODO: maybe pretransform the data to a space that is good for cutting; refactor mesh boolean so there is an option to have it not transform input
		int32 TransformIndex; // where the mesh was from in the geometry collection
		FTransform FromCollection; // transform that was used to go from the geometry collection to the local space used for processing

		FMeshData(int32 NumUVLayers)
		{
			SetGeometryCollectionAttributes(AugMesh, NumUVLayers);
		}

		FMeshData(const UE::Geometry::FDynamicMesh3& Mesh, int32 TransformIndex, FTransform FromCollection) : AugMesh(Mesh), TransformIndex(TransformIndex), FromCollection(FromCollection)
		{}

		void SetMesh(const UE::Geometry::FDynamicMesh3& NewAugMesh)
		{
			ClearCachedBounds();
			AugMesh = NewAugMesh;
		}

		/// Note this relies on the caller to also call ClearCachedBounds() as needed; it will not automatically invalidate any computed bounds
		const UE::Geometry::FAxisAlignedBox3d& GetCachedBounds()
		{
			if (!bHasBounds)
			{
				Bounds = AugMesh.GetBounds(true);
				bHasBounds = true;
			}
			return Bounds;
		}

		void ClearCachedBounds()
		{
			bHasBounds = false;
		}

	private:
		bool bHasBounds = false;
		UE::Geometry::FAxisAlignedBox3d Bounds;
	};
	TIndirectArray<FMeshData> Meshes;
	UE::Geometry::FAxisAlignedBox3d Bounds;
	
	// Settings to control the geometry import
	
	// If true, triangles where the Visible property is false will not be added to the MeshData
	bool bSkipInvisible = false;
	// If false, Transforms passed to Init are interpreted as relative to the parent bone transform. If true, Transforms are all in the same 'global' / component-relative space
	bool bComponentSpaceTransforms = false;
	
	FDynamicMeshCollection() {}

	FDynamicMeshCollection(const FGeometryCollection* Collection, const TArrayView<const int32>& TransformIndices, FTransform TransformCollection, bool bSaveIsolatedVertices = false)
	{
		Init(Collection, TransformIndices, TransformCollection, bSaveIsolatedVertices);
	}

	FDynamicMeshCollection(const FGeometryCollection* Collection, const TManagedArray<FTransform>& Transforms, const TArrayView<const int32>& TransformIndices, FTransform TransformCollection, bool bSaveIsolatedVertices = false)
	{
		Init(Collection, Transforms, TransformIndices, TransformCollection, bSaveIsolatedVertices);
	}

	FDynamicMeshCollection(const FGeometryCollection* Collection, const TManagedArray<FTransform3f>& Transforms, const TArrayView<const int32>& TransformIndices, FTransform TransformCollection, bool bSaveIsolatedVertices = false)
	{
		Init(Collection, Transforms, TransformIndices, TransformCollection, bSaveIsolatedVertices);
	}

	void Init(const FGeometryCollection* Collection, const TArrayView<const int32>& TransformIndices, FTransform TransformCollection, bool bSaveIsolatedVertices = false)
	{
		Init(Collection, Collection->Transform, TransformIndices, TransformCollection, bSaveIsolatedVertices);
	}

	void Init(const FGeometryCollection* Collection, const TManagedArray<FTransform>& Transforms, const TArrayView<const int32>& TransformIndices, FTransform TransformCollection, bool bSaveIsolatedVertices = false)
	{
		Init(Collection, TArrayView<const FTransform>(Transforms.GetConstArray()), TransformIndices, TransformCollection, bSaveIsolatedVertices);
	}
	void Init(const FGeometryCollection* Collection, const TManagedArray<FTransform3f>& Transforms, const TArrayView<const int32>& TransformIndices, FTransform TransformCollection, bool bSaveIsolatedVertices = false)
	{
		Init(Collection, TArrayView<const FTransform3f>(Transforms.GetConstArray()), TransformIndices, TransformCollection, bSaveIsolatedVertices);
	}

	void Init(const FGeometryCollection* Collection, TArrayView<const FTransform> Transforms, const TArrayView<const int32>& TransformIndices, FTransform TransformCollection, bool bSaveIsolatedVertices = false);
	void Init(const FGeometryCollection* Collection, TArrayView<const FTransform3f> Transforms, const TArrayView<const int32>& TransformIndices, FTransform TransformCollection, bool bSaveIsolatedVertices = false);

	/**
	 * Split islands for all collection meshes, and append results to a geometry collection
	 *
	 * @param Collection Results will be stored in this
	 * @param CollisionSampleSpacing If > 0, new geometry will have collision samples added (vertices not on any triangles) to fill any gaps greater than the this size
	 * @return Index of the first created geometry, or -1 if nothing was split
	 */
	int32 SplitAllIslands(FGeometryCollection* Collection, double CollisionSampleSpacing);

	static void SetVisibility(FGeometryCollection& Collection, int32 GeometryIdx, bool bVisible)
	{
		int32 FaceEnd = Collection.FaceCount[GeometryIdx] + Collection.FaceStart[GeometryIdx];
		for (int32 FaceIdx = Collection.FaceStart[GeometryIdx]; FaceIdx < FaceEnd; FaceIdx++)
		{
			Collection.Visible[FaceIdx] = bVisible;
		}
	}

	// Split mesh into connected components, including implicit connections by co-located vertices
	bool SplitIslands(UE::Geometry::FDynamicMesh3& Source, TArray<UE::Geometry::FDynamicMesh3>& SeparatedMeshes);

	FString GetBoneName(FGeometryCollection& Output, int TransformParent, int SubPartIndex)
	{
		return Output.BoneName[TransformParent] + "_" + FString::FromInt(SubPartIndex);
	}

	void AddCollisionSamples(double CollisionSampleSpacing);

	// Update all geometry in a GeometryCollection w/ the meshes in the MeshCollection
	// Resizes the GeometryCollection as needed
	bool UpdateAllCollections(FGeometryCollection& Collection);

	static int32 AppendToCollection(const FTransform& FromCollection, UE::Geometry::FDynamicMesh3& Mesh, double CollisionSampleSpacing, int32 TransformParent, FString BoneName, FGeometryCollection& Output, int32 InternalMaterialID);

private:

	template<typename TransformType>
	void InitTemplate(const FGeometryCollection* Collection, TArrayView<const TransformType> Transforms, const TArrayView<const int32>& TransformIndices, FTransform TransformCollection, bool bSaveIsolatedVertices);


	void SetGeometryVisibility(FGeometryCollection* Collection, const TArray<int32>& GeometryIndices, bool bVisible);

	// Update an existing geometry in a collection w/ a new mesh (w/ the same number of faces and vertices!)
	static bool UpdateCollection(const FTransform& FromCollection, UE::Geometry::FDynamicMesh3& Mesh, int32 GeometryIdx, FGeometryCollection& Output, int32 InternalMaterialID);

	void FillVertexHash(const UE::Geometry::FDynamicMesh3& Mesh, UE::Geometry::TPointHashGrid3d<int>& VertHash);

	bool IsNeighboring(
		UE::Geometry::FDynamicMesh3& MeshA, const UE::Geometry::TPointHashGrid3d<int>& VertHashA, const UE::Geometry::FAxisAlignedBox3d& BoundsA,
		UE::Geometry::FDynamicMesh3& MeshB, const UE::Geometry::TPointHashGrid3d<int>& VertHashB, const UE::Geometry::FAxisAlignedBox3d& BoundsB);
};

}
}
