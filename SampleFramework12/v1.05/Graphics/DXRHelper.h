//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "../PCH.h"
#include "../Containers.h"
#include "GraphicsTypes.h"

namespace SampleFramework12
{

class Model;

// Helper for building a ray tracing PSO (state object)
struct StateObjectBuilder
{
    Array<uint8_t> SubObjectData;
    Array<D3D12_STATE_SUBOBJECT> SubObjects;
    uint64_t NumSubObjects = 0;
    uint64_t MaxSubObjects = 0;

    void Init(uint64_t maxSubObjects);

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_STATE_OBJECT_CONFIG subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_STATE_OBJECT_CONFIG);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_GLOBAL_ROOT_SIGNATURE subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_LOCAL_ROOT_SIGNATURE subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_NODE_MASK subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_DXIL_LIBRARY_DESC subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_EXISTING_COLLECTION_DESC subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_RAYTRACING_SHADER_CONFIG subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_RAYTRACING_PIPELINE_CONFIG subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_HIT_GROUP_DESC subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_WORK_GRAPH_DESC subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_WORK_GRAPH);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_STREAM_OUTPUT_DESC subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_BLEND_DESC subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_BLEND);
    }

    const D3D12_STATE_SUBOBJECT* AddSampleMaskSubObject(UINT sampleMask)
    {
        return AddSubObject(&sampleMask, sizeof(sampleMask), D3D12_STATE_SUBOBJECT_TYPE_SAMPLE_MASK);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_RASTERIZER_DESC2 subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_RASTERIZER);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_DEPTH_STENCIL_DESC subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_INPUT_LAYOUT_DESC subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE cutValue)
    {
        return AddSubObject(&cutValue, sizeof(cutValue), D3D12_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_PRIMITIVE_TOPOLOGY_TYPE primTopologyType)
    {
        return AddSubObject(&primTopologyType, sizeof(primTopologyType), D3D12_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_RT_FORMAT_ARRAY subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS);
    }

    const D3D12_STATE_SUBOBJECT* AddDSVFormatSubObject(DXGI_FORMAT dsvFormat)
    {
        return AddSubObject(&dsvFormat, sizeof(dsvFormat), D3D12_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(DXGI_SAMPLE_DESC subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_SAMPLE_DESC);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_PIPELINE_STATE_FLAGS subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_FLAGS);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_DEPTH_STENCIL_DESC1 subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_VIEW_INSTANCING_DESC subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_GENERIC_PROGRAM_DESC subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_GENERIC_PROGRAM);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(D3D12_DEPTH_STENCIL_DESC2 subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL2);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const void* subObjDesc, uint64_t subObjDescSize, D3D12_STATE_SUBOBJECT_TYPE type);

    D3D12_STATE_OBJECT_DESC BuildDesc(D3D12_STATE_OBJECT_TYPE type) const;

    ID3D12StateObject* CreateStateObject(D3D12_STATE_OBJECT_TYPE type);
};

// Helper for embedding shader indentifiers in shader records inside of ray tracing shader table
struct ShaderIdentifier
{
    uint8_t Data[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES] = { };

    ShaderIdentifier() = default;
    explicit ShaderIdentifier(const void* idPointer)
    {
        memcpy(Data, idPointer, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    }
};

// Per-geometry data put into a buffer during the acceleration structure build process
struct GeometryInfo
{
    uint32_t VtxOffset = 0;
    uint32_t IdxOffset = 0;
    uint32_t MaterialIdx = 0;
    uint32_t PadTo16Bytes = 0;
};

// Data output by BuildModelAccelStructure
struct ModelAccelStructure
{
    RTAccelStructure BottomLevelAccelStructure;
    RTAccelStructure TopLevelAccelStructure;
    StructuredBuffer GeoInfoBuffer;
    DescriptorIndex VertexBufferSRV;
    DescriptorIndex IndexBufferSRV;

    void Shutdown()
    {
        BottomLevelAccelStructure.Shutdown();

        TopLevelAccelStructure.Shutdown();
        GeoInfoBuffer.Shutdown();
    }
};

void BuildModelAccelStructure(ID3D12GraphicsCommandList7* cmdList, const Model& model, float sceneScale, ModelAccelStructure& output);


// Data output by BuildSceneAccelStructure
struct SceneAccelStructure
{
    Array<ModelAccelStructure> ModelAccelStructures;
    RTAccelStructure TopLevelAccelStructure;

    void Shutdown()
    {
        for(ModelAccelStructure& modelAS : ModelAccelStructures)
            modelAS.Shutdown();
        TopLevelAccelStructure.Shutdown();
    }
};

void BuildSceneAccelStructure(ID3D12GraphicsCommandList7* cmdList, const Model* const* models, uint32_t numModels, SceneAccelStructure& output);

} // namespace SampleFramework12