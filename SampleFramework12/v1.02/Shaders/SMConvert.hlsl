//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

//=================================================================================================
// Includes
//=================================================================================================
#include <EVSM.hlsl>
#include <MSM.hlsl>
#include <DescriptorTables.hlsl>

//=================================================================================================
// Constants
//=================================================================================================
#ifndef MSAASamples_
    #define MSAASamples_ 1
#endif

#ifndef SampleRadius_
    #define SampleRadius_ 0
#endif

//=================================================================================================
// Resources
//=================================================================================================
struct ConvertConstants
{
    float2 ShadowMapSize;
    float PositiveExponent;
    float NegativeExponent;
    float FilterSizeU;
    float FilterSizeV;
    bool LinearizeDepth;
    float NearClip;
    float InvClipRange;
    float Proj33;
    float Proj43;
    uint InputMapIdx;
    uint ArraySliceIdx;
};

ConstantBuffer<ConvertConstants> CBuffer : register(b0);

SamplerState LinearSampler : register (s0);

float4 SMConvert(in float4 Position : SV_Position) : SV_Target0
{
    float sampleWeight = 1.0f / float(MSAASamples_);
    uint2 coords = uint2(Position.xy);

    float2 exponents = GetEVSMExponents(CBuffer.PositiveExponent, CBuffer.NegativeExponent, 1.0f);

    float4 average = float4(0.0f, 0.0f, 0.0f, 0.0f);

    // Sample indices to Load() must be literal, so force unroll
    [unroll]
    for(uint i = 0; i < MSAASamples_; ++i)
    {
        // Convert to EVSM representation
        #if MSAASamples_ > 1
            Texture2DMS<float4> shadowMap = Tex2DMSTable[CBuffer.InputMapIdx];
            float depth = shadowMap.Load(coords, i).x;
        #else
            Texture2D<float4> shadowMap = Tex2DTable[CBuffer.InputMapIdx];
            float depth = shadowMap[coords].x;
        #endif

        if(CBuffer.LinearizeDepth)
        {
            depth = CBuffer.Proj43 / (depth - CBuffer.Proj33);
            depth = (depth - CBuffer.NearClip) * CBuffer.InvClipRange;
        }

        #if MSM_
            float4 msmDepth = GetOptimizedMoments(depth);
            average += sampleWeight * msmDepth;
        #elif EVSM_
            float2 vsmDepth = WarpDepth(depth, exponents);
            average += sampleWeight * float4(vsmDepth.xy, vsmDepth.xy * vsmDepth.xy);
        #endif
    }

    return average;
}

float4 FilterSample(in float2 texelPos, in float offset, in float2 mapSize)
{
    float2 samplePos = texelPos;

    #if Vertical_
        samplePos.y = clamp(texelPos.y + offset, 0, mapSize.y - 1.0f);
        Texture2D<float4> shadowMap = Tex2DTable[CBuffer.InputMapIdx];
        return shadowMap[uint2(samplePos)];
    #else
        samplePos.x = clamp(texelPos.x + offset, 0, mapSize.x - 1.0f);
        Texture2DArray<float4> shadowMap = Tex2DArrayTable[CBuffer.InputMapIdx];
        return shadowMap[uint3(samplePos, CBuffer.ArraySliceIdx)];
    #endif
}

float4 FilterSM(in float4 Position : SV_Position) : SV_Target0
{
    #if Vertical_
        const float filterSize = CBuffer.FilterSizeU;
        const float texelSize = rcp(CBuffer.ShadowMapSize.y);
        Texture2D<float4> shadowMap = Tex2DTable[CBuffer.InputMapIdx];
    #else
        const float filterSize = CBuffer.FilterSizeV;
        const float texelSize = rcp(CBuffer.ShadowMapSize.x);
        Texture2DArray<float4> shadowMap = Tex2DArrayTable[CBuffer.InputMapIdx];
    #endif

    const float Radius = filterSize / 2.0f;

    float4 sum = 0.0f;

    #if 1 && SampleRadius_ > 0
        const float2 uv = Position.xy * rcp(CBuffer.ShadowMapSize);
        float edgeFraction = Radius - (SampleRadius_ - 0.5f);

        float weightSum = 0.0f;

        [unroll]
        for(float x = -SampleRadius_ + 0.5f; x <= SampleRadius_ + 0.5f; x += 2.0f)
        {
            float offset = x;
            float sampleWeight = 2.0f;
            if(x == -SampleRadius_ + 0.5f)
            {
                offset += (1.0f - edgeFraction) * 0.5f;
                sampleWeight = 1.0f + edgeFraction;
            }
            else if(x == SampleRadius_ + 0.5f)
            {
                offset -= 0.5f;
                sampleWeight = edgeFraction;
            }

            #if Vertical_
                const float2 sampleUV = uv + float2(0.0f, offset) * texelSize;
                sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * sampleWeight;
            #else
                const float2 sampleUV = uv + float2(offset, 0.0f) * texelSize;
                sum += shadowMap.SampleLevel(LinearSampler, float3(sampleUV, CBuffer.ArraySliceIdx), 0.0f) * sampleWeight;
            #endif

            weightSum += sampleWeight;
        }

        return sum / filterSize;
    #else
        [unroll]
        for(int i = -SampleRadius_; i <= SampleRadius_; ++i)
        {
            float4 smSample = FilterSample(Position.xy, i, CBuffer.ShadowMapSize);

            smSample *= saturate((Radius + 0.5f) - abs(i));

            sum += smSample;
        }

        return sum / filterSize;
    #endif
}

float4 FilterSM3x3(in float4 Position : SV_Position) : SV_Target0
{
    const float2 texelSize = rcp(CBuffer.ShadowMapSize);

    const float2 uv = Position.xy * texelSize;

    const float2 radius = float2(CBuffer.FilterSizeU, CBuffer.FilterSizeV) / 2.0f;
    float2 edgeFraction = radius - 0.5f;

    Texture2D<float4> shadowMap = Tex2DTable[CBuffer.InputMapIdx];

    float4 sum = 0.0f;

    // +---+---+---+
    // |       |   |
    // |       |   |
    // |       |   |
    // +   0   + 1 +
    // |       |   |
    // |       |   |
    // |       |   |
    // +---+---+---+
    // |       |   |
    // |   2   | 3 |
    // |       |   |
    // +---+---+---+

    {
        // Top left 2x2
        float2 offset = float2(-0.5f, -0.5f) + ((1.0f - edgeFraction) * float2(0.5f, 0.5f));
        float2 weights = 1.0f + edgeFraction;

        const float2 sampleUV = uv + offset * texelSize;
        sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
    }

    {
        // Top right 1x2
        float2 offset = float2(1.0f, -0.5f) + float2(0.0f, (1.0f - edgeFraction.y) * 0.5f);
        float2 weights = float2(0.0f, 1.0f) + edgeFraction;

        const float2 sampleUV = uv + offset * texelSize;
        sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
    }

    {
        // Bottom left 2x1
        float2 offset = float2(-0.5f, 1.0f) + float2((1.0f - edgeFraction.x) * 0.5f, 0.0f);
        float2 weights = float2(1.0f, 0.0f) + edgeFraction;

        const float2 sampleUV = uv + offset * texelSize;
        sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
    }

    {
        // Bottom right 1x1
        float2 offset = float2(1.0f, 1.0f);
        float2 weights = edgeFraction;

        const float2 sampleUV = uv + offset * texelSize;
        sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
    }

    return sum / (CBuffer.FilterSizeU * CBuffer.FilterSizeV);
}

float4 FilterSM5x5(in float4 Position : SV_Position) : SV_Target0
{
    const float2 texelSize = rcp(CBuffer.ShadowMapSize);

    const float2 uv = Position.xy * texelSize;

    const float2 radius = float2(CBuffer.FilterSizeU, CBuffer.FilterSizeV) / 2.0f;
    float2 edgeFraction = radius - 1.5f;

    Texture2D<float4> shadowMap = Tex2DTable[CBuffer.InputMapIdx];

    float4 sum = 0.0f;

    // +---+---+---+---+---+
    // |       |       |   |
    // |       |       |   |
    // |       |       |   |
    // +   0   +   1   + 2 +
    // |       |       |   |
    // |       |       |   |
    // |       |       |   |
    // +---+---+---+---+---+
    // |       |       |   |
    // |       |       |   |
    // |       |       |   |
    // +   3   +   4   + 5 +
    // |       |       |   |
    // |       |       |   |
    // |       |       |   |
    // +---+---+---+---+---+
    // |       |       |   |
    // |   6   |   7   | 8 |
    // |       |       |   |
    // +---+---+---+---+---+

    {
        // Top left 2x2
        float2 offset = float2(-1.5f, -1.5f) + ((1.0f - edgeFraction) * float2(0.5f, 0.5f));
        float2 weights = 1.0f + edgeFraction;

        const float2 sampleUV = uv + offset * texelSize;
        sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
    }

    {
        // Middle top 2x2
        float2 offset = float2(0.5f, -1.5f) + float2(0.0f, (1.0f - edgeFraction.y) * 0.5f);
        float2 weights = float2(2.0f, 1.0f + edgeFraction.y);

        const float2 sampleUV = uv + offset * texelSize;
        sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
    }

    {
        // Top right 1x2
        float2 offset = float2(2.0f, -1.5f) + float2(0.0f, (1.0f - edgeFraction.y) * 0.5f);
        float2 weights = float2(0.0f, 1.0f) + edgeFraction;

        const float2 sampleUV = uv + offset * texelSize;
        sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
    }

    {
        // Left middle 2x2
        float2 offset = float2(-1.5f, 0.5f) + float2((1.0f - edgeFraction.x) * 0.5f, 0.0f);
        float2 weights = float2(1.0f + edgeFraction.x, 2.0f);

        const float2 sampleUV = uv + offset * texelSize;
        sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
    }

    {
        // Middle 2x2
        float2 offset = float2(0.5f, 0.5f);
        float2 weights = float2(2.0f, 2.0f);

        const float2 sampleUV = uv + offset * texelSize;
        sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
    }

    {
        // Middle right 1x2
        float2 offset = float2(2.0f, 0.5f);
        float2 weights = float2(edgeFraction.x, 2.0f);

        const float2 sampleUV = uv + offset * texelSize;
        sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
    }

    {
        // Bottom left 2x1
        float2 offset = float2(-1.5f, 2.0f) + float2((1.0f - edgeFraction.x) * 0.5f, 0.0f);
        float2 weights = float2(1.0f, 0.0f) + edgeFraction;

        const float2 sampleUV = uv + offset * texelSize;
        sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
    }

    {
        // Bottom middle 2x1
        float2 offset = float2(0.5f, 2.0f);
        float2 weights = float2(2.0f, edgeFraction.y);

        const float2 sampleUV = uv + offset * texelSize;
        sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
    }

    {
        // Bottom right 1x1
        float2 offset = float2(2.0f, 2.0f);
        float2 weights = edgeFraction;

        const float2 sampleUV = uv + offset * texelSize;
        sum += shadowMap.SampleLevel(LinearSampler, sampleUV, 0.0f) * weights.x * weights.y;
    }

    return sum / (CBuffer.FilterSizeU * CBuffer.FilterSizeV);
}

#if CS_

static const uint BaseThreadGroupWidth = 8;
static const uint GuardBandSize = SampleRadius_;
static const uint ThreadGroupWidth = BaseThreadGroupWidth + GuardBandSize * 2;

#define UsePackedSharedMem_ 0

#if SampleRadius_ > 0
    #if UsePackedSharedMem_
        groupshared uint2 SharedMem[ThreadGroupWidth][ThreadGroupWidth];
    #else
        groupshared float4 SharedMem[ThreadGroupWidth][ThreadGroupWidth];
    #endif
#endif

#if UsePackedSharedMem_

uint2 PackToSharedMem(in float4 unpacked)
{
    uint2 packed = 0;
    packed.x = uint(unpacked.x * float(0xFFFF));
    packed.x |= uint(unpacked.y * float(0xFFFF)) << 16;
    packed.y = uint(unpacked.z * float(0xFFFF));
    packed.y |= uint(unpacked.w * float(0xFFFF)) << 16;

    return packed;
}

float4 UnpackFromSharedMem(in uint2 packed)
{
    float4 unpacked;
    unpacked.x = (packed.x & 0xFFFF) / float(0xFFFF);
    unpacked.y = ((packed.x >> 16) & 0xFFFF) / float(0xFFFF);
    unpacked.z = (packed.y & 0xFFFF) / float(0xFFFF);
    unpacked.w = ((packed.y >> 16) & 0xFFFF) / float(0xFFFF);

    return unpacked;
}

#else

float4 PackToSharedMem(in float4 unpacked) { return unpacked; }
float4 UnpackFromSharedMem(in float4 packed) { return packed; }

#endif


RWTexture2DArray<unorm float4> OutputTexture : register(u0);

[numthreads(ThreadGroupWidth, ThreadGroupWidth, 1)]
void SMConvertAndFilter(in uint3 GroupID : SV_GroupID, in uint3 GroupThreadID : SV_GroupThreadID, in uint GroupIndex : SV_GroupIndex)
{
    const float2 MaxTexelIdx = CBuffer.ShadowMapSize - 1.0f;
    const float2 UnclampedTexelIdx = (GroupID.xy * BaseThreadGroupWidth + GroupThreadID.xy) - float(GuardBandSize);
    const float2 TexelIdx = clamp(UnclampedTexelIdx, 0.0f, MaxTexelIdx);

    {
        float sampleWeight = 1.0f / float(MSAASamples_);

        float2 exponents = GetEVSMExponents(CBuffer.PositiveExponent, CBuffer.NegativeExponent, 1.0f);

        float4 converted = float4(0.0f, 0.0f, 0.0f, 0.0f);

        // Sample indices to Load() must be literal, so force unroll
        [unroll]
        for(uint i = 0; i < MSAASamples_; ++i)
        {
            // Convert to EVSM representation
            #if MSAASamples_ > 1
                Texture2DMS<float4> shadowMap = Tex2DMSTable[CBuffer.InputMapIdx];
                float depth = shadowMap.Load(TexelIdx, i).x;
            #else
                Texture2D<float4> shadowMap = Tex2DTable[CBuffer.InputMapIdx];
                float depth = shadowMap[TexelIdx].x;
            #endif

            if(CBuffer.LinearizeDepth)
            {
                depth = CBuffer.Proj43 / (depth - CBuffer.Proj33);
                depth = (depth - CBuffer.NearClip) * CBuffer.InvClipRange;
            }

            #if MSM_
                float4 msmDepth = GetOptimizedMoments(depth);
                converted += sampleWeight * msmDepth;
            #elif EVSM_
                float2 vsmDepth = WarpDepth(depth, exponents);
                converted += sampleWeight * float4(vsmDepth.xy, vsmDepth.xy * vsmDepth.xy);
            #endif
        }

        #if SampleRadius_ > 0
            SharedMem[GroupThreadID.y][GroupThreadID.x] = PackToSharedMem(converted);
        #else
            OutputTexture[uint3(GroupID.xy * ThreadGroupWidth + GroupThreadID.xy, CBuffer.ArraySliceIdx)] = converted;
        #endif
    }

    #if SampleRadius_ > 0
        GroupMemoryBarrierWithGroupSync();

        {
            const float radiusU = CBuffer.FilterSizeU / 2.0f;

            float4 filteredU = 0.0f;

            [unroll]
            for(int i = -SampleRadius_; i <= SampleRadius_; ++i)
            {
                float4 smSample = UnpackFromSharedMem(SharedMem[GroupThreadID.y][int(GroupThreadID.x) + i]);

                smSample *= saturate((radiusU + 0.5f) - abs(i));

                filteredU += smSample;
            }

            filteredU /= CBuffer.FilterSizeU;

            GroupMemoryBarrierWithGroupSync();

            SharedMem[GroupThreadID.y][GroupThreadID.x] = PackToSharedMem(filteredU);
        }

        GroupMemoryBarrierWithGroupSync();

        if(GroupIndex < BaseThreadGroupWidth * BaseThreadGroupWidth)
        {
            const uint2 localIdx = uint2(GroupIndex / BaseThreadGroupWidth, GroupIndex % BaseThreadGroupWidth);

            const float radiusV = CBuffer.FilterSizeV / 2.0f;

            float4 filtered = 0.0f;

            [unroll]
            for(int i = -SampleRadius_; i <= SampleRadius_; ++i)
            {
                float4 smSample = UnpackFromSharedMem(SharedMem[int(localIdx.y + GuardBandSize) + i][localIdx.x + GuardBandSize]);

                smSample *= saturate((radiusV + 0.5f) - abs(i));

                filtered += smSample;
            }

            filtered /= CBuffer.FilterSizeV;

            OutputTexture[uint3(GroupID.xy * BaseThreadGroupWidth + localIdx, CBuffer.ArraySliceIdx)] = filtered;
        }
    #endif
}

#endif // CS_