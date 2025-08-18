//=================================================================================================
//
//	MJP's DX11 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include <StaticSamplers.hlsli>

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