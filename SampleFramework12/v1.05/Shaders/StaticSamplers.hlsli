//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#ifndef STATIC_SAMPLERS_HLSL_
#define STATIC_SAMPLERS_HLSL_

// Standard static samplers for the universal root signature in DX12_Helpers (matches the SamplerState enum)
SamplerState LinearSampler : register(s0);
SamplerState LinearClampSampler : register(s1);
SamplerState LinearBorderSampler : register(s2);
SamplerState PointSampler : register(s3);
SamplerState AnisotropicSampler : register(s4);
SamplerComparisonState ShadowMapSampler : register(s5);
SamplerComparisonState ShadowMapPCFSampler : register(s6);
SamplerComparisonState ReversedShadowMapSampler : register(s7);
SamplerComparisonState ReversedShadowMapPCFSampler : register(s8);

#endif // STATIC_SAMPLERS_HLSL_