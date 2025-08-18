//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "SpriteRenderer.h"
#include "ShaderCompilation.h"
#include "Textures.h"
#include "DX12_Helpers.h"

namespace SampleFramework12
{

SpriteRenderer::SpriteRenderer()
{

}

SpriteRenderer::~SpriteRenderer()
{
}

void SpriteRenderer::Initialize()
{
    const std::string shaderPath = SampleFrameworkDir() + "Shaders\\Sprite.hlsl";

    // Load the shaders
    vertexShader = CompileFromFile(shaderPath.c_str(), "SpriteVS", ShaderType::Vertex);
    pixelShader = CompileFromFile(shaderPath.c_str(), "SpritePS", ShaderType::Pixel);

    // Create the index buffer
    uint16_t indices[] = { 0, 1, 2, 3, 0, 2 };
    FormattedBufferInit ibInit;
    ibInit.Format = DXGI_FORMAT_R16_UINT;
    ibInit.NumElements = ArraySize_(indices);    
    ibInit.InitData = indices;
    ibInit.Name = "SpriteRenderer Index Buffer";
    indexBuffer.Initialize(ibInit);

    LoadTexture(defaultTexture, "..\\Content\\Textures\\Default.dds");
}

void SpriteRenderer::Shutdown()
{
    DestroyPSOs();

    indexBuffer.Shutdown();
    defaultTexture.Shutdown();
}

void SpriteRenderer::CreatePSOs(DXGI_FORMAT rtFormat, uint32_t numMSAASamples)
{
    // Make PSO's for all blend modes
    const BlendState blendStates[] = { BlendState::AlphaBlend, BlendState::Disabled };
    StaticAssert_(ArraySize_(blendStates) == uint64_t(SpriteBlendMode::NumValues));

    for(uint64_t i = 0; i < uint64_t(SpriteBlendMode::NumValues); ++i)
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = DX12::UniversalRootSignature;
        psoDesc.VS = vertexShader.ByteCode();
        psoDesc.PS = pixelShader.ByteCode();
        psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::NoCull);
        psoDesc.BlendState = DX12::GetBlendState(blendStates[i]);
        psoDesc.DepthStencilState = DX12::GetDepthState(DepthState::Disabled);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = rtFormat;
        psoDesc.SampleDesc.Count = numMSAASamples;
        psoDesc.SampleDesc.Quality = numMSAASamples > 1 ? DX12::StandardMSAAPattern : 0;
        DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStates[i])));
    }
}

void SpriteRenderer::DestroyPSOs()
{
    for(uint64_t i = 0; i < ArraySize_(pipelineStates); ++i)
        DX12::DeferredRelease(pipelineStates[i]);
}

void SpriteRenderer::Begin(ID3D12GraphicsCommandList* cmdList, Float2 viewportSize, SpriteFilterMode filterMode, SpriteBlendMode blendMode)
{
    cmdList->SetPipelineState(pipelineStates[uint64_t(blendMode)]);
    cmdList->SetGraphicsRootSignature(DX12::UniversalRootSignature);

    perBatchData.LinearSampling = filterMode == SpriteFilterMode::Linear ? 1 : 0;
    perBatchData.ViewportSize = viewportSize;

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_INDEX_BUFFER_VIEW ibView = indexBuffer.IBView();
    cmdList->IASetIndexBuffer(&ibView);    
}

void SpriteRenderer::Render(ID3D12GraphicsCommandList* cmdList, const Texture* texture, const SpriteTransform& transform,
                            const Float4& color, const Float4* drawRect)
{
    if(texture == nullptr)
        texture = &defaultTexture;

    SpriteDrawData drawData;
    drawData.Transform = transform;
    drawData.Color = color;
    if(drawRect != nullptr)
        drawData.DrawRect = *drawRect;
    else
        drawData.DrawRect = Float4(0.0f, 0.0f, float(texture->Width), float(texture->Height));

    RenderBatch(cmdList, texture, &drawData, 1);
}

void SpriteRenderer::RenderBatch(ID3D12GraphicsCommandList* cmdList, const Texture* texture,
                                 const SpriteDrawData* drawData, uint64_t numSprites)
{
    if(numSprites == 0)
        return;

    if(texture == nullptr)
        texture = &defaultTexture;

    perBatchData.TextureSize = Float2(float(texture->Width), float(texture->Height));
    DX12::BindTempConstantBuffer(cmdList, perBatchData, URS_ConstantBuffers + 0, CmdListMode::Graphics);

    #if Debug_
        // Make sure the draw rects are all valid
        for(uint64_t i = 0; i < numSprites; ++i)
        {
            Float4 drawRect = drawData[i].DrawRect;
            Assert_(drawRect.x >= 0 && drawRect.x < texture->Width);
            Assert_(drawRect.y >= 0 && drawRect.y < texture->Height);
            Assert_(drawRect.z > 0 && drawRect.x + drawRect.z <= texture->Width);
            Assert_(drawRect.w > 0 && drawRect.y + drawRect.w <= texture->Height);
        }
    #endif

    uint64_t numSpritesLeft = numSprites;
    for(uint64_t offset = 0; offset < numSprites; offset += MaxBatchSize)
    {
        const uint64_t spritesToDraw = Min<uint64_t>(MaxBatchSize, numSpritesLeft);

        // Fill up the instance buffer
        TempBuffer instanceBuffer = DX12::TempStructuredBuffer(spritesToDraw, sizeof(SpriteDrawData));
        memcpy(instanceBuffer.CPUAddress, drawData + offset, spritesToDraw * sizeof(SpriteDrawData));

        uint32_t srvIndices[] = { instanceBuffer.DescriptorIndex, texture->SRV };
        DX12::BindTempConstantBuffer(cmdList, srvIndices, URS_ConstantBuffers + 1, CmdListMode::Graphics);

        // Draw
        cmdList->DrawIndexedInstanced(6, uint32_t(spritesToDraw), 0, 0, 0);

        numSpritesLeft -= spritesToDraw;
    }
}

void SpriteRenderer::End()
{
}

}