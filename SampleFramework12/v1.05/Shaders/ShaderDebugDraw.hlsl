//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

//=================================================================================================
// Includes
//=================================================================================================
#include <StaticSamplers.hlsli>
#include <ShaderDebug.hlsli>

using namespace ShaderDebug;

ConstantBuffer<InitDrawBuffersConstants> InitDrawBuffersCB : register(b0);

[numthreads(1, 1, 1)]
void InitDrawBuffersCS()
{
    RWByteAddressBuffer lineBuffer = ShaderDebug::GetLineBuffer();
    RWByteAddressBuffer sphereBuffer = ShaderDebug::GetSphereBuffer();

    DrawArgs lineDefaultArgs = (DrawArgs)0;
    lineDefaultArgs.InstanceCount = 1;
    lineBuffer.Store<DrawArgs>(0, lineDefaultArgs);

    DrawIndexedArgs sphereDefaultArgs = (DrawIndexedArgs)0;
    sphereDefaultArgs.IndexCountPerInstance = InitDrawBuffersCB.NumSphereIndices;
    sphereBuffer.Store<DrawIndexedArgs>(0, sphereDefaultArgs);
}

ConstantBuffer<DebugDrawConstants> DebugDrawCB : register(b0);

struct VSOutput
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
};

VSOutput DebugDrawLineVS(in uint vertexIndex : SV_VertexID)
{
    RWByteAddressBuffer lineBuffer = ShaderDebug::GetLineBuffer();
    const uint lineIndex = vertexIndex / 2;
    const DebugLine debugLine = lineBuffer.Load<DebugLine>(sizeof(DrawArgs) + (sizeof(DebugLine) * lineIndex));
    const float3 vertexPos = select(vertexIndex % 2 == 0, debugLine.Pos0, debugLine.Pos1);

    VSOutput output;
    output.Position = mul(float4(vertexPos, 1.0f), DebugDrawCB.ViewProjection);
    output.Color = debugLine.Color;
    return output;
}

VSOutput DebugDrawSphereVS(in uint vertexIndex : SV_VertexID, in uint sphereIndex : SV_InstanceID)
{
    RWByteAddressBuffer sphereBuffer = ShaderDebug::GetSphereBuffer();
    const DebugSphere debugSphere = sphereBuffer.Load<DebugSphere>(sizeof(DrawIndexedArgs) + (sizeof(DebugSphere) * sphereIndex));

    RWStructuredBuffer<float3> sphereVertexBuffer = ResourceDescriptorHeap[DebugDrawCB.SphereVertexBuffer];
    const float3 sourceVertexPos = sphereVertexBuffer[vertexIndex];
    const float3 vertexPos = (sphereVertexBuffer[vertexIndex] * debugSphere.Radius) + debugSphere.Center;

    VSOutput output;
    output.Position = mul(float4(vertexPos, 1.0f), DebugDrawCB.ViewProjection);
    output.Color = debugSphere.Color;
    return output;
}

float4 DebugDrawPS(in VSOutput input) : SV_Target0
{
    return input.Color;
}