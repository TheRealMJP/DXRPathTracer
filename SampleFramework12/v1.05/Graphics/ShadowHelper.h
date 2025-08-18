//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include <PCH.h>
#include "..\\SF12_Math.h"

namespace SampleFramework12
{

class Camera;
class OrthographicCamera;
class PerspectiveCamera;
struct ModelSpotLight;
struct DepthBuffer;
struct RenderTexture;

const uint64_t NumCascades = 4;
const float MaxShadowFilterSize = 9.0f;

struct SunShadowConstantsBase
{
    Float4x4 ShadowMatrix;
    float CascadeSplits[NumCascades] = { };
    Float4 CascadeOffsets[NumCascades];
    Float4 CascadeScales[NumCascades];
    Float4 CascadeSizes[NumCascades];
};

struct EVSMConstants
{
    float PositiveExponent = 0.0f;
    float NegativeExponent = 0.0f;
    float LightBleedingReduction = 0.25f;
    uint32_t Padding = 0;
};

struct MSMConstants
{
    float DepthBias = 0.0f;
    float MomentBias = 0.0003f;
    float LightBleedingReduction = 0.25f;
    uint32_t Padding = 0;
};

struct SunShadowConstantsDepthMap
{
    SunShadowConstantsBase Base;
    uint32_t Dummy[4] = { };
};

struct SunShadowConstantsEVSM
{
    SunShadowConstantsBase Base;
    EVSMConstants EVSM;
};

struct SunShadowConstantsMSM
{
    SunShadowConstantsBase Base;
    MSMConstants MSM;
};

enum class ShadowMapMode : uint32_t
{
    DepthMap,
    EVSM,
    MSM,

    NumValues,
};

enum class ShadowMSAAMode : uint32_t
{
    MSAA1x,
    MSAA2x,
    MSAA4x,

    NumValues,
};

namespace ShadowHelper
{

extern Float4x4 ShadowScaleOffsetMatrix;

void Initialize(ShadowMapMode smMode, ShadowMSAAMode msaaMode);
void Shutdown();

void CreatePSOs();
void DestroyPSOs();

uint32_t NumMSAASamples();
DXGI_FORMAT SMFormat();

void ConvertShadowMap(ID3D12GraphicsCommandList7* cmdList, const DepthBuffer& depthMap, RenderTexture& smTarget,
                      uint32_t arraySlice, RenderTexture& tempTarget, float filterSizeU, float filterSizeV,
                      bool32 linearizeDepth, float nearClip, float farClip, const Float4x4& projection,
                      bool32 useCSConversion = false, bool32 use3x3Filter = true, float positiveExponent = 0.0f, float negativeExponent = 0.0f);

extern Float4x4 ScaleOffsetMatrix;
void PrepareCascades(const Float3& lightDir, uint64_t shadowMapSize, bool stabilize, const Camera& camera,
                     SunShadowConstantsBase& constants, OrthographicCamera* cascadeCameras);

};

}
