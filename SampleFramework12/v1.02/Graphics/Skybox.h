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

#include "..\\InterfacePointers.h"
#include "..\\SF12_Math.h"
#include "ShaderCompilation.h"
#include "GraphicsTypes.h"
#include "SH.h"
#include "SG.h"

// HosekSky forward declares
struct ArHosekSkyModelState;

namespace SampleFramework12
{

#if EnableSkyModel_

// Cached data for the procedural sky model
struct SkyCache
{
    ArHosekSkyModelState* StateR = nullptr;
    ArHosekSkyModelState* StateG = nullptr;
    ArHosekSkyModelState* StateB = nullptr;
    Float3 SunDirection;
    Float3 SunRadiance;
    Float3 SunIrradiance;
    Float3 SunRenderColor;
    float SunSize = 0.0f;
    float Turbidity = 0.0f;
    Float3 Albedo;
    float Elevation = 0.0f;
    Texture CubeMap;
    SH9Color SH;
    SG9 SG;

    bool Init(const Float3& sunDirection, float sunSize, const Float3& groundAlbedo, float turbidity, bool createCubemap);
    void Shutdown();
    ~SkyCache();

    bool Initialized() const { return StateR != nullptr; }

    Float3 Sample(Float3 sampleDir) const;
};

#endif // EnableSkyModel_

class Skybox
{

public:

    Skybox();
    ~Skybox();

    void Initialize();
    void Shutdown();

    void CreatePSOs(DXGI_FORMAT rtFormat, DXGI_FORMAT depthFormat, uint32 numMSAASamples);
    void DestroyPSOs();

    void RenderEnvironmentMap(ID3D12GraphicsCommandList* cmdList,
                              const Float4x4& view,
                              const Float4x4& projection,
                              const Texture* environmentMap,
                              Float3 scale = Float3(1.0f, 1.0f, 1.0f));

#if EnableSkyModel_

    void RenderSky(ID3D12GraphicsCommandList* cmdList,
                   const Float4x4& view,
                   const Float4x4& projection,
                   const SkyCache& skyCache,
                   bool enableSun = true,
                   const Float3& scale = Float3(1.0f, 1.0f, 1.0f));

#endif // EnableSkyModel_

protected:

    void RenderCommon(ID3D12GraphicsCommandList* cmdList,
                      const Texture* environmentMap,
                      const Float4x4& view,
                      const Float4x4& projection,
                      Float3 scale);

    struct VSConstants
    {
        Float4x4 View;
        Float4x4 Projection;
    };

    struct PSConstants
    {
        Float3 SunDirection = Float3(0.0f, 1.0f, 0.0f);
        float CosSunAngularRadius = 0.0f;
        Float3 SunColor = 1.0f;
        uint32 Padding = 0;
        Float3 Scale = 1.0f;
        uint32 EnvMapIdx = uint32(-1);
    };

    CompiledShaderPtr vertexShader;
    CompiledShaderPtr pixelShader;
    StructuredBuffer vertexBuffer;
    FormattedBuffer indexBuffer;
    VSConstants vsConstants;
    PSConstants psConstants;
    ID3D12RootSignature* rootSignature;
    ID3D12PipelineState* pipelineState;
};

}