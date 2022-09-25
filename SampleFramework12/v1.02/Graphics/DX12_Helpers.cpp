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

namespace AppSettings
{
    extern const uint32 CBufferRegister;
}

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
D3D12_CPU_DESCRIPTOR_HANDLE NullTexture2DUAV = { };
D3D12_CPU_DESCRIPTOR_HANDLE NullStructuredBufferUAV = { };
D3D12_CPU_DESCRIPTOR_HANDLE NullRawBufferUAV = { };

ID3D12RootSignature* UniversalRootSignature = nullptr;
ID3D12RootSignature* UniversalRootSignatureWithIA = nullptr;

static const uint64 NumBlendStates = uint64(BlendState::NumValues);
static const uint64 NumRasterizerStates = uint64(RasterizerState::NumValues);
static const uint64 NumDepthStates = uint64(DepthState::NumValues);
static const uint64 NumSamplerStates = uint64(SamplerState::NumValues);

static D3D12_BLEND_DESC BlendStateDescs[NumBlendStates] = { };
static D3D12_RASTERIZER_DESC RasterizerStateDescs[NumRasterizerStates] = { };
static D3D12_DEPTH_STENCIL_DESC DepthStateDescs[NumBlendStates] = { };
static D3D12_SAMPLER_DESC SamplerStateDescs[NumSamplerStates] = { };

static D3D12_DESCRIPTOR_RANGE1 GlobalSRVDescriptorRangeDescs[NumGlobalSRVDescriptorRanges] = { };

static const uint32 URSMaxUAVs = 8;

// Resources for ConvertAndReadbackTexture
enum ConvertRootParams : uint32
{
    ConvertParams_UAV = 0,
    ConvertParams_CBuffer,

    NumConvertRootParams
};

static ID3D12GraphicsCommandList1* convertCmdList = nullptr;
static ID3D12CommandQueue* convertCmdQueue = nullptr;
static ID3D12CommandAllocator* convertCmdAllocator = nullptr;
static ID3D12RootSignature* convertRootSignature = nullptr;
static ID3D12PipelineState* convertPSO = nullptr;
static ID3D12PipelineState* convertArrayPSO = nullptr;
static ID3D12PipelineState* convertCubePSO = nullptr;
static CompiledShaderPtr convertCS;
static CompiledShaderPtr convertArrayCS;
static CompiledShaderPtr convertCubeCS;
static uint32 convertTGSize = 8;
static Fence convertFence;

void Initialize_Helpers()
{
    RTVDescriptorHeap.Init(256, 0, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);
    SRVDescriptorHeap.Init(4096, 4096, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
    DSVDescriptorHeap.Init(256, 0, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false);
    UAVDescriptorHeap.Init(256, 0, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false);

    RTVDescriptorSize = RTVDescriptorHeap.DescriptorSize;
    SRVDescriptorSize = UAVDescriptorSize = CBVDescriptorSize = SRVDescriptorHeap.DescriptorSize;
    DSVDescriptorSize = DSVDescriptorHeap.DescriptorSize;

    // Standard descriptor ranges for binding to the arrays in DescriptorTables.hlsl
    InsertGlobalSRVDescriptorRanges(GlobalSRVDescriptorRangeDescs);

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
        D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[uint64(RasterizerState::NoCullNoMS)];
        rastDesc.CullMode = D3D12_CULL_MODE_NONE;
        rastDesc.DepthClipEnable = true;
        rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rastDesc.MultisampleEnable = false;
    }

    {
        D3D12_RASTERIZER_DESC& rastDesc = RasterizerStateDescs[uint64(RasterizerState::NoCullNoZClip)];
        rastDesc.CullMode = D3D12_CULL_MODE_NONE;
        rastDesc.DepthClipEnable = true;
        rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rastDesc.MultisampleEnable = true;
        rastDesc.DepthClipEnable = false;
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
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
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
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
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
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
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
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
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
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
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

    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = { };
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        uavDesc.Texture2D.MipSlice = 0;
        uavDesc.Texture2D.PlaneSlice = 0;

        NullTexture2DUAV = UAVDescriptorHeap.AllocatePersistent().Handles[0];
        DX12::Device->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, NullTexture2DUAV);
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

        NullStructuredBufferUAV = UAVDescriptorHeap.AllocatePersistent().Handles[0];
        DX12::Device->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, NullStructuredBufferUAV);
    }

    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = { };
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        uavDesc.Buffer.NumElements = 1;

        NullRawBufferUAV = UAVDescriptorHeap.AllocatePersistent().Handles[0];
        DX12::Device->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, NullRawBufferUAV);
    }

    {
        // Universal root signature
        D3D12_ROOT_PARAMETER1 rootParameters[NumUniversalRootSignatureParams] = {};

        // Global SRV descriptors
        rootParameters[URS_GlobalSRVTable].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[URS_GlobalSRVTable].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[URS_GlobalSRVTable].DescriptorTable.pDescriptorRanges = DX12::GlobalSRVDescriptorRanges();
        rootParameters[URS_GlobalSRVTable].DescriptorTable.NumDescriptorRanges = DX12::NumGlobalSRVDescriptorRanges;

        // UAV descriptor table
        D3D12_DESCRIPTOR_RANGE1 uavRanges[1] = {};
        uavRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRanges[0].NumDescriptors = URSMaxUAVs;
        uavRanges[0].BaseShaderRegister = 0;
        uavRanges[0].RegisterSpace = 0;
        uavRanges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
        uavRanges[0].OffsetInDescriptorsFromTableStart = 0;

        rootParameters[URS_UAVTable].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[URS_UAVTable].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[URS_UAVTable].DescriptorTable.pDescriptorRanges = uavRanges;
        rootParameters[URS_UAVTable].DescriptorTable.NumDescriptorRanges = ArraySize_(uavRanges);

        // Constant buffers
        for(uint32 i = 0; i < NumUniversalRootSignatureConstantBuffers; ++i)
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
        D3D12_STATIC_SAMPLER_DESC staticSamplers[uint64(SamplerState::NumValues)] = {};
        for(uint32 i = 0; i < uint32(SamplerState::NumValues); ++i)
            staticSamplers[i] = GetStaticSamplerState(SamplerState(i), i, 0, D3D12_SHADER_VISIBILITY_ALL);

        D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
        rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.NumStaticSamplers = ArraySize_(staticSamplers);
        rootSignatureDesc.pStaticSamplers = staticSamplers;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        DX12::CreateRootSignature(&UniversalRootSignature, rootSignatureDesc);

        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        DX12::CreateRootSignature(&UniversalRootSignatureWithIA, rootSignatureDesc);
    }

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
    const std::wstring shaderPath = SampleFrameworkDir() + L"Shaders\\DecodeTextureCS.hlsl";
    convertCS = CompileFromFile(shaderPath.c_str(), "DecodeTextureCS", ShaderType::Compute, opts);
    convertArrayCS = CompileFromFile(shaderPath.c_str(), "DecodeTextureArrayCS", ShaderType::Compute, opts);
    convertCubeCS = CompileFromFile(shaderPath.c_str(), "DecodeTextureCubeCS", ShaderType::Compute, opts);

    {
        D3D12_DESCRIPTOR_RANGE1 uavRanges[1] = {};
        uavRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRanges[0].NumDescriptors = 1;
        uavRanges[0].BaseShaderRegister = 0;
        uavRanges[0].RegisterSpace = 0;
        uavRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER1 rootParameters[NumConvertRootParams] = {};
        rootParameters[ConvertParams_UAV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[ConvertParams_UAV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[ConvertParams_UAV].DescriptorTable.pDescriptorRanges = uavRanges;
        rootParameters[ConvertParams_UAV].DescriptorTable.NumDescriptorRanges = ArraySize_(uavRanges);

        rootParameters[ConvertParams_CBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParameters[ConvertParams_CBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[ConvertParams_CBuffer].Constants.Num32BitValues = 3;
        rootParameters[ConvertParams_CBuffer].Constants.RegisterSpace = 0;
        rootParameters[ConvertParams_CBuffer].Constants.ShaderRegister = 0;

        D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
        staticSamplers[0] = DX12::GetStaticSamplerState(SamplerState::Point, 0, 0, D3D12_SHADER_VISIBILITY_ALL);

        D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
        rootSignatureDesc.NumParameters = ArraySize_(rootParameters);
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.NumStaticSamplers = ArraySize_(staticSamplers);
        rootSignatureDesc.pStaticSamplers = staticSamplers;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

        DX12::CreateRootSignature(&convertRootSignature, rootSignatureDesc);
    }

    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = { };
        psoDesc.CS = convertCS.ByteCode();
        psoDesc.pRootSignature = convertRootSignature;
        psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&convertPSO));

        psoDesc.CS = convertArrayCS.ByteCode();
        Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&convertArrayPSO));

        psoDesc.CS = convertCubeCS.ByteCode();
        Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&convertCubePSO));
    }

    convertFence.Init(0);
}

void Shutdown_Helpers()
{
    SRVDescriptorHeap.FreePersistent(NullTexture2DSRV);
    UAVDescriptorHeap.FreePersistent(NullTexture2DUAV);
    UAVDescriptorHeap.FreePersistent(NullStructuredBufferUAV);
    UAVDescriptorHeap.FreePersistent(NullRawBufferUAV);

    RTVDescriptorHeap.Shutdown();
    SRVDescriptorHeap.Shutdown();
    DSVDescriptorHeap.Shutdown();
    UAVDescriptorHeap.Shutdown();

    DX12::Release(UniversalRootSignature);
    DX12::Release(UniversalRootSignatureWithIA);

    Release(convertCmdAllocator);
    Release(convertCmdList);
    Release(convertCmdQueue);
    Release(convertPSO);
    Release(convertArrayPSO);
    Release(convertCubePSO);
    Release(convertRootSignature);
    convertFence.Shutdown();
}

void EndFrame_Helpers()
{
    RTVDescriptorHeap.EndFrame();
    SRVDescriptorHeap.EndFrame();
    DSVDescriptorHeap.EndFrame();
    UAVDescriptorHeap.EndFrame();
}

D3D12_RESOURCE_BARRIER MakeTransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, uint32 subResource)
{
    D3D12_RESOURCE_BARRIER barrier = { };
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = subResource;
    return barrier;
}

void TransitionResource(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, uint32 subResource)
{
    D3D12_RESOURCE_BARRIER barrier = MakeTransitionBarrier(resource, before, after, subResource);
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

void BindUAVDescriptorTableToURS(ID3D12GraphicsCommandList* cmdList, const D3D12_CPU_DESCRIPTOR_HANDLE* handles, uint64 count, CmdListMode cmdListMode)
{
    Assert_(count <= URSMaxUAVs);
    D3D12_CPU_DESCRIPTOR_HANDLE uavs[URSMaxUAVs] = { };
    for(uint64 i = 0; i < count; ++i)
        uavs[i] = handles[i];

    for(uint64 i = count; i < URSMaxUAVs; ++i)
        uavs[i] = NullRawBufferUAV;

    BindTempDescriptorTable(cmdList, uavs, URSMaxUAVs, URS_UAVTable, cmdListMode);
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

const D3D12_DESCRIPTOR_RANGE1* GlobalSRVDescriptorRanges()
{
    Assert_(SRVDescriptorSize != 0);
    return GlobalSRVDescriptorRangeDescs;
}

void InsertGlobalSRVDescriptorRanges(D3D12_DESCRIPTOR_RANGE1* ranges)
{
    uint32 userStart = NumGlobalSRVDescriptorRanges - NumUserDescriptorRanges;
    for(uint32 i = 0; i < NumGlobalSRVDescriptorRanges; ++i)
    {
        GlobalSRVDescriptorRangeDescs[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        GlobalSRVDescriptorRangeDescs[i].NumDescriptors = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        GlobalSRVDescriptorRangeDescs[i].BaseShaderRegister = 0;
        GlobalSRVDescriptorRangeDescs[i].RegisterSpace = i;
        GlobalSRVDescriptorRangeDescs[i].OffsetInDescriptorsFromTableStart = 0;
        GlobalSRVDescriptorRangeDescs[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
        if(i >= userStart)
            GlobalSRVDescriptorRangeDescs[i].RegisterSpace = (i - userStart) + 100;
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

void BindGlobalSRVDescriptorTable(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter, CmdListMode cmdListMode)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = SRVDescriptorHeap.GPUStart[SRVDescriptorHeap.HeapIndex];
    if(cmdListMode == CmdListMode::Compute)
        cmdList->SetComputeRootDescriptorTable(rootParameter, handle);
    else
        cmdList->SetGraphicsRootDescriptorTable(rootParameter, handle);
}

void BindGlobalSRVDescriptorTableToURS(ID3D12GraphicsCommandList* cmdList, CmdListMode cmdListMode)
{
    BindGlobalSRVDescriptorTable(cmdList, URS_GlobalSRVTable, cmdListMode);
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
    init.InitialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    FormattedBuffer convertBuffer;
    convertBuffer.Initialize(init);

    // Run the conversion compute shader
    DX12::SetDescriptorHeaps(convertCmdList);
    convertCmdList->SetComputeRootSignature(convertRootSignature);

    if(texture.Cubemap)
        convertCmdList->SetPipelineState(convertCubePSO);
    else if(texture.ArraySize > 1)
        convertCmdList->SetPipelineState(convertArrayPSO);
    else
        convertCmdList->SetPipelineState(convertPSO);

    D3D12_CPU_DESCRIPTOR_HANDLE uavs[] = { convertBuffer.UAV };
    BindTempDescriptorTable(convertCmdList, uavs, ArraySize_(uavs), ConvertParams_UAV, CmdListMode::Compute);

    convertCmdList->SetComputeRoot32BitConstant(ConvertParams_CBuffer, texture.SRV, 0);
    convertCmdList->SetComputeRoot32BitConstant(ConvertParams_CBuffer, uint32(texture.Width), 1);
    convertCmdList->SetComputeRoot32BitConstant(ConvertParams_CBuffer, uint32(texture.Height), 2);

    uint32 dispatchX = DispatchSize(texture.Width, convertTGSize);
    uint32 dispatchY = DispatchSize(texture.Height, convertTGSize);
    uint32 dispatchZ = texture.ArraySize;
    convertCmdList->Dispatch(dispatchX, dispatchY, dispatchZ);

    convertBuffer.Transition(convertCmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

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

} // namespace DX12

} // namespace SampleFramework12