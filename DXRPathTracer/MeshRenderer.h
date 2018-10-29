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

#include <Graphics/Model.h>
#include <Graphics/GraphicsTypes.h>
#include <Graphics/Camera.h>
#include <Graphics/ShaderCompilation.h>
#include <Graphics/ShadowHelper.h>
#include <Graphics/SH.h>
#include <Graphics/PostProcessHelper.h>

#include "AppSettings.h"
#include "SharedTypes.h"

using namespace SampleFramework12;

namespace SampleFramework12
{
    struct SkyCache;
}

struct MainPassData
{
    const SkyCache* SkyCache = nullptr;
    const ConstantBuffer* SpotLightBuffer = nullptr;
    const RawBuffer* SpotLightClusterBuffer = nullptr;
};

struct ShadingConstants
{
    Float4Align Float3 SunDirectionWS;
    float CosSunAngularRadius = 0.0f;
    Float4Align Float3 SunIrradiance;
    float SinSunAngularRadius = 0.0f;
    Float4Align Float3 CameraPosWS;

    uint32 NumXTiles = 0;
    uint32 NumXYTiles = 0;
    float NearClip = 0.0f;
    float FarClip = 0.0f;

    Float4Align ShaderSH9Color SkySH;
};

class MeshRenderer
{


public:

    MeshRenderer();

    void Initialize(const Model* sceneModel);
    void Shutdown();

    void CreatePSOs(DXGI_FORMAT mainRTFormat, DXGI_FORMAT depthFormat, uint32 numMSAASamples);
    void DestroyPSOs();

    void RenderMainPass(ID3D12GraphicsCommandList* cmdList, const Camera& camera, const MainPassData& mainPassData);

    void RenderSunShadowDepth(ID3D12GraphicsCommandList* cmdList, const OrthographicCamera& camera);
    void RenderSpotLightShadowDepth(ID3D12GraphicsCommandList* cmdList, const Camera& camera);

    void RenderSunShadowMap(ID3D12GraphicsCommandList* cmdList, const Camera& camera);
    void RenderSpotLightShadowMap(ID3D12GraphicsCommandList* cmdList, const Camera& camera);

    const Float4x4* SpotLightShadowMatrices() const { return spotLightShadowMatrices; }
    const StructuredBuffer& MaterialBuffer() const { return materialBuffer; }

protected:

    void LoadShaders();
    void RenderDepth(ID3D12GraphicsCommandList* cmdList, const Camera& camera, ID3D12PipelineState* pso, uint64 numVisible, const uint32* meshDrawIndices);

    const Model* model = nullptr;

    DepthBuffer sunDepthMap;
    DepthBuffer spotLightDepthMap;
    Float4x4 spotLightShadowMatrices[AppSettings::MaxSpotLights];

    StructuredBuffer materialBuffer;

    CompiledShaderPtr meshVS;
    CompiledShaderPtr meshPS;
    CompiledShaderPtr meshAlphaTestPS;
    ID3D12PipelineState* mainPassPSO = nullptr;
    ID3D12PipelineState* mainPassAlphaTestPSO = nullptr;
    ID3D12RootSignature* mainPassRootSignature = nullptr;

    CompiledShaderPtr meshDepthVS;
    ID3D12PipelineState* depthPSO = nullptr;
    ID3D12PipelineState* sunShadowPSO = nullptr;
    ID3D12PipelineState* spotLightShadowPSO = nullptr;
    ID3D12RootSignature* depthRootSignature = nullptr;

    Array<DirectX::BoundingBox> meshBoundingBoxes;
    Array<uint32> frustumCulledIndices;
    Array<float> meshZDepths;

    SunShadowConstantsDepthMap sunShadowConstants;
};