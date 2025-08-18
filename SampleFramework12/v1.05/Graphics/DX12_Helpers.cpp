//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "DX12_Helpers.h"
#include "DX12.h"
#include "DX12_Upload.h"
#include "GraphicsTypes.h"
#include "ShaderCompilation.h"
#include "SF12_Math.h"

namespace AppSettings
{
    extern const uint32_t CBufferRegister;
}

namespace SampleFramework12
{

namespace DX12
{

uint32_t RTVDescriptorSize = 0;
uint32_t SRVDescriptorSize = 0;
uint32_t UAVDescriptorSize = 0;
uint32_t CBVDescriptorSize = 0;
uint32_t DSVDescriptorSize = 0;

DescriptorHeap RTVDescriptorHeap;
DescriptorHeap SRVDescriptorHeap;
DescriptorHeap DSVDescriptorHeap;

DescriptorIndex NullTexture2DSRV;
DescriptorIndex NullTexture2DUAV;
DescriptorIndex NullStructuredBufferUAV;
DescriptorIndex NullRawBufferUAV;

ID3D12RootSignature* UniversalRootSignature = nullptr;
ID3D12RootSignature* UniversalRootSignatureWithIA = nullptr;

static const uint64_t NumBlendStates = uint64_t(BlendState::NumValues);
static const uint64_t NumRasterizerStates = uint64_t(RasterizerState::NumValues);
static const uint64_t NumDepthStates = uint64_t(DepthState::NumValues);
static const uint64_t NumSamplerStates = uint64_t(SamplerState::NumValues);

static D3D12_BLEND_DESC BlendStateDescs[NumBlendStates] = { };
static D3D12_RASTERIZER_DESC RasterizerStateDescs[NumRasterizerStates] = { };
static D3D12_DEPTH_STENCIL_DESC DepthStateDescs[NumDepthStates] = { };
static D3D12_SAMPLER_DESC SamplerStateDescs[NumSamplerStates] = { };

static ID3D12GraphicsCommandList7* convertCmdList = nullptr;
static ID3D12CommandQueue* convertCmdQueue = nullptr;
static ID3D12CommandAllocator* convertCmdAllocator = nullptr;
static ID3D12PipelineState* convertPSO = nullptr;
static ID3D12PipelineState* convertArrayPSO = nullptr;
static ID3D12PipelineState* convertCubePSO = nullptr;
static CompiledShaderPtr convertCS;
static CompiledShaderPtr convertArrayCS;
static CompiledShaderPtr convertCubeCS;
static const uint32_t convertTGSize = 8;
static Fence convertFence;

static const uint32_t clearRawBufferTGSize = 64;
static ID3D12PipelineState* clearRawBufferPSO = nullptr;
static CompiledShaderPtr clearRawBufferCS = nullptr;

struct DecodeCBuffer
{
    uint32_t InputTextureIdx = uint32_t(-1);
    uint32_t OutputBufferIdx = uint32_t(-1);
    uint32_t Width = 0;
    uint32_t Height = 0;
};

struct ClearRawBufferConstants
{
    Uint4 ClearValue;
    uint32_t DescriptorIdx = uint32_t(-1);
    uint32_t Num16ByteElements = 0;
};

void Initialize_Helpers()
{
    RTVDescriptorHeap.Init(256, 0, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);
    SRVDescriptorHeap.Init(1024 * 16, 4096, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
    DSVDescriptorHeap.Init(256, 0, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false);

    RTVDescriptorSize = RTVDescriptorHeap.DescriptorSize;
    SRVDescriptorSize = UAVDescriptorSize = CBVDescriptorSize = SRVDescriptorHeap.DescriptorSize;
    DSVDescriptorSize = DSVDescriptorHeap.DescriptorSize;

    // Blend state initialization
    {
        D3D12_BLEND_DESC& blendDesc = BlendStateDescs[uint64_t(BlendState::Disabled)];
        blendDesc.RenderTarget[0].BlendEnable = false;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    }

    {
        D3D12_BLEND_DESC& blendDesc = BlendStateDescs[uint64_t(BlendState::Additive)];
        blendDesc.RenderTarget[0].BlendEnable = true;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    }

    {
        D3D12_BLEND_DESC& blendDesc = BlendStateDescs[uint64_t(BlendState::AlphaBlend)];
        blendDesc.RenderTarget[0].BlendEnable = true;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    }

    {
        D3D12_BLEND_DESC& blendDesc = BlendStateDescs[uint64_t(BlendState::PreMultiplied)];
        blendDesc.RenderTarget[0].BlendEnable = false;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    }

    {
        D3D12_BLEND_DESC& blendDesc = BlendStateDescs[uint64_t(BlendState::NoColorWrites)];
        blendDesc.RenderTarget[0].BlendEnable = false;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = 0;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    }

    {
        D3D12_BLEND_DESC& blendDesc = BlendStateDescs[uint64_t(BlendState::PreMultipliedRGB)];
        blendDesc.RenderTarget[0].BlendEnable = true;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC1_COLOR;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    }

    // Rasterizer state initialization
    {
        D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[uint64_t(RasterizerState::NoCull)];
        rastDesc.CullMode = D3D12_CULL_MODE_NONE;
        rastDesc.DepthClipEnable = true;
        rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rastDesc.MultisampleEnable = true;
    }

    {
        D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[uint64_t(RasterizerState::NoCullNoMS)];
        rastDesc.CullMode = D3D12_CULL_MODE_NONE;
        rastDesc.DepthClipEnable = true;
        rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rastDesc.MultisampleEnable = false;
    }

    {
        D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[uint64_t(RasterizerState::NoCullNoZClip)];
        rastDesc.CullMode = D3D12_CULL_MODE_NONE;
        rastDesc.DepthClipEnable = true;
        rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rastDesc.MultisampleEnable = true;
        rastDesc.DepthClipEnable = false;
    }

    {
        D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[uint64_t(RasterizerState::FrontFaceCull)];
        rastDesc.CullMode = D3D12_CULL_MODE_FRONT;
        rastDesc.DepthClipEnable = true;
        rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rastDesc.MultisampleEnable = true;
    }

    {
        D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[uint64_t(RasterizerState::BackFaceCull)];
        rastDesc.CullMode = D3D12_CULL_MODE_BACK;
        rastDesc.DepthClipEnable = true;
        rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rastDesc.MultisampleEnable = true;
    }

    {
        D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[uint64_t(RasterizerState::BackFaceCullNoZClip)];
        rastDesc.CullMode = D3D12_CULL_MODE_BACK;
        rastDesc.DepthClipEnable = false;
        rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rastDesc.MultisampleEnable = true;
    }

    {
        D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[uint64_t(RasterizerState::Wireframe)];
        rastDesc.CullMode = D3D12_CULL_MODE_NONE;
        rastDesc.DepthClipEnable = true;
        rastDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
        rastDesc.MultisampleEnable = true;
    }

    // Depth state initialization
    {
        D3D12_DEPTH_STENCIL_DESC& dsDesc = DepthStateDescs[uint64_t(DepthState::Disabled)];
        dsDesc.DepthEnable = false;
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    }

    {
        D3D12_DEPTH_STENCIL_DESC& dsDesc = DepthStateDescs[uint64_t(DepthState::Enabled)];
        dsDesc.DepthEnable = true;
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    }

    {
        D3D12_DEPTH_STENCIL_DESC& dsDesc = DepthStateDescs[uint64_t(DepthState::Reversed)];
        dsDesc.DepthEnable = true;
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    }

    {
        D3D12_DEPTH_STENCIL_DESC& dsDesc = DepthStateDescs[uint64_t(DepthState::WritesEnabled)];
        dsDesc.DepthEnable = true;
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    }

    {
        D3D12_DEPTH_STENCIL_DESC& dsDesc = DepthStateDescs[uint64_t(DepthState::ReversedWritesEnabled)];
        dsDesc.DepthEnable = true;
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    }

    {
        D3D12_DEPTH_STENCIL_DESC& dsDesc = DepthStateDescs[uint64_t(DepthState::Equal)];
        dsDesc.DepthEnable = true;
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
    }

    {
        D3D12_DEPTH_STENCIL_DESC& dsDesc = DepthStateDescs[uint64_t(DepthState::DepthFail)];
        dsDesc.DepthEnable = true;
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
    }

    // Sampler state initialization
    {
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64_t(SamplerState::Linear)];

        sampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampDesc.MipLODBias = 0.0f;
        sampDesc.MaxAnisotropy = 1;
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
        sampDesc.BorderColor[0] = sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 0;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
    }

    {
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64_t(SamplerState::LinearClamp)];

        sampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.MipLODBias = 0.0f;
        sampDesc.MaxAnisotropy = 1;
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
        sampDesc.BorderColor[0] = sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 0;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
    }

    {
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64_t(SamplerState::LinearBorder)];

        sampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampDesc.MipLODBias = 0.0f;
        sampDesc.MaxAnisotropy = 1;
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
        sampDesc.BorderColor[0] = sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 0;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
    }

    {
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64_t(SamplerState::Point)];

        sampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.MipLODBias = 0.0f;
        sampDesc.MaxAnisotropy = 1;
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
        sampDesc.BorderColor[0] = sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 0;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
    }

    {
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64_t(SamplerState::Anisotropic)];

        sampDesc.Filter = D3D12_FILTER_ANISOTROPIC;
        sampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampDesc.MipLODBias = 0.0f;
        sampDesc.MaxAnisotropy = 16;
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
        sampDesc.BorderColor[0] = sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 0;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
    }

    {
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64_t(SamplerState::ShadowMap)];

        sampDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
        sampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.MipLODBias = 0.0f;
        sampDesc.MaxAnisotropy = 1;
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        sampDesc.BorderColor[0] = sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 0;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
    }

    {
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64_t(SamplerState::ShadowMapPCF)];

        sampDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.MipLODBias = 0.0f;
        sampDesc.MaxAnisotropy = 1;
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        sampDesc.BorderColor[0] = sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 0;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
    }

    {
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64_t(SamplerState::ReversedShadowMap)];

        sampDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
        sampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.MipLODBias = 0.0f;
        sampDesc.MaxAnisotropy = 1;
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        sampDesc.BorderColor[0] = sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 0;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
    }

    {
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64_t(SamplerState::ReversedShadowMapPCF)];

        sampDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.MipLODBias = 0.0f;
        sampDesc.MaxAnisotropy = 1;
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        sampDesc.BorderColor[0] = sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 0;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
    }

    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
        srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        PersistentDescriptorAlloc srvAlloc = SRVDescriptorHeap.AllocatePersistent();
        for(uint32_t i = 0; i < ArraySize_(srvAlloc.Handles); ++i)
            Device->CreateShaderResourceView(nullptr, &srvDesc, srvAlloc.Handles[i]);
        NullTexture2DSRV = srvAlloc.Index;
    }

    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = { };
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        uavDesc.Texture2D.MipSlice = 0;
        uavDesc.Texture2D.PlaneSlice = 0;

        PersistentDescriptorAlloc uavAlloc = SRVDescriptorHeap.AllocatePersistent();
        for(uint32_t i = 0; i < ArraySize_(uavAlloc.Handles); ++i)
            DX12::Device->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, uavAlloc.Handles[i]);
        NullTexture2DUAV = uavAlloc.Index;
    }

    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = { };
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        uavDesc.Buffer.NumElements = 1;
        uavDesc.Buffer.StructureByteStride = 16;

        PersistentDescriptorAlloc uavAlloc = SRVDescriptorHeap.AllocatePersistent();
        for(uint32_t i = 0; i < ArraySize_(uavAlloc.Handles); ++i)
            DX12::Device->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, uavAlloc.Handles[i]);
        NullStructuredBufferUAV = uavAlloc.Index;
    }

    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = { };
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        uavDesc.Buffer.NumElements = 1;

        PersistentDescriptorAlloc uavAlloc = SRVDescriptorHeap.AllocatePersistent();
        for(uint32_t i = 0; i < ArraySize_(uavAlloc.Handles); ++i)
            DX12::Device->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, uavAlloc.Handles[i]);
        NullRawBufferUAV = uavAlloc.Index;
    }

    {
        // Universal root signature
        D3D12_ROOT_PARAMETER1 rootParameters[NumUniversalRootSignatureParams] = {};

        // Constant buffers
        for(uint32_t i = 0; i < NumUniversalRootSignatureConstantBuffers; ++i)
        {
            rootParameters[URS_ConstantBuffers + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            rootParameters[URS_ConstantBuffers + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            rootParameters[URS_ConstantBuffers + i].Descriptor.RegisterSpace = 0;
            rootParameters[URS_ConstantBuffers + i].Descriptor.ShaderRegister = i;
            rootParameters[URS_ConstantBuffers + i].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
        }

        // AppSettings
        rootParameters[URS_AppSettings].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[URS_AppSettings].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[URS_AppSettings].Descriptor.RegisterSpace = 0;
        rootParameters[URS_AppSettings].Descriptor.ShaderRegister = AppSettings::CBufferRegister;
        rootParameters[URS_AppSettings].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

        // Static samplers
        D3D12_STATIC_SAMPLER_DESC staticSamplers[uint64_t(SamplerState::NumValues)] = {};
        for(uint32_t i = 0; i < uint32_t(SamplerState::NumValues); ++i)
            staticSamplers[i] = GetStaticSamplerState(SamplerState(i), i, 0, D3D12_SHADER_VISIBILITY_ALL);

        D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
        rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.NumStaticSamplers = ArraySize_(staticSamplers);
        rootSignatureDesc.pStaticSamplers = staticSamplers;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

        DX12::CreateRootSignature(&UniversalRootSignature, rootSignatureDesc);

        rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        DX12::CreateRootSignature(&UniversalRootSignatureWithIA, rootSignatureDesc);
    }

    {
        // Texture conversion resources
        DXCall(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&convertCmdAllocator)));
        DXCall(Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, convertCmdAllocator, nullptr, IID_PPV_ARGS(&convertCmdList)));
        DXCall(convertCmdList->Close());
        DXCall(convertCmdList->Reset(convertCmdAllocator, nullptr));

        D3D12_COMMAND_QUEUE_DESC convertQueueDesc = {};
        convertQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        convertQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        DXCall(Device->CreateCommandQueue(&convertQueueDesc, IID_PPV_ARGS(&convertCmdQueue)));

        CompileOptions opts;
        opts.Add("TGSize_", convertTGSize);
        const std::string shaderPath = SampleFrameworkDir() + "Shaders\\DecodeTextureCS.hlsl";
        convertCS = CompileFromFile(shaderPath.c_str(), "DecodeTextureCS", ShaderType::Compute, opts);
        convertArrayCS = CompileFromFile(shaderPath.c_str(), "DecodeTextureArrayCS", ShaderType::Compute, opts);
        convertCubeCS = CompileFromFile(shaderPath.c_str(), "DecodeTextureCubeCS", ShaderType::Compute, opts);

        {
            D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = { };
            psoDesc.CS = convertCS.ByteCode();
            psoDesc.pRootSignature = UniversalRootSignature;
            psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
            Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&convertPSO));

            psoDesc.CS = convertArrayCS.ByteCode();
            Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&convertArrayPSO));

            psoDesc.CS = convertCubeCS.ByteCode();
            Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&convertCubePSO));
        }

        convertFence.Init(0);
    }

    {
        // Clear raw buffer resources
        CompileOptions opts;
        opts.Add("TGSize_", clearRawBufferTGSize);
        const std::string shaderPath = SampleFrameworkDir() + "Shaders\\ClearRawBuffer.hlsl";
        clearRawBufferCS = CompileFromFile(shaderPath.c_str(), "ClearRawBufferCS", ShaderType::Compute, opts);

        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = { };
        psoDesc.CS = clearRawBufferCS.ByteCode();
        psoDesc.pRootSignature = UniversalRootSignature;
        psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&clearRawBufferPSO));
    }

}

void Shutdown_Helpers()
{
    SRVDescriptorHeap.FreePersistent(NullTexture2DSRV);
    SRVDescriptorHeap.FreePersistent(NullTexture2DUAV);
    SRVDescriptorHeap.FreePersistent(NullStructuredBufferUAV);
    SRVDescriptorHeap.FreePersistent(NullRawBufferUAV);

    RTVDescriptorHeap.Shutdown();
    SRVDescriptorHeap.Shutdown();
    DSVDescriptorHeap.Shutdown();

    DX12::Release(UniversalRootSignature);
    DX12::Release(UniversalRootSignatureWithIA);

    Release(convertCmdAllocator);
    Release(convertCmdList);
    Release(convertCmdQueue);
    Release(convertPSO);
    Release(convertArrayPSO);
    Release(convertCubePSO);    
    convertFence.Shutdown();

    Release(clearRawBufferPSO);
}

void EndFrame_Helpers()
{
    RTVDescriptorHeap.EndFrame();
    SRVDescriptorHeap.EndFrame();
    DSVDescriptorHeap.EndFrame();
}

void Barrier(ID3D12GraphicsCommandList7* cmdList, const D3D12_GLOBAL_BARRIER& barrier)
{
    const D3D12_BARRIER_GROUP group =
    {
        .Type = D3D12_BARRIER_TYPE_GLOBAL,
        .NumBarriers = 1,
        .pGlobalBarriers = &barrier,
    };
    cmdList->Barrier(1, &group);
}

void Barrier(ID3D12GraphicsCommandList7* cmdList, const D3D12_BUFFER_BARRIER& barrier)
{
    const D3D12_BARRIER_GROUP group =
    {
        .Type = D3D12_BARRIER_TYPE_BUFFER,
        .NumBarriers = 1,
        .pBufferBarriers = &barrier,
    };
    cmdList->Barrier(1, &group);
}

void Barrier(ID3D12GraphicsCommandList7* cmdList, const D3D12_TEXTURE_BARRIER& barrier)
{
    const D3D12_BARRIER_GROUP group =
    {
        .Type = D3D12_BARRIER_TYPE_TEXTURE,
        .NumBarriers = 1,
        .pTextureBarriers = &barrier,
    };
    cmdList->Barrier(1, &group);
}

void Barrier(ID3D12GraphicsCommandList7* cmdList, const BarrierBatch& batch)
{
    if(batch.NumBufferBarriers + batch.NumTextureBarriers + batch.NumGlobalBarriers == 0)
        return;

    D3D12_BARRIER_GROUP groups[3] = { };
    uint32_t numGroups = 0;

    if(batch.NumBufferBarriers > 0)
    {
        Assert_(batch.BufferBarriers != nullptr);

        D3D12_BARRIER_GROUP& group = groups[numGroups];
        group =
        {
            .Type = D3D12_BARRIER_TYPE_BUFFER,
            .NumBarriers = batch.NumBufferBarriers,
            .pBufferBarriers = batch.BufferBarriers,
        };

        numGroups += 1;
    }

    if(batch.NumTextureBarriers > 0)
    {
        Assert_(batch.TextureBarriers != nullptr);

        D3D12_BARRIER_GROUP& group = groups[numGroups];
        group =
        {
            .Type = D3D12_BARRIER_TYPE_TEXTURE,
            .NumBarriers = batch.NumTextureBarriers,
            .pTextureBarriers = batch.TextureBarriers,
        };

        numGroups += 1;
    }

    if(batch.NumGlobalBarriers > 0)
    {
        Assert_(batch.GlobalBarriers != nullptr);

        D3D12_BARRIER_GROUP& group = groups[numGroups];
        group =
        {
            .Type = D3D12_BARRIER_TYPE_GLOBAL,
            .NumBarriers = batch.NumGlobalBarriers,
            .pGlobalBarriers = batch.GlobalBarriers,
        };

        numGroups += 1;
    }

    cmdList->Barrier(numGroups, groups);
}

uint64_t GetResourceSize(const D3D12_RESOURCE_DESC& desc, uint32_t firstSubResource, uint32_t numSubResources)
{
    uint64_t size = 0;
    Device->GetCopyableFootprints(&desc, firstSubResource, numSubResources, 0, nullptr, nullptr, nullptr, &size);
    return size;
}

uint64_t GetResourceSize(ID3D12Resource* resource, uint32_t firstSubResource, uint32_t numSubResources)
{
    D3D12_RESOURCE_DESC desc = resource->GetDesc();

    return GetResourceSize(desc, firstSubResource, numSubResources);
}

const D3D12_HEAP_PROPERTIES* GetDefaultHeapProps()
{
    static D3D12_HEAP_PROPERTIES heapProps =
    {
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0,
    };

    return &heapProps;
}

const D3D12_HEAP_PROPERTIES* GetUploadHeapProps()
{
    static D3D12_HEAP_PROPERTIES heapProps =
    {
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0,
    };

    return &heapProps;
}

const D3D12_HEAP_PROPERTIES* GetReadbackHeapProps()
{
    static D3D12_HEAP_PROPERTIES heapProps =
    {
        D3D12_HEAP_TYPE_READBACK,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0,
    };

    return &heapProps;
}

const D3D12_HEAP_PROPERTIES* GetGPUUploadHeapProps()
{
    static D3D12_HEAP_PROPERTIES heapProps =
    {
        D3D12_HEAP_TYPE_GPU_UPLOAD,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0,
    };

    return &heapProps;
}

D3D12_BLEND_DESC GetBlendState(BlendState blendState)
{
    Assert_(uint64_t(blendState) < ArraySize_(BlendStateDescs));
    return BlendStateDescs[uint64_t(blendState)];
}

D3D12_RASTERIZER_DESC GetRasterizerState(RasterizerState rasterizerState)
{
    Assert_(uint64_t(rasterizerState) < ArraySize_(RasterizerStateDescs));
    return RasterizerStateDescs[uint64_t(rasterizerState)];
}

D3D12_RASTERIZER_DESC2 GetRasterizerState2(RasterizerState rasterizerState)
{
    const D3D12_RASTERIZER_DESC desc = GetRasterizerState(rasterizerState);
    D3D12_RASTERIZER_DESC2 desc2 =
    {
        .FillMode = desc.FillMode,
        .CullMode = desc.CullMode,
        .FrontCounterClockwise = desc.FrontCounterClockwise,
        .DepthBias = 0.0f,
        .DepthBiasClamp = 0.0f,
        .SlopeScaledDepthBias = 0.0f,
        .DepthClipEnable = desc.DepthClipEnable,
        .LineRasterizationMode = D3D12_LINE_RASTERIZATION_MODE_ALIASED,
        .ForcedSampleCount = desc.ForcedSampleCount,
        .ConservativeRaster = desc.ConservativeRaster,
    };

    return desc2;
}

D3D12_DEPTH_STENCIL_DESC GetDepthState(DepthState depthState)
{
    Assert_(uint64_t(depthState) < ArraySize_(DepthStateDescs));
    return DepthStateDescs[uint64_t(depthState)];
}

D3D12_SAMPLER_DESC GetSamplerState(SamplerState samplerState)
{
    Assert_(uint64_t(samplerState) < ArraySize_(SamplerStateDescs));
    return SamplerStateDescs[uint64_t(samplerState)];
}

D3D12_STATIC_SAMPLER_DESC GetStaticSamplerState(SamplerState samplerState, uint32_t shaderRegister,
                                                uint32_t registerSpace, D3D12_SHADER_VISIBILITY visibility)
{
    Assert_(uint64_t(samplerState) < ArraySize_(SamplerStateDescs));
    return ConvertToStaticSampler(SamplerStateDescs[uint64_t(samplerState)], shaderRegister, registerSpace, visibility);
}

D3D12_STATIC_SAMPLER_DESC ConvertToStaticSampler(const D3D12_SAMPLER_DESC& samplerDesc, uint32_t shaderRegister,
                                                 uint32_t registerSpace, D3D12_SHADER_VISIBILITY visibility)
{
    D3D12_STATIC_SAMPLER_DESC staticDesc = { };
    staticDesc.Filter = samplerDesc.Filter;
    staticDesc.AddressU = samplerDesc.AddressU;
    staticDesc.AddressV = samplerDesc.AddressV;
    staticDesc.AddressW = samplerDesc.AddressW;
    staticDesc.MipLODBias = samplerDesc.MipLODBias;
    staticDesc.MaxAnisotropy = samplerDesc.MaxAnisotropy;
    staticDesc.ComparisonFunc = samplerDesc.ComparisonFunc;
    staticDesc.MinLOD = samplerDesc.MinLOD;
    staticDesc.MaxLOD = samplerDesc.MaxLOD;
    staticDesc.ShaderRegister = shaderRegister;
    staticDesc.RegisterSpace = registerSpace;
    staticDesc.ShaderVisibility = visibility;

    Float4 borderColor = Float4(samplerDesc.BorderColor[0], samplerDesc.BorderColor[1], samplerDesc.BorderColor[2], samplerDesc.BorderColor[3]);
    if(borderColor == Float4(1.0f, 1.0f, 1.0f, 1.0f))
        staticDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    else if(borderColor == Float4(0.0f, 0.0f, 0.0f, 1.0f))
        staticDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    else
        staticDesc.BorderColor =  D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;

    return staticDesc;
}

void SetViewport(ID3D12GraphicsCommandList* cmdList, uint64_t width, uint64_t height, float zMin, float zMax)
{
    D3D12_VIEWPORT viewport = { };
    viewport.Width = float(width);
    viewport.Height = float(height);
    viewport.MinDepth = zMin;
    viewport.MaxDepth = zMax;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;

    D3D12_RECT scissorRect = { };
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = uint32_t(width);
    scissorRect.bottom = uint32_t(height);

    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissorRect);
}

void CreateRootSignature(ID3D12RootSignature** rootSignature, const D3D12_ROOT_SIGNATURE_DESC1& desc)
{
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedDesc = { };
    versionedDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    versionedDesc.Desc_1_1 = desc;

    ID3DBlobPtr signature;
    ID3DBlobPtr error;
    HRESULT hr = D3D12SerializeVersionedRootSignature(&versionedDesc, &signature, &error);
    if(FAILED(hr))
    {
        const char* errString = error ? reinterpret_cast<const char*>(error->GetBufferPointer()) : "";

        #if UseAsserts_
            AssertMsg_(false, "Failed to create root signature: %s", errString);
        #else
            throw DXException(hr, MakeString("Failed to create root signature: %s", errString).c_str());
        #endif
    }

    DXCall(DX12::Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(rootSignature)));
}

uint32_t DispatchSize(uint64_t numElements, uint64_t groupSize)
{
    Assert_(groupSize > 0);
    return uint32_t((numElements + (groupSize - 1)) / groupSize);
}

static const uint64_t MaxBindCount = 16;
static const uint32_t DescriptorCopyRanges[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
StaticAssert_(ArraySize_(DescriptorCopyRanges) == MaxBindCount);

void SetDescriptorHeaps(ID3D12GraphicsCommandList* cmdList)
{
    ID3D12DescriptorHeap* heaps[] =
    {
        SRVDescriptorHeap.CurrentHeap(),
    };

    cmdList->SetDescriptorHeaps(ArraySize_(heaps), heaps);
}

TempBuffer TempConstantBuffer(uint64_t cbSize, bool makeDescriptor)
{
    Assert_(cbSize > 0);
    MapResult tempMem = DX12::AcquireTempBufferMem(cbSize, ConstantBufferAlignment);
    TempBuffer tempBuffer;
    tempBuffer.CPUAddress = tempMem.CPUAddress;
    tempBuffer.GPUAddress = tempMem.GPUAddress;
    if(makeDescriptor)
    {
        TempDescriptorAlloc cbvAlloc = SRVDescriptorHeap.AllocateTemporary(1);
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = { };
        cbvDesc.BufferLocation = tempMem.GPUAddress;
        cbvDesc.SizeInBytes = uint32_t(AlignTo(cbSize, ConstantBufferAlignment));
        DX12::Device->CreateConstantBufferView(&cbvDesc, cbvAlloc.StartCPUHandle);
        tempBuffer.DescriptorIndex = cbvAlloc.StartIndex;
    }

    return tempBuffer;
}

void BindTempConstantBuffer(ID3D12GraphicsCommandList* cmdList, const void* cbData, uint64_t cbSize, uint32_t rootParameter, CmdListMode cmdListMode)
{
    TempBuffer tempBuffer = TempConstantBuffer(cbSize, false);
    memcpy(tempBuffer.CPUAddress, cbData, cbSize);

    if(cmdListMode == CmdListMode::Graphics)
        cmdList->SetGraphicsRootConstantBufferView(rootParameter, tempBuffer.GPUAddress);
    else
        cmdList->SetComputeRootConstantBufferView(rootParameter, tempBuffer.GPUAddress);
}

TempBuffer TempStructuredBuffer(uint64_t numElements, uint64_t stride, bool makeDescriptor)
{
    Assert_(numElements > 0);
    Assert_(stride > 0);
    Assert_(stride % 4 == 0);

    MapResult tempMem = DX12::AcquireTempBufferMem(numElements * stride, stride);
    Assert_(tempMem.ResourceOffset % stride == 0);

    TempBuffer result;
    result.CPUAddress = tempMem.CPUAddress;
    result.GPUAddress = tempMem.GPUAddress;

    if(makeDescriptor)
    {
        TempDescriptorAlloc srvAlloc = SRVDescriptorHeap.AllocateTemporary(1);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = uint32_t(tempMem.ResourceOffset / stride);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        srvDesc.Buffer.NumElements = uint32_t(numElements);
        srvDesc.Buffer.StructureByteStride = uint32_t(stride);
        DX12::Device->CreateShaderResourceView(tempMem.Resource, &srvDesc, srvAlloc.StartCPUHandle);

        result.DescriptorIndex = srvAlloc.StartIndex;
    }

    return result;
}

TempBuffer TempFormattedBuffer(uint64_t numElements, DXGI_FORMAT format, bool makeDescriptor)
{
    Assert_(format != DXGI_FORMAT_UNKNOWN);
    Assert_(numElements > 0);
    uint64_t stride = DirectX::BitsPerPixel(format) / 8;

    MapResult tempMem = DX12::AcquireTempBufferMem(numElements * stride, stride);
    Assert_(tempMem.ResourceOffset % stride == 0);

    TempBuffer result;
    result.CPUAddress = tempMem.CPUAddress;
    result.GPUAddress = tempMem.GPUAddress;

    if(makeDescriptor)
    {
        TempDescriptorAlloc srvAlloc = SRVDescriptorHeap.AllocateTemporary(1);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = uint32_t(tempMem.ResourceOffset / stride);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        srvDesc.Buffer.NumElements = uint32_t(numElements);
        DX12::Device->CreateShaderResourceView(tempMem.Resource, &srvDesc, srvAlloc.StartCPUHandle);

        result.DescriptorIndex = srvAlloc.StartIndex;
    }

    return result;
}

TempBuffer TempRawBuffer(uint64_t numElements, bool makeDescriptor)
{
    Assert_(numElements > 0);
    const uint64_t stride = 4;

    MapResult tempMem = DX12::AcquireTempBufferMem(numElements * stride, stride);
    Assert_(tempMem.ResourceOffset % stride == 0);

    TempBuffer result;
    result.CPUAddress = tempMem.CPUAddress;

    if(makeDescriptor)
    {
        TempDescriptorAlloc srvAlloc = SRVDescriptorHeap.AllocateTemporary(1);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
        srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = uint32_t(tempMem.ResourceOffset / stride);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        srvDesc.Buffer.NumElements = uint32_t(numElements);
        DX12::Device->CreateShaderResourceView(tempMem.Resource, &srvDesc, srvAlloc.StartCPUHandle);

        result.DescriptorIndex = srvAlloc.StartIndex;
    }

    return result;
}

void ConvertAndReadbackTexture(const Texture& texture, DXGI_FORMAT outputFormat, ReadbackBuffer& readbackBuffer)
{
    Assert_(convertCmdList != nullptr);
    Assert_(texture.Valid());
    Assert_(texture.Depth == 1);

    // Create a buffer for the CS to write flattened, converted texture data into
    FormattedBufferInit init;
    init.Format = outputFormat;
    init.NumElements = texture.Width * texture.Height * texture.ArraySize;
    init.CreateUAV = true;

    FormattedBuffer convertBuffer;
    convertBuffer.Initialize(init);

    // Run the conversion compute shader
    DX12::SetDescriptorHeaps(convertCmdList);
    convertCmdList->SetComputeRootSignature(UniversalRootSignature);

    if(texture.Cubemap)
        convertCmdList->SetPipelineState(convertCubePSO);
    else if(texture.ArraySize > 1)
        convertCmdList->SetPipelineState(convertArrayPSO);
    else
        convertCmdList->SetPipelineState(convertPSO);

    DecodeCBuffer cbData =
    {
        .InputTextureIdx = texture.SRV,
        .OutputBufferIdx = convertBuffer.UAV,
        .Width = uint32_t(texture.Width),
        .Height = uint32_t(texture.Height),
    };
    BindTempConstantBuffer(convertCmdList, cbData, URS_ConstantBuffers + 0, CmdListMode::Compute);

    uint32_t dispatchX = DispatchSize(texture.Width, convertTGSize);
    uint32_t dispatchY = DispatchSize(texture.Height, convertTGSize);
    uint32_t dispatchZ = texture.ArraySize;
    convertCmdList->Dispatch(dispatchX, dispatchY, dispatchZ);

    DX12::Barrier(convertCmdList, convertBuffer.InternalBuffer.WriteToReadBarrier({
        .SyncBefore = D3D12_BARRIER_SYNC_COMPUTE_SHADING,
        .SyncAfter = D3D12_BARRIER_SYNC_COPY,
        .AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
        .AccessAfter = D3D12_BARRIER_ACCESS_COPY_SOURCE,
    }));

    // convertBuffer.Transition(convertCmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

    readbackBuffer.Shutdown();
    readbackBuffer.Initialize(convertBuffer.InternalBuffer.Size);

    convertCmdList->CopyResource(readbackBuffer.Resource, convertBuffer.InternalBuffer.Resource);

    // Execute the conversion command list and signal a fence
    DXCall(convertCmdList->Close());
    ID3D12CommandList* cmdLists[1] = { convertCmdList };
    convertCmdQueue->ExecuteCommandLists(1, cmdLists);

    convertFence.Signal(convertCmdQueue, 1);
    convertFence.Wait(1);

    // Clean up
    convertFence.Clear(0);

    DXCall(convertCmdAllocator->Reset());
    DXCall(convertCmdList->Reset(convertCmdAllocator, nullptr));

    convertBuffer.Shutdown();
}

void ClearRawBuffer(ID3D12GraphicsCommandList* cmdList, const RawBuffer& buffer, const Uint4& clearValue)
{
    cmdList->SetComputeRootSignature(UniversalRootSignature);
    cmdList->SetPipelineState(clearRawBufferPSO);

    Assert_(buffer.UAV != uint32_t(-1));

    ClearRawBufferConstants cbData =
    {
        .ClearValue = clearValue,
        .DescriptorIdx = buffer.UAV,
        .Num16ByteElements = uint32_t(AlignTo(buffer.NumElements * buffer.Stride, 16) / 16),
    };
    BindTempConstantBuffer(cmdList, cbData, URS_ConstantBuffers + 0, CmdListMode::Compute);

    uint32_t dispatchX = DispatchSize(cbData.Num16ByteElements, clearRawBufferTGSize);
    cmdList->Dispatch(dispatchX, 1, 1);
}

void ClearRawBuffer(ID3D12GraphicsCommandList* cmdList, const RawBuffer& buffer, const Float4& clearValue)
{
    Uint4 clearValueUint;
    memcpy(&clearValueUint, &clearValue, sizeof(clearValue));
    ClearRawBuffer(cmdList, buffer, clearValueUint);
}

ID3D12PipelineState* CreateComputePSO(const CompiledShaderPtr& shader)
{
    ID3D12PipelineState* pso = nullptr;
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc =
    {
        .pRootSignature = DX12::UniversalRootSignature,
        .CS = shader.ByteCode(),
        .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
    };
    DXCall(DX12::Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pso)));

    return pso;
}

} // namespace DX12

} // namespace SampleFramework12