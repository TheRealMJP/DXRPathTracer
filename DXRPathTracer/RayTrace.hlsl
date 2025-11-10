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
#include <StaticSamplers.hlsli>
#include <Constants.hlsli>
#include <Quaternion.hlsli>
#include <BRDF.hlsli>
#include <RayTracing.hlsli>
#include <Sampling.hlsli>
#include <ShaderDebug.hlsli>

#include "SharedTypes.h"
#include "AppSettings.hlsli"

struct LightConstants
{
    SpotLight Lights[MaxSpotLights];
};

ConstantBuffer<RayTraceConstants> RayTraceCB : register(b0);
ConstantBuffer<LightConstants> LightCBuffer : register(b1);

RaytracingAccelerationStructure GetSceneAS()
{
    return ResourceDescriptorHeap[RayTraceCB.SceneAS];
}

typedef BuiltInTriangleIntersectionAttributes HitAttributes;

struct [raypayload] PrimaryPayload
{
    float HitT : read(caller) : write(closesthit, miss);
    uint HitGeometryIndex : read(caller) : write(closesthit);
    uint HitTriangleIndex : read(caller) : write(closesthit);
    float2 HitBarycentrics : read(caller) : write(closesthit);
    bool HitFrontFace : read(caller) : write(closesthit);
};

struct [raypayload] ShadowPayload
{
    float Visibility : read(caller) : write(closesthit, miss);
};

enum RayTypes
{
    RayTypeRadiance = 0,
    RayTypeShadow = 1,

    NumRayTypes
};

struct RNG
{
    uint pixelIdx;
    uint setIdx;

    static RNG Init(uint pixelIdx)
    {
        RNG rng = { pixelIdx, 0 };
        return rng;
    }

    float Sample1D()
    {
        return Sample2D().x;
    }

    float2 Sample2D()
    {
        const uint permutation = setIdx * RayTraceCB.TotalNumPixels + pixelIdx;
        setIdx += 1;
        return SampleCMJ2D(RayTraceCB.CurrSampleIdx, AppSettings.SqrtNumSamples, AppSettings.SqrtNumSamples, permutation);
    }
};

struct Medium
{
    half SigmaA;
    half SigmaS;
    half Anisotropy;
    half IOR;
    uint16_t Flags;

    static Medium Default()
    {
        Medium medium;
        medium.SigmaA = MaxSigma;
        medium.SigmaS = MaxSigma;
        medium.Anisotropy = 0.0h;
        medium.Flags = 0;
        medium.IOR = 1.0h;

        return medium;
    }

    static Medium Init(Material material)
    {
        Medium medium;
        medium.SigmaA = material.SigmaA;
        medium.SigmaS = material.SigmaS;
        medium.Anisotropy = material.PhaseAnisotropy;
        medium.IOR = 1.33h; // assuming fixed water-like IOR at the momeent
        medium.Flags = material.Flags;

        return medium;
    }

    bool IsVolumetric()
    {
        return SigmaT() < MaxSigma;
    }

    half SigmaT()
    {
        return SigmaA + SigmaS;
    }

    bool HasSpecular()
    {
        return (Flags & MaterialFlags_EnableSpecular) ? true : false;
    }
};

// Loops up the vertex data for the hit triangle and interpolates its attributes
MeshVertex GetHitSurface(in float2 hitBarycentrics, in uint geometryIdx, in uint primitiveIdx, bool frontFace)
{
    float3 barycentrics = float3(1 - hitBarycentrics.x - hitBarycentrics.y, hitBarycentrics.x, hitBarycentrics.y);

    StructuredBuffer<GeometryInfo> geoInfoBuffer = ResourceDescriptorHeap[RayTraceCB.GeometryInfoBufferIdx];
    const GeometryInfo geoInfo = geoInfoBuffer[geometryIdx];

    StructuredBuffer<MeshVertex> vtxBuffer = ResourceDescriptorHeap[RayTraceCB.VtxBufferIdx];
    Buffer<uint> idxBuffer = ResourceDescriptorHeap[RayTraceCB.IdxBufferIdx];

    const uint idx0 = idxBuffer[primitiveIdx * 3 + geoInfo.IdxOffset + 0];
    const uint idx1 = idxBuffer[primitiveIdx * 3 + geoInfo.IdxOffset + 1];
    const uint idx2 = idxBuffer[primitiveIdx * 3 + geoInfo.IdxOffset + 2];

    const MeshVertex vtx0 = vtxBuffer[idx0 + geoInfo.VtxOffset];
    const MeshVertex vtx1 = vtxBuffer[idx1 + geoInfo.VtxOffset];
    const MeshVertex vtx2 = vtxBuffer[idx2 + geoInfo.VtxOffset];

    MeshVertex finalVertex = BarycentricLerp(vtx0, vtx1, vtx2, barycentrics);
    if (frontFace == false)
    {
        finalVertex.Normal *= -1;
        finalVertex.Tangent *= -1;
        finalVertex.Bitangent *= -1;
    }
    return finalVertex;
}

// Gets the material assigned to a geometry in the acceleration structure
Material GetGeometryMaterial(in uint geometryIdx)
{
    StructuredBuffer<GeometryInfo> geoInfoBuffer = ResourceDescriptorHeap[RayTraceCB.GeometryInfoBufferIdx];
    const GeometryInfo geoInfo = geoInfoBuffer[geometryIdx];

    StructuredBuffer<Material> materialBuffer = ResourceDescriptorHeap[RayTraceCB.MaterialBufferIdx];
    return materialBuffer[geoInfo.MaterialIdx];
}

float4 PathTrace(RayDesc initialRay, inout RNG rng)
{
    float3 pathRadiance = 0.0f;
    float3 pathThroughput = 1.0f;
    float pathMaxRoughness = 0.0f;
    float primaryRayT = -1.0f;
    Medium currentMedium = Medium::Default();

    RayDesc segmentRay = initialRay;

    bool hitVolumetric = false;

    const uint maxPathLength = AppSettings.EnableIndirect ? AppSettings.MaxPathLength : 2;
    for(uint pathLength = 1; pathLength <= maxPathLength; ++pathLength)
    {
        // Trace a ray into the scene
        uint traceRayFlags = 0;

        // Stop using the any-hit shader once we've hit the max path length, since it's *really* expensive
        if(pathLength > AppSettings.MaxAnyHitPathLength)
            traceRayFlags = RAY_FLAG_FORCE_OPAQUE;

        const uint hitGroupOffset = RayTypeRadiance;
        const uint hitGroupGeoMultiplier = NumRayTypes;
        const uint missShaderIdx = RayTypeRadiance;

        PrimaryPayload payload;
        TraceRay(GetSceneAS(), traceRayFlags, 0xFFFFFFFF, hitGroupOffset, hitGroupGeoMultiplier, missShaderIdx, segmentRay, payload);

        if (AppSettings.DrawDebugPaths)
            ShaderDebug::DrawArrow(segmentRay.Origin, segmentRay.Origin + segmentRay.Direction * (payload.HitT >= 0.0f ? payload.HitT : 1000.0f), float4(0, 1, 0, 1.0f), 0.025f);

        if (pathLength == 1)
            primaryRayT = payload.HitT;

        if (currentMedium.IsVolumetric())
        {
            const float exitT = payload.HitT >= 0.0f ? payload.HitT : FP32Max;
            const float sigmaT = currentMedium.SigmaT();

            const float uStep = rng.Sample1D();
            const float scatterEventT = segmentRay.TMin + SampleExponential(uStep, sigmaT);

            if (scatterEventT < exitT)
            {
                // const float transmittance *= exp(-(scatterEventT - currentT) * sigmaT);

                const float absorbProbability = currentMedium.SigmaA / sigmaT;
                const float scatterProbability = currentMedium.SigmaS / sigmaT;
                const float uScatterMode = rng.Sample1D();
                if (uScatterMode < absorbProbability)
                    break;

                // We can ignore the PDF, it's exact for sampling HG and cancels out
                const float2 phaseU1U2 = rng.Sample2D();
                const float3 scatterDir = SampleHenyeyGreenstein(-segmentRay.Direction, currentMedium.Anisotropy, phaseU1U2);

                RayDesc newRay;
                newRay.Origin = segmentRay.Origin + (segmentRay.Direction * scatterEventT);
                newRay.Direction = scatterDir;
                newRay.TMin = 0.0f;
                newRay.TMax = FP32Max;

                segmentRay = newRay;
                continue;
            }
        }

        if (payload.HitT >= 0.0f)
        {
            const MeshVertex hitSurface = GetHitSurface(payload.HitBarycentrics, payload.HitGeometryIndex, payload.HitTriangleIndex, payload.HitFrontFace);
            const Material material = GetGeometryMaterial(payload.HitGeometryIndex);

            const Medium enteringMedium = Medium::Init(material);
            if (enteringMedium.IsVolumetric() && !enteringMedium.HasSpecular())
            {
                // Just continue on and we'll handle scattering in the next iteration
                if (currentMedium.IsVolumetric())
                    currentMedium = Medium::Default();
                else
                    currentMedium = enteringMedium;

                RayDesc newRay;
                newRay.Origin = hitSurface.Position;
                newRay.Direction = segmentRay.Direction;
                newRay.TMin = 0.00001f;
                newRay.TMax = FP32Max;

                segmentRay = newRay;

                continue;
            }

            Texture2D emissiveMap = ResourceDescriptorHeap[NonUniformResourceIndex(material.Emissive)];
            pathRadiance += emissiveMap.SampleLevel(LinearSampler, hitSurface.UV, 0.0f).xyz * material.EmissiveTint * pathThroughput;

            // No point and continuing any further, anything else requires more segments
            if (pathLength == maxPathLength)
                break;

            float3x3 tangentToWorld = float3x3(hitSurface.Tangent, hitSurface.Bitangent, hitSurface.Normal);

            const float3 positionWS = hitSurface.Position;

            const float3 incomingRayOriginWS = segmentRay.Origin;
            const float3 incomingRayDirWS = segmentRay.Direction;

            float3 normalWS = hitSurface.Normal;
            if(AppSettings.EnableNormalMaps)
            {
                // Sample the normal map, and convert the normal to world space
                Texture2D normalMap = ResourceDescriptorHeap[NonUniformResourceIndex(material.Normal)];

                float3 normalTS;
                normalTS.xy = normalMap.SampleLevel(LinearSampler, hitSurface.UV, 0.0f).xy * 2.0f - 1.0f;
                normalTS.z = sqrt(1.0f - saturate(normalTS.x * normalTS.x + normalTS.y * normalTS.y));
                normalWS = lerp(normalWS, normalize(mul(normalTS, tangentToWorld)), material.NormalMapIntensity);

                // tangentToWorld._31_32_33 = normalWS;
            }

            tangentToWorld = CoordinateSystem(normalWS);

            float3 baseColor = 1.0f;
            if(AppSettings.EnableBaseColorMaps)
            {
                Texture2D baseColorMap = ResourceDescriptorHeap[NonUniformResourceIndex(material.BaseColor)];
                baseColor = baseColorMap.SampleLevel(LinearSampler, hitSurface.UV, 0.0f).xyz;
                baseColor = saturate(baseColor * material.BaseColorTint * material.BaseColorIntensity);
            }

            Texture2D metallicMap = ResourceDescriptorHeap[NonUniformResourceIndex(material.Metallic)];
            const float metallic = saturate(metallicMap.SampleLevel(LinearSampler, hitSurface.UV, 0.0f).x + material.MetallicOffset + AppSettings.MetallicOffset);

            const bool enableDiffuse = (AppSettings.EnableDiffuse && metallic < 1.0f && !material.IsVolumetric());
            const bool enableSpecular =  (AppSettings.EnableSpecular && material.HasSpecular() && (AppSettings.EnableIndirectSpecular || pathLength == 1));
            const bool enableSun = AppSettings.EnableSun; //  && dot(normalWS, RayTraceCB.SunDirectionWS) >= 0.0f;
            if (enableDiffuse == false && enableSpecular == false)
                break;

            Texture2D roughnessMap = ResourceDescriptorHeap[NonUniformResourceIndex(material.Roughness)];
            const float sqrtRoughness = clamp(roughnessMap.SampleLevel(LinearSampler, hitSurface.UV, 0.0f).x * material.RoughnessScale * AppSettings.RoughnessScale, 0.025f, 1.0f);

            const float3 diffuseAlbedo = lerp(baseColor, 0.0f, metallic) * (enableDiffuse ? 1.0f : 0.0f);
            const float3 specularF0 = lerp(0.03f, baseColor, metallic) * (enableSpecular ? 1.0f : 0.0f);
            float roughness = sqrtRoughness * sqrtRoughness;
            if(AppSettings.ClampRoughness)
            {
                roughness = max(roughness, pathMaxRoughness);
                pathMaxRoughness = roughness;
            }

            // Choose our next path with multiple importance sampling
            const float selector = rng.Sample1D();
            float diffuseProbability = enableDiffuse ? saturate(1.0f - metallic) : 0.0f;
            float specularProbability = enableSpecular ? 1.0f : 0.0f;
            float lightProbability = (enableSun && AppSettings.EnableDirectLightSampling) ? saturate(dot(normalWS, RayTraceCB.SunDirectionWS)) : 0.0f;

            const float probabilitySum = (diffuseProbability + specularProbability + lightProbability);
            diffuseProbability /= probabilitySum;
            specularProbability /= probabilitySum;
            lightProbability /= probabilitySum;

            float3 nextRayDirTS = 0.0f;
            const float3 incomingRayDirTS = normalize(mul(incomingRayDirWS, transpose(tangentToWorld)));
            const float3 viewDirTS = -incomingRayDirTS;

            if(selector < diffuseProbability)
            {
                // We're sampling the diffuse BRDF, so sample a cosine-weighted hemisphere
                nextRayDirTS = SampleDirectionCosineHemisphere(rng.Sample2D());
            }
            else if(selector < (diffuseProbability + specularProbability))
            {
                nextRayDirTS = SampleGGXReflectionVNDF(viewDirTS, roughness, rng.Sample2D());

                /*float3 microfacetNormalTS = SampleGGXVisibleNormal(-incomingRayDirTS, roughness, roughness, rng.Sample2D());
                float3 sampleDirTS = reflect(incomingRayDirTS, microfacetNormalTS);

                float3 normalTS = float3(0.0f, 0.0f, 1.0f);

                float3 F = RayTraceCB.EnableWhiteFurnaceMode ? 1.0.xxx : Fresnel(specularF0, microfacetNormalTS, sampleDirTS);
                if (currentMedium.IsVolumetric())
                {
                    // From "Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering", use cos(thetaT)
                    // so that we get total internal reflection behavior
                    const float cosThetaI = saturate(dot(-incomingRayDirTS, microfacetNormalTS));
                    const float cosThetaT2 = 1.0f - ((1.0f - (cosThetaI * cosThetaI))  / Square(rcp(currentMedium.IOR)));
                    if(cosThetaT2 > 0)
                    {
                        F = Fresnel(IORToF0Air(currentMedium.IOR), sqrt(cosThetaT2));
                    }
                    else
                    {
                        F = 1.0f;
                    }
                }

                float G1 = SmithGGXMasking(normalTS, sampleDirTS, -incomingRayDirTS, roughness * roughness);
                float G2 = SmithGGXMaskingShadowing(normalTS, sampleDirTS, -incomingRayDirTS, roughness * roughness);

                brdfThroughput = specularProbability;

                const float refractProbability = currentMedium.IsVolumetric() ? saturate(1.0f - F.x) : 0.0f;
                if (rng.Sample1D() < refractProbability)
                {
                    // brdfThroughput *= saturate(1.0f - F.x);
                    // brdfThroughput /= refractProbability;
                    float interfaceIOR = currentMedium.IOR / enteringMedium.IOR;
                    nextRayDirTS = refract(incomingRayDirTS, microfacetNormalTS, interfaceIOR);
                }
                else
                {
                    brdfThroughput *= (F * (G2 / G1));
                    brdfThroughput /= (1.0f - refractProbability);
                    nextRayDirTS = sampleDirTS;

                    if(AppSettings.ApplyMultiscatteringEnergyCompensation)
                    {
                        float2 DFG = GGXEnvironmentBRDFScaleBias(saturate(dot(normalTS, -incomingRayDirWS)), sqrtRoughness);

                        // Improve energy preservation by applying a scaled version of the original
                        // single scattering specular lobe. Based on "Practical multiple scattering
                        // compensation for microfacet models" [Turquin19].
                        //
                        // See: https://blog.selfshadow.com/publications/turquin/ms_comp_final.pdf
                        float Ess = DFG.x;
                        brdfThroughput *= 1.0.xxx + specularF0 * (1.0f / Ess - 1.0f);
                    }
                }*/
            }
            else
            {
                const float3x3 sunFrame = CoordinateSystem(RayTraceCB.SunDirectionWS);
                const float3 nextRayDirWS = mul(SampleDirectionCone(rng.Sample2D(), RayTraceCB.CosSunAngularRadius), sunFrame);
                nextRayDirTS = normalize(mul(nextRayDirWS, transpose(tangentToWorld)));
            }

            const float3 nextRayDirWS = normalize(mul(nextRayDirTS, tangentToWorld));
            const float nDotL = saturate(nextRayDirTS.z);

            const float diffusePDF = enableDiffuse ? SampleDirectionCosineHemisphere_PDF(nDotL) : 0.0f;
            const float specularPDF = enableSpecular ? SampleGGXReflectionVNDF_PDF(viewDirTS, nextRayDirTS, roughness) : 0.0f;
            const float lightPDF = (enableSun && dot(nextRayDirWS, RayTraceCB.SunDirectionWS) >= RayTraceCB.CosSunAngularRadius) ? SampleDirectionCone_PDF(RayTraceCB.CosSunAngularRadius) : 0.0f;
            const float totalPDF = diffuseProbability * diffusePDF + specularProbability * specularPDF + lightProbability * lightPDF;
            const float invPDF = totalPDF > 0.0f ? (1.0f / totalPDF) : 0.0f;

            float3 brdf = 0.0f;

            if (enableDiffuse)
                brdf += diffuseAlbedo * InvPi;

            if(enableSpecular)
            {
                float3 halfDirTS = normalize(nextRayDirTS + viewDirTS);
                float3 normalTS = float3(0, 0, 1);
                float spec = GGXSpecular(roughness, normalTS, halfDirTS, viewDirTS, nextRayDirTS);
                brdf += Fresnel(specularF0, halfDirTS, nextRayDirTS) * spec;
            }

            pathThroughput *= brdf * nDotL * invPDF;

            // Shoot another ray to get the next path
            RayDesc newRay;
            newRay.Origin = positionWS;
            newRay.Direction = nextRayDirWS;
            newRay.TMin = 0.00001f;
            newRay.TMax = FP32Max;

            segmentRay = newRay;
            currentMedium = enteringMedium;
        }
        else
        {
            // We didn't hit anything, sample the sun/sky
            float3 skyEmissive = 0.0f;

            if(RayTraceCB.EnableWhiteFurnaceMode)
            {
                skyEmissive = 1.0f;
            }
            else
            {
                const float3 rayDir = segmentRay.Direction;

                TextureCube skyTexture = ResourceDescriptorHeap[RayTraceCB.SkyTextureIdx];
                skyEmissive = AppSettings.EnableSky ? skyTexture.SampleLevel(LinearSampler, rayDir, 0.0f).xyz : 0.0.xxx;

                if (AppSettings.EnableSun)
                {
                    float cosSunAngle = dot(rayDir, RayTraceCB.SunDirectionWS);
                    if(cosSunAngle >= RayTraceCB.CosSunAngularRadius)
                        skyEmissive = RayTraceCB.SunRenderColor;
                }
            }

            pathRadiance += skyEmissive * pathThroughput;

            break;
        }
    }

    return float4(pathRadiance, primaryRayT);
}

[shader("raygeneration")]
void RaygenShader()
{
    const uint2 pixelCoord = DispatchRaysIndex().xy;
    const uint pixelIdx = pixelCoord.y * DispatchRaysDimensions().x + pixelCoord.x;
    RNG rng = RNG::Init(pixelIdx);

    ShaderDebug::FilterIfCursorOnPos(pixelCoord);

    // Form a primary ray by un-projecting the pixel coordinate using the inverse view * projection matrix
    float2 primaryRaySample = rng.Sample2D();

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
    RayDesc primaryRay;
    primaryRay.Origin = rayStart.xyz;
    primaryRay.Direction = rayDir;
    primaryRay.TMin = 0.0f;
    primaryRay.TMax = rayLength;

    float4 radianceAndHitT = PathTrace(primaryRay, rng);

    float3 radiance = clamp(radianceAndHitT.xyz, 0.0f, FP16Max);

    RWTexture2D<float4> renderTarget = ResourceDescriptorHeap[RayTraceCB.RenderTarget];

    // Update the progressive result with the new radiance sample
    const float lerpFactor = RayTraceCB.CurrSampleIdx / (RayTraceCB.CurrSampleIdx + 1.0f);
    float3 newSample = radiance;
    float3 currValue = renderTarget[pixelCoord].xyz;
    float3 newValue = lerp(newSample, currValue, lerpFactor);

    renderTarget[pixelCoord] = float4(newValue, 1.0f);

    RWTexture2D<float> depthTarget = ResourceDescriptorHeap[RayTraceCB.DepthTarget];

    float3 hitPos = primaryRay.Origin + primaryRay.Direction * radianceAndHitT.w;
    float4 projectedHitPos = mul(float4(hitPos, 1.0f), RayTraceCB.ViewProjection);
    depthTarget[pixelCoord] = projectedHitPos.z / projectedHitPos.w;
}

[shader("closesthit")]
void ClosestHitShader(inout PrimaryPayload payload, in HitAttributes attr)
{
    payload.HitGeometryIndex = GeometryIndex();
    payload.HitTriangleIndex = PrimitiveIndex();
    payload.HitT = RayTCurrent();
    payload.HitBarycentrics = attr.barycentrics;
    payload.HitFrontFace = HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE;
}

[shader("anyhit")]
void AnyHitShader(inout PrimaryPayload payload, in HitAttributes attr)
{
    const MeshVertex hitSurface = GetHitSurface(attr.barycentrics, GeometryIndex(), PrimitiveIndex(), true);
    const Material material = GetGeometryMaterial(GeometryIndex());

    // Standard alpha testing
    Texture2D opacityMap = ResourceDescriptorHeap[NonUniformResourceIndex(material.Opacity)];
    if(opacityMap.SampleLevel(LinearSampler, hitSurface.UV, 0.0f).x < 0.35f)
        IgnoreHit();
}

[shader("anyhit")]
void ShadowAnyHitShader(inout ShadowPayload payload, in HitAttributes attr)
{
    const MeshVertex hitSurface = GetHitSurface(attr.barycentrics, GeometryIndex(), PrimitiveIndex(), true);
    const Material material = GetGeometryMaterial(GeometryIndex());

    // Standard alpha testing
    Texture2D opacityMap = ResourceDescriptorHeap[NonUniformResourceIndex(material.Opacity)];
    if(opacityMap.SampleLevel(LinearSampler, hitSurface.UV, 0.0f).x < 0.35f)
        IgnoreHit();
}

[shader("miss")]
void MissShader(inout PrimaryPayload payload)
{
    payload.HitT = -1.0f;
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