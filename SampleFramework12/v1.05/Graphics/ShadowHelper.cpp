//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "PCH.h"
#include "ShadowHelper.h"
#include "Camera.h"

#include <Utility.h>
#include <Graphics\\ShaderCompilation.h>
#include <Graphics\\GraphicsTypes.h>
#include <Graphics\\DX12_Helpers.h>

namespace SampleFramework12
{

namespace ShadowHelper
{

static const uint32_t MaxFilterRadius = 4;

// Transforms from [-1,1] post-projection space to [0,1] UV space
Float4x4 ShadowScaleOffsetMatrix = Float4x4(Float4(0.5f,  0.0f, 0.0f, 0.0f),
                                            Float4(0.0f, -0.5f, 0.0f, 0.0f),
                                            Float4(0.0f,  0.0f, 1.0f, 0.0f),
                                            Float4(0.5f,  0.5f, 0.0f, 1.0f));

static CompiledShaderPtr fullScreenTriVS;
static CompiledShaderPtr smConvertPS;
static CompiledShaderPtr filterSMHorizontalPS[MaxFilterRadius + 1];
static CompiledShaderPtr filterSMVerticalPS[MaxFilterRadius + 1];
static CompiledShaderPtr filter3x3PS;
static CompiledShaderPtr filter5x5PS;
static CompiledShaderPtr smConvertAndFilterCS[MaxFilterRadius + 1];

static ID3D12PipelineState* smConvertPSO = nullptr;
static ID3D12PipelineState* filterSMHorizontalPSO[MaxFilterRadius + 1] = { };
static ID3D12PipelineState* filterSMVerticalPSO[MaxFilterRadius + 1] = { };
static ID3D12PipelineState* filter3x3PSO = nullptr;
static ID3D12PipelineState* filter5x5PSO = nullptr;

static ID3D12PipelineState* smConvertAndFilterPSO[MaxFilterRadius + 1] = { };

static ShadowMapMode currSMMode = ShadowMapMode::NumValues;
static ShadowMSAAMode currMSAAMode = ShadowMSAAMode::NumValues;
static bool initialized = false;

struct ConvertConstants
{
    Float2 ShadowMapSize;
    float PositiveExponent = 0.0f;
    float NegativeExponent = 0.0f;
    float FilterSizeU = 0.0f;
    float FilterSizeV = 0.0f;
    bool32 LinearizeDepth = 0;
    float NearClip = 0.0f;
    float InvClipRange = 0.0f;
    float Proj33 = 0.0f;
    float Proj43 = 0.0f;
    uint32_t InputMapIdx = uint32_t(-1);
    uint32_t ArraySliceIdx = 0;
    uint32_t OutputTextureIdx = uint32_t(-1);
};

void Initialize(ShadowMapMode smMode, ShadowMSAAMode msaaMode)
{
    Assert_(initialized == false);
    currSMMode = smMode;
    currMSAAMode = msaaMode;

    if(smMode == ShadowMapMode::EVSM || smMode == ShadowMapMode::MSM)
    {
        std::string fullScreenTriPath = SampleFrameworkDir() + "Shaders\\FullScreenTriangle.hlsl";
        std::string smConvertPath = SampleFrameworkDir() + "Shaders\\SMConvert.hlsl";
        fullScreenTriVS = CompileFromFile(fullScreenTriPath.c_str(), "FullScreenTriangleVS", ShaderType::Vertex);
        for(uint32_t i = 0; i <= MaxFilterRadius; ++i)
        {
            CompileOptions opts;
            opts.Add("SampleRadius_", i);
            opts.Add("Vertical_", 0);
            filterSMHorizontalPS[i] = CompileFromFile(smConvertPath.c_str(), "FilterSM", ShaderType::Pixel, opts);

            opts.Reset();
            opts.Add("SampleRadius_", i);
            opts.Add("Vertical_", 1);
            filterSMVerticalPS[i] = CompileFromFile(smConvertPath.c_str(), "FilterSM", ShaderType::Pixel, opts);
        }

        filter3x3PS = CompileFromFile(smConvertPath.c_str(), "FilterSM3x3", ShaderType::Pixel);
        filter5x5PS = CompileFromFile(smConvertPath.c_str(), "FilterSM5x5", ShaderType::Pixel);

        {
            CompileOptions opts;
            opts.Add("EVSM_", smMode == ShadowMapMode::EVSM ? 1 : 0);
            opts.Add("MSM_", smMode == ShadowMapMode::MSM ? 1 : 0);
            opts.Add("MSAASamples_", NumMSAASamples());
            smConvertPS = CompileFromFile(smConvertPath.c_str(), "SMConvert", ShaderType::Pixel, opts);
        }

        for(uint32_t i = 0; i <= MaxFilterRadius; ++i)
        {
            CompileOptions opts;
            opts.Add("SampleRadius_", i);
            opts.Add("EVSM_", smMode == ShadowMapMode::EVSM ? 1 : 0);
            opts.Add("MSM_", smMode == ShadowMapMode::MSM ? 1 : 0);
            opts.Add("MSAASamples_", NumMSAASamples());
            opts.Add("CS_", 1);
            smConvertAndFilterCS[i] = CompileFromFile(smConvertPath.c_str(), "SMConvertAndFilter", ShaderType::Compute, opts);
        }
    }

    initialized = true;
}

void Shutdown()
{
    Assert_(initialized);

    currSMMode = ShadowMapMode::NumValues;
    currMSAAMode = ShadowMSAAMode::NumValues;
    initialized = false;
}

void CreatePSOs()
{
    if(initialized == false)
        return;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = { };
    psoDesc.pRootSignature = DX12::UniversalRootSignature;
    psoDesc.VS = fullScreenTriVS.ByteCode();
    psoDesc.PS = smConvertPS.ByteCode();
    psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::NoCull);
    psoDesc.BlendState = DX12::GetBlendState(BlendState::Disabled);
    psoDesc.DepthStencilState = DX12::GetDepthState(DepthState::Disabled);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = SMFormat();
    psoDesc.SampleDesc.Count = 1;
    DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&smConvertPSO)));

    for(uint32_t i = 0; i <= MaxFilterRadius; ++i)
    {
        psoDesc.PS = filterSMHorizontalPS[i].ByteCode();
        DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&filterSMHorizontalPSO[i])));

        psoDesc.PS = filterSMVerticalPS[i].ByteCode();
        DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&filterSMVerticalPSO[i])));
    }

    psoDesc.PS = filter3x3PS.ByteCode();
    DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&filter3x3PSO)));

    psoDesc.PS = filter5x5PS.ByteCode();
    DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&filter5x5PSO)));

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDescCS = { };
    psoDescCS.pRootSignature = DX12::UniversalRootSignature;

    for(uint32_t i = 0; i <= MaxFilterRadius; ++i)
    {
        psoDescCS.CS = smConvertAndFilterCS[i].ByteCode();
        DXCall(DX12::Device->CreateComputePipelineState(&psoDescCS, IID_PPV_ARGS(&smConvertAndFilterPSO[i])));
    }
}

void DestroyPSOs()
{
    if(initialized == false)
        return;

    DX12::DeferredRelease(smConvertPSO);
    DX12::DeferredRelease(filter3x3PSO);
    DX12::DeferredRelease(filter5x5PSO);
    for(uint32_t i = 0; i <= MaxFilterRadius; ++i)
    {
        DX12::DeferredRelease(filterSMHorizontalPSO[i]);
        DX12::DeferredRelease(filterSMVerticalPSO[i]);
        DX12::DeferredRelease(smConvertAndFilterPSO[i]);
    }
}

uint32_t NumMSAASamples()
{
    Assert_(currMSAAMode != ShadowMSAAMode::NumValues);

    static const uint32_t numMSAASamples[] = { 1, 2, 4 };
    StaticAssert_(ArraySize_(numMSAASamples) == uint32_t(ShadowMSAAMode::NumValues));
    return numMSAASamples[uint32_t(currMSAAMode)];
}

DXGI_FORMAT SMFormat()
{
    Assert_(currSMMode != ShadowMapMode::NumValues);

    if(currSMMode == ShadowMapMode::EVSM)
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    else if(currSMMode == ShadowMapMode::MSM)
        return DXGI_FORMAT_R16G16B16A16_UNORM;

    Assert_(false);
    return DXGI_FORMAT_UNKNOWN;
}

void ConvertShadowMap(ID3D12GraphicsCommandList7* cmdList, const DepthBuffer& depthMap, RenderTexture& smTarget,
                      uint32_t arraySlice, RenderTexture& tempTarget, float filterSizeU, float filterSizeV,
                      bool32 linearizeDepth, float nearClip, float farClip, const Float4x4& projection,
                      bool32 useCSConversion, bool32 use3x3Filter, float positiveExponent, float negativeExponent)
{
    Assert_(initialized);
    Assert_(currSMMode == ShadowMapMode::MSM || currSMMode == ShadowMapMode::EVSM);
    Assert_(NumMSAASamples() == depthMap.MSAASamples);
    Assert_(depthMap.Width() == smTarget.Width() && depthMap.Height() == smTarget.Height());

    PIXMarker event(cmdList, "Shadow Map Conversion");

    filterSizeU = Clamp(filterSizeU, 1.0f, MaxShadowFilterSize);
    filterSizeV = Clamp(filterSizeV, 1.0f, MaxShadowFilterSize);

    const uint32_t sampleRadiusU = uint32_t((filterSizeU / 2) + 0.499f);
    const uint32_t sampleRadiusV = uint32_t((filterSizeV / 2) + 0.499f);

    ConvertConstants constants;
    constants.ShadowMapSize.x = float(depthMap.Width());
    constants.ShadowMapSize.y = float(depthMap.Height());
    constants.PositiveExponent = positiveExponent;
    constants.NegativeExponent = negativeExponent;
    constants.FilterSizeU = filterSizeU;
    constants.FilterSizeV = filterSizeV;
    constants.LinearizeDepth = linearizeDepth ? 1 : 0;
    constants.NearClip = nearClip;
    constants.InvClipRange = 1.0f / (farClip - nearClip);
    constants.Proj33 = projection._33;
    constants.Proj43 = projection._43;
    constants.InputMapIdx = depthMap.SRV();
    constants.ArraySliceIdx = arraySlice;
    constants.OutputTextureIdx = smTarget.UAV;

    if(useCSConversion)
    {
        uint32_t sampleRadius = uint32_t((Max(filterSizeU, filterSizeV) / 2.0f) + 0.499f);

        cmdList->SetComputeRootSignature(DX12::UniversalRootSignature);
        cmdList->SetPipelineState(smConvertAndFilterPSO[sampleRadius]);

        DX12::BindTempConstantBuffer(cmdList, constants, URS_ConstantBuffers + 0, CmdListMode::Compute);

        DX12::Barrier(cmdList, smTarget.UAVWritableBarrier({ .Discard = true, .StartArraySlice = arraySlice, .NumArraySlices = 1 }));

        const uint32_t BaseThreadGroupWidth = 8;
        cmdList->Dispatch(DX12::DispatchSize(smTarget.Width(), BaseThreadGroupWidth), DX12::DispatchSize(smTarget.Height(), BaseThreadGroupWidth), 1);

        DX12::Barrier(cmdList, smTarget.UAVToShaderReadableBarrier({ .StartArraySlice = arraySlice, .NumArraySlices = 1 }));
    }
    else if(use3x3Filter && ((sampleRadiusU == 1 && sampleRadiusV == 1) || (sampleRadiusU == 2 && sampleRadiusV == 2)))
    {
        // Do a conversion pass, then do 3x3 filtering in single pass
        DX12::Barrier(cmdList, tempTarget.RTWritableBarrier({ .Discard = true }));

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[1] = { tempTarget.RTV };
        cmdList->OMSetRenderTargets(1, rtvHandles, false, nullptr);

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        DX12::SetViewport(cmdList, tempTarget.Width(), tempTarget.Height());

        cmdList->SetGraphicsRootSignature(DX12::UniversalRootSignature);
        cmdList->SetPipelineState(smConvertPSO);

        DX12::BindTempConstantBuffer(cmdList, constants, URS_ConstantBuffers + 0, CmdListMode::Graphics);

        cmdList->DrawInstanced(3, 1, 0, 0);

        {
            BarrierBatchBuilder barrierBuilder;
            barrierBuilder.Add(tempTarget.RTToShaderReadableBarrier());
            barrierBuilder.Add(smTarget.RTWritableBarrier({ .Discard = true, .StartArraySlice = arraySlice, .NumArraySlices = 1 }));
            DX12::Barrier(cmdList, barrierBuilder.Build());
        }

        rtvHandles[0] = arraySlice == 0 ? smTarget.RTV : smTarget.ArrayRTVs[arraySlice];
        cmdList->OMSetRenderTargets(1, rtvHandles, false, nullptr);

        constants.InputMapIdx = tempTarget.SRV();
        DX12::BindTempConstantBuffer(cmdList, constants, URS_ConstantBuffers + 0, CmdListMode::Graphics);

        cmdList->SetPipelineState(sampleRadiusU == 2 ? filter5x5PSO : filter3x3PSO);

        cmdList->DrawInstanced(3, 1, 0, 0);

        DX12::Barrier(cmdList, smTarget.RTToShaderReadableBarrier({ .StartArraySlice = arraySlice, .NumArraySlices = 1 }));
    }
    else
    {
        // Standard pixel shader conversion/filtering path
        D3D12_CPU_DESCRIPTOR_HANDLE smTargetRTV = arraySlice == 0 ? smTarget.RTV : smTarget.ArrayRTVs[arraySlice];
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[1] = { smTargetRTV };
        cmdList->OMSetRenderTargets(1, rtvHandles, false, nullptr);

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        DX12::SetViewport(cmdList, smTarget.Width(), smTarget.Height());

        cmdList->SetGraphicsRootSignature(DX12::UniversalRootSignature);
        cmdList->SetPipelineState(smConvertPSO);

        DX12::BindTempConstantBuffer(cmdList, constants, URS_ConstantBuffers + 0, CmdListMode::Graphics);

        DX12::Barrier(cmdList, smTarget.RTWritableBarrier({ .Discard = true, .StartArraySlice = arraySlice, .NumArraySlices = 1 }));

        cmdList->DrawInstanced(3, 1, 0, 0);

        if(filterSizeU > 1.0f || filterSizeV > 1.0f)
        {
            {
                BarrierBatchBuilder barrierBuilder;
                barrierBuilder.Add(smTarget.RTToShaderReadableBarrier({ .StartArraySlice = arraySlice, .NumArraySlices = 1 }));
                barrierBuilder.Add(tempTarget.RTWritableBarrier({ .Discard = true }));
                DX12::Barrier(cmdList, barrierBuilder.Build());
            }

            // Horizontal pass
            rtvHandles[0] = tempTarget.RTV;
            cmdList->OMSetRenderTargets(1, rtvHandles, false, nullptr);

            constants.InputMapIdx = smTarget.SRV();
            DX12::BindTempConstantBuffer(cmdList, constants, URS_ConstantBuffers + 0, CmdListMode::Graphics);

            cmdList->SetPipelineState(filterSMHorizontalPSO[sampleRadiusU]);

            cmdList->DrawInstanced(3, 1, 0, 0);

            {
                BarrierBatchBuilder barrierBuilder;
                barrierBuilder.Add(tempTarget.RTToShaderReadableBarrier());
                barrierBuilder.Add(smTarget.RTWritableBarrier({ .Discard = true, .StartArraySlice = arraySlice, .NumArraySlices = 1 }));
                DX12::Barrier(cmdList, barrierBuilder.Build());
            }

            // Vertical pass
            rtvHandles[0] = smTargetRTV;
            cmdList->OMSetRenderTargets(1, rtvHandles, false, nullptr);

            constants.InputMapIdx = tempTarget.SRV();
            DX12::BindTempConstantBuffer(cmdList, constants, URS_ConstantBuffers + 0, CmdListMode::Graphics);

            cmdList->SetPipelineState(filterSMVerticalPSO[sampleRadiusV]);

            cmdList->DrawInstanced(3, 1, 0, 0);

            {
                BarrierBatchBuilder barrierBuilder;
                barrierBuilder.Add(smTarget.RTToShaderReadableBarrier({ .StartArraySlice = arraySlice, .NumArraySlices = 1 }));
                DX12::Barrier(cmdList, barrierBuilder.Build());
            }
        }
    }
}

void PrepareCascades(const Float3& lightDir, uint64_t shadowMapSize, bool stabilize, const Camera& camera,
                     SunShadowConstantsBase& constants, OrthographicCamera* cascadeCameras)
{
    const float MinDistance = 0.0f;
    const float MaxDistance = 1.0f;

    // Compute the split distances based on the partitioning mode
    float cascadeSplits[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    if(camera.IsOrthographic())
    {
        for(uint32_t i = 0; i < NumCascades; ++i)
            cascadeSplits[i] = Lerp(MinDistance, MaxDistance, (i + 1.0f) / NumCascades);
    }
    else
    {
        float lambda = 0.5f;

        float nearClip = camera.NearClip();
        float farClip = camera.FarClip();
        float clipRange = farClip - nearClip;

        float minZ = nearClip + MinDistance * clipRange;
        float maxZ = nearClip + MaxDistance * clipRange;

        float range = maxZ - minZ;
        float ratio = maxZ / minZ;

        for(uint32_t i = 0; i < NumCascades; ++i)
        {
            float p = (i + 1) / static_cast<float>(NumCascades);
            float log = minZ * std::pow(ratio, p);
            float uniform = minZ + range * p;
            float d = lambda * (log - uniform) + uniform;
            cascadeSplits[i] = (d - nearClip) / clipRange;
        }
    }

    Float4x4 c0Matrix;

    // Prepare the projections ofr each cascade
    for(uint64_t cascadeIdx = 0; cascadeIdx < NumCascades; ++cascadeIdx)
    {
        // Get the 8 points of the view frustum in world space
        Float3 frustumCornersWS[8] =
        {
            Float3(-1.0f,  1.0f, 0.0f),
            Float3( 1.0f,  1.0f, 0.0f),
            Float3( 1.0f, -1.0f, 0.0f),
            Float3(-1.0f, -1.0f, 0.0f),
            Float3(-1.0f,  1.0f, 1.0f),
            Float3( 1.0f,  1.0f, 1.0f),
            Float3( 1.0f, -1.0f, 1.0f),
            Float3(-1.0f, -1.0f, 1.0f),
        };

        float prevSplitDist = cascadeIdx == 0 ? MinDistance : cascadeSplits[cascadeIdx - 1];
        float splitDist = cascadeSplits[cascadeIdx];

        Float4x4 invViewProj = Float4x4::Invert(camera.ViewProjectionMatrix());
        for(uint64_t i = 0; i < 8; ++i)
            frustumCornersWS[i] = Float3::Transform(frustumCornersWS[i], invViewProj);

        // Get the corners of the current cascade slice of the view frustum
        for(uint64_t i = 0; i < 4; ++i)
        {
            Float3 cornerRay = frustumCornersWS[i + 4] - frustumCornersWS[i];
            Float3 nearCornerRay = cornerRay * prevSplitDist;
            Float3 farCornerRay = cornerRay * splitDist;
            frustumCornersWS[i + 4] = frustumCornersWS[i] + farCornerRay;
            frustumCornersWS[i] = frustumCornersWS[i] + nearCornerRay;
        }

        // Calculate the centroid of the view frustum slice
        Float3 frustumCenter = Float3(0.0f);
        for(uint64_t i = 0; i < 8; ++i)
            frustumCenter += frustumCornersWS[i];
        frustumCenter *= (1.0f / 8.0f);

        // Pick the up vector to use for the light camera
        Float3 upDir = camera.Right();

        Float3 minExtents;
        Float3 maxExtents;

        if(stabilize)
        {
            // This needs to be constant for it to be stable
            upDir = Float3(0.0f, 1.0f, 0.0f);

            // Calculate the radius of a bounding sphere surrounding the frustum corners
            float sphereRadius = 0.0f;
            for(uint64_t i = 0; i < 8; ++i)
            {
                float dist = Float3::Length(Float3(frustumCornersWS[i]) - frustumCenter);
                sphereRadius = Max(sphereRadius, dist);
            }

            sphereRadius = std::ceil(sphereRadius * 16.0f) / 16.0f;

            maxExtents = Float3(sphereRadius, sphereRadius, sphereRadius);
            minExtents = -maxExtents;
        }
        else
        {
            // Create a temporary view matrix for the light
            Float3 lightCameraPos = frustumCenter;
            Float3 lookAt = frustumCenter - lightDir;
            DirectX::XMMATRIX lightView = DirectX::XMMatrixLookAtLH(lightCameraPos.ToSIMD(), lookAt.ToSIMD(), upDir.ToSIMD());

            // Calculate an AABB around the frustum corners
            DirectX::XMVECTOR mins = DirectX::XMVectorSet(FloatMax, FloatMax, FloatMax, FloatMax);
            DirectX::XMVECTOR maxes = DirectX::XMVectorSet(-FloatMax, -FloatMax, -FloatMax, -FloatMax);
            for(uint32_t i = 0; i < 8; ++i)
            {
                DirectX::XMVECTOR corner = DirectX::XMVector3TransformCoord(frustumCornersWS[i].ToSIMD(), lightView);
                mins = DirectX::XMVectorMin(mins, corner);
                maxes = DirectX::XMVectorMax(maxes, corner);
            }

            minExtents = Float3(mins);
            maxExtents = Float3(maxes);
        }

        // Adjust the min/max to accommodate the filtering size
        float scale = (shadowMapSize + 7.0f) / shadowMapSize;
        minExtents.x *= scale;
        minExtents.y *= scale;
        maxExtents.x *= scale;
        maxExtents.y *= scale;

        Float3 cascadeExtents = maxExtents - minExtents;

        // Get position of the shadow camera
        Float3 shadowCameraPos = frustumCenter + lightDir * -minExtents.z;

        // Come up with a new orthographic camera for the shadow caster
        OrthographicCamera& shadowCamera = cascadeCameras[cascadeIdx];
        shadowCamera.Initialize(minExtents.x, minExtents.y, maxExtents.x, maxExtents.y, 0.0f, cascadeExtents.z);
        shadowCamera.SetLookAt(shadowCameraPos, frustumCenter, upDir);

        if(stabilize)
        {
            // Create the rounding matrix, by projecting the world-space origin and determining
            // the fractional offset in texel space
            DirectX::XMMATRIX shadowMatrix = shadowCamera.ViewProjectionMatrix().ToSIMD();
            DirectX::XMVECTOR shadowOrigin = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
            shadowOrigin = DirectX::XMVector4Transform(shadowOrigin, shadowMatrix);
            shadowOrigin = DirectX::XMVectorScale(shadowOrigin, shadowMapSize / 2.0f);

            DirectX::XMVECTOR roundedOrigin = DirectX::XMVectorRound(shadowOrigin);
            DirectX::XMVECTOR roundOffset = DirectX::XMVectorSubtract(roundedOrigin, shadowOrigin);
            roundOffset = DirectX::XMVectorScale(roundOffset, 2.0f / shadowMapSize);
            roundOffset = DirectX::XMVectorSetZ(roundOffset, 0.0f);
            roundOffset = DirectX::XMVectorSetW(roundOffset, 0.0f);

            DirectX::XMMATRIX shadowProj = shadowCamera.ProjectionMatrix().ToSIMD();
            shadowProj.r[3] = DirectX::XMVectorAdd(shadowProj.r[3], roundOffset);
            shadowCamera.SetProjection(Float4x4(shadowProj));
        }

        Float4x4 shadowMatrix = shadowCamera.ViewProjectionMatrix();
        shadowMatrix = shadowMatrix * ShadowScaleOffsetMatrix;

        // Store the split distance in terms of view space depth
        const float clipDist = camera.FarClip() - camera.NearClip();
        constants.CascadeSplits[cascadeIdx] = camera.NearClip() + splitDist * clipDist;
        constants.CascadeSizes[cascadeIdx] = Float4(maxExtents.x - minExtents.x, maxExtents.y - minExtents.y, cascadeExtents.z, 0.0f);

        if(cascadeIdx == 0)
        {
            c0Matrix = shadowMatrix;
            constants.ShadowMatrix = shadowMatrix;
            constants.CascadeOffsets[0] = Float4(0.0f, 0.0f, 0.0f, 0.0f);
            constants.CascadeScales[0] = Float4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        else
        {
            // Calculate the position of the lower corner of the cascade partition, in the UV space
            // of the first cascade partition
            Float4x4 invCascadeMat = Float4x4::Invert(shadowMatrix);
            Float3 cascadeCorner = Float3::Transform(Float3(0.0f, 0.0f, 0.0f), invCascadeMat);
            cascadeCorner = Float3::Transform(cascadeCorner, c0Matrix);

            // Do the same for the upper corner
            Float3 otherCorner = Float3::Transform(Float3(1.0f, 1.0f, 1.0f), invCascadeMat);
            otherCorner = Float3::Transform(otherCorner, c0Matrix);

            // Calculate the scale and offset
            Float3 cascadeScale = Float3(1.0f, 1.0f, 1.f) / (otherCorner - cascadeCorner);
            constants.CascadeOffsets[cascadeIdx] = Float4(-cascadeCorner, 0.0f);
            constants.CascadeScales[cascadeIdx] = Float4(cascadeScale, 1.0f);
        }
    }
}

}

}
