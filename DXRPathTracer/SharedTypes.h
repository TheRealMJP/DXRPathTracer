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
    DescriptorIndex Albedo;
    DescriptorIndex Normal;
    DescriptorIndex Roughness;
    DescriptorIndex Metallic;
    DescriptorIndex Opacity;
    DescriptorIndex Emissive;
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
    ShaderUint Padding;
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
