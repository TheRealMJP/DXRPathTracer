//=================================================================================================
//
//  DXR Path Tracer
//  by MJP
//  http://mynameismjp.wordpress.com/
//
//  All code and content licensed under the MIT license
//
//=================================================================================================

#pragma once

#include <PCH.h>

#include <App.h>
#include <InterfacePointers.h>
#include <Input.h>
#include <Graphics/Camera.h>
#include <Graphics/Model.h>
#include <Graphics/Skybox.h>
#include <Graphics/GraphicsTypes.h>

#include "PostProcessor.h"
#include "SharedTypes.h"

using namespace SampleFramework12;

enum class MaterialPreset : int32_t
{
    Default = 0,
    Gold,

    NumPresets
};


class DXRPathTracer : public App
{

protected:

    FirstPersonCamera camera;

    Skybox skybox;
    SkyCache skyCache;

    PostProcessor postProcessor;

    // Model
    Model sceneModels[uint64_t(Scenes::NumValues)];
    const Model* currentModel = nullptr;
    StructuredBuffer materialBuffer;

    RenderTexture rtTarget;
    RenderTexture rtDepthTarget;
    DepthBuffer depthBuffer;

    Array<SpotLight> spotLights;
    ConstantBuffer spotLightBuffer;

    bool32 stablePowerState = false;

    // Ray tracing resources
    CompiledShaderPtr rayTraceLib;
    ID3D12StateObject* rtPSO = nullptr;
    bool buildAccelStructure = true;
    uint64_t lastBuildAccelStructureFrame = uint64_t(-1);
    RTAccelStructure rtBottomLevelAccelStructure;
    RTAccelStructure rtTopLevelAccelStructure;
    StructuredBuffer rtRayGenTable;
    StructuredBuffer rtHitTable;
    StructuredBuffer rtMissTable;
    StructuredBuffer rtGeoInfoBuffer;
    FirstPersonCamera rtCurrCamera;
    bool rtShouldRestartPathTrace = false;
    uint32_t rtCurrSampleIdx = 0;

    Material knobMaterial;

    Texture whiteTexture;
    Texture blackTexture;
    Texture checkerTexture;
    Texture flatNormalMap;

    Texture goldBaseColor;
    Texture goldNormalMap;
    Texture goldRoughness;

    MaterialPreset materialPreset = MaterialPreset::Default;
    Material presets[int32_t(MaterialPreset::NumPresets)];

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void Render(const Timer& timer) override;
    virtual void Update(const Timer& timer) override;

    virtual void BeforeReset() override;
    virtual void AfterReset() override;

    virtual void CreatePSOs() override;
    virtual void DestroyPSOs() override;

    void CreateRenderTargets();
    void InitializeScene();

    void CreateRayTracingPSOs();

    void RenderRayTracing();
    void RenderHUD(const Timer& timer);

    void BuildRTAccelerationStructure();

public:

    DXRPathTracer(const char* cmdLine);
};
