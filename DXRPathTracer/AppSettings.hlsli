#pragma once

struct AppSettings_CBLayout
{
    bool EnableSun;
    bool EnableSky;
    bool SunAreaLightApproximation;
    float SunSize;
    float3 SunDirection;
    bool RenderLights;
    bool ClampRoughness;
    bool AvoidCausticPaths;
    int SqrtNumSamples;
    int MaxPathLength;
    int MaxAnyHitPathLength;
    int MaxVolumetricPathLength;
    float Exposure;
    float BloomExposure;
    float BloomMagnitude;
    float BloomBlurSigma;
    bool EnableBaseColorMaps;
    bool EnableNormalMaps;
    bool EnableDiffuse;
    bool EnableSpecular;
    bool EnableRefraction;
    bool EnableDirect;
    bool EnableIndirect;
    bool EnableIndirectSpecular;
    bool EnableDirectLightSampling;
    bool ApplyMultiscatteringEnergyCompensation;
    float RoughnessScale;
    float MetallicOffset;
};

ConstantBuffer<AppSettings_CBLayout> AppSettingsCB : register(b12);

struct AppSettings_Values
{
    bool EnableSun;
    bool EnableSky;
    bool SunAreaLightApproximation;
    float SunSize;
    float3 SunDirection;
    bool RenderLights;
    bool ClampRoughness;
    bool AvoidCausticPaths;
    int SqrtNumSamples;
    int MaxPathLength;
    int MaxAnyHitPathLength;
    int MaxVolumetricPathLength;
    float Exposure;
    float BloomExposure;
    float BloomMagnitude;
    float BloomBlurSigma;
    bool EnableBaseColorMaps;
    bool EnableNormalMaps;
    bool EnableDiffuse;
    bool EnableSpecular;
    bool EnableRefraction;
    bool EnableDirect;
    bool EnableIndirect;
    bool EnableIndirectSpecular;
    bool EnableDirectLightSampling;
    bool ApplyMultiscatteringEnergyCompensation;
    float RoughnessScale;
    float MetallicOffset;
    bool DrawDebugPaths;
};

static const AppSettings_Values AppSettings =
{
    AppSettingsCB.EnableSun,
    AppSettingsCB.EnableSky,
    AppSettingsCB.SunAreaLightApproximation,
    AppSettingsCB.SunSize,
    AppSettingsCB.SunDirection,
    AppSettingsCB.RenderLights,
    AppSettingsCB.ClampRoughness,
    AppSettingsCB.AvoidCausticPaths,
    AppSettingsCB.SqrtNumSamples,
    AppSettingsCB.MaxPathLength,
    AppSettingsCB.MaxAnyHitPathLength,
    AppSettingsCB.MaxVolumetricPathLength,
    AppSettingsCB.Exposure,
    AppSettingsCB.BloomExposure,
    AppSettingsCB.BloomMagnitude,
    AppSettingsCB.BloomBlurSigma,
    AppSettingsCB.EnableBaseColorMaps,
    AppSettingsCB.EnableNormalMaps,
    AppSettingsCB.EnableDiffuse,
    AppSettingsCB.EnableSpecular,
    AppSettingsCB.EnableRefraction,
    AppSettingsCB.EnableDirect,
    AppSettingsCB.EnableIndirect,
    AppSettingsCB.EnableIndirectSpecular,
    AppSettingsCB.EnableDirectLightSampling,
    AppSettingsCB.ApplyMultiscatteringEnergyCompensation,
    AppSettingsCB.RoughnessScale,
    AppSettingsCB.MetallicOffset,
    DrawDebugPaths_,
};

enum Scenes
{
    BoxTest = 0,
    Knob = 1,
    Dragon = 2,
    Cube = 3,
    Sponza = 4,
    SunTemple = 5,
    WhiteFurnace = 6,
};

static const uint ClusterTileSize = 16;
static const uint NumZTiles = 16;
static const uint MaxSpotLights = 32;
static const uint SpotLightElementsPerCluster = 1;
static const float SpotLightRange = 7.5000f;
static const float SpotShadowNearClip = 0.1000f;
static const uint NumSampleSets = 8;
static const uint SampleTileSize = 32;
static const uint NumPixelsPerTile = 1024;
