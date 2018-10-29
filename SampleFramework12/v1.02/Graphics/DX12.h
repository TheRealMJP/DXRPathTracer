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

namespace SampleFramework12
{

namespace DX12
{

// Constants
const uint64 RenderLatency = 2;

// Externals
extern ID3D12Device5* Device;
extern ID3D12GraphicsCommandList4* CmdList;
extern ID3D12CommandQueue* GfxQueue;
extern D3D_FEATURE_LEVEL FeatureLevel;
extern IDXGIFactory4* Factory;
extern IDXGIAdapter1* Adapter;

extern uint64 CurrentCPUFrame;  // Total number of CPU frames completed (completed means all command buffers submitted to the GPU)
extern uint64 CurrentGPUFrame;  // Total number of GPU frames completed (completed means that the GPU signals the fence)
extern uint64 CurrFrameIdx;     // CurrentCPUFrame % RenderLatency

// Lifetime
void Initialize(D3D_FEATURE_LEVEL minFeatureLevel, uint32 adapterIdx);
void Shutdown();

// Frame submission synchronization
void BeginFrame();
void EndFrame(IDXGISwapChain4* swapChain, uint32 syncIntervals);
void FlushGPU();

void DeferredRelease_(IUnknown* resource, bool forceDeferred = false);

template<typename T> void DeferredRelease(T*& resource, bool forceDeferred = false)
{
    IUnknown* base = resource;
    DeferredRelease_(base, forceDeferred);
    resource = nullptr;
}

template<typename T> void Release(T*& resource)
{
    if(resource != nullptr) {
        resource->Release();
        resource = nullptr;
    }
}

void DeferredCreateSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, uint32 descriptorIdx);

} // namespace DX12

} // namespace SampleFramework12


