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

namespace ShaderDebug
{

struct DebugInfo
{
    ShaderUint2 CursorXY;
    DescriptorIndex PrintBuffer;
    DescriptorIndex LineBuffer;
    DescriptorIndex SphereBuffer;
    ShaderUint PrintBufferSize;
    ShaderUint MaxLines;
    ShaderUint MaxSpheres;
};

SharedConstant_ DescriptorIndex MagicDebugBufferIndex = 1024;
SharedConstant_ uint32_t MaxDebugPrintArgs = 16;

enum ArgCode
{
    DebugPrint_Uint = 0,
    DebugPrint_Uint2,
    DebugPrint_Uint3,
    DebugPrint_Uint4,
    DebugPrint_Int,
    DebugPrint_Int2,
    DebugPrint_Int3,
    DebugPrint_Int4,
    DebugPrint_Float,
    DebugPrint_Float2,
    DebugPrint_Float3,
    DebugPrint_Float4,

    NumDebugPrintArgCodes,
};

struct DebugPrintHeader
{
    ShaderUint NumBytes;
    ShaderUint StringSize;
    ShaderUint NumArgs;
};

struct DrawArgs
{
    ShaderUint VertexCountPerInstance;
    ShaderUint InstanceCount;
    ShaderUint StartVertexLocation;
    ShaderUint StartInstanceLocation;
};

struct DrawIndexedArgs
{
    ShaderUint IndexCountPerInstance;
    ShaderUint InstanceCount;
    ShaderUint StartIndexLocation;
    ShaderInt BaseVertexLocation;
    ShaderUint StartInstanceLocation;
};

struct DebugLine
{
    ShaderFloat3 Pos0;
    ShaderFloat3 Pos1;
    ShaderFloat4 Color;
};

struct DebugSphere
{
    ShaderFloat3 Center;
    ShaderFloat Radius;
    ShaderFloat4 Color;
};

struct InitDrawBuffersConstants
{
    ShaderUint NumSphereIndices;
};

struct DebugDrawConstants
{
    row_major ShaderFloat4x4 ViewProjection;
    DescriptorIndex SphereVertexBuffer;
};

} // namespace ShaderDebug