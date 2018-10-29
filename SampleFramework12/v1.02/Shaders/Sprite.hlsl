//=================================================================================================
//
//	MJP's DX11 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include <DescriptorTables.hlsl>

//=================================================================================================
// Constant buffers
//=================================================================================================
cbuffer PerBatchCB : register(b0)
{
    float2 TextureSize;
    float2 ViewportSize;
    bool LinearSampling;
}

cbuffer SRVIndicesCB : register(b1)
{
    uint SpriteBufferIdx;
    uint SpriteTextureIdx;
}

struct SpriteDrawData
{
    float2 Position;
    float2 Scale;
    float2 SinCosRotation;
    float4 Color;
    float4 SourceRect;
};

//=================================================================================================
// Resources
//=================================================================================================
StructuredBuffer<SpriteDrawData> SpriteBuffers[] : register(t0, space100);
SamplerState PointSampler : register(s0);
SamplerState LinearSampler : register(s1);


//=================================================================================================
// Input/Output structs
//=================================================================================================
struct VSOutput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
    float4 Color : COLOR;
};

//=================================================================================================
// Vertex Shader, instanced
//=================================================================================================
VSOutput SpriteVS(in uint VertexIdx : SV_VertexID, in uint InstanceIdx : SV_InstanceID)
{
    float2 vtxPosition = 0.0f;
    if(VertexIdx == 1)
        vtxPosition = float2(1.0f, 0.0f);
    else if(VertexIdx == 2)
        vtxPosition = float2(1.0f, 1.0f);
    else if(VertexIdx == 3)
        vtxPosition = float2(0.0f, 1.0f);

    StructuredBuffer<SpriteDrawData> spriteBuffer = SpriteBuffers[SpriteBufferIdx];
    SpriteDrawData instanceData = spriteBuffer[InstanceIdx];

    // Scale the quad so that it's texture-sized
    float2 positionSS = vtxPosition * instanceData.SourceRect.zw;

    // Apply transforms in screen space
    float sinRotation = instanceData.SinCosRotation.x;
    float cosRotation = instanceData.SinCosRotation.y;
    positionSS *= instanceData.Scale;
    positionSS = mul(positionSS, float2x2(cosRotation, -sinRotation, sinRotation, cosRotation));
    positionSS += instanceData.Position;

    // Scale by the viewport size, flip Y, then rescale to device coordinates
    float2 positionDS = positionSS;
    positionDS /= ViewportSize;
    positionDS = positionDS * 2 - 1;
    positionDS.y *= -1;

    // Figure out the texture coordinates
    float2 outTexCoord = vtxPosition;
    outTexCoord.xy *= instanceData.SourceRect.zw / TextureSize;
    outTexCoord.xy += instanceData.SourceRect.xy / TextureSize;

    VSOutput output;
    output.Position = float4(positionDS, 1.0f, 1.0f);
    output.TexCoord = outTexCoord;
    output.Color = instanceData.Color;

    return output;
}

//=================================================================================================
// Pixel Shader
//=================================================================================================
float4 SpritePS(in VSOutput input) : SV_Target
{
    Texture2D spriteTexture = Tex2DTable[SpriteTextureIdx];

    float4 texColor = 0.0f;
    if(LinearSampling)
        texColor = spriteTexture.Sample(LinearSampler, input.TexCoord);
    else
        texColor = spriteTexture.Sample(PointSampler, input.TexCoord);
    texColor = texColor * input.Color;
    return texColor;
}
