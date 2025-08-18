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

#include "MeshRenderer.h"

#include <Exceptions.h>
#include <Utility.h>
#include <Graphics/ShaderCompilation.h>
#include <Graphics/Skybox.h>
#include <Graphics/Profiler.h>

#include "AppSettings.h"

// Constants
static const uint64_t SunShadowMapSize = 2048;
static const uint64_t SpotLightShadowMapSize = 1024;

enum MainPassRootParams
{
    MainPass_VSCBuffer,
    MainPass_PSCBuffer,
    MainPass_ShadowCBuffer,
    MainPass_MatIndexCBuffer,
    MainPass_LightCBuffer,
    MainPass_SRVIndices,
    MainPass_AppSettings,

    NumMainPassRootParams,
};

struct MeshVSConstants
{
    Float4Align Float4x4 World;
    Float4Align Float4x4 View;
    Float4Align Float4x4 WorldViewProjection;
    float NearClip = 0.0f;
    float FarClip = 0.0f;
};

// Frustum culls meshes, and produces a buffer of visible mesh indices
static uint64_t CullMeshes(const Camera& camera, const Array<DirectX::BoundingBox>& boundingBoxes, Array<uint32_t>& drawIndices)
{
    DirectX::BoundingFrustum frustum(camera.ProjectionMatrix().ToSIMD());
    frustum.Transform(frustum, 1.0f, camera.Orientation().ToSIMD(), camera.Position().ToSIMD());

    uint64_t numVisible = 0;
    const uint64_t numMeshes = boundingBoxes.Size();
    for(uint64_t i = 0; i < numMeshes; ++i)
    {
        if(frustum.Intersects(boundingBoxes[i]))
            drawIndices[numVisible++] = uint32_t(i);
    }

    return numVisible;
}

// Frustum culls meshes for an orthographic projection, and produces a buffer of visible mesh indices
static uint64_t CullMeshesOrthographic(const OrthographicCamera& camera, bool ignoreNearZ, const Array<DirectX::BoundingBox>& boundingBoxes, Array<uint32_t>& drawIndices)
{
    Float3 mins = Float3(camera.MinX(), camera.MinY(), camera.NearClip());
    Float3 maxes = Float3(camera.MaxX(), camera.MaxY(), camera.FarClip());
    if(ignoreNearZ)
        mins.z = -10000.0f;

    Float3 extents = (maxes - mins) / 2.0f;
    Float3 center = mins + extents;
    center = Float3::Transform(center, camera.Orientation());
    center += camera.Position();

    DirectX::BoundingOrientedBox obb;
    obb.Extents = extents.ToXMFLOAT3();
    obb.Center = center.ToXMFLOAT3();
    obb.Orientation = camera.Orientation().ToXMFLOAT4();

    uint64_t numVisible = 0;
    const uint64_t numMeshes = boundingBoxes.Size();
    for(uint64_t i = 0; i < numMeshes; ++i)
    {
        if(obb.Intersects(boundingBoxes[i]))
            drawIndices[numVisible++] = uint32_t(i);
    }

    return numVisible;
}

MeshRenderer::MeshRenderer()
{
}

void MeshRenderer::LoadShaders()
{
    // Load the mesh shaders
    meshDepthVS = CompileFromFile("DepthOnly.hlsl", "VS", ShaderType::Vertex);

    CompileOptions opts;
    meshVS = CompileFromFile("Mesh.hlsl", "VS", ShaderType::Vertex, opts);
    meshPS = CompileFromFile("Mesh.hlsl", "PSForward", ShaderType::Pixel, opts);

    opts.Add("AlphaTest_", 1);
    meshAlphaTestPS = CompileFromFile("Mesh.hlsl", "PSForward", ShaderType::Pixel, opts);
}

// Loads resources
void MeshRenderer::Initialize(const Model* model_)
{
    model = model_;

    const uint64_t numMeshes = model->Meshes().Size();
    meshBoundingBoxes.Init(numMeshes);
    frustumCulledIndices.Init(numMeshes, uint32_t(-1));
    meshZDepths.Init(numMeshes, FloatMax);
    for(uint64_t i = 0; i < numMeshes; ++i)
    {
        const Mesh& mesh = model->Meshes()[i];
        DirectX::BoundingBox& boundingBox = meshBoundingBoxes[i];
        Float3 extents = (mesh.AABBMax() - mesh.AABBMin()) / 2.0f;
        Float3 center = mesh.AABBMin() + extents;
        boundingBox.Center = center.ToXMFLOAT3();
        boundingBox.Extents = extents.ToXMFLOAT3();
    }

    LoadShaders();

    {
        DepthBufferInit dbInit;
        dbInit.Width = SunShadowMapSize;
        dbInit.Height = SunShadowMapSize;
        dbInit.Format = DXGI_FORMAT_D32_FLOAT;
        dbInit.MSAASamples = ShadowHelper::NumMSAASamples();
        dbInit.ArraySize = NumCascades;
        dbInit.Name = "Sun Shadow Map";
        sunDepthMap.Initialize(dbInit);
    }

    {
        DepthBufferInit dbInit;
        dbInit.Width = SpotLightShadowMapSize;
        dbInit.Height = SpotLightShadowMapSize;
        dbInit.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dbInit.MSAASamples = ShadowHelper::NumMSAASamples();
        dbInit.ArraySize = Max(uint32_t(model->SpotLights().Size()), 1u);
        dbInit.Name = "Spot Light Shadow Map";
        spotLightDepthMap.Initialize(dbInit);
    }

    {
        // Create a structured buffer containing texture indices per-material
        const Array<MeshMaterial>& materials = model->Materials();
        const uint64_t numMaterials = materials.Size();
        Array<Material> matBufferData(numMaterials);
        for(uint64_t i = 0; i < numMaterials; ++i)
        {
            Material& matIndices = matBufferData[i];
            const MeshMaterial& material = materials[i];

            matIndices.Albedo = material.Textures[uint64_t(MaterialTextures::Albedo)]->SRV;
            matIndices.Normal = material.Textures[uint64_t(MaterialTextures::Normal)]->SRV;
            matIndices.Roughness = material.Textures[uint64_t(MaterialTextures::Roughness)]->SRV;
            matIndices.Metallic = material.Textures[uint64_t(MaterialTextures::Metallic)]->SRV;
            matIndices.Emissive = material.Textures[uint64_t(MaterialTextures::Emissive)]->SRV;

            // Opacity is optional
            const Texture* opacity = material.Textures[uint64_t(MaterialTextures::Opacity)];
            matIndices.Opacity = opacity ? opacity->SRV : InvalidDescriptorIndex;
        }

        StructuredBufferInit sbInit;
        sbInit.Stride = sizeof(Material);
        sbInit.NumElements = numMaterials;
        sbInit.Dynamic = false;
        sbInit.InitData = matBufferData.Data();
        materialBuffer.Initialize(sbInit);
        materialBuffer.Resource()->SetName(L"Material Texture Indices");
    }

    {
        // Main pass root signature
        D3D12_ROOT_PARAMETER1 rootParameters[NumMainPassRootParams] = {};

        // VSCBuffer
        rootParameters[MainPass_VSCBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[MainPass_VSCBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParameters[MainPass_VSCBuffer].Descriptor.RegisterSpace = 0;
        rootParameters[MainPass_VSCBuffer].Descriptor.ShaderRegister = 0;
        rootParameters[MainPass_VSCBuffer].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

        // PSCBuffer
        rootParameters[MainPass_PSCBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[MainPass_PSCBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[MainPass_PSCBuffer].Descriptor.RegisterSpace = 0;
        rootParameters[MainPass_PSCBuffer].Descriptor.ShaderRegister = 0;
        rootParameters[MainPass_PSCBuffer].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

        // ShadowCBuffer
        rootParameters[MainPass_ShadowCBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[MainPass_ShadowCBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[MainPass_ShadowCBuffer].Descriptor.RegisterSpace = 0;
        rootParameters[MainPass_ShadowCBuffer].Descriptor.ShaderRegister = 1;
        rootParameters[MainPass_ShadowCBuffer].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

        // MatIndexCBuffer
        rootParameters[MainPass_MatIndexCBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParameters[MainPass_MatIndexCBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[MainPass_MatIndexCBuffer].Constants.Num32BitValues = 1;
        rootParameters[MainPass_MatIndexCBuffer].Constants.RegisterSpace = 0;
        rootParameters[MainPass_MatIndexCBuffer].Constants.ShaderRegister = 2;

        // LightCBuffer
        rootParameters[MainPass_LightCBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[MainPass_LightCBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[MainPass_LightCBuffer].Descriptor.RegisterSpace = 0;
        rootParameters[MainPass_LightCBuffer].Descriptor.ShaderRegister = 3;
        rootParameters[MainPass_LightCBuffer].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

        // SRV descriptor indices
        rootParameters[MainPass_SRVIndices].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[MainPass_SRVIndices].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[MainPass_SRVIndices].Descriptor.RegisterSpace = 0;
        rootParameters[MainPass_SRVIndices].Descriptor.ShaderRegister = 4;
        rootParameters[MainPass_SRVIndices].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

        // AppSettings
        rootParameters[MainPass_AppSettings].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[MainPass_AppSettings].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[MainPass_AppSettings].Descriptor.RegisterSpace = 0;
        rootParameters[MainPass_AppSettings].Descriptor.ShaderRegister = AppSettings::CBufferRegister;
        rootParameters[MainPass_AppSettings].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

        // Static samplers
        D3D12_STATIC_SAMPLER_DESC staticSamplers[uint64_t(SamplerState::NumValues)] = {};
        for(uint32_t i = 0; i < uint32_t(SamplerState::NumValues); ++i)
            staticSamplers[i] = DX12::GetStaticSamplerState(SamplerState(i), i, 0, D3D12_SHADER_VISIBILITY_ALL);


        D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
        rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.NumStaticSamplers = ArraySize_(staticSamplers);
        rootSignatureDesc.pStaticSamplers = staticSamplers;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

        DX12::CreateRootSignature(&mainPassRootSignature, rootSignatureDesc);
    }
}

void MeshRenderer::Shutdown()
{
    DestroyPSOs();
    sunDepthMap.Shutdown();
    spotLightDepthMap.Shutdown();
    materialBuffer.Shutdown();
    DX12::Release(mainPassRootSignature);
}

void MeshRenderer::CreatePSOs(DXGI_FORMAT mainRTFormat, DXGI_FORMAT depthFormat, uint32_t numMSAASamples)
{
    if(model == nullptr)
        return;


    ID3D12Device* device = DX12::Device;

    {
        // Main pass PSO
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = mainPassRootSignature;
        psoDesc.VS = meshVS.ByteCode();
        psoDesc.PS = meshPS.ByteCode();
        psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::BackFaceCull);
        psoDesc.BlendState = DX12::GetBlendState(BlendState::Disabled);
        psoDesc.DepthStencilState = DX12::GetDepthState(DepthState::WritesEnabled);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = mainRTFormat;
        psoDesc.DSVFormat = depthFormat;
        psoDesc.SampleDesc.Count = numMSAASamples;
        psoDesc.SampleDesc.Quality = numMSAASamples > 1 ? DX12::StandardMSAAPattern : 0;
        psoDesc.InputLayout.NumElements = uint32_t(Model::NumInputElements());
        psoDesc.InputLayout.pInputElementDescs = Model::InputElements();
        DXCall(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mainPassPSO)));

        psoDesc.PS = meshAlphaTestPS.ByteCode();
        DXCall(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mainPassAlphaTestPSO)));
    }

    {
        // Depth-only PSO
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = DX12::UniversalRootSignatureWithIA;
        psoDesc.VS = meshVS.ByteCode();
        psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::BackFaceCull);
        psoDesc.BlendState = DX12::GetBlendState(BlendState::Disabled);
        psoDesc.DepthStencilState = DX12::GetDepthState(DepthState::WritesEnabled);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 0;
        psoDesc.DSVFormat = depthFormat;
        psoDesc.SampleDesc.Count = numMSAASamples;
        psoDesc.SampleDesc.Quality = numMSAASamples > 1 ? DX12::StandardMSAAPattern : 0;
        psoDesc.InputLayout.NumElements = uint32_t(Model::NumInputElements());
        psoDesc.InputLayout.pInputElementDescs = Model::InputElements();
        DXCall(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&depthPSO)));

        // Spotlight shadow depth PSO
        psoDesc.DSVFormat = spotLightDepthMap.DSVFormat;
        psoDesc.SampleDesc.Count = spotLightDepthMap.MSAASamples;
        psoDesc.SampleDesc.Quality = spotLightDepthMap.MSAASamples > 1 ? DX12::StandardMSAAPattern : 0;
        psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::BackFaceCull);
        DXCall(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&spotLightShadowPSO)));

        // Sun shadow depth PSO
        psoDesc.DSVFormat = sunDepthMap.DSVFormat;
        psoDesc.SampleDesc.Count = sunDepthMap.MSAASamples;
        psoDesc.SampleDesc.Quality = sunDepthMap.MSAASamples > 1 ? DX12::StandardMSAAPattern : 0;
        psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::BackFaceCullNoZClip);
        DXCall(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&sunShadowPSO)));
    }
}

void MeshRenderer::DestroyPSOs()
{
    DX12::DeferredRelease(mainPassPSO);
    DX12::DeferredRelease(mainPassAlphaTestPSO);
    DX12::DeferredRelease(depthPSO);
    DX12::DeferredRelease(spotLightShadowPSO);
    DX12::DeferredRelease(sunShadowPSO);
}

// Renders all meshes in the model, with shadows
void MeshRenderer::RenderMainPass(ID3D12GraphicsCommandList10* cmdList, const Camera& camera, const MainPassData& mainPassData)
{
    PIXMarker marker(cmdList, "Mesh Rendering");

    const uint64_t numVisible = CullMeshes(camera, meshBoundingBoxes, frustumCulledIndices);
    const uint32_t* meshDrawIndices = frustumCulledIndices.Data();

    cmdList->SetGraphicsRootSignature(mainPassRootSignature);
    cmdList->SetPipelineState(mainPassPSO);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D12PipelineState* currPSO = mainPassPSO;

    Float4x4 world;

    // Set constant buffers
    MeshVSConstants vsConstants;
    vsConstants.World = world;
    vsConstants.View = camera.ViewMatrix();
    vsConstants.WorldViewProjection = world * camera.ViewProjectionMatrix();
    DX12::BindTempConstantBuffer(cmdList, vsConstants, MainPass_VSCBuffer, CmdListMode::Graphics);

    ShadingConstants psConstants;
    psConstants.SunDirectionWS = AppSettings::SunDirection;
    psConstants.SunIrradiance = mainPassData.SkyCache->SunIrradiance;
    psConstants.CosSunAngularRadius = std::cos(DegToRad(AppSettings::SunSize));
    psConstants.SinSunAngularRadius = std::sin(DegToRad(AppSettings::SunSize));
    psConstants.CameraPosWS = camera.Position();

    psConstants.NumXTiles = uint32_t(AppSettings::NumXTiles);
    psConstants.NumXYTiles = uint32_t(AppSettings::NumXTiles * AppSettings::NumYTiles);
    psConstants.NearClip = camera.NearClip();
    psConstants.FarClip = camera.FarClip();

    psConstants.SkySH = mainPassData.SkyCache->SH;
    DX12::BindTempConstantBuffer(cmdList, psConstants, MainPass_PSCBuffer, CmdListMode::Graphics);

    DX12::BindTempConstantBuffer(cmdList, sunShadowConstants, MainPass_ShadowCBuffer, CmdListMode::Graphics);

    mainPassData.SpotLightBuffer->SetAsGfxRootParameter(cmdList, MainPass_LightCBuffer);

    AppSettings::BindCBufferGfx(cmdList, MainPass_AppSettings);

    uint32_t psSRVs[] =
    {
        sunDepthMap.SRV(),
        spotLightDepthMap.SRV(),
        materialBuffer.SRV,
        mainPassData.SpotLightClusterBuffer->SRV,
    };

    DX12::BindTempConstantBuffer(cmdList, psSRVs, MainPass_SRVIndices, CmdListMode::Graphics);

    // Bind vertices and indices
    D3D12_VERTEX_BUFFER_VIEW vbView = model->VertexBuffer().VBView();
    D3D12_INDEX_BUFFER_VIEW ibView = model->IndexBuffer().IBView();
    cmdList->IASetVertexBuffers(0, 1, &vbView);
    cmdList->IASetIndexBuffer(&ibView);

    // Draw all visible meshes
    uint32_t currMaterial = uint32_t(-1);
    for(uint64_t i = 0; i < numVisible; ++i)
    {
        uint64_t meshIdx = meshDrawIndices[i];
        const Mesh& mesh = model->Meshes()[meshIdx];

        // Draw all parts
        for(uint64_t partIdx = 0; partIdx < mesh.NumMeshParts(); ++partIdx)
        {
            const MeshPart& part = mesh.MeshParts()[partIdx];
            if(part.MaterialIdx != currMaterial)
            {
                cmdList->SetGraphicsRoot32BitConstant(MainPass_MatIndexCBuffer, part.MaterialIdx, 0);
                currMaterial = part.MaterialIdx;
            }

            ID3D12PipelineState* newPSO = mainPassPSO;
            const MeshMaterial& material = model->Materials()[part.MaterialIdx];
            if(material.Textures[uint64_t(MaterialTextures::Opacity)] != nullptr)
                newPSO = mainPassAlphaTestPSO;

            if(currPSO != newPSO)
            {
                cmdList->SetPipelineState(newPSO);
                currPSO = newPSO;
            }

            cmdList->DrawIndexedInstanced(part.IndexCount, 1, mesh.IndexOffset() + part.IndexStart, mesh.VertexOffset(), 0);
        }
    }
}

// Renders all meshes using depth-only rendering
void MeshRenderer::RenderDepth(ID3D12GraphicsCommandList10* cmdList, const Camera& camera, ID3D12PipelineState* pso, uint64_t numVisible, const uint32_t* meshDrawIndices)
{
    cmdList->SetGraphicsRootSignature(DX12::UniversalRootSignatureWithIA);
    cmdList->SetPipelineState(pso);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    Float4x4 world;

    // Set constant buffers
    MeshVSConstants vsConstants;
    vsConstants.World = world;
    vsConstants.View = camera.ViewMatrix();
    vsConstants.WorldViewProjection = world * camera.ViewProjectionMatrix();
    DX12::BindTempConstantBufferToURS(cmdList, vsConstants, 0, CmdListMode::Graphics);

    // Bind vertices and indices
    D3D12_VERTEX_BUFFER_VIEW vbView = model->VertexBuffer().VBView();
    D3D12_INDEX_BUFFER_VIEW ibView = model->IndexBuffer().IBView();
    cmdList->IASetVertexBuffers(0, 1, &vbView);
    cmdList->IASetIndexBuffer(&ibView);

    // Draw all meshes
    for(uint64_t i = 0; i < numVisible; ++i)
    {
        uint64_t meshIdx = meshDrawIndices[i];
        const Mesh& mesh = model->Meshes()[meshIdx];

        // Draw the whole mesh
        cmdList->DrawIndexedInstanced(mesh.NumIndices(), 1, mesh.IndexOffset(), mesh.VertexOffset(), 0);
    }
}

// Renders all meshes using depth-only rendering for a sun shadow map
void MeshRenderer::RenderSunShadowDepth(ID3D12GraphicsCommandList10* cmdList, const OrthographicCamera& camera)
{
    const uint64_t numVisible = CullMeshesOrthographic(camera, true, meshBoundingBoxes, frustumCulledIndices);
    RenderDepth(cmdList, camera, sunShadowPSO, numVisible, frustumCulledIndices.Data());
}

void MeshRenderer::RenderSpotLightShadowDepth(ID3D12GraphicsCommandList10* cmdList, const Camera& camera)
{
    const uint64_t numVisible = CullMeshes(camera, meshBoundingBoxes, frustumCulledIndices);
    RenderDepth(cmdList, camera, spotLightShadowPSO, numVisible, frustumCulledIndices.Data());
}

// Renders meshes using cascaded shadow mapping
void MeshRenderer::RenderSunShadowMap(ID3D12GraphicsCommandList10* cmdList, const Camera& camera)
{
    PIXMarker marker(cmdList, "Sun Shadow Map Rendering");
    CPUProfileBlock cpuProfileBlock("Sun Shadow Map Rendering");
    ProfileBlock profileBlock(cmdList, "Sun Shadow Map Rendering");

    OrthographicCamera cascadeCameras[NumCascades];
    ShadowHelper::PrepareCascades(AppSettings::SunDirection, SunShadowMapSize, true, camera, sunShadowConstants.Base, cascadeCameras);

    // Transition all of the cascade array slices to a writable state
    DX12::Barrier(cmdList, sunDepthMap.DepthWritableBarrier({ .FirstAccess = true }));

    // Render the meshes to each cascade
    for(uint64_t cascadeIdx = 0; cascadeIdx < NumCascades; ++cascadeIdx)
    {
        PIXMarker cascadeMarker(cmdList, MakeString("Rendering Shadow Map Cascade %u", cascadeIdx).c_str());

        // Set the viewport
        DX12::SetViewport(cmdList, SunShadowMapSize, SunShadowMapSize);

        // Set the shadow map as the depth target
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = sunDepthMap.ArrayDSVs[cascadeIdx];
        cmdList->OMSetRenderTargets(0, nullptr, false, &dsv);
        cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

        // Draw the mesh with depth only, using the new shadow camera
        OrthographicCamera& cascadeCam = cascadeCameras[cascadeIdx];
        RenderSunShadowDepth(cmdList, cascadeCam);
    }

    DX12::Barrier(cmdList, sunDepthMap.DepthReadableBarrier());
}

// Render shadows for all spot lights
void MeshRenderer::RenderSpotLightShadowMap(ID3D12GraphicsCommandList10* cmdList, const Camera& camera)
{
    const Array<ModelSpotLight>& spotLights = model->SpotLights();
    const uint64_t numSpotLights = Min<uint64_t>(spotLights.Size(), AppSettings::MaxLightClamp);
    if(numSpotLights == 0)
        return;

    PIXMarker marker(cmdList, "Spot Light Shadow Map Rendering");
    CPUProfileBlock cpuProfileBlock("Spot Light Shadow Map Rendering");
    ProfileBlock profileBlock(cmdList, "Spot Light Shadow Map Rendering");

    // Transition all of the shadow array slices to a writable state
    DX12::Barrier(cmdList, spotLightDepthMap.DepthWritableBarrier({ .FirstAccess = true }));

    for(uint64_t i = 0; i < numSpotLights; ++i)
    {
        PIXMarker lightMarker(cmdList, MakeString("Rendering Spot Light Shadow %u", i).c_str());

        // Set the viewport
        DX12::SetViewport(cmdList, SpotLightShadowMapSize, SpotLightShadowMapSize);

        // Set the shadow map as the depth target
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = numSpotLights > 1 ? spotLightDepthMap.ArrayDSVs[i] : spotLightDepthMap.DSV;
        cmdList->OMSetRenderTargets(0, nullptr, false, &dsv);
        cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

        const ModelSpotLight& light = spotLights[i];

        // Draw the mesh with depth only, using the new shadow camera
        PerspectiveCamera shadowCamera;
        shadowCamera.Initialize(1.0f, light.AngularAttenuation.y, AppSettings::SpotShadowNearClip, AppSettings::SpotLightRange);
        shadowCamera.SetPosition(light.Position);
        shadowCamera.SetOrientation(light.Orientation);
        RenderSpotLightShadowDepth(cmdList, shadowCamera);

        Float4x4 shadowMatrix = shadowCamera.ViewProjectionMatrix() * ShadowHelper::ShadowScaleOffsetMatrix;
        spotLightShadowMatrices[i] = Float4x4::Transpose(shadowMatrix);
    }

    DX12::Barrier(cmdList, spotLightDepthMap.DepthReadableBarrier());
}