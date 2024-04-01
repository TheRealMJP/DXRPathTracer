#ifndef DISTRIBUTIONS_HLSL_
#define DISTRIBUTIONS_HLSL_

#define M_PI Pi

// Converts a spherical coordinate to a rectangular coordinate in a standard coordinate system.
// Theta is measured from the z axis. Phi is measured about the z axis from the x axis.
float3 SphericalDirection(float sinTheta, float cosTheta, float phi) {
    return float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

void TrowbridgeReitzSample11(
    float cosTheta, float U1, float U2, inout float slope_x, inout float slope_y
) {
    if (cosTheta > .9999) {
        float r = sqrt(U1 / (1 - U1));
        float phi = 6.28318530718 * U2;
        slope_x = r * cos(phi);
        slope_y = r * sin(phi);
        return;
    }

    float sinTheta = sqrt(max((float)0, (float)1 - cosTheta * cosTheta));
    float tanTheta = sinTheta / cosTheta;
    float a = 1 / tanTheta;
    float G1 = 2 / (1 + sqrt(1.f + 1.f / (a * a)));

    float A = 2 * U1 / G1 - 1;
    float tmp = 1.f / (A * A - 1.f);
    if (tmp > 1e10) tmp = 1e10;
    float B = tanTheta;
    float D = sqrt(max(float(B * B * tmp * tmp - (A * A - B * B) * tmp), float(0)));
    float slope_x_1 = B * tmp - D;
    float slope_x_2 = B * tmp + D;
    slope_x = (A < 0 || slope_x_2 > 1.f / tanTheta) ? slope_x_1 : slope_x_2;

    float S;
    if (U2 > 0.5f) {
        S = 1.f;
        U2 = 2.f * (U2 - .5f);
    } else {
        S = -1.f;
        U2 = 2.f * (.5f - U2);
    }
    float z =
        (U2 * (U2 * (U2 * 0.27385f - 0.73369f) + 0.46341f)) /
        (U2 * (U2 * (U2 * 0.093073f + 0.309420f) - 1.000000f) + 0.597999f);
    slope_y = S * z * sqrt(1.f + slope_x * slope_x);
}

float3 TrowbridgeReitzSample(
    float3 wi, float alpha_x, float alpha_y, float U1, float U2
) {
    float3 wiStretched = normalize(float3(alpha_x * wi.x, alpha_y * wi.y, wi.z));
    float slope_x, slope_y;
    TrowbridgeReitzSample11(CosTheta(wiStretched), U1, U2, slope_x, slope_y);
    float tmp = CosPhi(wiStretched) * slope_x - SinPhi(wiStretched) * slope_y;
    slope_y = SinPhi(wiStretched) * slope_x + CosPhi(wiStretched) * slope_y;
    slope_x = tmp;
    slope_x = alpha_x * slope_x;
    slope_y = alpha_y * slope_y;
    return normalize(float3(-slope_x, -slope_y, 1.));
}

struct TrowbridgeReitzDistribution {
    bool sampleVisibleArea;

    // Parameters of the distribution. Alpha X and Y control the roughness of the
    // surface. RoughnessToAlpha() maps a roughness value in the typical [0,1] range
    // to alpha X and Y values. 
    float alphaX;
    float alphaY;

    // Trowbridge-Reitz microfacet distribution function. Anisotropic in general (dependent on
    // azimuthal angle phi), isotropic in the case where alphaX = alphaY.
    float D(float3 wh) {
        float tan2Theta = Tan2Theta(wh);
        if (isinf(tan2Theta)) {
            return 0.;
        }
        float cos4Theta = Cos2Theta(wh) * Cos2Theta(wh);
        float e = (Cos2Phi(wh) / (alphaX * alphaX) + Sin2Phi(wh) / (alphaY * alphaY)) * tan2Theta;
        return 1 / (M_PI * alphaX * alphaY * cos4Theta * (1 + e) * (1 + e));
    }

    // Smith's geometric attenuation factor G(wo, wi) gives the fraction of microfacets in a
    // differential area dA that are visible from both directions wo and wi. We assume that
    // visibility is more likely the higher up a given point on a microfacet is.
    float G(float3 wo, float3 wi) {
        return 1 / (1 + Lambda(wo) + Lambda(wi));
    }

    // 0 <= G1(w) <= 1 is Smith's masking-shadowing function that gives the fraction of
    // normalized and projected microfacet area that is visible from the direction w. If
    // we let A+(w) denote the projected area of forward facing microfacets in the direction w
    // A-(w) the projected area of backfacing microfacets, then G1(w) = [A+(w) - A-(w)]/A+(w)
    // is the ratio of visible forward-facing microfacet area to total forward-facing microfacet
    // area. 
    float G1(float3 w) {
        return 1 / (1 + Lambda(w));
    }

    float3 Sample_wh(float3 wo, float2 u) {
        float3 wh;

        if (!sampleVisibleArea) {
            float cosTheta = 0;
            float phi = (2 * M_PI) * u.y;
            if (alphaX == alphaY) {
                float tanTheta2 = alphaX * alphaX * u.x / (1.0f - u.x);
                cosTheta = 1 / sqrt(1 + tanTheta2);
            } else {
                phi = atan(alphaY / alphaX * tan(2 * M_PI * u.y + .5f * M_PI));
                if (u.y > .5f) phi += M_PI;
                float sinPhi = sin(phi), cosPhi = cos(phi);
                float alphaX2 = alphaX * alphaX, alphaY2 = alphaY * alphaY;
                float alpha2 = 1 / (cosPhi * cosPhi / alphaX2 + sinPhi * sinPhi / alphaY2);
                float tanTheta2 = alpha2 * u.x / (1 - u.x);
                cosTheta = 1 / sqrt(1 + tanTheta2);
            }
            float sinTheta = sqrt(max((float)0., (float)1. - cosTheta * cosTheta));
            wh = SphericalDirection(sinTheta, cosTheta, phi);
            if (!SameHemisphere(wo, wh)) wh = -wh;
        } else {
            bool flip = wo.z < 0;
            wh = TrowbridgeReitzSample(flip ? -wo : wo, alphaX, alphaY, u.x, u.y);
            if (flip) wh = -wh;
        }

        return wh;
    }

    float Pdf(float3 wo, float3 wh) {
        if (sampleVisibleArea) {
            return D(wh) * G1(wo) * abs(dot(wo, wh)) / AbsCosTheta(wo);
        } else {
            return D(wh) * AbsCosTheta(wh);
        }
    }

    float Lambda(float3 w) {
        float absTanTheta = abs(TanTheta(w));
        if (isinf(absTanTheta)) return 0.;
        float alpha = sqrt(Cos2Phi(w) * alphaX * alphaX + Sin2Phi(w) * alphaY * alphaY);
        float alpha2Tan2Theta = (alpha * absTanTheta) * (alpha * absTanTheta);
        return (-1 + sqrt(1.f + alpha2Tan2Theta)) / 2;
    }

    // Maps a roughness value in the range [0,1] to a value for one of the Trowbridge-Reitz
    // alpha parameters.
    float RoughnessToAlpha(float roughness) {
        roughness = max(roughness, (float) 1e-3);
        float x = log(roughness);
        return 1.62142f + 0.819955f * x + 0.1734f * x * x + 0.0171201f * x * x * x + 0.000640711f * x * x * x * x;
    }
};

#endif // DISTRIBUTIONS_HLSL_