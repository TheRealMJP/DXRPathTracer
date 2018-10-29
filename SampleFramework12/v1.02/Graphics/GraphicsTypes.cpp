//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
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

// == DescriptorHeap ==============================================================================

DescriptorHeap::~DescriptorHeap()
{
    Assert_(Heaps[0] == nullptr);
}

void DescriptorHeap::Init(uint32 numPersistent, uint32 numTemporary, D3D12_DESCRIPTOR_HEAP_TYPE heapType, bool shaderVisible)
{
    Shutdown();

    uint32 totalNumDescriptors = numPersistent + numTemporary;
    Assert_(totalNumDescriptors > 0);

    NumPersistent = numPersistent;
    NumTemporary = numTemporary;
    HeapType = heapType;
    ShaderVisible = shaderVisible;
    if(heapType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || heapType == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
        ShaderVisible = false;

    NumHeaps = ShaderVisible ? 2 : 1;

    DeadList.Init(numPersistent);
    for(uint32 i = 0; i < numPersistent; ++i)
        DeadList[i] = uint32(i);

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };
    heapDesc.NumDescriptors = uint32(totalNumDescriptors);
    heapDesc.Type = heapType;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if(ShaderVisible)
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    for(uint32 i = 0; i < NumHeaps; ++i)
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
    for(uint64 i = 0; i < ArraySize_(Heaps); ++i)
        DX12::Release(Heaps[i]);
}

PersistentDescriptorAlloc DescriptorHeap::AllocatePersistent()
{
    Assert_(Heaps[0] != nullptr);

    AcquireSRWLockExclusive(&Lock);

    Assert_(PersistentAllocated < NumPersistent);
    uint32 idx = DeadList[PersistentAllocated];
    ++PersistentAllocated;

    ReleaseSRWLockExclusive(&Lock);

    PersistentDescriptorAlloc alloc;
    alloc.Index = idx;
    for(uint32 i = 0; i < NumHeaps; ++i)
    {
        alloc.Handles[i] = CPUStart[i];
        alloc.Handles[i].ptr += idx * DescriptorSize;
    }

    return alloc;
}

void DescriptorHeap::FreePersistent(uint32& idx)
{
    if(idx == uint32(-1))
        return;

    Assert_(idx < NumPersistent);
    Assert_(Heaps[0] != nullptr);

    AcquireSRWLockExclusive(&Lock);

    Assert_(PersistentAllocated > 0);
    DeadList[PersistentAllocated - 1] = idx;
    --PersistentAllocated;

    ReleaseSRWLockExclusive(&Lock);

    idx = uint32(-1);
}

void DescriptorHeap::FreePersistent(D3D12_CPU_DESCRIPTOR_HANDLE& handle)
{
    Assert_(NumHeaps == 1);
    if(handle.ptr != 0)
    {
        uint32 idx = IndexFromHandle(handle);
        FreePersistent(idx);
        handle = { };
    }
}

void DescriptorHeap::FreePersistent(D3D12_GPU_DESCRIPTOR_HANDLE& handle)
{
    Assert_(NumHeaps == 1);
    if(handle.ptr != 0)
    {
        uint32 idx = IndexFromHandle(handle);
        FreePersistent(idx);
        handle = { };
    }
}

TempDescriptorAlloc DescriptorHeap::AllocateTemporary(uint32 count)
{
    Assert_(Heaps[0] != nullptr);
    Assert_(count > 0);

    uint32 tempIdx = uint32(InterlockedAdd64(&TemporaryAllocated, count)) - count;
    Assert_(tempIdx < NumTemporary);

    uint32 finalIdx = tempIdx + NumPersistent;

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

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::CPUHandleFromIndex(uint32 descriptorIdx) const
{
    return CPUHandleFromIndex(descriptorIdx, HeapIndex);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GPUHandleFromIndex(uint32 descriptorIdx) const
{
    return GPUHandleFromIndex(descriptorIdx, HeapIndex);
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::CPUHandleFromIndex(uint32 descriptorIdx, uint64 heapIdx) const
{
    Assert_(Heaps[0] != nullptr);
    Assert_(heapIdx < NumHeaps);
    Assert_(descriptorIdx < TotalNumDescriptors());
    D3D12_CPU_DESCRIPTOR_HANDLE handle = CPUStart[heapIdx];
    handle.ptr += descriptorIdx * DescriptorSize;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GPUHandleFromIndex(uint32 descriptorIdx, uint64 heapIdx) const
{
    Assert_(Heaps[0] != nullptr);
    Assert_(heapIdx < NumHeaps);
    Assert_(descriptorIdx < TotalNumDescriptors());
    Assert_(ShaderVisible);
    D3D12_GPU_DESCRIPTOR_HANDLE handle = GPUStart[heapIdx];
    handle.ptr += descriptorIdx * DescriptorSize;
    return handle;
}

uint32 DescriptorHeap::IndexFromHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    Assert_(Heaps[0] != nullptr);
    Assert_(handle.ptr >= CPUStart[HeapIndex].ptr);
    Assert_(handle.ptr < CPUStart[HeapIndex].ptr + DescriptorSize * TotalNumDescriptors());
    Assert_((handle.ptr - CPUStart[HeapIndex].ptr) % DescriptorSize == 0);
    return uint32(handle.ptr - CPUStart[HeapIndex].ptr) / DescriptorSize;
}

uint32 DescriptorHeap::IndexFromHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle)
{
    Assert_(Heaps[0] != nullptr);
    Assert_(handle.ptr >= GPUStart[HeapIndex].ptr);
    Assert_(handle.ptr < GPUStart[HeapIndex].ptr + DescriptorSize * TotalNumDescriptors());
    Assert_((handle.ptr - GPUStart[HeapIndex].ptr) % DescriptorSize == 0);
    return uint32(handle.ptr - GPUStart[HeapIndex].ptr) / DescriptorSize;
}

ID3D12DescriptorHeap* DescriptorHeap::CurrentHeap() const
{
    Assert_(Heaps[0] != nullptr);
    return Heaps[HeapIndex];
}

// == Buffer ======================================================================================

Buffer::Buffer()
{
}

Buffer::~Buffer()
{
    Assert_(Resource == nullptr);
}

void Buffer::Initialize(uint64 size, uint64 alignment, bool32 dynamic, bool32 cpuAccessible,
                        bool32 allowUAV, const void* initData, D3D12_RESOURCE_STATES initialState,
                        ID3D12Heap* heap, uint64 heapOffset, const wchar* name)
{
    Assert_(size > 0);
    Assert_(alignment > 0);

    Size = AlignTo(size, alignment);
    Alignment = alignment;
    Dynamic = dynamic;
    CPUAccessible = cpuAccessible;
    CurrBuffer = 0;
    CPUAddress = nullptr;
    GPUAddress = 0;
    Heap = nullptr;
    HeapOffset = 0;
    CreateFrame = DX12::CurrentCPUFrame;

    Assert_(allowUAV == false || dynamic == false);
    Assert_(dynamic || cpuAccessible == false);

    D3D12_RESOURCE_DESC resourceDesc = { };
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = dynamic ? Size * DX12::RenderLatency : Size;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.Flags = allowUAV ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Alignment = 0;

    const D3D12_HEAP_PROPERTIES* heapProps = cpuAccessible ? DX12::GetUploadHeapProps() : DX12::GetDefaultHeapProps();
    D3D12_RESOURCE_STATES resourceState = initialState;
    if(cpuAccessible)
        resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
    else if(initData)
        resourceState = D3D12_RESOURCE_STATE_COMMON;

    if(heap)
    {
        Heap = heap;
        HeapOffset = heapOffset;
        DXCall(DX12::Device->CreatePlacedResource(heap, heapOffset, &resourceDesc, resourceState,
                                                    nullptr, IID_PPV_ARGS(&Resource)));
    }
    else
    {
        DXCall(DX12::Device->CreateCommittedResource(heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                                        resourceState, nullptr, IID_PPV_ARGS(&Resource)));
    }

    if(name)
        Resource->SetName(name);

    GPUAddress = Resource->GetGPUVirtualAddress();

    if(cpuAccessible)
    {
        D3D12_RANGE readRange = { };
        DXCall(Resource->Map(0, &readRange, reinterpret_cast<void**>(&CPUAddress)));
    }

    if(initData && cpuAccessible)
    {
        for(uint64 i = 0; i < DX12::RenderLatency; ++i)
        {
            uint8* dstMem = CPUAddress + Size * i;
            memcpy(dstMem, initData, size);
        }

    }
    else if(initData)
    {
        UploadContext uploadContext = DX12::ResourceUploadBegin(resourceDesc.Width);

        memcpy(uploadContext.CPUAddress, initData, size);
        if(dynamic)
            memcpy((uint8*)uploadContext.CPUAddress + size, initData, size);

        uploadContext.CmdList->CopyBufferRegion(Resource, 0, uploadContext.Resource, uploadContext.ResourceOffset, size);

        DX12::ResourceUploadEnd(uploadContext);
    }
}

void Buffer::Shutdown()
{
    // We generally don't want to destroy something the same frame that it was created
    const bool forceDeferred = CreateFrame == DX12::CurrentCPUFrame;
    DX12::DeferredRelease(Resource, forceDeferred);
}

MapResult Buffer::Map()
{
    Assert_(Initialized());
    Assert_(Dynamic);
    Assert_(CPUAccessible);

    // Make sure that we do this at most once per-frame
    Assert_(UploadFrame != DX12::CurrentCPUFrame);
    UploadFrame = DX12::CurrentCPUFrame;

    // Cycle to the next buffer
    CurrBuffer = (DX12::CurrentCPUFrame + 1) % DX12::RenderLatency;

    MapResult result;
    result.ResourceOffset = CurrBuffer * Size;
    result.CPUAddress = CPUAddress + CurrBuffer * Size;
    result.GPUAddress = GPUAddress + CurrBuffer * Size;
    result.Resource = Resource;
    return result;
}

MapResult Buffer::MapAndSetData(const void* data, uint64 dataSize)
{
    Assert_(dataSize <= Size);
    MapResult result = Map();
    memcpy(result.CPUAddress, data, dataSize);
    return result;
}

uint64 Buffer::UpdateData(const void* srcData, uint64 srcSize, uint64 dstOffset)
{
    return MultiUpdateData(&srcData, &srcSize, &dstOffset, 1);
}

uint64 Buffer::MultiUpdateData(const void* srcData[], uint64 srcSize[], uint64 dstOffset[], uint64 numUpdates)
{
    Assert_(Dynamic);
    Assert_(CPUAccessible == false);
    Assert_(numUpdates > 0);

    // Make sure that we do this at most once per-frame
    Assert_(UploadFrame != DX12::CurrentCPUFrame);
    UploadFrame = DX12::CurrentCPUFrame;

    // Cycle to the next buffer
    CurrBuffer = (DX12::CurrentCPUFrame + 1) % DX12::RenderLatency;

    uint64 currOffset = CurrBuffer * Size;

    uint64 totalUpdateSize = 0;
    for(uint64 i = 0; i < numUpdates; ++i)
        totalUpdateSize += srcSize[i];

    UploadContext uploadContext = DX12::ResourceUploadBegin(totalUpdateSize);

    uint64 uploadOffset  = 0;
    for(uint64 i = 0; i < numUpdates; ++i)
    {
        Assert_(dstOffset[i] + srcSize[i] <= Size);
        Assert_(uploadOffset + srcSize[i] <= totalUpdateSize);
        memcpy(reinterpret_cast<uint8*>(uploadContext.CPUAddress) + uploadOffset, srcData[i], srcSize[i]);
        uploadContext.CmdList->CopyBufferRegion(Resource, currOffset + dstOffset[i], uploadContext.Resource, uploadContext.ResourceOffset + uploadOffset, srcSize[i]);

        uploadOffset += srcSize[i];
    }

    DX12::ResourceUploadEnd(uploadContext);

    return GPUAddress + currOffset;
}

void Buffer::Transition(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) const
{
    Assert_(Resource != nullptr);
    DX12::TransitionResource(cmdList, Resource, before, after);
}

void Buffer::MakeReadable(ID3D12GraphicsCommandList* cmdList) const
{
    Assert_(Resource != nullptr);
    DX12::TransitionResource(cmdList, Resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
}

void Buffer::MakeWritable(ID3D12GraphicsCommandList* cmdList) const
{
    Assert_(Resource != nullptr);
    DX12::TransitionResource(cmdList, Resource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void Buffer::UAVBarrier(ID3D12GraphicsCommandList* cmdList) const
{
    Assert_(Resource != nullptr);
    D3D12_RESOURCE_BARRIER barrier = { };
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.UAV.pResource = Resource;
    cmdList->ResourceBarrier(1, &barrier);
}

#if UseAsserts_

bool Buffer::ReadyForBinding() const
{
    return Initialized();
}

#endif

// == ConstantBuffer ==============================================================================

ConstantBuffer::ConstantBuffer()
{
}

ConstantBuffer::~ConstantBuffer()
{
}

void ConstantBuffer::Initialize(const ConstantBufferInit& init)
{
    InternalBuffer.Initialize(init.Size, DX12::ConstantBufferAlignment, init.Dynamic, init.CPUAccessible,
                              false, init.InitData, init.InitialState, init.Heap, init.HeapOffset, init.Name);
}

void ConstantBuffer::Shutdown()
{
    InternalBuffer.Shutdown();
}

void ConstantBuffer::SetAsGfxRootParameter(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter) const
{
    Assert_(InternalBuffer.ReadyForBinding());
    cmdList->SetGraphicsRootConstantBufferView(rootParameter, CurrentGPUAddress);
}

void ConstantBuffer::SetAsComputeRootParameter(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter) const
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

void ConstantBuffer::MapAndSetData(const void* data, uint64 dataSize)
{
    Assert_(dataSize <= InternalBuffer.Size);
    void* cpuAddr = Map();
    memcpy(cpuAddr, data, dataSize);
}

void ConstantBuffer::UpdateData(const void* srcData, uint64 srcSize, uint64 dstOffset)
{
    CurrentGPUAddress = InternalBuffer.UpdateData(srcData, srcSize, dstOffset);
}

void ConstantBuffer::MultiUpdateData(const void* srcData[], uint64 srcSize[], uint64 dstOffset[], uint64 numUpdates)
{
    CurrentGPUAddress = InternalBuffer.MultiUpdateData(srcData, srcSize, dstOffset, numUpdates);
}

// == StructuredBuffer ============================================================================

StructuredBuffer::StructuredBuffer()
{

}

StructuredBuffer::~StructuredBuffer()
{
    Assert_(NumElements == 0);
}

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

    InternalBuffer.Initialize(Stride * NumElements, Stride, init.Dynamic, init.CPUAccessible, init.CreateUAV,
                              init.InitData, init.InitialState, init.Heap, init.HeapOffset, init.Name);
    GPUAddress = InternalBuffer.GPUAddress;

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    SRV = srvAlloc.Index;

    // Start off all SRV's pointing to the first buffer
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = SRVDesc(0);
    for(uint32 i = 0; i < ArraySize_(srvAlloc.Handles); ++i)
        DX12::Device->CreateShaderResourceView(InternalBuffer.Resource, &srvDesc, srvAlloc.Handles[i]);

    if(init.CreateUAV)
    {
        Assert_(init.Dynamic == false);

        ID3D12Resource* counterRes = nullptr;
        if(init.UseCounter)
        {
            D3D12_RESOURCE_DESC resourceDesc = { };
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resourceDesc.Width = sizeof(uint32);
            resourceDesc.Height = 1;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = 1;
            resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            resourceDesc.SampleDesc.Count = 1;
            resourceDesc.SampleDesc.Quality = 0;
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            resourceDesc.Alignment = 0;
            DXCall(DX12::Device->CreateCommittedResource(DX12::GetDefaultHeapProps(), D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                                         D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&CounterResource)));

            counterRes = CounterResource;

            CounterUAV = DX12::UAVDescriptorHeap.AllocatePersistent().Handles[0];

            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Format = DXGI_FORMAT_UNKNOWN;
            uavDesc.Buffer.CounterOffsetInBytes = 0;
            uavDesc.Buffer.FirstElement = 0;
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
            uavDesc.Buffer.NumElements = 1;
            uavDesc.Buffer.StructureByteStride = sizeof(uint32);
            DX12::Device->CreateUnorderedAccessView(counterRes, nullptr, &uavDesc, CounterUAV);
        }

        UAV = DX12::UAVDescriptorHeap.AllocatePersistent().Handles[0];

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = { };
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        uavDesc.Buffer.NumElements = uint32(NumElements);
        uavDesc.Buffer.StructureByteStride = uint32(Stride);
        DX12::Device->CreateUnorderedAccessView(InternalBuffer.Resource, counterRes, &uavDesc, UAV);
    }
}

void StructuredBuffer::Shutdown()
{
    // We generally don't want to destroy something the same frame that it was created
    const bool forceDeferred = InternalBuffer.CreateFrame == DX12::CurrentCPUFrame;
    DX12::DeferredRelease(CounterResource, forceDeferred);

    DX12::SRVDescriptorHeap.FreePersistent(SRV);
    DX12::UAVDescriptorHeap.FreePersistent(UAV);
    DX12::UAVDescriptorHeap.FreePersistent(CounterUAV);
    InternalBuffer.Shutdown();
    Stride = 0;
    NumElements = 0;
}

D3D12_VERTEX_BUFFER_VIEW StructuredBuffer::VBView() const
{
    Assert_(InternalBuffer.ReadyForBinding());
    D3D12_VERTEX_BUFFER_VIEW vbView = { };
    vbView.BufferLocation = GPUAddress;
    vbView.StrideInBytes = uint32(Stride);
    vbView.SizeInBytes = uint32(InternalBuffer.Size);
    return vbView;
}

D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE StructuredBuffer::ShaderTable(uint64 startElement, uint64 numElements) const
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

D3D12_GPU_VIRTUAL_ADDRESS_RANGE StructuredBuffer::ShaderRecord(uint64 element) const
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

void StructuredBuffer::MapAndSetData(const void* data, uint64 numElements)
{
    Assert_(numElements <= NumElements);
    void* cpuAddr = Map();
    memcpy(cpuAddr, data, numElements * Stride);
}

void StructuredBuffer::UpdateData(const void* srcData, uint64 srcNumElements, uint64 dstElemOffset)
{
    GPUAddress = InternalBuffer.UpdateData(srcData, srcNumElements * Stride, dstElemOffset * Stride);

    UpdateDynamicSRV();
}

void StructuredBuffer::MultiUpdateData(const void* srcData[], uint64 srcNumElements[], uint64 dstElemOffset[], uint64 numUpdates)
{
    uint64 srcSizes[16];
    uint64 dstOffsets[16];
    Assert_(numUpdates <= ArraySize_(srcSizes));
    for(uint64 i = 0; i < numUpdates; ++i)
    {
        srcSizes[i] = srcNumElements[i] * Stride;
        dstOffsets[i] = dstElemOffset[i] * Stride;
    }

    GPUAddress = InternalBuffer.MultiUpdateData(srcData, srcSizes, dstOffsets, numUpdates);

    UpdateDynamicSRV();
}

void StructuredBuffer::Transition(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) const
{
    InternalBuffer.Transition(cmdList, before, after);
}

void StructuredBuffer::MakeReadable(ID3D12GraphicsCommandList* cmdList) const
{
    InternalBuffer.MakeReadable(cmdList);
}

void StructuredBuffer::MakeWritable(ID3D12GraphicsCommandList* cmdList) const
{
    InternalBuffer.MakeWritable(cmdList);
}

void StructuredBuffer::UAVBarrier(ID3D12GraphicsCommandList* cmdList) const
{
    InternalBuffer.UAVBarrier(cmdList);
}

D3D12_SHADER_RESOURCE_VIEW_DESC StructuredBuffer::SRVDesc(uint64 bufferIdx) const
{
    Assert_(bufferIdx == 0 || InternalBuffer.Dynamic);
    Assert_(bufferIdx < DX12::RenderLatency);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = uint32(NumElements * bufferIdx);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Buffer.NumElements = uint32(NumElements);
    srvDesc.Buffer.StructureByteStride = uint32(Stride);
    return srvDesc;
}

void StructuredBuffer::UpdateDynamicSRV() const
{
    Assert_(InternalBuffer.Dynamic);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = SRVDesc(InternalBuffer.CurrBuffer);
    DX12::DeferredCreateSRV(InternalBuffer.Resource, srvDesc, SRV);
}

// == FormattedBuffer ============================================================================

FormattedBuffer::FormattedBuffer()
{
}

FormattedBuffer::~FormattedBuffer()
{
    Assert_(NumElements == 0);
    Shutdown();
}

void FormattedBuffer::Initialize(const FormattedBufferInit& init)
{
    Shutdown();

    Assert_(init.Format != DXGI_FORMAT_UNKNOWN);
    Assert_(init.NumElements > 0);
    Stride = DirectX::BitsPerPixel(init.Format) / 8;
    NumElements = init.NumElements;
    Format = init.Format;

    InternalBuffer.Initialize(Stride * NumElements, Stride, init.Dynamic, init.CPUAccessible, init.CreateUAV,
                              init.InitData, init.InitialState, init.Heap, init.HeapOffset, init.Name);
    GPUAddress = InternalBuffer.GPUAddress;

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    SRV = srvAlloc.Index;

// Start off all SRV's pointing to the first buffer
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = SRVDesc(0);
    for(uint32 i = 0; i < ArraySize_(srvAlloc.Handles); ++i)
        DX12::Device->CreateShaderResourceView(InternalBuffer.Resource, &srvDesc, srvAlloc.Handles[i]);

    if(init.CreateUAV)
    {
        Assert_(init.Dynamic == false);

        UAV = DX12::UAVDescriptorHeap.AllocatePersistent().Handles[0];

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = { };
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Format = Format;
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        uavDesc.Buffer.NumElements = uint32(NumElements);

        DX12::Device->CreateUnorderedAccessView(InternalBuffer.Resource, nullptr, &uavDesc, UAV);
    }
}

void FormattedBuffer::Shutdown()
{
    DX12::SRVDescriptorHeap.FreePersistent(SRV);
    DX12::SRVDescriptorHeap.FreePersistent(SRV);
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
    ibView.SizeInBytes = uint32(InternalBuffer.Size);
    return ibView;
}

void* FormattedBuffer::Map()
{
    MapResult mapResult = InternalBuffer.Map();

    GPUAddress = mapResult.GPUAddress;
    return mapResult.CPUAddress;
}

void FormattedBuffer::MapAndSetData(const void* data, uint64 numElements)
{
    Assert_(numElements <= NumElements);
    void* cpuAddr = Map();
    memcpy(cpuAddr, data, numElements * Stride);
}

void FormattedBuffer::UpdateData(const void* srcData, uint64 srcNumElements, uint64 dstElemOffset)
{
    GPUAddress = InternalBuffer.UpdateData(srcData, srcNumElements * Stride, dstElemOffset * Stride);
}

void FormattedBuffer::MultiUpdateData(const void* srcData[], uint64 srcNumElements[], uint64 dstElemOffset[], uint64 numUpdates)
{
    uint64 srcSizes[16];
    uint64 dstOffsets[16];
    Assert_(numUpdates <= ArraySize_(srcSizes));
    for(uint64 i = 0; i < numUpdates; ++i)
    {
        srcSizes[i] = srcNumElements[i] * Stride;
        dstOffsets[i] = dstElemOffset[i] * Stride;
    }

    GPUAddress = InternalBuffer.MultiUpdateData(srcData, srcSizes, dstOffsets, numUpdates);
}

void FormattedBuffer::Transition(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) const
{
    InternalBuffer.Transition(cmdList, before, after);
}

void FormattedBuffer::MakeReadable(ID3D12GraphicsCommandList* cmdList) const
{
    InternalBuffer.MakeReadable(cmdList);
}

void FormattedBuffer::MakeWritable(ID3D12GraphicsCommandList* cmdList) const
{
    InternalBuffer.MakeWritable(cmdList);
}

void FormattedBuffer::UAVBarrier(ID3D12GraphicsCommandList* cmdList) const
{
    InternalBuffer.UAVBarrier(cmdList);
}

D3D12_SHADER_RESOURCE_VIEW_DESC FormattedBuffer::SRVDesc(uint64 bufferIdx) const
{
    Assert_(bufferIdx == 0 || InternalBuffer.Dynamic);
    Assert_(bufferIdx < DX12::RenderLatency);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = uint32(NumElements * bufferIdx);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Buffer.NumElements = uint32(NumElements);
    return srvDesc;
}

void FormattedBuffer::UpdateDynamicSRV() const
{
    Assert_(InternalBuffer.Dynamic);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = SRVDesc(InternalBuffer.CurrBuffer);
    DX12::DeferredCreateSRV(InternalBuffer.Resource, srvDesc, SRV);
}

// == RawBuffer ============================================================================

RawBuffer::RawBuffer()
{
}

RawBuffer::~RawBuffer()
{
    Assert_(NumElements == 0);
    Shutdown();
}

void RawBuffer::Initialize(const RawBufferInit& init)
{
    Shutdown();

    Assert_(init.NumElements > 0);
    NumElements = init.NumElements;

    InternalBuffer.Initialize(Stride * NumElements, Stride, init.Dynamic, init.CPUAccessible, init.CreateUAV, init.InitData,
                              init.InitialState, init.Heap, init.HeapOffset, init.Name);
    GPUAddress = InternalBuffer.GPUAddress;

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    SRV = srvAlloc.Index;

    // Start off all SRV's pointing to the first buffer
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = SRVDesc(0);
    for(uint32 i = 0; i < ArraySize_(srvAlloc.Handles); ++i)
        DX12::Device->CreateShaderResourceView(InternalBuffer.Resource, &srvDesc, srvAlloc.Handles[i]);

    if(init.CreateUAV)
    {
        Assert_(init.Dynamic == false);

        UAV = DX12::UAVDescriptorHeap.AllocatePersistent().Handles[0];

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = { };
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        uavDesc.Buffer.NumElements = uint32(NumElements);
        DX12::Device->CreateUnorderedAccessView(InternalBuffer.Resource, nullptr, &uavDesc, UAV);
    }
}

void RawBuffer::Shutdown()
{
    DX12::SRVDescriptorHeap.FreePersistent(SRV);
    DX12::UAVDescriptorHeap.FreePersistent(UAV);
    InternalBuffer.Shutdown();
    NumElements = 0;
}

void* RawBuffer::Map()
{
    MapResult mapResult = InternalBuffer.Map();
    GPUAddress = mapResult.GPUAddress;
    return mapResult.CPUAddress;
}

void RawBuffer::MapAndSetData(const void* data, uint64 numElements)
{
    Assert_(numElements <= NumElements);
    void* cpuAddr = Map();
    memcpy(cpuAddr, data, numElements * Stride);
}

void RawBuffer::UpdateData(const void* srcData, uint64 srcNumElements, uint64 dstElemOffset)
{
    GPUAddress = InternalBuffer.UpdateData(srcData, srcNumElements * Stride, dstElemOffset * Stride);
}

void RawBuffer::MultiUpdateData(const void* srcData[], uint64 srcNumElements[], uint64 dstElemOffset[], uint64 numUpdates)
{
    uint64 srcSizes[16];
    uint64 dstOffsets[16];
    Assert_(numUpdates <= ArraySize_(srcSizes));
    for(uint64 i = 0; i < numUpdates; ++i)
    {
        srcSizes[i] = srcNumElements[i] * Stride;
        dstOffsets[i] = dstElemOffset[i] * Stride;
    }

    GPUAddress = InternalBuffer.MultiUpdateData(srcData, srcSizes, dstOffsets, numUpdates);
}

void RawBuffer::Transition(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) const
{
    InternalBuffer.Transition(cmdList, before, after);
}

void RawBuffer::MakeReadable(ID3D12GraphicsCommandList* cmdList) const
{
    InternalBuffer.MakeReadable(cmdList);
}

void RawBuffer::MakeWritable(ID3D12GraphicsCommandList* cmdList) const
{
    InternalBuffer.MakeWritable(cmdList);
}

void RawBuffer::UAVBarrier(ID3D12GraphicsCommandList* cmdList) const
{
    InternalBuffer.UAVBarrier(cmdList);
}

D3D12_SHADER_RESOURCE_VIEW_DESC RawBuffer::SRVDesc(uint64 bufferIdx) const
{
    Assert_(bufferIdx == 0 || InternalBuffer.Dynamic);
    Assert_(bufferIdx < DX12::RenderLatency);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = uint32(NumElements * bufferIdx);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
    srvDesc.Buffer.NumElements = uint32(NumElements);
    return srvDesc;
}

void RawBuffer::UpdateDynamicSRV() const
{
    Assert_(InternalBuffer.Dynamic);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = SRVDesc(InternalBuffer.CurrBuffer);
    DX12::DeferredCreateSRV(InternalBuffer.Resource, srvDesc, SRV);
}

// == ReadbackBuffer ==============================================================================

ReadbackBuffer::ReadbackBuffer()
{
}

ReadbackBuffer::~ReadbackBuffer()
{
    Assert_(Resource == nullptr);
}

void ReadbackBuffer::Initialize(uint64 size)
{
    Assert_(size > 0);
    Size = size;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = uint32(size);
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Alignment = 0;

    DXCall(DX12::Device->CreateCommittedResource(DX12::GetReadbackHeapProps(), D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                                 D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&Resource)));
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

Fence::~Fence()
{
    Assert_(D3DFence == nullptr);
    Shutdown();
}

void Fence::Init(uint64 initialValue)
{
    DXCall(DX12::Device->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&D3DFence)));
    FenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
    Win32Call(FenceEvent != 0);
}

void Fence::Shutdown()
{
    DX12::DeferredRelease(D3DFence);
}

void Fence::Signal(ID3D12CommandQueue* queue, uint64 fenceValue)
{
    Assert_(D3DFence != nullptr);
    DXCall(queue->Signal(D3DFence, fenceValue));
}

void Fence::Wait(uint64 fenceValue)
{
    Assert_(D3DFence != nullptr);
    if(D3DFence->GetCompletedValue() < fenceValue)
    {
        DXCall(D3DFence->SetEventOnCompletion(fenceValue, FenceEvent));
        WaitForSingleObject(FenceEvent, INFINITE);
    }
}


bool Fence::Signaled(uint64 fenceValue)
{
    Assert_(D3DFence != nullptr);
    return D3DFence->GetCompletedValue() >= fenceValue;
}

void Fence::Clear(uint64 fenceValue)
{
    Assert_(D3DFence != nullptr);
    D3DFence->Signal(fenceValue);
}

// == Texture ====================================================================================

Texture::Texture()
{
}

Texture::~Texture()
{
    Assert_(Resource == nullptr);
}

void Texture::Shutdown()
{
    DX12::SRVDescriptorHeap.FreePersistent(SRV);

    // We generally don't want to destroy something the same frame that it was created
    const bool forceDeferred = CreateFrame == DX12::CurrentCPUFrame;
    DX12::DeferredRelease(Resource, forceDeferred);
}

// == RenderTexture ===============================================================================

static D3D12_RESOURCE_STATES RTReadState(bool32 nonPSReadable)
{
    return nonPSReadable ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
                         : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

RenderTexture::RenderTexture()
{
}

RenderTexture::~RenderTexture()
{
    Assert_(RTV.ptr == 0);
}

void RenderTexture::Initialize(const RenderTextureInit& init)
{
    Shutdown();

    Assert_(init.Width > 0);
    Assert_(init.Height > 0);
    Assert_(init.MSAASamples > 0);
    Assert_(init.CreateUAV || init.CreateRTV);

    D3D12_RESOURCE_DESC textureDesc = { };
    textureDesc.MipLevels = uint16(init.NumMips);
    textureDesc.Format = init.Format;
    textureDesc.Width = uint32(init.Width);
    textureDesc.Height = uint32(init.Height);
    if(init.CreateRTV)
        textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if(init.CreateUAV)
        textureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    textureDesc.DepthOrArraySize = uint16(init.ArraySize);
    textureDesc.SampleDesc.Count = uint32(init.MSAASamples);
    textureDesc.SampleDesc.Quality = init.MSAASamples > 1 ? DX12::StandardMSAAPattern : 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Alignment = 0;

    // Use the initial state specified if it's valid, otherwise pick a sensible default
    D3D12_RESOURCE_STATES initialState = init.InitialState != D3D12_RESOURCE_STATES(-1) ? init.InitialState : RTReadState(init.NonPSReadable);

    D3D12_CLEAR_VALUE clearValue = { };
    clearValue.Format = init.Format;
    DXCall(DX12::Device->CreateCommittedResource(DX12::GetDefaultHeapProps(), D3D12_HEAP_FLAG_NONE, &textureDesc,
                                                 initialState, init.CreateRTV ? &clearValue : nullptr, IID_PPV_ARGS(&Texture.Resource)));

    if(init.Name != nullptr)
        Texture.Resource->SetName(init.Name);

    const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDescPtr = nullptr;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
    if(init.CubeMap)
    {
        Assert_(init.ArraySize == 6);
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = uint32(-1);
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        srvDescPtr = &srvDesc;
    }

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    Texture.SRV = srvAlloc.Index;
    for(uint32 i = 0; i < DX12::SRVDescriptorHeap.NumHeaps; ++i)
        DX12::Device->CreateShaderResourceView(Texture.Resource, srvDescPtr, srvAlloc.Handles[i]);

    Texture.Width = uint32(init.Width);
    Texture.Height = uint32(init.Height);
    Texture.Depth = 1;
    Texture.NumMips = init.NumMips;
    Texture.ArraySize = uint32(init.ArraySize);
    Texture.Format = init.Format;
    Texture.Cubemap = init.CubeMap;
    MSAASamples = uint32(init.MSAASamples);
    MSAAQuality = uint32(textureDesc.SampleDesc.Quality);
    NonPSReadable = init.NonPSReadable;

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

        for(uint64 i = 0; i < init.ArraySize; ++i)
        {
            if(init.MSAASamples > 1)
                rtvDesc.Texture2DMSArray.FirstArraySlice = uint32(i);
            else
                rtvDesc.Texture2DArray.FirstArraySlice = uint32(i);

            ArrayRTVs[i] = DX12::RTVDescriptorHeap.AllocatePersistent().Handles[0];
            DX12::Device->CreateRenderTargetView(Texture.Resource, &rtvDesc, ArrayRTVs[i]);
        }
    }

    if(init.CreateUAV)
    {
        UAV = DX12::UAVDescriptorHeap.AllocatePersistent().Handles[0];
        DX12::Device->CreateUnorderedAccessView(Texture.Resource, nullptr, nullptr, UAV);
    }
}

void RenderTexture::Shutdown()
{
    DX12::RTVDescriptorHeap.FreePersistent(RTV);
    DX12::UAVDescriptorHeap.FreePersistent(UAV);
    for(uint64 i = 0; i < ArrayRTVs.Size(); ++i)
        DX12::RTVDescriptorHeap.FreePersistent(ArrayRTVs[i]);
    ArrayRTVs.Shutdown();
    Texture.Shutdown();
}

void RenderTexture::Transition(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, uint64 mipLevel, uint64 arraySlice) const
{
    uint32 subResourceIdx = mipLevel == uint64(-1) || arraySlice == uint64(-1) ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
                                                                               : uint32(SubResourceIndex(mipLevel, arraySlice));
    Assert_(Texture.Resource != nullptr);
    DX12::TransitionResource(cmdList, Texture.Resource, before, after, subResourceIdx);
}

void RenderTexture::MakeReadable(ID3D12GraphicsCommandList* cmdList, uint64 mipLevel, uint64 arraySlice) const
{
    uint32 subResourceIdx = mipLevel == uint64(-1) || arraySlice == uint64(-1) ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
                                                                               : uint32(SubResourceIndex(mipLevel, arraySlice));
    Assert_(Texture.Resource != nullptr);
    DX12::TransitionResource(cmdList, Texture.Resource, D3D12_RESOURCE_STATE_RENDER_TARGET, RTReadState(NonPSReadable), subResourceIdx);
}

void RenderTexture::MakeWritable(ID3D12GraphicsCommandList* cmdList, uint64 mipLevel, uint64 arraySlice) const
{
    uint32 subResourceIdx = mipLevel == uint64(-1) || arraySlice == uint64(-1) ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
                                                                               : uint32(SubResourceIndex(mipLevel, arraySlice));
    Assert_(Texture.Resource != nullptr);
    DX12::TransitionResource(cmdList, Texture.Resource, RTReadState(NonPSReadable), D3D12_RESOURCE_STATE_RENDER_TARGET, subResourceIdx);
}

void RenderTexture::MakeReadableUAV(ID3D12GraphicsCommandList* cmdList, uint64 mipLevel, uint64 arraySlice) const
{
    uint32 subResourceIdx = mipLevel == uint64(-1) || arraySlice == uint64(-1) ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
                                                                               : uint32(SubResourceIndex(mipLevel, arraySlice));
    Assert_(Texture.Resource != nullptr);
    DX12::TransitionResource(cmdList, Texture.Resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, RTReadState(NonPSReadable), subResourceIdx);
}

void RenderTexture::MakeWritableUAV(ID3D12GraphicsCommandList* cmdList, uint64 mipLevel, uint64 arraySlice) const
{
    uint32 subResourceIdx = mipLevel == uint64(-1) || arraySlice == uint64(-1) ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
                                                                               : uint32(SubResourceIndex(mipLevel, arraySlice));
    Assert_(Texture.Resource != nullptr);
    DX12::TransitionResource(cmdList, Texture.Resource, RTReadState(NonPSReadable), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, subResourceIdx);
}

void RenderTexture::UAVBarrier(ID3D12GraphicsCommandList* cmdList) const
{
    Assert_(Texture.Resource != nullptr);

    D3D12_RESOURCE_BARRIER barrier = { };
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.UAV.pResource = Texture.Resource;
    cmdList->ResourceBarrier(1, &barrier);
}

D3D12_RESOURCE_STATES RenderTexture::ReadState() const {
    return RTReadState(NonPSReadable);
}

// == VolumeTexture ===============================================================================

VolumeTexture::VolumeTexture()
{
}

VolumeTexture::~VolumeTexture()
{
    Assert_(UAV.ptr == 0);
}

void VolumeTexture::Initialize(const VolumeTextureInit& init)
{
    Shutdown();

    Assert_(init.Width > 0);
    Assert_(init.Height > 0);
    Assert_(init.Depth > 0);

    D3D12_RESOURCE_DESC textureDesc = { };
    textureDesc.MipLevels = 1;
    textureDesc.Format = init.Format;
    textureDesc.Width = uint32(init.Width);
    textureDesc.Height = uint32(init.Height);
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    textureDesc.DepthOrArraySize = uint16(init.Depth);
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Alignment = 0;

    DXCall(DX12::Device->CreateCommittedResource(DX12::GetDefaultHeapProps(), D3D12_HEAP_FLAG_NONE, &textureDesc,
                                                 init.InitialState, nullptr, IID_PPV_ARGS(&Texture.Resource)));

    if(init.Name != nullptr)
        Texture.Resource->SetName(init.Name);

    PersistentDescriptorAlloc srvAlloc = DX12::SRVDescriptorHeap.AllocatePersistent();
    Texture.SRV = srvAlloc.Index;
    for(uint32 i = 0; i < DX12::SRVDescriptorHeap.NumHeaps; ++i)
        DX12::Device->CreateShaderResourceView(Texture.Resource, nullptr, srvAlloc.Handles[i]);

    Texture.Width = uint32(init.Width);
    Texture.Height = uint32(init.Height);
    Texture.Depth = uint32(init.Depth);
    Texture.NumMips = 1;
    Texture.ArraySize = 1;
    Texture.Format = init.Format;
    Texture.Cubemap = false;

    UAV = DX12::UAVDescriptorHeap.AllocatePersistent().Handles[0];
    DX12::Device->CreateUnorderedAccessView(Texture.Resource, nullptr, nullptr, UAV);
}

void VolumeTexture::Shutdown()
{
    DX12::UAVDescriptorHeap.FreePersistent(UAV);
    Texture.Shutdown();
}

void VolumeTexture::Transition(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) const
{
    Assert_(Texture.Resource != nullptr);
    DX12::TransitionResource(cmdList, Texture.Resource, before, after, 0);
}

void VolumeTexture::MakeReadable(ID3D12GraphicsCommandList* cmdList) const
{
    Assert_(Texture.Resource != nullptr);
    DX12::TransitionResource(cmdList, Texture.Resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0);
}

void VolumeTexture::MakeWritable(ID3D12GraphicsCommandList* cmdList) const
{
    Assert_(Texture.Resource != nullptr);
    DX12::TransitionResource(cmdList, Texture.Resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0);
}

// == DepthBuffer ===============================================================================

DepthBuffer::DepthBuffer()
{
}

DepthBuffer::~DepthBuffer()
{
    Assert_(DSVFormat == DXGI_FORMAT_UNKNOWN);
    Shutdown();
}

void DepthBuffer::Initialize(const DepthBufferInit& init)
{
    Shutdown();

    Assert_(init.Width > 0);
    Assert_(init.Height > 0);
    Assert_(init.MSAASamples > 0);

    DXGI_FORMAT texFormat = init.Format;
    DXGI_FORMAT srvFormat = init.Format;
    if(init.Format == DXGI_FORMAT_D16_UNORM)
    {
        texFormat = DXGI_FORMAT_R16_TYPELESS;
        srvFormat = DXGI_FORMAT_R16_UNORM;
    }
    else if(init.Format == DXGI_FORMAT_D24_UNORM_S8_UINT)
    {
        texFormat = DXGI_FORMAT_R24G8_TYPELESS;
        srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    }
    else if(init.Format == DXGI_FORMAT_D32_FLOAT)
    {
        texFormat = DXGI_FORMAT_R32_TYPELESS;
        srvFormat = DXGI_FORMAT_R32_FLOAT;
    }
    else if(init.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
    {
        texFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
        srvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    }
    else
    {
        AssertFail_("Invalid depth buffer format!");
    }

    D3D12_RESOURCE_DESC textureDesc = { };
    textureDesc.MipLevels = 1;
    textureDesc.Format = texFormat;
    textureDesc.Width = uint32(init.Width);
    textureDesc.Height = uint32(init.Height);
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    textureDesc.DepthOrArraySize = uint16(init.ArraySize);
    textureDesc.SampleDesc.Count = uint32(init.MSAASamples);
    textureDesc.SampleDesc.Quality = init.MSAASamples> 1 ? DX12::StandardMSAAPattern : 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Alignment = 0;

    D3D12_CLEAR_VALUE clearValue = { };
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;
    clearValue.Format = init.Format;

    DXCall(DX12::Device->CreateCommittedResource(DX12::GetDefaultHeapProps(), D3D12_HEAP_FLAG_NONE, &textureDesc,
                                                 init.InitialState, &clearValue, IID_PPV_ARGS(&Texture.Resource)));

    if(init.Name != nullptr)
        Texture.Resource->SetName(init.Name);

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
        srvDesc.Texture2DArray.ArraySize = uint32(init.ArraySize);
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
        srvDesc.Texture2DMSArray.ArraySize = uint32(init.ArraySize);
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
    }

    for(uint32 i = 0; i < DX12::SRVDescriptorHeap.NumHeaps; ++i)
        DX12::Device->CreateShaderResourceView(Texture.Resource, &srvDesc, srvAlloc.Handles[i]);

    Texture.Width = uint32(init.Width);
    Texture.Height = uint32(init.Height);
    Texture.Depth = 1;
    Texture.NumMips = 1;
    Texture.ArraySize = uint32(init.ArraySize);
    Texture.Format = srvFormat;
    Texture.Cubemap = false;
    MSAASamples = uint32(init.MSAASamples);
    MSAAQuality = uint32(textureDesc.SampleDesc.Quality);

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
        dsvDesc.Texture2DArray.ArraySize = uint32(init.ArraySize);
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
        dsvDesc.Texture2DMSArray.ArraySize = uint32(init.ArraySize);
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

        for(uint64 i = 0; i < init.ArraySize; ++i)
        {
            if(init.MSAASamples > 1)
                dsvDesc.Texture2DMSArray.FirstArraySlice = uint32(i);
            else
                dsvDesc.Texture2DArray.FirstArraySlice = uint32(i);

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
    for(uint64 i = 0; i < ArrayDSVs.Size(); ++i)
        DX12::DSVDescriptorHeap.FreePersistent(ArrayDSVs[i]);
    ArrayDSVs.Shutdown();
    Texture.Shutdown();
    DSVFormat = DXGI_FORMAT_UNKNOWN;
}

void DepthBuffer::Transition(ID3D12GraphicsCommandList* cmdList, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, uint64 arraySlice) const
{
    uint32 subResourceIdx = arraySlice == uint64(-1) ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : uint32(arraySlice);
    Assert_(Texture.Resource != nullptr);
    DX12::TransitionResource(cmdList, Texture.Resource, before, after, subResourceIdx);
}

void DepthBuffer::MakeReadable(ID3D12GraphicsCommandList* cmdList, uint64 arraySlice) const
{
    uint32 subResourceIdx = arraySlice == uint64(-1) ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : uint32(arraySlice);
    Assert_(Texture.Resource != nullptr);
    DX12::TransitionResource(cmdList, Texture.Resource, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                             D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, subResourceIdx);
}

void DepthBuffer::MakeWritable(ID3D12GraphicsCommandList* cmdList, uint64 arraySlice) const
{
    uint32 subResourceIdx = arraySlice == uint64(-1) ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : uint32(arraySlice);
    Assert_(Texture.Resource != nullptr);
    DX12::TransitionResource(cmdList, Texture.Resource, D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                             D3D12_RESOURCE_STATE_DEPTH_WRITE, subResourceIdx);
}

}