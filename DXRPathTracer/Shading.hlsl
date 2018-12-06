//=================================================================================================
//
//  DXR Path Tracer
//  by MJP
//  http://mynameismjp.wordpress.com/
//
//  All code and content licensed under the MIT license
//
//=================================================================================================

// Options
#ifndef UseImplicitShadowDerivatives_
    #define UseImplicitShadowDerivatives_ 0
#endif

#define ShadowMapMode_ 0
#define UseGatherPCF_ 1

#include <DescriptorTables.hlsl>
#include <SH.hlsl>
#include <Shadows.hlsl>
#include <BRDF.hlsl>
#include <Quaternion.hlsl>
#include "AppSettings.hlsl"
#include "SharedTypes.h"

struct ShadingConstants
{
    float3 SunDirectionWS;
    float CosSunAngularRadius;
    float3 SunIrradiance;
    float SinSunAngularRadius;
    float3 CameraPosWS;

    uint NumXTiles;
    uint NumXYTiles;
    float NearClip;
    float FarClip;

    SH9Color SkySH;
};

struct LightConstants
{
    SpotLight Lights[MaxSpotLights];
    float4x4 ShadowMatrices[MaxSpotLights];
};

struct ShadingInput
{
    uint2 PositionSS;
    float3 PositionWS;
    float3 PositionWS_DX;
    float3 PositionWS_DY;
    float DepthVS;
    float3x3 TangentFrame;

    float4 AlbedoMap;
    float2 NormalMap;
    float RoughnessMap;
    float MetallicMap;
    float3 EmissiveMap;

    ByteAddressBuffer SpotLightClusterBuffer;

    SamplerState AnisoSampler;

    ShadingConstants ShadingCBuffer;
    SunShadowConstants ShadowCBuffer;
    LightConstants LightCBuffer;
};

//-------------------------------------------------------------------------------------------------
// Calculates the full shading result for a single pixel. Note: some of the input textures
// are passed directly to this function instead of through the ShadingInput struct in order to
// work around incorrect behavior from the shader compiler
//-------------------------------------------------------------------------------------------------
float3 ShadePixel(in ShadingInput input, in Texture2DArray sunShadowMap,
                  in Texture2DArray spotLightShadowMap, in SamplerComparisonState shadowSampler)
{
    float3 vtxNormalWS = input.TangentFrame._m20_m21_m22;
    float3 normalWS = vtxNormalWS;
    float3 positionWS = input.PositionWS;

    const ShadingConstants CBuffer = input.ShadingCBuffer;
    const SunShadowConstants ShadowCBuffer = input.ShadowCBuffer;

    float3 viewWS = normalize(CBuffer.CameraPosWS - positionWS);

    if(AppSettings.EnableNormalMaps)
    {
        // Sample the normal map, and convert the normal to world space
        float3 normalTS;
        normalTS.xy = input.NormalMap * 2.0f - 1.0f;
        normalTS.z = sqrt(1.0f - saturate(normalTS.x * normalTS.x + normalTS.y * normalTS.y));
        normalWS = normalize(mul(normalTS, input.TangentFrame));
    }

    float4 albedoMap =  1.0f;
    if(AppSettings.EnableAlbedoMaps)
        albedoMap = input.AlbedoMap;

    float metallic = saturate(input.MetallicMap);
    float3 diffuseAlbedo = lerp(albedoMap.xyz, 0.0f, metallic);
    float3 specularAlbedo = lerp(0.03f, albedoMap.xyz, metallic) * (AppSettings.EnableSpecular ? 1.0f : 0.0f);

    float roughnessMap = input.RoughnessMap;
    float roughness = roughnessMap * roughnessMap;

    float depthVS = input.DepthVS;

    // Compute shared cluster lookup data
    uint2 pixelPos = uint2(input.PositionSS);
    float zRange = CBuffer.FarClip - CBuffer.NearClip;
    float normalizedZ = saturate((depthVS - CBuffer.NearClip) / zRange);
    uint zTile = normalizedZ * NumZTiles;

    uint3 tileCoords = uint3(pixelPos / ClusterTileSize, zTile);
    uint clusterIdx = (tileCoords.z * CBuffer.NumXYTiles) + (tileCoords.y * CBuffer.NumXTiles) + tileCoords.x;

    float3 positionNeighborX = input.PositionWS + input.PositionWS_DX;
    float3 positionNeighborY = input.PositionWS + input.PositionWS_DY;

    // Add in the primary directional light
    float3 output = 0.0f;

    if(AppSettings.EnableSun)
    {
        float3 sunDirection = CBuffer.SunDirectionWS;

        float2 shadowMapSize;
        float numSlices;
        sunShadowMap.GetDimensions(shadowMapSize.x, shadowMapSize.y, numSlices);

        const float3 shadowPosOffset = GetShadowPosOffset(saturate(dot(vtxNormalWS, sunDirection)), vtxNormalWS, shadowMapSize.x);

        #if UseImplicitShadowDerivatives_
            // Forward path
            float sunShadowVisibility = SunShadowVisibility(positionWS, depthVS, shadowPosOffset, 0.0f, sunShadowMap, shadowSampler, ShadowCBuffer);
        #else
            // Deferred path
            float sunShadowVisibility = SunShadowVisibility(positionWS, positionNeighborX, positionNeighborY,
                                                            depthVS, shadowPosOffset, 0.0f, sunShadowMap, shadowSampler, ShadowCBuffer);
        #endif

        if(AppSettings.SunAreaLightApproximation)
        {
            float3 D = CBuffer.SunDirectionWS;
            float3 R = reflect(-viewWS, normalWS);
            float r = CBuffer.SinSunAngularRadius;
            float d = CBuffer.CosSunAngularRadius;
            float3 DDotR = dot(D, R);
            float3 S = R - DDotR * D;
            sunDirection = DDotR < d ? normalize(d * D + normalize(S) * r) : R;
        }
        output += CalcLighting(normalWS, sunDirection, CBuffer.SunIrradiance, diffuseAlbedo, specularAlbedo,
                                 roughness, positionWS, CBuffer.CameraPosWS) * sunShadowVisibility;
    }

    // Apply the spot lights
    uint numLights = 0;
    if(AppSettings.RenderLights)
    {
        float2 shadowMapSize;
        float numSlices;
        spotLightShadowMap.GetDimensions(shadowMapSize.x, shadowMapSize.y, numSlices);

        uint clusterOffset = clusterIdx * SpotLightElementsPerCluster;

        // Loop over the number of 4-byte elements needed for each cluster
        [unroll]
        for(uint elemIdx = 0; elemIdx < SpotLightElementsPerCluster; ++elemIdx)
        {
            // Loop until we've processed every raised bit
            uint clusterElemMask = input.SpotLightClusterBuffer.Load((clusterOffset + elemIdx) * 4);
            while(clusterElemMask)
            {
                uint bitIdx = firstbitlow(clusterElemMask);
                clusterElemMask &= ~(1 << bitIdx);
                uint spotLightIdx = bitIdx + (elemIdx * 32);
                SpotLight spotLight = input.LightCBuffer.Lights[spotLightIdx];

                float3 surfaceToLight = spotLight.Position - positionWS;
                float distanceToLight = length(surfaceToLight);
                surfaceToLight /= distanceToLight;
                float angleFactor = saturate(dot(surfaceToLight, spotLight.Direction));
                float angularAttenuation = smoothstep(spotLight.AngularAttenuationY, spotLight.AngularAttenuationX, angleFactor);

                if(angularAttenuation > 0.0f)
                {
                    float d = distanceToLight / spotLight.Range;
                    float falloff = saturate(1.0f - (d * d * d * d));
                    falloff = (falloff * falloff) / (distanceToLight * distanceToLight + 1.0f);
                    float3 intensity = spotLight.Intensity * angularAttenuation * falloff;

                    const float3 shadowPosOffset = GetShadowPosOffset(saturate(dot(vtxNormalWS, surfaceToLight)), vtxNormalWS, shadowMapSize.x);

                    // We have to use explicit gradients for spotlight shadows, since the looping/branching is non-uniform
                    float spotLightVisibility = SpotLightShadowVisibility(positionWS, positionNeighborX, positionNeighborY,
                                                                          input.LightCBuffer.ShadowMatrices[spotLightIdx],
                                                                          spotLightIdx, shadowPosOffset, spotLightShadowMap, shadowSampler,
                                                                          float2(SpotShadowNearClip, spotLight.Range), ShadowCBuffer.Extra);

                    output += CalcLighting(normalWS, surfaceToLight, intensity, diffuseAlbedo, specularAlbedo,
                                           roughness, positionWS, CBuffer.CameraPosWS) * spotLightVisibility;
                }

                ++numLights;
            }
        }
    }

    float3 ambient = EvalSH9Irradiance(normalWS, CBuffer.SkySH) * InvPi;
    ambient *= 0.1f; // Darken the ambient since we don't have any sky occlusion
    output += ambient * diffuseAlbedo;

    output += input.EmissiveMap;

    output = clamp(output, 0.0f, FP16Max);

    return output;
}