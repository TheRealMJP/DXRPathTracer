enum Scenes
{
    Sponza = 0,
    SunTemple,
    BoxTest,
}

enum MSAAModes
{
    [EnumLabel("None")]
    MSAANone = 0,

    [EnumLabel("2x")]
    MSAA2x,

    [EnumLabel("4x")]
    MSAA4x,
}

enum ClusterRasterizationModes
{
    Normal,
    MSAA4x,
    MSAA8x,
    Conservative
}

enum DepthSortModes
{
    None,
    FrontToBack,
    BackToFront
}

public class Settings
{
    [ExpandGroup(true)]
    public class SunAndSky
    {
        [HelpText("Enables the sun light")]
        bool EnableSun = true;

        [HelpText("Controls whether the sun is treated as a disc area light in the real-time shader")]
        bool SunAreaLightApproximation = true;

        [HelpText("Angular radius of the sun in degrees")]
        [MinValue(0.01f)]
        [StepSize(0.01f)]
        float SunSize = 1.0f;

        [HelpText("Direction of the sun")]
        [DisplayInViewSpaceAttribute(true)]
        Direction SunDirection = new Direction(0.26f, 0.987f, -0.16f);

        [MinValue(1.0f)]
        [MaxValue(10.0f)]
        [UseAsShaderConstant(false)]
        [HelpText("Atmospheric turbidity (thickness) uses for procedural sun and sky model")]
        float Turbidity = 2.0f;

        [HDR(false)]
        [UseAsShaderConstant(false)]
        [HelpText("Ground albedo color used for procedural sun and sky model")]
        Color GroundAlbedo = new Color(0.25f, 0.25f, 0.25f);
    }

    [ExpandGroup(false)]
    public class AntiAliasing
    {
        [HelpText("MSAA mode to use for rendering")]
        [DisplayName("MSAA Mode")]
        MSAAModes MSAAMode = MSAAModes.MSAA4x;
    }

    [ExpandGroup(true)]
    public class Scene
    {
        [UseAsShaderConstant(false)]
        Scenes CurrentScene = Scenes.BoxTest;

        [HelpText("Enable or disable deferred light rendering")]
        bool RenderLights = false;
    }

    const uint ClusterTileSize = 16;
    const uint NumZTiles = 16;

    const uint MaxSpotLights = 32;
    const uint SpotLightElementsPerCluster = MaxSpotLights / 32;
    const float SpotLightRange = 7.5f;

    const float SpotShadowNearClip = 0.1f;

    [ExpandGroup(false)]
    public class Rendering
    {
        [UseAsShaderConstant(false)]
        [MinValue(0)]
        [MaxValue((int)MaxSpotLights)]
        [DisplayName("Max Lights")]
        [HelpText("Limits the number of lights in the scene")]
        int MaxLightClamp = (int)MaxSpotLights;

        [UseAsShaderConstant(false)]
        [HelpText("Conservative rasterization mode to use for light binning")]
        ClusterRasterizationModes ClusterRasterizationMode = ClusterRasterizationModes.Conservative;
    }

    const uint NumSampleSets = 8;
    const uint SampleTileSize = 32;
    const uint NumPixelsPerTile = SampleTileSize * SampleTileSize;

    const uint MaxPathLengthSetting = 8;

    [ExpandGroup(true)]
    public class PathTracing
    {
        bool EnableRayTracing = true;

        [HelpText("The square root of the number of per-pixel sample rays to use for path tracing")]
        [MinValue(1)]
        [MaxValue(100)]
        [DisplayName("Sqrt Num Samples")]
        int SqrtNumSamples = 4;

        [HelpText("Maximum path length (bounces) to use for path tracing")]
        [MinValue(2)]
        [MaxValue(MaxPathLengthSetting)]
        [DisplayName("Max Path Length")]
        int MaxPathLength = 3;
    }

    [ExpandGroup(false)]
    public class PostProcessing
    {
        [MinValue(-24.0f)]
        [MaxValue(24.0f)]
        [StepSize(0.1f)]
        [HelpText("Simple exposure value applied to the scene before tone mapping (uses log2 scale)")]
        float Exposure = -14.0f;

        [DisplayName("Bloom Exposure Offset")]
        [MinValue(-10.0f)]
        [MaxValue(0.0f)]
        [StepSize(0.01f)]
        [HelpText("Exposure offset applied to generate the input of the bloom pass")]
        float BloomExposure = -4.0f;

        [DisplayName("Bloom Magnitude")]
        [MinValue(0.0f)]
        [MaxValue(2.0f)]
        [StepSize(0.01f)]
        [HelpText("Scale factor applied to the bloom results when combined with tone-mapped result")]
        float BloomMagnitude = 1.0f;

        [DisplayName("Bloom Blur Sigma")]
        [MinValue(0.5f)]
        [MaxValue(2.5f)]
        [StepSize(0.01f)]
        [HelpText("Sigma parameter of the Gaussian filter used in the bloom pass")]
        float BloomBlurSigma = 2.5f;
    }

    [ExpandGroup(true)]
    public class Debug
    {
        [UseAsShaderConstant(false)]
        [DisplayName("Enable VSync")]
        [HelpText("Enables or disables vertical sync during Present")]
        bool EnableVSync = true;

        [DisplayName("Enable Albedo Maps")]
        [HelpText("Enables albedo maps")]
        bool EnableAlbedoMaps = true;

        [DisplayName("Enable Normal Maps")]
        [HelpText("Enables normal maps")]
        bool EnableNormalMaps = true;

        [HelpText("Enables diffuse reflections")]
        bool EnableDiffuse = true;

        [HelpText("Enables specular reflections")]
        bool EnableSpecular = true;

        [HelpText("Enables direct lighting")]
        bool EnableDirect = true;

        [HelpText("Enables indirect lighting")]
        bool EnableIndirect = true;

        [HelpText("Enables indirect specular reflections, it produces noisier output")]
        bool EnableIndirectSpecular = false;

        [HelpText("Scales the scene roughness by this value")]
        [MinValue(0.001f)]
        [MaxValue(2.0f)]
        float RoughnessScale = 1.0f;

        [UseAsShaderConstant(false)]
        bool AlwaysResetPathTrace = false;

        [UseAsShaderConstant(false)]
        bool ShowProgressBar = true;
    }
}