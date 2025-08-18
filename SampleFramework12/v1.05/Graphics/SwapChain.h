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

    void ChooseDefaultResolution(HWND outputWindow, float dpiScale);
    void Initialize(HWND outputWindow);
    void Shutdown();
    void Reset();
    void BeginFrame();
    void EndFrame();

    // Getters
    IDXGISwapChain4* D3DSwapChain() const { return swapChain; };
    const RenderTexture& BackBuffer() const { return backBuffers[backBufferIdx]; };

    DXGI_FORMAT Format() const { return format; };
    uint32_t Width() const { return width; };
    uint32_t Height() const { return height; };
    bool VSYNCEnabled() const { return vsync; };
    uint32_t NumVSYNCIntervals() const { return vsync ? numVSYNCIntervals : 0; };
    HANDLE WaitableObject() const { return waitableObject; }

    // Setters
    void SetFormat(DXGI_FORMAT format_) { format = format_; };
    void SetWidth(uint32_t width_) { width = width_; };
    void SetHeight(uint32_t height_) { height = height_; };
    void SetVSYNCEnabled(bool enabled) { vsync = enabled; };
    void SetNumVSYNCIntervals(uint32_t intervals) { numVSYNCIntervals = intervals; };

protected:

    void AfterReset();

    static const uint64_t NumBackBuffers = 2;

    IDXGISwapChain4* swapChain = nullptr;
    uint32_t backBufferIdx = 0;
    RenderTexture backBuffers[NumBackBuffers];
    HANDLE waitableObject = INVALID_HANDLE_VALUE;

    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT noSRGBFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    uint32_t width = 1280;
    uint32_t height = 720;
    bool vsync = true;
    uint32_t numVSYNCIntervals = 1;
};

}