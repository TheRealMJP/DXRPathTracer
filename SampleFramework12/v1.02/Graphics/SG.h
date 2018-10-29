//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "..\\PCH.h"
#include "..\\SF12_Math.h"

namespace SampleFramework12
{

struct Texture;

// SphericalGaussian(dir) := Amplitude * exp(Sharpness * (dot(Axis, Direction) - 1.0f))
struct SG
{
    Float3 Amplitude;
    Float3 Axis;
    float Sharpness = 1.0f;
};

struct SG9
{
    SG Lobes[9];
};

// Evaluates an SG given a direction on a unit sphere
inline Float3 EvaluateSG(const SG& sg, Float3 dir)
{
    return sg.Amplitude * std::exp(sg.Sharpness * (Float3::Dot(dir, sg.Axis) - 1.0f));
}

// Computes the inner product of two SG's, which is equal to Integrate(SGx(v) * SGy(v) * dv).
inline Float3 SGInnerProduct(const SG& x, const SG& y)
{
    float umLength = Float3::Length(x.Sharpness * x.Axis + y.Sharpness * y.Axis);
    Float3 expo = std::exp(umLength - x.Sharpness - y.Sharpness) * x.Amplitude * y.Amplitude;
    float other = 1.0f - std::exp(-2.0f * umLength);
    return (2.0f * Pi * expo * other) / umLength;
}

// Returns an approximation of the clamped cosine lobe represented as an SG
inline SG CosineLobeSG(Float3 direction)
{
    SG cosineLobe;
    cosineLobe.Axis = direction;
    cosineLobe.Sharpness = 2.133f;
    cosineLobe.Amplitude = 1.17f;

    return cosineLobe;
}

// Computes the approximate integral of an SG over the entire sphere. The error vs. the
// non-approximate version decreases as sharpeness increases.
inline Float3 ApproximateSGIntegral(const SG& sg)
{
    return 2 * Pi * (sg.Amplitude / sg.Sharpness);
}

// Computes the approximate incident irradiance from a single SG lobe containing incoming radiance.
// The irradiance is computed using a fitted approximation polynomial. This approximation
// and its implementation were provided by Stephen Hill.
inline Float3 SGIrradianceFitted(const SG& lightingLobe, const Float3& normal)
{
    const float muDotN = Float3::Dot(lightingLobe.Axis, normal);
    const float lambda = lightingLobe.Sharpness;

    const float c0 = 0.36f;
    const float c1 = 1.0f / (4.0f * c0);

    float eml  = std::exp(-lambda);
    float em2l = eml * eml;
    float rl   = 1.0f / lambda;

    float scale = 1.0f + 2.0f * em2l - rl;
    float bias  = (eml - em2l) * rl - em2l;

    float x  = std::sqrt(1.0f - scale);
    float x0 = c0 * muDotN;
    float x1 = c1 * x;

    float n = x0 + x1;

    float y = (std::abs(x0) <= x1) ? n * n / x : Saturate(muDotN);

    float normalizedIrradiance = scale * y + bias;

    return normalizedIrradiance * ApproximateSGIntegral(lightingLobe);
}

enum class SGSolveMode : uint32
{
    NNLS,
    SVD,
    Projection
};

enum class SGDistribution : uint32
{
    Spherical,
    Hemispherical,
};

// Input parameters for the solve
struct SGSolveParams
{
    Float3* SampleDirs = nullptr;
    Float3* SampleValues = nullptr;
    uint64 NumSamples = 0;

    SGSolveMode SolveMode = SGSolveMode::NNLS;
    SGDistribution Distribution = SGDistribution::Spherical;

    uint64 NumSGs = 0;                              // number of SG's we want to solve for
    SG* OutSGs = nullptr;                           // output of final SG's we solve for
};

void GenerateUniformSGs(SG* outSGs, uint64 numSGs, SGDistribution distribution);

// Solve for k-number of SG's based on a sphere or hemisphere of samples
void SolveSGs(SGSolveParams& params);

// Projects a sample onto a set of SG's
void ProjectOntoSGs(const Float3& dir, const Float3& color, SG* outSGs, uint64 numSGs);

void SolveSGsForCubemap(const Texture& texture, SG* outSGs, uint64 numSGs, SGSolveMode solveMode = SGSolveMode::NNLS);

}