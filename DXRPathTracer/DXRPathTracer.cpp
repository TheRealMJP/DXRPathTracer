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

struct SceneParameters
{
    const char* Path = nullptr;
    const char* TextureDir = nullptr;
    float Scale = 1.0f;
    Float3 CameraPosition;
    Float2 CameraRotation;
    Float3 SunDirection;
};

static const SceneParameters SceneParams[] =
{
    // BoxTest
    {
        .Path = nullptr,
        .TextureDir = nullptr,
        .Scale = 1.0f,
        .CameraPosition = Float3(0.0f, 2.5f, -10.0f),
        .CameraRotation = Float2(0.0f, 0.0f),
        .SunDirection = Float3(0.26f, 0.987f, -0.16f),
    },

    // Knob
    {
        .Path = "..\\Content\\Models\\MoriKnob\\MoriKnob.fbx",
        .TextureDir = nullptr,
        .Scale = 1.0f,
        .CameraPosition = Float3(0.125f, 0.125f, -2.75f),
        .CameraRotation = Float2(0.0f, 0.0f),
        .SunDirection = Float3(0.579f, 0.574f, -0.579f),
    },

    // Dragon
    {
        .Path = "..\\Content\\Models\\Dragon\\Dragon.fbx",
        .TextureDir = nullptr,
        .Scale = 1.0f,
        .CameraPosition = Float3(0.125f, 0.125f, -2.75f),
        .CameraRotation = Float2(0.0f, 0.0f),
        .SunDirection = Float3(0.579f, 0.574f, -0.579f),
    },


    // Sponza
    {
        .Path = "..\\Content\\Models\\Sponza\\Sponza.fbx",
        .TextureDir = nullptr,
        .Scale = 0.01f,
        .CameraPosition = Float3(-11.5f, 1.85f, -0.45f),
        .CameraRotation = Float2(0.0f, 1.544f),
        .SunDirection = Float3(0.26f, 0.987f, -0.16f),
    },

    // SunTemple
    {
        .Path = "..\\Content\\Models\\SunTemple\\SunTemple.fbx",
        .TextureDir = "Textures",
        .Scale = 0.005f,
        .CameraPosition = Float3(-1.0f, 5.5f, 12.0f),
        .CameraRotation = Float2(0.2f, 3.0f),
        .SunDirection = Float3(-0.133022308f, 0.642787635f, 0.75440651f),
    },

    // WhiteFurnace
    {
        .Path = "..\\Content\\Models\\WhiteFurnace\\WhiteFurnace.fbx",
        .TextureDir = nullptr,
        .Scale = 1.0f,
        .CameraPosition = Float3(0.0f, 0.0f, -3.0f),
        .CameraRotation = Float2(0.0f, 0.0f),
        .SunDirection = Float3(0.0f, 1.0f, 0.0f),
    },
};
StaticAssert_(ArraySize_(SceneParams) == uint64_t(Scenes::NumValues));

enum class MaterialPreset : int32_t
{
    Default = 0,
    Gold,
    Fog,
    Glass,

    NumPresets
};

static const char* PresetNames[] =
{
    "Default",
    "Gold",
    "Fog",
    "Glass",
};
StaticAssert_(ArraySize_(PresetNames) == int32_t(MaterialPreset::NumPresets));

static MaterialPreset materialPreset = MaterialPreset::Default;
static Material materialPresets[int32_t(MaterialPreset::NumPresets)];

static bool SceneIsEditable(Scenes scene)
{
    return scene == Scenes::Knob || scene == Scenes::Dragon;
}

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

    LoadTexture(whiteTexture, "..\\Content\\Textures\\Default.dds");
    LoadTexture(blackTexture, "..\\Content\\Textures\\DefaultBlack.dds");
    LoadTexture(checkerTexture, "..\\Content\\Textures\\DefaultBaseColor.dds");

    LoadTexture(flatNormalMap, "..\\Content\\Textures\\DefaultNormalMap.dds");
    LoadTexture(brickNormalMap, "..\\Content\\Textures\\Bricks_NML.dds");
    LoadTexture(ripplesNormalMap, "..\\Content\\Textures\\Ripples_Normal.png");
    LoadTexture(wavesNormalMap, "..\\Content\\Textures\\Waves_Normal.png");

    LoadTexture(goldBaseColor, "..\\Content\\Textures\\Gold\\Gold_Color.png", true);
    LoadTexture(goldNormalMap, "..\\Content\\Textures\\Gold\\Gold_Normal.png", false);
    LoadTexture(goldRoughness, "..\\Content\\Textures\\Gold\\Gold_Roughness.png", false);

    materialPresets[int32_t(MaterialPreset::Default)] =
    {
        .BaseColor = checkerTexture.SRV,
        .BaseColorTint = Half3(1.0f, 1.0f, 1.0f),
        .BaseColorIntensity = Half(0.75f),
        .Normal = flatNormalMap.SRV,
        .Roughness = whiteTexture.SRV,
        .NormalMapIntensity = Half(1.0f),
        .RoughnessScale = Half(0.1f),
        .Metallic = blackTexture.SRV,
        .Opacity = whiteTexture.SRV,
        .Emissive = whiteTexture.SRV,
        .MetallicOffset = Half(0.0f),
        .EmissiveTint = Half3(0.0f, 0.0f, 0.0f),
        .SigmaA = MaxSigma,
        .SigmaS = MaxSigma,
        .Flags = MaterialFlags_Default,
    };

    materialPresets[int32_t(MaterialPreset::Gold)] =
    {
        .BaseColor = goldBaseColor.SRV,
        .BaseColorTint = Half3(1.0f, 1.0f, 1.0f),
        .BaseColorIntensity = Half(1.0f),
        .Normal = goldNormalMap.SRV,
        .Roughness = goldRoughness.SRV,
        .NormalMapIntensity = Half(1.0f),
        .RoughnessScale = Half(1.0f),
        .Metallic = whiteTexture.SRV,
        .Opacity = whiteTexture.SRV,
        .Emissive = whiteTexture.SRV,
        .MetallicOffset = Half(0.0f),
        .EmissiveTint = Half3(0.0f, 0.0f, 0.0f),
        .SigmaA = MaxSigma,
        .SigmaS = MaxSigma,
        .Flags = MaterialFlags_Default,
    };

    materialPresets[int32_t(MaterialPreset::Fog)] =
    {
        .BaseColor = checkerTexture.SRV,
        .BaseColorTint = Half3(1.0f, 1.0f, 1.0f),
        .BaseColorIntensity = Half(0.75f),
        .Normal = flatNormalMap.SRV,
        .Roughness = whiteTexture.SRV,
        .NormalMapIntensity = Half(1.0f),
        .RoughnessScale = Half(0.1f),
        .Metallic = blackTexture.SRV,
        .Opacity = whiteTexture.SRV,
        .Emissive = whiteTexture.SRV,
        .MetallicOffset = Half(0.0f),
        .EmissiveTint = Half3(0.0f, 0.0f, 0.0f),
        .SigmaA = Half(5.0f),
        .SigmaS = Half(10.0f),
        .PhaseAnisotropy = Half(0.0f),
        .Flags = 0,
    };

    materialPresets[int32_t(MaterialPreset::Glass)] =
    {
        .BaseColor = checkerTexture.SRV,
        .BaseColorTint = Half3(1.0f, 1.0f, 1.0f),
        .BaseColorIntensity = Half(0.75f),
        .Normal = flatNormalMap.SRV,
        .Roughness = whiteTexture.SRV,
        .NormalMapIntensity = Half(1.0f),
        .RoughnessScale = Half(0.1f),
        .Metallic = blackTexture.SRV,
        .Opacity = whiteTexture.SRV,
        .Emissive = whiteTexture.SRV,
        .MetallicOffset = Half(0.0f),
        .EmissiveTint = Half3(0.0f, 0.0f, 0.0f),
        .SigmaA = Half(0.01f),
        .SigmaS = Half(0.01f),
        .PhaseAnisotropy = Half(0.0f),
        .Flags = MaterialFlags_Default,
    };

    editedMaterial = materialPresets[int32_t(materialPreset)];
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

    rtTarget.Shutdown();
    rtDepthTarget.Shutdown();
    depthBuffer.Shutdown();

    rtBottomLevelAccelStructure.Shutdown();
    rtTopLevelAccelStructure.Shutdown();
    rtRayGenTable.Shutdown();
    rtHitTable.Shutdown();
    rtMissTable.Shutdown();
    rtGeoInfoBuffer.Shutdown();

    whiteTexture.Shutdown();
    blackTexture.Shutdown();
    checkerTexture.Shutdown();

    flatNormalMap.Shutdown();
    brickNormalMap.Shutdown();
    ripplesNormalMap.Shutdown();
    wavesNormalMap.Shutdown();

    goldBaseColor.Shutdown();
    goldNormalMap.Shutdown();
    goldRoughness.Shutdown();
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

    rtTarget.Initialize({
        .Width = width,
        .Height = height,
        .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        .CreateUAV = true,
        .InitialLayout = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE,
        .Name = "RT Target",
    });

    rtDepthTarget.Initialize({
        .Width = width,
        .Height = height,
        .Format = DXGI_FORMAT_R32_FLOAT,
        .CreateUAV = true,
        .InitialLayout = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE,
        .Name = "RT Depth Target",
    });

    depthBuffer.Initialize({
        .Width = width,
        .Height = height,
        .Format = DXGI_FORMAT_D32_FLOAT,
        .Name = "Main Depth Buffer",
    });

    rtShouldRestartPathTrace = true;
}

void DXRPathTracer::InitializeScene()
{
    const uint64_t currSceneIdx = uint64_t(AppSettings::CurrentScene);
    const SceneParameters& currSceneParams = SceneParams[currSceneIdx];

    // Load the scene (if necessary)
    if(sceneModels[currSceneIdx].NumMeshes() == 0)
    {
        if(currSceneIdx == uint64_t(Scenes::BoxTest) || currSceneParams.Path == nullptr)
        {
            sceneModels[currSceneIdx].GenerateBoxTestScene({});
        }
        else
        {
            ModelLoadSettings settings;
            settings.FilePath = currSceneParams.Path;
            settings.TextureDir = currSceneParams.TextureDir;
            settings.ForceSRGB = true;
            settings.SceneScale = currSceneParams.Scale;
            settings.MergeMeshes = false;
            sceneModels[currSceneIdx].CreateWithAssimp(settings);
        }
    }

    currentModel = &sceneModels[currSceneIdx];
    DX12::FlushGPU();

    materialBuffer.Shutdown();

    camera.SetPosition(currSceneParams.CameraPosition);
    camera.SetXRotation(currSceneParams.CameraRotation.x);
    camera.SetYRotation(currSceneParams.CameraRotation.y);
    AppSettings::SunDirection.SetValue(currSceneParams.SunDirection);

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
        materialBuffer.Initialize({
            .Stride = sizeof(Material),
            .NumElements = numMaterials,
            .Dynamic = true,
            // .InitData = matBufferData.Data(),
            .Name = "Material Buffer",
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
        D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = { };
        shaderConfig.MaxAttributeSizeInBytes = 2 * sizeof(float);                           // float2 barycentrics;
        shaderConfig.MaxPayloadSizeInBytes = 3 * sizeof(float) + 2 * sizeof(uint32_t) * 2;  // float HitT + uint HitGeometryIndex + uint HitTriangleIndex + float2 HitBarycentrics + bool IsFrontFace
        builder.AddSubObject(shaderConfig);
    }

    {
        // Global root signature with all of our normal bindings
        D3D12_GLOBAL_ROOT_SIGNATURE globalRSDesc = { };
        globalRSDesc.pGlobalRootSignature = DX12::UniversalRootSignature;
        builder.AddSubObject(globalRSDesc);
    }

    {
        // The path tracer is not recursive, we don't cast rays from inside of hit shaders
        D3D12_RAYTRACING_PIPELINE_CONFIG configDesc = { };
        configDesc.MaxTraceRecursionDepth = 0;
        builder.AddSubObject(configDesc);
    }

    rtPSO = builder.CreateStateObject(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

    // Get shader identifiers (for making shader records)
    ID3D12StateObjectProperties* psoProps = nullptr;
    rtPSO->QueryInterface(IID_PPV_ARGS(&psoProps));

    const void* rayGenID = psoProps->GetShaderIdentifier(L"RaygenShader");
    const void* hitGroupID = psoProps->GetShaderIdentifier(L"HitGroup");
    const void* alphaTestHitGroupID = psoProps->GetShaderIdentifier(L"AlphaTestHitGroup");
    const void* missID = psoProps->GetShaderIdentifier(L"MissShader");

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
        ShaderIdentifier missRecords[1] = { ShaderIdentifier(missID) };

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

        Array<HitGroupRecord> hitGroupRecords(numMeshes);
        for(uint64_t i = 0; i < numMeshes; ++i)
        {
            // Use the alpha test hit group (with an any hit shader) if the material has an opacity map
            const Mesh& mesh = currentModel->Meshes()[i];
            Assert_(mesh.NumMeshParts() == 1);
            const uint32_t materialIdx = mesh.MeshParts()[0].MaterialIdx;
            const MeshMaterial& material = currentModel->Materials()[materialIdx];
            const bool alphaTest = material.Textures[uint32_t(MaterialTextures::Opacity)] != nullptr;

            hitGroupRecords[i].ID = alphaTest ? ShaderIdentifier(alphaTestHitGroupID) : ShaderIdentifier(hitGroupID);
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

    if(AppSettings::CurrentScene.Changed() && currentModel != &sceneModels[uint64_t(AppSettings::CurrentScene)])
    {
        currentModel = &sceneModels[uint64_t(AppSettings::CurrentScene)];
        DestroyPSOs();
        InitializeScene();
        CreatePSOs();

        rtShouldRestartPathTrace = true;
    }

    skyCache.Init(AppSettings::SunDirection, AppSettings::SunSize, AppSettings::GroundAlbedo, AppSettings::Turbidity, true);

    const Setting* settingsToCheck[] =
    {
        &AppSettings::SqrtNumSamples,
        &AppSettings::MaxPathLength,
        &AppSettings::EnableBaseColorMaps,
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
        &AppSettings::MetallicOffset,
        &AppSettings::MaxAnyHitPathLength,
        &AppSettings::AvoidCausticPaths,
        &AppSettings::ClampRoughness,
        &AppSettings::ApplyMultiscatteringEnergyCompensation,
        &AppSettings::EnableDirectLightSampling,
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

    {
        // Update the material buffer
        const Array<MeshMaterial>& meshMaterials = currentModel->Materials();
        const uint64_t numMaterials = meshMaterials.Size();

        MapResult stagingMem = DX12::AcquireTempBufferMem(numMaterials * sizeof(Material), alignof(Material));
        Material* materials = reinterpret_cast<Material*>(stagingMem.CPUAddress);

        Array<Material> matBufferData(numMaterials);
        for(uint64_t i = 0; i < numMaterials; ++i)
        {
            const MeshMaterial& meshMaterial = meshMaterials[i];

            Material material;
            material.BaseColor = meshMaterial.Textures[uint64_t(MaterialTextures::Albedo)]->SRV;
            material.BaseColorTint = Half3(1.0f, 1.0f, 1.0f);
            material.BaseColorIntensity = Half(1.0f);
            material.Normal = meshMaterial.Textures[uint64_t(MaterialTextures::Normal)]->SRV;
            material.NormalMapIntensity = Half(1.0f);
            material.Roughness = meshMaterial.Textures[uint64_t(MaterialTextures::Roughness)]->SRV;
            material.RoughnessScale = Half(1.0f);
            material.Metallic = meshMaterial.Textures[uint64_t(MaterialTextures::Metallic)]->SRV;
            material.MetallicOffset = Half(0.0f);
            material.Emissive = meshMaterial.Textures[uint64_t(MaterialTextures::Emissive)]->SRV;
            material.EmissiveTint = Half3(1.0f, 1.0f, 1.0f);

            // Opacity is optional
            const Texture* opacity = meshMaterial.Textures[uint64_t(MaterialTextures::Opacity)];
            material.Opacity = opacity ? opacity->SRV : InvalidDescriptorIndex;

            material.SigmaA = MaxSigma;
            material.SigmaS = MaxSigma;
            material.Flags = MaterialFlags_Default;

            if(AppSettings::CurrentScene == Scenes::WhiteFurnace)
            {
                material.BaseColor = whiteTexture.SRV;
                material.Metallic = whiteTexture.SRV;
                material.Roughness = whiteTexture.SRV;
            }
            else if(SceneIsEditable(AppSettings::CurrentScene) && i == 1)
            {
                material = editedMaterial;
            }

            memcpy(materials + i, &material, sizeof(material));
        }

        materialBuffer.QueueUpload(stagingMem.Resource, stagingMem.ResourceOffset, numMaterials, 0);
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
    rtConstants.ViewProjection = camera.ViewProjectionMatrix();

    rtConstants.SunDirectionWS = AppSettings::SunDirection;
    rtConstants.SunIrradiance = skyCache.SunIrradiance;
    rtConstants.CosSunAngularRadius = std::cos(DegToRad(AppSettings::SunSize));
    rtConstants.SinSunAngularRadius = std::sin(DegToRad(AppSettings::SunSize));
    rtConstants.SunRenderColor = skyCache.SunRenderColor;
    rtConstants.EnableWhiteFurnaceMode = AppSettings::CurrentScene == Scenes::WhiteFurnace ? 1 : 0;
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
    rtConstants.DepthTarget = rtDepthTarget.UAV;

    DX12::BindTempConstantBufferToURS(cmdList, rtConstants, 0, CmdListMode::Compute);

    spotLightBuffer.SetAsComputeRootParameter(cmdList, URS_ConstantBuffers + 1);

    AppSettings::BindCBufferCompute(cmdList, URS_AppSettings);

    {
        BarrierBatchBuilder builder;
        builder.Add(rtTarget.UAVWritableBarrier());
        builder.Add(rtDepthTarget.UAVWritableBarrier({ .FirstAccess = true }));
        DX12::Barrier(cmdList, builder.Build());
    }

    cmdList->SetPipelineState1(rtPSO);

    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
    dispatchDesc.HitGroupTable = rtHitTable.ShaderTable();
    dispatchDesc.MissShaderTable = rtMissTable.ShaderTable();
    dispatchDesc.RayGenerationShaderRecord = rtRayGenTable.ShaderRecord(0);
    dispatchDesc.Width = uint32_t(rtTarget.Width());
    dispatchDesc.Height = uint32_t(rtTarget.Height());
    dispatchDesc.Depth = 1;

    DX12::CmdList->DispatchRays(&dispatchDesc);

    {
        BarrierBatchBuilder builder;
        builder.Add(rtTarget.UAVToShaderReadableBarrier());

        if(rtCurrSampleIdx == 0)
        {
            builder.Add({
                .SyncBefore = D3D12_BARRIER_SYNC_ALL_SHADING,
                .SyncAfter = D3D12_BARRIER_SYNC_COPY,
                .AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
                .AccessAfter = D3D12_BARRIER_ACCESS_COPY_SOURCE,
                .LayoutBefore = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS,
                .LayoutAfter = D3D12_BARRIER_LAYOUT_COPY_SOURCE,
                .pResource = rtDepthTarget.Resource(),
                .Subresources = rtDepthTarget.Texture.BarrierRange(0, 1, 0, 1),
                .Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE,
            });
            builder.Add({
                .SyncBefore = D3D12_BARRIER_SYNC_NONE,
                .SyncAfter = D3D12_BARRIER_SYNC_COPY,
                .AccessBefore = D3D12_BARRIER_ACCESS_NO_ACCESS,
                .AccessAfter = D3D12_BARRIER_ACCESS_COPY_DEST,
                .LayoutBefore = D3D12_BARRIER_LAYOUT_UNDEFINED,
                .LayoutAfter = D3D12_BARRIER_LAYOUT_COPY_DEST,
                .pResource = depthBuffer.Resource(),
                .Subresources = depthBuffer.Texture.BarrierRange(0, 1, 0, 1),
                .Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE,
            });
        }
        DX12::Barrier(cmdList, builder.Build());
    }

    if(rtCurrSampleIdx == 0)
    {
        cmdList->CopyResource(depthBuffer.Resource(), rtDepthTarget.Resource());

        DX12::Barrier(cmdList,
        {
            .SyncBefore = D3D12_BARRIER_SYNC_COPY,
            .SyncAfter = D3D12_BARRIER_SYNC_ALL_SHADING | D3D12_BARRIER_SYNC_DEPTH_STENCIL,
            .AccessBefore = D3D12_BARRIER_ACCESS_COPY_DEST,
            .AccessAfter = D3D12_BARRIER_ACCESS_SHADER_RESOURCE | D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ,
            .LayoutBefore = D3D12_BARRIER_LAYOUT_COPY_DEST,
            .LayoutAfter = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ,
            .pResource = depthBuffer.Resource(),
            .Subresources = depthBuffer.Texture.BarrierRange(0, 1, 0, 1),
            .Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE,
        });
    }

    rtCurrSampleIdx += 1;
}

struct DescriptorNamePair
{
    DescriptorIndex Index;
    const char* Name;
};

template<int32_t N> bool TextureCombo(const char* name, DescriptorIndex* descriptorIndex, DescriptorNamePair const (&options)[N])
{
    int32_t currentSelection = 0;
    for (int32_t i = 0; i < N; ++i)
    {
        if (options[i].Index == *descriptorIndex)
        {
            currentSelection = i;
            break;
        }
    }

    if (ImGui::BeginCombo(name, options[currentSelection].Name))
    {
        int32_t selection = -1;
        for (int32_t i = 0; i < N; ++i)
        {
            if (ImGui::Selectable(options[i].Name, i == currentSelection))
                selection = i;
        }
        ImGui::EndCombo();

        if (selection != -1 && selection != currentSelection)
        {
            *descriptorIndex = options[selection].Index;
            return true;
        }
    }

    return false;
}

static bool SliderHalf(const char* name, Half* hv, float minVal, float maxVal, const char* format = "%.3f", ImGuiSliderFlags flags = ImGuiSliderFlags_None)
{
    float fv = hv->ToFloat();
    bool changed = ImGui::SliderFloat(name, &fv, minVal, maxVal, format, flags);
    if (changed)
        *hv = Half(fv);
    return changed;
}

static bool ColorEditHalf3(const char* name, Half3* hc)
{
    Float3 fc = hc->ToFloat3();
    bool changed = ImGui::ColorEdit3(name, &fc.x);
    if (changed)
        *hc = Half3(fc);
    return changed;
}

static bool CheckboxFlagsUint16(const char* name, uint16_t* flags, uint16_t flagsValue)
{
    uint32_t flags32 = *flags;
    bool changed = ImGui::CheckboxFlags(name, &flags32, uint32_t(flagsValue));
    *flags = uint16_t(flags32);
    return changed;
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

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();

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
    }

    if(SceneIsEditable(AppSettings::CurrentScene))
    {
        ImGui::SetNextWindowBgAlpha(0.5f);
        if(ImGui::Begin("Material Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            bool changed = ImGui::Combo("Preset", reinterpret_cast<int32_t*>(&materialPreset), PresetNames, int32_t(MaterialPreset::NumPresets));
            if(changed)
            {
                Assert_(int32_t(materialPreset) >= 0 && int32_t(materialPreset) < ArraySize_(materialPresets));
                editedMaterial = materialPresets[int32_t(materialPreset)];
            }

            ImGui::Separator();

            changed |= TextureCombo("Base Color Map", &editedMaterial.BaseColor,
            {
                { whiteTexture.SRV, "White" },
                { blackTexture.SRV, "Black" },
                { checkerTexture.SRV, "Checker" },
                { goldBaseColor.SRV, "Gold" },
            });
            changed |= ColorEditHalf3("Base Color Tint", &editedMaterial.BaseColorTint);
            changed |= SliderHalf("Base Color Intensity", &editedMaterial.BaseColorIntensity, 0.0f, 1.0f);

            changed |= TextureCombo("Normal Map", &editedMaterial.Normal,
            {
                { flatNormalMap.SRV, "Flat" },
                { brickNormalMap.SRV, "Bricks" },
                { ripplesNormalMap.SRV, "Ripples" },
                { wavesNormalMap.SRV, "Waves" },
                { goldNormalMap.SRV, "Gold" },
            });
            changed |= SliderHalf("Normal Map Intensity", &editedMaterial.NormalMapIntensity, 0.0f, 1.0f);

            changed |= SliderHalf("Roughness Scale", &editedMaterial.RoughnessScale, 0.0f, 2.0f);
            changed |= TextureCombo("Roughness Map", &editedMaterial.Roughness,
            {
                { whiteTexture.SRV, "White" },
                { blackTexture.SRV, "Black" },
                { checkerTexture.SRV, "Checker" },
                { goldRoughness.SRV, "Gold" },
            });

            changed |= SliderHalf("Metallic Offset", &editedMaterial.MetallicOffset, -1.0f, 1.0f);
            changed |= TextureCombo("Metallic Map", &editedMaterial.Metallic,
            {
                { whiteTexture.SRV, "White" },
                { blackTexture.SRV, "Black" },
            });

            changed |= ColorEditHalf3("Emissive Tint", &editedMaterial.EmissiveTint);

            changed |= SliderHalf("Absorption Coefficient", &editedMaterial.SigmaA, MinSigma.ToFloat(), MaxSigma.ToFloat(), "%.3f", ImGuiSliderFlags_Logarithmic);
            changed |= SliderHalf("Scattering Coefficient", &editedMaterial.SigmaS, MinSigma.ToFloat(), MaxSigma.ToFloat(), "%.3f", ImGuiSliderFlags_Logarithmic);
            changed |= SliderHalf("Phase Anisotropy", &editedMaterial.PhaseAnisotropy, -1.0f, 1.0f);

            changed |= CheckboxFlagsUint16("Enable Specular", &editedMaterial.Flags.Value, MaterialFlags_EnableSpecular);

            if (changed)
                rtShouldRestartPathTrace = true;
        }

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
        geometryDesc.Flags = material.Opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

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
