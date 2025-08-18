//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "SwapChain.h"
#include "DX12.h"
#include "..\\Exceptions.h"
#include "..\\Utility.h"

using std::wstring;

namespace SampleFramework12
{

SwapChain::SwapChain()
{
}

SwapChain::~SwapChain()
{
    Assert_(swapChain == nullptr);
    Shutdown();
}

void SwapChain::ChooseDefaultResolution(HWND outputWindow, float dpiScale)
{
    HMONITOR monitor = MonitorFromWindow(outputWindow, MONITOR_DEFAULTTONEAREST);
    if(monitor != 0)
    {
        MONITORINFOEX info;
        ZeroMemory(&info, sizeof(info));
        info.cbSize = sizeof(MONITORINFOEX);
        if(GetMonitorInfo(monitor, &info) != 0)
        {
            int32_t monitorWidth = int32_t((info.rcWork.right - info.rcWork.left) * dpiScale);
            int32_t monitorHeight = int32_t((info.rcWork.bottom - info.rcWork.top) * dpiScale);
            if(monitorWidth > 2560 && monitorHeight > 1440)
            {
                width = 2560;
                height = 1440;
            }
            else if(monitorWidth > 1920 && monitorHeight > 1080)
            {
                width = 1920;
                height = 1080;
            }
            else if(monitorWidth > 1536 && monitorHeight > 864)
            {
                width = 1536;
                height = 864;
            }
        }
    }
}

void SwapChain::Initialize(HWND outputWindow)
{
    Shutdown();

    if(format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
        noSRGBFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    else if(format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
        noSRGBFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    else
        noSRGBFormat = format;

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = uint32_t(NumBackBuffers);
    swapChainDesc.BufferDesc.Width = width;
    swapChainDesc.BufferDesc.Height = height;
    swapChainDesc.BufferDesc.Format = noSRGBFormat;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.OutputWindow = outputWindow;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING |
                          DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    IDXGISwapChain* tempSwapChain = nullptr;
    DXCall(DX12::Factory->CreateSwapChain(DX12::GfxQueue, &swapChainDesc, &tempSwapChain));
    DXCall(tempSwapChain->QueryInterface(IID_PPV_ARGS(&swapChain)));
    DX12::Release(tempSwapChain);

    backBufferIdx = swapChain->GetCurrentBackBufferIndex();
    waitableObject = swapChain->GetFrameLatencyWaitableObject();

    AfterReset();
}

void SwapChain::Shutdown()
{
    for(uint64_t i = 0; i < NumBackBuffers; ++i)
    {
        DX12::Release(backBuffers[i].Texture.Resource);
        DX12::RTVDescriptorHeap.FreePersistent(backBuffers[i].RTV);
    }

    DX12::Release(swapChain);
}

void SwapChain::AfterReset()
{
    // Re-create an RTV for each back buffer
    for(uint64_t i = 0; i < NumBackBuffers; i++)
    {
        backBuffers[i].RTV = DX12::RTVDescriptorHeap.AllocatePersistent().Handles[0];
        DXCall(swapChain->GetBuffer(uint32_t(i), IID_PPV_ARGS(&backBuffers[i].Texture.Resource)));

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = { };
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Format = format;
        rtvDesc.Texture2D.MipSlice = 0;
        rtvDesc.Texture2D.PlaneSlice = 0;
        DX12::Device->CreateRenderTargetView(backBuffers[i].Texture.Resource, &rtvDesc, backBuffers[i].RTV);

        backBuffers[i].Texture.Resource->SetName(MakeString(L"Back Buffer %llu", i).c_str());

        backBuffers[i].Texture.Width = width;
        backBuffers[i].Texture.Height = height;
        backBuffers[i].Texture.ArraySize = 1;
        backBuffers[i].Texture.Format = format;
        backBuffers[i].Texture.NumMips = 1;
        backBuffers[i].MSAASamples = 1;
    }

    backBufferIdx = swapChain->GetCurrentBackBufferIndex();
}

void SwapChain::Reset()
{
    Assert_(swapChain);

    // Release all references
    for(uint64_t i = 0; i < NumBackBuffers; ++i)
    {
        DX12::Release(backBuffers[i].Texture.Resource);
        DX12::RTVDescriptorHeap.FreePersistent(backBuffers[i].RTV);
    }

    if(format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
        noSRGBFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    else if(format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
        noSRGBFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    else
        noSRGBFormat = format;

    DXCall(swapChain->ResizeBuffers(NumBackBuffers, width, height, noSRGBFormat,
                                    DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING |
                                    DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));

    AfterReset();
}

void SwapChain::BeginFrame()
{
    backBufferIdx = swapChain->GetCurrentBackBufferIndex();

    // Indicate that the back buffer will be used as a render target.
    D3D12_TEXTURE_BARRIER barrier =
    {
        .SyncBefore = D3D12_BARRIER_SYNC_NONE,
        .SyncAfter = D3D12_BARRIER_SYNC_RENDER_TARGET,
        .AccessBefore = D3D12_BARRIER_ACCESS_NO_ACCESS,
        .AccessAfter = D3D12_BARRIER_ACCESS_RENDER_TARGET,
        .LayoutBefore = D3D12_BARRIER_LAYOUT_UNDEFINED,
        .LayoutAfter = D3D12_BARRIER_LAYOUT_RENDER_TARGET,
        .pResource = backBuffers[backBufferIdx].Texture.Resource,
        .Subresources = { },
        .Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE,
    };
    DX12::Barrier(DX12::CmdList, barrier);
}

void SwapChain::EndFrame()
{
    // Indicate that the back buffer will now be used to present.
    D3D12_TEXTURE_BARRIER barrier =
    {
        .SyncBefore = D3D12_BARRIER_SYNC_RENDER_TARGET,
        .SyncAfter = D3D12_BARRIER_SYNC_NONE,
        .AccessBefore = D3D12_BARRIER_ACCESS_RENDER_TARGET,
        .AccessAfter = D3D12_BARRIER_ACCESS_NO_ACCESS,
        .LayoutBefore = D3D12_BARRIER_LAYOUT_RENDER_TARGET,
        .LayoutAfter = D3D12_BARRIER_LAYOUT_PRESENT,
        .pResource = backBuffers[backBufferIdx].Texture.Resource,
        .Subresources = { },
        .Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE,
    };
    DX12::Barrier(DX12::CmdList, barrier);
}

}