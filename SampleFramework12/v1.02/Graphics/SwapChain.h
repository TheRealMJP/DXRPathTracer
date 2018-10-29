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

#include "..\\InterfacePointers.h"
#include "GraphicsTypes.h"
#include "DX12.h"

namespace SampleFramework12
{

class SwapChain
{

public:

    SwapChain();
    ~SwapChain();

    void Initialize(HWND outputWindow);
    void Shutdown();
    void Reset();
    void BeginFrame();
    void EndFrame();

    // Getters
    IDXGISwapChain4* D3DSwapChain() const { return swapChain; };
    const RenderTexture& BackBuffer() const { return backBuffers[backBufferIdx]; };

    DXGI_FORMAT Format() const { return format; };
    uint32 Width() const { return width; };
    uint32 Height() const { return height; };
    bool FullScreen() const { return fullScreen; };
    bool VSYNCEnabled() const { return vsync; };
    uint32 NumVSYNCIntervals() const { return vsync ? numVSYNCIntervals : 0; };
    HANDLE WaitableObject() const { return waitableObject; }

    // Setters
    void SetFormat(DXGI_FORMAT format_) { format = format_; };
    void SetWidth(uint32 width_) { width = width_; };
    void SetHeight(uint32 height_) { height = height_; };
    void SetFullScreen(bool enabled) { fullScreen = enabled; };
    void SetVSYNCEnabled(bool enabled) { vsync = enabled; };
    void SetNumVSYNCIntervals(uint32 intervals) { numVSYNCIntervals = intervals; };

protected:

    void CheckForSuitableOutput();
    void AfterReset();
    void PrepareFullScreenSettings();

    static const uint64 NumBackBuffers = 2;

    IDXGISwapChain4* swapChain = nullptr;
    uint32 backBufferIdx = 0;
    RenderTexture backBuffers[NumBackBuffers];
    HANDLE waitableObject = INVALID_HANDLE_VALUE;

    IDXGIOutput* output = nullptr;

    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT noSRGBFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    uint32 width = 1280;
    uint32 height = 720;
    bool fullScreen = false;
    bool vsync = true;
    DXGI_RATIONAL refreshRate = { };
    uint32 numVSYNCIntervals = 1;
};

}