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
#include "..\\Timer.h"
#include "..\\Containers.h"
#include "GraphicsTypes.h"

namespace SampleFramework12
{

struct ProfileData;

class Profiler
{

public:

    static Profiler GlobalProfiler;

    void Initialize();
    void Shutdown();

    uint64_t StartProfile(ID3D12GraphicsCommandList* cmdList, const char* name);
    void EndProfile(ID3D12GraphicsCommandList* cmdList, uint64_t idx);

    uint64_t StartCPUProfile(const char* name);
    void EndCPUProfile(uint64_t idx);

    void EndFrame(uint32_t displayWidth, uint32_t displayHeight, uint32_t avgFPS, double avgFrameTime);

    double GPUProfileTiming(const char* name) const;
    double CPUProfileTiming(const char* name) const;

    double GPUProfileTimingAvg(const char* name) const;
    double CPUProfileTimingAvg(const char* name) const;

    void SetAlwaysEnableGPUProfiling(bool enable);

protected:

    Array<ProfileData> profiles;
    Array<ProfileData> cpuProfiles;
    uint64_t numProfiles = 0;
    uint64_t numCPUProfiles = 0;
    Timer timer;
    ID3D12QueryHeap* queryHeap = nullptr;
    ReadbackBuffer readbackBuffers[DX12::RenderLatency];
    bool enableGPUProfiling = false;
    bool showUI = false;
    bool logToClipboard = false;
    bool alwaysEnableGPUProfiling = false;
};

class ProfileBlock
{
public:

    ProfileBlock(ID3D12GraphicsCommandList* cmdList, const char* name);
    ~ProfileBlock();

protected:

    ID3D12GraphicsCommandList* cmdList = nullptr;
    uint64_t idx = uint64_t(-1);
};

class CPUProfileBlock
{
public:

    CPUProfileBlock(const char* name);
    ~CPUProfileBlock();

protected:

    uint64_t idx = uint64_t(-1);
};

}
