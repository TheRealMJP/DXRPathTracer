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
#include "MeshRenderer.h"

using namespace SampleFramework12;

class DXRPathTracer : public App
{

protected:

    FirstPersonCamera camera;

    Skybox skybox;
    SkyCache skyCache;

    PostProcessor postProcessor;

    // Model
    Model sceneModels[uint64(Scenes::NumValues)];
    const Model* currentModel = nullptr;
    MeshRenderer meshRenderer;

    RenderTexture mainTarget;
    RenderTexture resolveTarget;
    RenderTexture deferredMSAATarget;
    DepthBuffer depthBuffer;

    Array<SpotLight> spotLights;
    ConstantBuffer spotLightBuffer;
    StructuredBuffer spotLightBoundsBuffer;
    StructuredBuffer spotLightInstanceBuffer;
    RawBuffer spotLightClusterBuffer;
    uint64 numIntersectingSpotLights = 0;

    ID3D12RootSignature* clusterRS = nullptr;
    CompiledShaderPtr clusterVS;
    CompiledShaderPtr clusterFrontFacePS;
    CompiledShaderPtr clusterBackFacePS;
    CompiledShaderPtr clusterIntersectingPS;
    ID3D12PipelineState* clusterFrontFacePSO = nullptr;
    ID3D12PipelineState* clusterBackFacePSO = nullptr;
    ID3D12PipelineState* clusterIntersectingPSO = nullptr;
    RenderTexture clusterMSAATarget;

    StructuredBuffer spotLightClusterVtxBuffer;
    FormattedBuffer spotLightClusterIdxBuffer;
    Array<Float3> coneVertices;

    CompiledShaderPtr fullScreenTriVS;
    CompiledShaderPtr resolvePS[NumMSAAModes];
    ID3D12RootSignature* resolveRootSignature = nullptr;
    ID3D12PipelineState* resolvePSO = nullptr;

    // Ray tracing resources
    CompiledShaderPtr rayTraceLib;
    RenderTexture rtTarget;
    ID3D12RootSignature* rtRootSignature = nullptr;
    ID3D12RootSignature* rtEmptyLocalRS = nullptr;
    ID3D12RootSignature* rtHitGroupLocalRS = nullptr;
    ID3D12StateObject* rtPSO = nullptr;
    bool buildAccelStructure = true;
    uint64 lastBuildAccelStructureFrame = uint64(-1);
    RawBuffer rtBottomLevelAccelStructure;
    RawBuffer rtTopLevelAccelStructure;
    StructuredBuffer rtRayGenTable;
    StructuredBuffer rtHitTable;
    StructuredBuffer rtMissTable;
    StructuredBuffer rtGeoInfoBuffer;
    FirstPersonCamera rtCurrCamera;
    bool rtShouldRestartPathTrace = false;
    uint32 rtCurrSampleIdx = 0;


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

    void InitRayTracing();
    void CreateRayTracingPSOs();

    void UpdateLights();

    void RenderClusters();
    void RenderForward();
    void RenderResolve();
    void RenderRayTracing();
    void RenderHUD(const Timer& timer);

    void BuildRTAccelerationStructure();

public:

    DXRPathTracer(const wchar* cmdLine);
};
