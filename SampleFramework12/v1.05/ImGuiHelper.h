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
#include "SF12_Math.h"
#include "ImGui/imgui.h"

namespace SampleFramework12
{

class Window;

namespace ImGuiHelper
{

void Initialize(Window& window, float dpiScale);
void Shutdown();

void CreatePSOs(DXGI_FORMAT rtFormat);
void DestroyPSOs();

void BeginFrame(uint32_t displayWidth, uint32_t displayHeight, float timeDelta, float dpiScale);
void EndFrame(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
              uint32_t displayWidth, uint32_t displayHeight);

} // namespace ImGuiHelper

inline ImVec2 ToImVec2(Float2 v)
{
    return ImVec2(v.x, v.y);
}

inline Float2 ToFloat2(ImVec2 v)
{
    return Float2(v.x, v.y);
}

inline ImColor ToImColor(Float3 v)
{
    return ImColor(v.x, v.y, v.z);
}

inline ImColor ToImColor(Float4 v)
{
    return ImColor(v.x, v.y, v.z, v.w);
}

inline ImTextureID ToImTextureID(uint32_t srvIndex, uint32_t samplerMode = 0)
{
    return uint64_t(srvIndex) | (uint64_t(samplerMode) << 32ull);
}

} // namespace SampleFramework12
