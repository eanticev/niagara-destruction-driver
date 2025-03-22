// Fill out your copyright notice in the Description page of Project Settings.


#include "NiagaraDestructionDriverHelper.h"

#include "CVars.h"
#include "NiagaraDestructionDriverActor.h"
#include "Engine/OverlapResult.h"

void UNiagaraDestructionDriverHelper::InitiateDestructionForce(const UObject* WorldContextObject, const FVector Location, const float Radius, const float Force)
{
	// Set up collision parameters
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.bReturnPhysicalMaterial = false;
    
	// Create collision shape (sphere)
	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(Radius);

	TArray<FOverlapResult> OverlapResults;
    
	// Perform the overlap check
	const bool bHasOverlap = WorldContextObject->GetWorld()->OverlapMultiByChannel(
		OverlapResults,
		Location,
		FQuat::Identity, // No rotation for a sphere
		ECC_WorldDynamic,
		CollisionShape,
		QueryParams
	);

	if (bHasOverlap)
	{
		for (FOverlapResult& Result: OverlapResults)
		{
			if (Result.OverlapObjectHandle.DoesRepresentClass(ANiagaraDestructionDriverActor::StaticClass()))
			{
				ANiagaraDestructionDriverActor* NDDActor = Result.OverlapObjectHandle.FetchActor<ANiagaraDestructionDriverActor>();
				NDDActor->InitiateDestructionForce(Location, Radius);

				if (CVarNDD_DebugCollisions.GetValueOnGameThread() == 1)
				{
					DrawDebugSphere(NDDActor->GetWorld(), Location, Radius, 32, FColor::Yellow, true, 2.f, 0, 1);
				}
			}
		}
	}
}
