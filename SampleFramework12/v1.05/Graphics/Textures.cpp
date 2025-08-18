//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "..\\Utility.h"
#include "..\\Exceptions.h"
#include "Textures.h"
#include "..\\FileIO.h"
#include "ShaderCompilation.h"
#include "GraphicsTypes.h"
#include "TinyEXR.h"
#include "DX12.h"

namespace SampleFramework12
{

// Returns the number of mip levels given a texture size
static uint64_t NumMipLevels(uint64_t width, uint64_t height, uint64_t depth = 1)
{
    uint64_t numMips = 0;
    uint64_t size = std::max(std::max(width, height), depth);
    while(1ull << numMips <= size)
        ++numMips;

    if(1ull << numMips < size)
        ++numMips;

    return numMips;
}

void LoadTexture(Texture& texture, const char* filePath, bool forceSRGB)
{
    texture.Shutdown();
    if(FileExists(filePath) == false)
        throw Exception(MakeString("Texture file with path '%s' does not exist", filePath));

    std::wstring filePathW = ToWString(filePath);

    DirectX::ScratchImage image;

    const std::string extension = GetFileExtension(filePath);
    if(extension == "DDS" || extension == "dds")
    {
        DXCall(DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image));
    }
    else if(extension == "TGA" || extension == "tga")
    {
        DirectX::ScratchImage tempImage;
        DXCall(DirectX::LoadFromTGAFile(filePathW.c_str(), nullptr, tempImage));
        DXCall(DirectX::GenerateMipMaps(*tempImage.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, image, false));
    }
    else
    {
        DirectX::ScratchImage tempImage;
        DXCall(DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, tempImage));
        DXCall(DirectX::GenerateMipMaps(*tempImage.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, image, false));
    }

    const DirectX::TexMetadata& metaData = image.GetMetadata();
    DXGI_FORMAT format = metaData.format;
    if(forceSRGB)
        format = DirectX::MakeSRGB(format);

    const bool is3D = metaData.dimension == DirectX::TEX_DIMENSION_TEXTURE3D;

    D3D12_RESOURCE_DESC1 textureDesc = { };
    textureDesc.MipLevels = uint16_t(metaData.mipLevels);
    textureDesc.Format = format;
    textureDesc.Width = uint32_t(metaData.width);
    textureDesc.Height = uint32_t(metaData.height);
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = is3D ? uint16_t(metaData.depth) : uint16_t(metaData.arraySize);
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = is3D ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Alignment = 0;

    ID3D12Device10* device = DX12::Device;
    DXCall(device->CreateCommittedResource3(DX12::GetDefaultHeapProps(), D3D12_HEAP_FLAG_NONE, &textureDesc,
                                           D3D12_BARRIER_LAYOUT_COMMON, nullptr, nullptr, 0, nullptr, IID_PPV_ARGS(&texture.Resource)));
    texture.Resource->SetName(filePathW.c_str());

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    texture.SRV = srvAlloc.Index;

    const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDescPtr = nullptr;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
    if(metaData.IsCubemap())
    {
        Assert_(metaData.arraySize == 6);
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = uint32_t(metaData.mipLevels);
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        srvDescPtr = &srvDesc;
    }

    for(uint32_t i = 0; i < DX12::SRVDescriptorHeap.NumHeaps; ++i)
        device->CreateShaderResourceView(texture.Resource, srvDescPtr, srvAlloc.Handles[i]);

    const uint64_t numSubResources = metaData.mipLevels * metaData.arraySize;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layouts = (D3D12_PLACED_SUBRESOURCE_FOOTPRINT*)_alloca(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) * numSubResources);
    uint32_t* numRows = (uint32_t*)_alloca(sizeof(uint32_t) * numSubResources);
    uint64_t* rowSizes = (uint64_t*)_alloca(sizeof(uint64_t) * numSubResources);

    uint64_t textureMemSize = 0;
    device->GetCopyableFootprints1(&textureDesc, 0, uint32_t(numSubResources), 0, layouts, numRows, rowSizes, &textureMemSize);

    // Get a GPU upload buffer
    UploadContext uploadContext = DX12::ResourceUploadBegin(textureMemSize);
    uint8_t* uploadMem = reinterpret_cast<uint8_t*>(uploadContext.CPUAddress);

    for(uint64_t arrayIdx = 0; arrayIdx < metaData.arraySize; ++arrayIdx)
    {

        for(uint64_t mipIdx = 0; mipIdx < metaData.mipLevels; ++mipIdx)
        {
            const uint64_t subResourceIdx = mipIdx + (arrayIdx * metaData.mipLevels);

            const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& subResourceLayout = layouts[subResourceIdx];
            const uint64_t subResourceHeight = numRows[subResourceIdx];
            const uint64_t subResourcePitch = subResourceLayout.Footprint.RowPitch;
            const uint64_t subResourceDepth = subResourceLayout.Footprint.Depth;
            uint8_t* dstSubResourceMem = reinterpret_cast<uint8_t*>(uploadMem) + subResourceLayout.Offset;

            for(uint64_t z = 0; z < subResourceDepth; ++z)
            {
                const DirectX::Image* subImage = image.GetImage(mipIdx, arrayIdx, z);
                Assert_(subImage != nullptr);
                const uint8_t* srcSubResourceMem = subImage->pixels;

                for(uint64_t y = 0; y < subResourceHeight; ++y)
                {
                    memcpy(dstSubResourceMem, srcSubResourceMem, Min(subResourcePitch, subImage->rowPitch));
                    dstSubResourceMem += subResourcePitch;
                    srcSubResourceMem += subImage->rowPitch;
                }
            }
        }
    }

    for(uint64_t subResourceIdx = 0; subResourceIdx < numSubResources; ++subResourceIdx)
    {
        D3D12_TEXTURE_COPY_LOCATION dst = { };
        dst.pResource = texture.Resource;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = uint32_t(subResourceIdx);
        D3D12_TEXTURE_COPY_LOCATION src = { };
        src.pResource = uploadContext.Resource;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = layouts[subResourceIdx];
        src.PlacedFootprint.Offset += uploadContext.ResourceOffset;
        uploadContext.CmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }

    DX12::ResourceUploadEnd(uploadContext);

    texture.Width = uint32_t(metaData.width);
    texture.Height = uint32_t(metaData.height);
    texture.Depth = uint32_t(metaData.depth);
    texture.NumMips = uint32_t(metaData.mipLevels);
    texture.ArraySize = uint32_t(metaData.arraySize);
    texture.Format = metaData.format;
    texture.Cubemap = metaData.IsCubemap() ? 1 : 0;
}

void Create2DTexture(Texture& texture, uint64_t width, uint64_t height, uint64_t numMips,
                     uint64_t arraySize, DXGI_FORMAT format, bool cubeMap, const void* initData)
{
    texture.Shutdown();

    Assert_(width > 0);
    Assert_(height > 0);
    Assert_(arraySize > 0);
    const uint64_t maxMipLevels = NumMipLevels(width, height);
    if(numMips == 0)
        numMips = maxMipLevels;
    Assert_(numMips <= maxMipLevels);

    const uint64_t srvArraySize = arraySize;
    if(cubeMap)
        arraySize *= 6;

    D3D12_RESOURCE_DESC1 textureDesc = { };
    textureDesc.MipLevels = uint16_t(numMips);
    textureDesc.Format = format;
    textureDesc.Width = uint32_t(width);
    textureDesc.Height = uint32_t(height);
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = uint16_t(arraySize);
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Alignment = 0;

    ID3D12Device10* device = DX12::Device;
    DXCall(device->CreateCommittedResource3(DX12::GetDefaultHeapProps(), D3D12_HEAP_FLAG_NONE, &textureDesc,
                                           D3D12_BARRIER_LAYOUT_COMMON, nullptr, nullptr, 0, nullptr, IID_PPV_ARGS(&texture.Resource)));

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    texture.SRV = srvAlloc.Index;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    if(srvArraySize == 1 && cubeMap == false)
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = uint32_t(numMips);
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    }
    else if(srvArraySize > 1 && cubeMap == false)
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MipLevels = uint32_t(numMips);
        srvDesc.Texture2DArray.MostDetailedMip = 0;
        srvDesc.Texture2DArray.PlaneSlice = 0;
        srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
        srvDesc.Texture2DArray.ArraySize = uint32_t(srvArraySize);
        srvDesc.Texture2DArray.FirstArraySlice = 0;
    }
    else if(srvArraySize == 1 && cubeMap)
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = uint32_t(numMips);
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    }
    else if(srvArraySize > 1 && cubeMap)
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCubeArray.MipLevels = uint32_t(numMips);
        srvDesc.TextureCubeArray.MostDetailedMip = 0;
        srvDesc.TextureCubeArray.ResourceMinLODClamp = 0.0f;
        srvDesc.TextureCubeArray.First2DArrayFace = 0;
        srvDesc.TextureCubeArray.NumCubes = uint32_t(srvArraySize);
    }

    for(uint32_t i = 0; i < DX12::SRVDescriptorHeap.NumHeaps; ++i)
        device->CreateShaderResourceView(texture.Resource, &srvDesc, srvAlloc.Handles[i]);

    texture.Width = uint32_t(width);
    texture.Height = uint32_t(height);
    texture.Depth = 1;
    texture.NumMips = uint32_t(numMips);
    texture.ArraySize = uint32_t(srvArraySize);
    texture.Format = format;
    texture.Cubemap = cubeMap;

    if(initData != nullptr)
        UploadTextureData(texture, initData);
}

void Create3DTexture(Texture& texture, uint64_t width, uint64_t height, uint64_t depth, uint64_t numMips, DXGI_FORMAT format, const void* initData)
{
        texture.Shutdown();

    Assert_(width > 0);
    Assert_(height > 0);
    Assert_(depth > 0);
    const uint64_t maxMipLevels = NumMipLevels(width, height, depth);
    if(numMips == 0)
        numMips = maxMipLevels;
    Assert_(numMips <= maxMipLevels);

    D3D12_RESOURCE_DESC1 textureDesc = { };
    textureDesc.MipLevels = uint16_t(numMips);
    textureDesc.Format = format;
    textureDesc.Width = uint32_t(width);
    textureDesc.Height = uint32_t(height);
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = uint16_t(depth);
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Alignment = 0;

    ID3D12Device10* device = DX12::Device;
    DXCall(device->CreateCommittedResource3(DX12::GetDefaultHeapProps(), D3D12_HEAP_FLAG_NONE, &textureDesc,
                                            D3D12_BARRIER_LAYOUT_COMMON,
                                            nullptr, nullptr, 0, nullptr, IID_PPV_ARGS(&texture.Resource)));

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    texture.SRV = srvAlloc.Index;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MipLevels = uint32_t(numMips);
    srvDesc.Texture3D.MostDetailedMip = 0;
    srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;

    for(uint32_t i = 0; i < DX12::SRVDescriptorHeap.NumHeaps; ++i)
        device->CreateShaderResourceView(texture.Resource, &srvDesc, srvAlloc.Handles[i]);

    texture.Width = uint32_t(width);
    texture.Height = uint32_t(height);
    texture.Depth = uint32_t(depth);
    texture.NumMips = uint32_t(numMips);
    texture.ArraySize = 1;
    texture.Format = format;
    texture.Cubemap = false;

    if(initData != nullptr)
        UploadTextureData(texture, initData);
}

void UploadTextureData(const Texture& texture, const void* initData)
{
    ID3D12Device* device = DX12::Device;
    D3D12_RESOURCE_DESC textureDesc = texture.Resource->GetDesc();

    const uint64_t numSubResources = texture.NumMips * texture.ArraySize;
    uint64_t textureMemSize = 0;
    device->GetCopyableFootprints(&textureDesc, 0, uint32_t(numSubResources), 0, nullptr, nullptr, nullptr, &textureMemSize);

    // Get a GPU upload buffer
    UploadContext uploadContext = DX12::ResourceUploadBegin(textureMemSize);

    UploadTextureData(texture, initData, uploadContext.CmdList, uploadContext.Resource, uploadContext.CPUAddress, uploadContext.ResourceOffset);

    DX12::ResourceUploadEnd(uploadContext);
}

void UploadTextureData(const Texture& texture, const void* initData, ID3D12GraphicsCommandList* cmdList,
                       ID3D12Resource* uploadResource, void* uploadCPUMem, uint64_t resourceOffset)
{
    ID3D12Device* device = DX12::Device;
    D3D12_RESOURCE_DESC textureDesc = texture.Resource->GetDesc();

    const uint64_t arraySize = texture.Cubemap ? texture.ArraySize * 6 : texture.ArraySize;

    const uint64_t numSubResources = texture.NumMips * arraySize;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layouts = (D3D12_PLACED_SUBRESOURCE_FOOTPRINT*)_alloca(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) * numSubResources);
    uint32_t* numRows = (uint32_t*)_alloca(sizeof(uint32_t) * numSubResources);
    uint64_t* rowSizes = (uint64_t*)_alloca(sizeof(uint64_t) * numSubResources);

    uint64_t textureMemSize = 0;
    device->GetCopyableFootprints(&textureDesc, 0, uint32_t(numSubResources), 0, layouts, numRows, rowSizes, &textureMemSize);

    // Get a GPU upload buffer
    uint8_t* uploadMem = reinterpret_cast<uint8_t*>(uploadCPUMem);

    const uint8_t* srcMem = reinterpret_cast<const uint8_t*>(initData);
    const uint64_t srcTexelSize = DirectX::BitsPerPixel(texture.Format) / 8;

    for(uint64_t arrayIdx = 0; arrayIdx < arraySize; ++arrayIdx)
    {
        uint64_t mipWidth = texture.Width;
        for(uint64_t mipIdx = 0; mipIdx < texture.NumMips; ++mipIdx)
        {
            const uint64_t subResourceIdx = mipIdx + (arrayIdx * texture.NumMips);

            const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& subResourceLayout = layouts[subResourceIdx];
            const uint64_t subResourceHeight = numRows[subResourceIdx];
            const uint64_t subResourcePitch = subResourceLayout.Footprint.RowPitch;
            const uint64_t subResourceDepth = subResourceLayout.Footprint.Depth;
            const uint64_t srcPitch = mipWidth * srcTexelSize;
            uint8_t* dstSubResourceMem = uploadMem + subResourceLayout.Offset;

            for(uint64_t z = 0; z < subResourceDepth; ++z)
            {
                for(uint64_t y = 0; y < subResourceHeight; ++y)
                {
                    memcpy(dstSubResourceMem, srcMem, Min(subResourcePitch, srcPitch));
                    dstSubResourceMem += subResourcePitch;
                    srcMem += srcPitch;
                }
            }

            mipWidth = Max(mipWidth / 2, 1ull);
        }
    }

    for(uint64_t subResourceIdx = 0; subResourceIdx < numSubResources; ++subResourceIdx)
    {
        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = texture.Resource;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = uint32_t(subResourceIdx);
        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = uploadResource;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = layouts[subResourceIdx];
        src.PlacedFootprint.Offset += resourceOffset;
        cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }
}

template<typename T>
static void GetTextureData(const Texture& texture, DXGI_FORMAT outFormat, TextureData<T>& texData)
{
    Assert_(DirectX::BitsPerPixel(outFormat) / 8 == sizeof(T));
    Assert_(texture.Depth == 1);

    ReadbackBuffer readbackBuffer;
    DX12::ConvertAndReadbackTexture(texture, outFormat, readbackBuffer);

    texData.Init(texture.Width, texture.Height, texture.ArraySize);
    Assert_(texData.Texels.MemorySize() == readbackBuffer.Size);
    memcpy(texData.Texels.Data(), readbackBuffer.Map(), readbackBuffer.Size);

    readbackBuffer.Shutdown();
}

// Decode a texture into 8-bit fixed point and copies it to the CPU
void GetTextureData(const Texture& texture, TextureData<UByte4N>& textureData, bool srgb)
{
    DXGI_FORMAT format = srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    GetTextureData(texture, format, textureData);
}

// Decode a texture into 16-bit fixed point and copies it to the CPU
void GetTextureData(const Texture& texture, TextureData<UShort4N>& textureData)
{
    GetTextureData(texture, DXGI_FORMAT_R16G16B16A16_UNORM, textureData);
}


// Decode a texture into 16-bit floats and copies it to the CPU
void GetTextureData(const Texture& texture, TextureData<Half4>& textureData)
{
    GetTextureData(texture, DXGI_FORMAT_R16G16B16A16_FLOAT, textureData);
}

// Decode a texture into 32-bit floats and copies it to the CPU
void GetTextureData(const Texture& texture, TextureData<Float4>& textureData)
{
    GetTextureData(texture, DXGI_FORMAT_R32G32B32A32_FLOAT, textureData);
}

void Create2DTexture(Texture& texture, const TextureData<UByte4N>& textureData, bool srgb)
{
    Assert_(textureData.Texels.Size() > 0);
    Assert_(textureData.Width * textureData.Height * textureData.NumSlices == textureData.Texels.Size());
    DXGI_FORMAT format = srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    Create2DTexture(texture, textureData.Width, textureData.Height, 1, textureData.NumSlices, format, false, textureData.Texels.Data());
}

void Create2DTexture(Texture& texture, const TextureData<UShort4N>& textureData)
{
    Assert_(textureData.Texels.Size() > 0);
    Assert_(textureData.Width * textureData.Height * textureData.NumSlices == textureData.Texels.Size());
    DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_UNORM;
    Create2DTexture(texture, textureData.Width, textureData.Height, 1, textureData.NumSlices, format, false, textureData.Texels.Data());
}

void Create2DTexture(Texture& texture, const TextureData<Half4>& textureData)
{
    Assert_(textureData.Texels.Size() > 0);
    Assert_(textureData.Width * textureData.Height * textureData.NumSlices == textureData.Texels.Size());
    Create2DTexture(texture, textureData.Width, textureData.Height, 1, textureData.NumSlices, DXGI_FORMAT_R16G16B16A16_FLOAT, false, textureData.Texels.Data());
}

void Create2DTexture(Texture& texture, const TextureData<Float4>& textureData)
{
    Assert_(textureData.Texels.Size() > 0);
    Assert_(textureData.Width * textureData.Height * textureData.NumSlices == textureData.Texels.Size());
    Create2DTexture(texture, textureData.Width, textureData.Height, 1, textureData.NumSlices, DXGI_FORMAT_R32G32B32A32_FLOAT, false, textureData.Texels.Data());
}

void SaveTextureAsDDS(const Texture& texture, const char* filePath)
{
    WriteLog("Saving DDS file '%s'", filePath);

    ReadbackBuffer readbackBuffer;
    DX12::ConvertAndReadbackTexture(texture, texture.Format, readbackBuffer);

    DirectX::ScratchImage scratchImage;
    scratchImage.Initialize2D(texture.Format, texture.Width, texture.Height, texture.ArraySize, 1);
    memcpy(scratchImage.GetPixels(), readbackBuffer.Map(), readbackBuffer.Size);
    readbackBuffer.Shutdown();

    DXCall(SaveToDDSFile(scratchImage.GetImages(), scratchImage.GetImageCount(),
                        scratchImage.GetMetadata(), DirectX::DDS_FLAGS_FORCE_DX10_EXT, ToWString(filePath).c_str()));
}

void SaveTextureAsEXR(const Texture& texture, const char* filePath)
{
    TextureData<Float4> textureData;
    GetTextureData(texture, textureData);
    SaveTextureAsEXR(textureData, filePath);
}

void SaveTextureAsEXR(const TextureData<Float4>& texture, const char* filePath)
{
    WriteLog("Saving EXR file '%s'", filePath);

    Assert_(texture.Texels.Size() > 0);
    Assert_(texture.Width > 0 && texture.Height > 0);
    Assert_(texture.NumSlices == 1);

    const uint64_t numTexels = texture.Texels.Size();
    std::vector<float> channelDataR;
    std::vector<float> channelDataG;
    std::vector<float> channelDataB;
    channelDataR.resize(numTexels);
    channelDataG.resize(numTexels);
    channelDataB.resize(numTexels);
    for(uint64_t i = 0; i < numTexels; ++i)
    {
        channelDataR[i] = texture.Texels[i].x;
        channelDataG[i] = texture.Texels[i].y;
        channelDataB[i] = texture.Texels[i].z;
    }

    float* imageChannels[3] = { channelDataB.data(), channelDataG.data(), channelDataR.data() };
    const char* channelNames[3] = { "B", "G", "R" };

    EXRImage exrImage;
    exrImage.num_channels = 3;
    exrImage.width = texture.Width;
    exrImage.height = texture.Height;
    exrImage.channel_names = channelNames;
    exrImage.images = imageChannels;

    const char* errorString = nullptr;
    int32_t returnCode = SaveMultiChannelEXR(&exrImage, filePath, &errorString);
    if(returnCode != 0)
    {
        AssertFail_("%s", errorString);
        throw Exception(errorString);
    }
}

void SaveTextureAsPNG(const Texture& texture, const char* filePath)
{
    const bool srgb = DirectX::IsSRGB(texture.Format);
    TextureData<UByte4N> textureData;
    GetTextureData(texture, textureData, srgb);
    SaveTextureAsPNG(textureData, filePath);
}

void SaveTextureAsPNG(const TextureData<UByte4N>& texture, const char* filePath)
{
    WriteLog("Saving PNG file '%s'", filePath);

    DirectX::ScratchImage scratchImage;
    scratchImage.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, texture.Width, texture.Height, texture.NumSlices, 1);
    memcpy(scratchImage.GetPixels(), texture.Texels.Data(), texture.Texels.MemorySize());

    DXCall(SaveToWICFile(scratchImage.GetImages(), scratchImage.GetImageCount(), DirectX::WIC_FLAGS_NONE,
                         DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), ToWString(filePath).c_str()));
}

void SaveTextureAsTIFF(const Texture& texture, const char* filePath)
{
    TextureData<UShort4N> textureData;
    GetTextureData(texture, textureData);
    SaveTextureAsTIFF(textureData, filePath);
}

void SaveTextureAsTIFF(const TextureData<UShort4N>& texture, const char* filePath)
{
    WriteLog("Saving TIFF file '%s'", filePath);

    DirectX::ScratchImage scratchImage;
    scratchImage.Initialize2D(DXGI_FORMAT_R16G16B16A16_UNORM, texture.Width, texture.Height, texture.NumSlices, 1);
    memcpy(scratchImage.GetPixels(), texture.Texels.Data(), texture.Texels.MemorySize());

    DXCall(SaveToWICFile(scratchImage.GetImages(), scratchImage.GetImageCount(), DirectX::WIC_FLAGS_NONE,
                         DirectX::GetWICCodec(DirectX::WIC_CODEC_TIFF), ToWString(filePath).c_str()));
}

// Utility function to map a XY + Side coordinate to a direction vector
Float3 MapXYSToDirection(uint32_t x, uint32_t y, uint32_t s, uint32_t width, uint32_t height)
{
    float u = ((x + 0.5f) / float(width)) * 2.0f - 1.0f;
    float v = ((y + 0.5f) / float(height)) * 2.0f - 1.0f;
    v *= -1.0f;

    Float3 dir = Float3(0.0f);

    // +x, -x, +y, -y, +z, -z
    switch(s) {
    case 0:
        dir = Float3::Normalize(Float3(1.0f, v, -u));
        break;
    case 1:
        dir = Float3::Normalize(Float3(-1.0f, v, u));
        break;
    case 2:
        dir = Float3::Normalize(Float3(u, 1.0f, -v));
        break;
    case 3:
        dir = Float3::Normalize(Float3(u, -1.0f, v));
        break;
    case 4:
        dir = Float3::Normalize(Float3(u, v, 1.0f));
        break;
    case 5:
        dir = Float3::Normalize(Float3(-u, v, -1.0f));
        break;
    }

    return dir;
}

uint32_t CalculateNumMips(uint32_t width, uint32_t height, uint32_t depth)
{
    uint32_t mipLevels = 1;

    while(height > 1 || width > 1 || depth > 1)
    {
        if(height > 1)
            height >>= 1;

        if(width > 1)
            width >>= 1;

        if(depth > 1)
            width >>= 1;

        mipLevels += 1;
    }

    return mipLevels;
}

}