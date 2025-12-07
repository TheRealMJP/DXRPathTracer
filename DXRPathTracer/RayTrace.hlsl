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

enum RayTypes
{
    RayTypeHitInfo = 0,

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

float DielectricFresnel(float currentIOR, float enteringIOR, float3 microfacetNormal, float3 viewDir)
{
    float F = 1.0f;
    const float cosThetaI = saturate(dot(viewDir, microfacetNormal));
    if (currentIOR > enteringIOR)
    {
        // From "Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering", use cos(thetaT)
        // so that we get total internal reflection behavior
        const float cosThetaT2 = 1.0f - ((1.0f - (cosThetaI * cosThetaI))  / Square(rcp(currentIOR)));
        if(cosThetaT2 > 0)
            F = Fresnel(IORToF0(currentIOR, enteringIOR), sqrt(cosThetaT2));
        else
            F = 1.0f;   // Total internal reflection
    }
    else
    {
        F = Fresnel(IORToF0(currentIOR, enteringIOR), cosThetaI);
    }

    return F;
}

float CosTheta(float3 w) {
    return w.z;
}
float Cos2Theta(float3 w) {
    return Square(w.z);
}
float AbsCosTheta(float3 w) {
    return abs(w.z);
}

float Sin2Theta(float3 w) {
    return max(0, 1 - Cos2Theta(w));
}
float SinTheta(float3 w) {
    return sqrt(Sin2Theta(w));
}

float TanTheta(float3 w) {
    return SinTheta(w) / CosTheta(w);
}
float Tan2Theta(float3 w) {
    return Sin2Theta(w) / Cos2Theta(w);
}

float CosPhi(float3 w) {
    float sinTheta = SinTheta(w);
    return (sinTheta == 0) ? 1 : clamp(w.x / sinTheta, -1, 1);
}
float SinPhi(float3 w) {
    float sinTheta = SinTheta(w);
    return (sinTheta == 0) ? 0 : clamp(w.y / sinTheta, -1, 1);
}

float Lambda(float3 w, float2 alpha) {
    float tan2Theta = Tan2Theta(w);
    if (isinf(tan2Theta))
        return 0;
    float alpha2 = Square(CosPhi(w) * alpha.x) + Square(SinPhi(w) * alpha.y);
    return (sqrt(1 + alpha2 * tan2Theta) - 1) / 2;
}

float GGX_G1(float3 w, float2 alpha)
{
    return 1 / (1 + Lambda(w, alpha));
}

float GGX_G(float3 wo, float3 wi, float2 alpha)
{
    return 1 / (1 + Lambda(wo, alpha) + Lambda(wi, alpha));
}

float GGX_Refract(float3 viewDirTS, float3 microfacetNormalTS, float3 refractDirTS, float roughness, float interfaceIOR)
{
    float denom = Square(dot(refractDirTS, microfacetNormalTS) + dot(viewDirTS, microfacetNormalTS) / interfaceIOR);
    float refractBRDF = GGX_D(roughness, microfacetNormalTS.z) * GGX_G(viewDirTS, refractDirTS, roughness) * abs(dot(refractDirTS, microfacetNormalTS) * dot(viewDirTS, microfacetNormalTS) / (refractDirTS.z * viewDirTS.z * denom));
    return refractBRDF;
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

        // ##########################################################################
        // traceRayFlags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
        // traceRayFlags |= RAY_FLAG_CULL_FRONT_FACING_TRIANGLES;

        const uint hitGroupOffset = RayTypeHitInfo;
        const uint hitGroupGeoMultiplier = NumRayTypes;
        const uint missShaderIdx = RayTypeHitInfo;

        PrimaryPayload payload;
        TraceRay(GetSceneAS(), traceRayFlags, 0xFFFFFFFF, hitGroupOffset, hitGroupGeoMultiplier, missShaderIdx, segmentRay, payload);

        if (AppSettings.DrawDebugPaths)
            ShaderDebug::DrawArrow(segmentRay.Origin, segmentRay.Origin + segmentRay.Direction * (payload.HitT >= 0.0f ? payload.HitT : 1000.0f), float4(0, 1, 0, 1.0f), 0.025f);

        if (pathLength == 1)
            primaryRayT = payload.HitT;

        // ##########################################################################
        if (currentMedium.IsVolumetric() && 0)
            currentMedium = Medium::Default();

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

                float phaseProbility = 1.0f;
                float lightProbability = (AppSettings.EnableSun && AppSettings.EnableDirectLightSampling) ? 1.0f : 0.0f;
                const float probabilitySum = phaseProbility + lightProbability;
                phaseProbility /= probabilitySum;
                lightProbability /= probabilitySum;

                const float phaseSelector = rng.Sample1D();
                float3 scatterDir = 0.0f;
                if (phaseSelector < phaseProbility)
                {
                    scatterDir = SampleHenyeyGreenstein(-segmentRay.Direction, currentMedium.Anisotropy, rng.Sample2D());
                }
                else
                {
                    const float3x3 sunFrame = CoordinateSystem(RayTraceCB.SunDirectionWS);
                    scatterDir = mul(SampleDirectionCone(rng.Sample2D(), RayTraceCB.CosSunAngularRadius), sunFrame);
                }

                const float phaseFunction = HenyeyGreenstein(dot(scatterDir, segmentRay.Direction), currentMedium.Anisotropy);
                const float phasePDF = phaseFunction;
                const float lightPDF = (AppSettings.EnableSun && dot(scatterDir, RayTraceCB.SunDirectionWS) >= RayTraceCB.CosSunAngularRadius) ? SampleDirectionCone_PDF(RayTraceCB.CosSunAngularRadius) : 0.0f;
                const float totalPDF = phaseProbility * phasePDF + lightProbability * lightPDF;
                const float invPDF = totalPDF > 0.0f ? (1.0f / totalPDF) : 0.0f;

                pathThroughput *= phaseFunction * invPDF;

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

            Medium enteringMedium = Medium::Init(material);
            if (payload.HitFrontFace == false)
                enteringMedium = Medium::Default();

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

            const bool enableSpecular =  (AppSettings.EnableSpecular && material.HasSpecular() && (AppSettings.EnableIndirectSpecular || pathLength == 1));
            const bool enableRefraction = enableSpecular && (enteringMedium.IsVolumetric() || currentMedium.IsVolumetric());
            const bool enableDiffuse = (AppSettings.EnableDiffuse && metallic < 1.0f && !enableRefraction);
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

            const float3 incomingRayDirTS = normalize(mul(incomingRayDirWS, transpose(tangentToWorld)));
            const float3 viewDirTS = -incomingRayDirTS;

            // Diffuse sampling, cosine-weighted hemisphere
            const float3 diffuseDirTS = SampleDirectionCosineHemisphere(rng.Sample2D());

            // Specular sampling, VNDF reflection
            const float3 microfacetNormalTS = SampleGGXMicrofacetVNDF(viewDirTS, roughness, rng.Sample2D());
            const float F = DielectricFresnel(currentMedium.IOR, enteringMedium.IOR, microfacetNormalTS, viewDirTS);
            const float T = saturate(1.0f - F);
            const float3 reflectDirTS = reflect(incomingRayDirTS, microfacetNormalTS);

            // Refraction sampling, VNDF refraction
            const float interfaceIOR = currentMedium.IOR / enteringMedium.IOR;
            const float3 refractDirTS = refract(incomingRayDirTS, microfacetNormalTS, interfaceIOR);

            // Light sampling, cone around the sun
            const float3x3 sunFrame = CoordinateSystem(RayTraceCB.SunDirectionWS);
            const float3 sunDirWS = mul(SampleDirectionCone(rng.Sample2D(), RayTraceCB.CosSunAngularRadius), sunFrame);
            const float3 sunDirTS = normalize(mul(sunDirWS, transpose(tangentToWorld)));

            // Choose our next path with multiple importance sampling
            const float selector = rng.Sample1D();
            float diffuseProbability = enableDiffuse ? saturate(1.0f - metallic) * T : 0.0f;
            float reflectProbability = enableSpecular ? (enableRefraction ? F : 1.0f) : 0.0f;
            float refractProbability = enableRefraction ? T : 0.0f;
            float lightProbability = (AppSettings.EnableSun && AppSettings.EnableDirectLightSampling) ? saturate(dot(normalWS, RayTraceCB.SunDirectionWS)) : 0.0f;

            const float probabilitySum = (diffuseProbability + reflectProbability + refractProbability + lightProbability);
            diffuseProbability /= probabilitySum;
            reflectProbability /= probabilitySum;
            refractProbability /= probabilitySum;
            lightProbability /= probabilitySum;

            float3 nextRayDirTS = 0.0f;
            if(selector < diffuseProbability)
                nextRayDirTS = diffuseDirTS;
            else if(selector < (diffuseProbability + reflectProbability))
                nextRayDirTS = reflectDirTS;
            else if(selector < (diffuseProbability + reflectProbability + refractProbability))
                nextRayDirTS = refractDirTS;
            else
                nextRayDirTS = sunDirTS;

            const float3 nextRayDirWS = normalize(mul(nextRayDirTS, tangentToWorld));
            const bool refracted = enableRefraction && dot(nextRayDirTS, microfacetNormalTS) < 0.0f;
            const float nDotL = saturate(nextRayDirTS.z);

            const float diffusePDF = enableDiffuse ? SampleDirectionCosineHemisphere_PDF(nDotL) : 0.0f;
            const float reflectPDF = (enableSpecular && !refracted) ? SampleGGXReflectionVNDF_PDF(viewDirTS, nextRayDirTS, roughness) : 0.0f;
            const float refractPDF = (enableRefraction && refracted) ? SampleGGXRefractionVNDF_PDF(viewDirTS, nextRayDirTS, microfacetNormalTS, roughness, interfaceIOR) : 0.0f;
            const float lightPDF = (AppSettings.EnableSun && dot(nextRayDirWS, RayTraceCB.SunDirectionWS) >= RayTraceCB.CosSunAngularRadius) ? SampleDirectionCone_PDF(RayTraceCB.CosSunAngularRadius) : 0.0f;
            const float totalPDF = (diffuseProbability * diffusePDF) + (reflectProbability * reflectPDF) + (refractProbability * refractPDF) + (lightProbability * lightPDF);
            const float invPDF = totalPDF > 0.0f ? (1.0f / totalPDF) : 0.0f;

            const bool internalReflection = enableRefraction && !refracted;

            float3 brdf = 0.0f;

            if (enableDiffuse)
                brdf += diffuseAlbedo * InvPi * nDotL;

            if(enableSpecular)
            {
                if (refracted)
                {
                    float refractBRDF = GGX_Refract(viewDirTS, microfacetNormalTS, nextRayDirTS, roughness, interfaceIOR);
                    brdf += refractBRDF * T; // * saturate(nextRayDirTS.z);
                }
                else
                {
                    float3 halfDirTS = normalize(nextRayDirTS + viewDirTS);
                    float3 normalTS = float3(0, 0, 1);
                    float spec = GGXSpecular(roughness, normalTS, halfDirTS, viewDirTS, nextRayDirTS);
                    brdf += Fresnel(specularF0, halfDirTS, nextRayDirTS) * spec * nDotL;
                }
            }

            pathThroughput *= brdf * invPDF;

            // Shoot another ray to get the next path
            RayDesc newRay;
            newRay.Origin = positionWS;
            newRay.Direction = nextRayDirWS;
            newRay.TMin = 0.00001f;
            newRay.TMax = FP32Max;

            segmentRay = newRay;

            if (internalReflection == false)
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

[shader("miss")]
void MissShader(inout PrimaryPayload payload)
{
    payload.HitT = -1.0f;
}