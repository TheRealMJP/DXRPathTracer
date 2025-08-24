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
#include <Graphics/BRDF.h>
#include <Graphics/ShaderDebug.h>
#include <EnkiTS/TaskScheduler_c.h>
#include <ImGui/ImGui.h>
#include <ImGuiHelper.h>

#include "DXRPathTracer.h"
#include "SharedTypes.h"

using namespace SampleFramework12;

// Model filenames
static const char* ScenePaths[] =
{
    "..\\Content\\Models\\Sponza\\Sponza.fbx",
    "..\\Content\\Models\\SunTemple\\SunTemple.fbx",
    nullptr,
    "..\\Content\\Models\\WhiteFurnace\\WhiteFurnace.fbx",
};

static const char* SceneTextureDirs[] = { nullptr, "Textures", nullptr, nullptr };
static const float SceneScales[] = { 0.01f, 0.005f, 1.0f, 1.0f };
static const Float3 SceneCameraPositions[] = { Float3(-11.5f, 1.85f, -0.45f), Float3(-1.0f, 5.5f, 12.0f), Float3(0.0f, 2.5f, -10.0f), Float3(0.0f, 0.0f, -3.0f) };
static const Float2 SceneCameraRotations[] = { Float2(0.0f, 1.544f), Float2(0.2f, 3.0f), Float2(0.0f, 0.0f), Float2(0.0f, 0.0f) };
static const Float3 SceneSunDirections[] = { Float3(0.26f, 0.987f, -0.16f), Float3(-0.133022308f, 0.642787635f, 0.75440651f), Float3(0.26f, 0.987f, -0.16f), Float3(0.0f, 1.0f, 0.0f) };

StaticAssert_(ArraySize_(ScenePaths) == uint64_t(Scenes::NumValues));
StaticAssert_(ArraySize_(SceneTextureDirs) == uint64_t(Scenes::NumValues));
StaticAssert_(ArraySize_(SceneScales) == uint64_t(Scenes::NumValues));
StaticAssert_(ArraySize_(SceneCameraPositions) == uint64_t(Scenes::NumValues));
StaticAssert_(ArraySize_(SceneCameraRotations) == uint64_t(Scenes::NumValues));
StaticAssert_(ArraySize_(SceneSunDirections) == uint64_t(Scenes::NumValues));

static const uint64_t NumConeSides = 16;

static const bool Benchmark = false;

struct HitGroupRecord
{
    ShaderIdentifier ID;
};

StaticAssert_(sizeof(HitGroupRecord) % D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT == 0);

struct LightConstants
{
    SpotLight Lights[AppSettings::MaxSpotLights];
};

struct RayTraceConstants
{
    Float4x4 InvViewProjection;

    Float3 SunDirectionWS;
    float CosSunAngularRadius = 0.0f;
    Float3 SunIrradiance;
    float SinSunAngularRadius = 0.0f;
    Float3 SunRenderColor;
    uint32_t Padding = 0;
    Float3 CameraPosWS;
    uint32_t CurrSampleIdx = 0;
    uint32_t TotalNumPixels = 0;

    DescriptorIndex VtxBufferIdx = InvalidDescriptorIndex;
    DescriptorIndex IdxBufferIdx = InvalidDescriptorIndex;
    DescriptorIndex GeometryInfoBufferIdx = InvalidDescriptorIndex;
    DescriptorIndex MaterialBufferIdx = InvalidDescriptorIndex;
    DescriptorIndex SkyTextureIdx = InvalidDescriptorIndex;
    uint32_t NumLights = 0;

    DescriptorIndex SceneAS = InvalidDescriptorIndex;
    DescriptorIndex RenderTarget = InvalidDescriptorIndex;
};

DXRPathTracer::DXRPathTracer(const char* cmdLine) : App("DXR Path Tracer", cmdLine)
{
    minFeatureLevel = D3D_FEATURE_LEVEL_11_1;
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
    if(Benchmark)
    {
        AppSettings::EnableVSync.SetValue(false);
        AppSettings::StablePowerState.SetValue(true);
        AppSettings::AlwaysResetPathTrace.SetValue(true);
        AppSettings::CurrentScene.SetValue(Scenes::SunTemple);
    }

    float aspect = float(swapChain.Width()) / swapChain.Height();
    camera.Initialize(aspect, Pi_4, 0.1f, 100.0f);

    InitializeScene();

    skybox.Initialize();

    postProcessor.Initialize();

    {
        // Spot light and shadow bounds buffer
        ConstantBufferInit cbInit;
        cbInit.Size = sizeof(LightConstants);
        cbInit.Dynamic = true;
        cbInit.CPUAccessible = false;
        cbInit.Name = "Spot Light Buffer";

        spotLightBuffer.Initialize(cbInit);
    }

    rayTraceLib = CompileFromFile("RayTrace.hlsl", nullptr, ShaderType::Library);

    rtCurrCamera = camera;
}

void DXRPathTracer::Shutdown()
{
    for(uint64_t i = 0; i < ArraySize_(sceneModels); ++i)
        sceneModels[i].Shutdown();
    materialBuffer.Shutdown();

    skybox.Shutdown();
    skyCache.Shutdown();
    postProcessor.Shutdown();

    spotLightBuffer.Shutdown();

    depthBuffer.Shutdown();

    rtTarget.Shutdown();
    rtBottomLevelAccelStructure.Shutdown();
    rtTopLevelAccelStructure.Shutdown();
    rtRayGenTable.Shutdown();
    rtHitTable.Shutdown();
    rtMissTable.Shutdown();
    rtGeoInfoBuffer.Shutdown();
}

void DXRPathTracer::CreatePSOs()
{
    skybox.CreatePSOs(rtTarget.Texture.Format, depthBuffer.DSVFormat, rtTarget.MSAASamples);
    postProcessor.CreatePSOs();

    CreateRayTracingPSOs();

    ShaderDebug::CreatePSOs(swapChain.Format(), depthBuffer.DSVFormat);
}

void DXRPathTracer::DestroyPSOs()
{
    skybox.DestroyPSOs();
    postProcessor.DestroyPSOs();

    DX12::DeferredRelease(rtPSO);

    ShaderDebug::DestroyPSOs();
}

// Creates all required render targets
void DXRPathTracer::CreateRenderTargets()
{
    uint32_t width = swapChain.Width();
    uint32_t height = swapChain.Height();

    depthBuffer.Initialize({
        .Width = width,
        .Height = height,
        .Format = DXGI_FORMAT_D32_FLOAT,
        .Name = "Main Depth Buffer",
    });

    rtTarget.Initialize({
        .Width = width,
        .Height = height,
        .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        .CreateUAV = true,
        .InitialLayout = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE,
        .Name = "RT Target",
    });

    rtShouldRestartPathTrace = true;
}

void DXRPathTracer::InitializeScene()
{
    const uint64_t currSceneIdx = uint64_t(AppSettings::CurrentScene);
    AppSettings::EnableWhiteFurnaceMode.SetValue(currSceneIdx == uint64_t(Scenes::WhiteFurnace));

    // Load the scene (if necessary)
    if(sceneModels[currSceneIdx].NumMeshes() == 0)
    {
        if(currSceneIdx == uint64_t(Scenes::BoxTest) || ScenePaths[currSceneIdx] == nullptr)
        {
            sceneModels[currSceneIdx].GenerateBoxTestScene({});
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
    DX12::FlushGPU();

    materialBuffer.Shutdown();

    camera.SetPosition(SceneCameraPositions[currSceneIdx]);
    camera.SetXRotation(SceneCameraRotations[currSceneIdx].x);
    camera.SetYRotation(SceneCameraRotations[currSceneIdx].y);
    AppSettings::SunDirection.SetValue(SceneSunDirections[currSceneIdx]);

    {
        // Initialize the spotlight data used for rendering
        const uint64_t numSpotLights = Min(currentModel->SpotLights().Size(), AppSettings::MaxSpotLights);
        spotLights.Init(numSpotLights);

        for(uint64_t i = 0; i < numSpotLights; ++i)
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

    {
        // Create a structured buffer containing texture indices per-material
        const Array<MeshMaterial>& materials = currentModel->Materials();
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

        materialBuffer.Initialize({
            .Stride = sizeof(Material),
            .NumElements = numMaterials,
            .Dynamic = false,
            .InitData = matBufferData.Data(),
            .Name = "Material Texture Indices",
        });
    }

    buildAccelStructure = true;
}

void DXRPathTracer::CreateRayTracingPSOs()
{
    StateObjectBuilder builder;
    builder.Init(12);

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
        // Primary alpha-test hit group
        D3D12_HIT_GROUP_DESC hitDesc = { };
        hitDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
        hitDesc.ClosestHitShaderImport = L"ClosestHitShader";
        hitDesc.AnyHitShaderImport = L"AnyHitShader";
        hitDesc.HitGroupExport = L"AlphaTestHitGroup";
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
        // Shadow alpha-test hit group
        D3D12_HIT_GROUP_DESC hitDesc = { };
        hitDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
        hitDesc.ClosestHitShaderImport = L"ShadowHitShader";
        hitDesc.AnyHitShaderImport = L"ShadowAnyHitShader";
        hitDesc.HitGroupExport = L"ShadowAlphaTestHitGroup";
        builder.AddSubObject(hitDesc);
    }

    {
        D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = { };
        shaderConfig.MaxAttributeSizeInBytes = 2 * sizeof(float);                      // float2 barycentrics;
        shaderConfig.MaxPayloadSizeInBytes = 4 * sizeof(float) + 4 * sizeof(uint32_t);   // float3 radiance + float roughness + uint pathLength + uint pixelIdx + uint setIdx + bool IsDiffuse
        builder.AddSubObject(shaderConfig);
    }

    {
        // Global root signature with all of our normal bindings
        D3D12_GLOBAL_ROOT_SIGNATURE globalRSDesc = { };
        globalRSDesc.pGlobalRootSignature = DX12::UniversalRootSignature;
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
    const void* alphaTestHitGroupID = psoProps->GetShaderIdentifier(L"AlphaTestHitGroup");
    const void* shadowHitGroupID = psoProps->GetShaderIdentifier(L"ShadowHitGroup");
    const void* shadowAlphaTestHitGroupID = psoProps->GetShaderIdentifier(L"ShadowAlphaTestHitGroup");
    const void* missID = psoProps->GetShaderIdentifier(L"MissShader");
    const void* shadowMissID = psoProps->GetShaderIdentifier(L"ShadowMissShader");

    // Make our shader tables
    {
        ShaderIdentifier rayGenRecords[1] = { ShaderIdentifier(rayGenID) };

        StructuredBufferInit sbInit;
        sbInit.Stride = sizeof(ShaderIdentifier);
        sbInit.NumElements = ArraySize_(rayGenRecords);
        sbInit.InitData = rayGenRecords;
        sbInit.ShaderTable = true;
        sbInit.Name = "Ray Gen Shader Table";
        rtRayGenTable.Initialize(sbInit);
    }

    {
        ShaderIdentifier missRecords[2] = { ShaderIdentifier(missID), ShaderIdentifier(shadowMissID) };

        StructuredBufferInit sbInit;
        sbInit.Stride = sizeof(ShaderIdentifier);
        sbInit.NumElements = ArraySize_(missRecords);
        sbInit.InitData = missRecords;
        sbInit.ShaderTable = true;
        sbInit.Name = "Miss Shader Table";
        rtMissTable.Initialize(sbInit);
    }

    {
        const uint32_t numMeshes = uint32_t(currentModel->NumMeshes());

        Array<HitGroupRecord> hitGroupRecords(numMeshes * 2);
        for(uint64_t i = 0; i < numMeshes; ++i)
        {
            // Use the alpha test hit group (with an any hit shader) if the material has an opacity map
            const Mesh& mesh = currentModel->Meshes()[i];
            Assert_(mesh.NumMeshParts() == 1);
            const uint32_t materialIdx = mesh.MeshParts()[0].MaterialIdx;
            const MeshMaterial& material = currentModel->Materials()[materialIdx];
            const bool alphaTest = material.Textures[uint32_t(MaterialTextures::Opacity)] != nullptr;

            hitGroupRecords[i * 2 + 0].ID = alphaTest ? ShaderIdentifier(alphaTestHitGroupID) : ShaderIdentifier(hitGroupID);
            hitGroupRecords[i * 2 + 1].ID = alphaTest ? ShaderIdentifier(shadowAlphaTestHitGroupID) : ShaderIdentifier(shadowHitGroupID);
        }

        rtHitTable.Initialize({
            .Stride = sizeof(HitGroupRecord),
            .NumElements = hitGroupRecords.Size(),
            .InitData = hitGroupRecords.Data(),
            .ShaderTable = true,
            .Name = "Hit Shader Table",
        });
    }

    DX12::Release(psoProps);
}

void DXRPathTracer::Update(const Timer& timer)
{
    CPUProfileBlock profileBlock("Update");

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

    appViewMatrix = camera.ViewMatrix();

    // Toggle VSYNC
    swapChain.SetVSYNCEnabled(AppSettings::EnableVSync ? true : false);

    // Toggle stable power state
    if(AppSettings::StablePowerState != stablePowerState)
    {
        DX12::Device->SetStablePowerState(AppSettings::StablePowerState);
        stablePowerState = AppSettings::StablePowerState;
    }

    skyCache.Init(AppSettings::SunDirection, AppSettings::SunSize, AppSettings::GroundAlbedo, AppSettings::Turbidity, true);

    if(AppSettings::CurrentScene.Changed() && currentModel != &sceneModels[uint64_t(AppSettings::CurrentScene)])
    {
        currentModel = &sceneModels[uint64_t(AppSettings::CurrentScene)];
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
        &AppSettings::EnableSky,
        &AppSettings::EnableSun,
        &AppSettings::RenderLights,
        &AppSettings::SunSize,
        &AppSettings::SunDirection,
        &AppSettings::Turbidity,
        &AppSettings::GroundAlbedo,
        &AppSettings::RoughnessScale,
        &AppSettings::MetallicScale,
        &AppSettings::EnableWhiteFurnaceMode,
        &AppSettings::MaxAnyHitPathLength,
        &AppSettings::AvoidCausticPaths,
        &AppSettings::ClampRoughness,
        &AppSettings::ApplyMultiscatteringEnergyCompensation
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

    if(rtShouldRestartPathTrace)
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

    ID3D12GraphicsCommandList10* cmdList = DX12::CmdList;

    CPUProfileBlock cpuProfileBlock("Render");
    ProfileBlock gpuProfileBlock(cmdList, "Render Total");

    if (spotLights.Size() > 0)
    {
        // Update the light constant buffer
        MapResult staging = DX12::AcquireTempBufferMem(spotLightBuffer.InternalBuffer.Size, 0);
        memcpy(staging.CPUAddress, spotLights.Data(), spotLights.MemorySize());
        spotLightBuffer.QueueUpload(staging.Resource, staging.ResourceOffset, spotLightBuffer.InternalBuffer.Size, 0);
    }

    RenderRayTracing();

    {
        ProfileBlock ppProfileBlock(cmdList, "Post Processing");
        postProcessor.Render(cmdList, rtTarget, swapChain.BackBuffer());
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[1] = { swapChain.BackBuffer().RTV };
    cmdList->OMSetRenderTargets(1, rtvHandles, false, &depthBuffer.DSV);

    DX12::SetViewport(cmdList, swapChain.Width(), swapChain.Height());

    ShaderDebug::EndRender(DX12::CmdList, camera.ViewProjectionMatrix());

    RenderHUD(timer);
}

void DXRPathTracer::RenderRayTracing()
{
    // Don't keep tracing rays if we've hit our maximum per-pixel sample count
    if(rtCurrSampleIdx >= uint32_t(AppSettings::SqrtNumSamples * AppSettings::SqrtNumSamples))
        return;

    ID3D12GraphicsCommandList10* cmdList = DX12::CmdList;
    cmdList->SetComputeRootSignature(DX12::UniversalRootSignature);

    RayTraceConstants rtConstants;
    rtConstants.InvViewProjection = Float4x4::Invert(camera.ViewProjectionMatrix());

    rtConstants.SunDirectionWS = AppSettings::SunDirection;
    rtConstants.SunIrradiance = skyCache.SunIrradiance;
    rtConstants.CosSunAngularRadius = std::cos(DegToRad(AppSettings::SunSize));
    rtConstants.SinSunAngularRadius = std::sin(DegToRad(AppSettings::SunSize));
    rtConstants.SunRenderColor = skyCache.SunRenderColor;
    rtConstants.CameraPosWS = camera.Position();
    rtConstants.CurrSampleIdx = rtCurrSampleIdx;
    rtConstants.TotalNumPixels = uint32_t(rtTarget.Width()) * uint32_t(rtTarget.Height());

    rtConstants.VtxBufferIdx = currentModel->VertexBuffer().SRV;
    rtConstants.IdxBufferIdx = currentModel->IndexBuffer().SRV;
    rtConstants.GeometryInfoBufferIdx = rtGeoInfoBuffer.SRV;
    rtConstants.MaterialBufferIdx = materialBuffer.SRV;
    rtConstants.SkyTextureIdx = skyCache.CubeMap.SRV;
    rtConstants.NumLights = Min<uint32_t>(uint32_t(spotLights.Size()), AppSettings::MaxLightClamp);

    rtConstants.SceneAS = rtTopLevelAccelStructure.SRV;
    rtConstants.RenderTarget = rtTarget.UAV;

    DX12::BindTempConstantBufferToURS(cmdList, rtConstants, 0, CmdListMode::Compute);

    spotLightBuffer.SetAsComputeRootParameter(cmdList, URS_ConstantBuffers + 1);

    AppSettings::BindCBufferCompute(cmdList, URS_AppSettings);

    DX12::Barrier(cmdList, rtTarget.UAVWritableBarrier());

    cmdList->SetPipelineState1(rtPSO);

    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
    dispatchDesc.HitGroupTable = rtHitTable.ShaderTable();
    dispatchDesc.MissShaderTable = rtMissTable.ShaderTable();
    dispatchDesc.RayGenerationShaderRecord = rtRayGenTable.ShaderRecord(0);
    dispatchDesc.Width = uint32_t(rtTarget.Width());
    dispatchDesc.Height = uint32_t(rtTarget.Height());
    dispatchDesc.Depth = 1;

    DX12::CmdList->DispatchRays(&dispatchDesc);

    DX12::Barrier(cmdList, rtTarget.UAVToShaderReadableBarrier());

    //####################################################################
    {
        DX12::Barrier(cmdList, depthBuffer.DepthWritableBarrier({ .FirstAccess = true }));
        cmdList->ClearDepthStencilView(depthBuffer.DSV, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
        DX12::Barrier(cmdList, depthBuffer.DepthReadableBarrier());
    }

    rtCurrSampleIdx += 1;
}

void DXRPathTracer::RenderHUD(const Timer& timer)
{
    ID3D12GraphicsCommandList* cmdList = DX12::CmdList;
    PIXMarker pixMarker(cmdList, "HUD Pass");

    // Draw the progress bar
    const uint32_t totalNumSamples = uint32_t(AppSettings::SqrtNumSamples * AppSettings::SqrtNumSamples);
    if(rtCurrSampleIdx < totalNumSamples && AppSettings::ShowProgressBar)
    {
        float width = float(swapChain.Width());
        float height = float(swapChain.Height());

        const uint32_t barEmptyColor = ImColor(0.0f, 0.0f, 0.0f, 1.0f);
        const uint32_t barFilledColor = ImColor(1.0f, 0.0f, 0.0f, 1.0f);
        const uint32_t barOutlineColor = ImColor(1.0f, 1.0f, 1.0f, 1.0f);
        const uint32_t textColor = ImColor(1.0f, 1.0f, 1.0f, 1.0f);

        const float barPercentage = 0.75f;
        const float barHeight = 75.0f;
        Float2 barStart = Float2(width * (1.0f - barPercentage) * 0.5f, height - 200.0f);
        Float2 barSize = Float2(width * barPercentage, barHeight);
        Float2 barEnd = barStart + barSize;

        Float2 windowStart = barStart - 8.0f;
        Float2 windowSize = barSize + 16.0f;
        Float2 windowEnd = windowStart + windowSize;

        ImGui::SetNextWindowPos(ToImVec2(windowStart), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ToImVec2(windowSize), ImGuiCond_Always);
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

        const uint64_t raysPerFrame = rtTarget.Width() * rtTarget.Height() * (1 + (AppSettings::MaxPathLength - 1) * 2);
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

    const uint64_t numMeshes = currentModel->NumMeshes();
    Array<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs(numMeshes);

    const uint32_t numGeometries = uint32_t(geometryDescs.Size());
    Array<GeometryInfo> geoInfoBufferData(numGeometries);

    for(uint64_t meshIdx = 0; meshIdx < numMeshes; ++meshIdx)
    {
        const Mesh& mesh = currentModel->Meshes()[meshIdx];
        Assert_(mesh.NumMeshParts() == 1);
        const uint32_t materialIdx = mesh.MeshParts()[0].MaterialIdx;
        const MeshMaterial& material = currentModel->Materials()[materialIdx];
        const bool opaque = material.Textures[uint32_t(MaterialTextures::Opacity)] == nullptr;

        D3D12_RAYTRACING_GEOMETRY_DESC& geometryDesc = geometryDescs[meshIdx];
        geometryDesc = { };
        geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometryDesc.Triangles.IndexBuffer = idxBuffer.GPUAddress + mesh.IndexOffset() * idxBuffer.Stride;
        geometryDesc.Triangles.IndexCount = uint32_t(mesh.NumIndices());
        geometryDesc.Triangles.IndexFormat = idxBuffer.Format;
        geometryDesc.Triangles.Transform3x4 = 0;
        geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geometryDesc.Triangles.VertexCount = uint32_t(mesh.NumVertices());
        geometryDesc.Triangles.VertexBuffer.StartAddress = vtxBuffer.GPUAddress + mesh.VertexOffset() * vtxBuffer.Stride;
        geometryDesc.Triangles.VertexBuffer.StrideInBytes = vtxBuffer.Stride;
        geometryDesc.Flags = opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

        GeometryInfo& geoInfo = geoInfoBufferData[meshIdx];
        geoInfo = { };
        geoInfo.VtxOffset = uint32_t(mesh.VertexOffset());
        geoInfo.IdxOffset = uint32_t(mesh.IndexOffset());
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

    scratchBuffer.Initialize({
        .NumElements = Max(topLevelPrebuildInfo.ScratchDataSizeInBytes, bottomLevelPrebuildInfo.ScratchDataSizeInBytes) / RawBuffer::Stride,
        .CreateUAV = true,
        .Name = "RT Scratch Buffer",
    });

    rtBottomLevelAccelStructure.Initialize({
        .Size = bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes,
        .Name = "RT Bottom Level Accel Structure",
    });

    rtTopLevelAccelStructure.Initialize({
        .Size = topLevelPrebuildInfo.ResultDataMaxSizeInBytes,
        .Name = "RT Top Level Accel Structure",
    });

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
        topLevelBuildDesc.DestAccelerationStructureData = rtTopLevelAccelStructure.GPUAddress;
        topLevelBuildDesc.ScratchAccelerationStructureData = scratchBuffer.GPUAddress;
    }

    {
        // ProfileBlock profileBlock(cmdList, "Build Acceleration Structure");

        ID3D12GraphicsCommandList10* cmdList = DX12::CmdList;
        cmdList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

        {
            D3D12_RESOURCE_BARRIER barrier =
            {
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                barrier.UAV.pResource = rtBottomLevelAccelStructure.Resource(),
            };
            cmdList->ResourceBarrier(1, &barrier);
        }
        // DX12::Barrier(cmdList, rtBottomLevelAccelStructure.BottomLevelPostBuildBarrier());

        cmdList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);

        {
            D3D12_RESOURCE_BARRIER barrier =
            {
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                barrier.UAV.pResource = rtTopLevelAccelStructure.Resource(),
            };
            cmdList->ResourceBarrier(1, &barrier);
        }
        // DX12::Barrier(cmdList, rtTopLevelAccelStructure.TopLevelPostBuildBarrier());
    }

    scratchBuffer.Shutdown();

    rtGeoInfoBuffer.Initialize({
        .Stride = sizeof(GeometryInfo),
        .NumElements = numGeometries,
        .InitData = geoInfoBufferData.Data(),
        .Name = "Geometry Info Buffer",
    });

    buildAccelStructure = false;
    lastBuildAccelStructureFrame = DX12::CurrentCPUFrame;
}

int32_t APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int32_t nCmdShow)
{
    DXRPathTracer app(lpCmdLine);
    app.Run();
}
