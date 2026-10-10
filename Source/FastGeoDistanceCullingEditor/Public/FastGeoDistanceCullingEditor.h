#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FFastGeoDistanceCullingVisualizer;

class FFastGeoDistanceCullingEditorModule : public IModuleInterface
{
    TUniquePtr<FFastGeoDistanceCullingVisualizer> FastGeoDistanceCullVisualizer;
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
