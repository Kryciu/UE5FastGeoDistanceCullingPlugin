// Fill out your copyright notice in the Description page of Project Settings.

#include "FastGeoDistanceCullingVisualizer.h"
#include "FastGeoDistanceCullCellTransformer.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/CullDistanceVolume.h"
#include "FastGeoStreaming/Private/FastGeoComponentCluster.h"
#include "FastGeoStreaming/Private/FastGeoContainer.h"
#include "FastGeoStreaming/Private/FastGeoPrimitiveComponent.h"
#include "GameFramework/ActorPrimitiveColorHandler.h"
#include "WorldPartition/WorldPartition.h"

const FName FFastGeoDistanceCullingVisualizer::ColorHandlerName(TEXT("FastGeoDistanceCull"));

namespace
{
	void ResetColors(const UWorld* InWorld)
	{
		if (!InWorld)
		{
			return;
		}

		for (ULevel* Level : InWorld->GetLevels())
		{
			if (!Level)
			{
				UFastGeoContainer* FastGeoContainer = Level->GetAssetUserData<UFastGeoContainer>();

				if (!FastGeoContainer)
				{
					continue;
				}

				FastGeoContainer->ForEachComponentCluster(
					[](FFastGeoComponentCluster& ComponentCluster)
					{
						ComponentCluster.ForEachComponent<FFastGeoPrimitiveComponent>(
							[](const FFastGeoPrimitiveComponent& Component)
							{
								if (FPrimitiveSceneProxy* SceneProxy = Component.GetSceneProxy())
								{
									SceneProxy->SetPrimitiveColor_GameThread(FLinearColor::White);
								}
							});
					});
			}
		}
	}

	const UFastGeoDistanceCullCellTransformer* FindTransformer(const UWorld* InWorld)
	{
		if (!InWorld)
		{
			return nullptr;
		}

		const UWorldPartition* WorldPartition = InWorld->GetWorldPartition();

		if (!WorldPartition)
		{
			return nullptr;
		}

		for (TObjectIterator<UFastGeoDistanceCullCellTransformer> It; It; ++It)
		{
			const UFastGeoDistanceCullCellTransformer* Transformer = *It;

			if (!IsValid(Transformer)
				|| Transformer->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
			{
				continue;
			}

			const UWorldPartition* WorldPartitionTransformer =
				Transformer->GetTypedOuter<UWorldPartition>();

			if (WorldPartitionTransformer == WorldPartition)
			{
				return Transformer;
			}
		}

		return nullptr;
	}

	int32 FindBucketIndex(const FPrimitiveSceneProxy* InSceneProxy,
	                      const UFastGeoDistanceCullCellTransformer* InTransformer,
	                      const float DistanceTolerance)
	{
		if (!InSceneProxy || !InTransformer)
		{
			return INDEX_NONE;
		}

		const TArray<FCullDistanceSizePair>& CullDistances = InTransformer->GetCullDistances();

		if (CullDistances.IsEmpty())
		{
			return INDEX_NONE;
		}

		float ActualCullDistance;
		FVector2f InstanceDrawDistance;

		if (InSceneProxy->GetInstanceDrawDistanceMinMax(InstanceDrawDistance))
		{
			ActualCullDistance = InstanceDrawDistance.Y;
		}
		else
		{
			const float PrimitiveMaxDrawDistance = InSceneProxy->GetMaxDrawDistance();

			const bool bInfinite = PrimitiveMaxDrawDistance <= 0.0f
				|| !FMath::IsFinite(PrimitiveMaxDrawDistance)
				|| PrimitiveMaxDrawDistance >= 1.0e20f;

			ActualCullDistance = bInfinite ? 0.0f : PrimitiveMaxDrawDistance;
		}

		for (int32 Index = 0; Index < CullDistances.Num(); ++Index)
		{
			if (FMath::IsNearlyEqual(CullDistances[Index].CullDistance,
			                         ActualCullDistance, DistanceTolerance))
			{
				return Index;
			}
		}

		return INDEX_NONE;
	}

	FLinearColor GetBucketColor(const int32 BucketIndex, const int32 NumBuckets)
	{
		if (BucketIndex < 0 || NumBuckets <= 0)
		{
			return FLinearColor::White;
		}

		const float NormalizedIndex =
			static_cast<float>(BucketIndex) /
			static_cast<float>(FMath::Max(NumBuckets, 1));

		const uint8 Hue = static_cast<uint8>(NormalizedIndex * 255.0f);

		return FLinearColor::MakeFromHSV8(
			Hue,
			220,
			255
		);
	}

	bool ShouldVisualize(const UFastGeoDistanceCullCellTransformer* Transformer)
	{
		if (!Transformer)
		{
			return false;
		}

		return Transformer->IsDebugViewEnabled();
	}

	void SetActorColorationViewportState(const UWorld* World, const bool bEnabled)
	{
		if (!World)
		{
			return;
		}

		if (UGameViewportClient* GameViewport = World->GetGameViewport())
		{
			GameViewport->EngineShowFlags.SetActorColoration(bEnabled);
		}

#if WITH_EDITOR

		if (GEditor)
		{
			const UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();

			for (FEditorViewportClient* ViewportClient : GEditor->GetAllViewportClients())
			{
				if (!ViewportClient)
				{
					continue;
				}

				const UWorld* ViewportWorld = ViewportClient->GetWorld();

				const bool bMatchesWorld = ViewportWorld == World
					|| (World->WorldType == EWorldType::PIE && ViewportWorld == EditorWorld);

				if (!bMatchesWorld)
				{
					continue;
				}

				ViewportClient->EngineShowFlags.SetActorColoration(bEnabled);
				ViewportClient->Invalidate(false, false);
			}
		}

#endif
	}
}

void FFastGeoDistanceCullingVisualizer::Startup()
{
#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER

	FActorPrimitiveColorHandler::FPrimitiveColorHandler Handler;

	Handler.HandlerName = ColorHandlerName;

	Handler.HandlerText =
		NSLOCTEXT(
			"FastGeoDistanceCullingEditor",
			"FastGeoDistanceCull",
			"FastGeo Distance Cull");

	Handler.HandlerToolTipText =
		NSLOCTEXT(
			"FastGeoDistanceCullingEditor",
			"FastGeoDistanceCullTooltip",
			"Visualizes FastGeo primitives using their configured distance-culling buckets.");

	Handler.bAvailalbleInEditor = true;

	Handler.GetColorFunc =
		[](const UPrimitiveComponent*)
		{
			return FLinearColor::White;
		};

	Handler.ActivateFunc =
		[this]
		{
			if (bShuttingDown || !GEngine)
			{
				return;
			}

			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (const UWorld* World = Context.World())
				{
					if (World->IsGameWorld())
					{
						if (const auto* Transformer = FindTransformer(World))
						{
							RefreshColors(World, Transformer);
						}
					}
				}
			}
		};

	Handler.DeactivateFunc =
		[this]
		{
			if (bShuttingDown)
			{
				return;
			}

			for (const TPair<TWeakObjectPtr<UWorld>, FWorldVisualizationState>& Pair : WorldStates)
			{
				const UWorld* World = Pair.Key.Get();

				if (!World)
				{
					continue;
				}

				ResetColors(World);
			}
		};

	FActorPrimitiveColorHandler::Get().RegisterPrimitiveColorHandler(Handler);

	bColorHandlerRegistered = true;

#endif

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(
			this,
			&FFastGeoDistanceCullingVisualizer::Tick),
		0.25f);

	DebugDrawHandle = UDebugDrawService::Register(
		TEXT("ActorColoration"),
		FDebugDrawDelegate::CreateRaw(
			this,
			&FFastGeoDistanceCullingVisualizer::DrawLegend));
}

void FFastGeoDistanceCullingVisualizer::Shutdown()
{
	if (bShuttingDown)
	{
		return;
	}

	bShuttingDown = true;

	if (TickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	if (DebugDrawHandle.IsValid())
	{
		UDebugDrawService::Unregister(DebugDrawHandle);
		DebugDrawHandle.Reset();
	}

#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER

	if (bColorHandlerRegistered)
	{
		FActorPrimitiveColorHandler::Get()
			.UnregisterPrimitiveColorHandler(ColorHandlerName);

		bColorHandlerRegistered = false;
	}

#endif

	WorldStates.Reset();
}

bool FFastGeoDistanceCullingVisualizer::Tick(float DeltaTime)
{
	(void)DeltaTime;

	if (!GEngine)
	{
		return true;
	}

	const double CurrentTime = FPlatformTime::Seconds();

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();

		if (!World || !World->IsGameWorld())
		{
			continue;
		}

		const UFastGeoDistanceCullCellTransformer* Transformer = FindTransformer(World);

		if (!Transformer)
		{
			continue;
		}

		FWorldVisualizationState& State = WorldStates.FindOrAdd(World);

		const bool bShouldEnable = ShouldVisualize(Transformer);

		if (bShouldEnable != State.bEnabled)
		{
			State.bEnabled = bShouldEnable;

			if (bShouldEnable)
			{
				EnableForWorld(World, Transformer);
			}
			else
			{
				DisableForWorld(World);
			}

			State.LastRefreshTime = CurrentTime;
		}

		if (!State.bEnabled)
		{
			continue;
		}

		if (FActorPrimitiveColorHandler::Get().GetActivePrimitiveColorHandler() != ColorHandlerName)
		{
			FActorPrimitiveColorHandler::Get().SetActivePrimitiveColorHandler(ColorHandlerName, World);
		}

		SetActorColorationViewportState(World, true);

		const UWorldPartition* WorldPartition = World->GetWorldPartition();

		const int32 StreamingEpoch = WorldPartition
			                             ? WorldPartition->GetStreamingStateEpoch()
			                             : INDEX_NONE;

		const bool bStreamingChanged = StreamingEpoch != State.LastStreamingEpoch;

		const bool bPeriodicRefresh = CurrentTime - State.LastRefreshTime >= 1.0;

		if (bStreamingChanged || bPeriodicRefresh)
		{
			RefreshColors(World, Transformer);

			State.LastStreamingEpoch = StreamingEpoch;
			State.LastRefreshTime = CurrentTime;
		}
	}

	return true;
}

void FFastGeoDistanceCullingVisualizer::EnableForWorld(const UWorld* InWorld,
                                                       const UFastGeoDistanceCullCellTransformer* InTransformer) const
{
	if (!InWorld || !InTransformer)
	{
		return;
	}

	SetActorColorationViewportState(InWorld, true);

	RefreshColors(InWorld, InTransformer);
}

void FFastGeoDistanceCullingVisualizer::DisableForWorld(UWorld* InWorld) const
{
	if (!InWorld)
	{
		return;
	}

	ResetColors(InWorld);

	FActorPrimitiveColorHandler& ColorHandler = FActorPrimitiveColorHandler::Get();

	if (ColorHandler.GetActivePrimitiveColorHandler() == ColorHandlerName)
	{
		ColorHandler.SetActivePrimitiveColorHandler(NAME_None, InWorld);
	}

	SetActorColorationViewportState(InWorld, false);
}

void FFastGeoDistanceCullingVisualizer::RefreshColors(const UWorld* InWorld,
                                                      const UFastGeoDistanceCullCellTransformer* InTransformer) const
{
	if (!InWorld || !InTransformer)
	{
		return;
	}

	const TArray<FCullDistanceSizePair>& CullDistances = InTransformer->GetCullDistances();

	for (ULevel* Level : InWorld->GetLevels())
	{
		if (!Level)
		{
			continue;
		}

		UFastGeoContainer* FastGeo = Level->GetAssetUserData<UFastGeoContainer>();

		if (!FastGeo)
		{
			continue;
		}

		FastGeo->ForEachComponentCluster(
			[this,
				InTransformer,
				&CullDistances](FFastGeoComponentCluster& ComponentCluster)
			{
				ComponentCluster.ForEachComponent<FFastGeoPrimitiveComponent>(
					[this,
						InTransformer,
						&CullDistances](const FFastGeoPrimitiveComponent& Component)
					{
						if (!Component.IsA<FFastGeoStaticMeshComponentBase>())
						{
							return;
						}

						FPrimitiveSceneProxy* SceneProxy = Component.GetSceneProxy();

						if (!SceneProxy)
						{
							return;
						}

						const int32 BucketIndex = FindBucketIndex(SceneProxy, InTransformer, DistanceTolerance);

						if (BucketIndex == INDEX_NONE)
						{
							SceneProxy->SetPrimitiveColor_GameThread(FLinearColor::White);

							return;
						}

						SceneProxy->SetPrimitiveColor_GameThread(
							GetBucketColor(BucketIndex, CullDistances.Num()));
					});
			});
	}
}

void FFastGeoDistanceCullingVisualizer::DrawLegend(UCanvas* Canvas,
                                                   APlayerController* PlayerController) const
{
	if (!Canvas)
	{
		return;
	}

	const UWorld* World = ResolveVisualizationWorld(PlayerController);

	if (!World)
	{
		return;
	}

	const UFastGeoDistanceCullCellTransformer* Transformer = FindTransformer(World);

	if (!Transformer || !ShouldVisualize(Transformer))
	{
		return;
	}

	const TArray<FCullDistanceSizePair>& CullDistances = Transformer->GetCullDistances();

	if (CullDistances.IsEmpty())
	{
		return;
	}

	float CurrentY = StartY;

	FCanvasTextItem Title(
		FVector2D(StartX, CurrentY),
		FText::FromString(TEXT("FastGeo Distance Cull")),
		GEngine->GetMediumFont(),
		FLinearColor::White);

	Title.EnableShadow(FLinearColor::Black);

	Canvas->DrawItem(Title);

	CurrentY += 24.0f;

	FCanvasTextItem Subtitle(
		FVector2D(StartX, CurrentY),
		FText::FromString(
			TEXT("Size = primitive bounds sphere diameter")),
		GEngine->GetSmallFont(),
		FLinearColor::White);

	Subtitle.EnableShadow(FLinearColor::Black);

	Canvas->DrawItem(Subtitle);

	CurrentY += 22.0f;

	for (int32 Index = 0; Index < CullDistances.Num(); ++Index)
	{
		const FCullDistanceSizePair& Pair = CullDistances[Index];

		const FLinearColor Color = GetBucketColor(Index,
		                                          CullDistances.Num());

		FCanvasTileItem ColorTile(
			FVector2D(StartX, CurrentY + 2.0f),
			FVector2D(ColorBoxSize, ColorBoxSize),
			Color);

		ColorTile.BlendMode = SE_BLEND_Translucent;

		Canvas->DrawItem(ColorTile);

		const FString CullDistanceString = Pair.CullDistance > 0.0f
			                                   ? FString::Printf(
				                                   TEXT("%.0f m"),
				                                   Pair.CullDistance / 100.0f)
			                                   : TEXT("Infinite");

		const FString RowText = FString::Printf(
			TEXT("Size %.0f cm  ->  Cull %s"),
			Pair.Size,
			*CullDistanceString);

		FCanvasTextItem TextItem(
			FVector2D(
				StartX + ColorBoxSize + 8.0f,
				CurrentY),
			FText::FromString(RowText),
			GEngine->GetSmallFont(),
			FLinearColor::White);

		TextItem.EnableShadow(FLinearColor::Black);

		Canvas->DrawItem(TextItem);

		CurrentY += RowHeight;
	}
}

UWorld* FFastGeoDistanceCullingVisualizer::ResolveVisualizationWorld(const APlayerController* InPlayerController) const
{
	if (InPlayerController)
	{
		UWorld* PlayerWorld = InPlayerController->GetWorld();

		if (PlayerWorld)
		{
			for (const TPair<TWeakObjectPtr<UWorld>, FWorldVisualizationState>& Pair : WorldStates)
			{
				if (Pair.Key.Get() == PlayerWorld && Pair.Value.bEnabled)
				{
					return PlayerWorld;
				}
			}
		}
	}

	for (const TPair<TWeakObjectPtr<UWorld>, FWorldVisualizationState>& Pair : WorldStates)
	{
		if (!Pair.Value.bEnabled)
		{
			continue;
		}

		if (UWorld* World = Pair.Key.Get())
		{
			return World;
		}
	}

	return nullptr;
}
