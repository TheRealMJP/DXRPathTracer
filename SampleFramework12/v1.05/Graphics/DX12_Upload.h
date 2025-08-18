//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "..\\PCH.h"

namespace SampleFramework12
{

struct MapResult
{
    void* CPUAddress = nullptr;
    uint64_t GPUAddress = 0;
    uint64_t ResourceOffset = 0;
    ID3D12Resource* Resource = nullptr;
};

struct UploadContext
{
    ID3D12GraphicsCommandList* CmdList;
    void* CPUAddress = nullptr;
    uint64_t ResourceOffset = 0;
    ID3D12Resource* Resource = nullptr;
    void* Submission = nullptr;
};

struct ReadbackBuffer;
struct Texture;

namespace DX12
{

void Initialize_Upload();
void Shutdown_Upload();

void EndFrame_Upload();

void Flush_Upload();

// Resource upload/init
UploadContext ResourceUploadBegin(uint64_t size);
void ResourceUploadEnd(UploadContext& context, bool syncOnGraphicsQueue = true);

// Temporary CPU-writable buffer memory
MapResult AcquireTempBufferMem(uint64_t size, uint64_t alignment);

// Fast in-frame upload path through the copy queue
void QueueFastUpload(ID3D12Resource* srcBuffer, uint64_t srcOffset, ID3D12Resource* dstBuffer, uint64_t dstOffset, uint64_t copySize);

}

}