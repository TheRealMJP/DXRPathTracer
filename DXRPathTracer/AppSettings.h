#pragma once

#include <PCH.h>
#include <Settings.h>
#include <Graphics\GraphicsTypes.h>

using namespace SampleFramework12;

enum class MSAAModes
{
    MSAANone = 0,
    MSAA2x = 1,
    MSAA4x = 2,

    NumValues
};

typedef EnumSettingT<MSAAModes> MSAAModesSetting;

enum class Scenes
{
    Sponza = 0,
    SunTemple = 1,
    BoxTest = 2,

    NumValues
};

typedef EnumSettingT<Scenes> ScenesSetting;

enum class ClusterRasterizationModes
{
    Normal = 0,
    MSAA4x = 1,
    MSAA8x = 2,
    Conservative = 3,

    NumValues
};

typedef EnumSettingT<ClusterRasterizationModes> ClusterRasterizationModesSetting;

namespace AppSettings
{
    static const uint64 ClusterTileSize = 16;
    static const uint64 NumZTiles = 16;
    static const uint64 MaxSpotLights = 32;
    static const uint64 SpotLightElementsPerCluster = 1;
    static const float SpotLightRange = 7.5000f;
    static const float SpotShadowNearClip = 0.1000f;
    static const uint64 NumSampleSets = 8;
    static const uint64 SampleTileSize = 32;
    static const uint64 NumPixelsPerTile = 1024;
    static const uint64 MaxPathLengthSetting = 8;

    extern BoolSetting EnableSun;
    extern BoolSetting SunAreaLightApproximation;
    extern FloatSetting SunSize;
    extern DirectionSetting SunDirection;
    extern FloatSetting Turbidity;
    extern ColorSetting GroundAlbedo;
    extern MSAAModesSetting MSAAMode;
    extern ScenesSetting CurrentScene;
    extern BoolSetting RenderLights;
    extern IntSetting MaxLightClamp;
    extern ClusterRasterizationModesSetting ClusterRasterizationMode;
    extern BoolSetting EnableRayTracing;
    extern IntSetting SqrtNumSamples;
    extern IntSetting MaxPathLength;
    extern FloatSetting Exposure;
    extern FloatSetting BloomExposure;
    extern FloatSetting BloomMagnitude;
    extern FloatSetting BloomBlurSigma;
    extern BoolSetting EnableVSync;
    extern BoolSetting EnableAlbedoMaps;
    extern BoolSetting EnableNormalMaps;
    extern BoolSetting EnableDiffuse;
    extern BoolSetting EnableSpecular;
    extern BoolSetting EnableDirect;
    extern BoolSetting EnableIndirect;
    extern BoolSetting EnableIndirectSpecular;
    extern FloatSetting RoughnessScale;
    extern BoolSetting AlwaysResetPathTrace;
    extern BoolSetting ShowProgressBar;

    struct AppSettingsCBuffer
    {
        bool32 EnableSun;
        bool32 SunAreaLightApproximation;
        float SunSize;
        Float4Align Float3 SunDirection;
        int32 MSAAMode;
        bool32 RenderLights;
        bool32 EnableRayTracing;
        int32 SqrtNumSamples;
        int32 MaxPathLength;
        float Exposure;
        float BloomExposure;
        float BloomMagnitude;
        float BloomBlurSigma;
        bool32 EnableAlbedoMaps;
        bool32 EnableNormalMaps;
        bool32 EnableDiffuse;
        bool32 EnableSpecular;
        bool32 EnableDirect;
        bool32 EnableIndirect;
        bool32 EnableIndirectSpecular;
        float RoughnessScale;
    };

    extern ConstantBuffer CBuffer;
    const extern uint32 CBufferRegister;

    void Initialize();
    void Shutdown();
    void Update(uint32 displayWidth, uint32 displayHeight, const Float4x4& viewMatrix);
    void UpdateCBuffer();
    void BindCBufferGfx(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter);
    void BindCBufferCompute(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter);
};

// ================================================================================================

const uint64 NumMSAAModes = uint64(MSAAModes::NumValues);

namespace AppSettings
{
    extern uint64 NumXTiles;
    extern uint64 NumYTiles;

    inline uint32 NumMSAASamples(MSAAModes mode)
    {
        static const uint32 NumSamples[] = { 1, 2, 4 };
        StaticAssert_(ArraySize_(NumSamples) >= uint64(MSAAModes::NumValues));
        return NumSamples[uint32(mode)];
    }

    inline uint32 NumMSAASamples()
    {
        return NumMSAASamples(MSAAMode);
    }

    void UpdateUI();
}