//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
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
static uint64 NumMipLevels(uint64 width, uint64 height, uint64 depth = 1)
{
    uint64 numMips = 0;
    uint64 size = std::max(std::max(width, height), depth);
    while(1ull << numMips <= size)
        ++numMips;

    if(1ull << numMips < size)
        ++numMips;

    return numMips;
}

void LoadTexture(Texture& texture, const wchar* filePath, bool forceSRGB)
{
    texture.Shutdown();
    if(FileExists(filePath) == false)
        throw Exception(MakeString(L"Texture file with path '%ls' does not exist", filePath));

    DirectX::ScratchImage image;

    const std::wstring extension = GetFileExtension(filePath);
    if(extension == L"DDS" || extension == L"dds")
    {
        DXCall(DirectX::LoadFromDDSFile(filePath, DirectX::DDS_FLAGS_NONE, nullptr, image));
    }
    else if(extension == L"TGA" || extension == L"tga")
    {
        DirectX::ScratchImage tempImage;
        DXCall(DirectX::LoadFromTGAFile(filePath, nullptr, tempImage));
        DXCall(DirectX::GenerateMipMaps(*tempImage.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, image, false));
    }
    else
    {
        DirectX::ScratchImage tempImage;
        DXCall(DirectX::LoadFromWICFile(filePath, DirectX::WIC_FLAGS_NONE, nullptr, tempImage));
        DXCall(DirectX::GenerateMipMaps(*tempImage.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, image, false));
    }

    const DirectX::TexMetadata& metaData = image.GetMetadata();
    DXGI_FORMAT format = metaData.format;
    if(forceSRGB)
        format = DirectX::MakeSRGB(format);

    const bool is3D = metaData.dimension == DirectX::TEX_DIMENSION_TEXTURE3D;

    D3D12_RESOURCE_DESC textureDesc = { };
    textureDesc.MipLevels = uint16(metaData.mipLevels);
	textureDesc.Format = format;
	textureDesc.Width = uint32(metaData.width);
	textureDesc.Height = uint32(metaData.height);
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	textureDesc.DepthOrArraySize = is3D ? uint16(metaData.depth) : uint16(metaData.arraySize);
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = is3D ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Alignment = 0;

    ID3D12Device* device = DX12::Device;
    DXCall(device->CreateCommittedResource(DX12::GetDefaultHeapProps(), D3D12_HEAP_FLAG_NONE, &textureDesc,
			                               D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&texture.Resource)));
    texture.Resource->SetName(filePath);

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
        srvDesc.TextureCube.MipLevels = uint32(metaData.mipLevels);
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        srvDescPtr = &srvDesc;
    }

    for(uint32 i = 0; i < DX12::SRVDescriptorHeap.NumHeaps; ++i)
        device->CreateShaderResourceView(texture.Resource, srvDescPtr, srvAlloc.Handles[i]);

    const uint64 numSubResources = metaData.mipLevels * metaData.arraySize;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layouts = (D3D12_PLACED_SUBRESOURCE_FOOTPRINT*)_alloca(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) * numSubResources);
    uint32* numRows = (uint32*)_alloca(sizeof(uint32) * numSubResources);
    uint64* rowSizes = (uint64*)_alloca(sizeof(uint64) * numSubResources);

	uint64 textureMemSize = 0;
    device->GetCopyableFootprints(&textureDesc, 0, uint32(numSubResources), 0, layouts, numRows, rowSizes, &textureMemSize);

	// Get a GPU upload buffer
    UploadContext uploadContext = DX12::ResourceUploadBegin(textureMemSize);
    uint8* uploadMem = reinterpret_cast<uint8*>(uploadContext.CPUAddress);

    for(uint64 arrayIdx = 0; arrayIdx < metaData.arraySize; ++arrayIdx)
    {

        for(uint64 mipIdx = 0; mipIdx < metaData.mipLevels; ++mipIdx)
        {
            const uint64 subResourceIdx = mipIdx + (arrayIdx * metaData.mipLevels);

            const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& subResourceLayout = layouts[subResourceIdx];
            const uint64 subResourceHeight = numRows[subResourceIdx];
            const uint64 subResourcePitch = subResourceLayout.Footprint.RowPitch;
            const uint64 subResourceDepth = subResourceLayout.Footprint.Depth;
            uint8* dstSubResourceMem = reinterpret_cast<uint8*>(uploadMem) + subResourceLayout.Offset;

            for(uint64 z = 0; z < subResourceDepth; ++z)
            {
                const DirectX::Image* subImage = image.GetImage(mipIdx, arrayIdx, z);
                Assert_(subImage != nullptr);
                const uint8* srcSubResourceMem = subImage->pixels;

                for(uint64 y = 0; y < subResourceHeight; ++y)
                {
                    memcpy(dstSubResourceMem, srcSubResourceMem, Min(subResourcePitch, subImage->rowPitch));
                    dstSubResourceMem += subResourcePitch;
                    srcSubResourceMem += subImage->rowPitch;
                }
            }
        }
    }

    for(uint64 subResourceIdx = 0; subResourceIdx < numSubResources; ++subResourceIdx)
    {
        D3D12_TEXTURE_COPY_LOCATION dst = { };
        dst.pResource = texture.Resource;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = uint32(subResourceIdx);
        D3D12_TEXTURE_COPY_LOCATION src = { };
        src.pResource = uploadContext.Resource;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = layouts[subResourceIdx];
        src.PlacedFootprint.Offset += uploadContext.ResourceOffset;
        uploadContext.CmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }

    DX12::ResourceUploadEnd(uploadContext);

    texture.Width = uint32(metaData.width);
    texture.Height = uint32(metaData.height);
    texture.Depth = uint32(metaData.depth);
    texture.NumMips = uint32(metaData.mipLevels);
    texture.ArraySize = uint32(metaData.arraySize);
    texture.Format = metaData.format;
    texture.Cubemap = metaData.IsCubemap() ? 1 : 0;
}

void Create2DTexture(Texture& texture, uint64 width, uint64 height, uint64 numMips,
                     uint64 arraySize, DXGI_FORMAT format, bool cubeMap, const void* initData)
{
    texture.Shutdown();

    Assert_(width > 0);
    Assert_(height > 0);
    Assert_(arraySize > 0);
    const uint64 maxMipLevels = NumMipLevels(width, height);
    if(numMips == 0)
        numMips = maxMipLevels;
    Assert_(numMips <= maxMipLevels);

    const uint64 srvArraySize = arraySize;
    if(cubeMap)
        arraySize *= 6;

    D3D12_RESOURCE_DESC textureDesc = { };
    textureDesc.MipLevels = uint16(numMips);
    textureDesc.Format = format;
    textureDesc.Width = uint32(width);
    textureDesc.Height = uint32(height);
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = uint16(arraySize);
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Alignment = 0;

    ID3D12Device* device = DX12::Device;
    DXCall(device->CreateCommittedResource(DX12::GetDefaultHeapProps(), D3D12_HEAP_FLAG_NONE, &textureDesc,
                                           initData ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                           nullptr, IID_PPV_ARGS(&texture.Resource)));

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    texture.SRV = srvAlloc.Index;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    if(srvArraySize == 1 && cubeMap == false)
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = uint32(numMips);
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    }
    else if(srvArraySize > 1 && cubeMap == false)
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MipLevels = uint32(numMips);
        srvDesc.Texture2DArray.MostDetailedMip = 0;
        srvDesc.Texture2DArray.PlaneSlice = 0;
        srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
        srvDesc.Texture2DArray.ArraySize = uint32(srvArraySize);
        srvDesc.Texture2DArray.FirstArraySlice = 0;
    }
    else if(srvArraySize == 1 && cubeMap)
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = uint32(numMips);
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    }
    else if(srvArraySize > 1 && cubeMap)
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCubeArray.MipLevels = uint32(numMips);
        srvDesc.TextureCubeArray.MostDetailedMip = 0;
        srvDesc.TextureCubeArray.ResourceMinLODClamp = 0.0f;
        srvDesc.TextureCubeArray.First2DArrayFace = 0;
        srvDesc.TextureCubeArray.NumCubes = uint32(srvArraySize);
    }

    for(uint32 i = 0; i < DX12::SRVDescriptorHeap.NumHeaps; ++i)
        device->CreateShaderResourceView(texture.Resource, &srvDesc, srvAlloc.Handles[i]);

    texture.Width = uint32(width);
    texture.Height = uint32(height);
    texture.Depth = 1;
    texture.NumMips = uint32(numMips);
    texture.ArraySize = uint32(srvArraySize);
    texture.Format = format;
    texture.Cubemap = cubeMap;

    if(initData != nullptr)
        UploadTextureData(texture, initData);
}

void Create3DTexture(Texture& texture, uint64 width, uint64 height, uint64 depth, uint64 numMips, DXGI_FORMAT format, const void* initData)
{
        texture.Shutdown();

    Assert_(width > 0);
    Assert_(height > 0);
    Assert_(depth > 0);
    const uint64 maxMipLevels = NumMipLevels(width, height, depth);
    if(numMips == 0)
        numMips = maxMipLevels;
    Assert_(numMips <= maxMipLevels);

    D3D12_RESOURCE_DESC textureDesc = { };
    textureDesc.MipLevels = uint16(numMips);
    textureDesc.Format = format;
    textureDesc.Width = uint32(width);
    textureDesc.Height = uint32(height);
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = uint16(depth);
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Alignment = 0;

    ID3D12Device* device = DX12::Device;
    DXCall(device->CreateCommittedResource(DX12::GetDefaultHeapProps(), D3D12_HEAP_FLAG_NONE, &textureDesc,
                                           initData ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                           nullptr, IID_PPV_ARGS(&texture.Resource)));

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    texture.SRV = srvAlloc.Index;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MipLevels = uint32(numMips);
    srvDesc.Texture3D.MostDetailedMip = 0;
    srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;

    for(uint32 i = 0; i < DX12::SRVDescriptorHeap.NumHeaps; ++i)
        device->CreateShaderResourceView(texture.Resource, &srvDesc, srvAlloc.Handles[i]);

    texture.Width = uint32(width);
    texture.Height = uint32(height);
    texture.Depth = uint32(depth);
    texture.NumMips = uint32(numMips);
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

    const uint64 numSubResources = texture.NumMips * texture.ArraySize;
    uint64 textureMemSize = 0;
    device->GetCopyableFootprints(&textureDesc, 0, uint32(numSubResources), 0, nullptr, nullptr, nullptr, &textureMemSize);

    // Get a GPU upload buffer
    UploadContext uploadContext = DX12::ResourceUploadBegin(textureMemSize);

    UploadTextureData(texture, initData, uploadContext.CmdList, uploadContext.Resource, uploadContext.CPUAddress, uploadContext.ResourceOffset);

    DX12::ResourceUploadEnd(uploadContext);
}

void UploadTextureData(const Texture& texture, const void* initData, ID3D12GraphicsCommandList* cmdList,
                       ID3D12Resource* uploadResource, void* uploadCPUMem, uint64 resourceOffset)
{
    ID3D12Device* device = DX12::Device;
    D3D12_RESOURCE_DESC textureDesc = texture.Resource->GetDesc();

    const uint64 arraySize = texture.Cubemap ? texture.ArraySize * 6 : texture.ArraySize;

    const uint64 numSubResources = texture.NumMips * arraySize;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layouts = (D3D12_PLACED_SUBRESOURCE_FOOTPRINT*)_alloca(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) * numSubResources);
    uint32* numRows = (uint32*)_alloca(sizeof(uint32) * numSubResources);
    uint64* rowSizes = (uint64*)_alloca(sizeof(uint64) * numSubResources);

    uint64 textureMemSize = 0;
    device->GetCopyableFootprints(&textureDesc, 0, uint32(numSubResources), 0, layouts, numRows, rowSizes, &textureMemSize);

    // Get a GPU upload buffer
    uint8* uploadMem = reinterpret_cast<uint8*>(uploadCPUMem);

    const uint8* srcMem = reinterpret_cast<const uint8*>(initData);
    const uint64 srcTexelSize = DirectX::BitsPerPixel(texture.Format) / 8;

    for(uint64 arrayIdx = 0; arrayIdx < arraySize; ++arrayIdx)
    {
        uint64 mipWidth = texture.Width;
        for(uint64 mipIdx = 0; mipIdx < texture.NumMips; ++mipIdx)
        {
            const uint64 subResourceIdx = mipIdx + (arrayIdx * texture.NumMips);

            const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& subResourceLayout = layouts[subResourceIdx];
            const uint64 subResourceHeight = numRows[subResourceIdx];
            const uint64 subResourcePitch = subResourceLayout.Footprint.RowPitch;
            const uint64 subResourceDepth = subResourceLayout.Footprint.Depth;
            const uint64 srcPitch = mipWidth * srcTexelSize;
            uint8* dstSubResourceMem = uploadMem + subResourceLayout.Offset;

            for(uint64 z = 0; z < subResourceDepth; ++z)
            {
                for(uint64 y = 0; y < subResourceHeight; ++y)
                {
                    memcpy(dstSubResourceMem, srcMem, Min(subResourcePitch, srcPitch));
                    dstSubResourceMem += subResourcePitch;
                    srcMem += srcPitch;
                }
            }

            mipWidth = Max(mipWidth / 2, 1ull);
        }
    }

    for(uint64 subResourceIdx = 0; subResourceIdx < numSubResources; ++subResourceIdx)
    {
        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = texture.Resource;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = uint32(subResourceIdx);
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

void SaveTextureAsDDS(const Texture& texture, const wchar* filePath)
{
    WriteLog("Saving DDS file '%ls'", filePath);

    ReadbackBuffer readbackBuffer;
    DX12::ConvertAndReadbackTexture(texture, texture.Format, readbackBuffer);

    DirectX::ScratchImage scratchImage;
    scratchImage.Initialize2D(texture.Format, texture.Width, texture.Height, texture.ArraySize, 1);
    memcpy(scratchImage.GetPixels(), readbackBuffer.Map(), readbackBuffer.Size);
    readbackBuffer.Shutdown();

    DXCall(SaveToDDSFile(scratchImage.GetImages(), scratchImage.GetImageCount(),
                        scratchImage.GetMetadata(), DirectX::DDS_FLAGS_FORCE_DX10_EXT, filePath));
}

void SaveTextureAsEXR(const Texture& texture, const wchar* filePath)
{
    TextureData<Float4> textureData;
    GetTextureData(texture, textureData);
    SaveTextureAsEXR(textureData, filePath);
}

void SaveTextureAsEXR(const TextureData<Float4>& texture, const wchar* filePath)
{
    WriteLog("Saving EXR file '%ls'", filePath);

    Assert_(texture.Texels.Size() > 0);
    Assert_(texture.Width > 0 && texture.Height > 0);
    Assert_(texture.NumSlices == 1);

    const uint64 numTexels = texture.Texels.Size();
    std::vector<float> channelDataR;
    std::vector<float> channelDataG;
    std::vector<float> channelDataB;
    channelDataR.resize(numTexels);
    channelDataG.resize(numTexels);
    channelDataB.resize(numTexels);
    for(uint64 i = 0; i < numTexels; ++i)
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

    std::string filePathAnsi = WStringToAnsi(filePath);

    const char* errorString = nullptr;
    int returnCode = SaveMultiChannelEXR(&exrImage, filePathAnsi.c_str(), &errorString);
    if(returnCode != 0)
    {
        AssertFail_("%s", errorString);
        throw Exception(AnsiToWString(errorString));
    }
}

void SaveTextureAsPNG(const Texture& texture, const wchar* filePath)
{
    const bool srgb = DirectX::IsSRGB(texture.Format);
    TextureData<UByte4N> textureData;
    GetTextureData(texture, textureData, srgb);
    SaveTextureAsPNG(textureData, filePath);
}

void SaveTextureAsPNG(const TextureData<UByte4N>& texture, const wchar* filePath)
{
    WriteLog("Saving PNG file '%ls'", filePath);

    DirectX::ScratchImage scratchImage;
    scratchImage.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, texture.Width, texture.Height, texture.NumSlices, 1);
    memcpy(scratchImage.GetPixels(), texture.Texels.Data(), texture.Texels.MemorySize());

    DXCall(SaveToWICFile(scratchImage.GetImages(), scratchImage.GetImageCount(), DirectX::WIC_FLAGS_NONE,
                         DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), filePath));
}

void SaveTextureAsTIFF(const Texture& texture, const wchar* filePath)
{
    TextureData<UShort4N> textureData;
    GetTextureData(texture, textureData);
    SaveTextureAsTIFF(textureData, filePath);
}

void SaveTextureAsTIFF(const TextureData<UShort4N>& texture, const wchar* filePath)
{
    WriteLog("Saving TIFF file '%ls'", filePath);

    DirectX::ScratchImage scratchImage;
    scratchImage.Initialize2D(DXGI_FORMAT_R16G16B16A16_UNORM, texture.Width, texture.Height, texture.NumSlices, 1);
    memcpy(scratchImage.GetPixels(), texture.Texels.Data(), texture.Texels.MemorySize());

    DXCall(SaveToWICFile(scratchImage.GetImages(), scratchImage.GetImageCount(), DirectX::WIC_FLAGS_NONE,
                         DirectX::GetWICCodec(DirectX::WIC_CODEC_TIFF), filePath));
}

// Utility function to map a XY + Side coordinate to a direction vector
Float3 MapXYSToDirection(uint64 x, uint64 y, uint64 s, uint64 width, uint64 height)
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

}