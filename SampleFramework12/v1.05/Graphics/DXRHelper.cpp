//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"
#include "DXRHelper.h"
#include "DX12.h"
#include "Model.h"
#include "Profiler.h"
#include "../Exceptions.h"

namespace SampleFramework12
{

static const uint64_t MaxSubObjDescSize = 512;

void StateObjectBuilder::Init(uint64_t maxSubObjects)
{
    Assert_(maxSubObjects > 0);

    MaxSubObjects = maxSubObjects;
    SubObjectData.Init(maxSubObjects * MaxSubObjDescSize, 0);

    D3D12_STATE_SUBOBJECT defSubObj = { };
    SubObjects.Init(maxSubObjects, defSubObj);
}

const D3D12_STATE_SUBOBJECT* StateObjectBuilder::AddSubObject(const void* subObjDesc, uint64_t subObjDescSize, D3D12_STATE_SUBOBJECT_TYPE type)
{
    Assert_(subObjDesc != nullptr);
    Assert_(subObjDescSize > 0);
    Assert_(type < D3D12_STATE_SUBOBJECT_TYPE_MAX_VALID);
    Assert_(subObjDescSize <= MaxSubObjDescSize);
    Assert_(NumSubObjects < MaxSubObjects);

    const uint64_t subObjOffset = NumSubObjects * MaxSubObjDescSize;
    memcpy(SubObjectData.Data() + subObjOffset, subObjDesc, subObjDescSize);

    D3D12_STATE_SUBOBJECT& newSubObj = SubObjects[NumSubObjects];
    newSubObj.Type = type;
    newSubObj.pDesc = SubObjectData.Data() + subObjOffset;

    NumSubObjects += 1;

    return &newSubObj;
}

D3D12_STATE_OBJECT_DESC StateObjectBuilder::BuildDesc(D3D12_STATE_OBJECT_TYPE type) const
{
    D3D12_STATE_OBJECT_DESC desc =
    {
        .Type = type,
        .NumSubobjects = uint32_t(NumSubObjects),
        .pSubobjects = NumSubObjects ? SubObjects.Data() : nullptr,
    };
    return desc;
}

ID3D12StateObject* StateObjectBuilder::CreateStateObject(D3D12_STATE_OBJECT_TYPE type)
{
    D3D12_STATE_OBJECT_DESC desc = BuildDesc(type);

    ID3D12StateObject* stateObj = nullptr;
    DXCall(DX12::Device->CreateStateObject(&desc, IID_PPV_ARGS(&stateObj)));

    return stateObj;
}

void BuildModelAccelStructure(ID3D12GraphicsCommandList7* cmdList, const Model& model, float sceneScale, ModelAccelStructure& output)
{
    output.Shutdown();

    const FormattedBuffer& idxBuffer = model.IndexBuffer();
    const StructuredBuffer& vtxBuffer = model.VertexBuffer();

    const uint64_t numMeshes = model.NumMeshes();
    Array<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs(numMeshes);

    const uint32_t numGeometries = uint32_t(geometryDescs.Size());
    Array<GeometryInfo> geoInfoBufferData(numGeometries);

    for(uint64_t meshIdx = 0; meshIdx < numMeshes; ++meshIdx)
    {
        const Mesh& mesh = model.Meshes()[meshIdx];
        Assert_(mesh.NumMeshParts() == 1);
        const uint32_t materialIdx = mesh.MeshParts()[0].MaterialIdx;
        const MeshMaterial& material = model.Materials()[materialIdx];

        D3D12_RAYTRACING_GEOMETRY_DESC& geometryDesc = geometryDescs[meshIdx];
        geometryDesc = { };
        geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometryDesc.Triangles.IndexBuffer = idxBuffer.GPUAddress + mesh.IndexOffset() * idxBuffer.Stride;
        geometryDesc.Triangles.IndexCount = uint32_t(mesh.NumIndices());
        geometryDesc.Triangles.IndexFormat = idxBuffer.Format;
        geometryDesc.Triangles.Transform3x4 = 0;
        geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geometryDesc.Triangles.VertexCount = uint32_t(mesh.NumVertices());
        geometryDesc.Triangles.VertexBuffer.StartAddress = vtxBuffer.GPUAddress + mesh.VertexOffset() * vtxBuffer.Stride;
        geometryDesc.Triangles.VertexBuffer.StrideInBytes = vtxBuffer.Stride;
        geometryDesc.Flags = material.Opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

        GeometryInfo& geoInfo = geoInfoBufferData[meshIdx];
        geoInfo = { };
        geoInfo.VtxOffset = uint32_t(mesh.VertexOffset());
        geoInfo.IdxOffset = uint32_t(mesh.IndexOffset());
        geoInfo.MaterialIdx = mesh.MeshParts()[0].MaterialIdx;

        Assert_(mesh.NumMeshParts() == 1);
    }

    // Get required sizes for an acceleration structure
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};

    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildInfoDesc = {};
        prebuildInfoDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        prebuildInfoDesc.Flags = buildFlags;
        prebuildInfoDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        prebuildInfoDesc.pGeometryDescs = nullptr;
        prebuildInfoDesc.NumDescs = 1;
        DX12::Device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildInfoDesc, &topLevelPrebuildInfo);
    }

    Assert_(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};

    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildInfoDesc = {};
        prebuildInfoDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        prebuildInfoDesc.Flags = buildFlags;
        prebuildInfoDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        prebuildInfoDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        prebuildInfoDesc.pGeometryDescs = geometryDescs.Data();
        prebuildInfoDesc.NumDescs = numGeometries;
        DX12::Device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildInfoDesc, &bottomLevelPrebuildInfo);
    }

    Assert_(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

    RawBuffer scratchBuffer;

    {
        RawBufferInit bufferInit;
        bufferInit.NumElements = Max(topLevelPrebuildInfo.ScratchDataSizeInBytes, bottomLevelPrebuildInfo.ScratchDataSizeInBytes) / RawBuffer::Stride;
        bufferInit.CreateUAV = true;        
        bufferInit.Name = "RT Scratch Buffer";
        scratchBuffer.Initialize(bufferInit);
    }

    {
        RTAccelStructureInit bufferInit;
        bufferInit.Size = bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes;
        bufferInit.Name = "RT Bottom Level Accel Structure";
        output.BottomLevelAccelStructure.Initialize(bufferInit);
    }

    {
        RTAccelStructureInit bufferInit;
        bufferInit.Size = topLevelPrebuildInfo.ResultDataMaxSizeInBytes;
        bufferInit.Name = "RT Top Level Accel Structure";
        output.TopLevelAccelStructure.Initialize(bufferInit);
    }

    // Create an instance desc for the bottom-level acceleration structure.
    D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
    instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = sceneScale;
    instanceDesc.InstanceMask = 1;
    instanceDesc.AccelerationStructure = output.BottomLevelAccelStructure.GPUAddress;

    TempBuffer instanceBuffer = DX12::TempStructuredBuffer(1, sizeof(D3D12_RAYTRACING_INSTANCE_DESC), false);
    memcpy(instanceBuffer.CPUAddress, &instanceDesc, sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

    // Bottom Level Acceleration Structure desc
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
    {
        bottomLevelBuildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        bottomLevelBuildDesc.Inputs.Flags = buildFlags;
        bottomLevelBuildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        bottomLevelBuildDesc.Inputs.NumDescs = numGeometries;
        bottomLevelBuildDesc.Inputs.pGeometryDescs = geometryDescs.Data();
        bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchBuffer.GPUAddress;
        bottomLevelBuildDesc.DestAccelerationStructureData = output.BottomLevelAccelStructure.GPUAddress;
    }

    // Top Level Acceleration Structure desc
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = bottomLevelBuildDesc;
    {
        topLevelBuildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        topLevelBuildDesc.Inputs.NumDescs = 1;
        topLevelBuildDesc.Inputs.pGeometryDescs = nullptr;
        topLevelBuildDesc.Inputs.InstanceDescs = instanceBuffer.GPUAddress;
        topLevelBuildDesc.DestAccelerationStructureData = output.TopLevelAccelStructure.GPUAddress;;
        topLevelBuildDesc.ScratchAccelerationStructureData = scratchBuffer.GPUAddress;
    }

    {
        // ProfileBlock profileBlock(cmdList, "Build Acceleration Structure");

        cmdList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

        {
            D3D12_RESOURCE_BARRIER barrier =
            {
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                barrier.UAV.pResource = output.BottomLevelAccelStructure.Resource(),
            };
            cmdList->ResourceBarrier(1, &barrier);
        }
        // DX12::Barrier(cmdList, output.BottomLevelAccelStructure.BottomLevelPostBuildBarrier());

        cmdList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);

        {
            D3D12_RESOURCE_BARRIER barrier =
            {
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                barrier.UAV.pResource = output.TopLevelAccelStructure.Resource(),
            };
            cmdList->ResourceBarrier(1, &barrier);
        }
        // DX12::Barrier(cmdList, output.TopLevelAccelStructure.TopLevelPostBuildBarrier());
    }

    scratchBuffer.Shutdown();

    {
        StructuredBufferInit sbInit;
        sbInit.Stride = sizeof(GeometryInfo);
        sbInit.NumElements = numGeometries;
        sbInit.Name = "Geometry Info Buffer";
        sbInit.InitData = geoInfoBufferData.Data();
        output.GeoInfoBuffer.Initialize(sbInit);
    }

    output.VertexBufferSRV = vtxBuffer.SRV;
    output.IndexBufferSRV = idxBuffer.SRV;
}

void BuildSceneAccelStructure(ID3D12GraphicsCommandList7* cmdList, const Model* const* models, uint32_t numModels, SceneAccelStructure& output)
{
    output.Shutdown();

    Assert_(models != nullptr);
    Assert_(numModels > 0);
    output.ModelAccelStructures.Init(numModels);
    for(uint32_t i = 0; i < numModels; ++i)
        BuildModelAccelStructure(cmdList, *models[i], 1.0f, output.ModelAccelStructures[i]);

    // Get required sizes for an acceleration structure
    const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildInfoDesc =
    {
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
        .Flags = buildFlags,
        .NumDescs = numModels,
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
        .pGeometryDescs = nullptr,
    };
    DX12::Device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildInfoDesc, &topLevelPrebuildInfo);

    Assert_(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

    RawBuffer scratchBuffer;
    scratchBuffer.Initialize({
        .NumElements = topLevelPrebuildInfo.ScratchDataSizeInBytes / RawBuffer::Stride,
        .CreateUAV = true,
        .Name = "RT Scratch Buffer",
    });

    {
        RTAccelStructureInit bufferInit;
        bufferInit.Size = topLevelPrebuildInfo.ResultDataMaxSizeInBytes;
        bufferInit.Name = "RT Top Level Accel Structure";
        output.TopLevelAccelStructure.Initialize(bufferInit);
    }

    // Create instance descs for the bottom-level acceleration structures
    TempBuffer instanceBuffer = DX12::TempStructuredBuffer(numModels, sizeof(D3D12_RAYTRACING_INSTANCE_DESC), false);
    D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs = (D3D12_RAYTRACING_INSTANCE_DESC*)instanceBuffer.CPUAddress;
    for(uint32_t i = 0; i < numModels; ++i)
    {
        D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = { };
        instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1.0f;
        instanceDesc.InstanceMask = 1u << i;
        instanceDesc.AccelerationStructure = output.ModelAccelStructures[i].BottomLevelAccelStructure.GPUAddress;
        memcpy(instanceDescs + i, &instanceDesc, sizeof(instanceDesc));
    }

    // Top Level Acceleration Structure desc
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
    topLevelBuildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    topLevelBuildDesc.Inputs.NumDescs = numModels;
    topLevelBuildDesc.Inputs.pGeometryDescs = nullptr;
    topLevelBuildDesc.Inputs.InstanceDescs = instanceBuffer.GPUAddress;
    topLevelBuildDesc.DestAccelerationStructureData = output.TopLevelAccelStructure.GPUAddress;;
    topLevelBuildDesc.ScratchAccelerationStructureData = scratchBuffer.GPUAddress;

    cmdList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);

    {
        D3D12_RESOURCE_BARRIER barrier =
        {
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            barrier.UAV.pResource = output.TopLevelAccelStructure.Resource(),
        };
        cmdList->ResourceBarrier(1, &barrier);
    }

    scratchBuffer.Shutdown();
}

} // namespace SampleFramework12