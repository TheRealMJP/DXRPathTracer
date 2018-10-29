//=================================================================================================
//
//	MJP's DX11 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

SamplerState PointSampler : register(s0);
SamplerState LinearSampler : register(s1);
SamplerState LinearWrapSampler : register(s2);
SamplerState LinearBorderSampler : register(s3);

struct SRVIndicesLayout
{
    uint Idx0;
    uint Idx1;
    uint Idx2;
    uint Idx3;
    uint Idx4;
    uint Idx5;
    uint Idx6;
    uint Idx7;
};

ConstantBuffer<SRVIndicesLayout> SRVIndices : register(b0);

struct PSInput
{
    float4 PositionSS : SV_Position;
    float2 TexCoord : TEXCOORD;
};