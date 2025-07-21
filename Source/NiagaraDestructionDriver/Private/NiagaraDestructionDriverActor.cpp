#include "NiagaraDestructionDriverActor.h"

#include "CVars.h"
#include "NiagaraDestructionDriver.h"
#include "NiagaraComponent.h"
#include "NiagaraDestructionDriverSettings.h"
#include "Engine/TextureRenderTarget2D.h"

void SetDebugMaterial(ANiagaraDestructionDriverActor* ForActor)
{
	if (const UNiagaraDestructionDriverSettings* Settings = GetDefault<UNiagaraDestructionDriverSettings>())
	{
		const auto MeshMaterial = Settings->DebugMaterialForNiagaraDestructibles;
		const auto BaseMaterial = MeshMaterial.LoadSynchronous();
		UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, ForActor);
		DynamicMaterial->SetScalarParameterValue(FName("RT_Size"), ForActor->NiagaraDestructionDriverParams->RenderTargetTextureSize);
		DynamicMaterial->SetTextureParameterValue(FName("RT_Position"), ForActor->PositionsTexture);
		DynamicMaterial->SetTextureParameterValue(FName("RT_Rotation"), ForActor->RotationsTexture);
		DynamicMaterial->SetTextureParameterValue(FName("InitialBoneLocations"), ForActor->NiagaraDestructionDriverParams->InitialBoneLocationsTexture);
		uint32 Idx = 0;
		for (const auto MaterialSlot : ForActor->MeshComponent->GetMaterialSlotNames())
		{
			ForActor->MeshComponent->SetMaterial(Idx, DynamicMaterial);
			Idx++;
		}
	}
}

ANiagaraDestructionDriverActor::ANiagaraDestructionDriverActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bIsInRestingState = true;
	
	// Create and set up the scene component as the root
	USceneComponent* RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	RootComponent = RootSceneComponent;
	
	// Create and attach the static mesh component to the root
	// this mesh component has the material that does the vertex WPO offset based on render targets
	// driven by the niagara system in this actor.
	//
	// by default it's hidden and will only be revealed when affected by a destruction force
	// see `InitiateDestructionForce(...)`
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootSceneComponent);
	MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	// MeshComponent->SetHiddenInGame(true);
	// MeshComponent->SetVisibility(false, true);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Create and attach the Niagara component to the root
	// this is the particle system that spawns a particle per bone of the destructible
	// by using the initial bones locations texture AND it will drive the physics simulation
	NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComponent"));
	NiagaraComponent->SetupAttachment(RootSceneComponent);
	NiagaraComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	
	// Create and attach the component that will house the temporary "original" geometry.
	// These are the static meshes that were used in the geometry collection so here we can
	// use them as a "preview" and potentially benefit from auto-instance batching in rendering
	// until we are ready to swap these with the destructible version of the mesh.
	// We will hot swap these with the actual destructible meshes when destruction starts
	SourceGeometryContainer = CreateDefaultSubobject<USceneComponent>(TEXT("SourceGeometryContainer"));
	SourceGeometryContainer->SetupAttachment(RootSceneComponent);
	SourceGeometryContainer->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
}

void ANiagaraDestructionDriverActor::InitiateDestructionForce(FVector ForceOrigin, float ForceRadius, float ForceDuration)
{
	// if this is the first time we are initiating destruction force on this mesh, hot swap with the true destructible.
	if (bIsInRestingState)
	{
		MeshComponent->SetVisibility(true, true);
		// MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		
		SourceGeometryContainer->SetVisibility(false,true);
		// TArray<USceneComponent*> SourceMeshes;
		// SourceGeometryContainer->GetChildrenComponents(false, SourceMeshes);
		// for (const auto Child : SourceMeshes)
		// {
		// 	Cast<UStaticMeshComponent>(Child)->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// }
		
		// increase the bounds scale of the mesh to allow for shadows and culling to work correctly
		MeshComponent->SetBoundsScale(CullingBoundsMultiplier);
		for (const auto DynamicMaterial : MeshMaterialsWithParamsSet)
		{
			DynamicMaterial->SetScalarParameterValue(FName("ObjectBoundsScale"), CullingBoundsMultiplier);
		}
		
		bIsInRestingState = false;
	}

	const auto ForceStartTime = GetWorld()->GetTimeSeconds();

	// provide the force parameters to the underlying particle system that drives the destruction simulation
	NiagaraComponent->SetVariableVec3("ForceCenter", ForceOrigin);
	NiagaraComponent->SetVariableFloat("ForceRadius", ForceRadius);
	NiagaraComponent->SetVariableFloat("ForceStartTime", ForceStartTime);
	NiagaraComponent->SetVariableFloat("ForceDuration", ForceDuration);

	UE_LOG(LogNiagaraDestructionDriver, Log, TEXT("Destruction Force Generated at (%f, %f, %f) with radius: %f, start time: %f, and duration: %f"),
			ForceOrigin.X,
			ForceOrigin.Y,
			ForceOrigin.Z,
			ForceRadius,
			ForceStartTime,
			ForceDuration);
}

void ANiagaraDestructionDriverActor::PostInitProperties()
{
	Super::PostInitProperties();
}

void ANiagaraDestructionDriverActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

void ANiagaraDestructionDriverActor::BeginPlay()
{
	Super::BeginPlay();

	ensureMsgf(NiagaraDestructionDriverParams != nullptr, TEXT("Niagara Destruction Driver Actor has no data asset specified. Make sure you set NiagaraDestructionDriverParams property."));
	ensureMsgf(NiagaraDestructionDriverParams->InitialBoneLocationsTexture != nullptr, TEXT("Niagara Destruction Driver data asset is missing the required initial bones locations texture. This should have been auto generated."));
	ensureMsgf(NiagaraDestructionDriverParams->ParticleSystemDriver.IsNull() == false, TEXT("Niagara Destruction Driver data asset is missing the required particle system property. This should have been auto generated."));

	if (NiagaraDestructionDriverParams != nullptr)
	{
		// Create the rotations render target object
		RotationsTexture = NewObject<UTextureRenderTarget2D>();
		RotationsTexture->RenderTargetFormat = RTF_RGBA16f;
		RotationsTexture->ClearColor = FLinearColor::Black;
		RotationsTexture->bAutoGenerateMips = false;
		RotationsTexture->bCanCreateUAV = false;
		RotationsTexture->InitAutoFormat(NiagaraDestructionDriverParams->RenderTargetTextureSize, NiagaraDestructionDriverParams->RenderTargetTextureSize);	
		RotationsTexture->LODGroup = TEXTUREGROUP_16BitData;
		RotationsTexture->UpdateResourceImmediate(true);

		// Create the positions render target object
		PositionsTexture = NewObject<UTextureRenderTarget2D>();
		PositionsTexture->RenderTargetFormat = RTF_RGBA16f;
		PositionsTexture->ClearColor = FLinearColor::Black;
		PositionsTexture->bAutoGenerateMips = false;
		PositionsTexture->bCanCreateUAV = false;
		PositionsTexture->InitAutoFormat(NiagaraDestructionDriverParams->RenderTargetTextureSize, NiagaraDestructionDriverParams->RenderTargetTextureSize);	
		PositionsTexture->LODGroup = TEXTUREGROUP_16BitData;
		PositionsTexture->UpdateResourceImmediate(true);
	}
	
	if (NiagaraDestructionDriverParams && NiagaraDestructionDriverParams->StaticMesh)
	{
		MeshComponent->SetStaticMesh(NiagaraDestructionDriverParams->StaticMesh);
		MeshComponent->SetVisibility(false, true);
	}

	// Create the dynamic material instance for our mesh and set the relevant parameters
	if (CVarNDD_DebugMaterial.GetValueOnGameThread() == 1)
	{
		SetDebugMaterial(this);
	}
	else
	{
		// Use quaternion for material parameter - best for smooth interpolation
		const FQuat QuatRotation = GetActorRotation().Quaternion();
		const FVector4 QuatVector = FVector4(QuatRotation.X, QuatRotation.Y, QuatRotation.Z, QuatRotation.W);
		const FVector Extents = MeshComponent->GetStaticMesh()->GetBoundingBox().GetExtent();
		
		// Create the dynamic material instance for our mesh and set the relevant parameters
		uint32 Idx = 0;
		MeshMaterialsWithParamsSet.Empty();
		MeshMaterialsWithParamsSet.Reserve(MeshComponent->GetMaterialSlotNames().Num());
		for (const auto MaterialSlot : MeshComponent->GetMaterialSlotNames())
		{
			const auto SlotMaterial = MeshComponent->GetMaterial(Idx);
			UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(SlotMaterial, this, FName(GetName()+"_Material"+FString::FromInt(Idx)));
			DynamicMaterial->SetScalarParameterValue(FName("RT_Size"), NiagaraDestructionDriverParams->RenderTargetTextureSize);
			DynamicMaterial->SetTextureParameterValue(FName("RT_Position"), PositionsTexture);
			DynamicMaterial->SetTextureParameterValue(FName("RT_Rotation"), RotationsTexture);
			DynamicMaterial->SetTextureParameterValue(FName("InitialBoneLocations"), NiagaraDestructionDriverParams->InitialBoneLocationsTexture);
			DynamicMaterial->SetVectorParameterValue(FName("ActorRotationQuat"), QuatVector);
			DynamicMaterial->SetVectorParameterValue(FName("MeshHalfExtents"), Extents);
			MeshMaterialsWithParamsSet.Add(DynamicMaterial);
			MeshComponent->SetMaterial(Idx, DynamicMaterial);
			Idx++;
		}

		// uint32 Index = 0;
		// for (const auto DynamicMaterial : MeshMaterialsWithParamsSet)
		// {
		// 	DynamicMaterial->SetScalarParameterValue(FName("RT_Size"), NiagaraDestructionDriverParams->RenderTargetTextureSize);
		// 	DynamicMaterial->SetTextureParameterValue(FName("RT_Position"), PositionsTexture);
		// 	DynamicMaterial->SetTextureParameterValue(FName("RT_Rotation"), RotationsTexture);
		// 	DynamicMaterial->SetTextureParameterValue(FName("InitialBoneLocations"), NiagaraDestructionDriverParams->InitialBoneLocationsTexture);
		// 	MeshComponent->SetMaterial(Index, DynamicMaterial);
		// 	Index++;
		// }
	}

	// set niagara asset variables
	if (!NiagaraDestructionDriverParams->ParticleSystemDriver.IsNull())
	{
		UNiagaraSystem* BaseNiagaraAsset = NiagaraDestructionDriverParams->ParticleSystemDriver.LoadSynchronous();
		NiagaraComponent->SetAsset(BaseNiagaraAsset);
		NiagaraComponent->SetVariableStaticMesh("DestructibleMesh", MeshComponent->GetStaticMesh());
		NiagaraComponent->SetVariableTexture("InitialBonePositionsTexture", NiagaraDestructionDriverParams->InitialBoneLocationsTexture);
		NiagaraComponent->SetVariableTextureRenderTarget("SimulatedParticlePositionsOut", PositionsTexture);
		NiagaraComponent->SetVariableTextureRenderTarget("SimulatedParticleRotationsOut", RotationsTexture);
		NiagaraComponent->SetVariableVec3(FName("DestructibleMeshLocalHalfExtents"), this->MeshComponent->GetStaticMesh()->GetBoundingBox().GetExtent());

		// moves the particle system to be centered against the destructible mesh and so that all the local space ([-1,1] space) particles are correctly aligned.
		NiagaraComponent->SetRelativeLocation(-NiagaraDestructionDriverParams->PivotOffset);
		// NiagaraComponent->ResetSystem();
	}
}

#if WITH_EDITOR
void ANiagaraDestructionDriverActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ANiagaraDestructionDriverActor, bDebugMaterial))
	{
		/*
		if (bDebugMaterial)
		{
			SetDebugMaterial(this);
		}
		else
		{
			uint32 Index = 0;
			for (const auto DynamicMaterial : MeshMaterialsWithParamsSet)
			{
				MeshComponent->SetMaterial(Index, DynamicMaterial);
				Index++;
			}
		}
		*/
	}
}
#endif