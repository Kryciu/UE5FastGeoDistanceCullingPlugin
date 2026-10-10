#include "FastGeoDistanceCullingEditor.h"
#include "FastGeoDistanceCullingVisualizer.h"

#define LOCTEXT_NAMESPACE "FFastGeoDistanceCullingEditorModule"

void FFastGeoDistanceCullingEditorModule::StartupModule()
{
	FastGeoDistanceCullVisualizer = MakeUnique<FFastGeoDistanceCullingVisualizer>();
	FastGeoDistanceCullVisualizer->Startup();
}

void FFastGeoDistanceCullingEditorModule::ShutdownModule()
{
	if (FastGeoDistanceCullVisualizer)
	{
		FastGeoDistanceCullVisualizer->Shutdown();
		FastGeoDistanceCullVisualizer.Reset();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFastGeoDistanceCullingEditorModule, FastGeoDistanceCullingEditor)
