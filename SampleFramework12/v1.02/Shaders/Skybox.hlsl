//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include <Constants.hlsl>
#include <DescriptorTables.hlsl>

//=================================================================================================
// Constant buffers
//=================================================================================================
struct VSConstants
{
    row_major float4x4 View;
    row_major float4x4 Projection;
    float3 Bias;
};

struct PSConstants
{
    float3 SunDirection;
    float CosSunAngularRadius;
    float3 SunColor;
    float3 Scale;
    uint EnvMapIdx;
};

ConstantBuffer<VSConstants> VSCBuffer : register(b0);
ConstantBuffer<PSConstants> PSCBuffer : register(b0);

//=================================================================================================
// Samplers
//=================================================================================================
SamplerState LinearSampler : register(s0);

//=================================================================================================
// Input/Output structs
//=================================================================================================

struct VSInput
{
    float3 PositionOS : POSITION;
};

struct VSOutput
{
    float4 PositionCS   : SV_Position;
    float3 TexCoord     : TEXCOORD;
};

//=================================================================================================
// Vertex Shader
//=================================================================================================
VSOutput SkyboxVS(in VSInput input)
{
    VSOutput output;

    // Rotate into view-space, centered on the camera
    float3 positionVS = mul(float4(input.PositionOS, 0.0f), VSCBuffer.View).xyz;

    // Transform to clip-space
    output.PositionCS = mul(float4(positionVS, 1.0f), VSCBuffer.Projection);
    output.PositionCS.z = output.PositionCS.w;

    // Make a texture coordinate
    output.TexCoord = input.PositionOS;

    return output;
}

//=================================================================================================
// Environment Map Pixel Shader
//=================================================================================================
float4 SkyboxPS(in VSOutput input) : SV_Target
{
    // Sample the environment map
    TextureCube envMap = TexCubeTable[PSCBuffer.EnvMapIdx];
    float3 color = envMap.Sample(LinearSampler, normalize(input.TexCoord)).xyz;

    // Draw a circle for the sun
    float3 dir = normalize(input.TexCoord);
    if(PSCBuffer.CosSunAngularRadius > 0.0f)
    {
        float cosSunAngle = dot(dir, PSCBuffer.SunDirection);
        if(cosSunAngle >= PSCBuffer.CosSunAngularRadius)
            color = PSCBuffer.SunColor;
    }

    color *= PSCBuffer.Scale;
    color = clamp(color, 0.0f, FP16Max);

    return float4(color, 1.0f);
}
