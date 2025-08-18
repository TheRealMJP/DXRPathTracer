//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "GraphicsTypes.h"
#include "..\\Exceptions.h"
#include "..\\Utility.h"
#include "..\\Serialization.h"
#include "..\\FileIO.h"

namespace SampleFramework12
{

static const D3D12_BARRIER_LAYOUT ShaderResourceQueueLayouts[] =
{
    D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE,      // Direct
    D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_SHADER_RESOURCE,     // Compute
    D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,                   // ComputeAndDirect
};
StaticAssert_(ArraySize_(ShaderResourceQueueLayouts) == NumQueueVisibilities);

static const D3D12_BARRIER_LAYOUT UnorderedAcccessQueueLayouts[] =
{
    D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS,      // Direct
    D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_UNORDERED_ACCESS,     // Compute
    D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS,                   // ComputeAndDirect
};
StaticAssert_(ArraySize_(UnorderedAcccessQueueLayouts) == NumQueueVisibilities);


// == DescriptorHeap ==============================================================================

void DescriptorHeap::Init(uint32_t numPersistent, uint32_t numTemporary, D3D12_DESCRIPTOR_HEAP_TYPE heapType, bool shaderVisible)
{
    Shutdown();

    uint32_t totalNumDescriptors = numPersistent + numTemporary;
    Assert_(totalNumDescriptors > 0);

    NumPersistent = numPersistent;
    NumTemporary = numTemporary;
    HeapType = heapType;
    ShaderVisible = shaderVisible;
    if(heapType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || heapType == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
        ShaderVisible = false;

    NumHeaps = ShaderVisible ? 2 : 1;

    DeadList.Init(numPersistent);
    for(uint32_t i = 0; i < numPersistent; ++i)
        DeadList[i] = uint32_t(i);

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };
    heapDesc.NumDescriptors = uint32_t(totalNumDescriptors);
    heapDesc.Type = heapType;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if(ShaderVisible)
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    for(uint32_t i = 0; i < NumHeaps; ++i)
    {
        DXCall(DX12::Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&Heaps[i])));
        CPUStart[i] = Heaps[i]->GetCPUDescriptorHandleForHeapStart();
        if(ShaderVisible)
            GPUStart[i] = Heaps[i]->GetGPUDescriptorHandleForHeapStart();
    }

    DescriptorSize = DX12::Device->GetDescriptorHandleIncrementSize(heapType);
}

void DescriptorHeap::Shutdown()
{
    Assert_(PersistentAllocated == 0);
    for(uint64_t i = 0; i < ArraySize_(Heaps); ++i)
        DX12::Release(Heaps[i]);
}

PersistentDescriptorAlloc DescriptorHeap::AllocatePersistent(DescriptorIndex index)
{
    Assert_(Heaps[0] != nullptr);

    AcquireSRWLockExclusive(&Lock);

    Assert_(PersistentAllocated < NumPersistent);
    if(PersistentAllocated >= NumPersistent)
        throw Exception(MakeString("Ran out of persistent descriptors in the global descriptor heap (max is %u)", NumPersistent));

    if(index != InvalidDescriptorIndex)
    {
        // Make sure the index is available, and swap it so it's next in the dead list
        for(uint32_t i = PersistentAllocated; i < NumPersistent; ++i)
        {
            if(DeadList[i] == index)
            {
                Swap(DeadList[i], DeadList[PersistentAllocated]);
                break;
            }
        }

        Assert_(DeadList[PersistentAllocated] == index);
    }

    // Use the next one from the dead list
    index = DeadList[PersistentAllocated];
    ++PersistentAllocated;

    ReleaseSRWLockExclusive(&Lock);

    PersistentDescriptorAlloc alloc;
    alloc.Index = index;
    for(uint32_t i = 0; i < NumHeaps; ++i)
    {
        alloc.Handles[i] = CPUStart[i];
        alloc.Handles[i].ptr += index * DescriptorSize;
    }

    return alloc;
}

void DescriptorHeap::FreePersistent(DescriptorIndex& idx)
{
    if(idx == InvalidDescriptorIndex)
        return;

    Assert_(idx < NumPersistent);
    Assert_(Heaps[0] != nullptr);

    AcquireSRWLockExclusive(&Lock);

    Assert_(PersistentAllocated > 0);
    DeadList[PersistentAllocated - 1] = idx;
    --PersistentAllocated;

    ReleaseSRWLockExclusive(&Lock);

    idx = uint32_t(-1);
}

void DescriptorHeap::FreePersistent(D3D12_CPU_DESCRIPTOR_HANDLE& handle)
{
    Assert_(NumHeaps == 1);
    if(handle.ptr != 0)
    {
        DescriptorIndex idx = IndexFromHandle(handle);
        FreePersistent(idx);
        handle = { };
    }
}

void DescriptorHeap::FreePersistent(D3D12_GPU_DESCRIPTOR_HANDLE& handle)
{
    Assert_(NumHeaps == 1);
    if(handle.ptr != 0)
    {
        DescriptorIndex idx = IndexFromHandle(handle);
        FreePersistent(idx);
        handle = { };
    }
}

TempDescriptorAlloc DescriptorHeap::AllocateTemporary(uint32_t count)
{
    Assert_(Heaps[0] != nullptr);
    Assert_(count > 0);

    uint32_t tempIdx = uint32_t(InterlockedAdd64(&TemporaryAllocated, count)) - count;
    Assert_(tempIdx < NumTemporary);

    uint32_t finalIdx = tempIdx + NumPersistent;

    TempDescriptorAlloc alloc;
    alloc.StartCPUHandle = CPUStart[HeapIndex];
    alloc.StartCPUHandle.ptr += finalIdx * DescriptorSize;
    alloc.StartGPUHandle = GPUStart[HeapIndex];
    alloc.StartGPUHandle.ptr += finalIdx * DescriptorSize;
    alloc.StartIndex = finalIdx;

    return alloc;
}

void DescriptorHeap::EndFrame()
{
    Assert_(Heaps[0] != nullptr);
    TemporaryAllocated = 0;
    HeapIndex = (HeapIndex + 1) % NumHeaps;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::CPUHandleFromIndex(uint32_t descriptorIdx) const
{
    return CPUHandleFromIndex(descriptorIdx, HeapIndex);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GPUHandleFromIndex(uint32_t descriptorIdx) const
{
    return GPUHandleFromIndex(descriptorIdx, HeapIndex);
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::CPUHandleFromIndex(uint32_t descriptorIdx, uint64_t heapIdx) const
{
    Assert_(Heaps[0] != nullptr);
    Assert_(heapIdx < NumHeaps);
    Assert_(descriptorIdx < TotalNumDescriptors());
    D3D12_CPU_DESCRIPTOR_HANDLE handle = CPUStart[heapIdx];
    handle.ptr += descriptorIdx * DescriptorSize;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GPUHandleFromIndex(uint32_t descriptorIdx, uint64_t heapIdx) const
{
    Assert_(Heaps[0] != nullptr);
    Assert_(heapIdx < NumHeaps);
    Assert_(descriptorIdx < TotalNumDescriptors());
    Assert_(ShaderVisible);
    D3D12_GPU_DESCRIPTOR_HANDLE handle = GPUStart[heapIdx];
    handle.ptr += descriptorIdx * DescriptorSize;
    return handle;
}

DescriptorIndex DescriptorHeap::IndexFromHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle) const
{
    Assert_(Heaps[0] != nullptr);
    Assert_(handle.ptr >= CPUStart[HeapIndex].ptr);
    Assert_(handle.ptr < CPUStart[HeapIndex].ptr + DescriptorSize * TotalNumDescriptors());
    Assert_((handle.ptr - CPUStart[HeapIndex].ptr) % DescriptorSize == 0);
    return DescriptorIndex(uint32_t((handle.ptr - CPUStart[HeapIndex].ptr) / DescriptorSize));
}

DescriptorIndex DescriptorHeap::IndexFromHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) const
{
    Assert_(Heaps[0] != nullptr);
    Assert_(handle.ptr >= GPUStart[HeapIndex].ptr);
    Assert_(handle.ptr < GPUStart[HeapIndex].ptr + DescriptorSize * TotalNumDescriptors());
    Assert_((handle.ptr - GPUStart[HeapIndex].ptr) % DescriptorSize == 0);
    return DescriptorIndex(uint32_t((handle.ptr - GPUStart[HeapIndex].ptr) / DescriptorSize));
}

ID3D12DescriptorHeap* DescriptorHeap::CurrentHeap() const
{
    Assert_(Heaps[0] != nullptr);
    return Heaps[HeapIndex];
}

// == Buffer ======================================================================================

void Buffer::Initialize(const BufferInit& init)
{
    Assert_(init.Size > 0);
    Assert_(init.Alignment > 0);

    Size = AlignTo(init.Size, init.Alignment);
    Alignment = init.Alignment;
    Dynamic = init.Dynamic;
    CPUAccessible = init.CPUAccessible;
    CurrBuffer = 0;
    CPUAddress = nullptr;
    GPUAddress = 0;
    Heap = nullptr;
    HeapOffset = 0;

    Assert_(init.AllowUAV == false || init.Dynamic == false);
    Assert_(init.Dynamic || init.CPUAccessible == false);
    // Assert_(init.RTAccelStructure == false || init.AllowUAV == false);

    D3D12_RESOURCE_DESC1 resourceDesc = { };
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = init.Dynamic ? Size * DX12::RenderLatency : Size;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.Flags = init.AllowUAV ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Alignment = 0;

    if(init.RTAccelStructure)
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE;

    const D3D12_HEAP_PROPERTIES* heapProps = init.CPUAccessible ? DX12::GetUploadHeapProps() : DX12::GetDefaultHeapProps();

    if(init.Heap)
    {
        Heap = init.Heap;
        HeapOffset = init.HeapOffset;
        DXCall(DX12::Device->CreatePlacedResource2(init.Heap, init.HeapOffset, &resourceDesc, D3D12_BARRIER_LAYOUT_UNDEFINED,
                                                   nullptr, 0, nullptr, IID_PPV_ARGS(&Resource)));
    }
    else
    {
        DXCall(DX12::Device->CreateCommittedResource3(heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                                      D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr, nullptr,
                                                      0, nullptr, IID_PPV_ARGS(&Resource)));
    }

    if(init.Name)
        Resource->SetName(ToWString(init.Name).c_str());

    GPUAddress = Resource->GetGPUVirtualAddress();

    if(init.CPUAccessible)
    {
        D3D12_RANGE readRange = { };
        DXCall(Resource->Map(0, &readRange, reinterpret_cast<void**>(&CPUAddress)));
    }

    if(init.InitData && init.CPUAccessible)
    {
        for(uint64_t i = 0; i < DX12::RenderLatency; ++i)
        {
            uint8_t* dstMem = CPUAddress + Size * i;
            memcpy(dstMem, init.InitData, init.Size);
        }

    }
    else if(init.InitData)
    {
        const uint64_t numBuffers = init.Dynamic ? DX12::RenderLatency : 1;
        for(uint64_t bufferIdx = 0; bufferIdx < numBuffers; ++bufferIdx)
        {
            UploadContext uploadContext = DX12::ResourceUploadBegin(init.Size);

            memcpy(uploadContext.CPUAddress, (uint8_t*)init.InitData, init.Size);

            const uint64_t dstOffset = bufferIdx * init.Size;
            uploadContext.CmdList->CopyBufferRegion(Resource, dstOffset, uploadContext.Resource, uploadContext.ResourceOffset, init.Size);

            DX12::ResourceUploadEnd(uploadContext);
        }
    }
}

void Buffer::Shutdown()
{
    DX12::DeferredRelease(Resource);
}

MapResult Buffer::Map()
{
    Assert_(Initialized());
    Assert_(Dynamic);
    Assert_(CPUAccessible);

    const uint64_t currOffset = CycleBuffer();

    MapResult result;
    result.ResourceOffset = currOffset;
    result.CPUAddress = CPUAddress + currOffset;
    result.GPUAddress = GPUAddress + currOffset;
    result.Resource = Resource;
    return result;
}

MapResult Buffer::MapAndSetData(const void* data, uint64_t dataSize)
{
    Assert_(dataSize <= Size);
    MapResult result = Map();
    memcpy(result.CPUAddress, data, dataSize);
    return result;
}

uint64_t Buffer::QueueUpload(ID3D12Resource* srcResource, uint64_t srcOffset, uint64_t srcSize, uint64_t dstOffset)
{
    Assert_(Dynamic);
    Assert_(CPUAccessible == false);
    Assert_((dstOffset + srcSize) <= Size);

    const uint64_t currOffset = CycleBuffer();

    DX12::QueueFastUpload(srcResource, srcOffset, Resource, currOffset + dstOffset, srcSize);

    return GPUAddress + currOffset;
}

D3D12_BUFFER_BARRIER Buffer::ReadToWriteBarrier(BufferReadToWriteBarrierDesc desc) const
{
    return
    {
        .SyncBefore = desc.SyncBefore,
        .SyncAfter = desc.SyncAfter,
        .AccessBefore = desc.AccessBefore,
        .AccessAfter = desc.AccessAfter,
        .pResource = Resource,
        .Offset = 0,
        .Size = UINT64_MAX,
    };
}

D3D12_BUFFER_BARRIER Buffer::WriteToReadBarrier(BufferWriteToReadBarrierDesc desc) const
{
    return
    {
        .SyncBefore = desc.SyncBefore,
        .SyncAfter = desc.SyncAfter,
        .AccessBefore = desc.AccessBefore,
        .AccessAfter = desc.AccessAfter,
        .pResource = Resource,
        .Offset = 0,
        .Size = UINT64_MAX,
    };
}

D3D12_BUFFER_BARRIER Buffer::WriteToWriteBarrier(BufferWriteToWriteBarrierDesc desc) const
{
    return
    {
        .SyncBefore = desc.SyncBefore,
        .SyncAfter = desc.SyncAfter,
        .AccessBefore = desc.AccessBefore,
        .AccessAfter = desc.AccessAfter,
        .pResource = Resource,
        .Offset = 0,
        .Size = UINT64_MAX,
    };
}

uint64_t Buffer::CycleBuffer()
{
    Assert_(Initialized());
    Assert_(Dynamic);

    // Make sure that we do this at most once per-frame
    Assert_(UploadFrame != DX12::CurrentCPUFrame);
    UploadFrame = DX12::CurrentCPUFrame;

    // Cycle to the next buffer
    CurrBuffer = (CurrBuffer + 1) % DX12::RenderLatency;

    return CurrBuffer * Size;
}

#if UseAsserts_

bool Buffer::ReadyForBinding() const
{
    return Initialized();
}

#endif

// == ConstantBuffer ==============================================================================

void ConstantBuffer::Initialize(const ConstantBufferInit& init)
{
    InternalBuffer.Initialize({
        .Size = init.Size,
        .Alignment = DX12::ConstantBufferAlignment,
        .Dynamic = init.Dynamic,
        .CPUAccessible = init.CPUAccessible,
        .AllowUAV = false,
        .RTAccelStructure = false,
        .InitData = init.InitData,
        .Heap = init.Heap,
        .HeapOffset = init.HeapOffset,
        .Name = init.Name,
    });

    CurrentGPUAddress = InternalBuffer.GPUAddress;
}

void ConstantBuffer::Shutdown()
{
    InternalBuffer.Shutdown();
}

void ConstantBuffer::SetAsGfxRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter) const
{
    Assert_(InternalBuffer.ReadyForBinding());
    cmdList->SetGraphicsRootConstantBufferView(rootParameter, CurrentGPUAddress);
}

void ConstantBuffer::SetAsComputeRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter) const
{
    Assert_(InternalBuffer.ReadyForBinding());
    cmdList->SetComputeRootConstantBufferView(rootParameter, CurrentGPUAddress);
}

void* ConstantBuffer::Map()
{
    MapResult mapResult = InternalBuffer.Map();
    CurrentGPUAddress = mapResult.GPUAddress;
    return mapResult.CPUAddress;
}

void ConstantBuffer::MapAndSetData(const void* data, uint64_t dataSize)
{
    Assert_(dataSize <= InternalBuffer.Size);
    void* cpuAddr = Map();
    memcpy(cpuAddr, data, dataSize);
}

void ConstantBuffer::QueueUpload(ID3D12Resource* srcResource, uint64_t srcOffset, uint64_t srcSize, uint64_t dstOffset)
{
    CurrentGPUAddress = InternalBuffer.QueueUpload(srcResource, srcOffset, srcSize, dstOffset);
}

// == StructuredBuffer ============================================================================

void StructuredBuffer::Initialize(const StructuredBufferInit& init)
{
    Shutdown();

    Assert_(init.Stride > 0);
    Assert_(init.NumElements > 0);
    if(init.ShaderTable)
    {
        Assert_(init.Stride % D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT == 0);
    }

    Stride = init.Stride;
    NumElements = init.NumElements;
    IsShaderTable = init.ShaderTable;

    InternalBuffer.Initialize({
        .Size = Stride * NumElements,
        .Alignment = Stride,
        .Dynamic = init.Dynamic,
        .CPUAccessible = init.CPUAccessible,
        .AllowUAV = init.CreateUAV,
        .RTAccelStructure = false,
        .InitData = init.InitData,
        .Heap = init.Heap,
        .HeapOffset = init.HeapOffset,
        .Name = init.Name,
    });

    GPUAddress = InternalBuffer.GPUAddress;

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    SRV = srvAlloc.Index;

    // Start off all SRV's pointing to the first buffer
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = SRVDesc(0);
    for(uint32_t i = 0; i < ArraySize_(srvAlloc.Handles); ++i)
        DX12::Device->CreateShaderResourceView(InternalBuffer.Resource, &srvDesc, srvAlloc.Handles[i]);

    if(init.CreateUAV)
    {
        Assert_(init.Dynamic == false);

        PersistentDescriptorAlloc uavAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
        UAV = uavAlloc.Index;

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = { };
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        uavDesc.Buffer.NumElements = uint32_t(NumElements);
        uavDesc.Buffer.StructureByteStride = uint32_t(Stride);
        for(uint32_t i = 0; i < ArraySize_(uavAlloc.Handles); ++i)
            DX12::Device->CreateUnorderedAccessView(InternalBuffer.Resource, nullptr, &uavDesc, uavAlloc.Handles[i]);
    }
}

void StructuredBuffer::Shutdown()
{    
    DX12::SRVDescriptorHeap.FreePersistent(SRV);
    DX12::SRVDescriptorHeap.FreePersistent(UAV);
    InternalBuffer.Shutdown();
    Stride = 0;
    NumElements = 0;
}

D3D12_VERTEX_BUFFER_VIEW StructuredBuffer::VBView() const
{
    Assert_(InternalBuffer.ReadyForBinding());
    D3D12_VERTEX_BUFFER_VIEW vbView = { };
    vbView.BufferLocation = GPUAddress;
    vbView.StrideInBytes = uint32_t(Stride);
    vbView.SizeInBytes = uint32_t(InternalBuffer.Size);
    return vbView;
}

D3D12_INDEX_BUFFER_VIEW StructuredBuffer::IBView() const
{
    Assert_(Stride == 2 || Stride == 4);
    D3D12_INDEX_BUFFER_VIEW ibView = { };
    ibView.BufferLocation = GPUAddress;
    ibView.Format = Stride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    ibView.SizeInBytes = uint32_t(InternalBuffer.Size);
    return ibView;
}

D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE StructuredBuffer::ShaderTable(uint64_t startElement, uint64_t numElements) const
{
    Assert_(IsShaderTable);
    Assert_(startElement < NumElements);
    numElements = Min(numElements, NumElements - startElement);

    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE result = { };
    result.StartAddress = GPUAddress + Stride * startElement;
    result.SizeInBytes = numElements * Stride;
    result.StrideInBytes = Stride;

    return result;
}

D3D12_GPU_VIRTUAL_ADDRESS_RANGE StructuredBuffer::ShaderRecord(uint64_t element) const
{
    Assert_(IsShaderTable);
    Assert_(element < NumElements);

    D3D12_GPU_VIRTUAL_ADDRESS_RANGE result = { };
    result.StartAddress = GPUAddress + Stride * element;
    result.SizeInBytes = Stride;

    return result;
}

void* StructuredBuffer::Map()
{
    MapResult mapResult = InternalBuffer.Map();
    GPUAddress = mapResult.GPUAddress;

    UpdateDynamicSRV();

    return mapResult.CPUAddress;
}

void StructuredBuffer::MapAndSetData(const void* data, uint64_t numElements)
{
    Assert_(numElements <= NumElements);
    void* cpuAddr = Map();
    memcpy(cpuAddr, data, numElements * Stride);
}

void StructuredBuffer::QueueUpload(ID3D12Resource* srcResource, uint64_t srcOffset, uint64_t srcNumElements, uint64_t dstElemOffset)
{
    GPUAddress = InternalBuffer.QueueUpload(srcResource, srcOffset, srcNumElements * Stride, dstElemOffset * Stride);

    UpdateDynamicSRV();
}

uint64_t StructuredBuffer::CycleBuffer()
{
    const uint64_t currOffset = InternalBuffer.CycleBuffer();

    GPUAddress = currOffset + InternalBuffer.GPUAddress;

    UpdateDynamicSRV();

    return currOffset;
}

D3D12_SHADER_RESOURCE_VIEW_DESC StructuredBuffer::SRVDesc(uint64_t bufferIdx) const
{
    Assert_(bufferIdx == 0 || InternalBuffer.Dynamic);
    Assert_(bufferIdx < DX12::RenderLatency);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = uint32_t(NumElements * bufferIdx);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Buffer.NumElements = uint32_t(NumElements);
    srvDesc.Buffer.StructureByteStride = uint32_t(Stride);
    return srvDesc;
}

void StructuredBuffer::UpdateDynamicSRV() const
{
    Assert_(InternalBuffer.Dynamic);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = SRVDesc(InternalBuffer.CurrBuffer);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = DX12::SRVDescriptorHeap.CPUHandleFromIndex(SRV, DX12::CurrFrameIdx);
    DX12::Device->CreateShaderResourceView(InternalBuffer.Resource, &srvDesc, handle);

    DX12::DeferredCreateSRV(InternalBuffer.Resource, srvDesc, SRV);
}

// == FormattedBuffer ============================================================================

void FormattedBuffer::Initialize(const FormattedBufferInit& init)
{
    Shutdown();

    Assert_(init.Format != DXGI_FORMAT_UNKNOWN);
    Assert_(init.NumElements > 0);
    Stride = DirectX::BitsPerPixel(init.Format) / 8;
    NumElements = init.NumElements;
    Format = init.Format;

    InternalBuffer.Initialize({
        .Size = Stride * NumElements,
        .Alignment = Stride,
        .Dynamic = init.Dynamic,
        .CPUAccessible = init.CPUAccessible,
        .AllowUAV = init.CreateUAV,
        .RTAccelStructure = false,
        .InitData = init.InitData,
        .Heap = init.Heap,
        .HeapOffset = init.HeapOffset,
        .Name = init.Name,
    });
    GPUAddress = InternalBuffer.GPUAddress;

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    SRV = srvAlloc.Index;

    // Start off all SRV's pointing to the first buffer
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = SRVDesc(0);
    for(uint32_t i = 0; i < ArraySize_(srvAlloc.Handles); ++i)
        DX12::Device->CreateShaderResourceView(InternalBuffer.Resource, &srvDesc, srvAlloc.Handles[i]);

    if(init.CreateUAV)
    {
        Assert_(init.Dynamic == false);

        PersistentDescriptorAlloc uavAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
        UAV = uavAlloc.Index;

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = { };
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Format = Format;
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        uavDesc.Buffer.NumElements = uint32_t(NumElements);

        for(uint32_t i = 0; i < ArraySize_(uavAlloc.Handles); ++i)
            DX12::Device->CreateUnorderedAccessView(InternalBuffer.Resource, nullptr, &uavDesc, uavAlloc.Handles[i]);
    }
}

void FormattedBuffer::Shutdown()
{
    DX12::SRVDescriptorHeap.FreePersistent(SRV);
    DX12::SRVDescriptorHeap.FreePersistent(UAV);
    InternalBuffer.Shutdown();
    Stride = 0;
    NumElements = 0;
}

D3D12_INDEX_BUFFER_VIEW FormattedBuffer::IBView() const
{
    Assert_(Format == DXGI_FORMAT_R16_UINT || Format == DXGI_FORMAT_R32_UINT);
    D3D12_INDEX_BUFFER_VIEW ibView = { };
    ibView.BufferLocation = GPUAddress;
    ibView.Format = Format;
    ibView.SizeInBytes = uint32_t(InternalBuffer.Size);
    return ibView;
}

void* FormattedBuffer::Map()
{
    MapResult mapResult = InternalBuffer.Map();

    GPUAddress = mapResult.GPUAddress;

    UpdateDynamicSRV();

    return mapResult.CPUAddress;
}

void FormattedBuffer::MapAndSetData(const void* data, uint64_t numElements)
{
    Assert_(numElements <= NumElements);
    void* cpuAddr = Map();
    memcpy(cpuAddr, data, numElements * Stride);
}

void FormattedBuffer::QueueUpload(ID3D12Resource* srcResource, uint64_t srcOffset, uint64_t srcNumElements, uint64_t dstElemOffset)
{
    GPUAddress = InternalBuffer.QueueUpload(srcResource, srcOffset, srcNumElements * Stride, dstElemOffset * Stride);

    UpdateDynamicSRV();
}

uint64_t FormattedBuffer::CycleBuffer()
{
    const uint64_t currOffset = InternalBuffer.CycleBuffer();

    GPUAddress = currOffset + InternalBuffer.GPUAddress;

    UpdateDynamicSRV();

    return currOffset;
}

D3D12_SHADER_RESOURCE_VIEW_DESC FormattedBuffer::SRVDesc(uint64_t bufferIdx) const
{
    Assert_(bufferIdx == 0 || InternalBuffer.Dynamic);
    Assert_(bufferIdx < DX12::RenderLatency);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = uint32_t(NumElements * bufferIdx);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Buffer.NumElements = uint32_t(NumElements);
    return srvDesc;
}

void FormattedBuffer::UpdateDynamicSRV() const
{
    Assert_(InternalBuffer.Dynamic);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = SRVDesc(InternalBuffer.CurrBuffer);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = DX12::SRVDescriptorHeap.CPUHandleFromIndex(SRV, DX12::CurrFrameIdx);
    DX12::Device->CreateShaderResourceView(InternalBuffer.Resource, &srvDesc, handle);

    DX12::DeferredCreateSRV(InternalBuffer.Resource, srvDesc, SRV);
}

// == RawBuffer ============================================================================

void RawBuffer::Initialize(const RawBufferInit& init)
{
    Shutdown();

    Assert_(init.NumElements > 0);
    NumElements = init.NumElements;

    InternalBuffer.Initialize({
        .Size = AlignTo(Stride * NumElements, 16),
        .Alignment = Stride,
        .Dynamic = init.Dynamic,
        .CPUAccessible = init.CPUAccessible,
        .AllowUAV = init.CreateUAV,
        .RTAccelStructure = false,
        .InitData = init.InitData,
        .Heap = init.Heap,
        .HeapOffset = init.HeapOffset,
        .Name = init.Name,
    });
    GPUAddress = InternalBuffer.GPUAddress;

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    SRV = srvAlloc.Index;

    // Start off all SRV's pointing to the first buffer
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = SRVDesc(0);
    for(uint32_t i = 0; i < ArraySize_(srvAlloc.Handles); ++i)
        DX12::Device->CreateShaderResourceView(InternalBuffer.Resource, &srvDesc, srvAlloc.Handles[i]);

    if(init.CreateUAV)
    {
        Assert_(init.Dynamic == false);

        PersistentDescriptorAlloc uavAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
        UAV = uavAlloc.Index;

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = { };
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        uavDesc.Buffer.NumElements = uint32_t(NumElements);
        for(uint32_t i = 0; i < ArraySize_(uavAlloc.Handles); ++i)
            DX12::Device->CreateUnorderedAccessView(InternalBuffer.Resource, nullptr, &uavDesc, uavAlloc.Handles[i]);
    }
}

void RawBuffer::Shutdown()
{
    DX12::SRVDescriptorHeap.FreePersistent(SRV);
    DX12::SRVDescriptorHeap.FreePersistent(UAV);
    InternalBuffer.Shutdown();
    NumElements = 0;
}

void* RawBuffer::Map()
{
    MapResult mapResult = InternalBuffer.Map();
    GPUAddress = mapResult.GPUAddress;

    UpdateDynamicSRV();

    return mapResult.CPUAddress;
}

void RawBuffer::MapAndSetData(const void* data, uint64_t numElements)
{
    Assert_(numElements <= NumElements);
    void* cpuAddr = Map();
    memcpy(cpuAddr, data, numElements * Stride);
}

void RawBuffer::QueueUpload(ID3D12Resource* srcResource, uint64_t srcOffset, uint64_t srcNumElements, uint64_t dstElemOffset)
{
    GPUAddress = InternalBuffer.QueueUpload(srcResource, srcOffset, srcNumElements * Stride, dstElemOffset * Stride);

    UpdateDynamicSRV();
}

uint64_t RawBuffer::CycleBuffer()
{
    const uint64_t currOffset = InternalBuffer.CycleBuffer();

    GPUAddress = currOffset + InternalBuffer.GPUAddress;

    UpdateDynamicSRV();

    return currOffset;
}

D3D12_SHADER_RESOURCE_VIEW_DESC RawBuffer::SRVDesc(uint64_t bufferIdx) const
{
    Assert_(bufferIdx == 0 || InternalBuffer.Dynamic);
    Assert_(bufferIdx < DX12::RenderLatency);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = uint32_t(AlignTo(NumElements, 4) * bufferIdx);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
    srvDesc.Buffer.NumElements = uint32_t(NumElements);
    return srvDesc;
}

void RawBuffer::UpdateDynamicSRV() const
{
    Assert_(InternalBuffer.Dynamic);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = SRVDesc(InternalBuffer.CurrBuffer);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = DX12::SRVDescriptorHeap.CPUHandleFromIndex(SRV, DX12::CurrFrameIdx);
    DX12::Device->CreateShaderResourceView(InternalBuffer.Resource, &srvDesc, handle);

    DX12::DeferredCreateSRV(InternalBuffer.Resource, srvDesc, SRV);
}

// == RTAccelStructure ============================================================================

void RTAccelStructure::Initialize(const RTAccelStructureInit& init)
{
    Shutdown();

    Assert_(init.Size > 0);
    Size = init.Size;

    InternalBuffer.Initialize({
        .Size = init.Size,
        .Alignment = 1,
        .Dynamic = false,
        .CPUAccessible = false,
        .AllowUAV = true,
        .RTAccelStructure = true,
        .InitData = nullptr,
        .Heap = init.Heap,
        .HeapOffset = init.HeapOffset,
        .Name = init.Name,
    });
    GPUAddress = InternalBuffer.GPUAddress;

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    SRV = srvAlloc.Index;

    // Initialize the SRV descriptors
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = GPUAddress;

    for(uint32_t i = 0; i < ArraySize_(srvAlloc.Handles); ++i)
        DX12::Device->CreateShaderResourceView(nullptr, &srvDesc, srvAlloc.Handles[i]);
}

void RTAccelStructure::Shutdown()
{
    DX12::SRVDescriptorHeap.FreePersistent(SRV);
    InternalBuffer.Shutdown();
    Size = 0;
}

D3D12_BUFFER_BARRIER RTAccelStructure::TopLevelPostBuildBarrier() const {
    return
    {
        .SyncBefore = D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
        .SyncAfter = D3D12_BARRIER_SYNC_ALL_SHADING,
        .AccessBefore = D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE,
        .AccessAfter = D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,
        .pResource = InternalBuffer.Resource,
        .Offset = 0,
        .Size = UINT64_MAX,
    };
}

D3D12_BUFFER_BARRIER RTAccelStructure::BottomLevelPostBuildBarrier() const {
    return
    {
        .SyncBefore = D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
        .SyncAfter = D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
        .AccessBefore = D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE,
        .AccessAfter = D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,
        .pResource = InternalBuffer.Resource,
        .Offset = 0,
        .Size = UINT64_MAX,
    };
}

// == ReadbackBuffer ==============================================================================

void ReadbackBuffer::Initialize(uint64_t size)
{
    Assert_(size > 0);
    Size = size;

    D3D12_RESOURCE_DESC1 resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = uint32_t(size);
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Alignment = 0;

    DXCall(DX12::Device->CreateCommittedResource3(DX12::GetReadbackHeapProps(), D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                                 D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr, nullptr, 0, nullptr, IID_PPV_ARGS(&Resource)));
}

void ReadbackBuffer::Shutdown()
{
    DX12::DeferredRelease(Resource);
    Size = 0;
}

const void* ReadbackBuffer::Map() const
{
    Assert_(Resource != nullptr);
    void* data = nullptr;
    Resource->Map(0, nullptr, &data);
    return data;
}

void ReadbackBuffer::Unmap() const
{
    Assert_(Resource != nullptr);
    Resource->Unmap(0, nullptr);
}

// == Fence =======================================================================================

void Fence::Init(uint64_t initialValue)
{
    DXCall(DX12::Device->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&D3DFence)));
    FenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
    Win32Call(FenceEvent != 0);
}

void Fence::Shutdown()
{
    DX12::DeferredRelease(D3DFence);
}

void Fence::Signal(ID3D12CommandQueue* queue, uint64_t fenceValue)
{
    Assert_(D3DFence != nullptr);
    DXCall(queue->Signal(D3DFence, fenceValue));
}

void Fence::Wait(uint64_t fenceValue)
{
    Assert_(D3DFence != nullptr);
    if(D3DFence->GetCompletedValue() < fenceValue)
    {
        DXCall(D3DFence->SetEventOnCompletion(fenceValue, FenceEvent));
        WaitForSingleObject(FenceEvent, INFINITE);
    }
}


bool Fence::Signaled(uint64_t fenceValue)
{
    Assert_(D3DFence != nullptr);
    return D3DFence->GetCompletedValue() >= fenceValue;
}

void Fence::Clear(uint64_t fenceValue)
{
    Assert_(D3DFence != nullptr);
    D3DFence->Signal(fenceValue);
}

// == Texture ====================================================================================

void Texture::Shutdown()
{
    DX12::SRVDescriptorHeap.FreePersistent(SRV);
    DX12::DeferredRelease(Resource);
}

D3D12_BARRIER_SUBRESOURCE_RANGE Texture::BarrierRange(uint32_t startMipLevel, uint32_t numMipLevels, uint32_t startArraySlice, uint32_t numArraySlices) const
{
    Assert_(startMipLevel < NumMips);
    Assert_(startArraySlice < ArraySize);
    return
    {
        .IndexOrFirstMipLevel = startMipLevel,
        .NumMipLevels = Min(numMipLevels, NumMips - startMipLevel),
        .FirstArraySlice = startArraySlice,
        .NumArraySlices = Min(numArraySlices, ArraySize - startArraySlice),
        .FirstPlane = 0,
        .NumPlanes = 1,
    };
}

// == RenderTexture ===============================================================================

void RenderTexture::Initialize(const RenderTextureInit& init)
{
    Shutdown();

    Assert_(init.Width > 0);
    Assert_(init.Height > 0);
    Assert_(init.MSAASamples > 0);
    Assert_(init.CreateUAV || init.CreateRTV);
    Assert_(init.NumMips > 0);

    D3D12_RESOURCE_DESC1 textureDesc = { };
    textureDesc.MipLevels = uint16_t(init.NumMips);
    textureDesc.Format = init.Format;
    textureDesc.Width = uint32_t(init.Width);
    textureDesc.Height = uint32_t(init.Height);
    if(init.CreateRTV)
        textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if(init.CreateUAV)
        textureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    textureDesc.DepthOrArraySize = uint16_t(init.ArraySize);
    textureDesc.SampleDesc.Count = uint32_t(init.MSAASamples);
    textureDesc.SampleDesc.Quality = init.MSAASamples > 1 ? DX12::StandardMSAAPattern : 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Alignment = 0;

    D3D12_CLEAR_VALUE clearValue = { };
    clearValue.Format = init.Format;

    DXCall(DX12::Device->CreateCommittedResource3(DX12::GetDefaultHeapProps(), D3D12_HEAP_FLAG_NONE, &textureDesc,
                                                  init.InitialLayout, init.CreateRTV ? &clearValue : nullptr, nullptr, 0, nullptr,
                                                  IID_PPV_ARGS(&Texture.Resource)));


    if(init.Name != nullptr)
        Texture.Resource->SetName(ToWString(init.Name).c_str());

    const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDescPtr = nullptr;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
    if(init.CubeMap)
    {
        Assert_(init.ArraySize == 6);
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = uint32_t(-1);
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        srvDescPtr = &srvDesc;
    }

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    Texture.SRV = srvAlloc.Index;
    for(uint32_t i = 0; i < DX12::SRVDescriptorHeap.NumHeaps; ++i)
        DX12::Device->CreateShaderResourceView(Texture.Resource, srvDescPtr, srvAlloc.Handles[i]);

    Texture.Width = init.Width;
    Texture.Height = init.Height;
    Texture.Depth = 1;
    Texture.NumMips = init.NumMips;
    Texture.ArraySize = init.ArraySize;
    Texture.Format = init.Format;
    Texture.Cubemap = init.CubeMap;
    MSAASamples = uint32_t(init.MSAASamples);
    MSAAQuality = uint32_t(textureDesc.SampleDesc.Quality);
    HasRTV = init.CreateRTV;

    if(init.CreateRTV)
    {
        RTV = DX12::RTVDescriptorHeap.AllocatePersistent().Handles[0];
        DX12::Device->CreateRenderTargetView(Texture.Resource, nullptr, RTV);
    }

    if(init.ArraySize > 1 && init.CreateRTV)
    {
        ArrayRTVs.Init(init.ArraySize);

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = { };
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Format = init.Format;
        if(init.MSAASamples > 1)
            rtvDesc.Texture2DMSArray.ArraySize = 1;
        else
            rtvDesc.Texture2DArray.ArraySize = 1;

        for(uint64_t i = 0; i < init.ArraySize; ++i)
        {
            if(init.MSAASamples > 1)
                rtvDesc.Texture2DMSArray.FirstArraySlice = uint32_t(i);
            else
                rtvDesc.Texture2DArray.FirstArraySlice = uint32_t(i);

            ArrayRTVs[i] = DX12::RTVDescriptorHeap.AllocatePersistent().Handles[0];
            DX12::Device->CreateRenderTargetView(Texture.Resource, &rtvDesc, ArrayRTVs[i]);
        }
    }

    if(init.CreateUAV)
    {
        PersistentDescriptorAlloc uavAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
        UAV = uavAlloc.Index;
        for(uint32_t i = 0; i < ArraySize_(uavAlloc.Handles); ++i)
            DX12::Device->CreateUnorderedAccessView(Texture.Resource, nullptr, nullptr, uavAlloc.Handles[i]);

        if(init.NumMips > 1)
        {
            MipLevelUAVs.Init(init.NumMips);
            for(uint32_t mipLevel = 0; mipLevel < init.NumMips; ++mipLevel)
            {
                PersistentDescriptorAlloc mipUAVAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
                MipLevelUAVs[mipLevel] = mipUAVAlloc.Index;

                D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                uavDesc.Format = init.Format;
                if(init.ArraySize > 1)
                {
                    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                    uavDesc.Texture2DArray.MipSlice = mipLevel;
                    uavDesc.Texture2DArray.FirstArraySlice = 0;
                    uavDesc.Texture2DArray.ArraySize = init.ArraySize;
                }
                else
                {
                    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                    uavDesc.Texture2D.MipSlice = mipLevel;
                }
                for(uint32_t i = 0; i < ArraySize_(mipUAVAlloc.Handles); ++i)
                    DX12::Device->CreateUnorderedAccessView(Texture.Resource, nullptr, &uavDesc, mipUAVAlloc.Handles[i]);
            }
        }
    }
}

void RenderTexture::Shutdown()
{
    DX12::RTVDescriptorHeap.FreePersistent(RTV);
    DX12::SRVDescriptorHeap.FreePersistent(UAV);
    for(uint64_t i = 0; i < ArrayRTVs.Size(); ++i)
        DX12::RTVDescriptorHeap.FreePersistent(ArrayRTVs[i]);
    for(uint64_t i = 0; i < MipLevelUAVs.Size(); ++i)
        DX12::SRVDescriptorHeap.FreePersistent(MipLevelUAVs[i]);
    ArrayRTVs.Shutdown();
    Texture.Shutdown();
}

D3D12_TEXTURE_BARRIER RenderTexture::RTWritableBarrier(RTWritableBarrierDesc desc) const
{
    Assert_(Texture.Resource != nullptr);
    Assert_(HasRTV);
    Assert_(desc.QueueVisibilityAfter == Direct);

    return
    {
        .SyncBefore = desc.FirstAccess ? D3D12_BARRIER_SYNC_NONE : desc.SyncBefore,
        .SyncAfter = D3D12_BARRIER_SYNC_RENDER_TARGET,
        .AccessBefore = (desc.FirstAccess || desc.Discard) ? D3D12_BARRIER_ACCESS_NO_ACCESS : desc.AccessBefore,
        .AccessAfter = D3D12_BARRIER_ACCESS_RENDER_TARGET,
        .LayoutBefore = (desc.FirstAccess || desc.Discard) ? D3D12_BARRIER_LAYOUT_UNDEFINED : desc.LayoutBefore,
        .LayoutAfter = D3D12_BARRIER_LAYOUT_RENDER_TARGET,
        .pResource = Texture.Resource,
        .Subresources = Texture.BarrierRange(desc.StartMipLevel, desc.NumMipLevels, desc.StartArraySlice, desc.NumArraySlices),
        .Flags = desc.Discard ? D3D12_TEXTURE_BARRIER_FLAG_DISCARD : D3D12_TEXTURE_BARRIER_FLAG_NONE,
    };
}

D3D12_TEXTURE_BARRIER RenderTexture::UAVWritableBarrier(RTWritableBarrierDesc desc) const
{
    Assert_(Texture.Resource != nullptr);
    Assert_(UAV != uint32_t(-1));
    Assert_(desc.QueueVisibilityAfter < NumQueueVisibilities);

    return
    {
        .SyncBefore = desc.FirstAccess ? D3D12_BARRIER_SYNC_NONE : desc.SyncBefore,
        .SyncAfter = D3D12_BARRIER_SYNC_ALL_SHADING,
        .AccessBefore = (desc.FirstAccess || desc.Discard) ? D3D12_BARRIER_ACCESS_NO_ACCESS : desc.AccessBefore,
        .AccessAfter = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
        .LayoutBefore = (desc.FirstAccess || desc.Discard) ? D3D12_BARRIER_LAYOUT_UNDEFINED : desc.LayoutBefore,
        .LayoutAfter = UnorderedAcccessQueueLayouts[desc.QueueVisibilityAfter],
        .pResource = Texture.Resource,
        .Subresources = Texture.BarrierRange(desc.StartMipLevel, desc.NumMipLevels, desc.StartArraySlice, desc.NumArraySlices),
        .Flags = desc.Discard ? D3D12_TEXTURE_BARRIER_FLAG_DISCARD : D3D12_TEXTURE_BARRIER_FLAG_NONE,
    };
}

D3D12_TEXTURE_BARRIER RenderTexture::RTToShaderReadableBarrier(RTReadableBarrierDesc desc) const
{
    Assert_(Texture.Resource != nullptr);
    Assert_(HasRTV);
    Assert_(desc.QueueVisibilityBefore == Direct);
    Assert_(desc.QueueVisibilityAfter < NumQueueVisibilities);

    return
    {
        .SyncBefore = D3D12_BARRIER_SYNC_RENDER_TARGET,
        .SyncAfter = desc.SyncAfter,
        .AccessBefore = D3D12_BARRIER_ACCESS_RENDER_TARGET,
        .AccessAfter = D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
        .LayoutBefore = D3D12_BARRIER_LAYOUT_RENDER_TARGET,
        .LayoutAfter = ShaderResourceQueueLayouts[desc.QueueVisibilityAfter],
        .pResource = Texture.Resource,
        .Subresources = Texture.BarrierRange(desc.StartMipLevel, desc.NumMipLevels, desc.StartArraySlice, desc.NumArraySlices),
        .Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE,
    };
}

D3D12_TEXTURE_BARRIER RenderTexture::UAVToShaderReadableBarrier(RTReadableBarrierDesc desc) const
{
    Assert_(Texture.Resource != nullptr);
    Assert_(UAV != uint32_t(-1));
    Assert_(desc.QueueVisibilityBefore < NumQueueVisibilities);
    Assert_(desc.QueueVisibilityAfter < NumQueueVisibilities);

    return
    {
        .SyncBefore = D3D12_BARRIER_SYNC_ALL_SHADING,
        .SyncAfter = desc.SyncAfter,
        .AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
        .AccessAfter = D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
        .LayoutBefore = UnorderedAcccessQueueLayouts[desc.QueueVisibilityBefore],
        .LayoutAfter = ShaderResourceQueueLayouts[desc.QueueVisibilityAfter],
        .pResource = Texture.Resource,
        .Subresources = Texture.BarrierRange(desc.StartMipLevel, desc.NumMipLevels, desc.StartArraySlice, desc.NumArraySlices),
        .Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE,
    };
}

D3D12_TEXTURE_BARRIER RenderTexture::MemoryBarrier(RTMemoryBarrierDesc desc) const
{
    Assert_(Texture.Resource != nullptr);
    Assert_(UAV != uint32_t(-1));

    return
    {
        .SyncBefore = desc.SyncBefore,
        .SyncAfter = desc.SyncAfter,
        .AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
        .AccessAfter = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
        .LayoutBefore = desc.Layout,
        .LayoutAfter = desc.Layout,
        .pResource = Texture.Resource,
        .Subresources = Texture.BarrierRange(desc.StartMipLevel, desc.NumMipLevels, desc.StartArraySlice, desc.NumArraySlices),
        .Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE,
    };
}

// == VolumeTexture ===============================================================================

void VolumeTexture::Initialize(const VolumeTextureInit& init)
{
    Shutdown();

    Assert_(init.Width > 0);
    Assert_(init.Height > 0);
    Assert_(init.Depth > 0);

    D3D12_RESOURCE_DESC1 textureDesc = { };
    textureDesc.MipLevels = 1;
    textureDesc.Format = init.Format;
    textureDesc.Width = init.Width;
    textureDesc.Height = init.Height;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    textureDesc.DepthOrArraySize = uint16_t(init.Depth);
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Alignment = 0;

    DXCall(DX12::Device->CreateCommittedResource3(DX12::GetDefaultHeapProps(), D3D12_HEAP_FLAG_NONE, &textureDesc,
                                                 init.InitialLayout, nullptr, nullptr, 0, nullptr, IID_PPV_ARGS(&Texture.Resource)));

    if(init.Name != nullptr)
        Texture.Resource->SetName(ToWString(init.Name).c_str());

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    Texture.SRV = srvAlloc.Index;
    for(uint32_t i = 0; i < DX12::SRVDescriptorHeap.NumHeaps; ++i)
        DX12::Device->CreateShaderResourceView(Texture.Resource, nullptr, srvAlloc.Handles[i]);

    Texture.Width = init.Width;
    Texture.Height = init.Height;
    Texture.Depth = init.Depth;
    Texture.NumMips = 1;
    Texture.ArraySize = 1;
    Texture.Format = init.Format;
    Texture.Cubemap = false;

    PersistentDescriptorAlloc uavAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    UAV = uavAlloc.Index;

    for(uint32_t i = 0; i < ArraySize_(uavAlloc.Handles); ++i)
        DX12::Device->CreateUnorderedAccessView(Texture.Resource, nullptr, nullptr, uavAlloc.Handles[i]);
}

void VolumeTexture::Shutdown()
{
    DX12::SRVDescriptorHeap.FreePersistent(UAV);
    Texture.Shutdown();
}

// == DepthBuffer ===============================================================================

void DepthBuffer::Initialize(const DepthBufferInit& init)
{
    Shutdown();

    Assert_(init.Width > 0);
    Assert_(init.Height > 0);
    Assert_(init.MSAASamples > 0);

    DXGI_FORMAT srvFormat = init.Format;
    if(init.Format == DXGI_FORMAT_D16_UNORM)
    {
        srvFormat = DXGI_FORMAT_R16_UNORM;
    }
    else if(init.Format == DXGI_FORMAT_D24_UNORM_S8_UINT)
    {
        srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    }
    else if(init.Format == DXGI_FORMAT_D32_FLOAT)
    {
        srvFormat = DXGI_FORMAT_R32_FLOAT;
    }
    else if(init.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
    {
        srvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    }
    else
    {
        AssertFail_("Invalid depth buffer format!");
    }

    D3D12_RESOURCE_DESC1 textureDesc = { };
    textureDesc.MipLevels = 1;
    textureDesc.Format = init.Format;
    textureDesc.Width = init.Width;
    textureDesc.Height = init.Height;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    textureDesc.DepthOrArraySize = uint16_t(init.ArraySize);
    textureDesc.SampleDesc.Count = init.MSAASamples;
    textureDesc.SampleDesc.Quality = init.MSAASamples> 1 ? DX12::StandardMSAAPattern : 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Alignment = 0;

    D3D12_CLEAR_VALUE clearValue = { };
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;
    clearValue.Format = init.Format;

    DXCall(DX12::Device->CreateCommittedResource3(DX12::GetDefaultHeapProps(), D3D12_HEAP_FLAG_NONE, &textureDesc,
                                                 init.InitialLayout, &clearValue, nullptr, 0, nullptr,
                                                 IID_PPV_ARGS(&Texture.Resource)));

    if(init.Name != nullptr)
        Texture.Resource->SetName(ToWString(init.Name).c_str());

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    Texture.SRV = srvAlloc.Index;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
    srvDesc.Format = srvFormat;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    if(init.MSAASamples == 1 && init.ArraySize == 1)
    {
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    }
    else if(init.MSAASamples == 1 && init.ArraySize > 1)
    {
        srvDesc.Texture2DArray.ArraySize = uint32_t(init.ArraySize);
        srvDesc.Texture2DArray.FirstArraySlice = 0;
        srvDesc.Texture2DArray.MipLevels = 1;
        srvDesc.Texture2DArray.MostDetailedMip = 0;
        srvDesc.Texture2DArray.PlaneSlice = 0;
        srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    }
    else if(init.MSAASamples > 1 && init.ArraySize == 1)
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    }
    else if(init.MSAASamples > 1 && init.ArraySize > 1)
    {
        srvDesc.Texture2DMSArray.FirstArraySlice = 0;
        srvDesc.Texture2DMSArray.ArraySize = uint32_t(init.ArraySize);
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
    }

    for(uint32_t i = 0; i < DX12::SRVDescriptorHeap.NumHeaps; ++i)
        DX12::Device->CreateShaderResourceView(Texture.Resource, &srvDesc, srvAlloc.Handles[i]);

    Texture.Width = uint32_t(init.Width);
    Texture.Height = uint32_t(init.Height);
    Texture.Depth = 1;
    Texture.NumMips = 1;
    Texture.ArraySize = uint32_t(init.ArraySize);
    Texture.Format = srvFormat;
    Texture.Cubemap = false;
    MSAASamples = uint32_t(init.MSAASamples);
    MSAAQuality = uint32_t(textureDesc.SampleDesc.Quality);

    DSV = DX12::DSVDescriptorHeap.AllocatePersistent().Handles[0];

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = { };
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.Format = init.Format;

    if(init.MSAASamples == 1 && init.ArraySize == 1)
    {
        dsvDesc.Texture2D.MipSlice = 0;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    }
    else if(init.MSAASamples == 1 && init.ArraySize > 1)
    {
        dsvDesc.Texture2DArray.ArraySize = uint32_t(init.ArraySize);
        dsvDesc.Texture2DArray.FirstArraySlice = 0;
        dsvDesc.Texture2DArray.MipSlice = 0;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
    }
    else if(init.MSAASamples > 1 && init.ArraySize == 1)
    {
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
    }
    else if(init.MSAASamples > 1 && init.ArraySize > 1)
    {
        dsvDesc.Texture2DMSArray.ArraySize = uint32_t(init.ArraySize);
        dsvDesc.Texture2DMSArray.FirstArraySlice = 0;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
    }

    DX12::Device->CreateDepthStencilView(Texture.Resource, &dsvDesc, DSV);

    bool hasStencil = init.Format == DXGI_FORMAT_D24_UNORM_S8_UINT || init.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

    ReadOnlyDSV = DX12::DSVDescriptorHeap.AllocatePersistent().Handles[0];
    dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
    if(hasStencil)
        dsvDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
    DX12::Device->CreateDepthStencilView(Texture.Resource, &dsvDesc, ReadOnlyDSV);

    if(init.ArraySize > 1)
    {
        ArrayDSVs.Init(init.ArraySize);

        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        if(init.MSAASamples > 1)
            dsvDesc.Texture2DMSArray.ArraySize = 1;
        else
            dsvDesc.Texture2DArray.ArraySize = 1;

        for(uint64_t i = 0; i < init.ArraySize; ++i)
        {
            if(init.MSAASamples > 1)
                dsvDesc.Texture2DMSArray.FirstArraySlice = uint32_t(i);
            else
                dsvDesc.Texture2DArray.FirstArraySlice = uint32_t(i);

            ArrayDSVs[i] = DX12::DSVDescriptorHeap.AllocatePersistent().Handles[0];
            DX12::Device->CreateDepthStencilView(Texture.Resource, &dsvDesc, ArrayDSVs[i]);
        }
    }

    DSVFormat = init.Format;
}

void DepthBuffer::Shutdown()
{
    DX12::DSVDescriptorHeap.FreePersistent(DSV);
    DX12::DSVDescriptorHeap.FreePersistent(ReadOnlyDSV);
    for(uint64_t i = 0; i < ArrayDSVs.Size(); ++i)
        DX12::DSVDescriptorHeap.FreePersistent(ArrayDSVs[i]);
    ArrayDSVs.Shutdown();
    Texture.Shutdown();
    DSVFormat = DXGI_FORMAT_UNKNOWN;
}

D3D12_TEXTURE_BARRIER DepthBuffer::DepthWritableBarrier(DepthWritableBarrierDesc desc) const
{
    Assert_(Texture.Resource != nullptr);

    return
    {
        .SyncBefore = desc.FirstAccess ? D3D12_BARRIER_SYNC_NONE : desc.SyncBefore,
        .SyncAfter = D3D12_BARRIER_SYNC_DEPTH_STENCIL,
        .AccessBefore = (desc.FirstAccess || desc.Discard) ? D3D12_BARRIER_ACCESS_NO_ACCESS : desc.AccessBefore,
        .AccessAfter = D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE,
        .LayoutBefore = (desc.FirstAccess || desc.Discard) ? D3D12_BARRIER_LAYOUT_UNDEFINED : desc.LayoutBefore,
        .LayoutAfter = D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE,
        .pResource = Texture.Resource,
        .Subresources = Texture.BarrierRange(0, 1, desc.StartArraySlice, desc.NumArraySlices),
        .Flags = desc.Discard ? D3D12_TEXTURE_BARRIER_FLAG_DISCARD : D3D12_TEXTURE_BARRIER_FLAG_NONE,
    };
}

D3D12_TEXTURE_BARRIER DepthBuffer::DepthReadableBarrier(DepthReadableBarrierDesc desc) const
{
    Assert_(Texture.Resource != nullptr);

    return
    {
        .SyncBefore = D3D12_BARRIER_SYNC_DEPTH_STENCIL,
        .SyncAfter = desc.SyncAfter,
        .AccessBefore = D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE,
        .AccessAfter = desc.AccessAfter,
        .LayoutBefore = D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE,
        .LayoutAfter = desc.LayoutAfter,
        .pResource = Texture.Resource,
        .Subresources = Texture.BarrierRange(0, 1, desc.StartArraySlice, desc.NumArraySlices),
        .Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE,
    };
}

D3D12_TEXTURE_BARRIER DepthBuffer::ShaderReadableBarrier(DepthShaderReadableBarrierDesc desc) const
{
    Assert_(Texture.Resource != nullptr);

    return
    {
        .SyncBefore = D3D12_BARRIER_SYNC_DEPTH_STENCIL,
        .SyncAfter = desc.SyncAfter,
        .AccessBefore = D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE,
        .AccessAfter = desc.AccessAfter,
        .LayoutBefore = D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE,
        .LayoutAfter = desc.LayoutAfter,
        .pResource = Texture.Resource,
        .Subresources = Texture.BarrierRange(0, 1, desc.StartArraySlice, desc.NumArraySlices),
        .Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE,
    };
}

// == FeedbackTexture ===============================================================================

void FeedbackTexture::Initialize(const FeedbackTextureInit& init)
{
    Shutdown();

    Assert_(init.Format == DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE || init.Format == DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE);
    Assert_(init.PairedTexture != nullptr);
    Assert_(init.PairedTexture->Resource != nullptr);

    MipRegion = init.MipRegion;

    D3D12_RESOURCE_DESC1 textureDesc = { };
    textureDesc.MipLevels = uint16_t(init.PairedTexture->NumMips);
    textureDesc.Format = init.Format;
    textureDesc.Width = init.PairedTexture->Width;
    textureDesc.Height = init.PairedTexture->Height;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    textureDesc.DepthOrArraySize = uint16_t(init.PairedTexture->ArraySize);
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Alignment = 0;
    textureDesc.SamplerFeedbackMipRegion = init.MipRegion;

    DXCall(DX12::Device->CreateCommittedResource3(DX12::GetDefaultHeapProps(), D3D12_HEAP_FLAG_NONE, &textureDesc,
                                                  init.InitialLayout, nullptr, nullptr, 0, nullptr, IID_PPV_ARGS(&Texture.Resource)));

    if(init.Name != nullptr)
        Texture.Resource->SetName(ToWString(init.Name).c_str());

    Texture.Width = init.PairedTexture->Width;
    Texture.Height = init.PairedTexture->Height;
    Texture.Depth = 1;
    Texture.NumMips = 1;
    Texture.ArraySize = init.PairedTexture->ArraySize;
    Texture.Format = init.Format;
    Texture.Cubemap = false;

    PersistentDescriptorAlloc uavAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    UAV = uavAlloc.Index;

    for(uint32_t i = 0; i < ArraySize_(uavAlloc.Handles); ++i)
        DX12::Device->CreateSamplerFeedbackUnorderedAccessView(init.PairedTexture->Resource, Texture.Resource, uavAlloc.Handles[i]);
}

void FeedbackTexture::Shutdown()
{
    DX12::SRVDescriptorHeap.FreePersistent(UAV);
    Texture.Shutdown();
}

uint32_t FeedbackTexture::DecodeWidth() const
{
    Assert_(Texture.Resource != nullptr);
    return AlignTo(Texture.Width, MipRegion.Width) / MipRegion.Width;
}

uint32_t FeedbackTexture::DecodeHeight() const
{
    Assert_(Texture.Resource != nullptr);
    return AlignTo(Texture.Height, MipRegion.Height) / MipRegion.Height;
}

uint32_t FeedbackTexture::DecodeBufferSize() const
{
    Assert_(Texture.Resource != nullptr);
    Assert_(Texture.ArraySize == 1);
    return DecodeWidth() * DecodeHeight();
}


}