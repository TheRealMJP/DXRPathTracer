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
    float Exposure;
    float BloomExposure;
    float BloomMagnitude;
    float BloomBlurSigma;
    bool EnableAlbedoMaps;
    bool EnableNormalMaps;
    bool EnableDiffuse;
    bool EnableSpecular;
    bool EnableDirect;
    bool EnableIndirect;
    bool EnableIndirectSpecular;
    bool ApplyMultiscatteringEnergyCompensation;
    float RoughnessScale;
    float MetallicScale;
    bool EnableWhiteFurnaceMode;
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
    float Exposure;
    float BloomExposure;
    float BloomMagnitude;
    float BloomBlurSigma;
    bool EnableAlbedoMaps;
    bool EnableNormalMaps;
    bool EnableDiffuse;
    bool EnableSpecular;
    bool EnableDirect;
    bool EnableIndirect;
    bool EnableIndirectSpecular;
    bool ApplyMultiscatteringEnergyCompensation;
    float RoughnessScale;
    float MetallicScale;
    bool EnableWhiteFurnaceMode;
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
    AppSettingsCB.Exposure,
    AppSettingsCB.BloomExposure,
    AppSettingsCB.BloomMagnitude,
    AppSettingsCB.BloomBlurSigma,
    AppSettingsCB.EnableAlbedoMaps,
    AppSettingsCB.EnableNormalMaps,
    AppSettingsCB.EnableDiffuse,
    AppSettingsCB.EnableSpecular,
    AppSettingsCB.EnableDirect,
    AppSettingsCB.EnableIndirect,
    AppSettingsCB.EnableIndirectSpecular,
    AppSettingsCB.ApplyMultiscatteringEnergyCompensation,
    AppSettingsCB.RoughnessScale,
    AppSettingsCB.MetallicScale,
    AppSettingsCB.EnableWhiteFurnaceMode,
    DrawDebugPaths_,
};

enum Scenes
{
    Sponza = 0,
    SunTemple = 1,
    BoxTest = 2,
    WhiteFurnace = 3,
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
static const uint MaxPathLengthSetting = 8;
