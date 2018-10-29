//=================================================================================================
//
//	MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include <DescriptorTables.hlsl>

//=================================================================================================
// Resources
//=================================================================================================

// Inputs
struct DecodeCBufferLayout
{
    uint InputTextureIdx;
    uint Width;
    uint Height;
};

ConstantBuffer<DecodeCBufferLayout> CBuffer : register(b0);

SamplerState PointSampler : register(s0);

// Outputs
RWBuffer<float4> OutputBuffer : register(u0);

//=================================================================================================
// Entry points
//=================================================================================================
[numthreads(TGSize_, TGSize_, 1)]
void DecodeTextureCS(in uint3 GroupID : SV_GroupID, in uint3 GroupThreadID : SV_GroupThreadID)
{
    Texture2D inputTexture = Tex2DTable[CBuffer.InputTextureIdx];

	const uint2 texelIdx = GroupID.xy * uint2(TGSize_, TGSize_) + GroupThreadID.xy;
    const uint bufferIdx = texelIdx.y * CBuffer.Width + texelIdx.x;
	OutputBuffer[bufferIdx] = inputTexture[texelIdx];
}

[numthreads(TGSize_, TGSize_, 1)]
void DecodeTextureArrayCS(in uint3 GroupID : SV_GroupID, in uint3 GroupThreadID : SV_GroupThreadID)
{
    Texture2DArray inputTexture = Tex2DArrayTable[CBuffer.InputTextureIdx];

	const uint3 texelIdx = uint3(GroupID.xy * uint2(TGSize_, TGSize_) + GroupThreadID.xy, GroupID.z);
    const uint bufferIdx = texelIdx.z * (CBuffer.Width * CBuffer.Height) + (texelIdx.y * CBuffer.Width) + texelIdx.x;
	OutputBuffer[bufferIdx] = inputTexture[texelIdx];
}

[numthreads(TGSize_, TGSize_, 1)]
void DecodeTextureCubeCS(in uint3 GroupID : SV_GroupID, in uint3 GroupThreadID : SV_GroupThreadID)
{
    TextureCube inputTexture = TexCubeTable[CBuffer.InputTextureIdx];

    const uint3 texelIdx = uint3(GroupID.xy * uint2(TGSize_, TGSize_) + GroupThreadID.xy, GroupID.z);
    const uint bufferIdx = texelIdx.z * (CBuffer.Width * CBuffer.Height) + (texelIdx.y * CBuffer.Width) + texelIdx.x;

    float u = ((texelIdx.x + 0.5f) / float(CBuffer.Width)) * 2.0f - 1.0f;
    float v = ((texelIdx.y + 0.5f) / float(CBuffer.Height)) * 2.0f - 1.0f;
    v *= -1.0f;

    float3 dir = 0.0f;

    [flatten]
    switch(texelIdx.z) {
        case 0:
            dir = normalize(float3(1.0f, v, -u));
            break;
        case 1:
            dir = normalize(float3(-1.0f, v, u));
            break;
        case 2:
            dir = normalize(float3(u, 1.0f, -v));
            break;
        case 3:
            dir = normalize(float3(u, -1.0f, v));
            break;
        case 4:
            dir = normalize(float3(u, v, 1.0f));
            break;
        case 5:
            dir = normalize(float3(-u, v, -1.0f));
            break;
    }

    OutputBuffer[bufferIdx] = inputTexture.SampleLevel(PointSampler, dir, 0.0f);
}