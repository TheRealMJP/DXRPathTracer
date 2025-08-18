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
#include <Graphics/Textures.h>

#include "PostProcessor.h"

void PostProcessor::Initialize()
{
    helper.Initialize();

    // Load the shaders
    toneMap = CompileFromFile("PostProcessing.hlsl", "ToneMap", ShaderType::Pixel);
    scale = CompileFromFile("PostProcessing.hlsl", "Scale", ShaderType::Pixel);
    blurH = CompileFromFile("PostProcessing.hlsl", "BlurH", ShaderType::Pixel);
    blurV = CompileFromFile("PostProcessing.hlsl", "BlurV", ShaderType::Pixel);
    bloom = CompileFromFile("PostProcessing.hlsl", "Bloom", ShaderType::Pixel);
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

void PostProcessor::Render(ID3D12GraphicsCommandList7* cmdList, const RenderTexture& input, const RenderTexture& output)
{
    helper.Begin(cmdList);

    TempRenderTarget* bloomTarget = Bloom(cmdList, input);

    // Apply tone mapping
    uint32_t inputs[] = { input.SRV(), bloomTarget->RT.SRV() };
    const RenderTexture* outputs[1] = { &output };
    helper.PostProcess(toneMap, "Tone Mapping", inputs, ArraySize_(inputs), outputs, ArraySize_(outputs));

    bloomTarget->InUse = false;

    helper.End();
}

TempRenderTarget* PostProcessor::Bloom(ID3D12GraphicsCommandList7* cmdList, const RenderTexture& input)
{
    PIXMarker marker(cmdList, "Bloom");

    const uint32_t bloomWidth = input.Texture.Width / 2;
    const uint32_t bloomHeight = input.Texture.Height / 2;

    TempRenderTarget* downscale1 = helper.GetTempRenderTarget(bloomWidth, bloomHeight, DXGI_FORMAT_R16G16B16A16_FLOAT);
    DX12::Barrier(cmdList, downscale1->RT.RTWritableBarrier({ .FirstAccess = true, .Discard = true }));

    helper.PostProcess(bloom, "Bloom Initial Pass", input, downscale1);

    TempRenderTarget* blurTemp = helper.GetTempRenderTarget(bloomWidth, bloomHeight, DXGI_FORMAT_R16G16B16A16_FLOAT);
    DX12::Barrier(cmdList, downscale1->RT.RTToShaderReadableBarrier());

    // Blur it
    for(uint64_t i = 0; i < 2; ++i)
    {
        DX12::Barrier(cmdList, blurTemp->RT.RTWritableBarrier({ .Discard = true }));

        helper.PostProcess(blurH, "Horizontal Bloom Blur", downscale1, blurTemp);

        {
            BarrierBatchBuilder barrierBuilder;
            barrierBuilder.Add(blurTemp->RT.RTToShaderReadableBarrier());
            barrierBuilder.Add(downscale1->RT.RTWritableBarrier({ .Discard = true }));
            DX12::Barrier(cmdList, barrierBuilder.Build());
        }

        helper.PostProcess(blurV, "Vertical Bloom Blur", blurTemp, downscale1);

        DX12::Barrier(cmdList, downscale1->RT.RTToShaderReadableBarrier());
    }

    blurTemp->InUse = false;

    return downscale1;
}