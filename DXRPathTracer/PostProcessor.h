//=================================================================================================
//
//  DXR Path Tracer
//  by MJP
//  http://mynameismjp.wordpress.com/
//
//  All code and content licensed under the MIT license
//
//=================================================================================================

#pragma once

#include <PCH.h>
#include <Graphics/GraphicsTypes.h>
#include <Graphics/PostProcessHelper.h>

#include "AppSettings.h"

using namespace SampleFramework12;

class PostProcessor
{

public:

    void Initialize();
    void Shutdown();

    void CreatePSOs();
    void DestroyPSOs();

    void Render(ID3D12GraphicsCommandList* cmdList, const RenderTexture& input, const RenderTexture& output);

protected:

    TempRenderTarget* Bloom(ID3D12GraphicsCommandList* cmdList, const RenderTexture& input);

    PostProcessHelper helper;

    CompiledShaderPtr toneMap;
    CompiledShaderPtr scale;
    CompiledShaderPtr bloom;
    CompiledShaderPtr blurH;
    CompiledShaderPtr blurV;
};