//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include <StaticSamplers.hlsli>

struct VSInput
{
    float2 Position : POSITION;
    float4 Color : COLOR;
    float2 UV : UV;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
    float2 UV : UV;
};

struct VSConstants
{
    row_major float4x4 ProjectionMatrix;
};

struct PSConstants
{
    uint SRVIndex;
    uint SamplerMode;
};

ConstantBuffer<VSConstants> VSCBuffer : register(b0);
ConstantBuffer<PSConstants> PSCBuffer : register(b1);

VSOutput ImGuiVS(in VSInput input)
{
    VSOutput output;
    output.Position = mul(float4(input.Position, 0.0f, 1.0f), VSCBuffer.ProjectionMatrix);
    output.Color = input.Color;
    output.UV = input.UV;

    return output;
}

float4 ImGuiPS(in VSOutput input) : SV_Target0
{
    Texture2D imGuiTexture = ResourceDescriptorHeap[PSCBuffer.SRVIndex];
    if(PSCBuffer.SamplerMode == 0)
        return imGuiTexture.Sample(LinearClampSampler, input.UV) * input.Color;
    else
        return imGuiTexture.Sample(PointSampler, input.UV) * input.Color;
}