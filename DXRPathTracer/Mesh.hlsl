//=================================================================================================
//
//  DXR Path Tracer
//  by MJP
//  http://mynameismjp.wordpress.com/
//
//  All code and content licensed under the MIT license
//
//=================================================================================================

#define UseImplicitShadowDerivatives_ 1

//=================================================================================================
// Includes
//=================================================================================================
#include "Shading.hlsl"

//=================================================================================================
// Constant buffers
//=================================================================================================
struct VSConstants
{
    row_major float4x4 World;
	row_major float4x4 View;
    row_major float4x4 WorldViewProjection;
    float NearClip;
    float FarClip;
};

struct MatIndexConstants
{
    uint MatIndex;
};

struct SRVIndexConstants
{
    uint SunShadowMapIdx;
    uint SpotLightShadowMapIdx;
    uint MaterialTextureIndicesIdx;
    uint SpotLightClusterBufferIdx;
};

ConstantBuffer<VSConstants> VSCBuffer : register(b0);
ConstantBuffer<ShadingConstants> PSCBuffer : register(b0);
ConstantBuffer<SunShadowConstants> ShadowCBuffer : register(b1);
ConstantBuffer<MatIndexConstants> MatIndexCBuffer : register(b2);
ConstantBuffer<LightConstants> LightCBuffer : register(b3);
ConstantBuffer<SRVIndexConstants> SRVIndices : register(b4);

//=================================================================================================
// Resources
//=================================================================================================
StructuredBuffer<Material> MaterialBuffers[] : register(t0, space100);

SamplerState AnisoSampler : register(s0);
SamplerComparisonState PCFSampler : register(s1);

//=================================================================================================
// Input/Output structs
//=================================================================================================
struct VSInput
{
    float3 PositionOS 		    : POSITION;
    float3 NormalOS 		    : NORMAL;
    float2 UV 		            : UV;
	float3 TangentOS 		    : TANGENT;
	float3 BitangentOS		    : BITANGENT;
};

struct VSOutput
{
    float4 PositionCS 		    : SV_Position;

    float3 NormalWS 		    : NORMALWS;
    float3 PositionWS           : POSITIONWS;
    float DepthVS               : DEPTHVS;
	float3 TangentWS 		    : TANGENTWS;
	float3 BitangentWS 		    : BITANGENTWS;
	float2 UV 		            : UV;
};

struct PSInput
{
    float4 PositionSS 		    : SV_Position;

    float3 NormalWS 		    : NORMALWS;
    float3 PositionWS           : POSITIONWS;
    float DepthVS               : DEPTHVS;
    float3 TangentWS 		    : TANGENTWS;
	float3 BitangentWS 		    : BITANGENTWS;
    float2 UV 		            : UV;
};

struct PSOutputForward
{
    float4 Color : SV_Target0;
    float4 TangentFrame : SV_Target1;
};

//=================================================================================================
// Vertex Shader
//=================================================================================================
VSOutput VS(in VSInput input, in uint VertexID : SV_VertexID)
{
    VSOutput output;

    float3 positionOS = input.PositionOS;

    // Calc the world-space position
    output.PositionWS = mul(float4(positionOS, 1.0f), VSCBuffer.World).xyz;

    // Calc the clip-space position
    output.PositionCS = mul(float4(positionOS, 1.0f), VSCBuffer.WorldViewProjection);
    output.DepthVS = output.PositionCS.w;

	// Rotate the normal into world space
    output.NormalWS = normalize(mul(float4(input.NormalOS, 0.0f), VSCBuffer.World)).xyz;

	// Rotate the rest of the tangent frame into world space
	output.TangentWS = normalize(mul(float4(input.TangentOS, 0.0f), VSCBuffer.World)).xyz;
	output.BitangentWS = normalize(mul(float4(input.BitangentOS, 0.0f), VSCBuffer.World)).xyz;

    // Pass along the texture coordinates
    output.UV = input.UV;

    return output;
}

//=================================================================================================
// Pixel Shader for clustered forward rendering
//=================================================================================================
float4 PSForward(in PSInput input) : SV_Target0
{
	float3 vtxNormalWS = normalize(input.NormalWS);
    float3 positionWS = input.PositionWS;

	float3 tangentWS = normalize(input.TangentWS);
	float3 bitangentWS = normalize(input.BitangentWS);
	float3x3 tangentFrame = float3x3(tangentWS, bitangentWS, vtxNormalWS);

    StructuredBuffer<Material> materialBuffer = MaterialBuffers[SRVIndices.MaterialTextureIndicesIdx];
    Material material = materialBuffer[MatIndexCBuffer.MatIndex];
    Texture2D AlbedoMap = Tex2DTable[material.Albedo];
    Texture2D NormalMap = Tex2DTable[material.Normal];
    Texture2D RoughnessMap = Tex2DTable[material.Roughness];
    Texture2D MetallicMap = Tex2DTable[material.Metallic];
    Texture2D OpacityMap = Tex2DTable[material.Opacity];
    Texture2D EmissiveMap = Tex2DTable[material.Emissive];

    ShadingInput shadingInput;
    shadingInput.PositionSS = uint2(input.PositionSS.xy);
    shadingInput.PositionWS = input.PositionWS;
    shadingInput.PositionWS_DX = ddx_fine(input.PositionWS);
    shadingInput.PositionWS_DY = ddy_fine(input.PositionWS);
    shadingInput.DepthVS = input.DepthVS;
    shadingInput.TangentFrame = tangentFrame;

    shadingInput.AlbedoMap = AlbedoMap.Sample(AnisoSampler, input.UV);
    shadingInput.NormalMap = NormalMap.Sample(AnisoSampler, input.UV).xy;
    shadingInput.RoughnessMap = RoughnessMap.Sample(AnisoSampler, input.UV).x;
    shadingInput.MetallicMap = MetallicMap.Sample(AnisoSampler, input.UV).x;
    shadingInput.EmissiveMap = EmissiveMap.Sample(AnisoSampler, input.UV).xyz;

    shadingInput.SpotLightClusterBuffer = RawBufferTable[SRVIndices.SpotLightClusterBufferIdx];

    shadingInput.AnisoSampler = AnisoSampler;

    shadingInput.ShadingCBuffer = PSCBuffer;
    shadingInput.ShadowCBuffer = ShadowCBuffer;
    shadingInput.LightCBuffer = LightCBuffer;

    Texture2DArray sunShadowMap = Tex2DArrayTable[SRVIndices.SunShadowMapIdx];
    Texture2DArray spotLightShadowMap = Tex2DArrayTable[SRVIndices.SpotLightShadowMapIdx];

    #if AlphaTest_
        if(OpacityMap.Sample(AnisoSampler, input.UV).x < 0.35f)
            discard;
    #endif

    float3 shadingResult = ShadePixel(shadingInput, sunShadowMap, spotLightShadowMap, PCFSampler);

    return float4(shadingResult, 1.0f);
}