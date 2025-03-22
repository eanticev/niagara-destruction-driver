// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraDestructionDriverDataAsset.h"
#include "NiagaraDestructionDriverActor.generated.h"

/**
 * Represents Niagara Destructible
 * - initializes the render targets used to drive vertex WPO of the niagara destructible mesh materials.
 * - hot swaps static meshes for niagara destructible ones.
 * - provides force input to the underlying particle system for niagara destruction.
 * @brief Holds niagara destructible assets and drives destruction.
 */
UCLASS()
class NIAGARADESTRUCTIONDRIVER_API ANiagaraDestructionDriverActor : public AActor
{
	GENERATED_BODY()

public:

	ANiagaraDestructionDriverActor();
	
	/**
	 * The core parameters used to construct and drive this niagara destruction actor.
	 * Should always include:
	 * - static mesh
	 * - custom uv channel index
	 * - niagara system
	 * - initial bone location texture
	 * - desired render target size (ex: 16x16) 
	 */
	UPROPERTY(EditAnywhere, Category = "Niagara Destructible")
	TObjectPtr<UNiagaraDestructionDriverDataAsset> NiagaraDestructionDriverParams;
	
	/**
	 * This render target is driven by a niagara simulation and
	 * holds quaternion rotations for each of the bones in each of the pixel RGB values.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara Destructible")
	TObjectPtr<UTextureRenderTarget2D> RotationsTexture;
	
	/**
	 * This render target is driven by a niagara simulation and
	 * holds local XYZ position vector for each of the bones in it's pixel RGB values.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara Destructible")
	TObjectPtr<UTextureRenderTarget2D> PositionsTexture;
	
	/**
	 * Forces the mesh to use the debug material. This is useful if you have
	 * still not implemented the required material functions
	 * inside your project's master material.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara Destructible")
	bool bDebugMaterial;
	
	/**
	 * How much to extend the mesh bounds by to prevent occlusion culling from hiding the
	 * fragments offset by WPO in the vertex shader.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara Destructible")
	float CullingBoundsMultiplier = 4.f;

	/**
	 * Use this to "destroy" parts of this actor. Under the hood it
	 * provides destruction force input to the underlying
	 * niagara system driving the physics simulation.
	 */
	UFUNCTION()
	void InitiateDestructionForce(FVector ForceOrigin, float ForceRadius, float ForceDuration = 0.1f);
	
	// <components>
	UPROPERTY() TObjectPtr<USceneComponent> SourceGeometryContainer; // will contain original static meshes used in the geometry collection that was processed into this actor
	UPROPERTY() TObjectPtr<UStaticMeshComponent> MeshComponent;
	UPROPERTY() TObjectPtr<UNiagaraComponent> NiagaraComponent;
	// </components>

	// <overrides>
	virtual void PostInitProperties() override;
	virtual void PostInitializeComponents() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// </overrides>

protected:

	virtual void BeginPlay() override;

private:

	/**
	 * The static mesh has materials where vertex WPO is driven by render targets coming from niagara.
	 * We need to wire all these parameters up. This array will be filled with these materials.
	 */
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> MeshMaterialsWithParamsSet;
	
	/** Is this destructible in resting state (untouched) or already damaged */
	UPROPERTY() bool bIsInRestingState;
};
