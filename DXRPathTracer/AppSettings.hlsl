struct AppSettings_Layout
{
    bool EnableSun;
    bool SunAreaLightApproximation;
    float SunSize;
    float3 SunDirection;
    int MSAAMode;
    bool RenderLights;
    bool EnableRayTracing;
    int SqrtNumSamples;
    int MaxPathLength;
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
    float RoughnessScale;
};

ConstantBuffer<AppSettings_Layout> AppSettings : register(b12);

static const int MSAAModes_MSAANone = 0;
static const int MSAAModes_MSAA2x = 1;
static const int MSAAModes_MSAA4x = 2;

static const int Scenes_Sponza = 0;
static const int Scenes_SunTemple = 1;
static const int Scenes_BoxTest = 2;

static const int ClusterRasterizationModes_Normal = 0;
static const int ClusterRasterizationModes_MSAA4x = 1;
static const int ClusterRasterizationModes_MSAA8x = 2;
static const int ClusterRasterizationModes_Conservative = 3;

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
