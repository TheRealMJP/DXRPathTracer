#include <PCH.h>
#include <Graphics\ShaderCompilation.h>
#include "AppSettings.h"

using namespace SampleFramework12;

const char* ScenesLabels[uint32_t(Scenes::NumValues)] =
{
    "BoxTest",
    "Knob (Editable)",
    "Dragon (Editable)",
    "Sponza",
    "SunTemple",
    "WhiteFurnace",
};

const Scenes ScenesValues[uint32_t(Scenes::NumValues)] =
{
    Scenes::BoxTest,
    Scenes::Knob,
    Scenes::Dragon,
    Scenes::Sponza,
    Scenes::SunTemple,
    Scenes::WhiteFurnace,
};

namespace AppSettings
{
    static SettingsContainer Settings;

    BoolSetting EnableSun;
    BoolSetting EnableSky;
    BoolSetting SunAreaLightApproximation;
    FloatSetting SunSize;
    DirectionSetting SunDirection;
    FloatSetting Turbidity;
    ColorSetting GroundAlbedo;
    ScenesSetting CurrentScene;
    BoolSetting RenderLights;
    IntSetting MaxLightClamp;
    BoolSetting ClampRoughness;
    BoolSetting AvoidCausticPaths;
    IntSetting SqrtNumSamples;
    IntSetting MaxPathLength;
    IntSetting MaxAnyHitPathLength;
    IntSetting MaxVolumetricPathLength;
    FloatSetting Exposure;
    FloatSetting BloomExposure;
    FloatSetting BloomMagnitude;
    FloatSetting BloomBlurSigma;
    BoolSetting EnableVSync;
    BoolSetting StablePowerState;
    BoolSetting EnableBaseColorMaps;
    BoolSetting EnableNormalMaps;
    BoolSetting EnableDiffuse;
    BoolSetting EnableSpecular;
    BoolSetting EnableDirect;
    BoolSetting EnableIndirect;
    BoolSetting EnableIndirectSpecular;
    BoolSetting EnableDirectLightSampling;
    BoolSetting ApplyMultiscatteringEnergyCompensation;
    FloatSetting RoughnessScale;
    FloatSetting MetallicOffset;
    BoolSetting AlwaysResetPathTrace;
    BoolSetting ShowProgressBar;
    BoolSetting DrawDebugPaths;

    ConstantBuffer CBuffer;
    const uint32_t CBufferRegister = 12;

    void Initialize()
    {

        Settings.Initialize(6);

        Settings.AddGroup("Sun And Sky", true);

        Settings.AddGroup("Scene", true);

        Settings.AddGroup("Rendering", false);

        Settings.AddGroup("Path Tracing", true);

        Settings.AddGroup("Post Processing", false);

        Settings.AddGroup("Debug", true);

        EnableSun.Initialize("EnableSun", "Sun And Sky", "Enable Sun", "Enables the sun light", true);
        Settings.AddSetting(&EnableSun);

        EnableSky.Initialize("EnableSky", "Sun And Sky", "Enable Sky", "Enables the sky environment", true);
        Settings.AddSetting(&EnableSky);

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

        CurrentScene.Initialize("CurrentScene", "Scene", "Current Scene", "", Scenes::Dragon, 6, ScenesLabels);
        Settings.AddSetting(&CurrentScene);

        RenderLights.Initialize("RenderLights", "Scene", "Render Lights", "Enable or disable spot light rendering", true);
        Settings.AddSetting(&RenderLights);

        MaxLightClamp.Initialize("MaxLightClamp", "Rendering", "Max Lights", "Limits the number of lights in the scene", 32, 0, 32);
        Settings.AddSetting(&MaxLightClamp);

        ClampRoughness.Initialize("ClampRoughness", "Path Tracing", "Clamp Roughness", "Clamp roughness for caustic paths from glossy bounces. Based on 'Physically Based Shader Design in Arnold' [Langlands14]", false);
        Settings.AddSetting(&ClampRoughness);

        AvoidCausticPaths.Initialize("AvoidCausticPaths", "Path Tracing", "Avoid Caustic Paths", "Avoid specular evaluation followed by diffuse path. Based on 'Physically Based Shader Design in Arnold' [Langlands14]", false);
        Settings.AddSetting(&AvoidCausticPaths);

        SqrtNumSamples.Initialize("SqrtNumSamples", "Path Tracing", "Sqrt Num Samples", "The square root of the number of per-pixel sample rays to use for path tracing", 4, 1, 100);
        Settings.AddSetting(&SqrtNumSamples);

        MaxPathLength.Initialize("MaxPathLength", "Path Tracing", "Max Path Length", "Maximum path length (bounces) to use for path tracing", 16, 2, 256);
        Settings.AddSetting(&MaxPathLength);

        MaxAnyHitPathLength.Initialize("MaxAnyHitPathLength", "Path Tracing", "Max Any-Hit Path Length", "The maximum path length where any-hit shaders will be used for alpha testing. Increasing this with improve the render quality, but will also increase frame times", 1, 0, 256);
        Settings.AddSetting(&MaxAnyHitPathLength);

        MaxVolumetricPathLength.Initialize("MaxVolumetricPathLength", "Path Tracing", "Max Volumetric Path Length", "", 16, 1, 256);
        Settings.AddSetting(&MaxVolumetricPathLength);

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

        StablePowerState.Initialize("StablePowerState", "Debug", "Stable Power State", "Enables the stable power state, which stabilizes GPU clocks for more consistent performance", false);
        Settings.AddSetting(&StablePowerState);

        EnableBaseColorMaps.Initialize("EnableBaseColorMaps", "Debug", "Enable Base Color Maps", "Enables base color maps", true);
        Settings.AddSetting(&EnableBaseColorMaps);

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

        EnableIndirectSpecular.Initialize("EnableIndirectSpecular", "Debug", "Enable Indirect Specular", "Enables indirect specular reflections, it produces noisier output", true);
        Settings.AddSetting(&EnableIndirectSpecular);

        EnableDirectLightSampling.Initialize("EnableDirectLightSampling", "Debug", "Enable Direct Light Sampling", "", true);
        Settings.AddSetting(&EnableDirectLightSampling);

        ApplyMultiscatteringEnergyCompensation.Initialize("ApplyMultiscatteringEnergyCompensation", "Debug", "Apply Multiscattering Energy Compensation", "Apply energy compensation to recover energy missing due to multiscattering. Based on 'Practical multiple scattering compensation for microfacet models' [Turquin19]", true);
        Settings.AddSetting(&ApplyMultiscatteringEnergyCompensation);

        RoughnessScale.Initialize("RoughnessScale", "Debug", "Roughness Scale", "Scales the scene roughness by this value", 1.0000f, 0.0010f, 2.0000f, 0.0100f, ConversionMode::None, 1.0000f);
        Settings.AddSetting(&RoughnessScale);

        MetallicOffset.Initialize("MetallicOffset", "Debug", "Metallic Offset", "Offsets the scene metallic by this value", 0.0000f, -1.0000f, 1.0000f, 0.0100f, ConversionMode::None, 1.0000f);
        Settings.AddSetting(&MetallicOffset);

        AlwaysResetPathTrace.Initialize("AlwaysResetPathTrace", "Debug", "Always Reset Path Trace", "", false);
        Settings.AddSetting(&AlwaysResetPathTrace);

        ShowProgressBar.Initialize("ShowProgressBar", "Debug", "Show Progress Bar", "", true);
        Settings.AddSetting(&ShowProgressBar);

        DrawDebugPaths.Initialize("DrawDebugPaths", "Debug", "Draw Debug Paths", "", false);
        Settings.AddSetting(&DrawDebugPaths);

        ConstantBufferInit cbInit;
        cbInit.Size = sizeof(AppSettingsCBuffer);
        cbInit.Dynamic = true;
        cbInit.Name = "AppSettings Constant Buffer";
        CBuffer.Initialize(cbInit);
    }

    void Update(uint32_t displayWidth, uint32_t displayHeight, const Float4x4& viewMatrix)
    {
        Settings.Update(displayWidth, displayHeight, viewMatrix);

    }

    void UpdateCBuffer()
    {
        AppSettingsCBuffer cbData;
        cbData.EnableSun = EnableSun;
        cbData.EnableSky = EnableSky;
        cbData.SunAreaLightApproximation = SunAreaLightApproximation;
        cbData.SunSize = SunSize;
        cbData.SunDirection = SunDirection;
        cbData.RenderLights = RenderLights;
        cbData.ClampRoughness = ClampRoughness;
        cbData.AvoidCausticPaths = AvoidCausticPaths;
        cbData.SqrtNumSamples = SqrtNumSamples;
        cbData.MaxPathLength = MaxPathLength;
        cbData.MaxAnyHitPathLength = MaxAnyHitPathLength;
        cbData.MaxVolumetricPathLength = MaxVolumetricPathLength;
        cbData.Exposure = Exposure;
        cbData.BloomExposure = BloomExposure;
        cbData.BloomMagnitude = BloomMagnitude;
        cbData.BloomBlurSigma = BloomBlurSigma;
        cbData.EnableBaseColorMaps = EnableBaseColorMaps;
        cbData.EnableNormalMaps = EnableNormalMaps;
        cbData.EnableDiffuse = EnableDiffuse;
        cbData.EnableSpecular = EnableSpecular;
        cbData.EnableDirect = EnableDirect;
        cbData.EnableIndirect = EnableIndirect;
        cbData.EnableIndirectSpecular = EnableIndirectSpecular;
        cbData.EnableDirectLightSampling = EnableDirectLightSampling;
        cbData.ApplyMultiscatteringEnergyCompensation = ApplyMultiscatteringEnergyCompensation;
        cbData.RoughnessScale = RoughnessScale;
        cbData.MetallicOffset = MetallicOffset;

        CBuffer.MapAndSetData(cbData);
    }

    void BindCBufferGfx(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter)
    {
        CBuffer.SetAsGfxRootParameter(cmdList, rootParameter);
    }

    void BindCBufferCompute(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter)
    {
        CBuffer.SetAsComputeRootParameter(cmdList, rootParameter);
    }

    void GetShaderCompileOptions(CompileOptions& opts)
    {
        opts.Add("DrawDebugPaths_", DrawDebugPaths.Value());
    }

    bool ShaderCompileOptionsChanged()
    {
        bool changed = false;
        changed = changed || DrawDebugPaths.Changed();
        return changed;
    }

    void Shutdown()
    {
        CBuffer.Shutdown();
    }
}

// ================================================================================================