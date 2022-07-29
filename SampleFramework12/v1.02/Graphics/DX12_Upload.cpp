//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "DX12_Upload.h"
#include "DX12.h"
#include "GraphicsTypes.h"

namespace SampleFramework12
{

namespace DX12
{

// Wraps the copy queue that we will submit to when uploading resource data
struct UploadQueue
{
    ID3D12CommandQueue* CmdQueue = nullptr;
    Fence Fence;
    uint64 FenceValue = 0;
    uint64 WaitCount = 0;

    SRWLOCK Lock = SRWLOCK_INIT;

    void Init(const wchar* name)
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = { };
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        DXCall(Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&CmdQueue)));
        CmdQueue->SetName(name);

        Fence.Init(0);
    }

    void Shutdown()
    {
        Fence.Shutdown();
        Release(CmdQueue);
    }

    void SyncDependentQueue(ID3D12CommandQueue* otherQueue)
    {
        AcquireSRWLockExclusive(&Lock);

        if(WaitCount > 0)
        {
            // Tell the other queue to wait for any pending uploads to finish before running
            otherQueue->Wait(Fence.D3DFence, FenceValue);

            WaitCount = 0;
        }

        ReleaseSRWLockExclusive(&Lock);
    }

    uint64 SubmitCmdList(ID3D12GraphicsCommandList* cmdList, bool syncOnDependentQueue)
    {
        AcquireSRWLockExclusive(&Lock);

        ID3D12CommandList* cmdLists[1] = { cmdList };
        CmdQueue->ExecuteCommandLists(1, cmdLists);

        const uint64 newFenceValue = ++FenceValue;
        Fence.Signal(CmdQueue, newFenceValue);

        if(syncOnDependentQueue)
            WaitCount += 1;

        ReleaseSRWLockExclusive(&Lock);

        return newFenceValue;
    }

    void Flush()
    {
        AcquireSRWLockExclusive(&Lock);

        // Wait for all pending submissions on the queue to finish
        Fence.Wait(FenceValue);

        ReleaseSRWLockExclusive(&Lock);
    }
};

// A single submission that goes through the upload ring buffer
struct UploadSubmission
{
    ID3D12CommandAllocator* CmdAllocator = nullptr;
    ID3D12GraphicsCommandList5* CmdList = nullptr;
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

struct UploadRingBuffer
{
    // A ring buffer of pending submissions
    static const uint64 MaxSubmissions = 16;
    UploadSubmission Submissions[MaxSubmissions];
    uint64 SubmissionStart = 0;
    uint64 SubmissionUsed = 0;

    // CPU-writable UPLOAD buffer
    uint64 BufferSize = 64 * 1024 * 1024;
    ID3D12Resource* Buffer = nullptr;
    uint8* BufferCPUAddr = nullptr;

    // For using the UPLOAD buffer as a ring buffer
    uint64 BufferStart = 0;
    uint64 BufferUsed = 0;

    // Thread safety
    SRWLOCK Lock = SRWLOCK_INIT;

    // The queue for submitting on
    UploadQueue* submitQueue = nullptr;

    void Init(UploadQueue* queue)
    {
        Assert_(queue != nullptr);
        submitQueue = queue;

        for(uint64 i = 0; i < MaxSubmissions; ++i) {
            UploadSubmission& submission = Submissions[i];
            DXCall(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&submission.CmdAllocator)));
            DXCall(Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, submission.CmdAllocator, nullptr, IID_PPV_ARGS(&submission.CmdList)));
            DXCall(submission.CmdList->Close());

            submission.CmdList->SetName(L"Upload Command List");
        }

        Resize(BufferSize);
    }

    void Shutdown()
    {
        Release(Buffer);
        for(uint64 i = 0; i < MaxSubmissions; ++i) {
            Release(Submissions[i].CmdAllocator);
            Release(Submissions[i].CmdList);
        }
    }

    void Resize(uint64 newBufferSize)
    {
        Release(Buffer);

        BufferSize = newBufferSize;

        D3D12_RESOURCE_DESC resourceDesc = { };
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = BufferSize;
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
                                               D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&Buffer)));

        D3D12_RANGE readRange = { };
        DXCall(Buffer->Map(0, &readRange, reinterpret_cast<void**>(&BufferCPUAddr)));
    }

    void ClearPendingUploads(uint64 waitCount)
    {
        const uint64 start = SubmissionStart;
        const uint64 used = SubmissionUsed;
        for(uint64 i = 0; i < used; ++i)
        {
            const uint64 idx = (start + i) % MaxSubmissions;
            UploadSubmission& submission = Submissions[idx];
            Assert_(submission.Size > 0);
            Assert_(BufferUsed >= submission.Size);

            // If the submission hasn't been sent to the GPU yet we can't wait for it
            if(submission.FenceValue == uint64(-1))
                break;

            ID3D12Fence* submitFence = submitQueue->Fence.D3DFence;

            if(i < waitCount)
                submitFence->SetEventOnCompletion(submission.FenceValue, NULL);

            if(submitFence->GetCompletedValue() >= submission.FenceValue)
            {
                SubmissionStart = (SubmissionStart + 1) % MaxSubmissions;
                SubmissionUsed -= 1;
                BufferStart = (BufferStart + submission.Padding) % BufferSize;
                Assert_(submission.Offset == BufferStart);
                Assert_(BufferStart + submission.Size <= BufferSize);
                BufferStart = (BufferStart + submission.Size) % BufferSize;
                BufferUsed -= (submission.Size + submission.Padding);
                submission.Reset();

                if(BufferUsed == 0)
                    BufferStart = 0;
            }
            else
            {
                // We don't want to retire our submissions out of allocation order, because
                // the ring buffer logic above will move the tail position forward (we don't
                // allow holes in the ring buffer). Submitting out-of-order should still be
                // ok though as long as we retire in-order.
                break;
            }
        }
    }

    void Flush()
    {
        AcquireSRWLockExclusive(&Lock);

        while(SubmissionUsed > 0)
            ClearPendingUploads(uint64(-1));

        ReleaseSRWLockExclusive(&Lock);
    }

    void TryClearPending()
    {
        // See if we can clear out any pending submissions, but only if we can grab the lock
        if(TryAcquireSRWLockExclusive(&Lock))
        {
            ClearPendingUploads(0);

            ReleaseSRWLockExclusive(&Lock);
        }
    }

    UploadSubmission* AllocSubmission(uint64 size)
    {
        Assert_(SubmissionUsed <= MaxSubmissions);
        if(SubmissionUsed == MaxSubmissions)
            return nullptr;

        const uint64 submissionIdx = (SubmissionStart + SubmissionUsed) % MaxSubmissions;
        Assert_(Submissions[submissionIdx].Size == 0);

        Assert_(BufferUsed <= BufferSize);
        if(size > (BufferSize - BufferUsed))
            return nullptr;

        const uint64 start = BufferStart;
        const uint64 end = BufferStart + BufferUsed;
        uint64 allocOffset = uint64(-1);
        uint64 padding = 0;
        if(end < BufferSize)
        {
            const uint64 endAmt = BufferSize - end;
            if(endAmt >= size)
            {
                allocOffset = end;
            }
            else if(start >= size)
            {
                // Wrap around to the beginning
                allocOffset = 0;
                BufferUsed += endAmt;
                padding = endAmt;
            }
        }
        else
        {
            const uint64 wrappedEnd = end % BufferSize;
            if((start - wrappedEnd) >= size)
                allocOffset = wrappedEnd;
        }

        if(allocOffset == uint64(-1))
            return nullptr;

        SubmissionUsed += 1;
        BufferUsed += size;

        UploadSubmission* submission = &Submissions[submissionIdx];
        submission->Offset = allocOffset;
        submission->Size = size;
        submission->FenceValue = uint64(-1);
        submission->Padding = padding;

        return submission;
    }

    UploadContext Begin(uint64 size)
    {
        Assert_(Device != nullptr);

        Assert_(size > 0);
        size = AlignTo(size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

        if(size > BufferSize)
        {
            // Resize the ring buffer so that it's big enough
            AcquireSRWLockExclusive(&Lock);

            while(SubmissionUsed > 0)
                ClearPendingUploads(uint64(-1));

            Resize(size);

            ReleaseSRWLockExclusive(&Lock);
        }

        UploadSubmission* submission = nullptr;

        {
            AcquireSRWLockExclusive(&Lock);

            ClearPendingUploads(0);

            submission = AllocSubmission(size);
            while(submission == nullptr)
            {
                ClearPendingUploads(1);
                submission = AllocSubmission(size);
            }

            ReleaseSRWLockExclusive(&Lock);
        }

        DXCall(submission->CmdAllocator->Reset());
        DXCall(submission->CmdList->Reset(submission->CmdAllocator, nullptr));

        UploadContext context;
        context.CmdList = submission->CmdList;
        context.Resource = Buffer;
        context.CPUAddress = BufferCPUAddr + submission->Offset;
        context.ResourceOffset = submission->Offset;
        context.Submission = submission;

        return context;
    }

    void End(UploadContext& context, bool syncOnDependentQueue)
    {
        Assert_(context.CmdList != nullptr);
        Assert_(context.Submission != nullptr);
        UploadSubmission* submission = reinterpret_cast<UploadSubmission*>(context.Submission);

        // Kick off the copy command
        DXCall(submission->CmdList->Close());
        submission->FenceValue = submitQueue->SubmitCmdList(submission->CmdList, syncOnDependentQueue);

        context = UploadContext();
    }
};

static UploadQueue uploadQueue;
static UploadRingBuffer uploadRingBuffer;

// Per-frame temporary upload buffer resources
static const uint64 TempBufferSize = 2 * 1024 * 1024;
static ID3D12Resource* TempFrameBuffers[RenderLatency] = { };
static uint8* TempFrameCPUMem[RenderLatency] = { };
static uint64 TempFrameGPUMem[RenderLatency] = { };
static int64 TempFrameUsed = 0;

// Resources for doing fast uploads while generating render commands
struct FastUpload
{
    ID3D12Resource* SrcBuffer = nullptr;
    uint64 SrcOffset = 0;
    ID3D12Resource* DstBuffer = nullptr;
    uint64 DstOffset = 0;
    uint64 CopySize = 0;
};

struct FastUploader
{
    ID3D12GraphicsCommandList5* CmdList = nullptr;
    ID3D12CommandAllocator* CmdAllocators[RenderLatency] = { };
    uint64 CmdAllocatorIdx = 0;

    static const uint64 MaxFastUploads = 256;
    FastUpload Uploads[MaxFastUploads];
    int64 NumUploads = 0;

    void Init()
    {
        CmdAllocatorIdx = 0;

        for(uint32 i = 0; i < RenderLatency; ++i)
        {
            DXCall(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&CmdAllocators[i])));
            CmdAllocators[i]->SetName(L"Fast Uploader Command Allocator");
        }

        DXCall(Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, CmdAllocators[0], nullptr, IID_PPV_ARGS(&CmdList)));
        DXCall(CmdList->Close());
        CmdList->SetName(L"Fast Upload Command List");
    }

    void Shutdown()
    {
        Assert_(NumUploads == 0);
        for(uint32 i = 0; i < RenderLatency; ++i)
            Release(CmdAllocators[i]);
        Release(CmdList);
    }

    void QueueUpload(FastUpload upload)
    {
        const int64 idx = InterlockedIncrement64(&NumUploads) - 1;
        Assert_(idx < MaxFastUploads);
        Uploads[idx] = upload;
    }

    void SubmitPending(UploadQueue& queue)
    {
        if(NumUploads == 0)
            return;

        CmdAllocators[CmdAllocatorIdx]->Reset();
        CmdList->Reset(CmdAllocators[CmdAllocatorIdx], nullptr);

        for(int64 uploadIdx = 0; uploadIdx < NumUploads; ++uploadIdx)
        {
            const FastUpload& upload = Uploads[uploadIdx];
            CmdList->CopyBufferRegion(upload.DstBuffer, upload.DstOffset, upload.SrcBuffer, upload.SrcOffset, upload.CopySize);
        }

        CmdList->Close();

        queue.SubmitCmdList(CmdList, true);

        NumUploads = 0;
        CmdAllocatorIdx = (CmdAllocatorIdx + 1) % RenderLatency;
    }
};

static UploadQueue fastUploadQueue;
static FastUploader fastUploader;

void Initialize_Upload()
{
    uploadQueue.Init(L"Upload Queue");
    uploadRingBuffer.Init(&uploadQueue);

    fastUploadQueue.Init(L"Fast Upload Queue");
    fastUploader.Init();

    // Temporary buffer memory that swaps every frame
    D3D12_RESOURCE_DESC resourceDesc = { };
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = uint32(TempBufferSize);
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Alignment = 0;

    for(uint64 i = 0; i < RenderLatency; ++i)
    {
        DXCall(Device->CreateCommittedResource(DX12::GetUploadHeapProps(), D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                               D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&TempFrameBuffers[i])));

        D3D12_RANGE readRange = { };
        DXCall(TempFrameBuffers[i]->Map(0, &readRange, reinterpret_cast<void**>(&TempFrameCPUMem[i])));
        TempFrameGPUMem[i] = TempFrameBuffers[i]->GetGPUVirtualAddress();
    }
}

void Shutdown_Upload()
{
    uploadQueue.Shutdown();
    uploadRingBuffer.Shutdown();
    fastUploader.Shutdown();
    fastUploadQueue.Shutdown();

    for(uint64 i = 0; i < ArraySize_(TempFrameBuffers); ++i)
        Release(TempFrameBuffers[i]);
}

void EndFrame_Upload()
{
    // Kick off any queued "fast" uploads
    fastUploader.SubmitPending(fastUploadQueue);

    uploadRingBuffer.TryClearPending();

    // Make sure that the graphics queue waits for any pending uploads that have been submitted.
    uploadQueue.SyncDependentQueue(GfxQueue);
    fastUploadQueue.SyncDependentQueue(GfxQueue);

    TempFrameUsed = 0;
}

void Flush_Upload()
{
    uploadQueue.Flush();
    uploadRingBuffer.Flush();
    fastUploadQueue.Flush();
}

UploadContext ResourceUploadBegin(uint64 size)
{
   return uploadRingBuffer.Begin(size);
}

void ResourceUploadEnd(UploadContext& context, bool syncOnGraphicsQueue)
{
    uploadRingBuffer.End(context, syncOnGraphicsQueue);
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

void QueueFastUpload(ID3D12Resource* srcBuffer, uint64 srcOffset, ID3D12Resource* dstBuffer, uint64 dstOffset, uint64 copySize)
{
    FastUpload upload = { .SrcBuffer = srcBuffer, .SrcOffset = srcOffset, .DstBuffer = dstBuffer, .DstOffset = dstOffset, .CopySize = copySize };
    fastUploader.QueueUpload(upload);
}

} // namespace DX12

} // namespace SampleFramework12