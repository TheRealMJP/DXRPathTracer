//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"
#include "Sampling.h"

namespace SampleFramework12
{

#define RadicalInverse_(base) \
    { \
        const float radical = 1.0f / float(base); \
        uint64 value = 0; \
        float factor = 1.0f; \
        while(sampleIdx) { \
            uint64 next  = sampleIdx / base; \
            uint64 digit = sampleIdx - next * base; \
            value = value * base + digit; \
            factor *= radical; \
            sampleIdx = next; \
        } \
        inverse = float(value) * factor; \
    }

static const float OneMinusEpsilon = 0.9999999403953552f;

float RadicalInverseFast(uint64 baseIdx, uint64 sampleIdx)
{
    Assert_(baseIdx < 64);

    float inverse = 0.0f;

    switch (baseIdx)
    {
        case 0: RadicalInverse_(2); break;
        case 1: RadicalInverse_(3); break;
        case 2: RadicalInverse_(5); break;
        case 3: RadicalInverse_(7); break;
        case 4: RadicalInverse_(11); break;
        case 5: RadicalInverse_(13); break;
        case 6: RadicalInverse_(17); break;
        case 7: RadicalInverse_(19); break;
        case 8: RadicalInverse_(23); break;
        case 9: RadicalInverse_(29); break;
        case 10: RadicalInverse_(31); break;
        case 11: RadicalInverse_(37); break;
        case 12: RadicalInverse_(41); break;
        case 13: RadicalInverse_(43); break;
        case 14: RadicalInverse_(47); break;
        case 15: RadicalInverse_(53); break;
        case 16: RadicalInverse_(59); break;
        case 17: RadicalInverse_(61); break;
        case 18: RadicalInverse_(67); break;
        case 19: RadicalInverse_(71); break;
        case 20: RadicalInverse_(73); break;
        case 21: RadicalInverse_(79); break;
        case 22: RadicalInverse_(83); break;
        case 23: RadicalInverse_(89); break;
        case 24: RadicalInverse_(97); break;
        case 25: RadicalInverse_(101); break;
        case 26: RadicalInverse_(103); break;
        case 27: RadicalInverse_(107); break;
        case 28: RadicalInverse_(109); break;
        case 29: RadicalInverse_(113); break;
        case 30: RadicalInverse_(127); break;
        case 31: RadicalInverse_(131); break;
        case 32: RadicalInverse_(137); break;
        case 33: RadicalInverse_(139); break;
        case 34: RadicalInverse_(149); break;
        case 35: RadicalInverse_(151); break;
        case 36: RadicalInverse_(157); break;
        case 37: RadicalInverse_(163); break;
        case 38: RadicalInverse_(167); break;
        case 39: RadicalInverse_(173); break;
        case 40: RadicalInverse_(179); break;
        case 41: RadicalInverse_(181); break;
        case 42: RadicalInverse_(191); break;
        case 43: RadicalInverse_(193); break;
        case 44: RadicalInverse_(197); break;
        case 45: RadicalInverse_(199); break;
        case 46: RadicalInverse_(211); break;
        case 47: RadicalInverse_(223); break;
        case 48: RadicalInverse_(227); break;
        case 49: RadicalInverse_(229); break;
        case 50: RadicalInverse_(233); break;
        case 51: RadicalInverse_(239); break;
        case 52: RadicalInverse_(241); break;
        case 53: RadicalInverse_(251); break;
        case 54: RadicalInverse_(257); break;
        case 55: RadicalInverse_(263); break;
        case 56: RadicalInverse_(269); break;
        case 57: RadicalInverse_(271); break;
        case 58: RadicalInverse_(277); break;
        case 59: RadicalInverse_(281); break;
        case 60: RadicalInverse_(283); break;
        case 61: RadicalInverse_(293); break;
        case 62: RadicalInverse_(307); break;
        case 63: RadicalInverse_(311); break;
    }
    return std::min(inverse, OneMinusEpsilon);
}

// Maps a value inside the square [0,1]x[0,1] to a value in a disk of radius 1 using concentric squares.
// This mapping preserves area, bi continuity, and minimizes deformation.
// Based off the algorithm "A Low Distortion Map Between Disk and Square" by Peter Shirley and
// Kenneth Chiu. Also includes polygon morphing modification from "CryEngine3 Graphics Gems"
// by Tiago Sousa
Float2 SquareToConcentricDiskMapping(float x, float y, float numSides, float polygonAmount)
{
    float phi, r;

    // -- (a,b) is now on [-1,1]ˆ2
    float a = 2.0f * x - 1.0f;
    float b = 2.0f * y - 1.0f;

    if(a > -b)                      // region 1 or 2
    {
        if(a > b)                   // region 1, also |a| > |b|
        {
            r = a;
            phi = (Pi / 4.0f) * (b / a);
        }
        else                        // region 2, also |b| > |a|
        {
            r = b;
            phi = (Pi / 4.0f) * (2.0f - (a / b));
        }
    }
    else                            // region 3 or 4
    {
        if(a < b)                   // region 3, also |a| >= |b|, a != 0
        {
            r = -a;
            phi = (Pi / 4.0f) * (4.0f + (b / a));
        }
        else                        // region 4, |b| >= |a|, but a==0 and b==0 could occur.
        {
            r = -b;
            if(b != 0)
                phi = (Pi / 4.0f) * (6.0f - (a / b));
            else
                phi = 0;
        }
    }

    const float N = numSides;
    float polyModifier = std::cos(Pi / N) / std::cos(phi - (Pi2 / N) * std::floor((N * phi + Pi) / Pi2));
    r *= Lerp(1.0f, polyModifier, polygonAmount);

    Float2 result;
    result.x = r * std::cos(phi);
    result.y = r * std::sin(phi);

    return result;
}

// Maps a value inside the square [0,1]x[0,1] to a value in a disk of radius 1 using concentric squares.
// This mapping preserves area, bi continuity, and minimizes deformation.
// Based off the algorithm "A Low Distortion Map Between Disk and Square" by Peter Shirley and
// Kenneth Chiu.
Float2 SquareToConcentricDiskMapping(float x, float y)
{
    float phi = 0.0f;
    float r = 0.0f;

    // -- (a,b) is now on [-1,1]ˆ2
    float a = 2.0f * x - 1.0f;
    float b = 2.0f * y - 1.0f;

    if(a > -b)                      // region 1 or 2
    {
        if(a > b)                   // region 1, also |a| > |b|
        {
            r = a;
            phi = (Pi / 4.0f) * (b / a);
        }
        else                        // region 2, also |b| > |a|
        {
            r = b;
            phi = (Pi / 4.0f) * (2.0f - (a / b));
        }
    }
    else                            // region 3 or 4
    {
        if(a < b)                   // region 3, also |a| >= |b|, a != 0
        {
            r = -a;
            phi = (Pi / 4.0f) * (4.0f + (b / a));
        }
        else                        // region 4, |b| >= |a|, but a==0 and b==0 could occur.
        {
            r = -b;
            if(b != 0)
                phi = (Pi / 4.0f) * (6.0f - (a / b));
            else
                phi = 0;
        }
    }

    Float2 result;
    result.x = r * std::cos(phi);
    result.y = r * std::sin(phi);
    return result;
}

// Returns a random direction for sampling a GGX distribution.
// Does everything in world space.
Float3 SampleDirectionGGX(const Float3& v, const Float3& n, float roughness, const Float3x3& tangentToWorld, float u1, float u2)
{
    float theta = std::atan2(roughness * std::sqrt(u1), std::sqrt(1 - u1));
    float phi = 2 * Pi * u2;

    Float3 h;
    h.x = std::sin(theta) * std::cos(phi);
    h.y = std::sin(theta) * std::sin(phi);
    h.z = std::cos(theta);

    h = Float3::Normalize(Float3::Transform(h, tangentToWorld));

    float hDotV = std::abs(Float3::Dot(h, v));
    Float3 sampleDir = 2.0f * hDotV * h - v;
    return Float3::Normalize(sampleDir);
}

// Returns a point inside of a unit sphere
Float3 SampleSphere(float x1, float x2, float x3, float u1)
{
    Float3 xyz = Float3(x1, x2, x3) * 2.0f - 1.0f;
    float scale = std::pow(u1, 1.0f / 3.0f) / Float3::Length(xyz);
    return xyz * scale;
}

// Returns a random direction on the unit sphere
Float3 SampleDirectionSphere(float u1, float u2)
{
    float z = u1 * 2.0f - 1.0f;
    float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    float phi = 2 * Pi * u2;
    float x = r * std::cos(phi);
    float y = r * std::sin(phi);

    return Float3(x, y, z);
}

// Returns a random direction on the hemisphere around z = 1
Float3 SampleDirectionHemisphere(float u1, float u2)
{
    float z = u1;
    float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    float phi = 2 * Pi * u2;
    float x = r * std::cos(phi);
    float y = r * std::sin(phi);

    return Float3(x, y, z);
}

// Returns a random cosine-weighted direction on the hemisphere around z = 1
Float3 SampleDirectionCosineHemisphere(float u1, float u2)
{
    Float2 uv = SquareToConcentricDiskMapping(u1, u2);
    float u = uv.x;
    float v = uv.y;

    // Project samples on the disk to the hemisphere to get a
    // cosine weighted distribution
    Float3 dir;
    float r = u * u + v * v;
    dir.x = u;
    dir.y = v;
    dir.z = std::sqrt(std::max(0.0f, 1.0f - r));

    return dir;
}

// Returns a random direction from within a cone with angle == theta
Float3 SampleDirectionCone(float u1, float u2, float cosThetaMax)
{
    float cosTheta = (1.0f - u1) + u1 * cosThetaMax;
    float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);
    float phi = u2 * 2.0f * Pi;
    return Float3(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta);
}

// Returns a direction that samples a rectangular area light
Float3 SampleDirectionRectangularLight(float u1, float u2, const Float3& sourcePos,
                                       const Float2& lightSize, const Float3& lightPos,
                                       const Quaternion lightOrientation, float& distanceToLight)
{
    float x = u1 - 0.5f;
    float y = u2 - 0.5f;

    Float3x3 lightBasis = lightOrientation.ToFloat3x3();
    Float3 lightBasisX = lightBasis.Right();
    Float3 lightBasisY = lightBasis.Up();
    Float3 lightBasisZ = lightBasis.Forward();

    // Pick random sample point
    Float3 samplePos = lightPos +
                       lightBasisX * x * lightSize.x +
                       lightBasisY * y * lightSize.y;

    Float3 sampleDir = samplePos - sourcePos;
    distanceToLight = Float3::Length(sampleDir);
    if(distanceToLight > 0.0f)
        sampleDir /= distanceToLight;

    return sampleDir;
}

// Returns the PDF for a particular GGX sample
float SampleDirectionGGX_PDF(const Float3& n, const Float3& h, const Float3& v, float roughness)
{
    float nDotH = Saturate(Float3::Dot(n, h));
    float hDotV = Saturate(Float3::Dot(h, v));
    float m2 = roughness * roughness;
    float d = m2 / (Pi * Square(nDotH * nDotH * (m2 - 1) + 1));
    float pM = d * nDotH;
    return pM / (4 * hDotV);
}

// Returns the (constant) PDF of sampling uniform directions on the unit sphere
float SampleDirectionSphere_PDF()
{
    return 1.0f / (Pi * 4.0f);
}

// Returns the (constant) PDF of sampling uniform directions on a unit hemisphere
float SampleDirectionHemisphere_PDF()
{
    return 1.0f / (Pi * 2.0f);
}

// Returns the PDF of of a single sample on a cosine-weighted hemisphere
float SampleDirectionCosineHemisphere_PDF(float cosTheta)
{
    return cosTheta / Pi;
}

// Returns the PDF of of a single sample on a cosine-weighted hemisphere
float SampleDirectionCosineHemisphere_PDF(const Float3& normal, const Float3& sampleDir)
{
    return Saturate(Float3::Dot(normal, sampleDir)) / Pi;
}

// Returns the PDF of of a single uniform sample within a cone
float SampleDirectionCone_PDF(float cosThetaMax)
{
    return 1.0f / (2.0f * Pi * (1.0f - cosThetaMax));
}

// Returns the PDF of of a single sample on a rectangular area light
float SampleDirectionRectangularLight_PDF(const Float2& lightSize, const Float3& sampleDir,
                                          const Quaternion lightOrientation, float distanceToLight)
{
    Float3 lightBasisZ = Float3::Transform(Float3(0.0f, 0.0f, -1.0f), lightOrientation);
    float areaNDotL = Saturate(Float3::Dot(sampleDir, lightBasisZ));
    return (distanceToLight * distanceToLight) / (areaNDotL * lightSize.x * lightSize.y);
}

// Computes a radical inverse with base 2 using crazy bit-twiddling from "Hacker's Delight"
float RadicalInverseBase2(uint32 bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

// Returns a single 2D point in a Hammersley sequence of length "numSamples", using base 1 and base 2
Float2 Hammersley2D(uint64 sampleIdx, uint64 numSamples)
{
    return Float2(float(sampleIdx) / float(numSamples), RadicalInverseBase2(uint32(sampleIdx)));
}

static uint32 CMJPermute(uint32 i, uint32 l, uint32 p)
{
    uint32 w = l - 1;
    w |= w >> 1;
    w |= w >> 2;
    w |= w >> 4;
    w |= w >> 8;
    w |= w >> 16;
    do
    {
        i ^= p; i *= 0xe170893d;
        i ^= p >> 16;
        i ^= (i & w) >> 4;
        i ^= p >> 8; i *= 0x0929eb3f;
        i ^= p >> 23;
        i ^= (i & w) >> 1; i *= 1 | p >> 27;
        i *= 0x6935fa69;
        i ^= (i & w) >> 11; i *= 0x74dcb303;
        i ^= (i & w) >> 2; i *= 0x9e501cc3;
        i ^= (i & w) >> 2; i *= 0xc860a3df;
        i &= w;
        i ^= i >> 5;
    }
    while (i >= l);
    return (i + p) % l;
}

static float CMJRandFloat(uint32 i, uint32 p)
{
    i ^= p;
    i ^= i >> 17;
    i ^= i >> 10; i *= 0xb36534e5;
    i ^= i >> 12;
    i ^= i >> 21; i *= 0x93fc4795;
    i ^= 0xdf6e307f;
    i ^= i >> 17; i *= 1 | p >> 18;
    return i * (1.0f / 4294967808.0f);
}

 // Returns a 2D sample from a particular pattern using correlated multi-jittered sampling [Kensler 2013]
Float2 SampleCMJ2D(uint32 sampleIdx, uint32 numSamplesX, uint32 numSamplesY, uint32 pattern)
{
    uint32 N = numSamplesX * numSamplesY;
    sampleIdx = CMJPermute(sampleIdx, N, pattern * 0x51633e2d);
    uint32 sx = CMJPermute(sampleIdx % numSamplesX, numSamplesX, pattern * 0x68bc21eb);
    uint32 sy = CMJPermute(sampleIdx / numSamplesX, numSamplesY, pattern * 0x02e5be93);
    float jx = CMJRandFloat(sampleIdx, pattern * 0x967a889b);
    float jy = CMJRandFloat(sampleIdx, pattern * 0x368cc8b7);
    return Float2((sx + (sy + jx) / numSamplesY) / numSamplesX, (sampleIdx + jy) / N);
}

void GenerateRandomSamples2D(Float2* samples, uint64 numSamples, Random& randomGenerator)
{
    for(uint64 i = 0; i < numSamples; ++i)
        samples[i] = randomGenerator.RandomFloat2();
}

void GenerateStratifiedSamples2D(Float2* samples, uint64 numSamplesX, uint64 numSamplesY, Random& randomGenerator)
{
    const Float2 delta = Float2(1.0f / numSamplesX, 1.0f / numSamplesY);
    uint64 sampleIdx = 0;
    for(uint64 y = 0; y < numSamplesY; ++y)
    {
        for(uint64 x = 0; x < numSamplesX; ++x)
        {
            Float2& currSample = samples[sampleIdx];
            currSample = Float2(float(x), float(y)) + randomGenerator.RandomFloat2();
            currSample *= delta;
            currSample = Float2::Clamp(currSample, 0.0f, OneMinusEpsilon);

            ++sampleIdx;
        }
    }
}

void GenerateGridSamples2D(Float2* samples, uint64 numSamplesX, uint64 numSamplesY)
{
    const Float2 delta = Float2(1.0f / numSamplesX, 1.0f / numSamplesY);
    uint64 sampleIdx = 0;
    for(uint64 y = 0; y < numSamplesY; ++y)
    {
        for(uint64 x = 0; x < numSamplesX; ++x)
        {
            Float2& currSample = samples[sampleIdx];
            currSample = Float2(float(x), float(y));
            currSample *= delta;

            ++sampleIdx;
        }
    }
}

// Generates hammersley using base 1 and 2
void GenerateHammersleySamples2D(Float2* samples, uint64 numSamples)
{
    for(uint64 i = 0; i < numSamples; ++i)
        samples[i] = Hammersley2D(i, numSamples);
}

// Generates hammersley using arbitrary bases
void GenerateHammersleySamples2D(Float2* samples, uint64 numSamples, uint64 dimIdx)
{
    if(dimIdx == 0)
    {
        GenerateHammersleySamples2D(samples, numSamples);
    }
    else
    {
        uint64 baseIdx0 = dimIdx * 2 - 1;
        uint64 baseIdx1 = baseIdx0 + 1;
        for(uint64 i = 0; i < numSamples; ++i)
            samples[i] = Float2(RadicalInverseFast(baseIdx0, i), RadicalInverseFast(baseIdx1, i));
    }
}

void GenerateLatinHypercubeSamples2D(Float2* samples, uint64 numSamples, Random& rng)
{
    // Generate LHS samples along diagonal
    const Float2 delta = Float2(1.0f / numSamples, 1.0f / numSamples);
    for(uint64 i = 0; i < numSamples; ++i)
    {
        Float2 currSample = Float2(float(i)) + rng.RandomFloat2();
        currSample *= delta;
        samples[i] =  Float2::Clamp(currSample, 0.0f, OneMinusEpsilon);
    }

    // Permute LHS samples in each dimension
    float* samples1D = reinterpret_cast<float*>(samples);
    const uint64 numDims = 2;
    for(uint64 i = 0; i < numDims; ++i)
    {
        for(uint64 j = 0; j < numSamples; ++j)
        {
            uint64 other = j + (rng.RandomUint() % (numSamples - j));
            Swap(samples1D[numDims * j + i], samples1D[numDims * other + i]);
        }
    }
}

void GenerateCMJSamples2D(Float2* samples, uint64 numSamplesX, uint64 numSamplesY, uint32 pattern)
{
    const uint64 numSamples = numSamplesX * numSamplesY;
    for(uint64 i = 0; i < numSamples; ++i)
        samples[i] = SampleCMJ2D(int32(i), int32(numSamplesX), int32(numSamplesY), int32(pattern));
}

}