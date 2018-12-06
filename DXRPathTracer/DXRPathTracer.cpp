//=================================================================================================
//
//  DXR Path Tracer
//  by MJP
//  http://mynameismjp.wordpress.com/
//
//  All code and content licensed under the MIT license
//
//=================================================================================================

#include <PCH.h>

#include <InterfacePointers.h>
#include <Window.h>
#include <Input.h>
#include <Utility.h>
#include <Graphics/SwapChain.h>
#include <Graphics/ShaderCompilation.h>
#include <Graphics/Profiler.h>
#include <Graphics/Textures.h>
#include <Graphics/Sampling.h>
#include <Graphics/DX12.h>
#include <Graphics/DX12_Helpers.h>
#include <Graphics/DXRHelper.h>
#include <EnkiTS/TaskScheduler_c.h>
#include <ImGui/ImGui.h>
#include <ImGuiHelper.h>

#include "DXRPathTracer.h"
#include "SharedTypes.h"

using namespace SampleFramework12;

// Model filenames
static const wchar* ScenePaths[] =
{
    L"..\\Content\\Models\\Sponza\\Sponza.fbx",
    L"..\\Content\\Models\\SunTemple\\SunTemple.fbx",
    nullptr,
};

static const wchar* SceneTextureDirs[] = { nullptr, L"Textures", nullptr };
static const float SceneScales[] = { 0.01f, 0.005f, 1.0f };
static const Float3 SceneCameraPositions[] = { Float3(-11.5f, 1.85f, -0.45f), Float3(-1.0f, 5.5f, 12.0f), Float3(0.0f, 2.5f, -10.0f) };
static const Float2 SceneCameraRotations[] = { Float2(0.0f, 1.544f), Float2(0.2f, 3.0f), Float2(0.0f, 0.0f) };
static const Float3 SceneSunDirections[] = { Float3(0.26f, 0.987f, -0.16f), Float3(-0.133022308f, 0.642787635f, 0.75440651f), Float3(0.26f, 0.987f, -0.16f) };

StaticAssert_(ArraySize_(ScenePaths) == uint64(Scenes::NumValues));
StaticAssert_(ArraySize_(SceneTextureDirs) == uint64(Scenes::NumValues));
StaticAssert_(ArraySize_(SceneScales) == uint64(Scenes::NumValues));
StaticAssert_(ArraySize_(SceneCameraPositions) == uint64(Scenes::NumValues));
StaticAssert_(ArraySize_(SceneCameraRotations) == uint64(Scenes::NumValues));
StaticAssert_(ArraySize_(SceneSunDirections) == uint64(Scenes::NumValues));

static const uint64 NumConeSides = 16;

struct HitGroupRecord
{
    ShaderIdentifier ID;
    uint32 GeometryIdx = 0;
    uint8 Padding[28] = { };
};

StaticAssert_(sizeof(HitGroupRecord) % D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT == 0);

struct LightConstants
{
    SpotLight Lights[AppSettings::MaxSpotLights];
    Float4x4 ShadowMatrices[AppSettings::MaxSpotLights];
};

struct ClusterConstants
{
    Float4x4 ViewProjection;
    Float4x4 InvProjection;
    float NearClip = 0.0f;
    float FarClip = 0.0f;
    float InvClipRange = 0.0f;
    uint32 NumXTiles = 0;
    uint32 NumYTiles = 0;
    uint32 NumXYTiles = 0;
    uint32 ElementsPerCluster = 0;
    uint32 InstanceOffset = 0;
    uint32 NumLights = 0;

    uint32 BoundsBufferIdx = uint32(-1);
    uint32 VertexBufferIdx = uint32(-1);
    uint32 InstanceBufferIdx = uint32(-1);
};

struct RayTraceConstants
{
    Float4x4 InvViewProjection;

    Float3 SunDirectionWS;
    float CosSunAngularRadius = 0.0f;
    Float3 SunIrradiance;
    float SinSunAngularRadius = 0.0f;
    Float3 SunRenderColor;
    uint32 Padding = 0;
    Float3 CameraPosWS;
    uint32 CurrSampleIdx = 0;
    uint32 TotalNumPixels = 0;

    uint32 VtxBufferIdx = uint32(-1);
    uint32 IdxBufferIdx = uint32(-1);
    uint32 GeometryInfoBufferIdx = uint32(-1);
    uint32 MaterialBufferIdx = uint32(-1);
    uint32 SkyTextureIdx = uint32(-1);
};

enum ClusterRootParams : uint32
{
    ClusterParams_StandardDescriptors,
    ClusterParams_UAVDescriptors,
    ClusterParams_CBuffer,
    ClusterParams_AppSettings,

    NumClusterRootParams,
};

enum ResolveRootParams : uint32
{
    ResolveParams_StandardDescriptors,
    ResolveParams_Constants,
    ResolveParams_AppSettings,

    NumResolveRootParams
};

enum RTRootParams : uint32
{
    RTParams_StandardDescriptors,
    RTParams_SceneDescriptor,
    RTParams_UAVDescriptor,
    RTParams_CBuffer,
    RTParams_AppSettings,

    NumRTRootParams
};

enum RTHitGroupRootParams : uint32
{
    RTHitGroupParams_GeometryID,

    NumRTHitGroupRootParams
};

// Returns true if a sphere intersects a capped cone defined by a direction, height, and angle
static bool SphereConeIntersection(const Float3& coneTip, const Float3& coneDir, float coneHeight,
                                   float coneAngle, const Float3& sphereCenter, float sphereRadius)
{
    if(Float3::Dot(sphereCenter - coneTip, coneDir) > coneHeight + sphereRadius)
        return false;

    float cosHalfAngle = std::cos(coneAngle * 0.5f);
    float sinHalfAngle = std::sin(coneAngle * 0.5f);

    Float3 v = sphereCenter - coneTip;
    float a = Float3::Dot(v, coneDir);
    float b = a * sinHalfAngle / cosHalfAngle;
    float c = std::sqrt(Float3::Dot(v, v) - a * a);
    float d = c - b;
    float e = d * cosHalfAngle;

    return e < sphereRadius;
}

DXRPathTracer::DXRPathTracer(const wchar* cmdLine) : App(L"DXR Path Tracer", cmdLine)
{
    minFeatureLevel = D3D_FEATURE_LEVEL_11_1;
    globalHelpText = "DXR Path Tracer\n\n"
                     "Controls:\n\n"
                     "Use W/S/A/D/Q/E to move the camera, and hold right-click while dragging the mouse to rotate.";
}

void DXRPathTracer::BeforeReset()
{
}

void DXRPathTracer::AfterReset()
{
    float aspect = float(swapChain.Width()) / swapChain.Height();
    camera.SetAspectRatio(aspect);

    CreateRenderTargets();
}

void DXRPathTracer::Initialize()
{
    // Check if the device supports conservative rasterization
    D3D12_FEATURE_DATA_D3D12_OPTIONS features = { };
    DX12::Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &features, sizeof(features));
    if(features.ResourceBindingTier < D3D12_RESOURCE_BINDING_TIER_2)
        throw Exception("This demo requires a GPU that supports FEATURE_LEVEL_11_1 with D3D12_RESOURCE_BINDING_TIER_2");

    if(features.ConservativeRasterizationTier == D3D12_CONSERVATIVE_RASTERIZATION_TIER_NOT_SUPPORTED)
    {
        AppSettings::ClusterRasterizationMode.SetValue(ClusterRasterizationModes::MSAA8x);
        AppSettings::ClusterRasterizationMode.ClampNumValues(uint32(ClusterRasterizationModes::NumValues) - 1);
    }

    float aspect = float(swapChain.Width()) / swapChain.Height();
    camera.Initialize(aspect, Pi_4, 0.1f, 100.0f);

    ShadowHelper::Initialize(ShadowMapMode::DepthMap, ShadowMSAAMode::MSAA1x);

    InitializeScene();

    skybox.Initialize();

    postProcessor.Initialize();

    {
        // Spot light bounds and instance buffers
        StructuredBufferInit sbInit;
        sbInit.Stride = sizeof(ClusterBounds);
        sbInit.NumElements = AppSettings::MaxSpotLights;
        sbInit.Dynamic = true;
        sbInit.CPUAccessible = true;
        spotLightBoundsBuffer.Initialize(sbInit);

        sbInit.Stride = sizeof(uint32);
        spotLightInstanceBuffer.Initialize(sbInit);
    }

    {
        // Spot light and shadow bounds buffer
        ConstantBufferInit cbInit;
        cbInit.Size = sizeof(LightConstants);
        cbInit.Dynamic = true;
        cbInit.CPUAccessible = false;
        cbInit.InitialState = D3D12_RESOURCE_STATE_COMMON;
        cbInit.Name = L"Spot Light Buffer";

        spotLightBuffer.Initialize(cbInit);
    }

    {
        CompileOptions opts;
        opts.Add("FrontFace_", 1);
        opts.Add("BackFace_", 0);
        opts.Add("Intersecting_", 0);

        // Clustering shaders
        clusterVS = CompileFromFile(L"Clusters.hlsl", "ClusterVS", ShaderType::Vertex, opts);
        clusterFrontFacePS = CompileFromFile(L"Clusters.hlsl", "ClusterPS", ShaderType::Pixel, opts);

        opts.Reset();
        opts.Add("FrontFace_", 0);
        opts.Add("BackFace_", 1);
        opts.Add("Intersecting_", 0);
        clusterBackFacePS = CompileFromFile(L"Clusters.hlsl", "ClusterPS", ShaderType::Pixel, opts);

        opts.Reset();
        opts.Add("FrontFace_", 0);
        opts.Add("BackFace_", 0);
        opts.Add("Intersecting_", 1);
        clusterIntersectingPS = CompileFromFile(L"Clusters.hlsl", "ClusterPS", ShaderType::Pixel, opts);
    }

    MakeConeGeometry(NumConeSides, spotLightClusterVtxBuffer, spotLightClusterIdxBuffer, coneVertices);

    // Compile resolve shaders
    for(uint64 msaaMode = 1; msaaMode < NumMSAAModes; ++msaaMode)
    {
        for(uint64 deferred = 0; deferred < 2; ++deferred)
        {
            CompileOptions opts;
            opts.Add("MSAASamples_", AppSettings::NumMSAASamples(MSAAModes(msaaMode)));
            resolvePS[msaaMode] = CompileFromFile(L"Resolve.hlsl", "ResolvePS", ShaderType::Pixel, opts);
        }
    }

    std::wstring fullScreenTriPath = SampleFrameworkDir() + L"Shaders\\FullScreenTriangle.hlsl";
    fullScreenTriVS = CompileFromFile(fullScreenTriPath.c_str(), "FullScreenTriangleVS", ShaderType::Vertex);

    {
        // Clustering root signature
        D3D12_DESCRIPTOR_RANGE1 uavRanges[1] = {};
        uavRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRanges[0].NumDescriptors = 1;
        uavRanges[0].BaseShaderRegister = 0;
        uavRanges[0].RegisterSpace = 0;
        uavRanges[0].OffsetInDescriptorsFromTableStart = 0;

        D3D12_ROOT_PARAMETER1 rootParameters[NumClusterRootParams] = {};

        // Standard SRV descriptors
        rootParameters[ClusterParams_StandardDescriptors].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[ClusterParams_StandardDescriptors].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParameters[ClusterParams_StandardDescriptors].DescriptorTable.pDescriptorRanges = DX12::StandardDescriptorRanges();
        rootParameters[ClusterParams_StandardDescriptors].DescriptorTable.NumDescriptorRanges = DX12::NumStandardDescriptorRanges;

        // PS UAV descriptors
        rootParameters[ClusterParams_UAVDescriptors].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[ClusterParams_UAVDescriptors].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[ClusterParams_UAVDescriptors].DescriptorTable.pDescriptorRanges = uavRanges;
        rootParameters[ClusterParams_UAVDescriptors].DescriptorTable.NumDescriptorRanges = ArraySize_(uavRanges);

        // CBuffer
        rootParameters[ClusterParams_CBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[ClusterParams_CBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[ClusterParams_CBuffer].Descriptor.RegisterSpace = 0;
        rootParameters[ClusterParams_CBuffer].Descriptor.ShaderRegister = 0;
        rootParameters[ClusterParams_CBuffer].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

        // AppSettings
        rootParameters[ClusterParams_AppSettings].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[ClusterParams_AppSettings].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[ClusterParams_AppSettings].Descriptor.RegisterSpace = 0;
        rootParameters[ClusterParams_AppSettings].Descriptor.ShaderRegister = AppSettings::CBufferRegister;
        rootParameters[ClusterParams_AppSettings].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

        D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
        rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.NumStaticSamplers = 0;
        rootSignatureDesc.pStaticSamplers = nullptr;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        DX12::CreateRootSignature(&clusterRS, rootSignatureDesc);
    }

    {
        // Resolve root signature
        D3D12_ROOT_PARAMETER1 rootParameters[NumResolveRootParams] = {};

        // Standard SRV descriptors
        rootParameters[ResolveParams_StandardDescriptors].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[ResolveParams_StandardDescriptors].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[ResolveParams_StandardDescriptors].DescriptorTable.pDescriptorRanges = DX12::StandardDescriptorRanges();
        rootParameters[ResolveParams_StandardDescriptors].DescriptorTable.NumDescriptorRanges = DX12::NumStandardDescriptorRanges;

        // CBuffer
        rootParameters[ResolveParams_Constants].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParameters[ResolveParams_Constants].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[ResolveParams_Constants].Constants.Num32BitValues = 3;
        rootParameters[ResolveParams_Constants].Constants.RegisterSpace = 0;
        rootParameters[ResolveParams_Constants].Constants.ShaderRegister = 0;

        // AppSettings
        rootParameters[ResolveParams_AppSettings].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[ResolveParams_AppSettings].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[ResolveParams_AppSettings].Descriptor.RegisterSpace = 0;
        rootParameters[ResolveParams_AppSettings].Descriptor.ShaderRegister = AppSettings::CBufferRegister;
        rootParameters[ResolveParams_AppSettings].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

        D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
        rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.NumStaticSamplers = 0;
        rootSignatureDesc.pStaticSamplers = nullptr;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        DX12::CreateRootSignature(&resolveRootSignature, rootSignatureDesc);
    }

    InitRayTracing();
}

void DXRPathTracer::Shutdown()
{
    ShadowHelper::Shutdown();

    for(uint64 i = 0; i < ArraySize_(sceneModels); ++i)
        sceneModels[i].Shutdown();

    meshRenderer.Shutdown();
    skybox.Shutdown();
    skyCache.Shutdown();
    postProcessor.Shutdown();

    spotLightBuffer.Shutdown();
    spotLightBoundsBuffer.Shutdown();
    spotLightClusterBuffer.Shutdown();
    spotLightInstanceBuffer.Shutdown();

    DX12::Release(clusterRS);
    clusterMSAATarget.Shutdown();

    spotLightClusterVtxBuffer.Shutdown();
    spotLightClusterIdxBuffer.Shutdown();

    mainTarget.Shutdown();
    resolveTarget.Shutdown();
    depthBuffer.Shutdown();

    DX12::Release(resolveRootSignature);

    rtTarget.Shutdown();
    DX12::Release(rtRootSignature);
    DX12::Release(rtEmptyLocalRS);
    DX12::Release(rtHitGroupLocalRS);
    rtBottomLevelAccelStructure.Shutdown();
    rtTopLevelAccelStructure.Shutdown();
    rtRayGenTable.Shutdown();
    rtHitTable.Shutdown();
    rtMissTable.Shutdown();
    rtGeoInfoBuffer.Shutdown();
}

void DXRPathTracer::CreatePSOs()
{
    meshRenderer.CreatePSOs(mainTarget.Texture.Format, depthBuffer.DSVFormat, mainTarget.MSAASamples);
    skybox.CreatePSOs(mainTarget.Texture.Format, depthBuffer.DSVFormat, mainTarget.MSAASamples);
    postProcessor.CreatePSOs();

    {
        // Clustering PSO
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = clusterRS;
        psoDesc.BlendState = DX12::GetBlendState(BlendState::Disabled);
        psoDesc.DepthStencilState = DX12::GetDepthState(DepthState::Disabled);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 0;
        psoDesc.VS = clusterVS.ByteCode();

        ClusterRasterizationModes rastMode = AppSettings::ClusterRasterizationMode;
        if(rastMode == ClusterRasterizationModes::MSAA4x || rastMode == ClusterRasterizationModes::MSAA8x)
        {
            psoDesc.SampleDesc.Count = clusterMSAATarget.MSAASamples;
            psoDesc.SampleDesc.Quality = DX12::StandardMSAAPattern;
            psoDesc.NumRenderTargets = 1;
            psoDesc.RTVFormats[0] = clusterMSAATarget.Format();
        }
        else
            psoDesc.SampleDesc.Count = 1;

        D3D12_CONSERVATIVE_RASTERIZATION_MODE crMode = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        if(rastMode == ClusterRasterizationModes::Conservative)
            crMode = D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;

        psoDesc.PS = clusterFrontFacePS.ByteCode();
        psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::BackFaceCull);
        psoDesc.RasterizerState.ConservativeRaster = crMode;
        DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&clusterFrontFacePSO)));

        psoDesc.PS = clusterBackFacePS.ByteCode();
        psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::FrontFaceCull);
        psoDesc.RasterizerState.ConservativeRaster = crMode;
        DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&clusterBackFacePSO)));

        psoDesc.PS = clusterIntersectingPS.ByteCode();
        psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::FrontFaceCull);
        psoDesc.RasterizerState.ConservativeRaster = crMode;
        DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&clusterIntersectingPSO)));

        clusterFrontFacePSO->SetName(L"Cluster Front-Face PSO");
        clusterBackFacePSO->SetName(L"Cluster Back-Face PSO");
        clusterIntersectingPSO->SetName(L"Cluster Intersecting PSO");
    }

    const bool msaaEnabled = AppSettings::MSAAMode != MSAAModes::MSAANone;
    const uint64 msaaModeIdx = uint64(AppSettings::MSAAMode);

    if(msaaEnabled)
    {
        // Resolve PSO
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = resolveRootSignature;
        psoDesc.VS = fullScreenTriVS.ByteCode();
        psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::NoCull);
        psoDesc.BlendState = DX12::GetBlendState(BlendState::Disabled);
        psoDesc.DepthStencilState = DX12::GetDepthState(DepthState::Disabled);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = mainTarget.Format();
        psoDesc.SampleDesc.Count = 1;

        psoDesc.PS = resolvePS[msaaModeIdx].ByteCode();
        DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&resolvePSO)));
    }

    CreateRayTracingPSOs();
}

void DXRPathTracer::DestroyPSOs()
{
    meshRenderer.DestroyPSOs();
    skybox.DestroyPSOs();
    postProcessor.DestroyPSOs();
    DX12::DeferredRelease(clusterFrontFacePSO);
    DX12::DeferredRelease(clusterBackFacePSO);
    DX12::DeferredRelease(clusterIntersectingPSO);
    DX12::DeferredRelease(resolvePSO);

    DX12::DeferredRelease(rtPSO);
}

// Creates all required render targets
void DXRPathTracer::CreateRenderTargets()
{
    uint32 width = swapChain.Width();
    uint32 height = swapChain.Height();
    const uint32 NumSamples = AppSettings::NumMSAASamples();

     {
        RenderTextureInit rtInit;
        rtInit.Width = width;
        rtInit.Height = height;
        rtInit.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        rtInit.MSAASamples = NumSamples;
        rtInit.ArraySize = 1;
        rtInit.CreateUAV = NumSamples == 1;
        rtInit.InitialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        rtInit.Name = L"Main Target";
        mainTarget.Initialize(rtInit);
    }

    if(NumSamples > 1)
    {
        RenderTextureInit rtInit;
        rtInit.Width = width;
        rtInit.Height = height;
        rtInit.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        rtInit.MSAASamples = 1;
        rtInit.ArraySize = 1;
        rtInit.CreateUAV = false;
        rtInit.InitialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        rtInit.Name = L"Resolve Target";
        resolveTarget.Initialize(rtInit);
    }

    {
        DepthBufferInit dbInit;
        dbInit.Width = width;
        dbInit.Height = height;
        dbInit.Format = DXGI_FORMAT_D32_FLOAT;
        dbInit.MSAASamples = NumSamples;
        dbInit.Name = L"Main Depth Buffer";
        depthBuffer.Initialize(dbInit);
    }

    AppSettings::NumXTiles = (width + (AppSettings::ClusterTileSize - 1)) / AppSettings::ClusterTileSize;
    AppSettings::NumYTiles = (height + (AppSettings::ClusterTileSize - 1)) / AppSettings::ClusterTileSize;
    const uint64 numXYZTiles = AppSettings::NumXTiles * AppSettings::NumYTiles * AppSettings::NumZTiles;

    {
        // Render target for forcing MSAA during cluster rasterization. Ideally we would use ForcedSampleCount for this,
        // but it's currently causing the Nvidia driver to crash. :(
        RenderTextureInit rtInit;
        rtInit.Width = AppSettings::NumXTiles;
        rtInit.Height = AppSettings::NumYTiles;
        rtInit.Format = DXGI_FORMAT_R8_UNORM;
        rtInit.MSAASamples = 1;
        rtInit.ArraySize = 1;
        rtInit.CreateUAV = false;
        rtInit.Name = L"Deferred MSAA Target";

        ClusterRasterizationModes rastMode = AppSettings::ClusterRasterizationMode;
        if(rastMode == ClusterRasterizationModes::MSAA4x)
        {
            rtInit.MSAASamples = 4;
            clusterMSAATarget.Initialize(rtInit);
        }
        else if(rastMode == ClusterRasterizationModes::MSAA8x)
        {
            rtInit.MSAASamples = 8;
            clusterMSAATarget.Initialize(rtInit);
        }
        else
            clusterMSAATarget.Shutdown();
    }

    {
        // Spotlight cluster bitmask buffer
        RawBufferInit rbInit;
        rbInit.NumElements = numXYZTiles * AppSettings::SpotLightElementsPerCluster;
        rbInit.CreateUAV = true;
        rbInit.Name = L"Spot Light Cluster Buffer";
        spotLightClusterBuffer.Initialize(rbInit);
    }

    {
        RenderTextureInit rtInit;
        rtInit.Width = width;
        rtInit.Height = height;
        rtInit.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        rtInit.CreateUAV = true;
        rtInit.Name = L"RT Target";
        rtTarget.Initialize(rtInit);
    }

    rtShouldRestartPathTrace = true;
}

void DXRPathTracer::InitializeScene()
{
    const uint64 currSceneIdx = uint64(AppSettings::CurrentScene);

    // Load the scene (if necessary)
    if(sceneModels[currSceneIdx].NumMeshes() == 0)
    {
        if(currSceneIdx == uint64(Scenes::BoxTest) || ScenePaths[currSceneIdx] == nullptr)
        {
            sceneModels[currSceneIdx].GenerateBoxTestScene();
        }
        else
        {
            ModelLoadSettings settings;
            settings.FilePath = ScenePaths[currSceneIdx];
            settings.TextureDir = SceneTextureDirs[currSceneIdx];
            settings.ForceSRGB = true;
            settings.SceneScale = SceneScales[currSceneIdx];
            settings.MergeMeshes = false;
            sceneModels[currSceneIdx].CreateWithAssimp(settings);
        }
    }

    currentModel = &sceneModels[currSceneIdx];
    meshRenderer.Shutdown();
    DX12::FlushGPU();
    meshRenderer.Initialize(currentModel);

    const uint64 numMaterialTextures = currentModel->MaterialTextures().Count();

    camera.SetPosition(SceneCameraPositions[currSceneIdx]);
    camera.SetXRotation(SceneCameraRotations[currSceneIdx].x);
    camera.SetYRotation(SceneCameraRotations[currSceneIdx].y);
    AppSettings::SunDirection.SetValue(SceneSunDirections[currSceneIdx]);

    {
        // Initialize the spotlight data used for rendering
        const uint64 numSpotLights = Min(currentModel->SpotLights().Size(), AppSettings::MaxSpotLights);
        spotLights.Init(numSpotLights);

        for(uint64 i = 0; i < numSpotLights; ++i)
        {
            const ModelSpotLight& srcLight = currentModel->SpotLights()[i];

            SpotLight& spotLight = spotLights[i];
            spotLight.Position = srcLight.Position;
            spotLight.Direction = -srcLight.Direction;
            spotLight.Intensity = srcLight.Intensity * 2500.0f;
            spotLight.AngularAttenuationX = std::cos(srcLight.AngularAttenuation.x * 0.5f);
            spotLight.AngularAttenuationY = std::cos(srcLight.AngularAttenuation.y * 0.5f);
            spotLight.Range = AppSettings::SpotLightRange;
        }
    }

    buildAccelStructure = true;
}

void DXRPathTracer::InitRayTracing()
{
    rayTraceLib = CompileFromFile(L"RayTrace.hlsl", nullptr, ShaderType::Library);

    {
        // RayTrace root signature
        D3D12_DESCRIPTOR_RANGE1 uavRanges[1] = {};
        uavRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRanges[0].NumDescriptors = 1;
        uavRanges[0].BaseShaderRegister = 0;
        uavRanges[0].RegisterSpace = 0;
        uavRanges[0].OffsetInDescriptorsFromTableStart = 0;

        D3D12_ROOT_PARAMETER1 rootParameters[NumRTRootParams] = {};

        // Standard SRV descriptors
        rootParameters[RTParams_StandardDescriptors].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[RTParams_StandardDescriptors].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[RTParams_StandardDescriptors].DescriptorTable.pDescriptorRanges = DX12::StandardDescriptorRanges();
        rootParameters[RTParams_StandardDescriptors].DescriptorTable.NumDescriptorRanges = DX12::NumStandardDescriptorRanges;

        // Acceleration structure SRV descriptor
        rootParameters[RTParams_SceneDescriptor].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        rootParameters[RTParams_SceneDescriptor].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[RTParams_SceneDescriptor].Descriptor.ShaderRegister = 0;
        rootParameters[RTParams_SceneDescriptor].Descriptor.RegisterSpace = 200;

        // UAV descriptor
        rootParameters[RTParams_UAVDescriptor].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[RTParams_UAVDescriptor].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[RTParams_UAVDescriptor].DescriptorTable.pDescriptorRanges = uavRanges;
        rootParameters[RTParams_UAVDescriptor].DescriptorTable.NumDescriptorRanges = ArraySize_(uavRanges);

        // CBuffer
        rootParameters[RTParams_CBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[RTParams_CBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[RTParams_CBuffer].Descriptor.RegisterSpace = 0;
        rootParameters[RTParams_CBuffer].Descriptor.ShaderRegister = 0;
        rootParameters[RTParams_CBuffer].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

        // AppSettings
        rootParameters[RTParams_AppSettings].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[RTParams_AppSettings].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[RTParams_AppSettings].Descriptor.RegisterSpace = 0;
        rootParameters[RTParams_AppSettings].Descriptor.ShaderRegister = AppSettings::CBufferRegister;
        rootParameters[RTParams_AppSettings].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

        D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};
        staticSamplers[0] = DX12::GetStaticSamplerState(SamplerState::Anisotropic, 0, 0, D3D12_SHADER_VISIBILITY_ALL);
        staticSamplers[1] = DX12::GetStaticSamplerState(SamplerState::LinearClamp, 1, 0, D3D12_SHADER_VISIBILITY_ALL);

        D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
        rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.NumStaticSamplers = ArraySize_(staticSamplers);
        rootSignatureDesc.pStaticSamplers = staticSamplers;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        DX12::CreateRootSignature(&rtRootSignature, rootSignatureDesc);
    }

    {
        // (empty) local root signature
        D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
        DX12::CreateRootSignature(&rtEmptyLocalRS, rootSignatureDesc);
    }

    {
        // Local root signature for the hit group
        D3D12_ROOT_PARAMETER1 rootParameters[NumRTHitGroupRootParams] = {};

        rootParameters[RTHitGroupParams_GeometryID].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParameters[RTHitGroupParams_GeometryID].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[RTHitGroupParams_GeometryID].Constants.Num32BitValues = 1;
        rootParameters[RTHitGroupParams_GeometryID].Constants.RegisterSpace = 200;
        rootParameters[RTHitGroupParams_GeometryID].Constants.ShaderRegister = 0;

        D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
        rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

        DX12::CreateRootSignature(&rtHitGroupLocalRS, rootSignatureDesc);
    }

    rtCurrCamera = camera;
}

void DXRPathTracer::CreateRayTracingPSOs()
{
    StateObjectBuilder builder;
    builder.Init(10);

    {
        // DXIL library sub-object containing all of our code
        D3D12_DXIL_LIBRARY_DESC dxilDesc = { };
        dxilDesc.DXILLibrary = rayTraceLib.ByteCode();
        builder.AddSubObject(dxilDesc);
    }

    {
        // Primary hit group
        D3D12_HIT_GROUP_DESC hitDesc = { };
        hitDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
        hitDesc.ClosestHitShaderImport = L"ClosestHitShader";
        hitDesc.HitGroupExport = L"HitGroup";
        builder.AddSubObject(hitDesc);
    }

    {
        // Shadow hit group
        D3D12_HIT_GROUP_DESC hitDesc = { };
        hitDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
        hitDesc.ClosestHitShaderImport = L"ShadowHitShader";
        hitDesc.HitGroupExport = L"ShadowHitGroup";
        builder.AddSubObject(hitDesc);
    }

    {
        D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = { };
        shaderConfig.MaxAttributeSizeInBytes = 2 * sizeof(float);                      // float2 barycentrics;
        shaderConfig.MaxPayloadSizeInBytes = 3 * sizeof(float) + 3 * sizeof(uint32);   // float3 radiance + uint pathLength + uint pixelIdx + uint setIdx
        builder.AddSubObject(shaderConfig);
    }

    {
        // Local (empty) root signature used in our ray gen shader and miss shaders
        D3D12_LOCAL_ROOT_SIGNATURE localRSDesc = { };
        localRSDesc.pLocalRootSignature = rtEmptyLocalRS;
        const D3D12_STATE_SUBOBJECT* localRSSubObj = builder.AddSubObject(localRSDesc);

        static const wchar* exports[] =
        {
            L"RaygenShader",
            L"MissShader",
            L"ShadowHitGroup",
            L"ShadowMissShader",
        };

        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION associations = { };
        associations.pSubobjectToAssociate = localRSSubObj;
        associations.NumExports = ArraySize_(exports);
        associations.pExports = exports;
        builder.AddSubObject(associations);
    }

    {
        // Local root signature to be used in the primary hit group. Passes the geometry index as a root parameter.
        D3D12_LOCAL_ROOT_SIGNATURE localRSDesc = { };
        localRSDesc.pLocalRootSignature = rtHitGroupLocalRS;
        const D3D12_STATE_SUBOBJECT* localRSSubObj = builder.AddSubObject(localRSDesc);

        static const wchar* exports[] =
        {
            L"HitGroup",
        };

        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION associations = { };
        associations.pSubobjectToAssociate = localRSSubObj;
        associations.NumExports = ArraySize_(exports);
        associations.pExports = exports;
        builder.AddSubObject(associations);
    }

    {
        // Global root signature with all of our normal bindings
        D3D12_GLOBAL_ROOT_SIGNATURE globalRSDesc = { };
        globalRSDesc.pGlobalRootSignature = rtRootSignature;
        builder.AddSubObject(globalRSDesc);
    }

    {
        // The path tracer is recursive, so set the max recursion depth to the max path length
        D3D12_RAYTRACING_PIPELINE_CONFIG configDesc = { };
        configDesc.MaxTraceRecursionDepth = AppSettings::MaxPathLengthSetting;
        builder.AddSubObject(configDesc);
    }

    rtPSO = builder.CreateStateObject(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

    // Get shader identifiers (for making shader records)
    ID3D12StateObjectProperties* psoProps = nullptr;
    rtPSO->QueryInterface(IID_PPV_ARGS(&psoProps));

    const void* rayGenID = psoProps->GetShaderIdentifier(L"RaygenShader");
    const void* hitGroupID = psoProps->GetShaderIdentifier(L"HitGroup");
    const void* shadowHitGroupID = psoProps->GetShaderIdentifier(L"ShadowHitGroup");
    const void* missID = psoProps->GetShaderIdentifier(L"MissShader");
    const void* shadowMissID = psoProps->GetShaderIdentifier(L"ShadowMissShader");

    const uint32 numGeometries = uint32(currentModel->NumMeshes());

    // Make our shader tables
    {
        ShaderIdentifier rayGenRecords[1] = { ShaderIdentifier(rayGenID) };

        StructuredBufferInit sbInit;
        sbInit.Stride = sizeof(ShaderIdentifier);
        sbInit.NumElements = ArraySize_(rayGenRecords);
        sbInit.InitData = rayGenRecords;
        sbInit.ShaderTable = true;
        sbInit.Name = L"Ray Gen Shader Table";
        rtRayGenTable.Initialize(sbInit);
    }

    {
        ShaderIdentifier missRecords[2] = { ShaderIdentifier(missID), ShaderIdentifier(shadowMissID) };

        StructuredBufferInit sbInit;
        sbInit.Stride = sizeof(ShaderIdentifier);
        sbInit.NumElements = ArraySize_(missRecords);
        sbInit.InitData = missRecords;
        sbInit.ShaderTable = true;
        sbInit.Name = L"Miss Shader Table";
        rtMissTable.Initialize(sbInit);
    }

    {
        Array<HitGroupRecord> hitGroupRecords(numGeometries * 2);
        for(uint64 i = 0; i < numGeometries; ++i)
        {
            hitGroupRecords[i * 2 + 0].ID = ShaderIdentifier(hitGroupID);
            hitGroupRecords[i * 2 + 0].GeometryIdx = uint32(i);

            hitGroupRecords[i * 2 + 1].ID = ShaderIdentifier(shadowHitGroupID);
            hitGroupRecords[i * 2 + 1].GeometryIdx = uint32(i);
        }

        StructuredBufferInit sbInit;
        sbInit.Stride = sizeof(HitGroupRecord);
        sbInit.NumElements = hitGroupRecords.Size();
        sbInit.InitData = hitGroupRecords.Data();
        sbInit.ShaderTable = true;
        sbInit.Name = L"Hit Shader Table";
        rtHitTable.Initialize(sbInit);
    }

    DX12::Release(psoProps);
}

void DXRPathTracer::Update(const Timer& timer)
{
    CPUProfileBlock profileBlock("Update");

    AppSettings::UpdateUI();

    MouseState mouseState = MouseState::GetMouseState(window);
    KeyboardState kbState = KeyboardState::GetKeyboardState(window);

    if(kbState.IsKeyDown(KeyboardState::Escape))
        window.Destroy();

    float CamMoveSpeed = 5.0f * timer.DeltaSecondsF();
    const float CamRotSpeed = 0.180f * timer.DeltaSecondsF();

    // Move the camera with keyboard input
    if(kbState.IsKeyDown(KeyboardState::LeftShift))
        CamMoveSpeed *= 0.25f;

    Float3 camPos = camera.Position();
    if(kbState.IsKeyDown(KeyboardState::W))
        camPos += camera.Forward() * CamMoveSpeed;
    else if (kbState.IsKeyDown(KeyboardState::S))
        camPos += camera.Back() * CamMoveSpeed;
    if(kbState.IsKeyDown(KeyboardState::A))
        camPos += camera.Left() * CamMoveSpeed;
    else if (kbState.IsKeyDown(KeyboardState::D))
        camPos += camera.Right() * CamMoveSpeed;
    if(kbState.IsKeyDown(KeyboardState::Q))
        camPos += camera.Up() * CamMoveSpeed;
    else if (kbState.IsKeyDown(KeyboardState::E))
        camPos += camera.Down() * CamMoveSpeed;
    camera.SetPosition(camPos);

    // Rotate the cameraFP with the mouse
    if(mouseState.RButton.Pressed && mouseState.IsOverWindow)
    {
        float xRot = camera.XRotation();
        float yRot = camera.YRotation();
        xRot += mouseState.DY * CamRotSpeed;
        yRot += mouseState.DX * CamRotSpeed;
        camera.SetXRotation(xRot);
        camera.SetYRotation(yRot);
    }

    UpdateLights();

    appViewMatrix = camera.ViewMatrix();

    // Toggle VSYNC
    swapChain.SetVSYNCEnabled(AppSettings::EnableVSync ? true : false);

    skyCache.Init(AppSettings::SunDirection, AppSettings::SunSize, AppSettings::GroundAlbedo, AppSettings::Turbidity, true);

    if(AppSettings::MSAAMode.Changed() || AppSettings::ClusterRasterizationMode.Changed())
    {
        DestroyPSOs();
        CreateRenderTargets();
        CreatePSOs();
    }

    if(AppSettings::CurrentScene.Changed())
    {
        currentModel = &sceneModels[uint64(AppSettings::CurrentScene)];
        DestroyPSOs();
        InitializeScene();
        CreatePSOs();

        rtShouldRestartPathTrace = true;
    }

    const Setting* settingsToCheck[] =
    {
        &AppSettings::SqrtNumSamples,
        &AppSettings::MaxPathLength,
        &AppSettings::EnableAlbedoMaps,
        &AppSettings::EnableNormalMaps,
        &AppSettings::EnableDiffuse,
        &AppSettings::EnableSpecular,
        &AppSettings::EnableDirect,
        &AppSettings::EnableIndirect,
        &AppSettings::EnableIndirectSpecular,
        &AppSettings::EnableSun,
        &AppSettings::SunSize,
        &AppSettings::SunDirection,
        &AppSettings::Turbidity,
        &AppSettings::GroundAlbedo,
        &AppSettings::RoughnessScale,
    };

    for(const Setting* setting : settingsToCheck)
    {
        if(setting->Changed())
            rtShouldRestartPathTrace = true;
    }

    if(AppSettings::AlwaysResetPathTrace)
        rtShouldRestartPathTrace = true;

    if(rtCurrCamera.Position() != camera.Position() || rtCurrCamera.Orientation() != camera.Orientation() || rtCurrCamera.ProjectionMatrix() != camera.ProjectionMatrix())
        rtShouldRestartPathTrace = true;

    rtCurrCamera = camera;

    if(AppSettings::EnableRayTracing && rtShouldRestartPathTrace)
    {
        rtCurrSampleIdx = 0;
        rtShouldRestartPathTrace = false;
    }
}

void DXRPathTracer::Render(const Timer& timer)
{
    if(buildAccelStructure)
        BuildRTAccelerationStructure();
    else if(lastBuildAccelStructureFrame + DX12::RenderLatency == DX12::CurrentCPUFrame)
        WriteLog("Acceleration structure build time: %.2f ms", Profiler::GlobalProfiler.GPUProfileTiming("Build Acceleration Structure"));

    ID3D12GraphicsCommandList4* cmdList = DX12::CmdList;

    CPUProfileBlock cpuProfileBlock("Render");
    ProfileBlock gpuProfileBlock(cmdList, "Render Total");

    RenderTexture* finalRT = nullptr;

    if(AppSettings::EnableRayTracing)
    {
        RenderRayTracing();

        finalRT = &rtTarget;
    }
    else
    {
        RenderClusters();

        if(AppSettings::EnableSun)
            meshRenderer.RenderSunShadowMap(cmdList, camera);

        if(AppSettings::RenderLights)
            meshRenderer.RenderSpotLightShadowMap(cmdList, camera);

        if(spotLights.Size() > 0)
        {
            // Update the light constant buffer
            const void* srcData[2] = { spotLights.Data(), meshRenderer.SpotLightShadowMatrices() };
            uint64 sizes[2] = { spotLights.MemorySize(), spotLights.Size() * sizeof(Float4x4) };
            uint64 offsets[2] = { 0, sizeof(SpotLight) * AppSettings::MaxSpotLights };
            spotLightBuffer.MultiUpdateData(srcData, sizes, offsets, ArraySize_(srcData));
        }

        RenderForward();

        RenderResolve();

        finalRT = mainTarget.MSAASamples > 1 ? &resolveTarget : &mainTarget;
    }

    {
        ProfileBlock ppProfileBlock(cmdList, "Post Processing");
        postProcessor.Render(cmdList, *finalRT, swapChain.BackBuffer());
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[1] = { swapChain.BackBuffer().RTV };
    cmdList->OMSetRenderTargets(1, rtvHandles, false, nullptr);

    DX12::SetViewport(cmdList, swapChain.Width(), swapChain.Height());

    RenderHUD(timer);
}

void DXRPathTracer::UpdateLights()
{
    const uint64 numSpotLights = Min<uint64>(spotLights.Size(), AppSettings::MaxLightClamp);

    // This is an additional scale factor that's needed to make sure that our polygonal bounding cone
    // fully encloses the actual cone representing the light's area of influence
    const float inRadius = std::cos(Pi / NumConeSides);
    const float scaleCorrection = 1.0f / inRadius;

    const Float4x4 viewMatrix = camera.ViewMatrix();
    const float nearClip = camera.NearClip();
    const float farClip = camera.FarClip();
    const float zRange = farClip - nearClip;
    const Float3 cameraPos = camera.Position();
    const uint64 numConeVerts = coneVertices.Size();

    // Come up with a bounding sphere that surrounds the near clipping plane. We'll test this sphere
    // for intersection with the spot light's bounding cone, and use that to over-estimate if the bounding
    // geometry will end up getting clipped by the camera's near clipping plane
    Float3 nearClipCenter = cameraPos + nearClip * camera.Forward();
    Float4x4 invViewProjection = Float4x4::Invert(camera.ViewProjectionMatrix());
    Float3 nearTopRight = Float3::Transform(Float3(1.0f, 1.0f, 0.0f), invViewProjection);
    float nearClipRadius = Float3::Length(nearTopRight - nearClipCenter);

    ClusterBounds* boundsData = spotLightBoundsBuffer.Map<ClusterBounds>();
    bool intersectsCamera[AppSettings::MaxSpotLights] = { };

    // Update the light bounds buffer
    for(uint64 spotLightIdx = 0; spotLightIdx < numSpotLights; ++spotLightIdx)
    {
        const SpotLight& spotLight = spotLights[spotLightIdx];
        const ModelSpotLight& srcSpotLight = currentModel->SpotLights()[spotLightIdx];
        ClusterBounds bounds;
        bounds.Position = spotLight.Position;
        bounds.Orientation = srcSpotLight.Orientation;
        bounds.Scale.x = bounds.Scale.y = std::tan(srcSpotLight.AngularAttenuation.y / 2.0f) * spotLight.Range * scaleCorrection;
        bounds.Scale.z = spotLight.Range;

        // Compute conservative Z bounds for the light based on vertices of the bounding geometry
        float minZ = FloatMax;
        float maxZ = -FloatMax;
        for(uint64 i = 0; i < numConeVerts; ++i)
        {
            Float3 coneVert = coneVertices[i] * bounds.Scale;
            coneVert = Float3::Transform(coneVert, bounds.Orientation);
            coneVert += bounds.Position;

            float vertZ = Float3::Transform(coneVert, viewMatrix).z;
            minZ = Min(minZ, vertZ);
            maxZ = Max(maxZ, vertZ);
        }

        minZ = Saturate((minZ - nearClip) / zRange);
        maxZ = Saturate((maxZ - nearClip) / zRange);

        bounds.ZBounds.x = uint32(minZ * AppSettings::NumZTiles);
        bounds.ZBounds.y = Min(uint32(maxZ * AppSettings::NumZTiles), uint32(AppSettings::NumZTiles - 1));

        // Estimate if the light's bounding geometry intersects with the camera's near clip plane
        boundsData[spotLightIdx] = bounds;
        intersectsCamera[spotLightIdx] = SphereConeIntersection(spotLight.Position, srcSpotLight.Direction, spotLight.Range,
                                                                srcSpotLight.AngularAttenuation.y, nearClipCenter, nearClipRadius);
    }

    numIntersectingSpotLights = 0;
    uint32* instanceData = spotLightInstanceBuffer.Map<uint32>();

    for(uint64 spotLightIdx = 0; spotLightIdx < numSpotLights; ++spotLightIdx)
        if(intersectsCamera[spotLightIdx])
            instanceData[numIntersectingSpotLights++] = uint32(spotLightIdx);

    uint64 offset = numIntersectingSpotLights;
    for(uint64 spotLightIdx = 0; spotLightIdx < numSpotLights; ++spotLightIdx)
        if(intersectsCamera[spotLightIdx] == false)
            instanceData[offset++] = uint32(spotLightIdx);
}

void DXRPathTracer::RenderClusters()
{
    ID3D12GraphicsCommandList* cmdList = DX12::CmdList;

    PIXMarker marker(cmdList, "Cluster Update");
    ProfileBlock profileBlock(cmdList, "Cluster Update");

    spotLightClusterBuffer.MakeWritable(cmdList);

    {
        // Clear spot light clusters
        D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptors[1] = { spotLightClusterBuffer.UAV };
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = DX12::TempDescriptorTable(cpuDescriptors, ArraySize_(cpuDescriptors));

        uint32 values[4] = { };
        cmdList->ClearUnorderedAccessViewUint(gpuHandle, cpuDescriptors[0], spotLightClusterBuffer.InternalBuffer.Resource, values, 0, nullptr);
    }

    ClusterConstants clusterConstants;
    clusterConstants.ViewProjection = camera.ViewProjectionMatrix();
    clusterConstants.InvProjection = Float4x4::Invert(camera.ProjectionMatrix());
    clusterConstants.NearClip = camera.NearClip();
    clusterConstants.FarClip = camera.FarClip();
    clusterConstants.InvClipRange = 1.0f / (camera.FarClip() - camera.NearClip());
    clusterConstants.NumXTiles = uint32(AppSettings::NumXTiles);
    clusterConstants.NumYTiles = uint32(AppSettings::NumYTiles);
    clusterConstants.NumXYTiles = uint32(AppSettings::NumXTiles * AppSettings::NumYTiles);
    clusterConstants.InstanceOffset = 0;
    clusterConstants.NumLights = Min<uint32>(uint32(spotLights.Size()), AppSettings::MaxLightClamp);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[1] = { clusterMSAATarget.RTV };
    ClusterRasterizationModes rastMode = AppSettings::ClusterRasterizationMode;
    if(rastMode == ClusterRasterizationModes::MSAA4x || rastMode == ClusterRasterizationModes::MSAA8x)
        cmdList->OMSetRenderTargets(1, rtvHandles, false, nullptr);
    else
        cmdList->OMSetRenderTargets(0, nullptr, false, nullptr);

    DX12::SetViewport(cmdList, AppSettings::NumXTiles, AppSettings::NumYTiles);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    cmdList->SetGraphicsRootSignature(clusterRS);

    DX12::BindStandardDescriptorTable(cmdList, ClusterParams_StandardDescriptors, CmdListMode::Graphics);

    if(AppSettings::RenderLights)
    {
        // Update light clusters
        spotLightClusterBuffer.UAVBarrier(cmdList);

        D3D12_INDEX_BUFFER_VIEW ibView = spotLightClusterIdxBuffer.IBView();
        cmdList->IASetIndexBuffer(&ibView);

        clusterConstants.ElementsPerCluster = uint32(AppSettings::SpotLightElementsPerCluster);
        clusterConstants.InstanceOffset = 0;
        clusterConstants.BoundsBufferIdx = spotLightBoundsBuffer.SRV;
        clusterConstants.VertexBufferIdx = spotLightClusterVtxBuffer.SRV;
        clusterConstants.InstanceBufferIdx = spotLightInstanceBuffer.SRV;
        DX12::BindTempConstantBuffer(cmdList, clusterConstants, ClusterParams_CBuffer, CmdListMode::Graphics);

        AppSettings::BindCBufferGfx(cmdList, ClusterParams_AppSettings);

        D3D12_CPU_DESCRIPTOR_HANDLE uavs[] = { spotLightClusterBuffer.UAV };
        DX12::BindTempDescriptorTable(cmdList, uavs, ArraySize_(uavs), ClusterParams_UAVDescriptors, CmdListMode::Graphics);

        const uint64 numLightsToRender = Min<uint64>(spotLights.Size(), AppSettings::MaxLightClamp);
        Assert_(numIntersectingSpotLights <= numLightsToRender);
        const uint64 numNonIntersecting = numLightsToRender - numIntersectingSpotLights;

        // Render back faces for lights that intersect with the camera
        cmdList->SetPipelineState(clusterIntersectingPSO);

        cmdList->DrawIndexedInstanced(uint32(spotLightClusterIdxBuffer.NumElements), uint32(numIntersectingSpotLights), 0, 0, 0);

        // Now for all other lights, render the back faces followed by the front faces
        cmdList->SetPipelineState(clusterBackFacePSO);

        clusterConstants.InstanceOffset = uint32(numIntersectingSpotLights);
        DX12::BindTempConstantBuffer(cmdList, clusterConstants, ClusterParams_CBuffer, CmdListMode::Graphics);

        cmdList->DrawIndexedInstanced(uint32(spotLightClusterIdxBuffer.NumElements), uint32(numNonIntersecting), 0, 0, 0);

        spotLightClusterBuffer.UAVBarrier(cmdList);

        cmdList->SetPipelineState(clusterFrontFacePSO);

        cmdList->DrawIndexedInstanced(uint32(spotLightClusterIdxBuffer.NumElements), uint32(numNonIntersecting), 0, 0, 0);
    }

    // Sync
    spotLightClusterBuffer.MakeReadable(cmdList);
}

void DXRPathTracer::RenderForward()
{
    ID3D12GraphicsCommandList* cmdList = DX12::CmdList;

    PIXMarker marker(cmdList, "Forward rendering");

    {
        // Transition render targets back to a writable state
        D3D12_RESOURCE_BARRIER barriers[1] = { };
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[0].Transition.pResource = mainTarget.Resource();
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[0].Transition.Subresource = 0;

        cmdList->ResourceBarrier(ArraySize_(barriers), barriers);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[1] = { mainTarget.RTV };
    cmdList->OMSetRenderTargets(1, rtvHandles, false, &depthBuffer.DSV);

    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    cmdList->ClearRenderTargetView(rtvHandles[0], clearColor, 0, nullptr);
    cmdList->ClearDepthStencilView(depthBuffer.DSV, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    DX12::SetViewport(cmdList, mainTarget.Width(), mainTarget.Height());

    {
        ProfileBlock profileBlock(cmdList, "Forward Rendering Pass");

        // Render the main forward pass
        MainPassData mainPassData;
        mainPassData.SkyCache = &skyCache;
        mainPassData.SpotLightBuffer = &spotLightBuffer;
        mainPassData.SpotLightClusterBuffer = &spotLightClusterBuffer;
        meshRenderer.RenderMainPass(cmdList, camera, mainPassData);

        cmdList->OMSetRenderTargets(1, rtvHandles, false, &depthBuffer.DSV);

        // Render the sky
        skybox.RenderSky(cmdList, camera.ViewMatrix(), camera.ProjectionMatrix(), skyCache, true);

        {
            // Make our targets readable again, which will force a sync point.
            D3D12_RESOURCE_BARRIER barriers[1] = {};
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barriers[0].Transition.pResource = mainTarget.Resource();
            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barriers[0].Transition.Subresource = 0;

            cmdList->ResourceBarrier(ArraySize_(barriers), barriers);
        }
    }
}

// Performs MSAA resolve with a full-screen pixel shader
void DXRPathTracer::RenderResolve()
{
    if(AppSettings::MSAAMode == MSAAModes::MSAANone)
        return;

    ID3D12GraphicsCommandList* cmdList = DX12::CmdList;

    PIXMarker pixMarker(cmdList, "MSAA Resolve");
    ProfileBlock profileBlock(cmdList, "MSAA Resolve");

    resolveTarget.MakeWritable(cmdList);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[1] = { resolveTarget.RTV };
    cmdList->OMSetRenderTargets(ArraySize_(rtvs), rtvs, false, nullptr);
    DX12::SetViewport(cmdList, resolveTarget.Width(), resolveTarget.Height());

    cmdList->SetGraphicsRootSignature(resolveRootSignature);
    cmdList->SetPipelineState(resolvePSO);

    DX12::BindStandardDescriptorTable(cmdList, ResolveParams_StandardDescriptors, CmdListMode::Graphics);

    cmdList->SetGraphicsRoot32BitConstant(ResolveParams_Constants, uint32(mainTarget.Width()), 0);
    cmdList->SetGraphicsRoot32BitConstant(ResolveParams_Constants, uint32(mainTarget.Height()), 1);
    cmdList->SetGraphicsRoot32BitConstant(ResolveParams_Constants, mainTarget.SRV(), 2);

    AppSettings::BindCBufferGfx(cmdList, ResolveParams_AppSettings);

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetIndexBuffer(nullptr);
    cmdList->IASetVertexBuffers(0, 0, nullptr);

    cmdList->DrawInstanced(3, 1, 0, 0);

    resolveTarget.MakeReadable(cmdList);
}

void DXRPathTracer::RenderRayTracing()
{
    // Don't keep tracing rays if we've hit our maximum per-pixel sample count
    if(rtCurrSampleIdx >= uint32(AppSettings::SqrtNumSamples * AppSettings::SqrtNumSamples))
        return;

    ID3D12GraphicsCommandList4* cmdList = DX12::CmdList;
    cmdList->SetComputeRootSignature(rtRootSignature);

    DX12::BindStandardDescriptorTable(cmdList, RTParams_StandardDescriptors, CmdListMode::Compute);

    cmdList->SetComputeRootShaderResourceView(RTParams_SceneDescriptor, rtTopLevelAccelStructure.GPUAddress);
    DX12::BindTempDescriptorTable(cmdList, &rtTarget.UAV, 1, RTParams_UAVDescriptor, CmdListMode::Compute);

    RayTraceConstants rtConstants;
    rtConstants.InvViewProjection = Float4x4::Invert(camera.ViewProjectionMatrix());

    rtConstants.SunDirectionWS = AppSettings::SunDirection;
    rtConstants.SunIrradiance = skyCache.SunIrradiance;
    rtConstants.CosSunAngularRadius = std::cos(DegToRad(AppSettings::SunSize));
    rtConstants.SinSunAngularRadius = std::sin(DegToRad(AppSettings::SunSize));
    rtConstants.SunRenderColor = skyCache.SunRenderColor;
    rtConstants.CameraPosWS = camera.Position();
    rtConstants.CurrSampleIdx = rtCurrSampleIdx;
    rtConstants.TotalNumPixels = uint32(rtTarget.Width()) * uint32(rtTarget.Height());

    rtConstants.VtxBufferIdx = currentModel->VertexBuffer().SRV;
    rtConstants.IdxBufferIdx = currentModel->IndexBuffer().SRV;
    rtConstants.GeometryInfoBufferIdx = rtGeoInfoBuffer.SRV;
    rtConstants.MaterialBufferIdx = meshRenderer.MaterialBuffer().SRV;
    rtConstants.SkyTextureIdx = skyCache.CubeMap.SRV;

    DX12::BindTempConstantBuffer(cmdList, rtConstants, RTParams_CBuffer, CmdListMode::Compute);

    AppSettings::BindCBufferCompute(cmdList, RTParams_AppSettings);

    rtTarget.MakeWritableUAV(cmdList);

    cmdList->SetPipelineState1(rtPSO);

    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
    dispatchDesc.HitGroupTable = rtHitTable.ShaderTable();
    dispatchDesc.MissShaderTable = rtMissTable.ShaderTable();
    dispatchDesc.RayGenerationShaderRecord = rtRayGenTable.ShaderRecord(0);
    dispatchDesc.Width = uint32(rtTarget.Width());
    dispatchDesc.Height = uint32(rtTarget.Height());
    dispatchDesc.Depth = 1;

    DX12::CmdList->DispatchRays(&dispatchDesc);

    rtTarget.MakeReadableUAV(cmdList);

    rtCurrSampleIdx += 1;
}

void DXRPathTracer::RenderHUD(const Timer& timer)
{
    ID3D12GraphicsCommandList* cmdList = DX12::CmdList;
    PIXMarker pixMarker(cmdList, "HUD Pass");

    Float2 viewportSize;
    viewportSize.x = float(swapChain.Width());
    viewportSize.y = float(swapChain.Height());
    spriteRenderer.Begin(cmdList, viewportSize, SpriteFilterMode::Point, SpriteBlendMode::AlphaBlend);

    Float2 textPos = Float2(25.0f, 25.0f);
    std::wstring fpsText = MakeString(L"Frame Time: %.2fms (%u FPS)", 1000.0f / fps, fps);
    spriteRenderer.RenderText(cmdList, font, fpsText.c_str(), textPos, Float4(1.0f, 1.0f, 0.0f, 1.0f));

    spriteRenderer.End();

    // Draw the progress bar
    const uint32 totalNumSamples = uint32(AppSettings::SqrtNumSamples * AppSettings::SqrtNumSamples);
    if(rtCurrSampleIdx < totalNumSamples && AppSettings::ShowProgressBar)
    {
        float width = float(swapChain.Width());
        float height = float(swapChain.Height());

        const uint32 barEmptyColor = ImColor(0.0f, 0.0f, 0.0f, 1.0f);
        const uint32 barFilledColor = ImColor(1.0f, 0.0f, 0.0f, 1.0f);
        const uint32 barOutlineColor = ImColor(1.0f, 1.0f, 1.0f, 1.0f);
        const uint32 textColor = ImColor(1.0f, 1.0f, 1.0f, 1.0f);

        const float barPercentage = 0.75f;
        const float barHeight = 75.0f;
        Float2 barStart = Float2(width * (1.0f - barPercentage) * 0.5f, height - 200.0f);
        Float2 barSize = Float2(width * barPercentage, barHeight);
        Float2 barEnd = barStart + barSize;

        Float2 windowStart = barStart - 8.0f;
        Float2 windowSize = barSize + 16.0f;
        Float2 windowEnd = windowStart + windowSize;

        ImGui::SetNextWindowPos(ToImVec2(windowStart), ImGuiSetCond_Always);
        ImGui::SetNextWindowSize(ToImVec2(windowSize), ImGuiSetCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("HUD Window", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs |
                                            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse);

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const float progress = float(rtCurrSampleIdx) / totalNumSamples;

        drawList->AddRectFilled(ToImVec2(barStart), ToImVec2(barEnd), barEmptyColor);
        drawList->AddRectFilled(ToImVec2(barStart), ImVec2(barStart.x + barSize.x * progress, barEnd.y), barFilledColor);
        drawList->AddRect(ToImVec2(barStart), ToImVec2(barEnd), barOutlineColor);

        const uint64 raysPerFrame = rtTarget.Width() * rtTarget.Height() * (1 + (AppSettings::MaxPathLength - 1) * 2);
        const double mRaysPerSecond = raysPerFrame * (1.0 / timer.DeltaSecondsF()) / 1000000.0;

        std::string progressText = MakeString("Progress: %.2f%% (%.2f Mrays per second)", progress * 100.0f, mRaysPerSecond);
        Float2 progressTextSize = ToFloat2(ImGui::CalcTextSize(progressText.c_str()));
        Float2 progressTextPos = barStart + (barSize * 0.5f) - (progressTextSize * 0.5f);
        drawList->AddText(ToImVec2(progressTextPos), textColor, progressText.c_str());

        ImGui::PopStyleVar();

        ImGui::End();
    }
}

void DXRPathTracer::BuildRTAccelerationStructure()
{
    const FormattedBuffer& idxBuffer = currentModel->IndexBuffer();
    const StructuredBuffer& vtxBuffer = currentModel->VertexBuffer();

    const uint64 numMeshes = currentModel->NumMeshes();
    Array<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs(numMeshes);

    const uint32 numGeometries = uint32(geometryDescs.Size());
    Array<GeometryInfo> geoInfoBufferData(numGeometries);

    for(uint64 meshIdx = 0; meshIdx < numMeshes; ++meshIdx)
    {
        const Mesh& mesh = currentModel->Meshes()[meshIdx];
        D3D12_RAYTRACING_GEOMETRY_DESC& geometryDesc = geometryDescs[meshIdx];
        geometryDesc = { };
        geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometryDesc.Triangles.IndexBuffer = idxBuffer.GPUAddress + mesh.IndexOffset() * idxBuffer.Stride;
        geometryDesc.Triangles.IndexCount = uint32(mesh.NumIndices());
        geometryDesc.Triangles.IndexFormat = idxBuffer.Format;
        geometryDesc.Triangles.Transform3x4 = 0;
        geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geometryDesc.Triangles.VertexCount = uint32(mesh.NumVertices());
        geometryDesc.Triangles.VertexBuffer.StartAddress = vtxBuffer.GPUAddress + mesh.VertexOffset() * vtxBuffer.Stride;
        geometryDesc.Triangles.VertexBuffer.StrideInBytes = vtxBuffer.Stride;
        geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        GeometryInfo& geoInfo = geoInfoBufferData[meshIdx];
        geoInfo = { };
        geoInfo.VtxOffset = uint32(mesh.VertexOffset());
        geoInfo.IdxOffset = uint32(mesh.IndexOffset());
        geoInfo.MaterialIdx = mesh.MeshParts()[0].MaterialIdx;

        Assert_(mesh.NumMeshParts() == 1);
    }

    // Get required sizes for an acceleration structure
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};

    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildInfoDesc = {};
        prebuildInfoDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        prebuildInfoDesc.Flags = buildFlags;
        prebuildInfoDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        prebuildInfoDesc.pGeometryDescs = nullptr;
        prebuildInfoDesc.NumDescs = 1;
        DX12::Device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildInfoDesc, &topLevelPrebuildInfo);
    }

    Assert_(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};

    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildInfoDesc = {};
        prebuildInfoDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        prebuildInfoDesc.Flags = buildFlags;
        prebuildInfoDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        prebuildInfoDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        prebuildInfoDesc.pGeometryDescs = geometryDescs.Data();
        prebuildInfoDesc.NumDescs = numGeometries;
        DX12::Device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildInfoDesc, &bottomLevelPrebuildInfo);
    }

    Assert_(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

    RawBuffer scratchBuffer;

    {
        RawBufferInit bufferInit;
        bufferInit.NumElements = Max(topLevelPrebuildInfo.ScratchDataSizeInBytes, bottomLevelPrebuildInfo.ScratchDataSizeInBytes) / RawBuffer::Stride;
        bufferInit.CreateUAV = true;
        bufferInit.InitialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        bufferInit.Name = L"RT Scratch Buffer";
        scratchBuffer.Initialize(bufferInit);
    }

    {
        RawBufferInit bufferInit;
        bufferInit.NumElements = bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes / RawBuffer::Stride;
        bufferInit.CreateUAV = true;
        bufferInit.InitialState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        bufferInit.Name = L"RT Bottom Level Accel Structure";
        rtBottomLevelAccelStructure.Initialize(bufferInit);
    }

    {
        RawBufferInit bufferInit;
        bufferInit.NumElements = topLevelPrebuildInfo.ResultDataMaxSizeInBytes / RawBuffer::Stride;
        bufferInit.CreateUAV = true;
        bufferInit.InitialState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        bufferInit.Name = L"RT Top Level Accel Structure";
        rtTopLevelAccelStructure.Initialize(bufferInit);
    }

    // Create an instance desc for the bottom-level acceleration structure.
    D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
    instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1.0f;
    instanceDesc.InstanceMask = 1;
    instanceDesc.AccelerationStructure = rtBottomLevelAccelStructure.GPUAddress;

    TempBuffer instanceBuffer = DX12::TempStructuredBuffer(1, sizeof(D3D12_RAYTRACING_INSTANCE_DESC), false);
    memcpy(instanceBuffer.CPUAddress, &instanceDesc, sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

    // Bottom Level Acceleration Structure desc
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
    {
        bottomLevelBuildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        bottomLevelBuildDesc.Inputs.Flags = buildFlags;
        bottomLevelBuildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        bottomLevelBuildDesc.Inputs.NumDescs = numGeometries;
        bottomLevelBuildDesc.Inputs.pGeometryDescs = geometryDescs.Data();
        bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchBuffer.GPUAddress;
        bottomLevelBuildDesc.DestAccelerationStructureData = rtBottomLevelAccelStructure.GPUAddress;
    }

    // Top Level Acceleration Structure desc
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = bottomLevelBuildDesc;
    {
        topLevelBuildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        topLevelBuildDesc.Inputs.NumDescs = 1;
        topLevelBuildDesc.Inputs.pGeometryDescs = nullptr;
        topLevelBuildDesc.Inputs.InstanceDescs = instanceBuffer.GPUAddress;
        topLevelBuildDesc.DestAccelerationStructureData = rtTopLevelAccelStructure.GPUAddress;;
        topLevelBuildDesc.ScratchAccelerationStructureData = scratchBuffer.GPUAddress;
    }

    {
        ProfileBlock profileBlock(DX12::CmdList, "Build Acceleration Structure");

        DX12::CmdList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
        rtBottomLevelAccelStructure.UAVBarrier(DX12::CmdList);

        DX12::CmdList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
        rtTopLevelAccelStructure.UAVBarrier(DX12::CmdList);
    }

    scratchBuffer.Shutdown();

    {
        StructuredBufferInit sbInit;
        sbInit.Stride = sizeof(GeometryInfo);
        sbInit.NumElements = numGeometries;
        sbInit.Name = L"Geometry Info Buffer";
        sbInit.InitData = geoInfoBufferData.Data();
        rtGeoInfoBuffer.Initialize(sbInit);
    }

    buildAccelStructure = false;
    lastBuildAccelStructureFrame = DX12::CurrentCPUFrame;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    DXRPathTracer app(lpCmdLine);
    app.Run();
}
