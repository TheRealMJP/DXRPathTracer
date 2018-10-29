//=================================================================================================
//
//  DXR Path Tracer
//  by MJP
//  http://mynameismjp.wordpress.com/
//
//  All code and content licensed under the MIT license
//
//=================================================================================================

#if _WINDOWS

#pragma once

typedef SampleFramework12::Float2 float2;
typedef SampleFramework12::Float3 float3;
typedef SampleFramework12::Float4 float4;

typedef uint32 uint;
typedef SampleFramework12::Uint2 uint2;
typedef SampleFramework12::Uint3 uint3;
typedef SampleFramework12::Uint4 uint4;

typedef SampleFramework12::Int2 int2;
typedef SampleFramework12::Int3 int3;
typedef SampleFramework12::Int4 int4;

#endif

struct Material
{
    uint Albedo;
    uint Normal;
    uint Roughness;
    uint Metallic;
    uint Opacity;
    uint Emissive;
};

struct SpotLight
{
    float3 Position;
    float AngularAttenuationX;
    float3 Direction;
    float AngularAttenuationY;
    float3 Intensity;
    float Range;
};

struct ClusterBounds
{
    float3 Position;
    Quaternion Orientation;
    float3 Scale;
    uint2 ZBounds;
};

struct GeometryInfo
{
    uint VtxOffset;
    uint IdxOffset;
    uint MaterialIdx;
    uint PadTo16Bytes;
};
