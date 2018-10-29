//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "..\\PCH.h"
#include "..\\SF12_Math.h"

namespace SampleFramework12
{

// Shape sampling functions
Float2 SquareToConcentricDiskMapping(float x, float y, float numSides, float polygonAmount);
Float2 SquareToConcentricDiskMapping(float x, float y);
Float3 SampleDirectionGGX(const Float3& v, const Float3& n, float roughness, const Float3x3& tangentToWorld, float u1, float u2);
Float3 SampleSphere(float x1, float x2, float x3, float u1);
Float3 SampleDirectionSphere(float u1, float u2);
Float3 SampleDirectionHemisphere(float u1, float u2);
Float3 SampleDirectionCosineHemisphere(float u1, float u2);
Float3 SampleDirectionCone(float u1, float u2, float cosThetaMax);
Float3 SampleDirectionRectangularLight(float u1, float u2, const Float3& sourcePos,
                                       const Float2& lightSize, const Float3& lightPos,
                                       const Quaternion lightOrientation, float& distanceToLight);

// PDF functions
float SampleDirectionGGX_PDF(const Float3& n, const Float3& h, const Float3& v, float roughness);
float SampleDirectionSphere_PDF();
float SampleDirectionHemisphere_PDF();
float SampleDirectionCosineHemisphere_PDF(float cosTheta);
float SampleDirectionCosineHemisphere_PDF(const Float3& normal, const Float3& sampleDir);
float SampleDirectionCone_PDF(float cosThetaMax);
float SampleDirectionRectangularLight_PDF(const Float2& lightSize, const Float3& sampleDir,
                                          const Quaternion lightOrientation, float distanceToLight);

// Random sample generation
Float2 Hammersley2D(uint64 sampleIdx, uint64 numSamples);
Float2 SampleCMJ2D(uint32 sampleIdx, uint32 numSamplesX, uint32 numSamplesY, uint32 pattern);

// Full random sample set generation
void GenerateRandomSamples2D(Float2* samples, uint64 numSamples, Random& randomGenerator);
void GenerateStratifiedSamples2D(Float2* samples, uint64 numSamplesX, uint64 numSamplesY, Random& randomGenerator);
void GenerateGridSamples2D(Float2* samples, uint64 numSamplesX, uint64 numSamplesY);
void GenerateHammersleySamples2D(Float2* samples, uint64 numSamples);
void GenerateHammersleySamples2D(Float2* samples, uint64 numSamples, uint64 dimIdx);
void GenerateLatinHypercubeSamples2D(Float2* samples, uint64 numSamples, Random& rng);
void GenerateCMJSamples2D(Float2* samples, uint64 numSamplesX, uint64 numSamplesY, uint32 pattern);

// Helpers
float RadicalInverseBase2(uint32 bits);
float RadicalInverseFast(uint64 baseIndex, uint64 index);

}