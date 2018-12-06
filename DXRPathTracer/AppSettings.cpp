#include <PCH.h>
#include "AppSettings.h"

using namespace SampleFramework12;

static const char* MSAAModesLabels[] =
{
    "None",
    "2x",
    "4x",
};

static const char* ScenesLabels[] =
{
    "Sponza",
    "SunTemple",
    "BoxTest",
};

static const char* ClusterRasterizationModesLabels[] =
{
    "Normal",
    "MSAA4x",
    "MSAA8x",
    "Conservative",
};

namespace AppSettings
{
    static SettingsContainer Settings;

    BoolSetting EnableSun;
    BoolSetting SunAreaLightApproximation;
    FloatSetting SunSize;
    DirectionSetting SunDirection;
    FloatSetting Turbidity;
    ColorSetting GroundAlbedo;
    MSAAModesSetting MSAAMode;
    ScenesSetting CurrentScene;
    BoolSetting RenderLights;
    IntSetting MaxLightClamp;
    ClusterRasterizationModesSetting ClusterRasterizationMode;
    BoolSetting EnableRayTracing;
    IntSetting SqrtNumSamples;
    IntSetting MaxPathLength;
    FloatSetting Exposure;
    FloatSetting BloomExposure;
    FloatSetting BloomMagnitude;
    FloatSetting BloomBlurSigma;
    BoolSetting EnableVSync;
    BoolSetting EnableAlbedoMaps;
    BoolSetting EnableNormalMaps;
    BoolSetting EnableDiffuse;
    BoolSetting EnableSpecular;
    BoolSetting EnableDirect;
    BoolSetting EnableIndirect;
    BoolSetting EnableIndirectSpecular;
    FloatSetting RoughnessScale;
    BoolSetting AlwaysResetPathTrace;
    BoolSetting ShowProgressBar;

    ConstantBuffer CBuffer;
    const uint32 CBufferRegister = 12;

    void Initialize()
    {

        Settings.Initialize(7);

        Settings.AddGroup("Sun And Sky", true);

        Settings.AddGroup("Anti Aliasing", false);

        Settings.AddGroup("Scene", true);

        Settings.AddGroup("Rendering", false);

        Settings.AddGroup("Path Tracing", true);

        Settings.AddGroup("Post Processing", false);

        Settings.AddGroup("Debug", true);

        EnableSun.Initialize("EnableSun", "Sun And Sky", "Enable Sun", "Enables the sun light", true);
        Settings.AddSetting(&EnableSun);

        SunAreaLightApproximation.Initialize("SunAreaLightApproximation", "Sun And Sky", "Sun Area Light Approximation", "Controls whether the sun is treated as a disc area light in the real-time shader", true);
        Settings.AddSetting(&SunAreaLightApproximation);

        SunSize.Initialize("SunSize", "Sun And Sky", "Sun Size", "Angular radius of the sun in degrees", 1.0000f, 0.0100f, 340282300000000000000000000000000000000.0000f, 0.0100f, ConversionMode::None, 1.0000f);
        Settings.AddSetting(&SunSize);

        SunDirection.Initialize("SunDirection", "Sun And Sky", "Sun Direction", "Direction of the sun", Float3(0.2600f, 0.9870f, -0.1600f), true);
        Settings.AddSetting(&SunDirection);

        Turbidity.Initialize("Turbidity", "Sun And Sky", "Turbidity", "Atmospheric turbidity (thickness) uses for procedural sun and sky model", 2.0000f, 1.0000f, 10.0000f, 0.0100f, ConversionMode::None, 1.0000f);
        Settings.AddSetting(&Turbidity);

        GroundAlbedo.Initialize("GroundAlbedo", "Sun And Sky", "Ground Albedo", "Ground albedo color used for procedural sun and sky model", Float3(0.2500f, 0.2500f, 0.2500f), false, -340282300000000000000000000000000000000.0000f, 340282300000000000000000000000000000000.0000f, 0.0100f, ColorUnit::None);
        Settings.AddSetting(&GroundAlbedo);

        MSAAMode.Initialize("MSAAMode", "Anti Aliasing", "MSAA Mode", "MSAA mode to use for rendering", MSAAModes::MSAA4x, 3, MSAAModesLabels);
        Settings.AddSetting(&MSAAMode);

        CurrentScene.Initialize("CurrentScene", "Scene", "Current Scene", "", Scenes::BoxTest, 3, ScenesLabels);
        Settings.AddSetting(&CurrentScene);

        RenderLights.Initialize("RenderLights", "Scene", "Render Lights", "Enable or disable deferred light rendering", false);
        Settings.AddSetting(&RenderLights);

        MaxLightClamp.Initialize("MaxLightClamp", "Rendering", "Max Lights", "Limits the number of lights in the scene", 32, 0, 32);
        Settings.AddSetting(&MaxLightClamp);

        ClusterRasterizationMode.Initialize("ClusterRasterizationMode", "Rendering", "Cluster Rasterization Mode", "Conservative rasterization mode to use for light binning", ClusterRasterizationModes::Conservative, 4, ClusterRasterizationModesLabels);
        Settings.AddSetting(&ClusterRasterizationMode);

        EnableRayTracing.Initialize("EnableRayTracing", "Path Tracing", "Enable Ray Tracing", "", true);
        Settings.AddSetting(&EnableRayTracing);

        SqrtNumSamples.Initialize("SqrtNumSamples", "Path Tracing", "Sqrt Num Samples", "The square root of the number of per-pixel sample rays to use for path tracing", 4, 1, 100);
        Settings.AddSetting(&SqrtNumSamples);

        MaxPathLength.Initialize("MaxPathLength", "Path Tracing", "Max Path Length", "Maximum path length (bounces) to use for path tracing", 3, 2, 8);
        Settings.AddSetting(&MaxPathLength);

        Exposure.Initialize("Exposure", "Post Processing", "Exposure", "Simple exposure value applied to the scene before tone mapping (uses log2 scale)", -14.0000f, -24.0000f, 24.0000f, 0.1000f, ConversionMode::None, 1.0000f);
        Settings.AddSetting(&Exposure);

        BloomExposure.Initialize("BloomExposure", "Post Processing", "Bloom Exposure Offset", "Exposure offset applied to generate the input of the bloom pass", -4.0000f, -10.0000f, 0.0000f, 0.0100f, ConversionMode::None, 1.0000f);
        Settings.AddSetting(&BloomExposure);

        BloomMagnitude.Initialize("BloomMagnitude", "Post Processing", "Bloom Magnitude", "Scale factor applied to the bloom results when combined with tone-mapped result", 1.0000f, 0.0000f, 2.0000f, 0.0100f, ConversionMode::None, 1.0000f);
        Settings.AddSetting(&BloomMagnitude);

        BloomBlurSigma.Initialize("BloomBlurSigma", "Post Processing", "Bloom Blur Sigma", "Sigma parameter of the Gaussian filter used in the bloom pass", 2.5000f, 0.5000f, 2.5000f, 0.0100f, ConversionMode::None, 1.0000f);
        Settings.AddSetting(&BloomBlurSigma);

        EnableVSync.Initialize("EnableVSync", "Debug", "Enable VSync", "Enables or disables vertical sync during Present", true);
        Settings.AddSetting(&EnableVSync);

        EnableAlbedoMaps.Initialize("EnableAlbedoMaps", "Debug", "Enable Albedo Maps", "Enables albedo maps", true);
        Settings.AddSetting(&EnableAlbedoMaps);

        EnableNormalMaps.Initialize("EnableNormalMaps", "Debug", "Enable Normal Maps", "Enables normal maps", true);
        Settings.AddSetting(&EnableNormalMaps);

        EnableDiffuse.Initialize("EnableDiffuse", "Debug", "Enable Diffuse", "Enables diffuse reflections", true);
        Settings.AddSetting(&EnableDiffuse);

        EnableSpecular.Initialize("EnableSpecular", "Debug", "Enable Specular", "Enables specular reflections", true);
        Settings.AddSetting(&EnableSpecular);

        EnableDirect.Initialize("EnableDirect", "Debug", "Enable Direct", "Enables direct lighting", true);
        Settings.AddSetting(&EnableDirect);

        EnableIndirect.Initialize("EnableIndirect", "Debug", "Enable Indirect", "Enables indirect lighting", true);
        Settings.AddSetting(&EnableIndirect);

        EnableIndirectSpecular.Initialize("EnableIndirectSpecular", "Debug", "Enable Indirect Specular", "Enables indirect specular reflections, it produces noisier output", false);
        Settings.AddSetting(&EnableIndirectSpecular);

        RoughnessScale.Initialize("RoughnessScale", "Debug", "Roughness Scale", "Scales the scene roughness by this value", 1.0000f, 0.0010f, 2.0000f, 0.0100f, ConversionMode::None, 1.0000f);
        Settings.AddSetting(&RoughnessScale);

        AlwaysResetPathTrace.Initialize("AlwaysResetPathTrace", "Debug", "Always Reset Path Trace", "", false);
        Settings.AddSetting(&AlwaysResetPathTrace);

        ShowProgressBar.Initialize("ShowProgressBar", "Debug", "Show Progress Bar", "", true);
        Settings.AddSetting(&ShowProgressBar);

        ConstantBufferInit cbInit;
        cbInit.Size = sizeof(AppSettingsCBuffer);
        cbInit.Dynamic = true;
        cbInit.Name = L"AppSettings Constant Buffer";
        CBuffer.Initialize(cbInit);
    }

    void Update(uint32 displayWidth, uint32 displayHeight, const Float4x4& viewMatrix)
    {
        Settings.Update(displayWidth, displayHeight, viewMatrix);

    }

    void UpdateCBuffer()
    {
        AppSettingsCBuffer cbData;
        cbData.EnableSun = EnableSun;
        cbData.SunAreaLightApproximation = SunAreaLightApproximation;
        cbData.SunSize = SunSize;
        cbData.SunDirection = SunDirection;
        cbData.MSAAMode = MSAAMode;
        cbData.RenderLights = RenderLights;
        cbData.EnableRayTracing = EnableRayTracing;
        cbData.SqrtNumSamples = SqrtNumSamples;
        cbData.MaxPathLength = MaxPathLength;
        cbData.Exposure = Exposure;
        cbData.BloomExposure = BloomExposure;
        cbData.BloomMagnitude = BloomMagnitude;
        cbData.BloomBlurSigma = BloomBlurSigma;
        cbData.EnableAlbedoMaps = EnableAlbedoMaps;
        cbData.EnableNormalMaps = EnableNormalMaps;
        cbData.EnableDiffuse = EnableDiffuse;
        cbData.EnableSpecular = EnableSpecular;
        cbData.EnableDirect = EnableDirect;
        cbData.EnableIndirect = EnableIndirect;
        cbData.EnableIndirectSpecular = EnableIndirectSpecular;
        cbData.RoughnessScale = RoughnessScale;

        CBuffer.MapAndSetData(cbData);
    }
    void BindCBufferGfx(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter)
    {
        CBuffer.SetAsGfxRootParameter(cmdList, rootParameter);
    }
    void BindCBufferCompute(ID3D12GraphicsCommandList* cmdList, uint32 rootParameter)
    {
        CBuffer.SetAsComputeRootParameter(cmdList, rootParameter);
    }
    void Shutdown()
    {
        CBuffer.Shutdown();
    }
}

// ================================================================================================

namespace AppSettings
{
    uint64 NumXTiles = 0;
    uint64 NumYTiles = 0;

    void UpdateUI()
    {
    }
}