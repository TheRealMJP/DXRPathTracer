//=================================================================================================
//
//  DXR Path Tracer
//  by MJP
//  http://mynameismjp.wordpress.com/
//
//  All code and content licensed under the MIT license
//
//=================================================================================================

#include <PCH.h>

#include <Graphics/ShaderCompilation.h>

#include "PostProcessor.h"

void PostProcessor::Initialize()
{
    helper.Initialize();

    // Load the shaders
    toneMap = CompileFromFile(L"PostProcessing.hlsl", "ToneMap", ShaderType::Pixel);
    scale = CompileFromFile(L"PostProcessing.hlsl", "Scale", ShaderType::Pixel);
    blurH = CompileFromFile(L"PostProcessing.hlsl", "BlurH", ShaderType::Pixel);
    blurV = CompileFromFile(L"PostProcessing.hlsl", "BlurV", ShaderType::Pixel);
    bloom = CompileFromFile(L"PostProcessing.hlsl", "Bloom", ShaderType::Pixel);
}

void PostProcessor::Shutdown()
{
    helper.Shutdown();
}

void PostProcessor::CreatePSOs()
{
}

void PostProcessor::DestroyPSOs()
{
    helper.ClearCache();
}

void PostProcessor::Render(ID3D12GraphicsCommandList* cmdList, const RenderTexture& input, const RenderTexture& output)
{
    helper.Begin(cmdList);

    TempRenderTarget* bloomTarget = Bloom(cmdList, input);

    // Apply tone mapping
    uint32 inputs[2] = { input.SRV(), bloomTarget->RT.SRV() };
    const RenderTexture* outputs[1] = { &output };
    helper.PostProcess(toneMap, "Tone Mapping", inputs, ArraySize_(inputs), outputs, ArraySize_(outputs));

    bloomTarget->InUse = false;

    helper.End();
}

TempRenderTarget* PostProcessor::Bloom(ID3D12GraphicsCommandList* cmdList, const RenderTexture& input)
{
    PIXMarker marker(cmdList, "Bloom");

    const uint64 bloomWidth = input.Texture.Width / 2;
    const uint64 bloomHeight = input.Texture.Height / 2;

    TempRenderTarget* downscale1 = helper.GetTempRenderTarget(bloomWidth, bloomHeight, DXGI_FORMAT_R16G16B16A16_FLOAT);
    downscale1->RT.MakeWritable(cmdList);

    helper.PostProcess(bloom, "Bloom Initial Pass", input, downscale1);

    TempRenderTarget* blurTemp = helper.GetTempRenderTarget(bloomWidth, bloomHeight, DXGI_FORMAT_R16G16B16A16_FLOAT);
    downscale1->RT.MakeReadable(cmdList);

    // Blur it
    for(uint64 i = 0; i < 2; ++i)
    {
        blurTemp->RT.MakeWritable(cmdList);

        helper.PostProcess(blurH, "Horizontal Bloom Blur", downscale1, blurTemp);

        blurTemp->RT.MakeReadable(cmdList);
        downscale1->RT.MakeWritable(cmdList);

        helper.PostProcess(blurV, "Vertical Bloom Blur", blurTemp, downscale1);

        downscale1->RT.MakeReadable(cmdList);
    }

    blurTemp->InUse = false;

    return downscale1;
}