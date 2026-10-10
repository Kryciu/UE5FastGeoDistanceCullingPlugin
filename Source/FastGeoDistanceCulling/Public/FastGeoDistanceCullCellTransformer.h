// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/WorldPartitionRuntimeCellTransformer.h"
#include "FastGeoDistanceCullCellTransformer.generated.h"

struct FCullDistanceSizePair;

UCLASS(MinimalAPI)
class UFastGeoDistanceCullCellTransformer : public UWorldPartitionRuntimeCellTransformer
{
	GENERATED_BODY()

	struct FRT_CullInstanceData
	{
		FTransform Transform;
		TArray<float> CustomData;
	};

	struct FRT_CullInstanceBucket
	{
		int32 CullPairIndex = INDEX_NONE;
		TArray<FRT_CullInstanceData> Instances;
	};

	UPROPERTY(EditAnywhere, Category = "Distance Culling")
	TArray<FCullDistanceSizePair> CullDistances{};
	
	UPROPERTY(EditAnywhere, Category = "Distance Culling", meta=(ClampMin="0.0", ClampMax="1.0"))
	float CullFadeStartFraction{ 0.8f };

	UPROPERTY(EditAnywhere, Category = "Debug Visualization")
	uint8 bEnableDebugView : 1{false};

public:
#if WITH_EDITOR
	virtual void Transform(ULevel* InLevel) override;
#endif

	FASTGEODISTANCECULLING_API const TArray<FCullDistanceSizePair>& GetCullDistances() const
	{
		return CullDistances;
	}

	FASTGEODISTANCECULLING_API bool IsDebugViewEnabled() const
	{
		return bEnableDebugView;
	}

private:
	int32 FindCullDistancePairIndex(float PrimitiveSize) const;

	void ApplyCullDistance(UStaticMeshComponent* Component, const FCullDistanceSizePair& Pair) const;
	
	void ApplyISMCullDistance(UInstancedStaticMeshComponent* Component, const FCullDistanceSizePair& Pair) const;

	void ProcessISM(AActor* Owner, UInstancedStaticMeshComponent* Component);
};
