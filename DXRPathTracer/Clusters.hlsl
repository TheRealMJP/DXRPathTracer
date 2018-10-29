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
#include <Quaternion.hlsl>
#include "SharedTypes.h"
#include "AppSettings.hlsl"

//=================================================================================================
// Constant buffers
//=================================================================================================
struct ClusterConstants
{
    row_major float4x4 ViewProjection;
    row_major float4x4 InvProjection;
    float NearClip;
    float FarClip;
    float InvClipRange;
    uint NumXTiles;
    uint NumYTiles;
    uint NumXYTiles;
    uint ElementsPerCluster;
    uint InstanceOffset;
    uint NumLights;

    uint BoundsBufferIdx;
    uint VertexBufferIdx;
    uint InstanceBufferIdx;
};

ConstantBuffer<ClusterConstants> CBuffer : register(b0);

//=================================================================================================
// Resources
//=================================================================================================
StructuredBuffer<ClusterBounds> BoundsBuffers[] : register(t0, space100);
StructuredBuffer<float3> VertexBuffers[] : register(t0, space101);
StructuredBuffer<uint> InstanceBuffers[] : register(t0, space102);

RWByteAddressBuffer ClusterBuffer : register(u0);

struct VSOutput
{
    float4 Position : SV_Position;
    uint2 ZTileRange : ZTILERANGE;
    uint Index : INDEX;
};

VSOutput ClusterVS(in uint VertexIdx : SV_VertexID, in uint InstanceIdx : SV_InstanceID)
{
    StructuredBuffer<ClusterBounds> boundsBuffer = BoundsBuffers[CBuffer.BoundsBufferIdx];
    StructuredBuffer<float3> vertexBuffer = VertexBuffers[CBuffer.VertexBufferIdx];
    StructuredBuffer<uint> instanceBuffer = InstanceBuffers[CBuffer.InstanceBufferIdx];

    uint idx = instanceBuffer[InstanceIdx + CBuffer.InstanceOffset];
    ClusterBounds bounds = boundsBuffer[idx];

    float3 vtxPos = vertexBuffer[VertexIdx] * bounds.Scale;
    vtxPos = QuatRotate(vtxPos, bounds.Orientation);
    vtxPos += bounds.Position;

    VSOutput output;
    output.Position = mul(float4(vtxPos, 1.0f), CBuffer.ViewProjection);
    output.Index = idx;
    output.ZTileRange = bounds.ZBounds;

    return output;
}

void ClusterPS(in VSOutput input)
{
    uint2 tilePosXY = uint2(input.Position.xy);
    uint elemIdx = input.Index / 32;
    uint mask = 1 << (input.Index % 32);

    // Estimate the minimum and maximum Z tile intersected by the current triangle, treating the triangle as a plane.
    // This estimate will be wrong if we end up extrapolating off of the triangle.
    float zw = input.Position.z;
    float zwDX = ddx_fine(zw);
    float zwDY = ddy_fine(zw);
    float tileMinZW = zw - abs(0.5f * zwDX) - abs(0.5f * zwDY);
    float tileMaxZW = zw + abs(0.5f * zwDX) + abs(0.5f * zwDY);

    float proj33 = CBuffer.FarClip * CBuffer.InvClipRange;
    float proj43 = -CBuffer.NearClip * CBuffer.FarClip * CBuffer.InvClipRange;
    float tileMinDepth = proj43 / (tileMinZW - proj33);
    float tileMaxDepth = proj43 / (tileMaxZW - proj33);

    tileMinDepth = saturate((tileMinDepth - CBuffer.NearClip) * CBuffer.InvClipRange);
    tileMaxDepth = saturate((tileMaxDepth - CBuffer.NearClip) * CBuffer.InvClipRange);

    uint minZTile = uint(tileMinDepth * NumZTiles);
    uint maxZTile = uint(tileMaxDepth * NumZTiles);

    #if Intersecting_
        // Go from the near plane all the way to the max Z tile intersected in this pixel
        uint zTileStart = 0;
        uint zTileEnd = min(maxZTile, input.ZTileRange.y);
    #elif BackFace_
        // Just mark the tiles intersected by the back face
        uint zTileStart = max(minZTile, input.ZTileRange.x);
        uint zTileEnd = min(maxZTile, input.ZTileRange.y);
    #elif FrontFace_
        // Start at the minimum Z tile intersected by the front faces, then walk forward until we
        // tile marked by the back face rendering
        uint zTileStart = max(minZTile, input.ZTileRange.x);
        uint zTileEnd = input.ZTileRange.y;
    #endif

    for(uint zTile = zTileStart; zTile <= zTileEnd; ++zTile)
    {
        uint3 tileCoords = uint3(tilePosXY, zTile);
        uint clusterIndex = (tileCoords.z * CBuffer.NumXYTiles) + (tileCoords.y * CBuffer.NumXTiles) + tileCoords.x;
        uint address = clusterIndex * CBuffer.ElementsPerCluster + elemIdx;

        #if FrontFace_
            if(ClusterBuffer.Load(address * 4) & mask)
                break;
        #endif

        ClusterBuffer.InterlockedOr(address * 4, mask);
    }
}
