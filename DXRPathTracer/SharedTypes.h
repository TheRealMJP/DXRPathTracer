//=================================================================================================
//
//  DXR Path Tracer
//  by MJP
//  http://mynameismjp.wordpress.com/
//
//  All code and content licensed under the MIT license
//
//=================================================================================================

#pragma once

#if CPP_
    #include <Shaders/ShaderShared.h>
#else
    #include <ShaderShared.h>
#endif

static const ShaderHalf MinSigma = ShaderHalf(0.01f);
static const ShaderHalf MaxSigma = ShaderHalf(65000.0f);

enum MaterialFlags : uint16_t
{
    MaterialFlags_EnableSpecular = 1u << 0,

    MaterialFlags_Default = MaterialFlags_EnableSpecular,
};

struct Material
{
    DescriptorIndex BaseColor;
    ShaderHalf3 BaseColorTint;
    ShaderHalf BaseColorIntensity;
    DescriptorIndex Normal;
    DescriptorIndex Roughness;
    ShaderHalf NormalMapIntensity;
    ShaderHalf RoughnessScale;
    DescriptorIndex Metallic;
    DescriptorIndex Opacity;
    DescriptorIndex Emissive;
    ShaderHalf MetallicOffset;
    ShaderHalf3 EmissiveTint;
    ShaderHalf SigmaA;
    ShaderHalf SigmaS;
    ShaderHalf PhaseAnisotropy;
    ShaderUint16 Flags;

    bool IsVolumetric()
    {
        return (SigmaA + SigmaS) < MaxSigma;
    }

    bool HasSpecular()
    {
        return (Flags & MaterialFlags_EnableSpecular) ? true : false;
    }
};

struct SpotLight
{
    ShaderFloat3 Position;
    ShaderFloat AngularAttenuationX;
    ShaderFloat3 Direction;
    ShaderFloat AngularAttenuationY;
    ShaderFloat3 Intensity;
    ShaderFloat Range;
};

struct RayTraceConstants
{
    ShaderFloat4x4 InvViewProjection;
    ShaderFloat4x4 ViewProjection;

    ShaderFloat3 SunDirectionWS;
    ShaderFloat CosSunAngularRadius;
    ShaderFloat3 SunIrradiance;
    ShaderFloat SinSunAngularRadius;
    ShaderFloat3 SunRenderColor;
    ShaderBool EnableWhiteFurnaceMode;
    ShaderFloat3 CameraPosWS;
    ShaderUint CurrSampleIdx;
    ShaderUint TotalNumPixels;

    DescriptorIndex VtxBufferIdx;
    DescriptorIndex IdxBufferIdx;
    DescriptorIndex GeometryInfoBufferIdx;
    DescriptorIndex MaterialBufferIdx;
    DescriptorIndex SkyTextureIdx;
    ShaderUint NumLights;

    DescriptorIndex SceneAS;
    DescriptorIndex RenderTarget;
    DescriptorIndex DepthTarget;
};
