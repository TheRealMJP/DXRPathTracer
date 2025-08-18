//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "../PCH.h"
#include "../SF12_Assert.h"
#include "DX12.h"
#include "Utility.h"
#include "../Shaders/ShaderShared.h"

namespace SampleFramework12
{

// Forward declarations
struct DescriptorHeap;
struct DescriptorHandle;
struct LinearDescriptorHeap;
struct Texture;
struct ReadbackBuffer;
struct RawBuffer;
struct Uint4;
struct Float4;
class CompiledShaderPtr;

enum class BlendState : uint64_t
{
    Disabled = 0,
    Additive,
    AlphaBlend,
    PreMultiplied,
    NoColorWrites,
    PreMultipliedRGB,

    NumValues
};

enum class RasterizerState : uint64_t
{
    NoCull = 0,
    NoCullNoMS,
    NoCullNoZClip,
    BackFaceCull,
    BackFaceCullNoZClip,
    FrontFaceCull,
    Wireframe,

    NumValues
};

enum class DepthState : uint64_t
{
    Disabled = 0,
    Enabled,
    Reversed,
    WritesEnabled,
    ReversedWritesEnabled,
    Equal,
    DepthFail,

    NumValues
};

enum class SamplerState : uint64_t
{
    Linear = 0,
    LinearClamp,
    LinearBorder,
    Point,
    Anisotropic,
    ShadowMap,
    ShadowMapPCF,
    ReversedShadowMap,
    ReversedShadowMapPCF,

    NumValues
};

enum class CmdListMode : uint64_t
{
    Graphics = 0,
    Compute,
};

// Universal root signature enum
enum UniversalRootSignatureParams : uint32_t
{
    URS_ConstantBuffers,
    URS_ConstantBuffersEnd = URS_ConstantBuffers + 7,
    URS_AppSettings,

    NumUniversalRootSignatureParams,
    NumUniversalRootSignatureConstantBuffers = (URS_ConstantBuffersEnd - URS_ConstantBuffers) + 1
};

struct TempBuffer
{
    void* CPUAddress = nullptr;
    uint64_t GPUAddress = 0;
    uint32_t DescriptorIndex = uint32_t(-1);
};

// Enhanced barrier helper types
struct BarrierBatch
{
    const D3D12_BUFFER_BARRIER* BufferBarriers = nullptr;
    uint32_t NumBufferBarriers = 0;
    const D3D12_TEXTURE_BARRIER* TextureBarriers = nullptr;
    uint32_t NumTextureBarriers = 0;
    const D3D12_GLOBAL_BARRIER* GlobalBarriers = nullptr;
    uint32_t NumGlobalBarriers = 0;
};

struct BarrierBatchBuilder
{
    D3D12_BUFFER_BARRIER BufferBarriers[16] = { };
    uint32_t NumBufferBarriers = 0;
    D3D12_TEXTURE_BARRIER TextureBarriers[16] = { };
    uint32_t NumTextureBarriers = 0;
    D3D12_GLOBAL_BARRIER GlobalBarriers[16] = { };
    uint32_t NumGlobalBarriers = 0;

    void Add(D3D12_BUFFER_BARRIER barrier)
    {
        Assert_(NumBufferBarriers < ArraySize_(BufferBarriers));
        BufferBarriers[NumBufferBarriers++] = barrier;
    }

    void Add(D3D12_TEXTURE_BARRIER barrier)
    {
        Assert_(NumTextureBarriers < ArraySize_(TextureBarriers));
        TextureBarriers[NumTextureBarriers++] = barrier;
    }

    void Add(D3D12_GLOBAL_BARRIER barrier)
    {
        Assert_(NumGlobalBarriers < ArraySize_(GlobalBarriers));
        GlobalBarriers[NumGlobalBarriers++] = barrier;
    }

    BarrierBatch Build() const
    {
        return
        {
            .BufferBarriers = BufferBarriers,
            .NumBufferBarriers = NumBufferBarriers,
            .TextureBarriers = TextureBarriers,
            .NumTextureBarriers = NumTextureBarriers,
            .GlobalBarriers = GlobalBarriers,
            .NumGlobalBarriers = NumGlobalBarriers,
        };
    }
};

namespace DX12
{

// Constants
const uint64_t ConstantBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
const uint64_t VertexBufferAlignment = 4;
const uint64_t IndexBufferAlignment = 4;
const uint32_t StandardMSAAPattern = 0xFFFFFFFF;

const uint32_t NumUserDescriptorRanges = 16;
const uint32_t NumGlobalSRVDescriptorRanges = 7 + NumUserDescriptorRanges;

// Externals
extern uint32_t RTVDescriptorSize;
extern uint32_t SRVDescriptorSize;
extern uint32_t UAVDescriptorSize;
extern uint32_t CBVDescriptorSize;
extern uint32_t DSVDescriptorSize;

extern DescriptorHeap RTVDescriptorHeap;
extern DescriptorHeap SRVDescriptorHeap;
extern DescriptorHeap DSVDescriptorHeap;

extern DescriptorIndex NullTexture2DSRV;
extern DescriptorIndex NullTexture2DUAV;
extern DescriptorIndex NullStructuredBufferUAV;
extern DescriptorIndex NullRawBufferUAV;

extern ID3D12RootSignature* UniversalRootSignature;
extern ID3D12RootSignature* UniversalRootSignatureWithIA;

// Lifetime
void Initialize_Helpers();
void Shutdown_Helpers();

void EndFrame_Helpers();

// Enhanced barriers
void Barrier(ID3D12GraphicsCommandList7* cmdList, const D3D12_GLOBAL_BARRIER& barrier);
void Barrier(ID3D12GraphicsCommandList7* cmdList, const D3D12_BUFFER_BARRIER& barrier);
void Barrier(ID3D12GraphicsCommandList7* cmdList, const D3D12_TEXTURE_BARRIER& barrier);
void Barrier(ID3D12GraphicsCommandList7* cmdList, const BarrierBatch& batch);

// Resource management
uint64_t GetResourceSize(const D3D12_RESOURCE_DESC& desc, uint32_t firstSubResource = 0, uint32_t numSubResources = 1);
uint64_t GetResourceSize(ID3D12Resource* resource, uint32_t firstSubResource = 0, uint32_t numSubResources = 1);

// Heap helpers
const D3D12_HEAP_PROPERTIES* GetDefaultHeapProps();
const D3D12_HEAP_PROPERTIES* GetUploadHeapProps();
const D3D12_HEAP_PROPERTIES* GetReadbackHeapProps();
const D3D12_HEAP_PROPERTIES* GetGPUUploadHeapProps();

// Render states
D3D12_BLEND_DESC GetBlendState(BlendState blendState);
D3D12_RASTERIZER_DESC GetRasterizerState(RasterizerState rasterizerState);
D3D12_RASTERIZER_DESC2 GetRasterizerState2(RasterizerState rasterizerState);
D3D12_DEPTH_STENCIL_DESC GetDepthState(DepthState depthState);
D3D12_SAMPLER_DESC GetSamplerState(SamplerState samplerState);
D3D12_STATIC_SAMPLER_DESC GetStaticSamplerState(SamplerState samplerState, uint32_t shaderRegister = 0, uint32_t registerSpace = 0,
                                                D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_PIXEL);
D3D12_STATIC_SAMPLER_DESC ConvertToStaticSampler(const D3D12_SAMPLER_DESC& samplerDesc, uint32_t shaderRegister = 0, uint32_t registerSpace = 0,
                                                 D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_PIXEL);

// Convenience functions
void SetViewport(ID3D12GraphicsCommandList* cmdList, uint64_t width, uint64_t height, float zMin = 0.0f, float zMax = 1.0f);
void CreateRootSignature(ID3D12RootSignature** rootSignature, const D3D12_ROOT_SIGNATURE_DESC1& desc);
uint32_t DispatchSize(uint64_t numElements, uint64_t groupSize);

// Resource binding
void SetDescriptorHeaps(ID3D12GraphicsCommandList* cmdList);

// Helpers for buffer types that use temporary buffer memory from the upload helper
TempBuffer TempConstantBuffer(uint64_t cbSize, bool makeDescriptor = false);
void BindTempConstantBuffer(ID3D12GraphicsCommandList* cmdList, const void* cbData, uint64_t cbSize, uint32_t rootParameter, CmdListMode cmdListMode);

template<typename T> void BindTempConstantBuffer(ID3D12GraphicsCommandList* cmdList, const T& cbData, uint32_t rootParameter, CmdListMode cmdListMode)
{
    BindTempConstantBuffer(cmdList, &cbData, sizeof(T), rootParameter, cmdListMode);
}

template<typename T> void BindTempConstantBufferToURS(ID3D12GraphicsCommandList* cmdList, const T& cbData, uint32_t cbIdx, CmdListMode cmdListMode)
{
    Assert_(cbIdx < NumUniversalRootSignatureConstantBuffers);
    BindTempConstantBuffer(cmdList, &cbData, sizeof(T), URS_ConstantBuffers + cbIdx, cmdListMode);
}

template<uint32_t N> void BindTempConstantBuffer(ID3D12GraphicsCommandList* cmdList, const uint32_t (&cbData)[N], uint32_t rootParameter, CmdListMode cmdListMode)
{
    BindTempConstantBuffer(cmdList, cbData, N * sizeof(uint32_t), rootParameter, cmdListMode);
}

template<uint32_t N> void BindTempConstantBufferToURS(ID3D12GraphicsCommandList* cmdList, const uint32_t (&cbData)[N], uint32_t cbIdx, CmdListMode cmdListMode)
{
    Assert_(cbIdx < NumUniversalRootSignatureConstantBuffers);
    BindTempConstantBuffer(cmdList, cbData, N * sizeof(uint32_t), URS_ConstantBuffers + cbIdx, cmdListMode);
}

TempBuffer TempStructuredBuffer(uint64_t numElements, uint64_t stride, bool makeDescriptor = true);
TempBuffer TempFormattedBuffer(uint64_t numElements, DXGI_FORMAT format, bool makeDescriptor = true);
TempBuffer TempRawBuffer(uint64_t numElements, bool makeDescriptor = true);

// Decode a texture read it back on the CPU
void ConvertAndReadbackTexture(const Texture& texture, DXGI_FORMAT outputFormat, ReadbackBuffer& buffer);

// Clear a raw buffer to a value
void ClearRawBuffer(ID3D12GraphicsCommandList* cmdList, const RawBuffer& buffer, const Uint4& clearValue);
void ClearRawBuffer(ID3D12GraphicsCommandList* cmdList, const RawBuffer& buffer, const Float4& clearValue);

ID3D12PipelineState* CreateComputePSO(const CompiledShaderPtr& shader);

} // namespace DX12

} // namespace SampleFramework12
