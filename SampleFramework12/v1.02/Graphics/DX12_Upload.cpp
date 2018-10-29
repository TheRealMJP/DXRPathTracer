//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "DX12_Upload.h"
#include "DX12.h"
#include "GraphicsTypes.h"
#include "ShaderCompilation.h"

namespace SampleFramework12
{

namespace DX12
{

enum ConvertRootParams : uint32
{
    ConvertParams_StandardDescriptors = 0,
    ConvertParams_UAV,
    ConvertParams_CBuffer,

    NumConvertRootParams
};

struct UploadSubmission
{
    ID3D12CommandAllocator* CmdAllocator = nullptr;
    ID3D12GraphicsCommandList1* CmdList = nullptr;
    uint64 Offset = 0;
    uint64 Size = 0;
    uint64 FenceValue = 0;
    uint64 Padding = 0;

    void Reset()
    {
        Offset = 0;
        Size = 0;
        FenceValue = 0;
        Padding = 0;
    }
};

static ID3D12GraphicsCommandList1* convertCmdList = nullptr;
static ID3D12CommandQueue* convertCmdQueue = nullptr;
static ID3D12CommandAllocator* convertCmdAllocator = nullptr;
static ID3D12RootSignature* convertRootSignature = nullptr;
static ID3D12PipelineState* convertPSO = nullptr;
static ID3D12PipelineState* convertArrayPSO = nullptr;
static ID3D12PipelineState* convertCubePSO = nullptr;
static CompiledShaderPtr convertCS;
static CompiledShaderPtr convertArrayCS;
static CompiledShaderPtr convertCubeCS;
static uint32 convertTGSize = 8;
static Fence convertFence;

static ID3D12GraphicsCommandList1* readbackCmdList = nullptr;
static ID3D12CommandAllocator* readbackCmdAllocator = nullptr;
static Fence readbackFence;

static const uint64 UploadBufferSize = 256 * 1024 * 1024;
static const uint64 MaxUploadSubmissions = 16;
static ID3D12Resource* UploadBuffer = nullptr;
static uint8* UploadBufferCPUAddr = nullptr;
static SRWLOCK UploadSubmissionLock = SRWLOCK_INIT;
static SRWLOCK UploadQueueLock = SRWLOCK_INIT;

// These are protected by UploadQueueLock
static ID3D12CommandQueue* UploadCmdQueue = nullptr;
static Fence UploadFence;
static uint64 UploadFenceValue = 0;

// These are protected by UploadSubmissionLock
static uint64 UploadBufferStart = 0;
static uint64 UploadBufferUsed = 0;
static UploadSubmission UploadSubmissions[MaxUploadSubmissions];
static uint64 UploadSubmissionStart = 0;
static uint64 UploadSubmissionUsed = 0;

static const uint64 TempBufferSize = 2 * 1024 * 1024;
static ID3D12Resource* TempFrameBuffers[RenderLatency] = { };
static uint8* TempFrameCPUMem[RenderLatency] = { };
static uint64 TempFrameGPUMem[RenderLatency] = { };
static volatile int64 TempFrameUsed = 0;

static void ClearFinishedUploads(uint64 flushCount)
{
    const uint64 start = UploadSubmissionStart;
    const uint64 used = UploadSubmissionUsed;
    for(uint64 i = 0; i < used; ++i)
    {
        const uint64 idx = (start + i) % MaxUploadSubmissions;
        UploadSubmission& submission = UploadSubmissions[idx];
        Assert_(submission.Size > 0);
        Assert_(UploadBufferUsed >= submission.Size);

        // If the submission hasn't been sent to the GPU yet we can't wait for it
        if(submission.FenceValue == uint64(-1))
            return;

        if(i < flushCount)
            UploadFence.Wait(submission.FenceValue);

        if(UploadFence.Signaled(submission.FenceValue))
        {
            UploadSubmissionStart = (UploadSubmissionStart + 1) % MaxUploadSubmissions;
            UploadSubmissionUsed -= 1;
            UploadBufferStart = (UploadBufferStart + submission.Padding) % UploadBufferSize;
            Assert_(submission.Offset == UploadBufferStart);
            Assert_(UploadBufferStart + submission.Size <= UploadBufferSize);
            UploadBufferStart = (UploadBufferStart + submission.Size) % UploadBufferSize;
            UploadBufferUsed -= (submission.Size + submission.Padding);
            submission.Reset();

            if(UploadBufferUsed == 0)
                UploadBufferStart = 0;
        }
    }
}

static UploadSubmission* AllocUploadSubmission(uint64 size)
{
    Assert_(UploadSubmissionUsed <= MaxUploadSubmissions);
    if(UploadSubmissionUsed == MaxUploadSubmissions)
        return nullptr;

    const uint64 submissionIdx = (UploadSubmissionStart + UploadSubmissionUsed) % MaxUploadSubmissions;
    Assert_(UploadSubmissions[submissionIdx].Size == 0);

    Assert_(UploadBufferUsed <= UploadBufferSize);
    if(size > (UploadBufferSize - UploadBufferUsed))
        return nullptr;

    const uint64 start = UploadBufferStart;
    const uint64 end = UploadBufferStart + UploadBufferUsed;
    uint64 allocOffset = uint64(-1);
    uint64 padding = 0;
    if(end < UploadBufferSize)
    {
        const uint64 endAmt = UploadBufferSize - end;
        if(endAmt >= size)
        {
            allocOffset = end;
        }
        else if(start >= size)
        {
            // Wrap around to the beginning
            allocOffset = 0;
            UploadBufferUsed += endAmt;
            padding = endAmt;
        }
    }
    else
    {
        const uint64 wrappedEnd = end % UploadBufferSize;
        if((start - wrappedEnd) >= size)
            allocOffset = wrappedEnd;
    }

    if(allocOffset == uint64(-1))
        return nullptr;

    UploadSubmissionUsed += 1;
    UploadBufferUsed += size;

    UploadSubmission* submission = &UploadSubmissions[submissionIdx];
    submission->Offset = allocOffset;
    submission->Size = size;
    submission->FenceValue = uint64(-1);
    submission->Padding = padding;

    return submission;
}

void Initialize_Upload()
{
    for(uint64 i = 0; i < MaxUploadSubmissions; ++i) {
        UploadSubmission& submission = UploadSubmissions[i];
        DXCall(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&submission.CmdAllocator)));
        DXCall(Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, submission.CmdAllocator, nullptr, IID_PPV_ARGS(&submission.CmdList)));
        DXCall(submission.CmdList->Close());

        submission.CmdList->SetName(L"Upload Command List");
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = { };
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    DXCall(Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&UploadCmdQueue)));
    UploadCmdQueue->SetName(L"Upload Copy Queue");

    UploadFence.Init(0);

    D3D12_RESOURCE_DESC resourceDesc = { };
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = uint32(UploadBufferSize);
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Alignment = 0;

    DXCall(Device->CreateCommittedResource(DX12::GetUploadHeapProps(), D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                           D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&UploadBuffer)));

    D3D12_RANGE readRange = { };
    DXCall(UploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&UploadBufferCPUAddr)));

    // Temporary buffer memory that swaps every frame
    resourceDesc.Width = uint32(TempBufferSize);

    for(uint64 i = 0; i < RenderLatency; ++i)
    {
        DXCall(Device->CreateCommittedResource(DX12::GetUploadHeapProps(), D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                               D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&TempFrameBuffers[i])));

        DXCall(TempFrameBuffers[i]->Map(0, &readRange, reinterpret_cast<void**>(&TempFrameCPUMem[i])));
        TempFrameGPUMem[i] = TempFrameBuffers[i]->GetGPUVirtualAddress();
    }

    // Texture conversion resources
    DXCall(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&convertCmdAllocator)));
    DXCall(Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, convertCmdAllocator, nullptr, IID_PPV_ARGS(&convertCmdList)));
    DXCall(convertCmdList->Close());
    DXCall(convertCmdList->Reset(convertCmdAllocator, nullptr));

    D3D12_COMMAND_QUEUE_DESC convertQueueDesc = {};
    convertQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    convertQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    DXCall(Device->CreateCommandQueue(&convertQueueDesc, IID_PPV_ARGS(&convertCmdQueue)));

    CompileOptions opts;
    opts.Add("TGSize_", convertTGSize);
    const std::wstring shaderPath = SampleFrameworkDir() + L"Shaders\\DecodeTextureCS.hlsl";
    convertCS = CompileFromFile(shaderPath.c_str(), "DecodeTextureCS", ShaderType::Compute, opts);
    convertArrayCS = CompileFromFile(shaderPath.c_str(), "DecodeTextureArrayCS", ShaderType::Compute, opts);
    convertCubeCS = CompileFromFile(shaderPath.c_str(), "DecodeTextureCubeCS", ShaderType::Compute, opts);

    {
        D3D12_DESCRIPTOR_RANGE1 uavRanges[1] = {};
        uavRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRanges[0].NumDescriptors = 1;
        uavRanges[0].BaseShaderRegister = 0;
        uavRanges[0].RegisterSpace = 0;
        uavRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER1 rootParameters[NumConvertRootParams] = {};
        rootParameters[ConvertParams_StandardDescriptors].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[ConvertParams_StandardDescriptors].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[ConvertParams_StandardDescriptors].DescriptorTable.pDescriptorRanges = DX12::StandardDescriptorRanges();
        rootParameters[ConvertParams_StandardDescriptors].DescriptorTable.NumDescriptorRanges = DX12::NumStandardDescriptorRanges;

        rootParameters[ConvertParams_UAV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[ConvertParams_UAV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[ConvertParams_UAV].DescriptorTable.pDescriptorRanges = uavRanges;
        rootParameters[ConvertParams_UAV].DescriptorTable.NumDescriptorRanges = ArraySize_(uavRanges);

        rootParameters[ConvertParams_CBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParameters[ConvertParams_CBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[ConvertParams_CBuffer].Constants.Num32BitValues = 3;
        rootParameters[ConvertParams_CBuffer].Constants.RegisterSpace = 0;
        rootParameters[ConvertParams_CBuffer].Constants.ShaderRegister = 0;

        D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
        staticSamplers[0] = DX12::GetStaticSamplerState(SamplerState::Point, 0, 0, D3D12_SHADER_VISIBILITY_ALL);

        D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
        rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.NumStaticSamplers = ArraySize_(staticSamplers);
        rootSignatureDesc.pStaticSamplers = staticSamplers;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        DX12::CreateRootSignature(&convertRootSignature, rootSignatureDesc);
    }

    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = { };
        psoDesc.CS = convertCS.ByteCode();
        psoDesc.pRootSignature = convertRootSignature;
        psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&convertPSO));

        psoDesc.CS = convertArrayCS.ByteCode();
        Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&convertArrayPSO));

        psoDesc.CS = convertCubeCS.ByteCode();
        Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&convertCubePSO));
    }

    convertFence.Init(0);

    // Readback resources
    DXCall(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&readbackCmdAllocator)));
    DXCall(Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, readbackCmdAllocator, nullptr, IID_PPV_ARGS(&readbackCmdList)));
    DXCall(readbackCmdList->Close());
    DXCall(readbackCmdList->Reset(readbackCmdAllocator, nullptr));

    readbackFence.Init(0);
}

void Shutdown_Upload()
{
    for(uint64 i = 0; i < ArraySize_(TempFrameBuffers); ++i)
        Release(TempFrameBuffers[i]);

    Release(UploadBuffer);
    Release(UploadCmdQueue);
    UploadFence.Shutdown();
    for(uint64 i = 0; i < MaxUploadSubmissions; ++i) {
        Release(UploadSubmissions[i].CmdAllocator);
        Release(UploadSubmissions[i].CmdList);
    }

    Release(convertCmdAllocator);
    Release(convertCmdList);
    Release(convertCmdQueue);
    Release(convertPSO);
    Release(convertArrayPSO);
    Release(convertCubePSO);
    Release(convertRootSignature);
    convertFence.Shutdown();

    Release(readbackCmdAllocator);
    Release(readbackCmdList);
    readbackFence.Shutdown();
}

void EndFrame_Upload()
{
    // If we can grab the lock, try to clear out any completed submissions
    if(TryAcquireSRWLockExclusive(&UploadSubmissionLock))
    {
        ClearFinishedUploads(0);

        ReleaseSRWLockExclusive(&UploadSubmissionLock);
    }

    {
        AcquireSRWLockExclusive(&UploadQueueLock);

        // Make sure to sync on any pending uploads
        ClearFinishedUploads(0);
        GfxQueue->Wait(UploadFence.D3DFence, UploadFenceValue);

        ReleaseSRWLockExclusive(&UploadQueueLock);
    }

    TempFrameUsed = 0;
}

void Flush_Upload()
{
    AcquireSRWLockExclusive(&UploadSubmissionLock);

    ClearFinishedUploads(uint64(-1));

    ReleaseSRWLockExclusive(&UploadSubmissionLock);
}

UploadContext ResourceUploadBegin(uint64 size)
{
    Assert_(Device != nullptr);

    size = AlignTo(size, 512);
    Assert_(size <= UploadBufferSize);
    Assert_(size > 0);

    UploadSubmission* submission = nullptr;

    {
        AcquireSRWLockExclusive(&UploadSubmissionLock);

        ClearFinishedUploads(0);

        submission = AllocUploadSubmission(size);
        while(submission == nullptr)
        {
            ClearFinishedUploads(1);
            submission = AllocUploadSubmission(size);
        }

        ReleaseSRWLockExclusive(&UploadSubmissionLock);
    }

    DXCall(submission->CmdAllocator->Reset());
    DXCall(submission->CmdList->Reset(submission->CmdAllocator, nullptr));

    UploadContext context;
    context.CmdList = submission->CmdList;
    context.Resource = UploadBuffer;
    context.CPUAddress = UploadBufferCPUAddr + submission->Offset;
    context.ResourceOffset = submission->Offset;
    context.Submission = submission;

    return context;
}

void ResourceUploadEnd(UploadContext& context)
{
    Assert_(context.CmdList != nullptr);
    Assert_(context.Submission != nullptr);
    UploadSubmission* submission = reinterpret_cast<UploadSubmission*>(context.Submission);

    {
        AcquireSRWLockExclusive(&UploadQueueLock);

        // Finish off and execute the command list
        DXCall(submission->CmdList->Close());
        ID3D12CommandList* cmdLists[1] = { submission->CmdList };
        UploadCmdQueue->ExecuteCommandLists(1, cmdLists);

        ++UploadFenceValue;
        UploadFence.Signal(UploadCmdQueue, UploadFenceValue);
        submission->FenceValue = UploadFenceValue;

        ReleaseSRWLockExclusive(&UploadQueueLock);
    }

    context = UploadContext();
}

MapResult AcquireTempBufferMem(uint64 size, uint64 alignment)
{
    uint64 allocSize = size + alignment;
    uint64 offset = InterlockedAdd64(&TempFrameUsed, allocSize) - allocSize;
    if(alignment > 0)
        offset = AlignTo(offset, alignment);
    Assert_(offset + size <= TempBufferSize);

    MapResult result;
    result.CPUAddress = TempFrameCPUMem[CurrFrameIdx] + offset;
    result.GPUAddress = TempFrameGPUMem[CurrFrameIdx] + offset;
    result.ResourceOffset = offset;
    result.Resource = TempFrameBuffers[CurrFrameIdx];

    return result;
}

void ConvertAndReadbackTexture(const Texture& texture, DXGI_FORMAT outputFormat, ReadbackBuffer& readbackBuffer)
{
    Assert_(convertCmdList != nullptr);
    Assert_(texture.Valid());
    Assert_(texture.Depth == 1);

    // Create a buffer for the CS to write flattened, converted texture data into
    FormattedBufferInit init;
    init.Format = outputFormat;
    init.NumElements = texture.Width * texture.Height * texture.ArraySize;
    init.CreateUAV = true;
    init.InitialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    FormattedBuffer convertBuffer;
    convertBuffer.Initialize(init);

    // Run the conversion compute shader
    DX12::SetDescriptorHeaps(convertCmdList);
    convertCmdList->SetComputeRootSignature(convertRootSignature);

    if(texture.Cubemap)
        convertCmdList->SetPipelineState(convertCubePSO);
    else if(texture.ArraySize > 1)
        convertCmdList->SetPipelineState(convertArrayPSO);
    else
        convertCmdList->SetPipelineState(convertPSO);

    BindStandardDescriptorTable(convertCmdList, ConvertParams_StandardDescriptors, CmdListMode::Compute);

    D3D12_CPU_DESCRIPTOR_HANDLE uavs[] = { convertBuffer.UAV };
    BindTempDescriptorTable(convertCmdList, uavs, ArraySize_(uavs), ConvertParams_UAV, CmdListMode::Compute);

    convertCmdList->SetComputeRoot32BitConstant(ConvertParams_CBuffer, texture.SRV, 0);
    convertCmdList->SetComputeRoot32BitConstant(ConvertParams_CBuffer, uint32(texture.Width), 1);
    convertCmdList->SetComputeRoot32BitConstant(ConvertParams_CBuffer, uint32(texture.Height), 2);

    uint32 dispatchX = DispatchSize(texture.Width, convertTGSize);
    uint32 dispatchY = DispatchSize(texture.Height, convertTGSize);
    uint32 dispatchZ = texture.ArraySize;
    convertCmdList->Dispatch(dispatchX, dispatchY, dispatchZ);

    convertBuffer.Transition(convertCmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

    // Execute the conversion command list and signal a fence
    DXCall(convertCmdList->Close());
    ID3D12CommandList* cmdLists[1] = { convertCmdList };
    convertCmdQueue->ExecuteCommandLists(1, cmdLists);

    convertFence.Signal(convertCmdQueue, 1);

    // Have the readback wait for conversion finish, and then have it copy the data to a readback buffer
    ID3D12CommandQueue* readbackQueue = UploadCmdQueue;
    readbackQueue->Wait(convertFence.D3DFence, 1);

    readbackBuffer.Shutdown();
    readbackBuffer.Initialize(convertBuffer.InternalBuffer.Size);

    readbackCmdList->CopyResource(readbackBuffer.Resource, convertBuffer.InternalBuffer.Resource);

    // Execute the readback command list and signal a fence
    DXCall(readbackCmdList->Close());
    cmdLists[0] = readbackCmdList;
    readbackQueue->ExecuteCommandLists(1, cmdLists);

    readbackFence.Signal(readbackQueue, 1);

    readbackFence.Wait(1);

    // Clean up
    convertFence.Clear(0);
    readbackFence.Clear(0);

    DXCall(convertCmdAllocator->Reset());
    DXCall(convertCmdList->Reset(convertCmdAllocator, nullptr));

    DXCall(readbackCmdAllocator->Reset());
    DXCall(readbackCmdList->Reset(readbackCmdAllocator, nullptr));

    convertBuffer.Shutdown();
}

} // namespace DX12

} // namespace SampleFramework12