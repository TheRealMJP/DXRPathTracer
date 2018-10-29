//=================================================================================================
//
//  DXR Path Tracer
//  by MJP
//  http://mynameismjp.wordpress.com/
//
//  All code and content licensed under the MIT license
//
//=================================================================================================
#include <Descriptortables.hlsl>
#include <PPCommon.hlsl>
#include <Constants.hlsl>
#include "AppSettings.hlsl"

//=================================================================================================
// Helper Functions
//=================================================================================================

// Calculates the gaussian blur weight for a given distance and sigmas
float CalcGaussianWeight(int sampleDist, float sigma)
{
    float g = 1.0f / sqrt(2.0f * 3.14159 * sigma * sigma);
    return (g * exp(-(sampleDist * sampleDist) / (2 * sigma * sigma)));
}

// Performs a gaussian blur in one direction
float4 Blur(in PSInput input, float2 texScale, float sigma, bool nrmlize)
{
    Texture2D inputTexture = Tex2DTable[SRVIndices.Idx0];

    float2 inputSize = 0.0f;
    inputTexture.GetDimensions(inputSize.x, inputSize.y);

    float4 color = 0;
    float weightSum = 0.0f;
    for(int i = -7; i < 7; i++)
    {
        float weight = CalcGaussianWeight(i, sigma);
        weightSum += weight;

        float2 texCoord = input.TexCoord;
        texCoord += (i / inputSize) * texScale;

        float4 texSample = inputTexture.Sample(PointSampler, texCoord);

        color += texSample * weight;
    }

    if(nrmlize)
        color /= weightSum;

    return color;
}

// Applies the approximated version of HP Duiker's film stock curve
float3 ToneMapFilmicALU(in float3 color)
{
    color = max(0, color - 0.004f);
    color = (color * (6.2f * color + 0.5f)) / (color * (6.2f * color + 1.7f)+ 0.06f);
    return color;
}

// ================================================================================================
// Shader Entry Points
// ================================================================================================

// Initial pass for bloom
float4 Bloom(in PSInput input) : SV_Target
{
    Texture2D inputTexture = Tex2DTable[SRVIndices.Idx0];

    float4 reds = inputTexture.GatherRed(LinearSampler, input.TexCoord);
    float4 greens = inputTexture.GatherGreen(LinearSampler, input.TexCoord);
    float4 blues = inputTexture.GatherBlue(LinearSampler, input.TexCoord);

    float3 result = 0.0f;

    [unroll]
    for(uint i = 0; i < 4; ++i)
    {
        float3 color = float3(reds[i], greens[i], blues[i]);

        result += color;
    }

    result /= 4.0f;

    return float4(result, 1.0f);
}

// Uses hw bilinear filtering for upscaling or downscaling
float4 Scale(in PSInput input) : SV_Target
{
    Texture2D inputTexture = Tex2DTable[SRVIndices.Idx0];
    return inputTexture.Sample(LinearSampler, input.TexCoord);
}

// Horizontal gaussian blur
float4 BlurH(in PSInput input) : SV_Target
{
    return Blur(input, float2(1, 0), AppSettings.BloomBlurSigma, false);
}

// Vertical gaussian blur
float4 BlurV(in PSInput input) : SV_Target
{
    return Blur(input, float2(0, 1), AppSettings.BloomBlurSigma, false);
}

// Applies exposure and tone mapping to the input
float4 ToneMap(in PSInput input) : SV_Target0
{
    Texture2D inputTexture0 = Tex2DTable[SRVIndices.Idx0];
    Texture2D inputTexture1 = Tex2DTable[SRVIndices.Idx1];

    float3 color = inputTexture0.Sample(PointSampler, input.TexCoord).xyz;

    // Add bloom
    color += inputTexture1.Sample(LinearSampler, input.TexCoord).xyz * AppSettings.BloomMagnitude * exp2(AppSettings.BloomExposure);

    // Apply exposure (accounting for the FP16 scale used for lighting and emissive sources)
    color *= exp2(AppSettings.Exposure) / FP16Scale;

    // Tone map to sRGB color space with the appropriate transfer function applied
    color = ToneMapFilmicALU(color);

    return float4(color, 1.0f);
}