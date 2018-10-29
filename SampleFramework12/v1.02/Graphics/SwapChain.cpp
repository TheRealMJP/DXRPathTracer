//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
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
    refreshRate.Numerator = 60;
    refreshRate.Denominator = 1;

    // Try to figure out if we should default to 1280x720 or 1920x1080
    POINT point;
    point.x = 0;
    point.y = 0;
    HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTOPRIMARY);
    if(monitor != 0)
    {
        MONITORINFOEX info;
        ZeroMemory(&info, sizeof(info));
        info.cbSize = sizeof(MONITORINFOEX);
        if(GetMonitorInfo(monitor, &info) != 0)
        {
            int32 monitorWidth = info.rcWork.right - info.rcWork.left;
            int32 monitorHeight = info.rcWork.bottom - info.rcWork.top;
            if(monitorWidth > 1920 && monitorHeight > 1080)
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

SwapChain::~SwapChain()
{
    Assert_(swapChain == nullptr);
    Shutdown();
}

void SwapChain::Initialize(HWND outputWindow)
{
    Shutdown();

    // We'll just use the first output for fullscreen
    DX12::Adapter->EnumOutputs(0, &output);

    if(format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
        noSRGBFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    else if(format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
        noSRGBFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    else
        noSRGBFormat = format;

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = uint32(NumBackBuffers);
    swapChainDesc.BufferDesc.Width = width;
    swapChainDesc.BufferDesc.Height = height;
    swapChainDesc.BufferDesc.Format = noSRGBFormat;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.OutputWindow = outputWindow;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH |
                          DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING |
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
    for(uint64 i = 0; i < NumBackBuffers; ++i)
    {
        DX12::Release(backBuffers[i].Texture.Resource);
        DX12::RTVDescriptorHeap.FreePersistent(backBuffers[i].RTV);
    }

    DX12::Release(swapChain);
    DX12::Release(output);
}

void SwapChain::AfterReset()
{
    // Re-create an RTV for each back buffer
    for(uint64 i = 0; i < NumBackBuffers; i++)
    {
        backBuffers[i].RTV = DX12::RTVDescriptorHeap.AllocatePersistent().Handles[0];
        DXCall(swapChain->GetBuffer(uint32(i), IID_PPV_ARGS(&backBuffers[i].Texture.Resource)));

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

void SwapChain::CheckForSuitableOutput()
{

}

void SwapChain::PrepareFullScreenSettings()
{
    Assert_(output);

    // Have the Output look for the closest matching mode
    DXGI_MODE_DESC desiredMode;
    desiredMode.Format = noSRGBFormat;
    desiredMode.Width = width;
    desiredMode.Height = height;
    desiredMode.RefreshRate.Numerator = 0;
    desiredMode.RefreshRate.Denominator = 0;
    desiredMode.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    desiredMode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;

    DXGI_MODE_DESC closestMatch;
    DXCall(output->FindClosestMatchingMode(&desiredMode, &closestMatch, DX12::Device));

    width = closestMatch.Width;
    height = closestMatch.Height;
    refreshRate = closestMatch.RefreshRate;
}

void SwapChain::Reset()
{
    Assert_(swapChain);
    if(output == nullptr)
        fullScreen = false;

    // Release all references
    for(uint64 i = 0; i < NumBackBuffers; ++i)
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

    if(fullScreen)
        PrepareFullScreenSettings();
    else
    {
        refreshRate.Numerator = 60;
        refreshRate.Denominator = 1;
    }

    DXCall(swapChain->SetFullscreenState(fullScreen, NULL));

    DXCall(swapChain->ResizeBuffers(NumBackBuffers, width, height, noSRGBFormat,
                                    DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH |
                                    DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING |
                                    DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));

    if(fullScreen)
    {
        DXGI_MODE_DESC mode;
        mode.Format = noSRGBFormat;
        mode.Width = width;
        mode.Height = height;
        mode.RefreshRate.Numerator = 0;
        mode.RefreshRate.Denominator = 0;
        mode.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        mode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        DXCall(swapChain->ResizeTarget(&mode));
    }

    AfterReset();
}

void SwapChain::BeginFrame()
{
    backBufferIdx = swapChain->GetCurrentBackBufferIndex();

    // Indicate that the back buffer will be used as a render target.
    DX12::TransitionResource(DX12::CmdList, backBuffers[backBufferIdx].Texture.Resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void SwapChain::EndFrame()
{
    // Indicate that the back buffer will now be used to present.
    DX12::TransitionResource(DX12::CmdList, backBuffers[backBufferIdx].Texture.Resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
}

}