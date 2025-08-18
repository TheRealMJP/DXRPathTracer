//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "PostProcessHelper.h"

#include "..\\Utility.h"
#include "ShaderCompilation.h"
#include "DX12.h"
#include "DX12_Helpers.h"

namespace AppSettings
{
    const extern uint32_t CBufferRegister;
    void BindCBufferGfx(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter);
}

namespace SampleFramework12
{

static const uint32_t MaxInputs = 8;

enum RootParams : uint32_t
{
    RootParam_StandardDescriptors,
    RootParam_SRVIndices,
    RootParam_AppSettings,

    NumRootParams,
};

PostProcessHelper::PostProcessHelper()
{
}

PostProcessHelper::~PostProcessHelper()
{
    Assert_(tempRenderTargets.Count() == 0);
    Assert_(pipelineStates.Count() == 0);
}

void PostProcessHelper::Initialize()
{
    // Load the shaders
    std::string fullScreenTriPath = SampleFrameworkDir() + "Shaders\\FullScreenTriangle.hlsl";
    fullScreenTriVS = CompileFromFile(fullScreenTriPath.c_str(), "FullScreenTriangleVS", ShaderType::Vertex);
}

void PostProcessHelper::Shutdown()
{
    ClearCache();
}

void PostProcessHelper::ClearCache()
{
    for(uint64_t i = 0; i < tempRenderTargets.Count(); ++i)
    {
        TempRenderTarget* tempRT = tempRenderTargets[i];
        tempRT->RT.Shutdown();
        delete tempRT;
    }

    tempRenderTargets.RemoveAll();

    for(uint64_t i = 0; i < pipelineStates.Count(); ++i)
        DX12::DeferredRelease(pipelineStates[i].PSO);

    pipelineStates.RemoveAll();
}

TempRenderTarget* PostProcessHelper::GetTempRenderTarget(uint32_t width, uint32_t height, DXGI_FORMAT format, bool useAsUAV)
{
    for(uint64_t i = 0; i < tempRenderTargets.Count(); ++i)
    {
        TempRenderTarget* tempRT = tempRenderTargets[i];
        if(tempRT->InUse)
            continue;

        const RenderTexture& rt = tempRT->RT;
        if(rt.Texture.Width == width && rt.Texture.Height == height && rt.Texture.Format == format && useAsUAV == (rt.UAV != uint32_t(-1))) {
            tempRT->InUse = true;
            return tempRT;
        }
    }

    RenderTextureInit rtInit;
    rtInit.Width = width;
    rtInit.Height = height;
    rtInit.Format = format;
    rtInit.CreateUAV = useAsUAV;
    rtInit.InitialLayout = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE;

    TempRenderTarget* tempRT = new TempRenderTarget();
    tempRT->RT.Initialize(rtInit);
    tempRT->RT.Texture.Resource->SetName(L"PP Temp Render Target");
    tempRT->InUse = true;
    tempRenderTargets.Add(tempRT);

    return tempRT;
}

void PostProcessHelper::Begin(ID3D12GraphicsCommandList* cmdList_)
{
    Assert_(cmdList == nullptr);
    cmdList = cmdList_;
}

void PostProcessHelper::End()
{
    Assert_(cmdList != nullptr);
    cmdList = nullptr;

    for(uint64_t i = 0; i < tempRenderTargets.Count(); ++i)
        Assert_(tempRenderTargets[i]->InUse == false);
}

void PostProcessHelper::PostProcess(CompiledShaderPtr pixelShader, const char* name, const RenderTexture& input, const RenderTexture& output)
{
    uint32_t inputs[1] = { input.SRV() };
    const RenderTexture* outputs[1] = { &output };
    PostProcess(pixelShader, name, inputs, 1, outputs, 1);
}

void PostProcessHelper::PostProcess(CompiledShaderPtr pixelShader, const char* name, const RenderTexture& input, const TempRenderTarget* output)
{
    uint32_t inputs[1] = { input.SRV() };
    const RenderTexture* outputs[1] = { &output->RT };
    PostProcess(pixelShader, name, inputs, 1, outputs, 1);
}

void PostProcessHelper::PostProcess(CompiledShaderPtr pixelShader, const char* name, const TempRenderTarget* input, const RenderTexture& output)
{
    uint32_t inputs[1] = { input->RT.SRV() };
    const RenderTexture* outputs[1] = { &output };
    PostProcess(pixelShader, name, inputs, 1, outputs, 1);
}

void PostProcessHelper::PostProcess(CompiledShaderPtr pixelShader, const char* name, const TempRenderTarget* input, const TempRenderTarget* output)
{
    uint32_t inputs[1] = { input->RT.SRV() };
    const RenderTexture* outputs[1] = { &output->RT };
    PostProcess(pixelShader, name, inputs, 1, outputs, 1);
}

struct HashSource
{
    DXGI_FORMAT OutputFormats[8] = { };
    uint64_t MSAASamples = 0;
};

void PostProcessHelper::PostProcess(CompiledShaderPtr pixelShader, const char* name, const uint32_t* inputs, uint64_t numInputs,
                                    const RenderTexture*const* outputs, uint64_t numOutputs)
{
    Assert_(cmdList != nullptr);
    Assert_(numOutputs > 0);
    Assert_(outputs != nullptr);
    Assert_(numInputs == 0 || inputs != nullptr);
    Assert_(numInputs <= MaxInputs);

    PIXMarker marker(cmdList, name);

    HashSource hashSource;
    for(uint64_t i = 0; i < numOutputs; ++i)
    {
        hashSource.OutputFormats[i] = outputs[i]->Texture.Format;
        hashSource.MSAASamples = outputs[i]->MSAASamples;
    }

    Hash psoHash = GenerateHash(&hashSource, sizeof(HashSource));
    psoHash = CombineHashes(psoHash, pixelShader->ByteCodeHash);

    ID3D12PipelineState* pso = nullptr;

    // The most linear of searches!
    const uint64_t numPSOs = pipelineStates.Count();
    for(uint64_t i = 0; i < numPSOs; ++i)
    {
        if(pipelineStates[i].Hash == psoHash)
        {
            pso = pipelineStates[i].PSO;
            break;
        }
    }

    if(pso == nullptr)
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = DX12::UniversalRootSignature;
        psoDesc.VS = fullScreenTriVS.ByteCode();
        psoDesc.PS = pixelShader.ByteCode();
        psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::NoCull);
        psoDesc.BlendState = DX12::GetBlendState(BlendState::Disabled);
        psoDesc.DepthStencilState = DX12::GetDepthState(DepthState::Disabled);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = uint32_t(numOutputs);
        for(uint64_t i = 0; i < numOutputs; ++i)
            psoDesc.RTVFormats[i] = hashSource.OutputFormats[i];
        psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        psoDesc.SampleDesc.Count = uint32_t(hashSource.MSAASamples);
        DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));

        CachedPSO cachedPSO;
        cachedPSO.Hash = psoHash;
        cachedPSO.PSO = pso;
        pipelineStates.Add(cachedPSO);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[8] = { };
    for(uint64_t i = 0; i < numOutputs; ++i)
        rtvHandles[i] = outputs[i]->RTV;
    cmdList->OMSetRenderTargets(uint32_t(numOutputs), rtvHandles, false, nullptr);

    cmdList->SetGraphicsRootSignature(DX12::UniversalRootSignature);
    cmdList->SetPipelineState(pso);    

    AppSettings::BindCBufferGfx(cmdList, URS_AppSettings);

    uint32_t srvIndices[MaxInputs] = { };
    for(uint64_t i = 0; i < numInputs; ++i)
        srvIndices[i] = inputs[i];
    for(uint64_t i = numInputs; i < MaxInputs; ++i)
        srvIndices[i] = DX12::NullTexture2DSRV;

    DX12::BindTempConstantBuffer(cmdList, srvIndices, URS_ConstantBuffers + 0, CmdListMode::Graphics);

    DX12::SetViewport(cmdList, outputs[0]->Texture.Width, outputs[0]->Texture.Height);

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(3, 1, 0, 0);

}

}