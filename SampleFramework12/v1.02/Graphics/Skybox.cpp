//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "Skybox.h"

#include "../Utility.h"
#include "../SF12_Math.h"
#include "../HosekSky/ArHosekSkyModel.h"
#include "ShaderCompilation.h"
#include "Textures.h"
#include "Spectrum.h"
#include "Sampling.h"
#include "DX12.h"

namespace SampleFramework12
{

static const uint64 NumIndices = 36;
static const uint64 NumVertices = 8;

#if EnableSkyModel_

// Actual physical size of the sun, expressed as an angular radius (in radians)
static const float PhysicalSunSize = DegToRad(0.27f);
static const float CosPhysicalSunSize = std::cos(PhysicalSunSize);

static float AngleBetween(const Float3& dir0, const Float3& dir1)
{
    return std::acos(std::max(Float3::Dot(dir0, dir1), 0.00001f));
}

// Returns the result of performing a irradiance integral over the portion
// of the hemisphere covered by a region with angular radius = theta
static float IrradianceIntegral(float theta)
{
    float sinTheta = std::sin(theta);
    return Pi * sinTheta * sinTheta;
}

bool SkyCache::Init(const Float3& sunDirection_, float sunSize, const Float3& groundAlbedo_, float turbidity, bool createCubemap)
{
    Float3 sunDirection = sunDirection_;
    Float3 groundAlbedo = groundAlbedo_;
    sunDirection.y = Saturate(sunDirection.y);
    sunDirection = Float3::Normalize(sunDirection);
    turbidity = Clamp(turbidity, 1.0f, 32.0f);
    groundAlbedo = Saturate(groundAlbedo);
    sunSize = Max(sunSize, 0.01f);

    // Do nothing if we're already up-to-date
    if(Initialized() && sunDirection == SunDirection && groundAlbedo == Albedo && turbidity == Turbidity && SunSize == sunSize)
        return false;

    Shutdown();

    sunDirection.y = Saturate(sunDirection.y);
    sunDirection = Float3::Normalize(sunDirection);
    turbidity = Clamp(turbidity, 1.0f, 32.0f);
    groundAlbedo = Saturate(groundAlbedo);

    float thetaS = AngleBetween(sunDirection, Float3(0, 1, 0));
    float elevation = Pi_2 - thetaS;
    StateR = arhosek_rgb_skymodelstate_alloc_init(turbidity, groundAlbedo.x, elevation);
    StateG = arhosek_rgb_skymodelstate_alloc_init(turbidity, groundAlbedo.y, elevation);
    StateB = arhosek_rgb_skymodelstate_alloc_init(turbidity, groundAlbedo.z, elevation);

    Albedo = groundAlbedo;
    Elevation = elevation;
    SunDirection = sunDirection;
    Turbidity = turbidity;
    SunSize = sunSize;

    // Compute the irradiance of the sun for a surface perpendicular to the sun using monte carlo integration.
    // Note that the solar radiance function provided by the authors of this sky model only works using
    // spectral rendering, so we sample a range of wavelengths and then convert to RGB.
    SampledSpectrum groundAlbedoSpectrum = SampledSpectrum::FromRGB(Albedo, SpectrumType::Reflectance);
    SampledSpectrum solarRadiance;

    // Init the Hosek solar radiance model for all wavelengths
    ArHosekSkyModelState* skyStates[NumSpectralSamples] = { };
    for(int32 i = 0; i < NumSpectralSamples; ++i)
        skyStates[i] = arhosekskymodelstate_alloc_init(thetaS, turbidity, groundAlbedoSpectrum[i]);

    SunIrradiance = Float3(0.0f);

    // Uniformly sample the solid area of the solar disc.
    // Note that we use the *actual* sun size here and not the passed in the sun direction, so that
    // we always end up with the appropriate intensity. This allows changing the size of the sun
    // as it appears in the skydome without actually changing the sun intensity.
    Float3 sunDirX = Float3::Perpendicular(sunDirection);
    Float3 sunDirY = Float3::Cross(sunDirection, sunDirX);
    Float3x3 sunOrientation = Float3x3(sunDirX, sunDirY, sunDirection);

    const uint64 NumSamples = 8;
    for(uint64 x = 0; x < NumSamples; ++x)
    {
        for(uint64 y = 0; y < NumSamples; ++y)
        {
            float u1 = (x + 0.5f) / NumSamples;
            float u2 = (y + 0.5f) / NumSamples;
            Float3 sampleDir = SampleDirectionCone(u1, u2, CosPhysicalSunSize);
            sampleDir = Float3::Transform(sampleDir, sunOrientation);

            float sampleThetaS = AngleBetween(sampleDir, Float3(0, 1, 0));
            float sampleGamma = AngleBetween(sampleDir, sunDirection);

            for(int32 i = 0; i < NumSpectralSamples; ++i)
            {
                float wavelength = Lerp(float(SampledLambdaStart), float(SampledLambdaEnd), i / float(NumSpectralSamples));
                solarRadiance[i] = float(arhosekskymodel_solar_radiance(skyStates[i], sampleThetaS, sampleGamma, wavelength));
            }

            Float3 sampleRadiance = solarRadiance.ToRGB();

            // Pre-scale by our FP16 scaling factor, so that we can use the irradiance value
            // and have the resulting lighting still fit comfortably in an FP16 render target
            sampleRadiance *= FP16Scale;

            SunIrradiance += sampleRadiance * Saturate(Float3::Dot(sampleDir, sunDirection));
        }
    }

    // Apply the monte carlo factor of 1 / (PDF * N)
    float pdf = SampleDirectionCone_PDF(CosPhysicalSunSize);
    SunIrradiance *= (1.0f / NumSamples) * (1.0f / NumSamples) * (1.0f / pdf);

    // Account for luminous efficiency and coordinate system scaling
    SunIrradiance *= 683.0f * 100.0f;

    // Clean up
    for(uint64 i = 0; i < NumSpectralSamples; ++i)
    {
        arhosekskymodelstate_free(skyStates[i]);
        skyStates[i] = nullptr;
    }

    // Compute a uniform solar radiance value such that integrating this radiance over a disc with
    // the provided angular radius
    SunRadiance = SunIrradiance / IrradianceIntegral(DegToRad(SunSize));

    // Compute a (clamped) RGB value for direct rendering of the sun
    Float3 sunColor = SunRadiance;
    float maxComponent = Max(sunColor.x, Max(sunColor.y, sunColor.z));
    if(maxComponent > FP16Max)
        sunColor *= (FP16Max / maxComponent);
    SunRenderColor = Float3::Clamp(sunColor, 0.0f, FP16Max);

    if(createCubemap)
    {
        // Make a pre-computed cubemap with the sky radiance values, minus the sun.
        // For this we again pre-scale by our FP16 scale factor so that we can use an FP16 format.
        const uint64 CubeMapRes = 128;
        const uint64 NumTexels = CubeMapRes * CubeMapRes * 6;
        Array<Float3> samples(NumTexels);
        Array<Float3> sampleDirs(NumTexels);
        Array<Half4> texels(NumTexels);

        // We'll also project the sky onto SH coefficients for use during rendering
        SH = SH9Color();
        float weightSum = 0.0f;

        for(uint64 s = 0; s < 6; ++s)
        {
            for(uint64 y = 0; y < CubeMapRes; ++y)
            {
                for(uint64 x = 0; x < CubeMapRes; ++x)
                {
                    Float3 dir = MapXYSToDirection(x, y, s, CubeMapRes, CubeMapRes);
                    Float3 radiance = Sample(dir);

                    uint64 idx = (s * CubeMapRes * CubeMapRes) + (y * CubeMapRes) + x;
                    samples[idx] = radiance;
                    texels[idx] = Half4(Float4(radiance, 1.0f));
                    sampleDirs[idx] = dir;

                    float u = (x + 0.5f) / CubeMapRes;
                    float v = (y + 0.5f) / CubeMapRes;

                    // Account for cubemap texel distribution
                    u = u * 2.0f - 1.0f;
                    v = v * 2.0f - 1.0f;
                    const float temp = 1.0f + u * u + v * v;
                    const float weight = 4.0f / (std::sqrt(temp) * temp);

                    SH += ProjectOntoSH9Color(dir, radiance) * weight;
                    weightSum += weight;
                }
            }
        }

        SH *= (4.0f * 3.14159f) / weightSum;

        Create2DTexture(CubeMap, CubeMapRes, CubeMapRes, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, true, texels.Data());

        SGSolveParams solveParams;
        solveParams.SampleDirs = sampleDirs.Data();
        solveParams.SampleValues = samples.Data();
        solveParams.NumSamples = NumTexels;
        solveParams.SolveMode = SGSolveMode::NNLS;
        solveParams.Distribution = SGDistribution::Spherical;
        solveParams.NumSGs = 9;
        solveParams.OutSGs = SG.Lobes;
        SolveSGs(solveParams);
    }

    return true;
}

void SkyCache::Shutdown()
{
    if(StateR != nullptr)
    {
        arhosekskymodelstate_free(StateR);
        StateR = nullptr;
    }

    if(StateG != nullptr)
    {
        arhosekskymodelstate_free(StateG);
        StateG = nullptr;
    }

    if(StateB != nullptr)
    {
        arhosekskymodelstate_free(StateB);
        StateB = nullptr;
    }

    CubeMap.Shutdown();
    Turbidity = 0.0f;
    Albedo = 0.0f;
    Elevation = 0.0f;
    SunDirection = 0.0f;
    SunRadiance = 0.0f;
    SunIrradiance = 0.0f;
    SH = SH9Color();
}

SkyCache::~SkyCache()
{
    Assert_(Initialized() == false);
}

Float3 SkyCache::Sample(Float3 sampleDir) const
{
    Assert_(StateR != nullptr);

    float gamma = AngleBetween(sampleDir, SunDirection);
    float theta = AngleBetween(sampleDir, Float3(0, 1, 0));

    Float3 radiance;

    radiance.x = float(arhosek_tristim_skymodel_radiance(StateR, theta, gamma, 0));
    radiance.y = float(arhosek_tristim_skymodel_radiance(StateG, theta, gamma, 1));
    radiance.z = float(arhosek_tristim_skymodel_radiance(StateB, theta, gamma, 2));

    // Multiply by standard luminous efficacy of 683 lm/W to bring us in line with the photometric
    // units used during rendering
    radiance *= 683.0f;

    return radiance * FP16Scale;
}

#endif // EnableSkyModel_

// == Skybox ======================================================================================

enum RootParams : uint32
{
    RootParam_StandardDescriptors = 0,
    RootParam_VSCBuffer,
    RootParam_PSCBuffer,

    NumRootParams
};

Skybox::Skybox()
{
}

Skybox::~Skybox()
{
}

void Skybox::Initialize()
{
    // Load the shaders
    const std::wstring shaderPath = SampleFrameworkDir() + L"Shaders\\Skybox.hlsl";
    vertexShader = CompileFromFile(shaderPath.c_str(), "SkyboxVS", ShaderType::Vertex);
    pixelShader = CompileFromFile(shaderPath.c_str(), "SkyboxPS", ShaderType::Pixel);

    {
        // Make a root signature
        D3D12_ROOT_PARAMETER1 rootParameters[NumRootParams] = { };
        rootParameters[RootParam_StandardDescriptors].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[RootParam_StandardDescriptors].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[RootParam_StandardDescriptors].DescriptorTable.pDescriptorRanges = DX12::StandardDescriptorRanges();
        rootParameters[RootParam_StandardDescriptors].DescriptorTable.NumDescriptorRanges = DX12::NumStandardDescriptorRanges;

        rootParameters[RootParam_VSCBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[RootParam_VSCBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParameters[RootParam_VSCBuffer].Descriptor.RegisterSpace = 0;
        rootParameters[RootParam_VSCBuffer].Descriptor.ShaderRegister = 0;

        rootParameters[RootParam_PSCBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[RootParam_PSCBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[RootParam_PSCBuffer].Descriptor.RegisterSpace = 0;
        rootParameters[RootParam_PSCBuffer].Descriptor.ShaderRegister = 0;

        D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = { };
        staticSamplers[0] = DX12::GetStaticSamplerState(SamplerState::LinearClamp);

        D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = { };
        rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.NumStaticSamplers = 1;
        rootSignatureDesc.pStaticSamplers = staticSamplers;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        DX12::CreateRootSignature(&rootSignature, rootSignatureDesc);
    }

    // Create and initialize the vertex and index buffers
    Float3 verts[NumVertices] =
    {
        Float3(-1.0f, 1.0f, 1.0f),
        Float3(1.0f, 1.0f, 1.0f),
        Float3(1.0f, -1.0f, 1.0f),
        Float3(-1.0f, -1.0f, 1.0f),
        Float3(1.0f, 1.0f, -1.0f),
        Float3(-1.0f, 1.0f, -1.0f),
        Float3(-1.0f, -1.0f, -1.0f),
        Float3(1.0f, -1.0f, -1.0f),
    };

    StructuredBufferInit vbInit;
    vbInit.Stride = sizeof(Float3);
    vbInit.NumElements = uint32(NumVertices);
    vbInit.InitData = verts;
    vbInit.InitialState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    vbInit.Name = L"Skybox Vertex Buffer";
    vertexBuffer.Initialize(vbInit);

    uint16 indices[NumIndices] =
    {
        0, 1, 2, 2, 3, 0,   // Front
        1, 4, 7, 7, 2, 1,   // Right
        4, 5, 6, 6, 7, 4,   // Back
        5, 0, 3, 3, 6, 5,   // Left
        5, 4, 1, 1, 0, 5,   // Top
        3, 2, 7, 7, 6, 3    // Bottom
    };

    FormattedBufferInit ibInit;
    ibInit.Format = DXGI_FORMAT_R16_UINT;
    ibInit.NumElements = uint32(NumIndices);
    ibInit.InitialState = D3D12_RESOURCE_STATE_INDEX_BUFFER;
    ibInit.InitData = indices;
    ibInit.Name = L"SpriteRenderer Index Buffer";
    indexBuffer.Initialize(ibInit);
}

void Skybox::Shutdown()
{
    DestroyPSOs();

    vertexBuffer.Shutdown();
    indexBuffer.Shutdown();
    DX12::Release(rootSignature);
}

void Skybox::CreatePSOs(DXGI_FORMAT rtFormat, DXGI_FORMAT depthFormat, uint32 numMSAASamples)
{
    D3D12_INPUT_ELEMENT_DESC inputElements[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = { };
    psoDesc.pRootSignature = rootSignature;
    psoDesc.VS = vertexShader.ByteCode();
    psoDesc.PS = pixelShader.ByteCode();
    psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::NoCull);
    psoDesc.BlendState = DX12::GetBlendState(BlendState::Disabled);
    psoDesc.DepthStencilState = DX12::GetDepthState(DepthState::Enabled);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = rtFormat;
    psoDesc.DSVFormat = depthFormat;
    psoDesc.SampleDesc.Count = numMSAASamples;
    psoDesc.SampleDesc.Quality = numMSAASamples > 1 ? DX12::StandardMSAAPattern : 0;
    psoDesc.InputLayout.pInputElementDescs = inputElements;
    psoDesc.InputLayout.NumElements = ArraySize_(inputElements);
    DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)));
}

void Skybox::DestroyPSOs()
{
    DX12::DeferredRelease(pipelineState);
}

void Skybox::RenderCommon(ID3D12GraphicsCommandList* cmdList, const Texture* environmentMap,
                          const Float4x4& view, const Float4x4& projection, Float3 scale)
{
    // Set pipeline state
    cmdList->SetPipelineState(pipelineState);
    cmdList->SetGraphicsRootSignature(rootSignature);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    DX12::BindStandardDescriptorTable(cmdList, RootParam_StandardDescriptors, CmdListMode::Graphics);

    // Bind the constant buffers
    vsConstants.View = view;
    vsConstants.Projection = projection;
    DX12::BindTempConstantBuffer(cmdList, vsConstants, RootParam_VSCBuffer, CmdListMode::Graphics);

    psConstants.Scale = scale;
    psConstants.EnvMapIdx = environmentMap->SRV;
    DX12::BindTempConstantBuffer(cmdList, psConstants, RootParam_PSCBuffer, CmdListMode::Graphics);

    // Set vertex/index buffers
    D3D12_VERTEX_BUFFER_VIEW vbView = vertexBuffer.VBView();
    cmdList->IASetVertexBuffers(0, 1, &vbView);

    D3D12_INDEX_BUFFER_VIEW ibView = indexBuffer.IBView();
    cmdList->IASetIndexBuffer(&ibView);

    // Draw
    cmdList->DrawIndexedInstanced(NumIndices, 1, 0, 0, 0);
}

void Skybox::RenderEnvironmentMap(ID3D12GraphicsCommandList* cmdList,  const Float4x4& view,
                                  const Float4x4& projection, const Texture* environmentMap, Float3 scale)
{
    PIXMarker pixEvent(cmdList, "Skybox Render Environment Map");

    psConstants.CosSunAngularRadius = 0.0f;
    RenderCommon(cmdList, environmentMap, view, projection, scale);
}

#if EnableSkyModel_

void Skybox::RenderSky(ID3D12GraphicsCommandList* cmdList, const Float4x4& view,
                       const Float4x4& projection, const SkyCache& skyCache,
                       bool enableSun, const Float3& scale)
{
    PIXMarker pixEvent(cmdList, "Skybox Render Sky");

    Assert_(skyCache.Initialized());

    // Set the pixel shader constants
    psConstants.SunDirection = skyCache.SunDirection;
    psConstants.Scale = scale;
    if(enableSun)
    {
        psConstants.SunColor = skyCache.SunRenderColor;
        psConstants.CosSunAngularRadius = std::cos(DegToRad(skyCache.SunSize));
    }
    else
    {
        psConstants.SunColor = 0.0f;
        psConstants.CosSunAngularRadius = 0.0f;
    }

    RenderCommon(cmdList, &skyCache.CubeMap, view, projection, scale);
}

#endif // EnableSkyModel_

}
