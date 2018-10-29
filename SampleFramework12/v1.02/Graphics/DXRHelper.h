//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "../PCH.h"
#include "../Containers.h"

namespace SampleFramework12
{

struct StateObjectBuilder
{
    Array<uint8> SubObjectData;
    Array<D3D12_STATE_SUBOBJECT> SubObjects;
    uint64 NumSubObjects = 0;
    uint64 MaxSubObjects = 0;

    void Init(uint64 maxSubObjects);

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_STATE_OBJECT_CONFIG& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_STATE_OBJECT_CONFIG);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_GLOBAL_ROOT_SIGNATURE& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_LOCAL_ROOT_SIGNATURE& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_NODE_MASK& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_DXIL_LIBRARY_DESC& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_EXISTING_COLLECTION_DESC& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_RAYTRACING_SHADER_CONFIG& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_RAYTRACING_PIPELINE_CONFIG& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const D3D12_HIT_GROUP_DESC& subObjDesc)
    {
        return AddSubObject(&subObjDesc, sizeof(subObjDesc), D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP);
    }

    const D3D12_STATE_SUBOBJECT* AddSubObject(const void* subObjDesc, uint64 subObjDescSize, D3D12_STATE_SUBOBJECT_TYPE type);

    void BuildDesc(D3D12_STATE_OBJECT_TYPE type, D3D12_STATE_OBJECT_DESC& desc);

    ID3D12StateObject* CreateStateObject(D3D12_STATE_OBJECT_TYPE type);
};

struct ShaderIdentifier
{
    uint8 Data[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES] = { };

    ShaderIdentifier() = default;
    explicit ShaderIdentifier(const void* idPointer)
    {
        memcpy(Data, idPointer, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    }
};

} // namespace SampleFramework12