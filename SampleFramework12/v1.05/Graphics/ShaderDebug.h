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

struct Float4x4;

namespace ShaderDebug
{
    void Initialize();
    void Shutdown();

    void CreatePSOs(DXGI_FORMAT rtFormat, DXGI_FORMAT dsvFormat);
    void DestroyPSOs();

    void BeginRender(ID3D12GraphicsCommandList7* cmdList, uint32_t cursorX, uint32_t cursorY);
    void EndRender(ID3D12GraphicsCommandList7* cmdList, const Float4x4& viewProjection);
}

}