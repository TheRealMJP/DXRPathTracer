//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "../PCH.h"

#include "../InterfacePointers.h"
#include "../Utility.h"
#include "../Containers.h"
#include "DX12.h"
#include "DX12_Upload.h"
#include "DX12_Helpers.h"
#include "../Shaders/ShaderShared.h"

namespace SampleFramework12
{

struct PersistentDescriptorAlloc
{
    D3D12_CPU_DESCRIPTOR_HANDLE Handles[DX12::RenderLatency] = { };
    DescriptorIndex Index = InvalidDescriptorIndex;
};

struct TempDescriptorAlloc
{
    D3D12_CPU_DESCRIPTOR_HANDLE StartCPUHandle = { };
    D3D12_GPU_DESCRIPTOR_HANDLE StartGPUHandle = { };
    DescriptorIndex StartIndex = InvalidDescriptorIndex;
};

// Wrapper for D3D12 descriptor heaps that supports persistent and temporary allocations
struct DescriptorHeap
{
    ID3D12DescriptorHeap* Heaps[DX12::RenderLatency] = { };
    uint32_t NumPersistent = 0;
    uint32_t PersistentAllocated = 0;
    Array<DescriptorIndex> DeadList;
    uint32_t NumTemporary = 0;
    volatile int64_t TemporaryAllocated = 0;
    uint32_t HeapIndex = 0;
    uint32_t NumHeaps = 0;
    uint32_t DescriptorSize = 0;
    bool32 ShaderVisible = false;
    D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    D3D12_CPU_DESCRIPTOR_HANDLE CPUStart[DX12::RenderLatency] = { };
    D3D12_GPU_DESCRIPTOR_HANDLE GPUStart[DX12::RenderLatency] = { };
    SRWLOCK Lock = SRWLOCK_INIT;

    void Init(uint32_t numPersistent, uint32_t numTemporary, D3D12_DESCRIPTOR_HEAP_TYPE heapType, bool shaderVisible);
    void Shutdown();

    PersistentDescriptorAlloc AllocatePersistent(DescriptorIndex index = InvalidDescriptorIndex);
    void FreePersistent(DescriptorIndex& idx);
    void FreePersistent(D3D12_CPU_DESCRIPTOR_HANDLE& handle);
    void FreePersistent(D3D12_GPU_DESCRIPTOR_HANDLE& handle);

    TempDescriptorAlloc AllocateTemporary(uint32_t count);
    void EndFrame();

    D3D12_CPU_DESCRIPTOR_HANDLE CPUHandleFromIndex(uint32_t descriptorIdx) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GPUHandleFromIndex(uint32_t descriptorIdx) const;

    D3D12_CPU_DESCRIPTOR_HANDLE CPUHandleFromIndex(uint32_t descriptorIdx, uint64_t heapIdx) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GPUHandleFromIndex(uint32_t descriptorIdx, uint64_t heapIdx) const;

    DescriptorIndex IndexFromHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle) const;
    DescriptorIndex IndexFromHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) const;

    ID3D12DescriptorHeap* CurrentHeap() const;
    uint32_t TotalNumDescriptors() const { return NumPersistent + NumTemporary; }
};

struct BufferInit
{
    uint64_t Size = 0;
    uint64_t Alignment = 0;
    bool32 Dynamic = false;
    bool32 CPUAccessible = false;
    bool32 AllowUAV = false;
    bool32 RTAccelStructure = false;
    const void* InitData = nullptr;
    ID3D12Heap* Heap = nullptr;
    uint64_t HeapOffset = uint64_t(-1);
    const char* Name = nullptr;
};

struct BufferReadToWriteBarrierDesc
{
    D3D12_BARRIER_SYNC SyncBefore = D3D12_BARRIER_SYNC_ALL_SHADING;
    D3D12_BARRIER_SYNC SyncAfter = D3D12_BARRIER_SYNC_ALL_SHADING;
    D3D12_BARRIER_ACCESS AccessBefore = D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
    D3D12_BARRIER_ACCESS AccessAfter = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
};

struct BufferWriteToReadBarrierDesc
{
    D3D12_BARRIER_SYNC SyncBefore = D3D12_BARRIER_SYNC_ALL_SHADING;
    D3D12_BARRIER_SYNC SyncAfter = D3D12_BARRIER_SYNC_ALL_SHADING;
    D3D12_BARRIER_ACCESS AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
    D3D12_BARRIER_ACCESS AccessAfter = D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
};

struct BufferWriteToWriteBarrierDesc
{
    D3D12_BARRIER_SYNC SyncBefore = D3D12_BARRIER_SYNC_ALL_SHADING;
    D3D12_BARRIER_SYNC SyncAfter = D3D12_BARRIER_SYNC_ALL_SHADING;
    D3D12_BARRIER_ACCESS AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
    D3D12_BARRIER_ACCESS AccessAfter = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
};

struct Buffer
{
    ID3D12Resource* Resource = nullptr;
    uint64_t CurrBuffer = 0;
    uint8_t* CPUAddress = 0;
    uint64_t GPUAddress = 0;
    uint64_t Alignment = 0;
    uint64_t Size = 0;
    bool32 Dynamic = false;
    bool32 CPUAccessible = false;
    ID3D12Heap* Heap = nullptr;
    uint64_t HeapOffset = 0;
    uint64_t UploadFrame = uint64_t(-1);
    uint64_t CreateFrame = uint64_t(-1);

    void Initialize(const BufferInit& init);
    void Shutdown();

    MapResult Map();
    MapResult MapAndSetData(const void* data, uint64_t dataSize);
    template<typename T> MapResult MapAndSetData(const T& data) { return MapAndSetData(&data, sizeof(T)); }
    uint64_t QueueUpload(ID3D12Resource* srcResource, uint64_t srcOffset, uint64_t srcSize, uint64_t dstOffset);

    D3D12_BUFFER_BARRIER ReadToWriteBarrier(BufferReadToWriteBarrierDesc desc = BufferReadToWriteBarrierDesc()) const;
    D3D12_BUFFER_BARRIER WriteToReadBarrier(BufferWriteToReadBarrierDesc desc = BufferWriteToReadBarrierDesc()) const;
    D3D12_BUFFER_BARRIER WriteToWriteBarrier(BufferWriteToWriteBarrierDesc desc = BufferWriteToWriteBarrierDesc()) const;

    uint64_t CycleBuffer();

    bool Initialized() const { return Size > 0; }

    #if UseAsserts_
        bool ReadyForBinding() const;
    #endif
};

// For aligning to float4 boundaries
#define Float4Align __declspec(align(16))
#define Float4Align_ __declspec(align(16))

struct ConstantBufferInit
{
    uint64_t Size = 0;
    bool32 Dynamic = true;
    bool32 CPUAccessible = true;
    const void* InitData = nullptr;
    ID3D12Heap* Heap = nullptr;
    uint64_t HeapOffset = 0;
    const char* Name = nullptr;
};

struct ConstantBuffer
{
    Buffer InternalBuffer;
    uint64_t CurrentGPUAddress = 0;

    void Initialize(const ConstantBufferInit& init);
    void Shutdown();

    void SetAsGfxRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter) const;
    void SetAsComputeRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter) const;

    void* Map();
    template<typename T> T* Map() { return reinterpret_cast<T*>(Map()); }
    void MapAndSetData(const void* data, uint64_t dataSize);
    template<typename T> void MapAndSetData(const T& data) { MapAndSetData(&data, sizeof(T)); }
    void QueueUpload(ID3D12Resource* srcResource, uint64_t srcOffset, uint64_t srcSize, uint64_t dstOffset);
};

struct StructuredBufferInit
{
    uint64_t Stride = 0;
    uint64_t NumElements = 0;
    bool32 CreateUAV = false;
    bool32 Dynamic = false;
    bool32 CPUAccessible = false;
    const void* InitData = nullptr;
    bool32 ShaderTable = false;
    ID3D12Heap* Heap = nullptr;
    uint64_t HeapOffset = 0;
    const char* Name = nullptr;
};

struct StructuredBuffer
{
    Buffer InternalBuffer;
    uint64_t Stride = 0;
    uint64_t NumElements = 0;
    DescriptorIndex SRV = InvalidDescriptorIndex;
    bool32 IsShaderTable = false;
    DescriptorIndex UAV = InvalidDescriptorIndex;
    uint64_t GPUAddress = 0;

    void Initialize(const StructuredBufferInit& init);
    void Shutdown();

    D3D12_VERTEX_BUFFER_VIEW VBView() const;
    D3D12_INDEX_BUFFER_VIEW IBView() const;
    ID3D12Resource* Resource() const { return InternalBuffer.Resource; }

    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE ShaderTable(uint64_t startElement = 0, uint64_t numElements = uint64_t(-1)) const;
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE ShaderRecord(uint64_t element) const;

    void* Map();
    template<typename T> T* Map() { return reinterpret_cast<T*>(Map()); }
    void MapAndSetData(const void* data, uint64_t numElements);
    void QueueUpload(ID3D12Resource* srcResource, uint64_t srcOffset, uint64_t srcNumElements, uint64_t dstElemOffset);

    uint64_t CycleBuffer();

private:

    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc(uint64_t bufferIdx) const;
    void UpdateDynamicSRV() const;
};

struct FormattedBufferInit
{
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
    uint64_t NumElements = 0;
    bool32 CreateUAV = false;
    bool32 Dynamic = false;
    bool32 CPUAccessible = false;
    const void* InitData = nullptr;
    ID3D12Heap* Heap = nullptr;
    uint64_t HeapOffset = 0;
    const char* Name = nullptr;
};

struct FormattedBuffer
{
    Buffer InternalBuffer;
    uint64_t Stride = 0;
    uint64_t NumElements = 0;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
    DescriptorIndex SRV = InvalidDescriptorIndex;
    DescriptorIndex UAV = InvalidDescriptorIndex;
    uint64_t GPUAddress = 0;

    void Initialize(const FormattedBufferInit& init);
    void Shutdown();

    D3D12_INDEX_BUFFER_VIEW IBView() const;
    ID3D12Resource* Resource() const { return InternalBuffer.Resource; }

    void* Map();
    template<typename T> T* Map() { return reinterpret_cast<T*>(Map()); };
    void MapAndSetData(const void* data, uint64_t numElements);
    void QueueUpload(ID3D12Resource* srcResource, uint64_t srcOffset, uint64_t srcNumElements, uint64_t dstElemOffset);

    uint64_t CycleBuffer();

private:

    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc(uint64_t bufferIdx) const;
    void UpdateDynamicSRV() const;
};

struct RawBufferInit
{
    uint64_t NumElements = 0;
    bool32 CreateUAV = false;
    bool32 Dynamic = false;
    bool32 CPUAccessible = false;
    const void* InitData = nullptr;
    ID3D12Heap* Heap = nullptr;
    uint64_t HeapOffset = 0;
    const char* Name = nullptr;
};

struct RawBuffer
{
    Buffer InternalBuffer;
    uint64_t NumElements = 0;
    DescriptorIndex SRV = InvalidDescriptorIndex;
    DescriptorIndex UAV = InvalidDescriptorIndex;
    uint64_t GPUAddress = 0;

    static const uint64_t Stride = 4;

    void Initialize(const RawBufferInit& init);
    void Shutdown();

    ID3D12Resource* Resource() const { return InternalBuffer.Resource; }

    void* Map();
    template<typename T> T* Map() { return reinterpret_cast<T*>(Map()); };
    void MapAndSetData(const void* data, uint64_t numElements);
    void QueueUpload(ID3D12Resource* srcResource, uint64_t srcOffset, uint64_t srcNumElements, uint64_t dstElemOffset);

    uint64_t CycleBuffer();

    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc(uint64_t bufferIdx) const;

private:

    void UpdateDynamicSRV() const;
};

struct RTAccelStructureInit
{
    uint64_t Size = 0;
    ID3D12Heap* Heap = nullptr;
    uint64_t HeapOffset = 0;
    const char* Name = nullptr;
};

struct RTAccelStructure
{
    Buffer InternalBuffer;
    uint64_t Size = 0;
    DescriptorIndex SRV = InvalidDescriptorIndex;
    uint64_t GPUAddress = 0;

    void Initialize(const RTAccelStructureInit& init);
    void Shutdown();

    ID3D12Resource* Resource() const { return InternalBuffer.Resource; }

    D3D12_BUFFER_BARRIER TopLevelPostBuildBarrier() const;
    D3D12_BUFFER_BARRIER BottomLevelPostBuildBarrier() const;
};

struct ReadbackBuffer
{
    ID3D12Resource* Resource = nullptr;
    uint64_t Size = 0;

    void Initialize(uint64_t size);
    void Shutdown();

    const void* Map() const;
    template<typename T> const T* Map() const { return reinterpret_cast<const T*>(Map()); };
    void Unmap() const;
};

struct Fence
{
    ID3D12Fence* D3DFence = nullptr;
    HANDLE FenceEvent = INVALID_HANDLE_VALUE;

    void Init(uint64_t initialValue = 0);
    void Shutdown();

    void Signal(ID3D12CommandQueue* queue, uint64_t fenceValue);
    void Wait(uint64_t fenceValue);
    bool Signaled(uint64_t fenceValue);
    void Clear(uint64_t fenceValue);
};

struct Texture
{
    DescriptorIndex SRV;
    ID3D12Resource* Resource = nullptr;
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t Depth = 0;
    uint32_t NumMips = 0;
    uint32_t ArraySize = 0;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
    bool32 Cubemap = false;

    bool Valid() const
    {
        return Resource != nullptr;
    }

    void Shutdown();

    D3D12_BARRIER_SUBRESOURCE_RANGE BarrierRange(uint32_t startMipLevel, uint32_t numMipLevels, uint32_t startArraySlice, uint32_t numArraySlices) const;
};

struct RenderTextureInit
{
    uint32_t Width = 0;
    uint32_t Height = 0;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
    uint32_t MSAASamples = 1;
    uint32_t ArraySize = 1;
    bool32 CreateUAV = false;
    bool32 CreateRTV = true;
    bool32 CubeMap = false;
    uint32_t NumMips = 1;
    D3D12_BARRIER_LAYOUT InitialLayout = D3D12_BARRIER_LAYOUT_UNDEFINED;
    const char* Name = nullptr;
};

enum QueueVisibility : uint32_t
{
    Direct = 0,
    Compute,
    ComputeAndDirect,

    NumQueueVisibilities,
};

struct RTWritableBarrierDesc
{
    bool FirstAccess = false;
    bool Discard = false;
    QueueVisibility QueueVisibilityAfter = Direct;
    D3D12_BARRIER_SYNC SyncBefore = D3D12_BARRIER_SYNC_ALL_SHADING;
    D3D12_BARRIER_ACCESS AccessBefore = D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
    D3D12_BARRIER_LAYOUT LayoutBefore = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE;
    uint32_t StartMipLevel = 0;
    uint32_t NumMipLevels = uint32_t(-1);
    uint32_t StartArraySlice = 0;
    uint32_t NumArraySlices = uint32_t(-1);
};

struct RTReadableBarrierDesc
{
    D3D12_BARRIER_SYNC SyncAfter = D3D12_BARRIER_SYNC_ALL_SHADING;
    QueueVisibility QueueVisibilityBefore = Direct;
    QueueVisibility QueueVisibilityAfter = Direct;
    uint32_t StartMipLevel = 0;
    uint32_t NumMipLevels = uint32_t(-1);
    uint32_t StartArraySlice = 0;
    uint32_t NumArraySlices = uint32_t(-1);
};

struct RTMemoryBarrierDesc
{
    D3D12_BARRIER_SYNC SyncBefore = D3D12_BARRIER_SYNC_ALL_SHADING;
    D3D12_BARRIER_SYNC SyncAfter = D3D12_BARRIER_SYNC_ALL_SHADING;
    D3D12_BARRIER_LAYOUT Layout = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS;
    uint32_t StartMipLevel = 0;
    uint32_t NumMipLevels = uint32_t(-1);
    uint32_t StartArraySlice = 0;
    uint32_t NumArraySlices = uint32_t(-1);
};

struct RenderTexture
{
    Texture Texture;
    D3D12_CPU_DESCRIPTOR_HANDLE RTV = { };
    DescriptorIndex UAV;
    Array<D3D12_CPU_DESCRIPTOR_HANDLE> ArrayRTVs;
    Array<DescriptorIndex> MipLevelUAVs;
    uint32_t MSAASamples = 0;
    uint32_t MSAAQuality = 0;
    bool32 HasRTV = false;

    void Initialize(const RenderTextureInit& init);
    void Shutdown();

    D3D12_TEXTURE_BARRIER RTWritableBarrier(RTWritableBarrierDesc desc = RTWritableBarrierDesc()) const;
    D3D12_TEXTURE_BARRIER UAVWritableBarrier(RTWritableBarrierDesc desc = RTWritableBarrierDesc()) const;
    D3D12_TEXTURE_BARRIER RTToShaderReadableBarrier(RTReadableBarrierDesc desc = RTReadableBarrierDesc()) const;
    D3D12_TEXTURE_BARRIER UAVToShaderReadableBarrier(RTReadableBarrierDesc desc = RTReadableBarrierDesc()) const;
    D3D12_TEXTURE_BARRIER MemoryBarrier(RTMemoryBarrierDesc desc = RTMemoryBarrierDesc()) const;

    uint32_t SRV() const { return Texture.SRV; }
    uint32_t Width() const { return Texture.Width; }
    uint32_t Height() const { return Texture.Height; }
    DXGI_FORMAT Format() const { return Texture.Format; }
    ID3D12Resource* Resource() const { return Texture.Resource; }
    uint32_t SubResourceIndex(uint32_t mipLevel, uint32_t arraySlice) const { return arraySlice * Texture.NumMips + mipLevel; }
};

struct VolumeTextureInit
{
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t Depth = 0;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
    D3D12_BARRIER_LAYOUT InitialLayout = D3D12_BARRIER_LAYOUT_UNDEFINED;
    const char* Name = nullptr;
};

struct VolumeTexture
{
    Texture Texture;
    DescriptorIndex UAV;

    void Initialize(const VolumeTextureInit& init);
    void Shutdown();

    uint32_t SRV() const { return Texture.SRV; }
    uint32_t Width() const { return Texture.Width; }
    uint32_t Height() const { return Texture.Height; }
    uint32_t Depth() const { return Texture.Depth; }
    DXGI_FORMAT Format() const { return Texture.Format; }
    ID3D12Resource* Resource() const { return Texture.Resource; }
};

struct DepthBufferInit
{
    uint32_t Width = 0;
    uint32_t Height = 0;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
    uint32_t MSAASamples = 1;
    uint32_t ArraySize = 1;
    D3D12_BARRIER_LAYOUT InitialLayout = D3D12_BARRIER_LAYOUT_UNDEFINED;
    const char* Name = nullptr;
};

struct DepthWritableBarrierDesc
{
    bool FirstAccess = false;
    bool Discard = false;
    D3D12_BARRIER_SYNC SyncBefore = D3D12_BARRIER_SYNC_ALL_SHADING;
    D3D12_BARRIER_ACCESS AccessBefore = D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
    D3D12_BARRIER_LAYOUT LayoutBefore = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE;
    uint32_t StartArraySlice = 0;
    uint32_t NumArraySlices = uint32_t(-1);
};

struct DepthReadableBarrierDesc
{
    D3D12_BARRIER_SYNC SyncAfter = D3D12_BARRIER_SYNC_ALL_SHADING | D3D12_BARRIER_SYNC_DEPTH_STENCIL;
    D3D12_BARRIER_ACCESS AccessAfter = D3D12_BARRIER_ACCESS_SHADER_RESOURCE | D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
    D3D12_BARRIER_LAYOUT LayoutAfter = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ;
    uint32_t StartArraySlice = 0;
    uint32_t NumArraySlices = uint32_t(-1);
};

struct DepthShaderReadableBarrierDesc
{
    D3D12_BARRIER_SYNC SyncAfter = D3D12_BARRIER_SYNC_ALL_SHADING;
    D3D12_BARRIER_ACCESS AccessAfter = D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
    D3D12_BARRIER_LAYOUT LayoutAfter = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE;
    uint32_t StartArraySlice = 0;
    uint32_t NumArraySlices = uint32_t(-1);
};

struct DepthBuffer
{
    Texture Texture;
    D3D12_CPU_DESCRIPTOR_HANDLE DSV = { };
    D3D12_CPU_DESCRIPTOR_HANDLE ReadOnlyDSV = { };
    Array<D3D12_CPU_DESCRIPTOR_HANDLE> ArrayDSVs;
    uint32_t MSAASamples = 0;
    uint32_t MSAAQuality = 0;
    DXGI_FORMAT DSVFormat = DXGI_FORMAT_UNKNOWN;

    void Initialize(const DepthBufferInit& init);
    void Shutdown();

    D3D12_TEXTURE_BARRIER DepthWritableBarrier(DepthWritableBarrierDesc desc = DepthWritableBarrierDesc()) const;
    D3D12_TEXTURE_BARRIER DepthReadableBarrier(DepthReadableBarrierDesc desc = DepthReadableBarrierDesc()) const;
    D3D12_TEXTURE_BARRIER ShaderReadableBarrier(DepthShaderReadableBarrierDesc desc = DepthShaderReadableBarrierDesc()) const;

    uint32_t SRV() const { return Texture.SRV; }
    uint32_t Width() const { return Texture.Width; }
    uint32_t Height() const { return Texture.Height; }
    ID3D12Resource* Resource() const { return Texture.Resource; }
};

struct FeedbackTextureInit
{
    const Texture* PairedTexture = nullptr;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
    D3D12_BARRIER_LAYOUT InitialLayout = D3D12_BARRIER_LAYOUT_UNDEFINED;
    D3D12_MIP_REGION MipRegion = { };
    const char* Name = nullptr;
};

struct FeedbackTexture
{
    Texture Texture;
    D3D12_MIP_REGION MipRegion = { };
    DescriptorIndex UAV;

    void Initialize(const FeedbackTextureInit& init);
    void Shutdown();

    uint32_t Width() const { return Texture.Width; }
    uint32_t Height() const { return Texture.Height; }
    DXGI_FORMAT Format() const { return Texture.Format; }
    ID3D12Resource* Resource() const { return Texture.Resource; }
    uint32_t DecodeWidth() const;
    uint32_t DecodeHeight() const;
    uint32_t DecodeBufferSize() const;
};

struct PIXMarker
{
    ID3D12GraphicsCommandList* CmdList = nullptr;

    PIXMarker(ID3D12GraphicsCommandList* cmdList, const char* msg) : CmdList(cmdList)
    {
        PIXBeginEvent(cmdList, 0, msg);
    }

    ~PIXMarker()
    {
        PIXEndEvent(CmdList);
    }
};

}