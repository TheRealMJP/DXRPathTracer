//=================================================================================================
//
//  DXR Path Tracer
//  by MJP
//  http://mynameismjp.wordpress.com/
//
//  All code and content licensed under the MIT license
//
//=================================================================================================

//=================================================================================================
// Includes
//=================================================================================================
#include <DescriptorTables.hlsl>
#include <Constants.hlsl>
#include <Quaternion.hlsl>
#include <BRDF.hlsl>
#include <RayTracing.hlsl>
#include <Sampling.hlsl>

#include "SharedTypes.h"
#include "AppSettings.hlsl"

struct RayTraceConstants
{
    row_major float4x4 InvViewProjection;

    float3 SunDirectionWS;
    float CosSunAngularRadius;
    float3 SunIrradiance;
    float SinSunAngularRadius;
    float3 SunRenderColor;
    uint Padding;
    float3 CameraPosWS;
    uint CurrSampleIdx;
    uint TotalNumPixels;

    uint VtxBufferIdx;
    uint IdxBufferIdx;
    uint GeometryInfoBufferIdx;
    uint MaterialBufferIdx;
    uint SkyTextureIdx;
};

struct HitConstants
{
    uint GeometryIdx;
    uint MaterialIdx;
};

RaytracingAccelerationStructure Scene : register(t0, space200);
RWTexture2D<float4> RenderTarget : register(u0);
StructuredBuffer<GeometryInfo> GeometryInfoBuffers[] : register(t0, space100);
StructuredBuffer<MeshVertex> VertexBuffers[] : register(t0, space101);
StructuredBuffer<Material> MaterialBuffers[] : register(t0, space102);

ConstantBuffer<RayTraceConstants> RayTraceCB : register(b0);
ConstantBuffer<HitConstants> HitCB : register(b0, space200);

SamplerState MeshSampler : register(s0);
SamplerState LinearSampler : register(s1);

typedef BuiltInTriangleIntersectionAttributes HitAttributes;
struct PrimaryPayload
{
    float3 Radiance;
    uint PathLength;
    uint PixelIdx;
    uint SampleSetIdx;
};

struct ShadowPayload
{
    float Visibility;
};

enum RayTypes {
    RayTypeRadiance = 0,
    RayTypeShadow = 1,

    NumRayTypes
};

static float2 SamplePoint(in uint pixelIdx, inout uint setIdx)
{
    const uint permutation = setIdx * RayTraceCB.TotalNumPixels + pixelIdx;
    setIdx += 1;
    return SampleCMJ2D(RayTraceCB.CurrSampleIdx, AppSettings.SqrtNumSamples, AppSettings.SqrtNumSamples, permutation);
}

[shader("raygeneration")]
void RaygenShader()
{
    const uint2 pixelCoord = DispatchRaysIndex().xy;
    const uint pixelIdx = pixelCoord.y * DispatchRaysDimensions().x + pixelCoord.x;

    uint sampleSetIdx = 0;

    // Form a primary ray by un-projecting the pixel coordinate using the inverse view * projection matrix
    float2 primaryRaySample = SamplePoint(pixelIdx, sampleSetIdx);

    float2 rayPixelPos = pixelCoord + primaryRaySample;
    float2 ncdXY = (rayPixelPos / (DispatchRaysDimensions().xy * 0.5f)) - 1.0f;
    ncdXY.y *= -1.0f;
    float4 rayStart = mul(float4(ncdXY, 0.0f, 1.0f), RayTraceCB.InvViewProjection);
    float4 rayEnd = mul(float4(ncdXY, 1.0f, 1.0f), RayTraceCB.InvViewProjection);

    rayStart.xyz /= rayStart.w;
    rayEnd.xyz /= rayEnd.w;
    float3 rayDir = normalize(rayEnd.xyz - rayStart.xyz);
    float rayLength = length(rayEnd.xyz - rayStart.xyz);

    // Trace a primary ray
    RayDesc ray;
    ray.Origin = rayStart.xyz;
    ray.Direction = rayDir;
    ray.TMin = 0.0f;
    ray.TMax = rayLength;

    PrimaryPayload payload;
    payload.Radiance = 0.0f;
    payload.PathLength = 1;
    payload.PixelIdx = pixelIdx;
    payload.SampleSetIdx = sampleSetIdx;

    const uint hitGroupOffset = RayTypeRadiance;
    const uint hitGroupGeoMultiplier = NumRayTypes;
    const uint missShaderIdx = RayTypeRadiance;
    TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFFFFFFFF, hitGroupOffset, hitGroupGeoMultiplier, missShaderIdx, ray, payload);

    payload.Radiance = clamp(payload.Radiance, 0.0f, FP16Max);

    // Update the progressive result with the new radiance sample
    const float lerpFactor = RayTraceCB.CurrSampleIdx / (RayTraceCB.CurrSampleIdx + 1.0f);
    float3 newSample = payload.Radiance;
    float3 currValue = RenderTarget[pixelCoord].xyz;
    float3 newValue = lerp(newSample, currValue, lerpFactor);

    RenderTarget[pixelCoord] = float4(newValue, 1.0f);
}

static float3 PathTrace(in MeshVertex hitSurface, in Material material, in uint pathLength, in uint pixelIdx, in uint sampleSetIdx)
{
    if((!AppSettings.EnableDiffuse && !AppSettings.EnableSpecular) ||
        (!AppSettings.EnableDirect && !AppSettings.EnableIndirect))
        return 0.0.xxx;

    if(pathLength > 1 && !AppSettings.EnableIndirect)
        return 0.0.xxx;

    float3x3 tangentToWorld = float3x3(hitSurface.Tangent, hitSurface.Bitangent, hitSurface.Normal);

    const float3 positionWS = hitSurface.Position;

    const float3 incomingRayOriginWS = WorldRayOrigin();
    const float3 incomingRayDirWS = WorldRayDirection();

    float3 normalWS = hitSurface.Normal;
    if(AppSettings.EnableNormalMaps)
    {
        // Sample the normal map, and convert the normal to world space
        Texture2D normalMap = Tex2DTable[material.Normal];

        float3 normalTS;
        normalTS.xy = normalMap.SampleLevel(MeshSampler, hitSurface.UV, 0.0f).xy * 2.0f - 1.0f;
        normalTS.z = sqrt(1.0f - saturate(normalTS.x * normalTS.x + normalTS.y * normalTS.y));
        normalWS = normalize(mul(normalTS, tangentToWorld));

        tangentToWorld._31_32_33 = normalWS;
    }

    float3 baseColor = 1.0f;
    if(AppSettings.EnableAlbedoMaps)
    {
        Texture2D albedoMap = Tex2DTable[material.Albedo];
        baseColor = albedoMap.SampleLevel(MeshSampler, hitSurface.UV, 0.0f).xyz;
    }

    Texture2D metallicMap = Tex2DTable[material.Metallic];
    const float metallic = metallicMap.SampleLevel(MeshSampler, hitSurface.UV, 0.0f).x;

    const bool enableDiffuse = (AppSettings.EnableDiffuse && metallic < 1.0f);
    const bool enableSpecular = (AppSettings.EnableSpecular && (AppSettings.EnableIndirectSpecular ? true : (pathLength == 1)));
    if(enableDiffuse == false && enableSpecular == false)
        return 0.0f;

    Texture2D roughnessMap = Tex2DTable[material.Roughness];
    const float sqrtRoughness = roughnessMap.SampleLevel(MeshSampler, hitSurface.UV, 0.0f).x * AppSettings.RoughnessScale;

    const float3 diffuseAlbedo = lerp(baseColor, 0.0f, metallic) * (enableDiffuse ? 1.0f : 0.0f);
    const float3 specularAlbedo = lerp(0.03f, baseColor, metallic) * (enableSpecular ? 1.0f : 0.0f);
    const float roughness = sqrtRoughness * sqrtRoughness;

    Texture2D emissiveMap = Tex2DTable[material.Emissive];
    float3 radiance = emissiveMap.SampleLevel(MeshSampler, hitSurface.UV, 0.0f).xyz;

    if(AppSettings.EnableSun)
    {
        float3 sunDirection = RayTraceCB.SunDirectionWS;

        if(AppSettings.SunAreaLightApproximation)
        {
            float3 D = RayTraceCB.SunDirectionWS;
            float3 R = reflect(incomingRayDirWS, normalWS);
            float r = RayTraceCB.SinSunAngularRadius;
            float d = RayTraceCB.CosSunAngularRadius;
            float3 DDotR = dot(D, R);
            float3 S = R - DDotR * D;
            sunDirection = DDotR < d ? normalize(d * D + normalize(S) * r) : R;
        }

        // Shoot a shadow ray to see if the sun is occluded
        RayDesc ray;
        ray.Origin = positionWS;
        ray.Direction = RayTraceCB.SunDirectionWS;
        ray.TMin = 0.00001f;
        ray.TMax = FP32Max;

        ShadowPayload payload;
        payload.Visibility = 1.0f;

        const uint hitGroupOffset = RayTypeShadow;
        const uint hitGroupGeoMultiplier = NumRayTypes;
        const uint missShaderIdx = RayTypeShadow;
        TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFFFFFFFF, hitGroupOffset, hitGroupGeoMultiplier, missShaderIdx, ray, payload);

        radiance += CalcLighting(normalWS, sunDirection, RayTraceCB.SunIrradiance, diffuseAlbedo, specularAlbedo,
                                 roughness, positionWS, incomingRayOriginWS) * payload.Visibility;
    }

    // Choose our next path by importance sampling our BRDFs
    float2 brdfSample = SamplePoint(pixelIdx, sampleSetIdx);

    float3 throughput = 0.0f;
    float3 rayDirTS = 0.0f;

    float selector = brdfSample.x;
    if(enableSpecular == false)
        selector = 0.0f;
    else if(enableDiffuse == false)
        selector = 1.0f;

    if(selector < 0.5f)
    {
        // We're sampling the diffuse BRDF, so sample a cosine-weighted hemisphere
        if(enableSpecular)
            brdfSample.x *= 2.0f;
        rayDirTS = SampleDirectionCosineHemisphere(brdfSample.x, brdfSample.y);

        // The PDF of sampling a cosine hemisphere is NdotL / Pi, which cancels out those terms
        // from the diffuse BRDF and the irradiance integral
        throughput = diffuseAlbedo;
    }
    else
    {
        // We're sampling the GGX specular BRDF by sampling the distribution of visible normals. See this post
        // for more info: https://schuttejoe.github.io/post/ggximportancesamplingpart2/.
        // Also see: https://hal.inria.fr/hal-00996995v1/document and https://hal.archives-ouvertes.fr/hal-01509746/document
        if(enableDiffuse)
            brdfSample.x = (brdfSample.x - 0.5f) * 2.0f;

        float3 incomingRayDirTS = normalize(mul(incomingRayDirWS, transpose(tangentToWorld)));
        float3 microfacetNormalTS = SampleGGXVisibleNormal(-incomingRayDirTS, roughness, roughness, brdfSample.x, brdfSample.y);
        float3 sampleDirTS = reflect(incomingRayDirTS, microfacetNormalTS);

        float3 normalTS = float3(0.0f, 0.0f, 1.0f);

        float3 F = Fresnel(specularAlbedo, microfacetNormalTS, sampleDirTS);
        float G1 = SmithGGXMasking(normalTS, sampleDirTS, -incomingRayDirTS, roughness * roughness);
        float G2 = SmithGGXMaskingShadowing(normalTS, sampleDirTS, -incomingRayDirTS, roughness * roughness);

        throughput = (F * (G2 / G1));
        rayDirTS = sampleDirTS;
    }

    const float3 rayDirWS = normalize(mul(rayDirTS, tangentToWorld));

    if(enableDiffuse && enableSpecular)
        throughput *= 2.0f;

    // Shoot another ray to get the next path
    RayDesc ray;
    ray.Origin = positionWS;
    ray.Direction = rayDirWS;
    ray.TMin = 0.00001f;
    ray.TMax = FP32Max;

    if(pathLength == 1 && !AppSettings.EnableDirect)
        radiance = 0.0.xxx;

    if(AppSettings.EnableIndirect && (pathLength + 1 < AppSettings.MaxPathLength))
    {
        PrimaryPayload payload;
        payload.Radiance = 0.0f;
        payload.PathLength = pathLength + 1;
        payload.PixelIdx = pixelIdx;
        payload.SampleSetIdx = sampleSetIdx;

        const uint hitGroupOffset = RayTypeRadiance;
        const uint hitGroupGeoMultiplier = NumRayTypes;
        const uint missShaderIdx = RayTypeRadiance;
        TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFFFFFFFF, hitGroupOffset, hitGroupGeoMultiplier, missShaderIdx, ray, payload);

        radiance += payload.Radiance * throughput;
    }
    else
    {
        ShadowPayload payload;
        payload.Visibility = 1.0f;

        const uint hitGroupOffset = RayTypeShadow;
        const uint hitGroupGeoMultiplier = NumRayTypes;
        const uint missShaderIdx = RayTypeShadow;
        TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFFFFFFFF, hitGroupOffset, hitGroupGeoMultiplier, missShaderIdx, ray, payload);

        TextureCube skyTexture = TexCubeTable[RayTraceCB.SkyTextureIdx];
        float3 skyRadiance = skyTexture.SampleLevel(LinearSampler, rayDirWS, 0.0f).xyz;

        radiance += payload.Visibility * skyRadiance * throughput;
    }

    return radiance;
}

[shader("closesthit")]
void ClosestHitShader(inout PrimaryPayload payload, in HitAttributes attr)
{
    float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

    StructuredBuffer<GeometryInfo> geoInfoBuffer = GeometryInfoBuffers[RayTraceCB.GeometryInfoBufferIdx];
    const GeometryInfo geoInfo = geoInfoBuffer[HitCB.GeometryIdx];

    StructuredBuffer<MeshVertex> vtxBuffer = VertexBuffers[RayTraceCB.VtxBufferIdx];
    Buffer<uint> idxBuffer = BufferUintTable[RayTraceCB.IdxBufferIdx];

    StructuredBuffer<Material> materialBuffer = MaterialBuffers[RayTraceCB.MaterialBufferIdx];
    const Material material = materialBuffer[geoInfo.MaterialIdx];

    const uint primIdx = PrimitiveIndex();
    const uint idx0 = idxBuffer[primIdx * 3 + geoInfo.IdxOffset + 0];
    const uint idx1 = idxBuffer[primIdx * 3 + geoInfo.IdxOffset + 1];
    const uint idx2 = idxBuffer[primIdx * 3 + geoInfo.IdxOffset + 2];

    const MeshVertex vtx0 = vtxBuffer[idx0 + geoInfo.VtxOffset];
    const MeshVertex vtx1 = vtxBuffer[idx1 + geoInfo.VtxOffset];
    const MeshVertex vtx2 = vtxBuffer[idx2 + geoInfo.VtxOffset];

    const MeshVertex hitSurface = BarycentricLerp(vtx0, vtx1, vtx2, barycentrics);

    payload.Radiance = PathTrace(hitSurface, material, payload.PathLength, payload.PixelIdx, payload.SampleSetIdx);
}

[shader("miss")]
void MissShader(inout PrimaryPayload payload)
{
    const float3 rayDir = WorldRayDirection();

    TextureCube skyTexture = TexCubeTable[RayTraceCB.SkyTextureIdx];
    payload.Radiance = skyTexture.SampleLevel(LinearSampler, rayDir, 0.0f).xyz;

    if(payload.PathLength == 1)
    {
        float cosSunAngle = dot(rayDir, RayTraceCB.SunDirectionWS);
        if(cosSunAngle >= RayTraceCB.CosSunAngularRadius)
            payload.Radiance = RayTraceCB.SunRenderColor;
    }
}

[shader("closesthit")]
void ShadowHitShader(inout ShadowPayload payload, in HitAttributes attr)
{
    payload.Visibility = 0.0f;
}

[shader("miss")]
void ShadowMissShader(inout ShadowPayload payload)
{
    payload.Visibility = 1.0f;
}