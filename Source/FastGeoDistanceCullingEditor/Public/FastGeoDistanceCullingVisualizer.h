// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UFastGeoDistanceCullCellTransformer;

class FFastGeoDistanceCullingVisualizer
{
	struct FWorldVisualizationState
	{
		bool bEnabled{false};
		double LastRefreshTime{0.0};
		int32 LastStreamingEpoch{INDEX_NONE};
	};

	TMap<TWeakObjectPtr<UWorld>, FWorldVisualizationState> WorldStates{};

	FTSTicker::FDelegateHandle TickerHandle{};
	FDelegateHandle DebugDrawHandle{};

	uint8 bColorHandlerRegistered : 1{false};
	uint8 bShuttingDown : 1{false};

	static const FName ColorHandlerName;
	static constexpr float DistanceTolerance{1.0f};
	
	//Draw legend variables
	static constexpr float StartX = 24.0f;
	static constexpr float StartY = 40.0f;
	static constexpr float ColorBoxSize = 14.0f;
	static constexpr float RowHeight = 20.0f;

public:
	void Startup();
	void Shutdown();

private:
	bool Tick(float DeltaTime);

	void EnableForWorld(const UWorld* InWorld,
	                    const UFastGeoDistanceCullCellTransformer* InTransformer) const;
	void DisableForWorld(UWorld* InWorld) const;
	void RefreshColors(const UWorld* InWorld,
	                   const UFastGeoDistanceCullCellTransformer* InTransformer) const;
	void DrawLegend(UCanvas* Canvas, APlayerController* PlayerController) const;
	UWorld* ResolveVisualizationWorld(const APlayerController* InPlayerController) const;
};
