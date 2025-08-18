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

struct ClusterBounds
{
    ShaderFloat3 Position;
    ShaderQuaternion Orientation;
    ShaderFloat3 Scale;
    ShaderUint2 ZBounds;
};

struct ResolveConstants
{
    ShaderUint2 OutputSize;
    DescriptorIndex InputTextureIdx;
};
