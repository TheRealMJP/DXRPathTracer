//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
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

class SpriteFont;

enum class SpriteFilterMode : uint64
{
    Linear = 0,
    Point
};

enum class SpriteBlendMode : uint64
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

    SpriteTransform()
    {
    }

    explicit SpriteTransform(Float2 position) : Position(position)
    {
    }

    explicit SpriteTransform(Float2 position, float rotation) : Position(position)
    {
        SinCosRotation = Float2(std::sin(rotation), std::cos(rotation));
    }
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

    static const uint64 MaxBatchSize = 1024;

    SpriteRenderer();
    ~SpriteRenderer();

    void Initialize();
    void Shutdown();

    void CreatePSOs(DXGI_FORMAT rtFormat);
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
                     uint64 numSprites);

    void RenderText(ID3D12GraphicsCommandList* cmdList,
                    const SpriteFont& font,
                    const wchar* text,
                    Float2 position,
                    const Float4& color = Float4(1, 1, 1, 1));

    void End();

protected:

    ShaderPtr vertexShader;
    ShaderPtr vertexShaderInstanced;
    ShaderPtr pixelShader;
    ShaderPtr pixelShaderOpaque;
    FormattedBuffer indexBuffer;
    StructuredBuffer instanceDataBuffer;
    SpriteDrawData textDrawData[MaxBatchSize];
    ID3D12PipelineState* pipelineStates[uint64(SpriteBlendMode::NumValues)] = { };
    ID3D12RootSignature* rootSignature = nullptr;
    Texture defaultTexture;

    struct PerBatchConstants
    {
        Float2 TextureSize;
        Float2 ViewportSize;
        bool32 LinearSampling = false;
    } perBatchData;
};

}