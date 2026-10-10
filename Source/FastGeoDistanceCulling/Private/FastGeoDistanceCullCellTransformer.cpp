// Fill out your copyright notice in the Description page of Project Settings.

#include "FastGeoDistanceCullCellTransformer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CullDistanceVolume.h"
#include "WorldPartition/HLOD/HLODActor.h"

namespace
{
	float GetStaticMeshDiameter(const UStaticMeshComponent* Component)
	{
		if (!Component || !Component->GetStaticMesh())
		{
			return 0.0f;
		}

		return Component->GetStaticMesh()->GetBounds().SphereRadius * 2.0f;
	}

	float GetComponentScale(const UStaticMeshComponent* Component)
	{
		return Component
			       ? Component->GetComponentTransform().GetScale3D().GetAbsMax()
			       : 0.0f;
	}

	float GetInstancePrimitiveSize(const UInstancedStaticMeshComponent* ISM,
	                               const FTransform& InstanceTransform)
	{
		if (!ISM || !ISM->GetStaticMesh())
		{
			return 0.0f;
		}

		const float MeshDiameter = ISM->GetStaticMesh()->GetBounds().SphereRadius * 2.0f;
		const float ComponentScale = ISM->GetComponentTransform().GetScale3D().GetAbsMax();
		const float InstanceScale = InstanceTransform.GetScale3D().GetAbsMax();

		return MeshDiameter * ComponentScale * InstanceScale;
	}
}

#if WITH_EDITOR
void UFastGeoDistanceCullCellTransformer::Transform(ULevel* InLevel)
{
	if (!InLevel)
	{
		return;
	}

	for (AActor* Actor : InLevel->Actors)
	{
		if (!IsValid(Actor)
			|| Actor->IsA<AWorldPartitionHLOD>())
		{
			continue;
		}

		TInlineComponentArray<UStaticMeshComponent*> StaticMeshComponents;

		Actor->GetComponents(StaticMeshComponents);

		for (UStaticMeshComponent* Component : StaticMeshComponents)
		{
			if (!IsValid(Component)
				|| !Component->GetStaticMesh()
				|| Component->bNeverDistanceCull)
			{
				continue;
			}

			if (UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Component))
			{
				ProcessISM(Actor, ISM);
				continue;
			}

			const float PrimitiveSize = GetStaticMeshDiameter(Component) * GetComponentScale(Component);
			const int32 CullPairIndex = FindCullDistancePairIndex(PrimitiveSize);

			if (CullPairIndex == INDEX_NONE)
			{
				continue;
			}

			ApplyCullDistance(Component, CullDistances[CullPairIndex]);
		}
	}
}
#endif

int32 UFastGeoDistanceCullCellTransformer::FindCullDistancePairIndex(const float PrimitiveSize) const
{
	if (CullDistances.IsEmpty())
	{
		return INDEX_NONE;
	}

	int32 BestIndex = INDEX_NONE;
	float BestDifference = FLT_MAX;

	for (int32 Index = 0; Index < CullDistances.Num(); ++Index)
	{
		const FCullDistanceSizePair& Pair = CullDistances[Index];

		if (const float Difference = FMath::Abs(PrimitiveSize - Pair.Size); Difference < BestDifference)
		{
			BestDifference = Difference;
			BestIndex = Index;
		}
		else if (FMath::IsNearlyEqual(Difference, BestDifference))
		{
			if (const float CurrentDistance = CullDistances[BestIndex].CullDistance; Pair.CullDistance > 0.0f
				&& (CurrentDistance <= 0.0f
					|| Pair.CullDistance < CurrentDistance))
			{
				BestIndex = Index;
			}
		}
	}

	return BestIndex;
}

void UFastGeoDistanceCullCellTransformer::ApplyCullDistance(UStaticMeshComponent* Component,
                                                            const FCullDistanceSizePair& Pair) const
{
	if (!Component)
	{
		return;
	}

	const float EndCullDistance = FMath::Max(Pair.CullDistance, 0.0f);
	Component->SetCullDistance(EndCullDistance);
	Component->SetCachedMaxDrawDistance(EndCullDistance);
}

void UFastGeoDistanceCullCellTransformer::ApplyISMCullDistance(UInstancedStaticMeshComponent* Component,
                                                               const FCullDistanceSizePair& Pair) const
{
	if (!Component)
	{
		return;
	}

	const float EndCullDistance = FMath::Max(Pair.CullDistance, 0.0f);

	if (EndCullDistance <= 0.0f)
	{
		Component->SetCullDistances(0, 0);
		Component->SetCullDistance(0.0f);
		Component->SetCachedMaxDrawDistance(0.0f);

		return;
	}

	const int32 EndDistance = FMath::RoundToInt(EndCullDistance);
	const int32 StartDistance = FMath::RoundToInt(EndCullDistance * CullFadeStartFraction);

	Component->SetCullDistances(StartDistance, EndDistance);

	Component->UpdateBounds();

	const float ConservativeDistance = EndCullDistance + Component->Bounds.SphereRadius;

	Component->SetCullDistance(ConservativeDistance);
	Component->SetCachedMaxDrawDistance(ConservativeDistance);
}

void UFastGeoDistanceCullCellTransformer::ProcessISM(AActor* Owner,
                                                     UInstancedStaticMeshComponent* Component)
{
	if (!Owner
		|| !Component
		|| !Component->GetStaticMesh())
	{
		return;
	}

	TMap<int32, FRT_CullInstanceBucket> Buckets;

	const int32 InstanceCount = Component->GetInstanceCount();

	for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
	{
		FTransform InstanceTransform;

		if (!Component->GetInstanceTransform(InstanceIndex, InstanceTransform, false))
		{
			continue;
		}

		const float PrimitiveSize = GetInstancePrimitiveSize(Component, InstanceTransform);
		const int32 CullPairIndex = FindCullDistancePairIndex(PrimitiveSize);

		if (CullPairIndex == INDEX_NONE)
		{
			continue;
		}

		FRT_CullInstanceBucket& Bucket = Buckets.FindOrAdd(CullPairIndex);
		Bucket.CullPairIndex = CullPairIndex;
		FRT_CullInstanceData& InstanceData = Bucket.Instances.AddDefaulted_GetRef();
		InstanceData.Transform = InstanceTransform;

		if (Component->NumCustomDataFloats > 0)
		{
			const int32 Offset = InstanceIndex * Component->NumCustomDataFloats;

			if (Component->PerInstanceSMCustomData.IsValidIndex(
				Offset +
				Component->NumCustomDataFloats - 1))
			{
				InstanceData.CustomData.Append(
					Component->PerInstanceSMCustomData.GetData()
					+ Offset,
					Component->NumCustomDataFloats);
			}
		}
	}

	if (Buckets.IsEmpty())
	{
		return;
	}

	if (Buckets.Num() == 1)
	{
		const FRT_CullInstanceBucket& Bucket = Buckets.CreateConstIterator().Value();
		ApplyISMCullDistance(Component, CullDistances[Bucket.CullPairIndex]);
		return;
	}

	auto PopulateISM = [](UInstancedStaticMeshComponent* Target, const FRT_CullInstanceBucket& Bucket)
	{
		Target->ClearInstances();

		for (const FRT_CullInstanceData& Instance : Bucket.Instances)
		{
			const int32 NewInstanceIndex = Target->AddInstance(Instance.Transform, false);

			if (NewInstanceIndex != INDEX_NONE && !Instance.CustomData.IsEmpty())
			{
				Target->SetCustomData(NewInstanceIndex, MakeArrayView(Instance.CustomData), false);
			}
		}

		Target->UpdateBounds();
	};

	const FRT_CullInstanceBucket* MainBucket = nullptr;

	for (const TPair<int32, FRT_CullInstanceBucket>& Pair : Buckets)
	{
		if (!MainBucket
			|| Pair.Value.Instances.Num() > MainBucket->Instances.Num())
		{
			MainBucket = &Pair.Value;
		}
	}

	check(MainBucket);

	PopulateISM(Component, *MainBucket);

	ApplyISMCullDistance(Component, CullDistances[MainBucket->CullPairIndex]);

	for (const TPair<int32, FRT_CullInstanceBucket>& Pair : Buckets)
	{
		const FRT_CullInstanceBucket& Bucket = Pair.Value;

		if (&Bucket == MainBucket)
		{
			continue;
		}

		const FName NewComponentName = MakeUniqueObjectName(
			Owner,
			Component->GetClass(),
			*FString::Printf(
				TEXT("%s_CullBucket_%d"),
				*Component->GetName(),
				Bucket.CullPairIndex));

		UInstancedStaticMeshComponent* NewISM = DuplicateObject<UInstancedStaticMeshComponent>(
			Component,
			Owner,
			NewComponentName);

		if (!NewISM)
		{
			continue;
		}

		PopulateISM(NewISM, Bucket);

		Owner->AddInstanceComponent(NewISM);
		Owner->AddOwnedComponent(NewISM);

		ApplyISMCullDistance(NewISM, CullDistances[Bucket.CullPairIndex]);

		if (Component->IsRegistered())
		{
			NewISM->RegisterComponent();
		}
	}
}
