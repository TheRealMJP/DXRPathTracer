#pragma once

#include <PCH.h>
#include <Settings.h>
#include <Graphics\GraphicsTypes.h>

using namespace SampleFramework12;

enum class Scenes
{
    BoxTest = 0,
    Knob = 1,
    Dragon = 2,
    Sponza = 3,
    SunTemple = 4,
    WhiteFurnace = 5,

    NumValues
};

extern const char* ScenesLabels[uint32_t(Scenes::NumValues)];

extern const Scenes ScenesValues[uint32_t(Scenes::NumValues)];

typedef EnumSettingT<Scenes> ScenesSetting;

namespace AppSettings
{
    static const uint64_t ClusterTileSize = 16;
    static const uint64_t NumZTiles = 16;
    static const uint64_t MaxSpotLights = 32;
    static const uint64_t SpotLightElementsPerCluster = 1;
    static const float SpotLightRange = 7.5000f;
    static const float SpotShadowNearClip = 0.1000f;
    static const uint64_t NumSampleSets = 8;
    static const uint64_t SampleTileSize = 32;
    static const uint64_t NumPixelsPerTile = 1024;

    extern BoolSetting EnableSun;
    extern BoolSetting EnableSky;
    extern BoolSetting SunAreaLightApproximation;
    extern FloatSetting SunSize;
    extern DirectionSetting SunDirection;
    extern FloatSetting Turbidity;
    extern ColorSetting GroundAlbedo;
    extern ScenesSetting CurrentScene;
    extern BoolSetting RenderLights;
    extern IntSetting MaxLightClamp;
    extern BoolSetting ClampRoughness;
    extern BoolSetting AvoidCausticPaths;
    extern IntSetting SqrtNumSamples;
    extern IntSetting MaxPathLength;
    extern IntSetting MaxAnyHitPathLength;
    extern IntSetting MaxVolumetricPathLength;
    extern FloatSetting Exposure;
    extern FloatSetting BloomExposure;
    extern FloatSetting BloomMagnitude;
    extern FloatSetting BloomBlurSigma;
    extern BoolSetting EnableVSync;
    extern BoolSetting StablePowerState;
    extern BoolSetting EnableBaseColorMaps;
    extern BoolSetting EnableNormalMaps;
    extern BoolSetting EnableDiffuse;
    extern BoolSetting EnableSpecular;
    extern BoolSetting EnableDirect;
    extern BoolSetting EnableIndirect;
    extern BoolSetting EnableIndirectSpecular;
    extern BoolSetting ApplyMultiscatteringEnergyCompensation;
    extern FloatSetting RoughnessScale;
    extern FloatSetting MetallicOffset;
    extern BoolSetting AlwaysResetPathTrace;
    extern BoolSetting ShowProgressBar;
    extern BoolSetting DrawDebugPaths;

    struct AppSettingsCBuffer
    {
        bool32 EnableSun;
        bool32 EnableSky;
        bool32 SunAreaLightApproximation;
        float SunSize;
        Float3 SunDirection;
        bool32 RenderLights;
        bool32 ClampRoughness;
        bool32 AvoidCausticPaths;
        int32_t SqrtNumSamples;
        int32_t MaxPathLength;
        int32_t MaxAnyHitPathLength;
        int32_t MaxVolumetricPathLength;
        float Exposure;
        float BloomExposure;
        float BloomMagnitude;
        float BloomBlurSigma;
        bool32 EnableBaseColorMaps;
        bool32 EnableNormalMaps;
        bool32 EnableDiffuse;
        bool32 EnableSpecular;
        bool32 EnableDirect;
        bool32 EnableIndirect;
        bool32 EnableIndirectSpecular;
        bool32 ApplyMultiscatteringEnergyCompensation;
        float RoughnessScale;
        float MetallicOffset;
    };

    extern ConstantBuffer CBuffer;
    const extern uint32_t CBufferRegister;

    void Initialize();
    void Shutdown();
    void Update(uint32_t displayWidth, uint32_t displayHeight, const Float4x4& viewMatrix);
    void UpdateCBuffer();
    void BindCBufferGfx(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter);
    void BindCBufferCompute(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter);
    void GetShaderCompileOptions(CompileOptions& opts);
    bool ShaderCompileOptionsChanged();
};

// ================================================================================================