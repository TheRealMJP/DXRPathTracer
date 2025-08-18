//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include <Mesh_Shared.h>

#ifndef RAYTRACING_HLSL_
#define RAYTRACING_HLSL_

struct GeometryInfo
{
    uint VtxOffset;
    uint IdxOffset;
    uint MaterialIdx;
    uint PadTo16Bytes;
};

float BarycentricLerp(in float v0, in float v1, in float v2, in float3 barycentrics)
{
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

float2 BarycentricLerp(in float2 v0, in float2 v1, in float2 v2, in float3 barycentrics)
{
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

float3 BarycentricLerp(in float3 v0, in float3 v1, in float3 v2, in float3 barycentrics)
{
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

float4 BarycentricLerp(in float4 v0, in float4 v1, in float4 v2, in float3 barycentrics)
{
    return v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
}

MeshVertex BarycentricLerp(in MeshVertex v0, in MeshVertex v1, in MeshVertex v2, in float3 barycentrics)
{
    MeshVertex vtx;
    vtx.Position = BarycentricLerp(v0.Position, v1.Position, v2.Position, barycentrics);
    vtx.Normal = normalize(BarycentricLerp(v0.Normal, v1.Normal, v2.Normal, barycentrics));
    vtx.UV = BarycentricLerp(v0.UV, v1.UV, v2.UV, barycentrics);
    vtx.Tangent = normalize(BarycentricLerp(v0.Tangent, v1.Tangent, v2.Tangent, barycentrics));
    vtx.Bitangent = normalize(BarycentricLerp(v0.Bitangent, v1.Bitangent, v2.Bitangent, barycentrics));

    return vtx;
}

struct HitInfo
{
    float T;
    uint GeometryIdx;
    uint PrimitiveIdx;
    uint InstanceIdx;
    float2 Barycentrics;
    bool FrontFacing;
};

template<uint RayQueryFlags = RAY_FLAG_CULL_BACK_FACING_TRIANGLES>
HitInfo ClosestRayHit(in RayDesc ray, in RaytracingAccelerationStructure accelStructure, in uint mask)
{
    HitInfo hitInfo = { -1.0f, -1, -1, -1, float2(0.0f, 0.0f), false };

    RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RayQueryFlags> query;
    query.TraceRayInline(accelStructure, 0, mask, ray);
    while(query.Proceed());
    if(query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        hitInfo.T = query.CommittedRayT();
        hitInfo.GeometryIdx = query.CommittedGeometryIndex();
        hitInfo.PrimitiveIdx = query.CommittedPrimitiveIndex();
        hitInfo.InstanceIdx = query.CommittedInstanceIndex();
        hitInfo.Barycentrics = query.CommittedTriangleBarycentrics();
        hitInfo.FrontFacing = query.CommittedTriangleFrontFace();
    }

    return hitInfo;
}

MeshVertex GetHitInterpolatedVertex(in HitInfo hitInfo, in StructuredBuffer<GeometryInfo> geoInfoBuffer, in StructuredBuffer<MeshVertex> vtxBuffer, in Buffer<uint> idxBuffer)
{
    const float3 barycentrics = float3(1 - hitInfo.Barycentrics.x - hitInfo.Barycentrics.y, hitInfo.Barycentrics.x, hitInfo.Barycentrics.y);

    const GeometryInfo geoInfo = geoInfoBuffer[hitInfo.GeometryIdx];

    const uint primIdx = hitInfo.PrimitiveIdx;
    const uint idx0 = idxBuffer[primIdx * 3 + geoInfo.IdxOffset + 0];
    const uint idx1 = idxBuffer[primIdx * 3 + geoInfo.IdxOffset + 1];
    const uint idx2 = idxBuffer[primIdx * 3 + geoInfo.IdxOffset + 2];

    const MeshVertex vtx0 = vtxBuffer[idx0 + geoInfo.VtxOffset];
    const MeshVertex vtx1 = vtxBuffer[idx1 + geoInfo.VtxOffset];
    const MeshVertex vtx2 = vtxBuffer[idx2 + geoInfo.VtxOffset];

    return BarycentricLerp(vtx0, vtx1, vtx2, barycentrics);
}

#endif // RAYTRACING_HLSL_