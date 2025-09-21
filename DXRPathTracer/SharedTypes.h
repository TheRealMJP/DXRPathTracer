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

struct Material
{
    DescriptorIndex BaseColor;
    ShaderFloat3 BaseColorTint;
    DescriptorIndex Normal;
    DescriptorIndex Roughness;
    ShaderFloat RoughnessScale;
    DescriptorIndex Metallic;
    ShaderFloat MetallicOffset;
    DescriptorIndex Opacity;
    DescriptorIndex Emissive;
    ShaderFloat3 EmissiveTint;
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
