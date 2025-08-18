//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "PCH.h"

#include "Window.h"
#include "Graphics\\SwapChain.h"
#include "Timer.h"

namespace SampleFramework12
{

class App
{

public:

    App(const char* appName, const char* cmdLine);
    virtual ~App();

    int32_t Run();

protected:

    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;

    virtual void Update(const Timer& timer) = 0;
    virtual void Render(const Timer& timer) = 0;

    virtual void BeforeReset() = 0;
    virtual void AfterReset() = 0;

    virtual void CreatePSOs() = 0;
    virtual void DestroyPSOs() = 0;

    virtual void BeforeFlush();

    void Exit();
    void CalculateFPS();

    static void OnWindowResized(void* context, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    Window window;
    SwapChain swapChain;
    Timer appTimer;

    static const uint32_t NumTimeDeltaSamples = 64;
    float timeDeltaBuffer[NumTimeDeltaSamples] = { };
    uint32_t currentTimeDeltaSample = 0;
    double avgFrameTime = 0.0;
    uint32_t avgFPS = 0;

    std::string applicationName;

    bool showWindow = true;
    int32_t returnCode = 0;
    D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_12_1;
    uint32_t adapterIdx = 0;

    Float4x4 appViewMatrix;

    static const uint64_t MaxLogMessages = 1024;
    std::string logMessages[MaxLogMessages];
    volatile int64_t numLogMessages = 0;
    bool showLog = false;
    bool newLogMessage = false;


    std::string logMessagesGPU[MaxLogMessages];
    uint64_t numLogMessagesGPU = 0;
    bool newLogMessageGPU = false;
    bool clearGPULogEveryFrame = true;
    bool pauseGPULog = false;
    bool autoShowGPULog = true;

    bool showGUI = true;

    float dpiScale = 1.0f;

private:

    void ParseCommandLine(const char* cmdLine);

    void Initialize_Internal();
    void Shutdown_Internal();

    void Update_Internal();
    void Render_Internal();

    void BeforeReset_Internal();
    void AfterReset_Internal();

    void CreatePSOs_Internal();
    void DestroyPSOs_Internal();

    void DrawLog();

public:

    // Accessors
    Window& Window() { return window; }
    SwapChain& SwapChain() { return swapChain; }    

    void AddToLog(const char* msg);
    void AddToGPULog(const char* msg);
};

extern App* GlobalApp;

}