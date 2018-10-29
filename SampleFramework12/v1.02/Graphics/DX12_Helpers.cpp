//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "DX12_Helpers.h"
#include "DX12.h"
#include "DX12_Upload.h"
#include "GraphicsTypes.h"

namespace SampleFramework12
{

namespace DX12
{

uint32 RTVDescriptorSize = 0;
uint32 SRVDescriptorSize = 0;
uint32 UAVDescriptorSize = 0;
uint32 CBVDescriptorSize = 0;
uint32 DSVDescriptorSize = 0;

DescriptorHeap RTVDescriptorHeap;
DescriptorHeap SRVDescriptorHeap;
DescriptorHeap DSVDescriptorHeap;
DescriptorHeap UAVDescriptorHeap;

uint32 NullTexture2DSRV = uint32(-1);

static const uint64 NumBlendStates = uint64(BlendState::NumValues);
static const uint64 NumRasterizerStates = uint64(RasterizerState::NumValues);
static const uint64 NumDepthStates = uint64(DepthState::NumValues);
static const uint64 NumSamplerStates = uint64(SamplerState::NumValues);

static D3D12_BLEND_DESC BlendStateDescs[NumBlendStates] = { };
static D3D12_RASTERIZER_DESC RasterizerStateDescs[NumRasterizerStates] = { };
static D3D12_DEPTH_STENCIL_DESC DepthStateDescs[NumBlendStates] = { };
static D3D12_SAMPLER_DESC SamplerStateDescs[NumSamplerStates] = { };

static D3D12_DESCRIPTOR_RANGE1 StandardDescriptorRangeDescs[NumStandardDescriptorRanges] = { };

void Initialize_Helpers()
{
    RTVDescriptorHeap.Init(256, 0, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);
    SRVDescriptorHeap.Init(1024, 1024, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
    DSVDescriptorHeap.Init(256, 0, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false);
    UAVDescriptorHeap.Init(256, 0, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false);

    RTVDescriptorSize = RTVDescriptorHeap.DescriptorSize;
    SRVDescriptorSize = UAVDescriptorSize = CBVDescriptorSize = SRVDescriptorHeap.DescriptorSize;
    DSVDescriptorSize = DSVDescriptorHeap.DescriptorSize;

    // Standard descriptor ranges for binding to the arrays in DescriptorTables.hlsl
    InsertStandardDescriptorRanges(StandardDescriptorRangeDescs);

    // Blend state initialization
    {
        D3D12_BLEND_DESC& blendDesc = BlendStateDescs[uint64(BlendState::Disabled)];
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
        D3D12_BLEND_DESC& blendDesc = BlendStateDescs[uint64(BlendState::Additive)];
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
        D3D12_BLEND_DESC& blendDesc = BlendStateDescs[uint64(BlendState::AlphaBlend)];
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
        D3D12_BLEND_DESC& blendDesc = BlendStateDescs[uint64(BlendState::PreMultiplied)];
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
        D3D12_BLEND_DESC& blendDesc = BlendStateDescs[uint64(BlendState::NoColorWrites)];
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
        D3D12_BLEND_DESC& blendDesc = BlendStateDescs[uint64(BlendState::PreMultipliedRGB)];
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
        D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[uint64(RasterizerState::NoCull)];
        rastDesc.CullMode = D3D12_CULL_MODE_NONE;
        rastDesc.DepthClipEnable = true;
        rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rastDesc.MultisampleEnable = true;
    }

    {
        D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[uint64(RasterizerState::FrontFaceCull)];
        rastDesc.CullMode = D3D12_CULL_MODE_FRONT;
        rastDesc.DepthClipEnable = true;
        rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rastDesc.MultisampleEnable = true;
    }

    {
        D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[uint64(RasterizerState::BackFaceCull)];
        rastDesc.CullMode = D3D12_CULL_MODE_BACK;
        rastDesc.DepthClipEnable = true;
        rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rastDesc.MultisampleEnable = true;
    }

    {
        D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[uint64(RasterizerState::BackFaceCullNoZClip)];
        rastDesc.CullMode = D3D12_CULL_MODE_BACK;
        rastDesc.DepthClipEnable = false;
        rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rastDesc.MultisampleEnable = true;
    }

    {
        D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[uint64(RasterizerState::NoCullNoMS)];
        rastDesc.CullMode = D3D12_CULL_MODE_NONE;
        rastDesc.DepthClipEnable = true;
        rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rastDesc.MultisampleEnable = false;
    }

    {
        D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[uint64(RasterizerState::Wireframe)];
        rastDesc.CullMode = D3D12_CULL_MODE_NONE;
        rastDesc.DepthClipEnable = true;
        rastDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
        rastDesc.MultisampleEnable = true;
    }

    // Depth state initialization
    {
        D3D12_DEPTH_STENCIL_DESC& dsDesc = DepthStateDescs[uint64(DepthState::Disabled)];
        dsDesc.DepthEnable = false;
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    }

    {
        D3D12_DEPTH_STENCIL_DESC& dsDesc = DepthStateDescs[uint64(DepthState::Enabled)];
        dsDesc.DepthEnable = true;
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    }

    {
        D3D12_DEPTH_STENCIL_DESC& dsDesc = DepthStateDescs[uint64(DepthState::Reversed)];
        dsDesc.DepthEnable = true;
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    }

    {
        D3D12_DEPTH_STENCIL_DESC& dsDesc = DepthStateDescs[uint64(DepthState::WritesEnabled)];
        dsDesc.DepthEnable = true;
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    }

    {
        D3D12_DEPTH_STENCIL_DESC& dsDesc = DepthStateDescs[uint64(DepthState::ReversedWritesEnabled)];
        dsDesc.DepthEnable = true;
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    }

    // Sampler state initialization
    {
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64(SamplerState::Linear)];

        sampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampDesc.MipLODBias = 0.0f;
        sampDesc.MaxAnisotropy = 1;
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampDesc.BorderColor[0] = sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 0;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
    }

    {
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64(SamplerState::LinearClamp)];

        sampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.MipLODBias = 0.0f;
        sampDesc.MaxAnisotropy = 1;
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampDesc.BorderColor[0] = sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 0;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
    }

    {
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64(SamplerState::LinearBorder)];

        sampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampDesc.MipLODBias = 0.0f;
        sampDesc.MaxAnisotropy = 1;
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampDesc.BorderColor[0] = sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 0;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
    }

    {
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64(SamplerState::Point)];

        sampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.MipLODBias = 0.0f;
        sampDesc.MaxAnisotropy = 1;
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampDesc.BorderColor[0] = sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 0;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
    }

    {
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64(SamplerState::Anisotropic)];

        sampDesc.Filter = D3D12_FILTER_ANISOTROPIC;
        sampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampDesc.MipLODBias = 0.0f;
        sampDesc.MaxAnisotropy = 16;
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampDesc.BorderColor[0] = sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 0;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
    }

    {
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64(SamplerState::ShadowMap)];

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
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64(SamplerState::ShadowMapPCF)];

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
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64(SamplerState::ReversedShadowMap)];

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
        D3D12_SAMPLER_DESC& sampDesc = SamplerStateDescs[uint64(SamplerState::ReversedShadowMapPCF)];

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
        for(uint32 i = 0; i < SRVDescriptorHeap.NumHeaps; ++i)
            Device->CreateShaderResourceView(nullptr, &srvDesc, srvAlloc.Handles[i]);
        NullTexture2DSRV = srvAlloc.Index;
    }
}

void Shutdown_Helpers()
{
    SRVDescriptorHeap.FreePersistent(NullTexture2DSRV);

    RTVDescriptorHeap.Shutdown();
    SRVDescriptorHeap.Shutdown();
    DSVDescriptorHeap.Shutdown();
    UAVDescriptorHeap.Shutdown();
}

void EndFrame_Helpers()
{
    RTVDescriptorHeap.EndFrame();
    SRVDescriptorHeap.EndFrame();
    DSVDescriptorHeap.EndFrame();
    UAVDescriptorHeap.EndFrame();
}

void TransitionResource(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, uint32 subResource)
{
    D3D12_RESOURCE_BARRIER barrier = { };
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = subResource;
    cmdList->ResourceBarrier(1, &barrier);
}

uint64 GetResourceSize(const D3D12_RESOURCE_DESC& desc, uint32 firstSubResource, uint32 numSubResources)
{
    uint64 size = 0;
    Device->GetCopyableFootprints(&desc, firstSubResource, numSubResources, 0, nullptr, nullptr, nullptr, &size);
    return size;
}

uint64 GetResourceSize(ID3D12Resource* resource, uint32 firstSubResource, uint32 numSubResources)
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

D3D12_BLEND_DESC GetBlendState(BlendState blendState)
{
    Assert_(uint64(blendState) < ArraySize_(BlendStateDescs));
    return BlendStateDescs[uint64(blendState)];
}

D3D12_RASTERIZER_DESC GetRasterizerState(RasterizerState rasterizerState)
{
    Assert_(uint64(rasterizerState) < ArraySize_(RasterizerStateDescs));
    return RasterizerStateDescs[uint64(rasterizerState)];
}

D3D12_DEPTH_STENCIL_DESC GetDepthState(DepthState depthState)
{
    Assert_(uint64(depthState) < ArraySize_(DepthStateDescs));
    return DepthStateDescs[uint64(depthState)];
}

D3D12_SAMPLER_DESC GetSamplerState(SamplerState samplerState)
{
    Assert_(uint64(samplerState) < ArraySize_(SamplerStateDescs));
    return SamplerStateDescs[uint64(samplerState)];
}

D3D12_STATIC_SAMPLER_DESC GetStaticSamplerState(SamplerState samplerState, uint32 shaderRegister,
                                                uint32 registerSpace, D3D12_SHADER_VISIBILITY visibility)
{
    Assert_(uint64(samplerState) < ArraySize_(SamplerStateDescs));
    return ConvertToStaticSampler(SamplerStateDescs[uint64(samplerState)], shaderRegister, registerSpace, visibility);
}

D3D12_STATIC_SAMPLER_DESC ConvertToStaticSampler(const D3D12_SAMPLER_DESC& samplerDesc, uint32 shaderRegister,
                                                 uint32 registerSpace, D3D12_SHADER_VISIBILITY visibility)
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

void SetViewport(ID3D12GraphicsCommandList* cmdList, uint64 width, uint64 height, float zMin, float zMax)
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
    scissorRect.right = uint32(width);
    scissorRect.bottom = uint32(height);

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
            throw DXException(hr, MakeString(L"Failed to create root signature: %s", errString).c_str());
        #endif
    }

    DXCall(DX12::Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(rootSignature)));
}

uint32 DispatchSize(uint64 numElements, uint64 groupSize)
{
    Assert_(groupSize > 0);
    return uint32((numElements + (groupSize - 1)) / groupSize);
}

static const uint64 MaxBindCount = 16;
static const uint32 DescriptorCopyRanges[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
StaticAssert_(ArraySize_(DescriptorCopyRanges) == MaxBindCount);

void SetDescriptorHeaps(ID3D12GraphicsCommandList* cmdList)
{
    ID3D12DescriptorHeap* heaps[] =
    {
        SRVDescriptorHeap.CurrentHeap(),
    };

    cmdList->SetDescriptorHeaps(ArraySize_(heaps), heaps);
}

D3D12_GPU_DESCRIPTOR_HANDLE TempDescriptorTable(const D3D12_CPU_DESCRIPTOR_HANDLE* handles, uint64 count)
{
    Assert_(count <= MaxBindCount);
    Assert_(count > 0);

    TempDescriptorAlloc tempAlloc = SRVDescriptorHeap.AllocateTemporary(uint32(count));

    uint32 destRanges[1] = { uint32(count) };
    Device->CopyDescriptors(1, &tempAlloc.StartCPUHandle, destRanges, uint32(count), handles, DescriptorCopyRanges, SRVDescriptorHeap.HeapType);

    return tempAlloc.StartGPUHandle;
}

void BindTempDescriptorTable(ID3D12GraphicsCommandList* cmdList, const D3D12_CPU_DESCRIPTOR_HANDLE* handles,
                             uint64 count, uint32 rootParameter, CmdListMode cmdListMode)
{
    D3D12_GPU_DESCRIPTOR_HANDLE tempTable = TempDescriptorTable(handles, count);

    if(cmdListMode == CmdListMode::Graphics)
        cmdList->SetGraphicsRootDescriptorTable(rootParameter, tempTable);
    else
        cmdList->SetComputeRootDescriptorTable(rootParameter, tempTable);
}

TempBuffer TempConstantBuffer(uint64 cbSize, bool makeDescriptor)
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
        cbvDesc.SizeInBytes = uint32(AlignTo(cbSize, ConstantBufferAlignment));
        DX12::Device->CreateConstantBufferView(&cbvDesc, cbvAlloc.StartCPUHandle);
        tempBuffer.DescriptorIndex = cbvAlloc.StartIndex;
    }

    return tempBuffer;
}

void BindTempConstantBuffer(ID3D12GraphicsCommandList* cmdList, const void* cbData, uint64 cbSize, uint32 rootParameter, CmdListMode cmdListMode)
{
    TempBuffer tempBuffer = TempConstantBuffer(cbSize, false);
    memcpy(tempBuffer.CPUAddress, cbData, cbSize);

    if(cmdListMode == CmdListMode::Graphics)
        cmdList->SetGraphicsRootConstantBufferView(rootParameter, tempBuffer.GPUAddress);
    else
        cmdList->SetComputeRootConstantBufferView(rootParameter, tempBuffer.GPUAddress);
}

TempBuffer TempStructuredBuffer(uint64 numElements, uint64 stride, bool makeDescriptor)
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
        srvDesc.Buffer.FirstElement = uint32(tempMem.ResourceOffset / stride);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        srvDesc.Buffer.NumElements = uint32(numElements);
        srvDesc.Buffer.StructureByteStride = uint32(stride);
        DX12::Device->CreateShaderResourceView(tempMem.Resource, &srvDesc, srvAlloc.StartCPUHandle);

        result.DescriptorIndex = srvAlloc.StartIndex;
    }

    return result;
}

TempBuffer TempFormattedBuffer(uint64 numElements, DXGI_FORMAT format, bool makeDescriptor)
{
    Assert_(format != DXGI_FORMAT_UNKNOWN);
    Assert_(numElements > 0);
    uint64 stride = DirectX::BitsPerPixel(format) / 8;

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
        srvDesc.Buffer.FirstElement = uint32(tempMem.ResourceOffset / stride);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        srvDesc.Buffer.NumElements = uint32(numElements);
        DX12::Device->CreateShaderResourceView(tempMem.Resource, &srvDesc, srvAlloc.StartCPUHandle);

        result.DescriptorIndex = srvAlloc.StartIndex;
    }

    return result;
}

TempBuffer TempRawBuffer(uint64 numElements, bool makeDescriptor)
{
    Assert_(numElements > 0);
    const uint64 stride = 4;

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
        srvDesc.Buffer.FirstElement = uint32(tempMem.ResourceOffset / stride);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        srvDesc.Buffer.NumElements = uint32(numElements);
        DX12::Device->CreateShaderResourceView(tempMem.Resource, &srvDesc, srvAlloc.StartCPUHandle);

        result.DescriptorIndex = srvAlloc.StartIndex;
    }

    return result;
}

const D3D12_DESCRIPTOR_RANGE1* StandardDescriptorRanges()
{
    Assert_(SRVDescriptorSize != 0);
    return StandardDescriptorRangeDescs;
}

void InsertStandardDescriptorRanges(D3D12_DESCRIPTOR_RANGE1* ranges)
{
    uint32 userStart = NumStandardDescriptorRanges - NumUserDescriptorRanges;
    for(uint32 i = 0; i < NumStandardDescriptorRanges; ++i)
    {
        StandardDescriptorRangeDescs[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        StandardDescriptorRangeDescs[i].NumDescriptors = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        StandardDescriptorRangeDescs[i].BaseShaderRegister = 0;
        StandardDescriptorRangeDescs[i].RegisterSpace = i;
        StandardDescriptorRangeDescs[i].OffsetInDescriptorsFromTableStart = 0;
        StandardDescriptorRangeDescs[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
        if(i >= userStart)
            StandardDescriptorRangeDescs[i].RegisterSpace = (i - userStart) + 100;
    }
}

void BindAsDescriptorTable(ID3D12GraphicsCommandList* cmdList, uint32 descriptorIdx, uint32 rootParameter, CmdListMode cmdListMode)
{
    Assert_(descriptorIdx != uint32(-1));
    D3D12_GPU_DESCRIPTOR_HANDLE handle = SRVDescriptorHeap.GPUHandleFromIndex(descriptorIdx);
    if(cmdListMode == CmdListMode::Compute)
        cmdList->SetComputeRootDescriptorTable(rootParameter, handle);
    else
        cmdList->SetGraphicsRootDescriptorTable(rootParameter, handle);
}

void BindStandardDescriptorTable(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter, CmdListMode cmdListMode)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = SRVDescriptorHeap.GPUStart[SRVDescriptorHeap.HeapIndex];
    if(cmdListMode == CmdListMode::Compute)
        cmdList->SetComputeRootDescriptorTable(rootParameter, handle);
    else
        cmdList->SetGraphicsRootDescriptorTable(rootParameter, handle);
}

} // namespace DX12

} // namespace SampleFramework12