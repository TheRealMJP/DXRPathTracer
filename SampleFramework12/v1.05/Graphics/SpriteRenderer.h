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

#include "..\\Exceptions.h"
#include "..\\Utility.h"
#include "..\\InterfacePointers.h"
#include "..\\SF12_Math.h"
#include "ShaderCompilation.h"
#include "GraphicsTypes.h"

namespace SampleFramework12
{

enum class SpriteFilterMode : uint64_t
{
    Linear = 0,
    Point
};

enum class SpriteBlendMode : uint64_t
{
    AlphaBlend = 0,
    Opaque,

    NumValues
};

struct SpriteTransform
{
    Float2 Position;
    Float2 Scale = Float2(1.0f, 1.0f);
    Float2 SinCosRotation = Float2(0.0f, 1.0f);
};

struct SpriteDrawData
{
    SpriteTransform Transform;
    Float4 Color;
    Float4 DrawRect;
};

class SpriteRenderer
{

public:

    static const uint64_t MaxBatchSize = 1024;

    SpriteRenderer();
    ~SpriteRenderer();

    void Initialize();
    void Shutdown();

    void CreatePSOs(DXGI_FORMAT rtFormat, uint32_t numMSAASamples);
    void DestroyPSOs();

    void Begin(ID3D12GraphicsCommandList* cmdList, Float2 viewportSize, SpriteFilterMode filterMode = SpriteFilterMode::Linear,
               SpriteBlendMode = SpriteBlendMode::AlphaBlend);

    void Render(ID3D12GraphicsCommandList* cmdList,
                const Texture* texture,
                const SpriteTransform& transform,
                const Float4& color = Float4(1, 1, 1, 1),
                const Float4* drawRect = NULL);

    void RenderBatch(ID3D12GraphicsCommandList* cmdList,
                     const Texture* texture,
                     const SpriteDrawData* drawData,
                     uint64_t numSprites);

    void End();

protected:

    ShaderPtr vertexShader;
    ShaderPtr vertexShaderInstanced;
    ShaderPtr pixelShader;
    ShaderPtr pixelShaderOpaque;
    FormattedBuffer indexBuffer;
    StructuredBuffer instanceDataBuffer;
    SpriteDrawData textDrawData[MaxBatchSize];
    ID3D12PipelineState* pipelineStates[uint64_t(SpriteBlendMode::NumValues)] = { };
    Texture defaultTexture;

    struct PerBatchConstants
    {
        Float2 TextureSize;
        Float2 ViewportSize;
        bool32 LinearSampling = false;
    } perBatchData;
};

}