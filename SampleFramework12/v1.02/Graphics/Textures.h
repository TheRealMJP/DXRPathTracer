//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "..\\PCH.h"

#include "..\\InterfacePointers.h"
#include "..\\Serialization.h"
#include "GraphicsTypes.h"

namespace SampleFramework12
{

struct Float4;
struct Half4;
struct UByte4N;
struct UShort4N;
class File;

// Texture loading and creation
void LoadTexture(Texture& texture, const wchar* filePath, bool forceSRGB = false);
void Create2DTexture(Texture& texture, uint64 width, uint64 height, uint64 numMips,
                     uint64 arraySize, DXGI_FORMAT format, bool cubeMap, const void* initData);
void Create3DTexture(Texture& texture, uint64 width, uint64 height, uint64 depth, uint64 numMips,
                     DXGI_FORMAT format, const void* initData);

void UploadTextureData(const Texture& texture, const void* initData);
void UploadTextureData(const Texture& texture, const void* initData, ID3D12GraphicsCommandList* cmdList,
                       ID3D12Resource* uploadResource, void* uploadCPUMem, uint64 resourceOffset);

template<typename T> struct TextureData
{
    Array<T> Texels;
    uint32 Width = 0;
    uint32 Height = 0;
    uint32 NumSlices = 0;

    void Init(uint32 width, uint32 height, uint32 numSlices)
    {
        Width = width;
        Height = height;
        NumSlices = numSlices;
        Texels.Init(width * height * numSlices);
    }

    template<typename TSerializer> void Serialize(TSerializer& serializer)
    {
        BulkSerializeItem(serializer, Texels);
        SerializeItem(serializer, Width);
        SerializeItem(serializer, Height);
        SerializeItem(serializer, NumSlices);
    }
};

void Create2DTexture(Texture& texture, const TextureData<UByte4N>& textureData, bool srgb = false);
void Create2DTexture(Texture& texture, const TextureData<UShort4N>& textureData);
void Create2DTexture(Texture& texture, const TextureData<Half4>& textureData);
void Create2DTexture(Texture& texture, const TextureData<Float4>& textureData);

// Decode a texture and copies it to the CPU
void GetTextureData(const Texture& texture, TextureData<UByte4N>& textureData, bool srgb = false);
void GetTextureData(const Texture& texture, TextureData<UShort4N>& textureData);
void GetTextureData(const Texture& texture, TextureData<Half4>& textureData);
void GetTextureData(const Texture& texture, TextureData<Float4>& textureData);

void SaveTextureAsDDS(const Texture& texture, const wchar* filePath);
void SaveTextureAsEXR(const Texture& texture, const wchar* filePath);
void SaveTextureAsEXR(const TextureData<Float4>& texture, const wchar* filePath);
void SaveTextureAsPNG(const Texture& texture, const wchar* filePath);
void SaveTextureAsPNG(const TextureData<UByte4N>& texture, const wchar* filePath);
void SaveTextureAsTIFF(const Texture& texture, const wchar* filePath);
void SaveTextureAsTIFF(const TextureData<UShort4N>& texture, const wchar* filePath);

Float3 MapXYSToDirection(uint64 x, uint64 y, uint64 s, uint64 width, uint64 height);

// == Texture Sampling Functions ==================================================================

template<typename T> static DirectX::XMVECTOR SampleTexture2D(Float2 uv, uint32 arraySlice, const Array<T>& texels,
                                                              uint32 texWidth, uint32 texHeight, uint32 numSlices)
{
    Float2 texSize = Float2(float(texWidth), float(texHeight));
    Float2 halfTexelSize(0.5f / texSize.x, 0.5f / texSize.y);
    Float2 samplePos = Frac(uv - halfTexelSize);
    if(samplePos.x < 0.0f)
        samplePos.x = 1.0f + samplePos.x;
    if(samplePos.y < 0.0f)
        samplePos.y = 1.0f + samplePos.y;
    samplePos *= texSize;
    uint32 samplePosX = std::min(uint32(samplePos.x), texWidth - 1);
    uint32 samplePosY = std::min(uint32(samplePos.y), texHeight - 1);
    uint32 samplePosXNext = std::min(samplePosX + 1, texWidth - 1);
    uint32 samplePosYNext = std::min(samplePosY + 1, texHeight - 1);

    Float2 lerpAmts = Float2(Frac(samplePos.x), Frac(samplePos.y));

    numSlices = std::max<uint32>(numSlices, 1);
    const uint32 sliceOffset = std::min(arraySlice, numSlices) * texWidth * texHeight;

    DirectX::XMVECTOR samples[4];
    samples[0] = texels[sliceOffset + samplePosY * texWidth + samplePosX].ToSIMD();
    samples[1] = texels[sliceOffset + samplePosY * texWidth + samplePosXNext].ToSIMD();
    samples[2] = texels[sliceOffset + samplePosYNext * texWidth + samplePosX].ToSIMD();
    samples[3] = texels[sliceOffset + samplePosYNext * texWidth + samplePosXNext].ToSIMD();

    // lerp between the shadow values to calculate our light amount
    return DirectX::XMVectorLerp(DirectX::XMVectorLerp(samples[0], samples[1], lerpAmts.x),
                        DirectX::XMVectorLerp(samples[2], samples[3], lerpAmts.x), lerpAmts.y);
}

template<typename T> static DirectX::XMVECTOR SampleTexture2D(Float2 uv, uint32 arraySlice, const TextureData<T>& texData)
{
    return SampleTexture2D(uv, arraySlice, texData.Texels, texData.Width, texData.Height, texData.NumSlices);
}

template<typename T> static DirectX::XMVECTOR SampleTexture2D(Float2 uv, const TextureData<T>& texData)
{
    return SampleTexture2D(uv, 0, texData.Texels, texData.Width, texData.Height, texData.NumSlices);
}

template<typename T> static DirectX::XMVECTOR SampleCubemap(Float3 direction, const TextureData<T>& texData)
{
    Assert_(texData.NumSlices == 6);

    float maxComponent = std::max(std::max(std::abs(direction.x), std::abs(direction.y)), std::abs(direction.z));
    uint32 faceIdx = 0;
    Float2 uv = Float2(direction.y, direction.z);
    if(direction.x == maxComponent)
    {
        faceIdx = 0;
        uv = Float2(-direction.z, -direction.y) / direction.x;
    }
    else if(-direction.x == maxComponent)
    {
        faceIdx = 1;
        uv = Float2(direction.z, -direction.y) / -direction.x;
    }
    else if(direction.y == maxComponent)
    {
        faceIdx = 2;
        uv = Float2(direction.x, direction.z) / direction.y;
    }
    else if(-direction.y == maxComponent)
    {
        faceIdx = 3;
        uv = Float2(direction.x, -direction.z) / -direction.y;
    }
    else if(direction.z == maxComponent)
    {
        faceIdx = 4;
        uv = Float2(direction.x, -direction.y) / direction.z;
    }
    else if(-direction.z == maxComponent)
    {
        faceIdx = 5;
        uv = Float2(-direction.x, -direction.y) / -direction.z;
    }

    uv = uv * Float2(0.5f, 0.5f) + Float2(0.5f, 0.5f);
    return SampleTexture2D(uv, faceIdx, texData);
}

}