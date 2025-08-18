//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "ShaderShared.h"

struct MeshVertex
{
    ShaderFloat3 Position;
    ShaderFloat3 Normal;
    ShaderFloat2 UV;
    ShaderFloat3 Tangent;
    ShaderFloat3 Bitangent;
};

struct Meshlet
{
    ShaderUint VertexOffset;    // These is an offset into the MeshletVertexBuffer, which actually contains indices
    ShaderUint TriangleOffset;
    ShaderUint16 VertexCount;
    ShaderUint16 TriangleCount;

    ShaderUint16 MeshIndex;
    ShaderUint16 MaterialIndex;
    ShaderUint MeshVertexOffset;
};

struct MeshletTriangle
{
    ShaderUint Packed;

    static MeshletTriangle Pack(uint32_t v0, uint32_t v1, uint32_t v2)
    {
        MeshletTriangle tri;
        tri.Packed = v0 | (v1 << 8) | (v2 << 16);
        return tri;
    }

    ShaderUint3 Unpack()
    {
        const uint32_t idx0 = Packed & 0xFF;
        const uint32_t idx1 = (Packed >> 8) & 0xFF;
        const uint32_t idx2 = (Packed >> 16) & 0xFF;
        return ShaderUint3(idx0, idx1, idx2);
    }
};

struct MeshletBounds
{
    ShaderFloat3 Center;
    ShaderFloat Radius;
};

SharedConstant_ uint32_t MaxMeshletVertices = 64;
SharedConstant_ uint32_t MaxMeshletTriangles = 64;

#if HLSL_

struct TriVertices
{
    MeshVertex Vtx0;
    MeshVertex Vtx1;
    MeshVertex Vtx2;
};

TriVertices GetMeshletTriVertices(DescriptorIndex meshletTrianglesBufferIdx, DescriptorIndex meshletVerticesBufferIdx, DescriptorIndex vertexBufferIdx, Meshlet meshlet, uint meshletTriangleIndex)
{
    StructuredBuffer<MeshletTriangle> meshletTrianglesBuffer = ResourceDescriptorHeap[meshletTrianglesBufferIdx];
    ByteAddressBuffer meshletVerticesBuffer = ResourceDescriptorHeap[meshletVerticesBufferIdx];
    StructuredBuffer<MeshVertex> vertexBuffer = ResourceDescriptorHeap[vertexBufferIdx];
    const MeshletTriangle meshletTriangle = meshletTrianglesBuffer[meshlet.TriangleOffset + meshletTriangleIndex];
    const uint localVertexIdx0 = meshletTriangle.Packed & 0xFF;
    const uint localVertexIdx1 = (meshletTriangle.Packed >> 8) & 0xFF;
    const uint localVertexIdx2 = (meshletTriangle.Packed >> 16) & 0xFF;
    const uint vertexIdx0 = meshletVerticesBuffer.Load((localVertexIdx0 + meshlet.VertexOffset) * sizeof(uint));
    const uint vertexIdx1 = meshletVerticesBuffer.Load((localVertexIdx1 + meshlet.VertexOffset) * sizeof(uint));
    const uint vertexIdx2 = meshletVerticesBuffer.Load((localVertexIdx2 + meshlet.VertexOffset) * sizeof(uint));

    TriVertices triVertices;
    triVertices.Vtx0 = vertexBuffer[vertexIdx0 + meshlet.MeshVertexOffset];
    triVertices.Vtx1 = vertexBuffer[vertexIdx1 + meshlet.MeshVertexOffset];
    triVertices.Vtx2 = vertexBuffer[vertexIdx2 + meshlet.MeshVertexOffset];
    return triVertices;
}

#endif // HLSL_