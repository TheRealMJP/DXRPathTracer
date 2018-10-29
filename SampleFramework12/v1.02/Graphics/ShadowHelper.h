//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
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

const uint64 NumCascades = 4;
const float MaxShadowFilterSize = 9.0f;

struct SunShadowConstantsBase
{
    Float4x4 ShadowMatrix;
    float CascadeSplits[NumCascades] = { };
    Float4 CascadeOffsets[NumCascades];
    Float4 CascadeScales[NumCascades];
};

struct EVSMConstants
{
    float PositiveExponent = 0.0f;
    float NegativeExponent = 0.0f;
    float LightBleedingReduction = 0.25f;
    uint32 Padding = 0;
};

struct MSMConstants
{
    float DepthBias = 0.0f;
    float MomentBias = 0.0003f;
    float LightBleedingReduction = 0.25f;
    uint32 Padding = 0;
};

struct SunShadowConstantsDepthMap
{
    SunShadowConstantsBase Base;
    uint32 Dummy[4] = { };
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

enum class ShadowMapMode : uint32
{
    DepthMap,
    EVSM,
    MSM,

    NumValues,
};

enum class ShadowMSAAMode : uint32
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

uint32 NumMSAASamples();
DXGI_FORMAT SMFormat();

void ConvertShadowMap(ID3D12GraphicsCommandList* cmdList, const DepthBuffer& depthMap, RenderTexture& smTarget,
                      uint32 arraySlice, RenderTexture& tempTarget, float filterSizeU, float filterSizeV,
                      bool32 linearizeDepth, float nearClip, float farClip, const Float4x4& projection,
                      bool32 useCSConversion = false, bool32 use3x3Filter = true, float positiveExponent = 0.0f, float negativeExponent = 0.0f);

extern Float4x4 ScaleOffsetMatrix;
void PrepareCascades(const Float3& lightDir, uint64 shadowMapSize, bool stabilize, const Camera& camera,
                     SunShadowConstantsBase& constants, OrthographicCamera* cascadeCameras);

};

}
