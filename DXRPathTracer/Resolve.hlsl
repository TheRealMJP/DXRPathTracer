//=================================================================================================
//
//  DXR Path Tracer
//  by MJP
//  http://mynameismjp.wordpress.com/
//
//  All code and content licensed under the MIT license
//
//=================================================================================================

//=================================================================================================
// Includes
//=================================================================================================
#include <DescriptorTables.hlsl>
#include <Constants.hlsl>
#include "AppSettings.hlsl"

//=================================================================================================
// Resources
//=================================================================================================
struct ResolveConstants
{
    uint2 OutputSize;
    uint InputTextureIdx;
};

ConstantBuffer<ResolveConstants> CBuffer : register(b0);

float Luminance(in float3 clr)
{
    return dot(clr, float3(0.299f, 0.587f, 0.114f));
}

float4 ResolvePS(in float4 Position : SV_Position) : SV_Target0
{
    Texture2DMS<float4> inputTexture = Tex2DMSTable[CBuffer.InputTextureIdx];

    uint2 pixelPos = uint2(Position.xy);

    const float ExposureFilterOffset = 2.0f;
    const float exposure = exp2(AppSettings.Exposure + ExposureFilterOffset) / FP16Scale;

    float3 sum = 0.0f;
    float totalWeight = 0.0f;

    [unroll]
    for(uint subSampleIdx = 0; subSampleIdx < MSAASamples_; ++subSampleIdx)
    {

        float3 texSample = inputTexture.Load(pixelPos, subSampleIdx).xyz;

        texSample = max(texSample, 0.0f);

        float sampleLum = Luminance(texSample);
        sampleLum *= exposure;
        float weight = 1.0f / (1.0f + sampleLum);

        sum += texSample * weight;
        totalWeight += weight;
    }

    float3 output = sum / max(totalWeight, 0.00001f);
    output = max(output, 0.0f);

    return float4(output, 1.0f);
}