//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#ifndef BRDF_HLSL_
#define BRDF_HLSL_

#include <Distributions.hlsl>
#include <Sampling.hlsl>

//-------------------------------------------------------------------------------------------------
// Calculates the Fresnel factor using Schlick's approximation
//-------------------------------------------------------------------------------------------------
float3 Fresnel(in float3 specAlbedo, in float3 h, in float3 l)
{
    float3 fresnel = specAlbedo + (1.0f - specAlbedo) * pow((1.0f - saturate(dot(l, h))), 5.0f);

    // Fade out spec entirely when lower than 0.1% albedo
    fresnel *= saturate(dot(specAlbedo, 333.0f));

    return fresnel;
}

//-------------------------------------------------------------------------------------------------
// Calculates the Fresnel factor using Schlick's approximation
//-------------------------------------------------------------------------------------------------
float3 Fresnel(in float3 specAlbedo, in float3 fresnelAlbedo, in float3 h, in float3 l)
{
    float3 fresnel = specAlbedo + (fresnelAlbedo - specAlbedo) * pow((1.0f - saturate(dot(l, h))), 5.0f);

    // Fade out spec entirely when lower than 0.1% albedo
    fresnel *= saturate(dot(specAlbedo, 333.0f));

    return fresnel;
}

//-------------------------------------------------------------------------------------------------
// Helper for computing the Beckmann geometry term
//-------------------------------------------------------------------------------------------------
float BeckmannG1(float m, float nDotX)
{
    float nDotX2 = nDotX * nDotX;
    float tanTheta = sqrt((1 - nDotX2) / nDotX2);
    float a = 1.0f / (m * tanTheta);
    float a2 = a * a;

    float g = 1.0f;
    if(a < 1.6f)
        g *= (3.535f * a + 2.181f * a2) / (1.0f + 2.276f * a + 2.577f * a2);

    return g;
}

//-------------------------------------------------------------------------------------------------
// Computes the specular term using a Beckmann microfacet distribution, with a matching
// geometry factor and visibility term. Based on "Microfacet Models for Refraction Through
// Rough Surfaces" [Walter 07]. m is roughness, n is the surface normal, h is the half vector,
// l is the direction to the light source, and specAlbedo is the RGB specular albedo
//-------------------------------------------------------------------------------------------------
float BeckmannSpecular(in float m, in float3 n, in float3 h, in float3 v, in float3 l)
{
    float nDotH = max(dot(n, h), 0.0001f);
    float nDotL = saturate(dot(n, l));
    float nDotV = max(dot(n, v), 0.0001f);

    float nDotH2 = nDotH * nDotH;
    float nDotH4 = nDotH2 * nDotH2;
    float m2 = m * m;

    // Calculate the distribution term
    float tanTheta2 = (1 - nDotH2) / nDotH2;
    float expTerm = exp(-tanTheta2 / m2);
    float d = expTerm / (Pi * m2 * nDotH4);

    // Calculate the matching geometric term
    float g1i = BeckmannG1(m, nDotL);
    float g1o = BeckmannG1(m, nDotV);
    float g = g1i * g1o;

    return d * g * (1.0f / (4.0f * nDotL * nDotV));
}

float BeckmannDistribution(float3 wh, float alphax, float alphay) {
    float tan2Theta = Tan2Theta(wh);
    if (isinf(tan2Theta)) return 0.;
    float cos4Theta = Cos2Theta(wh) * Cos2Theta(wh);
    return exp(-tan2Theta * (Cos2Phi(wh) / (alphax * alphax) +
                                  Sin2Phi(wh) / (alphay * alphay))) /
        (Pi * alphax * alphay * cos4Theta);
}

float3 SchlickFresnel(float cosTheta, float3 specularAlbedo) {
    return specularAlbedo + pow(1 - cosTheta, 5) * (float3(1, 1, 1) - specularAlbedo);
}

//-------------------------------------------------------------------------------------------------
// Helper for computing the GGX visibility term
//-------------------------------------------------------------------------------------------------
float GGXV1(in float m2, in float nDotX)
{
    return 1.0f / (nDotX + sqrt(m2 + (1 - m2) * nDotX * nDotX));
}

//-------------------------------------------------------------------------------------------------
// Computes the GGX visibility term
//-------------------------------------------------------------------------------------------------
float GGXVisibility(in float m2, in float nDotL, in float nDotV)
{
    return GGXV1(m2, nDotL) * GGXV1(m2, nDotV);
}

float SmithGGXMasking(float3 n, float3 l, float3 v, float a2)
{
    float dotNL = saturate(dot(n, l));
    float dotNV = saturate(dot(n, v));
    float denomC = sqrt(a2 + (1.0f - a2) * dotNV * dotNV) + dotNV;

    return 2.0f * dotNV / denomC;
}

float SmithGGXMaskingShadowing(float3 n, float3 l, float3 v, float a2)
{
    float dotNL = saturate(dot(n, l));
    float dotNV = saturate(dot(n, v));

    float denomA = dotNV * sqrt(a2 + (1.0f - a2) * dotNL * dotNL);
    float denomB = dotNL * sqrt(a2 + (1.0f - a2) * dotNV * dotNV);

    return 2.0f * dotNL * dotNV / (denomA + denomB);
}

//-------------------------------------------------------------------------------------------------
// Computes the specular term using a GGX microfacet distribution, with a matching
// geometry factor and visibility term. Based on "Microfacet Models for Refraction Through
// Rough Surfaces" [Walter 07]. m is roughness, n is the surface normal, h is the half vector,
// l is the direction to the light source, and specAlbedo is the RGB specular albedo
//-------------------------------------------------------------------------------------------------
float GGXSpecular(in float m, in float3 n, in float3 h, in float3 v, in float3 l)
{
    float nDotH = saturate(dot(n, h));
    float nDotL = saturate(dot(n, l));
    float nDotV = saturate(dot(n, v));

    float nDotH2 = nDotH * nDotH;
    float m2 = m * m;

    // Calculate the distribution term
    float x = nDotH * nDotH * (m2 - 1) + 1;
    float d = m2 / (Pi * x * x);

    // Calculate the matching visibility term
    float vis = GGXVisibility(m2, nDotL, nDotV);

    return d * vis;
}

float GGXSpecularAnisotropic(float m, float3 n, float3 h, float3 v, float3 l, float3 tx, float3 ty, float anisotropy)
{
    float nDotH = saturate(dot(n, h));
    float nDotL = saturate(dot(n, l));
    float nDotV = saturate(dot(n, v));
    float nDotH2 = nDotH * nDotH;

    float ax = m;
    float ay = lerp(ax, 1.0f, anisotropy);
    float ax2 = ax * ax;
    float ay2 = ay * ay;

    float xDotH = dot(tx, h);
    float yDotH = dot(ty, h);
    float xDotH2 = xDotH * xDotH;
    float yDotH2 = yDotH * yDotH;

    // Calculate the distribution term
    float denom = (xDotH2 / ax2) + (yDotH2 / ay2) + nDotH2;
    denom *= denom;
    float d = (1.0f / (Pi * ax * ay)) * 1.0f / denom;

    // Calculate the matching visibility term
    float vis = GGXVisibility(m * m, nDotL, nDotV);

    return d * vis;
}

// Distribution term for the velvet BRDF
float VelvetDistribution(in float m, in float nDotH2, in float offset)
{
    m = 0.25f + 0.75f * m;
    float cot2 = nDotH2 / (1.000001f - nDotH2);
    float sin2 = 1.0f - nDotH2;
    float sin4 = sin2 * sin2;
    float amp = 4.0f;
    float m2 = m * m + 0.000001f;
    float cnorm = 1.0f / (Pi * (offset + amp * m2));

    return cnorm * (offset + (amp * exp(-cot2 / (m2 + 0.000001f)) / sin4));
}

// Specular term for the velvet BRDF
float VelvetSpecular(in float m, in float3 n, in float3 h, in float3 v, in float3 l, in float offset)
{
    float nDotH = saturate(dot(n, h));
    float nDotH2 = nDotH * nDotH;
    float nDotV = saturate(dot(n, v));
    float nDotL = saturate(dot(n, l));

    float D = VelvetDistribution(m, nDotH2, offset);
    float G = 1.0f;
    float denom = 1.0f / (4.0f * (nDotL + nDotV - nDotL * nDotV));
    return D * G * denom;
}

//-------------------------------------------------------------------------------------------------
// Returns scale and bias values for environment specular reflections that represents the
// integral of the geometry/visibility + fresnel terms for a GGX BRDF given a particular
// viewing angle and roughness value. The final value is computed using polynomials that were
// fitted to tabulated data generated via monte carlo integration.
//-------------------------------------------------------------------------------------------------
float2 GGXEnvironmentBRDFScaleBias(in float nDotV, in float sqrtRoughness)
{
    const float nDotV2 = nDotV * nDotV;
    const float sqrtRoughness2 = sqrtRoughness * sqrtRoughness;
    const float sqrtRoughness3 = sqrtRoughness2 * sqrtRoughness;

    const float delta = 0.991086418474895f + (0.412367709802119f * sqrtRoughness * nDotV2) -
                        (0.363848256078895f * sqrtRoughness2) -
                        (0.758634385642633f * nDotV * sqrtRoughness2);
    const float bias = saturate((0.0306613448029984f * sqrtRoughness) + 0.0238299731830387f /
                                (0.0272458171384516f + sqrtRoughness3 + nDotV2) -
                                0.0454747751719356f);

    const float scale = saturate(delta - bias);
    return float2(scale, bias);
}

//-------------------------------------------------------------------------------------------------
// Returns an adjusted scale factor for environment specular reflections that represents the
// integral of the geometry/visibility + fresnel terms for a GGX BRDF given a particular
// viewing angle and roughness value. The final value is computed using polynomials that were
// fitted to tabulated data generated via monte carlo integration.
//-------------------------------------------------------------------------------------------------
float3 GGXEnvironmentBRDF(in float3 specAlbedo, in float nDotV, in float sqrtRoughness)
{
    float2 scaleBias = GGXEnvironmentBRDFScaleBias(nDotV, sqrtRoughness);
    return specAlbedo * scaleBias.x + scaleBias.y;
}

//-------------------------------------------------------------------------------------------------
// Calculates the lighting result for an analytical light source
//-------------------------------------------------------------------------------------------------
float3 CalcLighting(in float3 normal, in float3 lightDir, in float3 peakIrradiance,
                    in float3 diffuseAlbedo, in float3 specularAlbedo, in float roughness,
                    in float3 positionWS, in float3 cameraPosWS, in float3 msEnergyCompensation)
{
    float3 lighting = diffuseAlbedo * (1.0f / 3.14159f);

    float3 view = normalize(cameraPosWS - positionWS);
    const float nDotL = saturate(dot(normal, lightDir));
    if(nDotL > 0.0f)
    {
        const float nDotV = saturate(dot(normal, view));
        float3 h = normalize(view + lightDir);

        float3 fresnel = Fresnel(specularAlbedo, h, lightDir);

        float specular = GGXSpecular(roughness, normal, h, view, lightDir);
        lighting += specular * fresnel * msEnergyCompensation;
    }

    return lighting * nDotL * peakIrradiance;
}


float pow5(float v) {
    return (v*v) * (v*v) * v;
};

struct AshikhminShirleyBRDF {
    // Diffuse reflectance.
    float3 Rd;

    // Glossy specular reflectance.
    float3 Rs;

    // Shading normal.
    float3 sn;

    float roughness;

    // Microfacet distribution for the glossy coat.
    TrowbridgeReitzDistribution distribution;

    // Evaluates Schlick's approximation to the Fresnel equations.
    float3 SchlickFresnel(float cosTheta) {
        return Rs + pow5(1 - cosTheta) * (float3(1.f, 1.f, 1.f) - Rs);
    }

    // Evaluates the Ashikhmin-Shirley BRDF for the input pair of incident and reflected
    // directions. 
    float3 f(float3 wo, float3 wi) {
        // The specular microfacet distribution is a function of the half vector.
        float3 wh = wi + wo;
        if (wh.x == 0 && wh.y == 0 && wh.z == 0) {
            return float3(0.f, 0.f, 0.f);
        }
        wh = normalize(wh);

        float3 specularTerm = distribution.D(wh)
            * SchlickFresnel(dot(wi, wh))
            / (4 * abs(dot(wi, wh)) * max(AbsCosTheta(wi), AbsCosTheta(wo)));

        float3 diffuseTerm = (28.f / (23.f * M_PI))
            * Rd
            * (float3(1.f, 1.f, 1.f) - Rs)
            * (1 - pow5(1 - 0.5f * AbsCosTheta(wi))) 
            * (1 - pow5(1 - 0.5f * AbsCosTheta(wo)));

        return diffuseTerm + specularTerm;
    }

    // Samples the Ashikhmin-Shirley BRDF.
    float3 Sample_f(
        float3 wo,
        inout float3 wi,
        float2 u,
        inout float pdf
    ) {
        float2 uRemapped = u;
        if (u.x < 0.5) {
            // Sample the diffuse term.

            // The uniform random sample u = (u_0, u_1) is used by CosineSampleHemisphere,
            // but u_0 < 0.5 at this point because it was used to select the diffuse term
            // (this conditional branch). Remap u_0 to the range [0,1).
            uRemapped.x = min(2*u.x, ONE_MINUS_EPSILON);

            // Diffuse reflection.
            wi = CosineSampleHemisphere(uRemapped);
            if (wo.z < 0.0) {
                // CosineSampleHemisphere sampled wi on an arbitrary hemisphere of 
                // reflection space. Invert wi so that it lies on the same
                // hemisphere as wo.
                wi.z *= -1;
            }
        } else {
            // Sample the glossy specular term.

            // The uniform random sample u = (u_0, u_1) is used by distribution->Sample_wh,
            // but u_0 >= 0.5 at this point because it was used to select the specular term
            // (this conditional branch). Remap u_0 to the range [0,1).
            uRemapped.x = min(2 * (u.x - 0.5f), ONE_MINUS_EPSILON);

            float3 wh = distribution.Sample_wh(wo, uRemapped);
            wi = Reflect(wo, wh);
            if (!SameHemisphere(wo, wi)) {
                return float3(0.f, 0.f, 0.f);
            }
        }

        pdf = Pdf(wo, wi);

        return f(wo, wi);
    }

    // Obtains the probability of sampling wi given wo.
    float Pdf(float3 wo, float3 wi) {
        if (!SameHemisphere(wo, wi)) {
            return 0.f;
        }

        // Half vector.
        float3 wh = normalize(wo + wi);

        float diffusePdf = AbsCosTheta(wi) * M_1_PI;
        float specularPdf = distribution.Pdf(wo, wh) / (4 * dot(wo, wh));

        // Average.
        return 0.5f * (diffusePdf + specularPdf);
    }

};

#endif // BRDF_HLSL_